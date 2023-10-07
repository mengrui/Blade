// Copyright Epic Games, Inc. All Rights Reserved.

#include "BladeCharacter.h"

#include "AttackNotifyState.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "Components/InputComponent.h"
#include "Components/ArrowComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "BladeAnimInstance.h"
#include "Engine/DamageEvents.h"
#include "MotionWarpingComponent.h"
#include "PhysicsControlComponent.h"
#include "Components/MaterialBillboardComponent.h"
#include "NextComboState.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "BladeCharacterMovementComponent.h"
#include "BladeWeapon.h"
#include "BladeUtility.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/SkeletalMeshSocket.h"
#include "TrackCollisionComponent.h"
#include "BladeAIControllerBase.h"

//////////////////////////////////////////////////////////////////////////
// ABladeCharacter

ABladeCharacter::ABladeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBladeCharacterMovementComponent>(CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 375;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
	MotionWarping = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarping"));
	PhysicsControlComponent = CreateDefaultSubobject<UPhysicsControlComponent>(TEXT("PhysicsControl"));
	PlayerTitleComponent = CreateDefaultSubobject<UMaterialBillboardComponent>(TEXT("PlayerInfoBillboard"));
	PlayerTitleComponent->SetupAttachment(RootComponent);
}

void ABladeCharacter::InitializePhysicsAnimation()
{
	GetMesh()->SetConstraintProfileForAll(TEXT("None"));
	FPhysicsControlSettings  ControlSettings;
	PhysicsControlComponent->CreateControlsAndBodyModifiersFromLimbBones(
		AllWorldSpaceControls,
		LimbWorldSpaceControls,
		AllParentSpaceControls,
		LimbParentSpaceControls,
		AllBodyModifiers,
		LimbBodyModifiers,
		GetMesh(),
		PhysicsControlLimbSetupDatas,
		WorldSpaceControlData,
		ControlSettings,
		false,
		ParentSpaceControlData,
		ControlSettings,
		false,
		EPhysicsMovementType::Kinematic,
		0
	);

	EnablePhysicsAnimation(true);
}

void ABladeCharacter::EnablePhysicsAnimation(bool bTrue)
{
	if (bTrue)
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PhysicsControlComponent->SetBodyModifiersMovementType(AllBodyModifiers.Names, EPhysicsMovementType::Simulated);
		TArray<FName> FootBoneNames;
		for (const auto& val : AllBodyModifiers.Names)
			if (val.ToString().Contains(TEXT("foot")) || val.ToString().Contains(TEXT("hand_r")))
				FootBoneNames.Add(val);

		PhysicsControlComponent->SetBodyModifiersMovementType(FootBoneNames, EPhysicsMovementType::Kinematic);
		PhysicsControlComponent->SetControlsEnabled(AllWorldSpaceControls.Names, true);
		PhysicsControlComponent->SetControlsEnabled(AllParentSpaceControls.Names, false);
		PhysicsControlComponent->SetBodyModifiersGravityMultiplier(AllBodyModifiers.Names, 0);
	}
	else
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		PhysicsControlComponent->SetControlsEnabled(AllWorldSpaceControls.Names, false);
		PhysicsControlComponent->SetControlsEnabled(AllParentSpaceControls.Names, false);
		PhysicsControlComponent->SetBodyModifiersGravityMultiplier(AllBodyModifiers.Names, 1);
		PhysicsControlComponent->SetBodyModifiersMovementType(AllBodyModifiers.Names, EPhysicsMovementType::Kinematic);
	}
}

bool ABladeCharacter::IsRagdoll() const
{
	return bRagdoll;
}

void ABladeCharacter::SetRagdoll(bool bEnable)
{	
	bRagdoll = bEnable;
	if (bRagdoll)
	{
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		if (!PhysicsControlComponent->GetAllControlNames().IsEmpty())
		{
			PhysicsControlComponent->SetControlsEnabled(AllWorldSpaceControls.Names, false);
			PhysicsControlComponent->SetControlsEnabled(AllParentSpaceControls.Names, false);
			PhysicsControlComponent->SetBodyModifiersGravityMultiplier(AllBodyModifiers.Names, 1);
			PhysicsControlComponent->SetBodyModifiersMovementType(AllBodyModifiers.Names, EPhysicsMovementType::Simulated);
		}
		else
		{
			GetMesh()->SetSimulatePhysics(true);
		}

		GetMesh()->SetAllMassScale(MassScale);
		for (auto& Setup : WeaponSlots)
		{
			if (Setup.Weapon)
				Setup.Weapon->SetSimulatePhysics(bRagdoll);
		}

		if (auto AnimInst = GetAnimInstance())
		{
			GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(AnimInst, [AnimInst]()
				{
					AnimInst->bRagdoll = true;
				}));
		}
	}
	else
	{
		if (auto AnimInst = GetAnimInstance())
		{
			AnimInst->SavePoseSnapshot(TEXT("Ragdoll"));
			FTransform PelvisTransform = GetMesh()->GetBoneTransform(GetMesh()->GetBoneIndex(AnimInst->PelvisBone));
			FHitResult Hit;
			const auto ActorRot = FRotationMatrix::MakeFromX(PelvisTransform.GetUnitAxis(EAxis::X).GetSafeNormal2D() * (GetMesh()->GetBoneAxis(AnimInst->PelvisBone, EAxis::Y).Z > 0 ? -1 : 1)).Rotator();
			SetActorLocation(PelvisTransform.GetLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
			GetCharacterMovement()->SafeMoveUpdatedComponent(PelvisTransform.GetLocation() - GetActorLocation(), ActorRot, true, Hit);

			if (!PhysicsControlComponent->GetAllControlNames().IsEmpty())
			{
				PhysicsControlComponent->SetControlsEnabled(AllWorldSpaceControls.Names, false);
				PhysicsControlComponent->SetControlsEnabled(AllParentSpaceControls.Names, false);
				PhysicsControlComponent->SetBodyModifiersGravityMultiplier(AllBodyModifiers.Names, 1);
				PhysicsControlComponent->SetBodyModifiersMovementType(AllBodyModifiers.Names, EPhysicsMovementType::Kinematic);
			}
			else
			{
				GetMesh()->SetSimulatePhysics(false);
			}

			GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			//GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Block);
			GetMesh()->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepWorldTransform);
			GetMesh()->SetRelativeLocationAndRotation(GetBaseTranslationOffset(), GetBaseRotationOffset(), false, nullptr, ETeleportType::TeleportPhysics);
			AnimInst->PelvisTransform = PelvisTransform.GetRelativeTransform(GetMesh()->GetComponentToWorld());
			GetMesh()->TickPose(0, false);
			GetMesh()->RefreshBoneTransforms();
			for (auto& Setup : WeaponSlots)
			{
				if (Setup.Weapon)
					Setup.Weapon->SetSimulatePhysics(false);
			}
		}
	}
}

