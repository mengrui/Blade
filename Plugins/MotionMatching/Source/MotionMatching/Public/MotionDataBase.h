// Copyright Terry Meng 2022 All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "MotionKey.h"
#include "Engine/DataAsset.h"
#include "Animation/Skeleton.h"
#include "MotionDataBase.generated.h"

class USkeleton;
struct FMotionExtractionContext;

/*Data asset class that holds the List of Candidate Motion Keys to transition to*/
UCLASS(BlueprintType)
class MOTIONMATCHING_API UMotionDataBase : public UDataAsset
{
	GENERATED_BODY()

public:
	const float TimeStep = 1.0f/30.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	USkeleton* Skeleton = nullptr;

	UPROPERTY()
	TArray <FMotionKey> MotionKeys;

    UMotionDataBase(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	TArray <UAnimSequence*> SourceAnimations;

	//Motion Bones are the Bones I store the Joint Data from for the Matching process
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	TArray <FName> MotionBones;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	TArray<float> TrajectoryTimes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	float MotionKeyLength = 0.1f;

	bool IsValidMotionKeyIndex(const int Index) const
	{
		return MotionKeys.IsValidIndex(Index);
	}
	int GetNumMotionKey() const
	{
		return MotionKeys.Num();
	}

	const FMotionKey& GetMotionKeyChecked(const int FrameIndex) const
	{
		return MotionKeys[FrameIndex];
	}
	
	const FMotionKey* GetMotionKey(const int FrameIndex) const
	{
		if (MotionKeys.IsValidIndex(FrameIndex))
		{
			return &MotionKeys[FrameIndex];
		}

		return nullptr;
	}

	const UAnimSequence* GetMotionKeyAnimSequence(int AtMotionKeyIndex) const
	{
		if (MotionKeys.IsValidIndex(AtMotionKeyIndex))
		{
			return MotionKeys[AtMotionKeyIndex].AnimSeq;
		}
		
		return nullptr;
	}

	int GetSrcAnimNum() const
	{
		return SourceAnimations.Num();
	}

	bool IsValidSrcAnimIndex(const int AtIndex) const
	{
		return SourceAnimations.IsValidIndex(AtIndex);
	}

	UAnimSequence* GetSrcAnimAtIndex(const int Index) const
	{
		if (SourceAnimations.IsValidIndex(Index))
			return SourceAnimations[Index];

		return nullptr;
	}

	USkeleton* GetMotionDataBaseSkeleton() const
	{
		return Skeleton;
	}
#if WITH_EDITOR
	static bool IsLoopAnimation(const TArray<int>& MotionBoneIndices, const UAnimSequence* InSequence);
	void RebuildMotionKeysInAnim(const UAnimSequence* InSequence, const TArray<int> MotionBoneIndices);
	void ExtractMotionKey(float Time,const TArray<int>& MotionBoneIndices, const UAnimSequence* InSequence, FMotionKey& MotionKey, int FootStepIndex, bool bLoop = false) const;
	void RebuildAllAnim();
#endif //WITH_EDITOR

#if WITH_EDITORONLY_DATA
	virtual void Serialize(FStructuredArchiveRecord Record) override;
#endif
};
