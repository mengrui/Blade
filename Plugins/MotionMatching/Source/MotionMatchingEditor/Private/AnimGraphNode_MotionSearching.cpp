// Copyright Terry Meng 2022 All Rights Reserved.

#include "AnimGraphNode_MotionSearching.h"
#include "MotionMatchingEditor.h"


#define LOCTEXT_NAMESPACE "A3Nodes"

UAnimGraphNode_MotionSearching::UAnimGraphNode_MotionSearching(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{

}

FLinearColor UAnimGraphNode_MotionSearching::GetNodeTitleColor() const
{
	return FLinearColor::Blue;
}

FText UAnimGraphNode_MotionSearching::GetTooltipText() const
{
	return LOCTEXT("NodeToolTip", "Motion Matching");
}

FText UAnimGraphNode_MotionSearching::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("Motion_Matching", "Motion Searching");
}

FText UAnimGraphNode_MotionSearching::GetMenuCategory() const
{
	return LOCTEXT("NodeCategory", "Animation");
}

#undef LOCTEXT_NAMESPACE
