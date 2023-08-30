// Fill out your copyright notice in the Description page of Project Settings.

#include "AnimNotifyState_EnableRootMotion.h"
#include "Animation/AnimMontage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNotifyState_EnableRootMotion)

UAnimNotifyState_EnableRootMotion::UAnimNotifyState_EnableRootMotion(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsNativeBranchingPoint = true;
}

void UAnimNotifyState_EnableRootMotion::BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyBegin(BranchingPointPayload);

	if (USkeletalMeshComponent* MeshComp = BranchingPointPayload.SkelMeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(BranchingPointPayload.MontageInstanceID))
			{
				MontageInstance->PopDisableRootMotion();
			}
		}
	}
}

void UAnimNotifyState_EnableRootMotion::BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload)
{
	Super::BranchingPointNotifyEnd(BranchingPointPayload);

	if (USkeletalMeshComponent* MeshComp = BranchingPointPayload.SkelMeshComponent)
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			if (FAnimMontageInstance* MontageInstance = AnimInstance->GetMontageInstanceForID(BranchingPointPayload.MontageInstanceID))
			{
				MontageInstance->PushDisableRootMotion();
			}
		}
	}
}

#if WITH_EDITOR
bool UAnimNotifyState_EnableRootMotion::CanBePlaced(UAnimSequenceBase* Animation) const
{
	return (Animation && Animation->IsA(UAnimMontage::StaticClass()));
}
#endif // WITH_EDITOR