void ABladeCharacter::SnapCapsuleToRagdoll()
{
	if (auto AnimInst = GetAnimInstance())
	{
		FHitResult Hit;
		FTransform PelvisTransform = GetMesh()->GetBoneTransform(GetMesh()->GetBoneIndex(AnimInst->PelvisBone));
		SetActorLocation(PelvisTransform.GetLocation() + FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		GetCharacterMovement()->SafeMoveUpdatedComponent(
			PelvisTransform.GetLocation() - GetActorLocation(), GetActorRotation(), true, Hit);
	}
}

FName ABladeCharacter::GetWeaponSocketName(ABladeWeapon* Weapon)
{
	for (int i = 0; i < WeaponSlots.Num(); i++)
	{
		if (WeaponSlots[i].Weapon == Weapon)
		{
			return WeaponSlots[i].AttachSocketName;
			break;
		}
	}

	return NAME_None;
}

void ABladeCharacter::Destroyed()
{
	for (int i = 0; i < WeaponSlots.Num(); i++)
	{
		if (WeaponSlots[i].Weapon)
			WeaponSlots[i].Weapon->Destroy();

		WeaponSlots[i].Weapon = nullptr;
	}
}

float SmoothTo(float Src, float Dst, float Val)
{
	Val = FMath::Abs(Val);
	float Delta = Dst - Src;
	if (Delta <= KINDA_SMALL_NUMBER && Delta >= -KINDA_SMALL_NUMBER)
	{
		return Dst;
	}
	else if (Delta > KINDA_SMALL_NUMBER)
	{
		return Dst - FMath::Max(Delta - Val, 0);
	}
	else
	{
		return Dst - FMath::Min(Delta + Val, 0);
	}
}

FVector SmoothTo(const FVector& Src, const FVector& Dst, FVector Val)
{
	const FVector Delta = Dst - Src;
	if (Delta.IsNearlyZero())
	{
		return Dst;
	}
	else
	{
		return FVector(SmoothTo(Src.X, Dst.X, Val.X), SmoothTo(Src.Y, Dst.Y, Val.Y), SmoothTo(Src.Z, Dst.Z, Val.Z));
	}
}

void ABladeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	auto AnimInst = GetAnimInstance();
	if (!AnimInst) return;

	if (bWantBlock != GetBlock())
	{
		SetBlock(bWantBlock);
	}

	if (IsSprint())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
	else if(GetBlock())
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	else
		GetCharacterMovement()->MaxWalkSpeed = RunSpeed;

	if (IsRagdoll())
	{
		FHitResult Hit;
		GetCharacterMovement()->SafeMoveUpdatedComponent(
			GetMesh()->GetBoneLocation(AnimInst->PelvisBone) - GetActorLocation(), GetActorQuat(), true, Hit);
	}

	if (!AnimInst->IsAnyMontagePlaying())
	{
		bMoveAble = true;
		CachedCommands.Empty();
	}

	if (HealthUIDisplayTime > 0)
	{
		HealthUIDisplayTime -= DeltaSeconds;
	}

	PlayerTitleComponent->SetHiddenInGame(HealthUIDisplayTime <= 0);

	if (auto Ch = const_cast<ABladeCharacter*>(Cast<ABladeCharacter>(SelectedTarget)))
	{
		if (!Ch->IsPlayerControlled())
		{
			Ch->HealthUIDisplayTime = 2;
		}
	}
}

static const UHitMetaData* GetHitData(UAnimSequenceBase* Anim)
{
	if (Anim)
	{
		const auto& MetaDataArray = Anim->GetMetaData();
		for (const auto MetaData : MetaDataArray)
		{
			if (const UHitMetaData* HitData = Cast<UHitMetaData>(MetaData))
				return HitData;
		}
	}

	return  nullptr;
}

FVector ABladeCharacter::GetInputVector() const
{
	FRotator Rot = GetActorRotation();
	if (Controller)
	{
		Rot.Yaw = Controller->GetControlRotation().Yaw;
	}

	return FTransform(Rot).TransformVector(GetInputMove().GetSafeNormal2D());
}

FVector ABladeCharacter::GetShootLocation()
{
	return CameraTraceHit.Location;
}

void ABladeCharacter::CameraTrace()
{
	CameraTraceHit = FHitResult();
	if (auto PlayerController = Cast<APlayerController>(GetController()))
	{
		FVector WorldPosition;
		FVector WorldDirection;

		FVector2D ScreenPos;
		int ScreenSizeX = 0, ScreenSizeY = 0;
		PlayerController->GetViewportSize(ScreenSizeX, ScreenSizeY);

		UGameplayStatics::DeprojectScreenToWorld(PlayerController, FVector2D(ScreenSizeX * 0.5, ScreenSizeY * 0.5), WorldPosition, WorldDirection);
		FVector TraceLocation = WorldPosition + WorldDirection * 100000;
		TArray<AActor*> IgnoreActors;
		FHitResult Hit;
		if (UKismetSystemLibrary::LineTraceSingle(
			this, WorldPosition, TraceLocation,
			UEngineTypes::ConvertToTraceType(ECollisionChannel::ECC_Visibility),
			false, IgnoreActors, EDrawDebugTrace::None, Hit, true))
		{
			TraceLocation = Hit.Location;
			CameraTraceHit = Hit;
		}
		else
		{
			CameraTraceHit.Location = TraceLocation;
		}
	}
}

