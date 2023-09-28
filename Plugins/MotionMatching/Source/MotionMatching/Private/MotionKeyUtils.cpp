// Copyright Terry Meng 2022 All Rights Reserved.

#include "MotionKeyUtils.h"
#include "MotionDataBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine.h"
#include "AnimationRuntime.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/AnimSequence.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/CharacterMovementComponent.h"

const float IntervalTime = 1.f / 30;

FTransform FMotionKeyUtils::GetBoneTransform(const UAnimSequence* InSequence, const float AtTime, const FName& BoneName)
{
	const USkeleton* SourceSkeleton = InSequence->GetSkeleton();
	const FReferenceSkeleton& RefSkel = SourceSkeleton->GetReferenceSkeleton();
	const int BoneIndex = RefSkel.FindBoneIndex(BoneName);
	return GetBoneTransform(InSequence, AtTime, BoneIndex);
}

 void FMotionKeyUtils::GetComponentSpacePose(
	 const UAnimSequence* InSequence, 
	 const FBoneContainer& BoneContainer, 
	 float Time, FCSPose<FCompactPose>& ComponentSpacePose, 
	 UMirrorDataTable* MirrorDataTable)
{
	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	FAnimExtractContext Context((double)Time, false);
	InSequence->GetBonePose(AnimationPoseData, Context, false);

	if (MirrorDataTable)
	{
		TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;
		TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;
		MirrorDataTable->FillCompactPoseAndComponentRefRotations(
			BoneContainer,
			CompactPoseMirrorBones,
			ComponentSpaceRefRotations);

		FAnimationRuntime::MirrorPose(Pose, MirrorDataTable->MirrorAxis, CompactPoseMirrorBones, ComponentSpaceRefRotations);
	}

	ComponentSpacePose.InitPose(Pose);
}

static FTransform ExtractBoneTransform(
	const UAnimSequence* Animation, 
	const FBoneContainer& BoneContainer, 
	FCompactPoseBoneIndex CompactPoseBoneIndex, 
	float Time, 
	bool bComponentSpace = false,
	UMirrorDataTable* MirrorDataTable = nullptr)
{
	FMemMark Mark(FMemStack::Get());
	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context((double)Time);
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	Animation->GetBonePose(AnimationPoseData, Context);
	check(Pose.IsValidIndex(CompactPoseBoneIndex));
	if (MirrorDataTable)
	{
		TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex> CompactPoseMirrorBones;
		TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex> ComponentSpaceRefRotations;
		MirrorDataTable->FillCompactPoseAndComponentRefRotations(
			BoneContainer,
			CompactPoseMirrorBones,
			ComponentSpaceRefRotations);

		FAnimationRuntime::MirrorPose(Pose, MirrorDataTable->MirrorAxis, CompactPoseMirrorBones, ComponentSpaceRefRotations);
	}

	if (bComponentSpace)
	{
		FCSPose<FCompactPose> ComponentSpacePose;
		ComponentSpacePose.InitPose(Pose);
		return ComponentSpacePose.GetComponentSpaceTransform(CompactPoseBoneIndex);
	}
	else
	{
		return Pose[CompactPoseBoneIndex];
	}
}

FTransform FMotionKeyUtils::GetBoneTransform(const UAnimSequence* InSequence, const float AtTime, const int BoneIndex, bool bComponentSpace, UMirrorDataTable* MirrorDataTable)
{
	USkeleton* Skeleton = InSequence->GetSkeleton();
	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(BoneIndex);
	Skeleton->GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBones);
	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);
	
	const FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));
	return ExtractBoneTransform(InSequence, BoneContainer, CompactPoseBoneIndex, AtTime, bComponentSpace, MirrorDataTable);
}

void FMotionKeyUtils::GetAnimJointData(const UAnimSequence * InSequence, const float AtTime, const int32 BoneIndex, FMotionJoint& OutJointData, UMirrorDataTable* MirrorDataTable)
{
	USkeleton* Skeleton = InSequence->GetSkeleton();
	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(BoneIndex);
	Skeleton->GetReferenceSkeleton().EnsureParentsExistAndSort(RequiredBones);
	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);
	FMemMark Mark(FMemStack::Get());

	const FCompactPoseBoneIndex CompactPoseRootIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(0));
	const FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));

	FCSPose<FCompactPose> ComponentSpacePose;
	GetComponentSpacePose(InSequence, BoneContainer, AtTime, ComponentSpacePose, MirrorDataTable);
	FTransform RootTM = ComponentSpacePose.GetComponentSpaceTransform(CompactPoseRootIndex);
	FTransform BoneTM = ComponentSpacePose.GetComponentSpaceTransform(CompactPoseBoneIndex);
	BoneTM = BoneTM.GetRelativeTransform(RootTM);
	OutJointData.Postion = BoneTM.GetLocation();
	OutJointData.Rotation = BoneTM.GetRotation();

	GetComponentSpacePose(InSequence, BoneContainer, AtTime - IntervalTime, ComponentSpacePose, MirrorDataTable);
	FTransform PreBoneTM = ComponentSpacePose.GetComponentSpaceTransform(CompactPoseBoneIndex);
	PreBoneTM = PreBoneTM.GetRelativeTransform(RootTM);
	OutJointData.Velocity = (BoneTM.GetLocation() - PreBoneTM.GetLocation()) / IntervalTime;

	OutJointData.AngularVelocity = CalcAngularVelocity(PreBoneTM, BoneTM, IntervalTime);
}

