// Fill out your copyright notice in the Description page of Project Settings.


#include "AnimationProcessLibrary.h"
#include "BladeUtility.h"
#include "BladeAnimInstance.h"

#if WITH_EDITOR
#include "AnimationBlueprintLibrary.h"

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

	UE::Anim::FCurveFilterSettings CurveFilterSettings;
	FBoneContainer BoneContainer(RequiredBones, CurveFilterSettings, *Skeleton);

	const double AnimLength = Animation->GetPlayLength();
	const double SampleInterval = Animation->GetSamplingFrameRate().AsInterval();
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

	Controller.OpenBracket(FText::FromString("CopyBoneTransform"));
	{
		Controller.AddBoneCurve(DestBone);
		Controller.SetBoneTrackKeys(DestBone, DestTrack.PosKeys, DestTrack.RotKeys, DestTrack.ScaleKeys);
	}
	Controller.CloseBracket();
}

static void GenBoneSyncMarker(UAnimSequence* Animation, const int32 BoneIndex, float FootHeightThreshold, const FName& MarkerName, const FName& SyncMarkerNotifyTrackName)
{
	float SequenceLen = Animation->GetPlayLength();
	const float Step = 1.0 / 60;
	FVector LastFootLoc = GetAnimationBoneTransform(Animation, BoneIndex, 0).GetLocation();
	bool bLastOnGround = LastFootLoc.Z < FootHeightThreshold;
	for (float CurTime = Step; CurTime <= SequenceLen; CurTime += Step)
	{
		FVector FootLoc = GetAnimationBoneTransform(Animation, BoneIndex, CurTime).GetLocation();
		//float Speed = (FootLoc - LastFootLoc).Size() / Step;
		bool bOnGround = FootLoc.Z < (bLastOnGround ? 10 : FootHeightThreshold);
		if (!bLastOnGround && bOnGround)
		{
			UAnimationBlueprintLibrary::AddAnimationSyncMarker(Animation, MarkerName, CurTime, SyncMarkerNotifyTrackName);
		}
	
		//LastFootLoc = LastFootLoc;
		bLastOnGround = bOnGround;
	}
}

void UAnimationProcessLibrary::AddSyncMarker(UAnimSequence* Animation, const FName& SyncMarkerNotifyTrackName,
	const FName& LeftBoneName, const FName& RightBoneName,
	const FName& LeftMarkerName, const FName& RightMarkerName, float FootHeightThreshold)
{
	UAnimationBlueprintLibrary::RemoveAnimationNotifyTrack(Animation, SyncMarkerNotifyTrackName);
	UAnimationBlueprintLibrary::AddAnimationNotifyTrack(Animation, SyncMarkerNotifyTrackName);
	USkeleton* Skeleton = Animation->GetSkeleton();
	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();

	const int32 LeftBoneIndex = RefSkeleton.FindBoneIndex(LeftBoneName);
	const int32 RightBoneIndex = RefSkeleton.FindBoneIndex(RightBoneName);
	GenBoneSyncMarker(Animation, LeftBoneIndex, FootHeightThreshold, LeftMarkerName, SyncMarkerNotifyTrackName);
	GenBoneSyncMarker(Animation, RightBoneIndex, FootHeightThreshold, RightMarkerName, SyncMarkerNotifyTrackName);
}

void UAnimationProcessLibrary::GenerateBoneTrackMetaData(UAnimSequenceBase* Animation, const FName& BoneName)
{
	TArray<UAnimMetaData*> TobeRemoved;
	for (auto& MetaData : Animation->GetMetaData())
	{
		if (UBoneTrackMetaData* TrackData = Cast<UBoneTrackMetaData>(MetaData))
		{
			if (TrackData->BoneName == BoneName)
			{
				TobeRemoved.Add(TrackData);
			}
		}
	}

	Animation->RemoveMetaData(TobeRemoved);
	UBoneTrackMetaData* TrackData = NewObject<UBoneTrackMetaData>(Animation);
	TrackData->BoneName = BoneName;
	USkeleton* Skeleton = Animation->GetSkeleton();
	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();
	int BoneIndex = RefSkeleton.FindBoneIndex(BoneName);
	const float FrameTime = 1.f / 60.f;
	if (BoneIndex != INDEX_NONE)
	{
		int FrameNum = Animation->GetPlayLength() / FrameTime + 1;
		for (int i = 0; i < FrameNum; i++)
		{
			float SampleTime = i * FrameTime;
			TrackData->BoneTracks.Add(GetAnimationBoneTransform(Animation, BoneIndex, SampleTime));
		}
	}

	Animation->AddMetaData(TrackData);
}

#endif