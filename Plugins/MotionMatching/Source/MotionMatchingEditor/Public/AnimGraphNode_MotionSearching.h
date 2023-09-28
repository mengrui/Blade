// Copyright Terry Meng 2022 All Rights Reserved.
#pragma once
#include "AnimGraphNode_Base.h"
#include "AnimNode_MotionSearching.h"
#include "AnimGraphNode_MotionSearching.generated.h"
/**
*
*/
/*class that holds Editor version of the AnimGraph Node Motion Matching, along its tittle, tooltip, Node Color, and the category of the node*/
UCLASS(MinimalAPI)
class UAnimGraphNode_MotionSearching : public UAnimGraphNode_Base
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Settings)
	FAnimNode_MotionSearching Node;

	//~ Begin UEdGraphNode Interface.
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetMenuCategory() const override;
	//~ End UEdGraphNode Interface.

	//~ Begin UAnimGraphNode_Base Interface
	//~ End UAnimGraphNode_Base Interface

	UAnimGraphNode_MotionSearching(const FObjectInitializer& ObjectInitializer);
};
