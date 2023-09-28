// Copyright Terry Meng 2022 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimSequence.h"
#include "MotionKey.generated.h"

/**
 * 
 */

class UAnimSequence;

FORCEINLINE FVector CalcAngularVelocity(const FTransform& PreTransform, const FTransform& CurTransform, float DeltaTime)
{
	FQuat Q0 = PreTransform.GetRotation();
	FQuat Q1 = CurTransform.GetRotation();
	Q1.EnforceShortestArcWith(Q0);

	// Given angular velocity vector w, quaternion differentiation can be represented as
	//   dq/dt = (w * q)/2
	// Solve for w
	//   w = 2 * dq/dt * q^-1
	// And let dq/dt be expressed as the finite difference
	//   dq/dt = (q(t+h) - q(t)) / h
	FQuat DQDt = (Q1 - Q0) / DeltaTime;
	FQuat QInv = Q0.Inverse();
	FQuat W = (DQDt * QInv) * 2.0f;

	return FVector(W.X, W.Y, W.Z);
}

USTRUCT(BlueprintType)
struct MOTIONMATCHING_API FMotionJoint
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Postion = FVector::ZeroVector;

	UPROPERTY()
	FVector Velocity = FVector::ZeroVector;

	UPROPERTY()
	FQuat Rotation = FQuat::Identity;

	UPROPERTY()
	FVector AngularVelocity = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct MOTIONMATCHING_API FMotionKey 
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PoseData")
	float StartTime = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PoseData")
	float EndTime = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "KeyBoneMotionData")
	TArray <FMotionJoint> MotionJointData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionTrajectoryData")
	TArray <FTransform> TrajectoryPoints;
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FootIKData")
	int FootStepIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FootIKData")
	FTransform NextFootStep = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category = "FootIKData")
	const UAnimSequence* AnimSeq = nullptr;

	FORCEINLINE bool operator==(const FMotionKey Other) const
	{
		return (AnimSeq == Other.AnimSeq) && (StartTime == Other.StartTime);
	}
};