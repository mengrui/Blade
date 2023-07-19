// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimationProcessLibrary.h"

void UAnimationProcessLibrary::CopyBoneTransform(UAnimSequence* Animation, const FName& SourceBone, const FName& DestBone)
{
	if (Animation == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("CopyBone failed. Reason: Invalid Animation"));
		return;
	}

	USkeleton* Skeleton = Animation->GetSkeleton();
	if (Skeleton == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("CopyBone failed. Reason: Animation with invalid Skeleton. Animation: %s"),
			*GetNameSafe(Animation));
		return;
	}

	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const FName RootBoneName = RefSkeleton.GetBoneName(0);
	const int32 SourceBoneIndex = RefSkeleton.FindBoneIndex(SourceBone);
	const int32 DestBoneIndex = RefSkeleton.FindBoneIndex(DestBone);
	if (SourceBoneIndex == INDEX_NONE || DestBoneIndex == INDEX_NONE)
		return;

	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(SourceBoneIndex);
	RequiredBones.Add(DestBoneIndex);
	RefSkeleton.EnsureParentsExistAndSort(RequiredBones);

	FMemMark Mark(FMemStack::Get());

	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);

	const float AnimLength = Animation->GetPlayLength();
	const float SampleInterval = Animation->GetSamplingFrameRate().AsInterval();
	int32 FrameNumber = Animation->GetNumberOfSampledKeys();

	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	IAnimationDataController& Controller = Animation->GetController();
	FRawAnimSequenceTrack SourceTrack;
	FRawAnimSequenceTrack DestTrack;
	
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	for (int32 i = 0; i < FrameNumber; i++)
	{
		FTransform SourceTransform;
		FAnimExtractContext Context(i * SampleInterval, false);
		Animation->GetBonePose(AnimationPoseData, Context, false);
		FCSPose<FCompactPose> ComponentSpacePose;
		ComponentSpacePose.InitPose(Pose);

		auto DestCompactPoseIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(DestBoneIndex));
		SourceTransform = ComponentSpacePose.GetComponentSpaceTransform(BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(SourceBoneIndex)));
		ComponentSpacePose.SetComponentSpaceTransform(DestCompactPoseIndex, SourceTransform);
		auto DestTransform = ComponentSpacePose.GetLocalSpaceTransform(DestCompactPoseIndex);
		DestTrack.ScaleKeys.Add(FVector3f(DestTransform.GetScale3D()));
		DestTrack.PosKeys.Add(FVector3f(DestTransform.GetTranslation()));
		DestTrack.RotKeys.Add(FQuat4f(DestTransform.GetRotation()));
	}

	Controller.AddBoneTrack(DestBone);
	Controller.SetBoneTrackKeys(DestBone, DestTrack.PosKeys, DestTrack.RotKeys, DestTrack.ScaleKeys);
}
