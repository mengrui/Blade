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
	Float,
	Blocked,
	BreakBlock,
	Knock,
	Bullet,
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
	FName BoneName;
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

	void OnSmoothTeleport(const FTransform& PelvisOffset, float Time);

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
	float SmoothTeleportAlpha = 0;

	float SmoothTeleportTime = 0;

	UPROPERTY(BlueprintReadWrite)
	FTransform PelvisOffset;

	UPROPERTY(BlueprintReadOnly)
	bool IsMoveForward = true;

	UPROPERTY(BlueprintReadWrite)
	bool IsPlayingRootMotion = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UBlendSpace* StrafeMove;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Ground)
	UAnimSequence* StandIdle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Shoot)
	TArray<UAnimSequence*>	TurnInplaces;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Jump)
	UAnimSequence* Jump;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Jump)
	UAnimSequence* Landed;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Jump)
	UAnimSequence* Falling;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimSequenceBase*>	AttackAnimations;

	/** front 0,right 1, back 2, left 3 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimSequence*>	Hits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimSequence*>	Deads;

	/** eight Direction clockwise 0~8 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Melee)
	TArray<UAnimSequence*>	Dodges;

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

protected:
	UPROPERTY(BlueprintReadWrite, Category = Shoot)
	float					TurnInplaceYawOffset = 0;

	float					LastIdleYaw = 0;

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void BeginIdle(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);

	UFUNCTION(BlueprintCallable, meta = (BlueprintThreadSafe))
	void UpdateIdle(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference);
};
