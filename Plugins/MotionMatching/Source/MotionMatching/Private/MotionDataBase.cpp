// Copyright Terry Meng 2022 All Rights Reserved.
#include "MotionDataBase.h"
#include "MotionMatching.h"
#include "AnimationRuntime.h"
#include "MotionKeyUtils.h"
#include "Misc/MessageDialog.h"

UMotionDataBase::UMotionDataBase(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	TrajectoryTimes.Add(-0.3f);
	TrajectoryTimes.Add(1.f);
}

#if WITH_EDITOR
bool UMotionDataBase::IsLoopAnimation(const TArray<int>& MotionBoneIndices, const UAnimSequence* InSequence)
{
	const float AnimLength = InSequence->GetPlayLength();
	float Diff = 0;
	for (int i = 0; i < MotionBoneIndices.Num(); i++)
	{
		FMotionJoint StartJoint, EndJoint;
		FMotionKeyUtils::GetAnimJointData(InSequence, 0, MotionBoneIndices[i], StartJoint);
		FMotionKeyUtils::GetAnimJointData(InSequence, AnimLength, MotionBoneIndices[i], EndJoint);
		Diff += (StartJoint.Postion - EndJoint.Postion).Size();
	}
	return Diff < 5;
}

void UMotionDataBase::RebuildMotionKeysInAnim(const UAnimSequence* InSequence, const TArray<int> MotionBoneIndices)
{
	if (InSequence == nullptr) return;
	
	if (Skeleton == InSequence->GetSkeleton())
	{
		const float AnimLength = InSequence->GetPlayLength();
		const bool bLoop = IsLoopAnimation(MotionBoneIndices, InSequence);
		const int KeyIndexStart = MotionKeys.Num();

		for (float AccumulatedTime = 0; AccumulatedTime <= AnimLength - MotionKeyLength; AccumulatedTime += MotionKeyLength)
		{
			auto& MotionKey = MotionKeys.AddDefaulted_GetRef();
			ExtractMotionKey(AccumulatedTime, MotionBoneIndices, InSequence, MotionKey, -1, bLoop);
			MotionKey.EndTime = FMath::Min( MotionKey.StartTime + MotionKeyLength, AnimLength);
		}
	}
	else
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText(FText::FromString(FString::Printf(TEXT("Sequence %s Skeleton No Matching!"), *InSequence->GetFullName()))));
	}
}

void UMotionDataBase::ExtractMotionKey(float Time,const TArray<int>& MotionBoneIndices, const UAnimSequence* InSequence, FMotionKey& MotionKey, int FootStepIndex, bool bLoop) const
{
	MotionKey.AnimSeq = InSequence;
	MotionKey.StartTime = Time;
	MotionKey.FootStepIndex = FootStepIndex;

	for (int i = 0; i < MotionBoneIndices.Num(); i++)
	{
		MotionKey.MotionJointData.AddDefaulted();
		FMotionKeyUtils::GetAnimJointData(InSequence, MotionKey.StartTime, MotionBoneIndices[i], MotionKey.MotionJointData.Last());
	}

	const FTransform RefTransform = FMotionKeyUtils::GetBoneTransform(InSequence, Time, 0);
	const FTransform EndTransform = FMotionKeyUtils::GetBoneTransform(InSequence, InSequence->GetPlayLength(), 0);
	for (int i = 0; i < TrajectoryTimes.Num(); i++)
	{
		const float TrajectoryTime = MotionKey.StartTime + TrajectoryTimes[i] * InSequence->RateScale;
		const FTransform TrajectoryPointTM = FMotionKeyUtils::GetTrajectoryTransform(InSequence, TrajectoryTime, RefTransform, EndTransform, bLoop);
		MotionKey.TrajectoryPoints.Add(TrajectoryPointTM);
	}
}

void UMotionDataBase::RebuildAllAnim()
{
	MotionKeys.Empty();
	if (SourceAnimations.Num() > 0)
	{
		const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
		TArray<int> MotionBoneIndices;
		for (size_t i = 0; i < MotionBones.Num(); i++)
		{
			int BoneIndex = ReferenceSkeleton.FindBoneIndex(MotionBones[i]);
			if (BoneIndex != INDEX_NONE)
				MotionBoneIndices.Add(BoneIndex);
			else if(MotionBones[i] != NAME_None)
				FMessageDialog::Open(EAppMsgType::Ok, FText(FText::FromString(TEXT("Invalid Bone Name!"))));
		}

		for (int32 i = 0; i < SourceAnimations.Num(); i++)
		{
			RebuildMotionKeysInAnim(SourceAnimations[i],MotionBoneIndices);
		}
	}
}
#endif //WITH_EDITOR

#if WITH_EDITORONLY_DATA
void UMotionDataBase::Serialize(FStructuredArchive::FRecord Record)
{
	RebuildAllAnim();
	Super::Serialize(Record);
}
#endif