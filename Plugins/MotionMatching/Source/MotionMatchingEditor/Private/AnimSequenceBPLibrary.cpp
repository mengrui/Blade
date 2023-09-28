// Copyright Terry Meng 2022 All Rights Reserved.

#include "AnimSequenceBPLibrary.h"

#include "CoreMinimal.h"
#include "MotionMatchingEditor.h"
#include "AnimationBlueprintLibrary.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "BonePose.h"

UAnimSequenceBPLibrary::UAnimSequenceBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

static FVector GetFaceAxis(const FTransform& Transform, EFaceAxis DirectionAxis)
{
	switch (DirectionAxis)
	{
	case EFaceAxis::X:
		return Transform.GetUnitAxis(EAxis::X);
	case EFaceAxis::Y:
		return Transform.GetUnitAxis(EAxis::Y);
	case EFaceAxis::Z:
		return Transform.GetUnitAxis(EAxis::Z);
	case EFaceAxis::NegX:
		return -Transform.GetUnitAxis(EAxis::X);
	case EFaceAxis::NegY:
		return -Transform.GetUnitAxis(EAxis::Y);
	case EFaceAxis::NegZ:
		return -Transform.GetUnitAxis(EAxis::Z);

	default:
		return FVector(0, 0, 0);
	}

	return FVector(0, 0, 0);
}

void UAnimSequenceBPLibrary::GenerateRootMotion(UAnimSequence* Animation, const FName& PelvisBoneName, EFaceAxis DirectionAxis)
{
	if (Animation == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("RootMotionModifier failed. Reason: Invalid Animation"));
		return;
	}

	USkeleton* Skeleton = Animation->GetSkeleton();
	if (Skeleton == nullptr)
	{
		UE_LOG(LogAnimation, Error, TEXT("RootMotionModifier failed. Reason: Animation with invalid Skeleton. Animation: %s"),
			*GetNameSafe(Animation));
		return;
	}

	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const FName RootBoneName = RefSkeleton.GetBoneName(0);
	const int32 PelvisBoneIndex = RefSkeleton.FindBoneIndex(PelvisBoneName);
	if (PelvisBoneIndex == INDEX_NONE )
		return;

	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(0);
	RequiredBones.Add(PelvisBoneIndex);
	RefSkeleton.EnsureParentsExistAndSort(RequiredBones);

	FMemMark Mark(FMemStack::Get());

	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);

	const float AnimLength = Animation->GetPlayLength();
	const auto SampleInterval = Animation->GetSamplingFrameRate().AsInterval();
	int32 FrameNumber = Animation->GetNumberOfSampledKeys();

	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	IAnimationDataController& Controller = Animation->GetController();
	FRawAnimSequenceTrack RootTrack;
	FRawAnimSequenceTrack PelvisTrack;

	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);
	FQuat LastRot = FQuat::Identity;

	for (int32 i = 0; i < FrameNumber; i++)
	{
		FTransform PelvisTransform;
		FTransform RootTransform;
		FAnimExtractContext Context(i * SampleInterval, false);
		Animation->GetBonePose(AnimationPoseData, Context, false);
		FCSPose<FCompactPose> ComponentSpacePose;
		ComponentSpacePose.InitPose(Pose);

		auto CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(PelvisBoneIndex));
		PelvisTransform = ComponentSpacePose.GetComponentSpaceTransform(CompactPoseBoneIndex);
		RootTransform = ComponentSpacePose.GetComponentSpaceTransform(BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(0)));
		FVector Forward = GetFaceAxis(PelvisTransform, DirectionAxis);
		const FQuat DesireRot = Forward.GetSafeNormal2D().ToOrientationQuat();
		FQuat CurRot = FMath::Lerp(LastRot, DesireRot, 1);
		float Yaw = FMath::ClampAngle(CurRot.Rotator().Yaw, DesireRot.Rotator().Yaw - 20, DesireRot.Rotator().Yaw + 20);
		CurRot = FRotator(0, Yaw, 0).Quaternion();
		LastRot = CurRot;
		RootTransform.SetRotation(CurRot);
		RootTransform.SetTranslation(FVector(PelvisTransform.GetLocation().X, PelvisTransform.GetLocation().Y, 0));

		PelvisTransform = PelvisTransform.GetRelativeTransform(RootTransform);
		RootTrack.ScaleKeys.Add(FVector3f(RootTransform.GetScale3D()));
		RootTrack.PosKeys.Add(FVector3f(RootTransform.GetTranslation()));
		RootTrack.RotKeys.Add(FQuat4f(RootTransform.GetRotation()));

		PelvisTrack.ScaleKeys.Add(FVector3f(PelvisTransform.GetScale3D()));
		PelvisTrack.PosKeys.Add(FVector3f(PelvisTransform.GetTranslation()));
		PelvisTrack.RotKeys.Add(FQuat4f(PelvisTransform.GetRotation()));
	}

	Controller.RemoveBoneTrack(RootBoneName);
	Controller.AddBoneCurve(RootBoneName);
	Controller.SetBoneTrackKeys(RootBoneName, RootTrack.PosKeys, RootTrack.RotKeys, RootTrack.ScaleKeys);
	Controller.RemoveBoneTrack(PelvisBoneName);
	Controller.AddBoneCurve(PelvisBoneName);
	Controller.SetBoneTrackKeys(PelvisBoneName, PelvisTrack.PosKeys, PelvisTrack.RotKeys, PelvisTrack.ScaleKeys);
}