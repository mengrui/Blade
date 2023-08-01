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
}

void ABladeCharacter::EnablePhysicsAnimation(bool bTrue)
{
	if (bTrue)
	{
		GetMesh()->SetConstraintProfileForAll(TEXT("HingesOnly"));
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PhysicsControlComponent->SetBodyModifiersMovementType(AllBodyModifiers.Names, EPhysicsMovementType::Simulated);
		PhysicsControlComponent->SetControlsEnabled(AllWorldSpaceControls.Names, true);
		PhysicsControlComponent->SetControlsEnabled(AllParentSpaceControls.Names, false);
		PhysicsControlComponent->SetBodyModifiersGravityMultiplier(AllBodyModifiers.Names, 0);
	}
	else
	{
		GetMesh()->SetConstraintProfileForAll(TEXT("None"), true);
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
		GetMesh()->SetConstraintProfileForAll(TEXT("None"), true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		if (!PhysicsControlComponent->GetAllControlNames().IsEmpty())
		{
			PhysicsControlComponent->SetControlsEnabled(AllWorldSpaceControls.Names, false);
			PhysicsControlComponent->SetControlsEnabled(AllParentSpaceControls.Names, true);
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
			GetWorldTimerManager().SetTimerForNextTick([AnimInst]()
				{
					AnimInst->bRagdoll = true;
				});
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

	ExecutedCachedInput();

	if (bWantBlock != GetBlock())
	{
		SetBlock(bWantBlock);
	}

	//if (GetBlock())
	//{
	//	GetCharacterMovement()->MaxWalkSpeed = 0;
	//}
	//else
	//{
	//	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
	//}

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

	//CameraTrace();

	TickExecution();

	if (auto Ch = const_cast<ABladeCharacter*>(Cast<ABladeCharacter>(SelectedTarget)))
	{
		if (!Ch->IsPlayerControlled())
		{
			Ch->HealthUIDisplayTime = 2;
		}
	}
}

static const FVector HitVectors[24] = {
	FVector(1, 0, 0),FVector(1, 1, 0), FVector(0,1,  0), FVector(-1, 1, 0), FVector(-1, 0, 0), FVector(-1, -1, 0), FVector(0,-1,  0), FVector(1, -1, 0),
	FVector(1, 0, 1),FVector(1, 1, 1), FVector(0,1,  1), FVector(-1, 1, 1), FVector(-1, 0, 1), FVector(-1, -1, 1), FVector(0,-1,  1), FVector(1, -1, 1),
	FVector(1, 0, -1),FVector(1, 1, -1), FVector(0,1,  -1), FVector(-1, 1, -1), FVector(-1, 0, -1), FVector(-1, -1, -1), FVector(0,-1,  -1), FVector(1, -1, -1),
};

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

	return FTransform(Rot).TransformVector(InputVector.GetSafeNormal2D());
}

bool ABladeCharacter::IsInState(ECharacterState State) const
{
	if (auto AnimInst = GetAnimInstance())
	{
		FName CurveName = UEnum::GetValueAsName(State);
		return AnimInst->GetCurveValue(CurveName) > 0.5;
	}

	return false;
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
	const FVector CauserDir = (DamageCauser->GetOwner()->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	EHitAnimType AnimType = EHitAnimType::StandLight;
	const UBladeDamageType* DamageClassType = Cast<UBladeDamageType>(InDamageType);
	const auto DamageType = DamageClassType->DamageType;
	const float Force = DamageClassType->DamageImpulse;
	if (CanBlock(CauserDir))
	{
		if (DamageType == EDamageType::Melee_Light)
		{
			AnimType = EHitAnimType::Blocked;
		}
		else
		{
			AnimType = EHitAnimType::BreakBlock;
		}
	}
	else if (DamageType == Melee_Light)
	{
		AnimType = EHitAnimType::StandLight;
	}
	else if (DamageType == Melee_Heavy)
		AnimType = EHitAnimType::StandHeavy;
	else if (DamageType == Melee_Float)
		AnimType = EHitAnimType::Float;
	else if (DamageType == Melee_Knock)
		AnimType = EHitAnimType::Knock;
	else if (DamageType == Bullet_Light)
		AnimType = EHitAnimType::Bullet;

	if (AnimType != EHitAnimType::Blocked)
	{
		SetHealth(Health - Damage);
	}
	const bool bSnapToCauser = AnimType == EHitAnimType::StandHeavy || AnimType == EHitAnimType::Float || AnimType == EHitAnimType::Knock;
	if (bSnapToCauser)
	{
		ForceFromDir = CauserDir;
	}

	const auto& RefSkeleton = AnimInst->CurrentSkeleton->GetReferenceSkeleton();
	const int HitBoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	const FVector HitDirInMeshSpace = GetMesh()->GetComponentTransform().InverseTransformVector(ForceFromDir);
	float MinCost = FLT_MAX;
	UAnimSequenceBase* HitAnim = nullptr;
	FVector HitAnimDirection = FVector::Zero();
	if (Health > 0)
	{
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
	}
	else
	{
		float Yaw = HitDirInMeshSpace.Rotation().Yaw;
		Yaw += 45;
		if (Yaw < 0)
		{
			Yaw += 360;
		}
		int DeadAnimIndex = Yaw / 90;
		if (AnimType == EHitAnimType::StandLight && AnimInst->Deads.IsValidIndex(DeadAnimIndex))
		{
			HitAnim = AnimInst->Deads[DeadAnimIndex];
			HitAnimDirection = FRotator(0, DeadAnimIndex * 90, 0).Vector();
		}
	}

	if (HitAnim)
	{
		if (bSnapToCauser)
		{
			const auto DeltaRot = FQuat::FindBetween(HitAnimDirection, HitDirInMeshSpace);
			SmoothTeleport(GetActorLocation(), (DeltaRot * GetActorQuat()).Rotator());
		}

		bMoveAble = false;
		PlayAction(HitAnim);
		GetCharacterMovement()->Velocity = FVector::ZeroVector;
		if (AnimType == EHitAnimType::Float)
		{
			GetCharacterMovement()->AddImpulse(FVector(0, 0, 1) * Force, true);
		}
		else
		{
			GetCharacterMovement()->AddImpulse(-CauserDir * Force, true);
		}
		
		//UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Play HitAnimation %s"), *HitAnim->GetName()));
	}
	else
	{
		if (AnimType == EHitAnimType::Float || AnimType == EHitAnimType::Knock || Health <= 0)
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
		else if (bUsePhysicsAnimation)
		{
			EnablePhysicsAnimation(true);
			FTimerHandle HitTimerHandle;
			GetWorldTimerManager().SetTimer(HitTimerHandle, FSimpleDelegate::CreateWeakLambda(this,
				[this, Impulse = -ShotFromDirection * Force, HitLocation, BoneName]()
				{
					GetMesh()->AddImpulseAtLocation(Impulse, HitLocation, BoneName);
				}), 0.1, false);

			GetWorldTimerManager().SetTimer(StopPhysAnimTimerHandle, FSimpleDelegate::CreateWeakLambda(this, [this]() { EnablePhysicsAnimation(false); }), 0.5, false);
		}
	}

	if (AnimType == EHitAnimType::Blocked || AnimType == EHitAnimType::BreakBlock)
	{
		auto Matrix = FRotationMatrix::MakeFromZ(ShotFromDirection);
		Matrix.SetOrigin(HitLocation);
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BlockEffect, FTransform(Matrix));
	}

	/*if (AnimType != EHitAnimType::Blocked)
	{
		auto Matrix = FRotationMatrix::MakeFromXZ(DamageCauser->GetActorForwardVector(), ShotFromDirection);
		Matrix.SetOrigin(HitLocation);
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), BloodEffect, FTransform(Matrix));
	}*/
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

void ABladeCharacter::ExecutedCachedInput()
{
	for (int i = CachedInputs.Num() - 1; i >= 0; i--)
	{
		const auto& Input = CachedInputs[i];
		if (Input.Time + CacheInputTime < GetWorld()->GetTimeSeconds())
		{
			CachedInputs.RemoveAt(i);
		}
		else
		{
			for (int j = 0; j < CachedCommands.Num(); j++)
			{
				const auto& Cmd = CachedCommands[j];
				if (Cmd.InputIndex == Input.Index)
				{
					PlayAction(Cmd.Animation);
					CachedCommands.Empty();
					CachedInputs.Empty();
					return;
				}
			}
		}
	}
}

void ABladeCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	OnTakePointDamage.AddDynamic(this, &ABladeCharacter::OnPointDamage);
	InitializePhysicsAnimation();

	//GetArrowComponent()->SetHiddenInGame(false);
	MaxHealth = Health;
	GetCharacterMovement()->MaxWalkSpeed = RunSpeed;
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
	const FTransform PreMeshWorldTransform = GetMesh()->GetComponentTransform();
	SetActorLocationAndRotation(Location, Rotation);
	const FTransform AfterMeshWorldTransform = GetMesh()->GetComponentTransform();
	const FTransform  PelvisOffset = PreMeshWorldTransform.GetRelativeTransform(AfterMeshWorldTransform);
	GetAnimInstance()->OnSmoothTeleport(PelvisOffset, SmoothTime);
	GetMesh()->TickAnimation(0, false);
	GetMesh()->RefreshBoneTransforms();
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
	InputVector = FVector(Input2D.X, Input2D.Y, 0); 
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
		if (auto AnimInst = GetAnimInstance())
		{
			FAnimMontageInstance* MontageInstance = AnimInst->GetRootMotionMontageInstance();
			if (bMoveAble && MontageInstance && !MontageInstance->IsRootMotionDisabled())
			{
				MontageInstance->PushDisableRootMotion();
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

float ABladeCharacter::PlayAction(UAnimSequenceBase* AnimSeq)
{
	if (auto AnimInst = GetAnimInstance())
	{
		UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimSeq);
		if (AnimMontage)
			AnimInst->Montage_Play(AnimMontage);
		else
			AnimMontage = AnimInst->PlaySlotAnimationAsDynamicMontage(AnimSeq, DefaultSlot);
		
		if (auto MontageInst = AnimInst->GetActiveInstanceForMontage(AnimMontage))
		{
			MontageInst->Advance(0.001, nullptr, false);
			AnimInst->TriggerAnimNotifies(0.001);
		}

		bMoveAble = false;
		return GetNextComboTime(AnimSeq);
	}

	return 0;
}

template<typename F>
float ABladeCharacter::PlayAction(UAnimMontage* AnimSeq, F Func)
{
	if (auto AnimInst = GetAnimInstance())
	{
		bMoveAble = false;
		UAnimMontage* AnimMontage = Cast<UAnimMontage>(AnimSeq);
		if (AnimMontage)
			AnimInst->Montage_Play(AnimMontage);
		else
			AnimMontage = AnimInst->PlaySlotAnimationAsDynamicMontage(AnimSeq, DefaultSlot);

		if (AnimMontage)
		{
			MontageEndedDelegate.BindLambda(Func);
			AnimInst->Montage_SetBlendingOutDelegate(MontageEndedDelegate, AnimMontage);
			return AnimMontage->GetPlayLength();
		}
	}

	return 0;
}

bool ABladeCharacter::IsAttacking() const
{
	return false;
}
//#pragma optimize("", off)
void ABladeCharacter::PredictAttackHit(UAnimSequenceBase* Animation, float StartTime, float EndTime, int WeaponIndex) const
{
	if (auto AnimInst = GetAnimInstance())
	{
		ABladeWeapon* Weapon = WeaponSlots[WeaponIndex].Weapon;
		if (Weapon)
		{
			const float TimeStep = 1.0 / 30;
			FName AttachBoneName = WeaponSlots[WeaponIndex].AttachSocketName;
			auto BoneIndex = GetMesh()->GetBoneIndex(AttachBoneName);
			FTransform SocketLocalTransform = FTransform::Identity;
			if (BoneIndex == -1)
			{
				if (auto AttachSocket = GetMesh()->GetSkeletalMeshAsset()->FindSocket(AttachBoneName))
				{
					AttachBoneName = AttachSocket->BoneName;
					SocketLocalTransform = AttachSocket->GetSocketLocalTransform();
				}

				BoneIndex = GetMesh()->GetBoneIndex(AttachBoneName);
			}

			FTransform RootTransform = GetMesh()->GetBoneTransform(0);
			FTransform RefTransform = GetAnimationBoneTransform(Animation, 0, StartTime);
			FTransform AnimToWorld = RefTransform.Inverse() * RootTransform;
			FTransform LastTransform = SocketLocalTransform * GetAnimationBoneTransform(Animation, BoneIndex, StartTime) * AnimToWorld;

			TArray<AActor*> IgnoreActors;
			for (float SampleTime = StartTime; SampleTime < EndTime; SampleTime += TimeStep)
			{
				FTransform CurrentTransform = SocketLocalTransform * GetAnimationBoneTransform(Animation, BoneIndex, SampleTime) * AnimToWorld;
				TArray<FHitResult> Hits;
				Weapon->SweepTraceCharacter(Hits, LastTransform, CurrentTransform);
				for (const auto& Hit : Hits)
				{
					if (!IgnoreActors.Contains(Hit.GetActor()))
					{
						IgnoreActors.Add(Hit.GetActor());
						if (ABladeCharacter* Ch = Cast<ABladeCharacter>(Hit.GetActor()))
						{
							Ch->NotifyAttack(Hit, SampleTime - StartTime);
						}
					}
				}
							
				LastTransform = CurrentTransform;
			}
		}
	}
}
//#pragma optimize("", on)

void ABladeCharacter::NotifyAttack(const FHitResult& Hit, float AfterTime)
{
	PredictedHit = Hit;
	HitAfterTime = AfterTime;
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

	if (AutoSelectAngleCurve == nullptr || AutoSelectDistCurve == nullptr)
	{
		return nullptr;
	}

	ACharacter* AutoTarget = nullptr;

	FVector SelectDirection = GetInputVector().IsZero() ? GetActorForwardVector() : GetInputVector();
	float MaxWeight = KINDA_SMALL_NUMBER;
	for (ACharacter* Pawn : TActorRange<ACharacter>(GetWorld()))
	{
		if (Pawn != this)
		{
			FVector PawnVector = Pawn->GetActorLocation() - GetActorLocation();
			float Angle = FMath::Acos(PawnVector.GetSafeNormal2D() | SelectDirection);
			Angle = Angle / PI * 180;
			const float AngleWeight = AutoSelectAngleCurve->GetFloatValue(Angle);
			const float DistWeight = AutoSelectDistCurve->GetFloatValue(PawnVector.Size());
			
			if (AngleWeight * DistWeight > MaxWeight)
			{
				MaxWeight = AngleWeight * DistWeight;
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
	if ((bMoveAble) && GetAnimInstance()->Dodges.Num() > 0)
	{
		const FVector LocalDir = GetActorTransform().InverseTransformVector(GetInputVector().GetSafeNormal2D());
		float AngleBetweenDirection = 360.0f / (float)GetAnimInstance()->Dodges.Num();
		const int DirectionIndex = FRotator::ClampAxis(LocalDir.Rotation().Yaw + AngleBetweenDirection*0.5) / AngleBetweenDirection;

		if (const auto AnimSeq = GetAnimInstance()->Dodges[DirectionIndex])
		{
			FTransform RootTrans = AnimSeq->ExtractRootMotionFromRange(0, AnimSeq->GetPlayLength());
			RootTrans = ConvertLocalRootMotionToWorld(RootTrans, GetMesh()->GetRelativeTransform());
			const auto DeltaRot = FQuat::FindBetween(RootTrans.GetLocation().GetSafeNormal2D(), LocalDir);
			SmoothTeleport(GetActorLocation(), (DeltaRot * GetActorQuat()).Rotator());
			bMoveAble = false;
			PlayAction(AnimSeq);
		}
	}
}

float ABladeCharacter::Attack(int InputIndex)
{
	if(auto AnimInst = GetAnimInstance())
	{
		if (bMoveAble && GetCharacterMovement()->IsWalking())
		{
			for (int i = 0; i < CachedCommands.Num(); i++)
			{
				if (CachedCommands[i].InputIndex == InputIndex)
				{
					auto ActionAnim = CachedCommands[i].Animation;
					CachedCommands.Empty();
					CachedInputs.Empty();
					float PlayLength = PlayAction(ActionAnim);
					return PlayLength;
				}
			}

			if(AnimInst->AttackAnimations.IsValidIndex(InputIndex) && AnimInst->AttackAnimations[InputIndex])
			{
				return PlayAction(AnimInst->AttackAnimations[InputIndex]);
			}
		}
		else
		{
			CachedInputs.Add(InputRecord{ InputIndex, GetWorld()->GetTimeSeconds() });
		}
	}

	return 0;
}

void ABladeCharacter::LeftAttack()
{
	if (auto AnimInst = GetAnimInstance())
	{
		int AttackIndex = InputVector.IsZero()? 0 : FRotator::ClampAxis(InputVector.ToOrientationRotator().Yaw + 45) / 90 + 1;
		Attack(AttackIndex);
	}
}

void ABladeCharacter::RightAttack()
{
}

void ABladeCharacter::Block(const FInputActionValue& ActionValue)
{
	bWantBlock = ActionValue.Get<bool>();
}

void ABladeCharacter::SetBlock(bool bEnable)
{
	if (bEnable)
	{
		if (bMoveAble && !GetBlock())
		{
			GetAnimInstance()->StopAllMontages(0.2);
			PlayAction(GetAnimInstance()->Block);
		}
	}
	else
	{
		GetAnimInstance()->Montage_Play(GetAnimInstance()->StopBlock);
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
	if (!bMoveAble) return;

	ExecutionTarget = nullptr;
	ExecutionIndex = -1;
	FVector SelectDirection = GetInputVector().IsZero() ? GetActorForwardVector() : GetInputVector();
	float MinDist = ExecutionDist;
	for (ABladeCharacter* Pawn : TActorRange<ABladeCharacter>(GetWorld()))
	{
		if (Pawn != this && Pawn->IsExcuteable())
		{
			FVector PawnVector = Pawn->GetActorLocation() - GetActorLocation();
			float Angle = PawnVector.GetSafeNormal2D() | GetActorForwardVector();
			float Dist = PawnVector.Size();
			if (Angle > FMath::Cos(ExecutionAngle) && Dist < ExecutionDist)
			{
				if (Dist < MinDist)
				{
					MinDist = Dist;
					ExecutionTarget = Pawn;
				}
			}
		}
	}

	if (ExecutionTarget && ExecutionTarget->GetMesh()->GetSkeletalMeshAsset())
	{
		FTransform TargetOnLocal = ExecutionTarget->GetActorTransform().GetRelativeTransform(GetActorTransform());
	
		float MaxWeight = 0;
		const auto& ExecutionAnimations = GetAnimInstance()->ExecutionAnimations;
		for (int i = 0; i < ExecutionAnimations.Num(); i++)
		{
			const auto& Val = ExecutionAnimations[i];
			if (Val.ExecutedAnimation && Val.ExecutionAnimation && Val.ExecutedAnimation->GetSkeleton() == ExecutionTarget->GetMesh()->GetSkeletalMeshAsset()->GetSkeleton())
			{
				FTransform TargetOffset = GetAnimationBoneTransform(Val.ExecutedAnimation, 0, 0);
				TargetOffset = ConvertLocalRootMotionToWorld(TargetOffset, ExecutionTarget->GetMesh()->GetRelativeTransform());
				FTransform SelfOffset = GetAnimationBoneTransform(Val.ExecutionAnimation, 0, 0);
				SelfOffset = ConvertLocalRootMotionToWorld(SelfOffset, GetMesh()->GetRelativeTransform());
				
				const FTransform TargetOnSelf = TargetOffset.GetRelativeTransform(SelfOffset);
				float Dist = (TargetOnLocal.GetLocation() - TargetOnSelf.GetLocation()).Size();
				float Weight = FMath::Max(100 - Dist, 0) * (1+(TargetOnLocal.GetUnitAxis(EAxis::X) | TargetOnSelf.GetUnitAxis(EAxis::X)));
				if (Weight > MaxWeight)
				{
					MaxWeight = Weight;
					ExecutionIndex = i;
				}	

				if (Val.bDebugDraw)
				{
					FTransform TargetOnWorld = TargetOnSelf * GetActorTransform();
					DrawDebugCapsule(GetWorld(), TargetOnWorld.GetLocation(),
						GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),
						GetCapsuleComponent()->GetScaledCapsuleRadius(),
						TargetOnWorld.GetRotation(), FColor::Red, false, 5);
					DrawDebugDirectionalArrow(GetWorld(), TargetOnWorld.GetLocation(), TargetOnWorld.TransformPosition(FVector(60, 0, 0)), 20, FColor::Green, false, 5, 0, 3);
				}
			}
		}

		if (ExecutionIndex != -1)
		{
			FVector ExecutionLoction = ExecutionTarget->GetActorLocation();
			FVector ExecutionDirection = (ExecutionTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();

			FTransform SelfTransform(ExecutionDirection.Rotation(), ExecutionLoction);
			ExecutionTransform = SelfTransform;
			FTransform TargetOffset = GetAnimationBoneTransform(ExecutionAnimations[ExecutionIndex].ExecutedAnimation, 0, 0);
			TargetOffset = ConvertLocalRootMotionToWorld(TargetOffset, ExecutionTarget->GetMesh()->GetRelativeTransform());
			FTransform TargetTransform = TargetOffset * SelfTransform;
			ExecutionTarget->SmoothTeleport(TargetTransform.GetLocation(), TargetTransform.Rotator(), 0.2);
			ExecutionTarget->GetCapsuleComponent()->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);

			FTransform SelfOffset = GetAnimationBoneTransform(ExecutionAnimations[ExecutionIndex].ExecutionAnimation, 0, 0);
			SelfOffset = ConvertLocalRootMotionToWorld(SelfOffset, GetMesh()->GetRelativeTransform());
			SelfTransform = SelfOffset * SelfTransform;
			SmoothTeleport(SelfTransform.GetLocation(), SelfTransform.Rotator(), 0.2);
			PlayAction(ExecutionAnimations[ExecutionIndex].ExecutionAnimation,
				[this](UAnimMontage* Montage, bool bInterrupted) {
					bMoveAble = true;
					ExecutionTarget = nullptr;
					ExecutionIndex = -1;
				});

			bMoveAble = false;
			ExecutionTarget->PlayAction(ExecutionAnimations[ExecutionIndex].ExecutedAnimation,
				[ch = ExecutionTarget](UAnimMontage* Montage, bool bInterrupted) {
					ch->bMoveAble = true;
				});

			ExecutionTarget->bMoveAble = false;
		}
		else
			ExecutionTarget = nullptr;
	}
}

void ABladeCharacter::TickExecution()
{
	if (auto AnimInst = GetAnimInstance())
	{
		if (ExecutionTarget && ExecutionTarget->IsRagdoll() == false && ExecutionIndex != INDEX_NONE )
		{
			const auto& ExecutionData = AnimInst->ExecutionAnimations[ExecutionIndex];
			if (auto* MontageInst = AnimInst->GetActiveMontageInstance())
			{
				float PlayPosition = MontageInst->GetPosition();
				if (PlayPosition > MontageInst->Montage->GetDefaultBlendInTime())
				{
					const FTransform OtherCompTM = ExecutionTarget->GetMesh()->GetComponentTransform();
					const FTransform SelfCompTM = GetMesh()->GetComponentTransform();
					const FTransform SelfRootTransform = GetAnimationBoneTransform(ExecutionData.ExecutionAnimation, 0, PlayPosition);
					const FTransform OtherRootTransform = GetAnimationBoneTransform(ExecutionData.ExecutedAnimation, 0, PlayPosition);
					const FTransform OtherOnSelf = ConvertLocalRootMotionToWorld(
						OtherRootTransform.GetRelativeTransform(SelfRootTransform), ExecutionTarget->GetMesh()->GetRelativeTransform());
			
					const FTransform TargetTransform = OtherOnSelf * GetActorTransform();
					const FVector TargetLocation = ExecutionTarget->GetActorLocation();
					FVector Delta = TargetTransform.GetLocation() - TargetLocation;
					if (Delta.Size() > 1)
					{
						FHitResult Hit(1.f);
						ExecutionTarget->GetCharacterMovement()->SafeMoveUpdatedComponent(Delta, TargetTransform.GetRotation(), true, Hit);
					}

					const FTransform SelfOnOther = OtherOnSelf.Inverse();
					FTransform SelfTransform = SelfOnOther * ExecutionTarget->GetActorTransform();
					FVector SelfLocation = GetActorLocation();
					Delta = SelfTransform.GetLocation() - SelfLocation;
					if (Delta.Size() > 1)
					{
						FHitResult Hit(1.f);
						GetCharacterMovement()->SafeMoveUpdatedComponent(SelfTransform.GetLocation() - SelfLocation, SelfTransform.GetRotation(), true, Hit);
					}
				}
			}
		}
	}
}
