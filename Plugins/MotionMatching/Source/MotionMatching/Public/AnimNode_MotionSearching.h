// Copyright Terry Meng 2022 All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimInstanceProxy.h"
#include "MotionDataBase.h"
#include "AnimNode_MotionSearching.generated.h"

class FBaseMatchingPolicy;

struct FInertializeBoneData
{
	FVector		Location = FVector::ZeroVector;
	FVector		LinearVelocity = FVector::ZeroVector;
	FQuat		Rotation = FQuat::Identity;
	FVector		AngularVelocity = FVector::ZeroVector;
};

/*Currently This is where the process gets done, Both Finding the best Candidate Motion Key and Blending the Animations together along with their Root Motion*/
USTRUCT(BlueprintInternalUseOnly)
struct MOTIONMATCHING_API FAnimNode_MotionSearching : public FAnimNode_AssetPlayerBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = MotionData, meta = (PinShownByDefault))
	UMotionDataBase* MotionDataBase = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	bool bInertialBlend = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	float PoseBlendHalfLife = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	float VelocityHalfLife = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Config)
	float BlendTime = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pose)
	float PoseWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Pose)
	float JointVelocityWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Trajectory)
	float TrajectoryWeight = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Trajectory)
	float TrajectoryFaceWeight = 2.f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebugDraw = true;

	UPROPERTY(EditAnywhere, Category = Debug, meta = (PinHiddenByDefault))
	bool bShowPoseCost = false;

	UPROPERTY(EditAnywhere, Category = Debug, meta = (PinHiddenByDefault))
	bool bShowTrajectoryCost = false;

	UPROPERTY(EditAnywhere, Category = Debug, meta = (PinHiddenByDefault))
	bool bShowFaceCost = false;
#endif

public:
	FAnimNode_MotionSearching();
	~FAnimNode_MotionSearching();

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override; 
	virtual bool HasPreUpdate() const override;
	virtual void PreUpdate(const UAnimInstance* InAnimInstance) override;
protected:
	// FAnimNode_AssetPlayerBase interface
	virtual float GetAccumulatedTime() const override;
	virtual UAnimationAsset* GetAnimAsset() const override;
	virtual float GetCurrentAssetLength() const override;
	virtual float GetCurrentAssetTime() const override;
	virtual float GetCurrentAssetTimePlayRateAdjusted() const override;
	virtual bool GetIgnoreForRelevancyTest() const override;
	virtual bool SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest) override;
	// End of FAnimNode_AssetPlayerBase interface
private:	
	void MakeGoal(const FAnimationUpdateContext& Context, float DeltaTime, TArray<FTransform>& TrajectoryPoints);
	int GetMotionKey(const TArray<FMotionJoint>& JointDatas, const TArray<FTransform>& TrajectoryPoints) const;
	void GetMotionData(const USkeletalMeshComponent* SkelComp, float DeltaTime, TArray<FMotionJoint>& JointDatas);
	void GetMotionData(const FMotionKey* MotionKey,float DeltaTime, TArray<FMotionJoint>& JointDatas);

	struct FSequencePlayerTrack
	{
		FAnimNode_SequencePlayer_Standalone		SequencePlayer;
		float									StartTime = 0;
		float									BlendWeight = 1;
		float									RootYawOffset = 0;
	};

	TArray<FSequencePlayerTrack>		SequencePlayerTracks;
	int32								MotionKeyIndex = -1;
	FVector								MotionKeyAccel = FVector::ZeroVector;

	TArray<FTransform>					LastJoints;
	TArray<TPair<float, FTransform>>	HistoryPoints;
	double								CacheTime = 0;
	double								CacheDeltaTime = 0;

	TArray<FInertializeBoneData>		OffsetData;
	FBlendedHeapCurve					BlendCurve;
	bool								bInertializeTransition = false;
	TArray<FInertializeBoneData>		SrcBoneDatas;

#if WITH_EDITORONLY_DATA
	// If true, "Relevant anim" nodes that look for the highest weighted animation in a state will ignore this node
	UPROPERTY(EditAnywhere, Category = Relevancy, meta = (FoldProperty, PinHiddenByDefault))
	bool bIgnoreForRelevancyTest = false;
	TArray<FTransform>					DebugTrajectoryPoints;
	TArray<FVector>						DebugPathPoint;
	FTransform							MatchingComponentTransform;
	FTransform							MatchingRootBoneTransform;
#endif // WITH_EDITORONLY_DATA
};
