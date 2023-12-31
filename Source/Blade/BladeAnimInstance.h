// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMetaData.h"
#include "BladeDamageType.h"
#include "NextComboState.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "BladeAnimInstance.generated.h"

UENUM(BlueprintType)
enum EHitAnimType
{
	StandLight,
	StandHeavy,
	Blocked,
	BreakBlock,
	Knock,
	Bullet,
	Death,
	None
}; 

UCLASS(Blueprintable)
class UHitMetaData : public UAnimMetaData
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Direction;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TEnumAsByte<EHitAnimType> HitAnimType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName BoneName = TEXT("pelvis");
};

UCLASS(Blueprintable)
class UBoneTrackMetaData : public UAnimMetaData
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName BoneName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FTransform> BoneTracks;
};

USTRUCT(BlueprintType)
struct FExecutionAnimation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimMontage* ExecutionAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimMontage* ExecutedAnimation = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bDebugDraw = false;
};

extern FName DefaultSlot;

/**
 * 
 */
UCLASS()
class BLADE_API UBladeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UBladeAnimInstance();

	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	void StopMontage(float InBlendOutTime, const FName& SlotNodeName);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void BeginTurnInplace(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateTurnInplace(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Name)
	FName InplaceSlot = FName(TEXT("InplaceSlot"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Name)
	FName PelvisBone = FName(TEXT("pelvis"));

	UPROPERTY(BlueprintReadWrite)
	FVector2D AimOffset;

	UPROPERTY(BlueprintReadOnly)
	FVector Velocity;

	UPROPERTY(BlueprintReadOnly)
	FVector Acceleration;

	UPROPERTY(BlueprintReadOnly)
	float Speed = 0;

	UPROPERTY(BlueprintReadOnly)
	bool bSprint = false;

	UPROPERTY(BlueprintReadOnly)
	FVector2D Strafe = FVector2D::Zero();

	UPROPERTY(BlueprintReadOnly)
	bool bInputMove = false;

	UPROPERTY(BlueprintReadOnly)
	TEnumAsByte<EMovementMode> MovementMode;

	UPROPERTY(BlueprintReadWrite)
	bool bRagdoll = false;

	UPROPERTY(BlueprintReadWrite)
	bool bFaceUp = true;

	UPROPERTY(BlueprintReadWrite)
	FTransform PelvisTransform;

	UPROPERTY(BlueprintReadWrite)
	FTransform RootOffset;

	UPROPERTY(BlueprintReadWrite)
	bool bHasRootOffset = false;

	UPROPERTY(BlueprintReadWrite)
	float SmoothTeleportBlendTime = 0.2f;

	UPROPERTY(BlueprintReadOnly)
	bool IsMoveForward = true;

	UPROPERTY(BlueprintReadWrite)
	bool IsPlayingRootMotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UBlendSpace*	StrafeMove;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UBlendSpace*	StrafeAimOffset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UAnimSequence*	StandIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UAnimSequence*	SprintStart;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UAnimSequence*	SprintLoop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UAnimSequence*	SprintStopRU;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UAnimSequence*	SprintStopLU;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	TArray<UAnimSequence*>	StrafeStopRU;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	TArray<UAnimSequence*>	StrafeStopLU;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	TArray<UAnimSequence*>	TurnInplaces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Jump)
	UAnimSequence* Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Jump)
	UAnimSequence* Landed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Jump)
	UAnimSequence* Falling;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TMap<UInputAction*, UAnimMontage*>	ActionAnimationMap;

	/** front 0,right 1, back 2, left 3 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimMontage*>	Hits;

	/** eight Direction clockwise 0~8 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimMontage*>	Dodges;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	UAnimMontage*			Block;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	UAnimMontage*			StopBlock;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	UAnimSequence*			GetUpFront;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	UAnimSequence*			GetUpBack;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<FExecutionAnimation> ExecutionAnimations;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimMontage*>	ParryAnimations;

	UPROPERTY(BlueprintReadOnly)
	float					ActorYaw;

	UPROPERTY(BlueprintReadOnly)
	float					StopBeginYaw = 0;

protected:
	UPROPERTY(BlueprintReadWrite)
	float					TurnInplaceYawOffset = 0;

	UPROPERTY(BlueprintReadWrite)
	float					SprintYawOffset = 0;

	float					StopDistRemain = 0;
	float					LastIdleYaw = 0;
	int						DirectionIndex = 0;

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void BeginIdle(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateIdle(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void BeginStop(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateStop(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);
};