void ABladeCharacter::OnPointDamage(AActor* DamagedActor, float Damage, 
	AController* InstigatedBy, FVector HitLocation, UPrimitiveComponent* FHitComponent, 
	FName BoneName, FVector ShotFromDirection, const UDamageType* InDamageType, AActor* DamageCauser)
{
	auto AnimInst = GetAnimInstance();
	if (!AnimInst || !bHitAble) return;

	FVector ForceFromDir = ShotFromDirection.GetSafeNormal();
	HitNormal = FVector::ZeroVector;
	const FVector CauserDir = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	EHitAnimType AnimType = EHitAnimType::StandLight;
	const UBladeDamageType* DamageClassType = Cast<UBladeDamageType>(InDamageType);
	const auto DamageType = DamageClassType->DamageType;
	const float Force = DamageClassType->DamageImpulse;

	if (CanBlock(CauserDir))
	{
		if (DamageType == EDamageType::Melee_BreakBlock)
		{
			AnimType = EHitAnimType::BreakBlock;
		}
		else
		{
			AnimType = EHitAnimType::Blocked;
		}
	}
	else if (DamageType == Melee_Light)
	{
		AnimType = EHitAnimType::StandLight;
	}
	else if (DamageType == Melee_Heavy)
		AnimType = EHitAnimType::StandHeavy;
	else if (DamageType == Melee_Knock)
		AnimType = EHitAnimType::Knock;
	else if (DamageType == Bullet_Light)
		AnimType = EHitAnimType::Bullet;

	if (AnimType != EHitAnimType::Blocked)
	{
		SetHealth(Health - Damage);
	}

	if (Health <= 0)
	{
		if (DamageType != Melee_Knock)
			AnimType = EHitAnimType::Death;
	}

	const auto& RefSkeleton = AnimInst->CurrentSkeleton->GetReferenceSkeleton();
	const int HitBoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	const FVector HitDirInMeshSpace = GetMesh()->GetComponentTransform().InverseTransformVector(ForceFromDir);
	float MinCost = FLT_MAX;
	UAnimMontage* HitAnim = nullptr;
	FVector HitAnimDirection = FVector::Zero();
	for (const auto Anim : AnimInst->Hits)
	{
		if (const auto HitData = GetHitData(Anim))
		{
			if (AnimType == HitData->HitAnimType)
			{
				const int Depth = RefSkeleton.GetDepthBetweenBones(HitBoneIndex, RefSkeleton.FindBoneIndex(HitData->BoneName));
				if (Depth >= 0)
				{
					FVector AnimHitVec = HitData->Direction.GetSafeNormal();
					float Cost = 0;
					Cost += 1 - (AnimHitVec | HitDirInMeshSpace);
					Cost += Depth;
					if (Cost < MinCost)
					{
						MinCost = Cost;
						HitAnim = Anim;
						HitAnimDirection = AnimHitVec;
					}
				}
			}
		}
	}

	if (HitAnim)
	{
		if (HitAnim->HasRootMotion())
		{
			FVector RootMotionDir = HitAnim->ExtractRootMotionFromTrackRange(0, HitAnim->GetPlayLength()).GetLocation().GetSafeNormal2D();
			RootMotionDir = GetMesh()->GetComponentTransform().TransformVector(RootMotionDir);
			FVector HitMoveDir = - CauserDir;
			auto DeltaRot = FQuat::FindBetweenVectors(RootMotionDir, HitMoveDir);
			SmoothTeleport(GetActorLocation(), (DeltaRot * GetActorQuat()).Rotator());
			bMoveAble = false;
		}
		PlayAction(HitAnim);
		//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Play HitAnimation %s"), *HitAnim->GetName()));
	}
	else if(bUsePhysicsAnimation)
	{
		if (AnimType == EHitAnimType::Knock || Health <= 0)
		{
			SetRagdoll(true);
			FVector ForceVector = FRotator(DamageClassType->ImpulsePitch, (-CauserDir).Rotation().Yaw, 0).Vector();
			ForceVector *= Force;
			if (DamageClassType->ImpulseAtFullBody)
			{
				GetMesh()->AddImpulse(ForceVector, FName(), true);
			}
			else
			{
				GetMesh()->AddVelocityChangeImpulseAtLocation(ForceVector, HitLocation);
			}
		}
		else
		{
			if(GetMesh()->IsSimulatingPhysics(BoneName))
				GetMesh()->AddImpulseAtLocation(ShotFromDirection * Force, HitLocation, BoneName);
		/*	EnablePhysicsAnimation(true);
			FTimerHandle HitTimerHandle;
			GetWorldTimerManager().SetTimer(HitTimerHandle, FSimpleDelegate::CreateWeakLambda(this,
				[this, Impulse = -ShotFromDirection * Force, HitLocation, BoneName]()
				{
					GetMesh()->AddImpulseAtLocation(Impulse, HitLocation, BoneName);
				}), 0.1, false);

			GetWorldTimerManager().SetTimer(StopPhysAnimTimerHandle, FSimpleDelegate::CreateWeakLambda(this, [this]() { EnablePhysicsAnimation(false); }), 0.5, false);*/
		}
	}
}


void ABladeCharacter::SetHealth(float val)
{
	Health = val;

	if (!IsPlayerControlled())
	{
		HealthUIDisplayTime = 3;
		if (PlayerTitleComponent->Elements.Num() > 0)
		{
			UMaterialInstanceDynamic* MaterialInst = Cast<UMaterialInstanceDynamic>(PlayerTitleComponent->GetMaterial(0));
			if (!MaterialInst)
			{
				MaterialInst = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, PlayerTitleComponent->GetMaterial(0));
				PlayerTitleComponent->SetMaterial(0, MaterialInst);
			}

			if (MaterialInst)
			{
				MaterialInst->SetScalarParameterValue(FName(TEXT("Percentage")), Health / MaxHealth);
			}
		}
	}

	if (Health <= 0)
	{
		bHitAble = false;
		OnDead();
	}
}

void ABladeCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	OnTakePointDamage.AddDynamic(this, &ABladeCharacter::OnPointDamage);
	if(bUsePhysicsAnimation)
		InitializePhysicsAnimation();

	//GetArrowComponent()->SetHiddenInGame(false);
	MaxHealth = Health;
	for (auto& WeaponSlot : WeaponSlots)
	{
		if (WeaponSlot.WeaponClass)
		{
			FActorSpawnParameters ActorSpawnParameters;
			ActorSpawnParameters.Owner = this;
			WeaponSlot.Weapon = GetWorld()->SpawnActor<ABladeWeapon>(WeaponSlot.WeaponClass, ActorSpawnParameters);
			WeaponSlot.Weapon->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponSlot.AttachSocketName);
		}
	}
}

void ABladeCharacter::BeginPlay()
{
	Super::BeginPlay();
}

//////////////////////////////////////////////////////////////////////////
// Input

void ABladeCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	if (UEnhancedInputComponent* PlayerEnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		TArray<UObject*> LoadedObjects;
		FString ResourcePath("/Game/Blueprints/Input");
		if (EngineUtils::FindOrLoadAssetsByPath(ResourcePath, LoadedObjects, EngineUtils::ATL_Regular))
		{
			for (auto ObjectIter = LoadedObjects.CreateConstIterator(); ObjectIter; ++ObjectIter)
			{
				if (UInputAction* Action = Cast<UInputAction>(*ObjectIter))
				{
					//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("%s"), *Action->GetName()));
					PlayerEnhancedInputComponent->BindAction(Action, ETriggerEvent::Triggered, this, *Action->GetName());
				}
			}
		}
	}
}

void ABladeCharacter::SmoothTeleport(const FVector& Location, const FRotator& Rotation, float SmoothTime)
{
	if (auto AnimInst = GetAnimInstance())
	{
		const FTransform PreMeshWorldTransform = GetMesh()->GetComponentTransform();
		SetActorLocationAndRotation(Location, Rotation);
		const FTransform AfterMeshWorldTransform = GetMesh()->GetComponentTransform();
		AnimInst->RootOffset = PreMeshWorldTransform.GetRelativeTransform(AfterMeshWorldTransform);
		AnimInst->bHasRootOffset = true;
		AnimInst->SmoothTeleportBlendTime = SmoothTime;
		GetMesh()->TickPose(0, false);
		GetMesh()->RefreshBoneTransforms();
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(AnimInst, [AnimInst]() { AnimInst->bHasRootOffset = false; }));
	}
}

void ABladeCharacter::PushActionCommand(const FActionCommand& Cmd)
{
	CachedCommands.AddUnique(Cmd);
}

void ABladeCharacter::RemoveActionCommand(const FActionCommand& Cmd)
{
	CachedCommands.Remove(Cmd);
}

ABladeWeapon* ABladeCharacter::GetWeapon(int Index) const
{
	if (WeaponSlots.IsValidIndex(Index))
	{
		return WeaponSlots[Index].Weapon;
	}

	return nullptr;
}

