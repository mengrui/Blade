#include "BladeUtility.h"
#include "Animation/AnimSequence.h"

FTransform ConvertLocalRootMotionToWorld(const FTransform& InTransform, const FTransform& ComponentTransform)
{
	//Calculate new actor transform after applying root motion to this component
	const FTransform ActorToComponent = ComponentTransform.Inverse();
	const FTransform NewComponentToWorld = InTransform * ComponentTransform;
	const FTransform NewActorTransform = ActorToComponent * NewComponentToWorld;

	const FVector DeltaWorldTranslation = NewActorTransform.GetTranslation();

	const FQuat NewWorldRotation = ComponentTransform.GetRotation() * InTransform.GetRotation();
	const FQuat DeltaWorldRotation = NewWorldRotation * ComponentTransform.GetRotation().Inverse();

	const FTransform DeltaWorldTransform(DeltaWorldRotation, DeltaWorldTranslation);

	return DeltaWorldTransform;
}

FTransform GetAnimationBoneTransform(UAnimSequenceBase* Animation, int BoneIndex, float Time, bool bComponentSpace)
{
	if (UAnimMontage* AnimMontage = Cast<UAnimMontage>(Animation))
	{
		return GetAnimMontageBoneTransform(AnimMontage, BoneIndex, Time, bComponentSpace);
	}

	USkeleton* Skeleton = Animation->GetSkeleton();
	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();

	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(BoneIndex);
	RefSkeleton.EnsureParentsExistAndSort(RequiredBones);

	FMemMark Mark(FMemStack::Get());
	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);

	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context(Time, false);
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	Animation->GetAnimationPose(AnimationPoseData, Context);

	FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));
	check(Pose.IsValidIndex(CompactPoseBoneIndex));

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

FTransform GetAnimMontageBoneTransform(UAnimMontage* Montage, int BoneIndex, float Time, bool bComponentSpace /*= true*/)
{
	USkeleton* Skeleton = Montage->GetSkeleton();
	const auto& RefSkeleton = Skeleton->GetReferenceSkeleton();

	TArray<FBoneIndexType> RequiredBones;
	RequiredBones.Add(BoneIndex);
	RefSkeleton.EnsureParentsExistAndSort(RequiredBones);

	FMemMark Mark(FMemStack::Get());
	FBoneContainer BoneContainer(RequiredBones, false, *Skeleton);

	FCompactPose Pose;
	Pose.SetBoneContainer(&BoneContainer);

	FBlendedCurve Curve;
	Curve.InitFrom(BoneContainer);

	FAnimExtractContext Context(Time, false);
	UE::Anim::FStackAttributeContainer Attributes;
	FAnimationPoseData AnimationPoseData(Pose, Curve, Attributes);

	const FAnimTrack* AnimTrack = Montage->GetAnimationData(Montage->SlotAnimTracks[0].SlotName);
	AnimTrack->GetAnimationPose(AnimationPoseData, Context);

	FCompactPoseBoneIndex CompactPoseBoneIndex = BoneContainer.MakeCompactPoseIndex(FMeshPoseBoneIndex(BoneIndex));
	check(Pose.IsValidIndex(CompactPoseBoneIndex));

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
