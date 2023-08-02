// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PhysicsControlLimbData.h"
#include "GameFramework/Character.h"
#include "BladeAnimInstance.h"
#include "NextComboState.h"
#include "Engine/SpringInterpolator.h"
#include "BladeCharacter.generated.h"

UENUM(BlueprintType)
enum ECharacterState
{
	State_Falling,
	State_Attack,
	State_Hit,
	State_Lying,
	State_Dodge,
	State_Block,
	State_Execution,
	State_Executed,
	State_Idle
};

USTRUCT(BlueprintType)
struct FWeaponSlot 
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<ABladeWeapon>   WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName				   AttachSocketName;

	TObjectPtr<ABladeWeapon>	Weapon;
};

UCLASS(BlueprintType, config=Game)
class BLADE_API ABladeCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;

	///** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Physics, meta = (AllowPrivateAccess = "true"))
	class UPhysicsControlComponent* PhysicsControlComponent;

public:
	ABladeCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Animation, meta = (AllowPrivateAccess = "true"))
	class UMotionWarpingComponent* MotionWarping;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = UI, meta = (AllowPrivateAccess = "true"))
	class UMaterialBillboardComponent* PlayerTitleComponent;

	virtual void Destroyed() override;

	virtual void Tick(float DeltaSeconds) override;

	void ExecutedCachedInput();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	virtual void InitializePhysicsAnimation();

	UFUNCTION(BlueprintCallable)
	virtual void EnablePhysicsAnimation(bool bTrue);

	UPROPERTY(BlueprintReadWrite)
	bool bRagdoll = false;

	UFUNCTION(BlueprintCallable)
	virtual void SetRagdoll(bool bEnable);

	UFUNCTION(BlueprintPure)
	bool IsRagdoll() const;

	UFUNCTION(BlueprintImplementableEvent)
	void OnBecomeRagdoll();

	UFUNCTION(BlueprintCallable)
	FName GetWeaponSocketName(ABladeWeapon* Weapon);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Physics)
	bool bUsePhysicsAnimation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly,Category = Physics)
	float MassScale = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float LyingTime = 3;

	UFUNCTION(BlueprintCallable,BlueprintImplementableEvent)
	void OnPhysicsHit(const FVector& Impulse,const FVector& Location, const FName& BoneName = NAME_None);

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Input)
	float TurnRateGamepad;

	UPROPERTY(BlueprintReadWrite)
	bool bMoveAble = true;

	UPROPERTY(BlueprintReadWrite)
	bool bHitAble = true;

	UPROPERTY(BlueprintReadWrite)
	bool bWantBlock = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float CacheInputTime = 0.25f;

	UFUNCTION(BlueprintImplementableEvent)
	void OnDead();

	float PlayAction(UAnimSequenceBase* Montage);

	template<typename F>
	float PlayAction(UAnimMontage* AnimSeq, F Func);

	UFUNCTION(BlueprintCallable)
	bool IsAttacking() const;

	UFUNCTION(BlueprintCallable)
	void PredictAttackHit(UAnimSequenceBase* Animation, float StartTime,float EndTime,  int WeaponIndex) const;

	UFUNCTION(BlueprintCallable)
	void NotifyAttack(const FHitResult& Hit, float AfterTime);

	FHitResult PredictedHit;
	float HitAfterTime = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray <FWeaponSlot>			WeaponSlots;

	UFUNCTION(BlueprintPure)
	ABladeWeapon* GetWeapon(int Index) const;

	virtual void Jump() override;
	void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintCallable)
	class UBladeAnimInstance* GetAnimInstance() const;

	UFUNCTION(BlueprintCallable)
	void OnPointDamage( AActor* DamagedActor, float Damage, class AController* InstigatedBy, FVector HitLocation, class UPrimitiveComponent* FHitComponent, FName BoneName, FVector ShotFromDirection, const class UDamageType* DamageType, AActor* DamageCauser );

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetHealth)
	float Health = 100;

	UPROPERTY(BlueprintReadOnly)
	float MaxHealth = 100;

	UFUNCTION(BlueprintCallable)
	void SetHealth(float val);

	UFUNCTION(BlueprintImplementableEvent)
	void OnHealthChange();

	UPROPERTY(BlueprintReadWrite)
	float HealthUIDisplayTime = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UCurveFloat* AutoSelectDistCurve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UCurveFloat* AutoSelectAngleCurve = nullptr;

	UPROPERTY(BlueprintReadWrite, Transient)
	TObjectPtr<ACharacter> SelectedTarget = nullptr;

	UPROPERTY(BlueprintReadWrite, Transient)
	FVector AttackTargetLocation;

	UPROPERTY(BlueprintReadWrite, Transient)
	FVector HitNormal;

	UPROPERTY(BlueprintReadWrite, Transient)
	TArray<TObjectPtr< ACharacter>> TargetList;

	UFUNCTION(BlueprintCallable)
	bool IsExcuteable() const;

	UFUNCTION(BlueprintCallable)
	void SelectTarget();

	UFUNCTION(BlueprintCallable)
	ACharacter* GetAutoTarget() const;

	UFUNCTION(BlueprintCallable)
	FTransform FindMotionWarpingTargetTransform(const ACharacter* AutoTarget, float MaxAngle, float MaxDistance, float ReachedRadius = 0) const;

	UFUNCTION(BlueprintCallable)
	void Dodge();

	UFUNCTION(BlueprintCallable)
	float Attack(int Index);

	UFUNCTION(BlueprintCallable)
	void LeftAttack();

	UFUNCTION(BlueprintCallable)
	void RightAttack();

	UFUNCTION(BlueprintCallable)
	void Block(const FInputActionValue& ActionValue);

	UFUNCTION(BlueprintCallable)
	void SetBlock(bool bEnable);

	UFUNCTION(BlueprintCallable)
	bool GetBlock() const;

	UFUNCTION(BlueprintCallable)
	bool CanBlock(const FVector& HitFromDir) const;

	UFUNCTION(BlueprintCallable)
	void Execution();

	UPROPERTY(BlueprintReadWrite, Transient)
	TObjectPtr<ABladeCharacter> ExecutionTarget = nullptr;

	void TickExecution();

	UPROPERTY(BlueprintReadWrite, Transient)
	int ExecutionIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ExecutionDist = 200;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ExecutionAngle = 45;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BlockAngle = 90;

	UPROPERTY(EditAnywhere, Category = Effect)
	UParticleSystem* BlockEffect;

	UPROPERTY(EditAnywhere, Category = Effect)
	UParticleSystem* BloodEffect;

	UFUNCTION(BlueprintCallable)
	void SmoothTeleport(const FVector& Location, const FRotator& Rotation, float SmoothTime = 0.2f);

	UFUNCTION(BlueprintCallable)
	FVector GetInputVector() const;

	UFUNCTION(BlueprintImplementableEvent)
	FVector GetInputMove() const;

	UFUNCTION(BlueprintCallable)
	bool IsInState(ECharacterState State) const;

	UPROPERTY(EditAnywhere, Category = Movement)
	float RunSpeed = 500;

	UPROPERTY(EditAnywhere, Category = Movement)
	float WalkSpeed = 300;

	UPROPERTY(EditAnywhere, Category = Movement)
	float SprintSpeed = 650;

	UFUNCTION(BlueprintPure)
	FVector GetShootLocation();

	void CameraTrace();

	UPROPERTY(BlueprintReadOnly, Transient)
	FHitResult CameraTraceHit;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	class UInputMappingContext*				InputMappingContext = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	FPhysicsControlData						WorldSpaceControlData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	FPhysicsControlData						ParentSpaceControlData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	TArray<FPhysicsControlLimbSetupData>	PhysicsControlLimbSetupDatas;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	FPhysicsControlNames					AllWorldSpaceControls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	TMap<FName, FPhysicsControlNames>		LimbWorldSpaceControls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	FPhysicsControlNames					AllParentSpaceControls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	TMap<FName, FPhysicsControlNames>		LimbParentSpaceControls;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	FPhysicsControlNames					AllBodyModifiers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PhyscsControl)
	TMap<FName, FPhysicsControlNames>		LimbBodyModifiers;

protected:
	UFUNCTION(BlueprintCallable)
	void Move(const FInputActionValue& Value);

	UFUNCTION(BlueprintCallable)
	void LookAt(const FInputActionValue& Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	FTransform ExecutionTransform;

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

	FName AttackTargetName = FName(TEXT("AttackTarget"));

	UPROPERTY(BlueprintReadWrite, Transient)
	TArray<FActionCommand>                CachedCommands;

	struct InputRecord
	{
		int		Index = 0;
		double	Time = 0;
	};
	TArray<InputRecord>					  CachedInputs;

	FOnMontageEnded						  MontageEndedDelegate;

	void SnapCapsuleToRagdoll();

	FTimerHandle StopPhysAnimTimerHandle;

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	void PushActionCommand(const FActionCommand& Cmd);
	void RemoveActionCommand(const FActionCommand& Cmd);
	static float GetNextComboTime(const UAnimSequenceBase* Animation);

	virtual void PawnClientRestart() override;
};