void FMotionKeyUtils::GetAnimJointData(const UAnimSequence * InSequence, const float AtTime, const FName& BoneName, FMotionJoint& OutJointData)
{
	if (InSequence && (BoneName != NAME_None))
	{
		const USkeleton* SourceSkeleton = InSequence->GetSkeleton();
		const FReferenceSkeleton& RefSkel = SourceSkeleton->GetReferenceSkeleton();
		const int BoneIndex = RefSkel.FindBoneIndex(BoneName);
		GetAnimJointData(InSequence, AtTime, BoneIndex, OutJointData);
	}
}

FTransform FMotionKeyUtils::GetTrajectoryTransform(const UAnimSequence* InSequence, float TrajectoryTime, const FTransform& RefTransform, const FTransform& EndTransform, const bool bLoop)
{
	const float AnimLength = InSequence->GetPlayLength();
	FTransform TrajectoryPointTM;
	if (bLoop && TrajectoryTime > AnimLength)
	{
		TrajectoryTime -= AnimLength;
		TrajectoryPointTM = FMotionKeyUtils::GetBoneTransform(InSequence, TrajectoryTime, 0);
		TrajectoryPointTM *= EndTransform;
	}
	else if (bLoop && TrajectoryTime < 0)
	{
		TrajectoryTime += AnimLength;
		TrajectoryPointTM = FMotionKeyUtils::GetBoneTransform(InSequence, TrajectoryTime, 0);
		TrajectoryPointTM = TrajectoryPointTM.GetRelativeTransform(EndTransform);
	}
	else
		TrajectoryPointTM = FMotionKeyUtils::GetBoneTransform(InSequence, TrajectoryTime, 0);

	TrajectoryPointTM = TrajectoryPointTM.GetRelativeTransform(RefTransform);
	return TrajectoryPointTM;
}
#if WITH_EDITOR
void FMotionKeyUtils::DrawTrajectoryData(const UMotionDataBase* MotionDataBase,
	float PlayTime, UAnimSequence* InSequence, FTransform& WorldTM, FPrimitiveDrawInterface* PDI, const FColor& InColor, const uint8 Depth)
{
	TArray<int> MotionBoneIndices;
	for (size_t i = 0; i < MotionDataBase->MotionBones.Num(); i++)
	{
		int BoneIndex = InSequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(MotionDataBase->MotionBones[i]);
		if (BoneIndex != INDEX_NONE)
			MotionBoneIndices.Add(BoneIndex);
	}

	const FTransform RefTransform = FTransform::Identity;
	const FTransform EndTransform = FMotionKeyUtils::GetBoneTransform(InSequence, InSequence->GetPlayLength(), 0);
	const bool bLoop = UMotionDataBase::IsLoopAnimation(MotionBoneIndices, InSequence);
	if (MotionDataBase->TrajectoryTimes.Num() > 0)
	{
		float DT = 1.0 / 30.0;
		float LastTime = MotionDataBase->TrajectoryTimes.Num() > 1 ? MotionDataBase->TrajectoryTimes[0] * InSequence->RateScale : 0;
		float StartTime = PlayTime + LastTime;
		FVector LastPos = GetTrajectoryTransform(InSequence, StartTime, RefTransform, EndTransform, bLoop).GetLocation();
		const float Length = MotionDataBase->TrajectoryTimes.Last() * InSequence->RateScale - LastTime;
		int LoopNum = Length / DT;
		DT = Length / LoopNum;
		for (int i = 1; i <= LoopNum; i++)
		{
			float TrajectoryTime = StartTime + i * DT;
			FVector CurPos = GetTrajectoryTransform(InSequence, TrajectoryTime, RefTransform, EndTransform, bLoop).GetLocation();
			PDI->DrawLine(LastPos, CurPos, InColor, Depth, 1);
			LastPos = CurPos;
		}
	}
	
	FTransform Trans(FRotationMatrix::MakeFromX(FVector::ForwardVector));
	for (int i = 0; i < MotionDataBase->TrajectoryTimes.Num(); i++)
	{
		float time = MotionDataBase->TrajectoryTimes[i]*InSequence->RateScale;
		FTransform TrajectoryTM = GetTrajectoryTransform(InSequence, PlayTime + time, RefTransform, EndTransform, bLoop);
		DrawDirectionalArrow(PDI, (Trans * TrajectoryTM).ToMatrixNoScale(), InColor, 20.0f, 3, Depth, 1);
	}
}
#endif



