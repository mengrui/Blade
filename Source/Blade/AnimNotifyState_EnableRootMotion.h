// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimNotifyState_EnableRootMotion.generated.h"

/**
 * 
 */
UCLASS(editinlinenew, const, hidecategories = Object, collapsecategories, meta = (DisplayName = "Enable Root Motion"))
class BLADE_API UAnimNotifyState_EnableRootMotion : public UAnimNotifyState
{
	GENERATED_UCLASS_BODY()
	
public:
	virtual void BranchingPointNotifyBegin(FBranchingPointNotifyPayload& BranchingPointPayload) override;
	virtual void BranchingPointNotifyEnd(FBranchingPointNotifyPayload& BranchingPointPayload) override;

#if WITH_EDITOR
	virtual bool CanBePlaced(UAnimSequenceBase* Animation) const override;
#endif
};