void ABladeCharacter::Jump()
{
	if (bMoveAble)
	{
		Super::Jump();
		if (CanJump())
		{
			GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(GetAnimInstance()->Jump, FName(TEXT("DefaultSlot")), 0.1f);
		}
	}
}

float ABladeCharacter::GetNextComboTime(const UAnimSequenceBase* Animation)
{
	if (Animation == nullptr) return 0;
	
	FAnimNotifyContext NotifyContext;
	Animation->GetAnimNotifies(0, Animation->GetPlayLength(), NotifyContext);
	for (auto Notify : NotifyContext.ActiveNotifies)
	{
		if (Notify.GetNotify()->NotifyStateClass && Notify.GetNotify()->NotifyStateClass->IsA(UNextComboState::StaticClass()))
		{
			return Notify.GetNotify()->GetTriggerTime();
		}
	}

	return Animation->GetPlayLength();
}

void ABladeCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(InputMappingContext, 1);
		}
	}
}

void ABladeCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if(auto AnimInst = GetAnimInstance())
		AnimInst->PlaySlotAnimationAsDynamicMontage(AnimInst->Landed, AnimInst->InplaceSlot, 0.1f);
}

void ABladeCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void ABladeCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void ABladeCharacter::Move(const FInputActionValue& Value)
{
	auto Input2D = Value.Get<FVector2D>();
	if (Controller != nullptr && !Input2D.IsZero())
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
		// get forward vector
		const FVector XAxis = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		const FVector YAxis = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		AddMovementInput(XAxis, Input2D.X);
		AddMovementInput(YAxis, Input2D.Y);

		if (bMoveAble)
		{
			if (auto AnimInst = GetAnimInstance())
			{
				if (auto MontageInst = AnimInst->GetRootMotionMontageInstance())
				{
					AnimInst->Montage_Stop(0.3, MontageInst->Montage);
				}
			}
		}
	}
}

