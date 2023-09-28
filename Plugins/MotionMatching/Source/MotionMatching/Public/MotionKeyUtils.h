// Copyright Terry Meng 2022 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimSequence.h"
#include "Math/UnrealMathUtility.h"
#include "MotionKey.h"
/**
*
*/

class UAnimSequence;
class FPrimitiveDrawInterface;
class UMotionDataBase;
class USkeletalMeshComponent;

/**
 * 
 */
class MOTIONMATCHING_API FMotionKeyUtils
{
public:
	static FTransform GetBoneTransform(const UAnimSequence* InSequence, const float AtTime, const int BoneIndex, bool bComponentSpace = true, UMirrorDataTable* MirrorDataTable = nullptr);
	static FTransform GetTrajectoryTransform(const UAnimSequence* InSequence, float AtTime, const FTransform& RefTransform, const FTransform& EndTransform, const bool bLoop = false);
	static FTransform GetBoneTransform(const UAnimSequence* InSequence, const float AtTime, const FName& BoneName);

	static void GetAnimJointData(const UAnimSequence* InSequence, const float AtTime, const FName& BoneName, FMotionJoint& OutJointData);
	static void GetAnimJointData(const UAnimSequence* InSequence, const float AtTime, const int32 BoneIndex, FMotionJoint& OutJointData, UMirrorDataTable* MirrorDataTable = nullptr);
	
	static void GetComponentSpacePose(
		const UAnimSequence* InSequence,
		const FBoneContainer& BoneContainer,
		float Time,
		FCSPose<FCompactPose>& ComponentSpacePose,
		UMirrorDataTable* MirrorDataTable = nullptr);

#if WITH_EDITOR
	static void DrawTrajectoryData(const class UMotionDataBase* MotionDataBase,  float PlayTime,class UAnimSequence* CurrentSequence, FTransform& WorldTM, FPrimitiveDrawInterface* PDI, const FColor& InColor, const uint8 Depth);
#endif
};
