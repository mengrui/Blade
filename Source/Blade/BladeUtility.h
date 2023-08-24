#pragma once
#include "CoreMinimal.h"

extern FTransform ConvertLocalRootMotionToWorld(const FTransform& InTransform, const FTransform& ComponentTransform);
extern FTransform GetAnimationBoneTransform(class UAnimSequenceBase* Animation, int BoneIndex, float Time, bool bComponentSpace = true);
extern FTransform GetAnimMontageBoneTransform(class UAnimMontage* Montage, int BoneIndex, float Time, bool bComponentSpace = true);
extern bool SweepCheck(const FCollisionShape& A, const FTransform& TransformA,	const  FCollisionShape& B, const FTransform& TransformB, const FVector& MovedB,  FVector& HitLocation, FVector& HitNormal);