void ABladeCharacter::LookAt(const FInputActionValue& Value)
{
	auto MouseDelta = Value.Get<FVector2D>();

	AddControllerYawInput(MouseDelta.X * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
	AddControllerPitchInput(MouseDelta.Y * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

bool ABladeCharacter::IsSprint() const
{
	return bWantSprint && (GetCharacterMovement()->GetCurrentAcceleration() | GetActorForwardVector()) > 0.7;
}

void ABladeCharacter::OnDead_Implementation()
{
	if (auto AIController = GetController<AAIController>())
	{
		AIController->UnPossess();
		AIController->Destroy();
	}
}

void ABladeCharacter::PlayAction(UAnimMontage* Montage, bool bDisableRootMotion)
{
	if (GetLocalRole() == ROLE_AutonomousProxy)
	{
		PlayMontage(Montage, bDisableRootMotion);
		ServerPlayAction(Montage, bDisableRootMotion);
	}
	else if (GetLocalRole() == ROLE_Authority)
	{
		MultiPlayAction(Montage, bDisableRootMotion);
	}
}

void ABladeCharacter::PlayMontage(UAnimMontage* AnimMontage, bool bDisableRootMotion)
{
	if (auto AnimInst = GetAnimInstance())
	{
		if (AnimInst->Montage_Play(AnimMontage) > 0)
		{
			bMoveAble = false;
		}

		if (auto MontageInst = AnimInst->GetActiveInstanceForMontage(AnimMontage))
		{
			//MontageInst->Advance(0.001, nullptr, false);
			//AnimInst->TriggerAnimNotifies(0.001);

			if (bDisableRootMotion)
			{
				MontageInst->PushDisableRootMotion();
			}
		}
	}
}

void ABladeCharacter::ServerPlayAction_Implementation(UAnimMontage* Montage, bool bDisableRootMotion)
{
	MultiPlayAction(Montage, bDisableRootMotion);
}

void ABladeCharacter::MultiPlayAction_Implementation(UAnimMontage* Montage, bool bDisableRootMotion)
{
	if (GetLocalRole() != ROLE_AutonomousProxy)
	{
		PlayMontage(Montage, bDisableRootMotion);
	}
}

bool ABladeCharacter::IsAttacking() const
{
	return false;
}

static void GetBoneTrackData(UAnimSequenceBase* Animation, const FName& BoneName, TArray<FTransform>& BoneTransforms)
{
	const TArray<UAnimMetaData*>& MetaDatas = Animation->GetMetaData();
	for (auto& MetaData : MetaDatas)
	{
		if (UBoneTrackMetaData* TrackData = Cast<UBoneTrackMetaData>(MetaData))
		{
			if (TrackData->BoneName == BoneName)
			{
				BoneTransforms = TrackData->BoneTracks;
			}
		}
	}
}

void ABladeCharacter::PredictAttackHit(UAnimSequenceBase* Animation, int WeaponIndex) const
{
	if (auto AnimInst = GetAnimInstance())
	{
		ABladeWeapon* Weapon = WeaponSlots[WeaponIndex].Weapon;
		if (Weapon)
		{
			const float TimeStep = 1.0 / 60;
			float AnimLength = Animation->GetPlayLength();

			FAnimNotifyContext NotifyContext;
			Animation->GetAnimNotifies(0, Animation->GetPlayLength(), NotifyContext);
			float StartTime = 0;
			float EndTime = Animation->GetPlayLength();
			for (auto& NotifieRef : NotifyContext.ActiveNotifies)
			{
				if (NotifieRef.GetNotify()->NotifyName.ToString().Contains(TEXT("AttackState")))
				{
					StartTime = NotifieRef.GetNotify()->GetTriggerTime();
					EndTime = NotifieRef.GetNotify()->GetEndTriggerTime();
					break;
				}
			}
		
			TArray<UTrackCollisionComponent*> SelfCollisionComponents;
			Weapon->GetComponents(SelfCollisionComponents);
			if (SelfCollisionComponents.IsEmpty()) return;
			UTrackCollisionComponent* SelfCollision = SelfCollisionComponents[0];

			int32 BoneIndex = GetWeaponBoneIndex(WeaponIndex);
			TArray<FTransform> SelfWeaponTransforms;
			int StartFrame = FMath::RoundToInt(StartTime / TimeStep);
			int EndFrame = FMath::RoundToInt(EndTime / TimeStep);
			FTransform SocketLocalTransform = GetMesh()->GetSocketTransform(WeaponSlots[WeaponIndex].AttachSocketName).GetRelativeTransform(GetMesh()->GetBoneTransform(BoneIndex));
			FTransform RootTransform = GetMesh()->GetBoneTransform(0);
			FTransform RefTransform = GetAnimationBoneTransform(Animation, 0, 0);
			FTransform AnimToWorld = RefTransform.Inverse() * RootTransform;
			TArray<FTransform> BoneTransforms;
			GetBoneTrackData(Animation, GetMesh()->GetBoneName(BoneIndex), BoneTransforms);

			for (int i = StartFrame; i <= EndFrame; i++)
			{
				auto HandBoneTransform = BoneTransforms.IsValidIndex(i) ? BoneTransforms[i] : GetAnimationBoneTransform(Animation, BoneIndex, i * TimeStep);
				SelfWeaponTransforms.Add(SocketLocalTransform * HandBoneTransform * AnimToWorld);
			}

			FTransform LastTransform = SelfWeaponTransforms[0];

			ABladeCharacter* HitCharater = nullptr;
			TArray<AActor*> IgnoreActors;
			bool bSweepAttack = false;
			for (int i = 1; i < SelfWeaponTransforms.Num(); i++)
			{
				FTransform CurrentTransform = SelfWeaponTransforms[i];
				TArray<FHitResult> Hits;
				Weapon->SweepTraceCharacter(Hits, LastTransform, CurrentTransform);
				for (const auto& Hit : Hits)
				{
					if (!IgnoreActors.Contains(Hit.GetActor()))
					{
						IgnoreActors.Add(Hit.GetActor());
						if (ABladeCharacter* Ch = Cast<ABladeCharacter>(Hit.GetActor()))
						{
							//Ch->NotifyAttack(Hit, SampleTime - StartTime);
							HitCharater = Ch;
							FVector LastPosInLocal = (SelfCollision->GetRelativeTransform() * LastTransform).GetRelativeTransform(SelfCollision->GetRelativeTransform() * CurrentTransform).GetLocation();
							bSweepAttack = !LastPosInLocal.IsNearlyZero() && FMath::Abs(LastPosInLocal.GetSafeNormal().X) < 0.5f;
							goto CharacterChecked;
						}
					}
				}

				LastTransform = CurrentTransform;
			}

		CharacterChecked:
			UKismetSystemLibrary::PrintString(this, bSweepAttack ? TEXT("Sweep") : TEXT("Stab"));
			if (HitCharater)
			{
				for (auto& WeaponTransform : SelfWeaponTransforms)
				{
					WeaponTransform = SelfCollision->GetRelativeTransform() * WeaponTransform;
				}

				HitCharater->PlayParryAnim(SelfCollision, SelfWeaponTransforms, StartFrame * TimeStep, bSweepAttack);
			}
		}
	}
}

void ABladeCharacter::PlayParryAnim(UPrimitiveComponent* PrimitiveComponent, const TArray<FTransform>& AttackShapeTracks, float StartTime, bool bSweep)
{
	if (AttackShapeTracks.IsEmpty() || !PrimitiveComponent) return;

	if (auto AnimInst = GetAnimInstance())
	{
		const float TimeStep = 1.0 / 60;
		const FCollisionShape AttackShape = PrimitiveComponent->GetCollisionShape();
		const auto& ParryAnimations = AnimInst->ParryAnimations;
		for (int WeaponIndex = 0; WeaponIndex < WeaponSlots.Num(); WeaponIndex++)
		{
			auto Weapon = WeaponSlots[WeaponIndex].Weapon;
			if(!Weapon->Mesh->GetSkeletalMeshAsset()) 
				continue;

			int32 WeaponBoneIndex = GetWeaponBoneIndex(WeaponIndex);
			FName WeaponBoneName = GetMesh()->GetBoneName(WeaponBoneIndex);
			FTransform SocketLocalTransform = GetMesh()->GetSocketTransform(WeaponSlots[WeaponIndex].AttachSocketName).GetRelativeTransform(GetMesh()->GetBoneTransform(WeaponBoneIndex));
			FTransform RootTransform = GetMesh()->GetBoneTransform(0);
			TArray<UTrackCollisionComponent*> WeaponCollisionComponents;
			Weapon->GetComponents(WeaponCollisionComponents);
			if (WeaponCollisionComponents.IsEmpty()) 
				continue;

			TArray<TArray<FTransform>> ParryWeaponTracks;
			ParryWeaponTracks.SetNum(ParryAnimations.Num());
			TArray<int> ParryStartFrames;
			ParryStartFrames.SetNum(ParryAnimations.Num());
			for (int i = 0; i < ParryAnimations.Num(); i++)
			{
				UAnimMontage* ParryAnim = ParryAnimations[i];
				FTransform ParryRefTransform = GetAnimationBoneTransform(ParryAnim, 0, 0);
				FTransform ParryAnimToWorld = ParryRefTransform.Inverse() * RootTransform;

				FAnimNotifyContext NotifyContext;
				ParryAnim->GetAnimNotifies(0, ParryAnim->GetPlayLength(), NotifyContext);
				float ParryStartTime = 0, ParryEndTime = ParryAnim->GetPlayLength();
				for (auto& NotifieRef : NotifyContext.ActiveNotifies)
				{
					if (NotifieRef.GetNotify()->NotifyName == TEXT("ParryState_C"))
					{
						ParryStartTime = NotifieRef.GetNotify()->GetTriggerTime();
						ParryEndTime = NotifieRef.GetNotify()->GetEndTriggerTime();
					}
				}
				int ParryStartFrame = FMath::RoundToInt(ParryStartTime / TimeStep);
				int ParryEndFrame = FMath::RoundToInt(ParryEndTime / TimeStep);
				ParryStartFrames[i] = ParryStartFrame;

				TArray<FTransform> WeaponBoneTransforms;
				GetBoneTrackData(ParryAnim, WeaponBoneName, WeaponBoneTransforms);

				for (int ParryFrame = ParryStartFrame; ParryFrame <= ParryEndFrame; ParryFrame++)
				{
					FTransform HandBoneTransform = WeaponBoneTransforms.IsEmpty() ? GetAnimationBoneTransform(ParryAnim, WeaponBoneIndex, ParryFrame * TimeStep) : WeaponBoneTransforms[ParryFrame];
					FTransform ParryTransform = SocketLocalTransform * HandBoneTransform * ParryAnimToWorld;
					ParryWeaponTracks[i].Add(ParryTransform);
				}
			}

			FTransform LastAttackShapeTransform = AttackShapeTracks[0];
			for (int AttackFrame = 1; AttackFrame < AttackShapeTracks.Num(); AttackFrame++)
			{
				float SampleTime = TimeStep * AttackFrame + StartTime;
				FTransform AttackShapeTransform = AttackShapeTracks[AttackFrame];
				FVector AttackDirection = AttackShapeTransform.GetLocation() - LastAttackShapeTransform.GetLocation();

				UAnimMontage* BestParryAnim = nullptr;
				float ParryTime = 0;
				FTransform ParryStartTransform, ParryEndTransform;
				FCollisionShape ParryedShape;
				float MinCost = FLT_MAX;
				for (int j = 0; j < ParryAnimations.Num(); j++)
				{
					UAnimMontage* ParryAnim = ParryAnimations[j];
					auto& ParryWeaponTrack = ParryWeaponTracks[j];
					int ParryEndFrame = FMath::Min(ParryWeaponTrack.Num(), FMath::RoundToInt(SampleTime/ TimeStep) - ParryStartFrames[j] + 1);
					for (auto CollisionComponent : WeaponCollisionComponents)
					{
						auto ParryShape = CollisionComponent->GetCollisionShape();
						FTransform CollisionLocalTransform = CollisionComponent->GetRelativeTransform();;

						if (bSweep)
						{
							for (int k = 0; k < ParryEndFrame; k++)
							{
								//int k = ParryWeaponTrack.Num() - 1;
								FTransform ParryCurTransform = CollisionLocalTransform * ParryWeaponTrack[k];
								FVector ImpactLoc, ImpactNorm;
								if (SweepCheck(ParryShape, ParryCurTransform, AttackShape, LastAttackShapeTransform, AttackDirection, ImpactLoc, ImpactNorm))
								{
									float Cost = 0;
									//Cost += IterTime;
									Cost += AttackShapeTransform.InverseTransformPosition(ImpactLoc).Size() / AttackShape.GetExtent().X
										+ ParryCurTransform.InverseTransformPosition(ImpactLoc).Size() / ParryShape.GetExtent().X;
									Cost -= FMath::Abs(ParryCurTransform.GetUnitAxis(EAxis::X) | AttackShapeTransform.GetUnitAxis(EAxis::Z));
									if (Cost < MinCost)
									{
										MinCost = Cost;
										BestParryAnim = ParryAnim;
										ParryTime = TimeStep * (ParryStartFrames[j] + k);
										ParryStartTransform = ParryCurTransform;
										ParryEndTransform = ParryCurTransform;
										ParryedShape = ParryShape;
									}
								}
							}
						}
						else
						{
							FTransform ParryLastTransform = CollisionLocalTransform * ParryWeaponTrack[0];

							for (int k = 1; k < ParryEndFrame; k++)
							{
								FTransform ParryCurTransform = CollisionLocalTransform * ParryWeaponTrack[k];
								FHitResult ParryHit;
								FVector ParryDirection = ParryCurTransform.GetLocation() - ParryLastTransform.GetLocation();
								FVector ImpactLoc, ImpactNorm;
								if (SweepCheck(AttackShape, AttackShapeTransform, ParryShape, ParryLastTransform, ParryDirection, ImpactLoc, ImpactNorm))
								{
									float Cost = AttackShapeTransform.InverseTransformPosition(ImpactLoc).Size();
									if (Cost < MinCost)
									{
										MinCost = Cost;
										BestParryAnim = ParryAnim;
										ParryTime = TimeStep * (ParryStartFrames[j] + k);
										ParryStartTransform = ParryLastTransform;
										ParryEndTransform = ParryCurTransform;
										ParryedShape = ParryShape;
									}
								}

								ParryLastTransform = ParryCurTransform;
							}
						}
					}
				}

				if (BestParryAnim)
				{
					//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("%f"), MinCost));

					float DelayTime = SampleTime - ParryTime;
				
					if (DelayTime > 0)
					{
						FTimerHandle TimerHandle;
						GetWorldTimerManager().SetTimer(TimerHandle,
							FTimerDelegate::CreateWeakLambda(this, [this, BestParryAnim]() { PlayAction(BestParryAnim); }), DelayTime, false);
					}
					else
					{
						AnimInst->Montage_Play(BestParryAnim, 1, EMontagePlayReturnType::MontageLength, -DelayTime);
					}

					UKismetSystemLibrary::DrawDebugBox(this, ParryEndTransform.GetLocation(), ParryedShape.GetExtent(), FLinearColor::Green, ParryEndTransform.Rotator(), 3);
					UKismetSystemLibrary::DrawDebugBox(this, AttackShapeTransform.GetLocation(), AttackShape.GetExtent(), FLinearColor::Red, AttackShapeTransform.Rotator(), 3);
					return;
				}

				LastAttackShapeTransform = AttackShapeTransform;
			}
		}
	}
}

#pragma optimize("", off)
void ABladeCharacter::ParryProject(UPrimitiveComponent* PrimitiveComponent, class UProjectileMovementComponent* ProjectComponent)
{
	FVector StartPosition = PrimitiveComponent->GetComponentLocation();
	FVector Velocity = ProjectComponent->Velocity;
	if (Velocity.IsNearlyZero()) return;

	//PlayParryAnim(PrimitiveComponent, Track, 0, false);
	float HalfCapsuleHeight = (GetActorLocation() - StartPosition).Size() * 0.5f + 50;
	float HalfCapsuleRadius = 0.01;//PrimitiveComponent->GetCollisionShape().GetSphereRadius();
	FVector CapsuleLocation = StartPosition + Velocity.GetSafeNormal()* HalfCapsuleHeight;
	auto CapsuleRotation = FRotationMatrix::MakeFromZX(Velocity.GetSafeNormal(), FVector::UpVector).Rotator();
	FTransform CapsuleTransform(CapsuleRotation, CapsuleLocation);
	UKismetSystemLibrary::DrawDebugCapsule(this, CapsuleLocation, HalfCapsuleHeight, HalfCapsuleRadius, CapsuleRotation, FLinearColor::Blue, 3);

	FCollisionShape AttackShape;
	AttackShape.SetCapsule(HalfCapsuleRadius,HalfCapsuleHeight);

	if (auto AnimInst = GetAnimInstance())
	{
		const float TimeStep = 1.0 / 60;
		const auto& ParryAnimations = AnimInst->ParryAnimations;
		for (int WeaponIndex = 0; WeaponIndex < WeaponSlots.Num(); WeaponIndex++)
		{
			auto Weapon = WeaponSlots[WeaponIndex].Weapon;
			if (!Weapon->Mesh->GetSkeletalMeshAsset())
				continue;

			int32 WeaponBoneIndex = GetWeaponBoneIndex(WeaponIndex);
			FName WeaponBoneName = GetMesh()->GetBoneName(WeaponBoneIndex);
			FTransform SocketLocalTransform = GetMesh()->GetSocketTransform(WeaponSlots[WeaponIndex].AttachSocketName).GetRelativeTransform(GetMesh()->GetBoneTransform(WeaponBoneIndex));
			FTransform RootTransform = GetMesh()->GetBoneTransform(0);
			TArray<UTrackCollisionComponent*> WeaponCollisionComponents;
			Weapon->GetComponents(WeaponCollisionComponents);
			if (WeaponCollisionComponents.IsEmpty())
				continue;

			for (int i = 0; i < ParryAnimations.Num(); i++)
			{
				UAnimMontage* ParryAnim = ParryAnimations[i];
				FTransform ParryRefTransform = GetAnimationBoneTransform(ParryAnim, 0, 0);
				FTransform ParryAnimToWorld = ParryRefTransform.Inverse() * RootTransform;

				FAnimNotifyContext NotifyContext;
				ParryAnim->GetAnimNotifies(0, ParryAnim->GetPlayLength(), NotifyContext);
				float ParryStartTime = 0, ParryEndTime = ParryAnim->GetPlayLength();
				for (auto& NotifieRef : NotifyContext.ActiveNotifies)
				{
					if (NotifieRef.GetNotify()->NotifyName == TEXT("ParryState_C"))
					{
						ParryStartTime = NotifieRef.GetNotify()->GetTriggerTime();
						ParryEndTime = NotifieRef.GetNotify()->GetEndTriggerTime();
					}
				}
				int ParryStartFrame = FMath::RoundToInt(ParryStartTime / TimeStep);
				int ParryEndFrame = FMath::RoundToInt(ParryEndTime / TimeStep);

				TArray<FTransform> WeaponBoneTransforms;
				GetBoneTrackData(ParryAnim, WeaponBoneName, WeaponBoneTransforms);

				for (auto CollisionComponent : WeaponCollisionComponents)
				{
					FTransform ParryLastTransform;
					for (int ParryFrame = ParryStartFrame; ParryFrame <= ParryEndFrame; ParryFrame++)
					{
						FTransform HandBoneTransform = WeaponBoneTransforms.IsEmpty() ? GetAnimationBoneTransform(ParryAnim, WeaponBoneIndex, ParryFrame * TimeStep) : WeaponBoneTransforms[ParryFrame];
						FTransform ParryTransform = SocketLocalTransform * HandBoneTransform * ParryAnimToWorld;

						auto ParryShape = CollisionComponent->GetCollisionShape();
						FTransform CollisionLocalTransform = CollisionComponent->GetRelativeTransform();

						FTransform ParryCurTransform = CollisionLocalTransform * ParryTransform;

						if (ParryFrame != ParryStartFrame)
						{
							FHitResult ParryHit;
							FVector ParryDirection = ParryCurTransform.GetLocation() - ParryLastTransform.GetLocation();

							FVector ImpactLoc, ImpactNorm;
							if (SweepCheck(AttackShape, CapsuleTransform, ParryShape, ParryLastTransform, ParryDirection, ImpactLoc, ImpactNorm))
							{
								float ParryedAnimTime = ParryFrame * TimeStep;
								float ProjectTime = (ImpactLoc - StartPosition).Size()/Velocity.Size();
								float DelayTime = ProjectTime - ParryedAnimTime;

								if (DelayTime > 0)
								{
									FTimerHandle TimerHandle;
									GetWorldTimerManager().SetTimer(TimerHandle,
										FTimerDelegate::CreateWeakLambda(this, [this, ParryAnim]() { PlayAction(ParryAnim); }), DelayTime, false);

									FTimerHandle TimerHandle2;
									GetWorldTimerManager().SetTimer(TimerHandle2,
										FTimerDelegate::CreateWeakLambda(CollisionComponent, [CollisionComponent, ProjectComponent, BounceNormal = (ParryDirection/ TimeStep - Velocity).GetSafeNormal()]()
										{
											ProjectComponent->Velocity = BounceNormal * ProjectComponent->Velocity.Size();
											UKismetSystemLibrary::DrawDebugBox(CollisionComponent, CollisionComponent->GetComponentLocation(), CollisionComponent->GetCollisionShape().GetExtent(), FLinearColor::Yellow, CollisionComponent->GetComponentRotation(), 3);
										}),
										ProjectTime, false);

									TArray<AActor*> IgnoreActors;
									FHitResult Hit;
									UKismetSystemLibrary::BoxTraceSingle(this,
										ParryLastTransform.GetLocation(), ParryCurTransform.GetLocation(),
										ParryShape.GetExtent(), ParryCurTransform.Rotator(), ETraceTypeQuery::TraceTypeQuery1,
										false, IgnoreActors, EDrawDebugTrace::ForDuration, Hit, true, FLinearColor::Green);
									//UKismetSystemLibrary::DrawDebugBox(this, ParryCurTransform.GetLocation(), ParryShape.GetExtent(), FLinearColor::Green, ParryCurTransform.Rotator(), 6);
									UKismetSystemLibrary::DrawDebugSphere(this, StartPosition + Velocity * ProjectTime, PrimitiveComponent->GetCollisionShape().GetSphereRadius(), 12, FLinearColor::Red, 3);
									return;
								}
							}
						}

						ParryLastTransform = ParryCurTransform;
					}
				}
			}
		}
	}
}
#pragma optimize("", on)
int32 ABladeCharacter::GetWeaponBoneIndex(int WeaponIndex) const
{
	FName AttachBoneName = WeaponSlots[WeaponIndex].AttachSocketName;
	auto BoneIndex = GetMesh()->GetBoneIndex(AttachBoneName);
	if (BoneIndex == -1)
	{
		if (auto AttachSocket = GetMesh()->GetSkeletalMeshAsset()->FindSocket(AttachBoneName))
		{
			AttachBoneName = AttachSocket->BoneName;
		}

		BoneIndex = GetMesh()->GetBoneIndex(AttachBoneName);
	}

	return BoneIndex;
}

void ABladeCharacter::DebugDrawWeaponTrack(UAnimSequenceBase* Animation, float StartTime, float EndTime, int WeaponIndex, float TimeStep) const
{
#if ENABLE_DRAW_DEBUG
	if (WeaponSlots.IsValidIndex(WeaponIndex) && WeaponSlots[WeaponIndex].Weapon)
	{
		TArray<UTrackCollisionComponent*> CollisionComponents;
		WeaponSlots[WeaponIndex].Weapon->GetComponents(CollisionComponents);
		int32 BoneIndex = GetWeaponBoneIndex(WeaponIndex);
		FTransform WeaponLocal = GetMesh()->GetSocketTransform(WeaponSlots[WeaponIndex].AttachSocketName) .GetRelativeTransform(GetMesh()->GetBoneTransform(BoneIndex));
		FTransform RootTransform = GetMesh()->GetBoneTransform(0);
		FTransform RefTransform = GetAnimationBoneTransform(Animation, 0, StartTime);
		FTransform AnimToWorld = RefTransform.Inverse() * RootTransform;
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(const_cast<ABladeCharacter*>(this));

		for (auto TrackCollistion : CollisionComponents)
		{
			auto ScaledExtent = TrackCollistion->GetScaledBoxExtent();
			FTransform CollistionLocal = TrackCollistion->GetRelativeTransform() * WeaponLocal;
			FTransform LastTransform = CollistionLocal * GetAnimationBoneTransform(Animation, BoneIndex, StartTime) * AnimToWorld;
			for (float SampleTime = StartTime + TimeStep; SampleTime <= EndTime; SampleTime += TimeStep)
			{
				FTransform CurrentTransform = CollistionLocal * GetAnimationBoneTransform(Animation, BoneIndex, SampleTime) * AnimToWorld;
				FHitResult Hit;
				UKismetSystemLibrary::BoxTraceSingleForObjects(
					this,
					LastTransform.GetLocation(), CurrentTransform.GetLocation(), ScaledExtent,
					CurrentTransform.GetRotation().Rotator(),
					TrackCollistion->TraceChannels,
					false, IgnoreActors,
					EDrawDebugTrace::Type::ForDuration,
					Hit, true, FLinearColor::Green);

				LastTransform = CurrentTransform;
			}
		}
	}
#endif
}

bool ABladeCharacter::IsWeaponSweeping(UAnimSequenceBase* Animation, int WeaponIndex, float Time) const
{
	if (WeaponSlots.IsValidIndex(WeaponIndex) && WeaponSlots[WeaponIndex].Weapon)
	{
		TArray<UTrackCollisionComponent*> CollisionComponents;
		WeaponSlots[WeaponIndex].Weapon->GetComponents(CollisionComponents);
		int32 BoneIndex = GetWeaponBoneIndex(WeaponIndex);
		FTransform WeaponLocal = GetMesh()->GetSocketTransform(WeaponSlots[WeaponIndex].AttachSocketName).GetRelativeTransform(GetMesh()->GetBoneTransform(BoneIndex));;

		if (!CollisionComponents.IsEmpty())
		{
			auto TrackCollistion = CollisionComponents[0];
			FTransform CollistionLocal = TrackCollistion->GetRelativeTransform() * WeaponLocal;
			float TimeStep = 1.0 / 30;
			FTransform LastTransform = CollistionLocal * GetAnimationBoneTransform(Animation, BoneIndex, Time - TimeStep);
			FTransform CurrentTransform = CollistionLocal * GetAnimationBoneTransform(Animation, BoneIndex, Time);
			FVector LastPosInLocal = LastTransform.GetRelativeTransform(CurrentTransform).GetLocation();
			if (!LastPosInLocal.IsNearlyZero() && FMath::Abs(LastPosInLocal.GetSafeNormal().X) < 0.5f)
			{
				return true;
			}
		}
	}

	return false;
}

UBladeAnimInstance* ABladeCharacter::GetAnimInstance() const
{
	return  Cast<UBladeAnimInstance>(GetMesh()->GetAnimInstance());
}

bool ABladeCharacter::IsExcuteable() const
{
	return !GetMesh()->IsSimulatingPhysics() && !GetAnimInstance()->IsAnyMontagePlaying() && Health > 0;
}

void ABladeCharacter::SelectTarget()
{
	const FVector ControlDir = GetControlRotation().Vector();

	TArray<ACharacter*> IgnoreList;
	IgnoreList.Add(this);
	IgnoreList.Append(TargetList);
	SelectedTarget = nullptr;
	float MinCost = FLT_MAX;
	for (ACharacter* Pawn : TActorRange<ACharacter>(GetWorld()))
	{
		if (!IgnoreList.Contains(Pawn))
		{
			FVector PawnVector = Pawn->GetActorLocation() - GetActorLocation();
			const float Dot = PawnVector.GetSafeNormal2D() | GetControlRotation().Vector();
			const float Cost = PawnVector.Size() - 500 * Dot;
			if (Dot > 0 && Cost < MinCost)
			{
				MinCost = Cost;
				SelectedTarget = Pawn;
			}
		}
	}

	if (!SelectedTarget)
	{
		TargetList.Empty();
	}
	else
	{
		TargetList.Add(SelectedTarget);
	}
}

ACharacter* ABladeCharacter::GetAutoTarget() const
{
	if (SelectedTarget)
	{
		return SelectedTarget;
	}

	ACharacter* AutoTarget = nullptr;

	FVector SelectDirection = GetInputVector().IsZero() ? GetActorForwardVector() : GetInputVector();
	float MinDist = FLT_MAX;
	for (ACharacter* Pawn : TActorRange<ACharacter>(GetWorld()))
	{
		if (Pawn != this)
		{
			FVector PawnVector = Pawn->GetActorLocation() - GetActorLocation();
			float Angle = FMath::Acos(PawnVector.GetSafeNormal2D() | SelectDirection);
			Angle = Angle / PI * 180;
			const float Dist = PawnVector.Size();
			
			if (Angle < 30 && Dist < MinDist)
			{
				MinDist = Dist;
				AutoTarget = Pawn;
			}
		}
	}

	if (!IsValid(AutoTarget))
		AutoTarget = nullptr;

	return AutoTarget;
}

FTransform ABladeCharacter::FindMotionWarpingTargetTransform(const ACharacter* AutoTarget, float MaxAngle, float MaxDistance, float ReachedRadius) const
{
	FTransform TargetTransform;
	if (AutoTarget)
	{
		FVector TargetLocation = AutoTarget->GetActorLocation();
		TargetLocation = GetActorTransform().InverseTransformPosition(TargetLocation);
		const float Dist = FMath::Clamp(TargetLocation.Size2D() - ReachedRadius, 1, MaxDistance);
		FRotator ClampedRot = TargetLocation.GetSafeNormal2D().Rotation();
		ClampedRot.Yaw = FMath::Clamp(ClampedRot.Yaw, - MaxAngle, MaxAngle);
		TargetLocation = ClampedRot.Vector() * Dist;
		TargetTransform = FTransform(ClampedRot, TargetLocation);
		TargetTransform = TargetTransform * GetActorTransform();
	}
	
	return TargetTransform;
}

void ABladeCharacter::Dodge()
{
	if (bMoveAble && GetAnimInstance()->Dodges.Num() > 0)
	{
		const float InputYaw = GetActorTransform().InverseTransformVector(GetInputVector()).Rotation().Yaw;
		const int DirectionIndex = FMath::Abs(InputYaw) > 120? 2 : (FMath::Abs(InputYaw) < 60 ? 0 : InputYaw > 0? 1 : 3);

		if (const auto AnimSeq = GetAnimInstance()->Dodges[DirectionIndex])
		{
			FVector RootTrans = AnimSeq->ExtractRootMotionFromTrackRange(0, AnimSeq->GetPlayLength()).GetLocation();
			RootTrans = GetMesh()->GetComponentTransform().TransformVector(RootTrans);
			const auto DeltaRot = FQuat::FindBetween(RootTrans.GetSafeNormal2D(), GetInputVector());
			SmoothTeleport(GetActorLocation(), (DeltaRot * GetActorQuat()).Rotator());
			bMoveAble = false;
			PlayAction(AnimSeq);
		}
	}
}

void ABladeCharacter::Attack(UInputAction* InputAction)
{
	if(auto AnimInst = GetAnimInstance())
	{
		if (GetCharacterMovement()->IsWalking())
		{
			if (bMoveAble)
			{
				if (auto pVal = AnimInst->ActionAnimationMap.Find(InputAction))
				{
					PlayAction(*pVal, true);
					return;
				}
				 
			}

			for (int i = 0; i < CachedCommands.Num(); i++)
			{
				if (CachedCommands[i].InputAction == InputAction)
				{
					PlayAction(CachedCommands[i].Animation, true);
					CachedCommands.Empty();
					break;
				}
			}
		}
	}
}

void ABladeCharacter::SetBlock(bool bEnable)
{
	if (auto AnimInst = GetAnimInstance())
	{
		if (bEnable)
		{
			if (bMoveAble && !GetBlock())
			{
				AnimInst->StopAllMontages(0.2);
				PlayAction(AnimInst->Block);
			}
		}
		else
		{
			if (AnimInst->StopBlock)
				PlayAction(AnimInst->StopBlock);
			else
				AnimInst->Montage_Stop(0.25, AnimInst->Block);
		}
	}
}

bool ABladeCharacter::GetBlock() const
{
	return GetAnimInstance()->Montage_IsPlaying(GetAnimInstance()->Block);
}

bool ABladeCharacter::CanBlock(const FVector& HitFromDir) const
{
	return GetBlock() && (HitFromDir | GetActorForwardVector()) > FMath::Cos(BlockAngle/180 * PI);
}

void ABladeCharacter::Execution()
{
}
