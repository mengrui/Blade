// Copyright Terry Meng 2022 All Rights Reserved.

#include "AnimNode_MotionSearching.h"
#include "MotionMatching.h"
#include "Engine/Engine.h"
#include "AnimationRuntime.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionKeyUtils.h"
#include "Inertialization.h"
#include "DrawDebugHelpers.h"

struct FAnimationInitializeContext;
DECLARE_CYCLE_STAT(TEXT("Motion Matching"), STAT_MotionMatching, STATGROUP_Anim);

FAnimNode_MotionSearching::FAnimNode_MotionSearching()
{
}

FAnimNode_MotionSearching::~FAnimNode_MotionSearching()
{
}

void FAnimNode_MotionSearching::Initialize_AnyThread(const FAnimationInitializeContext & Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);
	GetEvaluateGraphExposedInputs().Execute(Context);
	SequencePlayerTracks.Empty();
	MotionKeyIndex = INDEX_NONE;
	OffsetData.Empty();
}

void FAnimNode_MotionSearching::GatherDebugData(FNodeDebugData & DebugData)
{
#if WITH_EDITORONLY_DATA
#endif
}

bool FAnimNode_MotionSearching::HasPreUpdate() const
{
#if WITH_EDITORONLY_DATA
	return true;
#else
	return false;
#endif
}

static void DrawDebugArrow(const UWorld* world, const FTransform& WorldTM, const FColor& color)
{
	TArray<int32> Indices;
	Indices.Add(0); Indices.Add(1); Indices.Add(2);

	const FMatrix Trans = WorldTM.ToMatrixNoScale();
	TArray <FVector> MeshVertices;
	MeshVertices.Add(Trans.TransformPosition(FVector(-8, -5, 0)));
	MeshVertices.Add(Trans.TransformPosition(FVector(-8, 5, 0)));
	MeshVertices.Add(Trans.TransformPosition(FVector(12, 0, 0)));

	DrawDebugMesh(world, MeshVertices, Indices, color, false);
}

void FAnimNode_MotionSearching::PreUpdate(const UAnimInstance* InAnimInstance)
{
#if WITH_EDITORONLY_DATA
	if (bDebugDraw)
	{
		if (MotionDataBase && !DebugPathPoint.IsEmpty())
		{
			const auto SkelComp = InAnimInstance->GetSkelMeshComponent();
			const FVector Offset(0, 0, 1);
			auto DrawTransform = MatchingComponentTransform;
			DrawTransform.AddToTranslation(Offset);
			FTransform ActorToComponent = FTransform(SkelComp->GetRelativeRotation()).Inverse();
			for (int i = 1; i < DebugPathPoint.Num(); i++)
			{
				DrawDebugLine(SkelComp->GetWorld(), DebugPathPoint[i - 1] + Offset, DebugPathPoint[i] + Offset, FColor::Green, false, -1, 0, 1.5);
			}
	
			for (int i = 0; i < DebugTrajectoryPoints.Num(); i++)
			{
				auto TrajTM = ActorToComponent * DebugTrajectoryPoints[i] * DrawTransform;
				DrawDebugArrow(SkelComp->GetWorld(), TrajTM, FColor::Green);
			}

			if (const FMotionKey* CurMotionKey = MotionDataBase->GetMotionKey(MotionKeyIndex))
			{
				if (MotionDataBase->TrajectoryTimes.Num() > 0)
				{
					auto InSequence = CurMotionKey->AnimSeq;
					float DT = 1.0 / 30.0;
					const float RateScale = InSequence->RateScale;
					TArray<int> MotionBoneIndices;
					for (size_t i = 0; i < MotionDataBase->MotionBones.Num(); i++)
					{
						int BoneIndex = InSequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(MotionDataBase->MotionBones[i]);
						if (BoneIndex != INDEX_NONE)
							MotionBoneIndices.Add(BoneIndex);
					}
					const bool bLoop = UMotionDataBase::IsLoopAnimation(MotionBoneIndices, InSequence);
					const FTransform EndTransform = FMotionKeyUtils::GetBoneTransform(CurMotionKey->AnimSeq, InSequence->GetPlayLength(), 0);
					const FTransform RelativeTM = FMotionKeyUtils::GetBoneTransform(CurMotionKey->AnimSeq, CurMotionKey->StartTime, 0);
					DrawTransform = MatchingRootBoneTransform;
					DrawTransform.AddToTranslation(Offset);
					FTransform WorldTM = RelativeTM.Inverse() * DrawTransform;
					FVector LastPos = (FMotionKeyUtils::GetBoneTransform(CurMotionKey->AnimSeq,
						CurMotionKey->StartTime + MotionDataBase->TrajectoryTimes[0], 0) * WorldTM).GetLocation();
					int TrajectoryIndex = 0;
					float StartTime = MotionDataBase->TrajectoryTimes.Num() > 1 ? MotionDataBase->TrajectoryTimes[0] : 0;
					for (float time = StartTime; time < MotionDataBase->TrajectoryTimes.Last() + DT; time += DT)
					{
						FTransform CurTM = FMotionKeyUtils::GetTrajectoryTransform(InSequence, CurMotionKey->StartTime + time * RateScale, RelativeTM, EndTransform, bLoop);
						auto TrajTM = CurTM * DrawTransform;
						auto CurPos = TrajTM.GetLocation();

						DrawDebugLine(SkelComp->GetWorld(), LastPos, CurPos, FColor::Red, false, -1, 0, 1.5);
						if (time >= MotionDataBase->TrajectoryTimes[TrajectoryIndex])
						{
							DrawDebugArrow(SkelComp->GetWorld(), ActorToComponent * CurMotionKey->TrajectoryPoints[TrajectoryIndex] * DrawTransform, FColor::Red);
							TrajectoryIndex++;
						}
						LastPos = CurPos;
					}
				}
			}
		}
	}
#endif
}
//static void GetMotionData(const UAnimSequence* InSequence, float AnimTime, UMotionDataBase* MotionDataBase, FMatchingContext& MatchingContext)
//{
//	if (MotionDataBase && InSequence)
//	{
//		MatchingContext.JointData.SetNum(MotionDataBase->MotionBones.Num());
//
//		for (int32 i = 0; i < MatchingContext.JointData.Num(); i++)
//		{
//			FMotionKeyUtils::GetAnimJointData(InSequence, AnimTime, MotionDataBase->MotionBones[i], MatchingContext.JointData[i]);
//		}
//	}
//}

void FAnimNode_MotionSearching::GetMotionData(const USkeletalMeshComponent* SkelComp, float DeltaTime, TArray<FMotionJoint> &JointDatas)
{
	if (MotionDataBase)
	{
		const int BoneNum = MotionDataBase->MotionBones.Num();
		if (JointDatas.Num() != BoneNum)
		{
			JointDatas.SetNumZeroed(BoneNum);
			LastJoints.SetNum(BoneNum);
			for (int32 i = 0; i < BoneNum; i++)
			{
				const auto& BoneName = MotionDataBase->MotionBones[i];
				LastJoints[i] = SkelComp->GetBoneTransform(SkelComp->GetBoneIndex(BoneName));
			}
		}

		const auto LocalTransform = SkelComp->GetComponentTransform();
		for (int32 i = 0; i < BoneNum; i++)
		{
			const auto& BoneName = MotionDataBase->MotionBones[i];
			FTransform BoneTM = SkelComp->GetBoneTransform(SkelComp->GetBoneIndex(BoneName));
			JointDatas[i].Velocity = (BoneTM.GetLocation() - LastJoints[i].GetLocation()) / DeltaTime;
			JointDatas[i].Velocity = JointDatas[i].Velocity.GetClampedToMaxSize(2000);
			JointDatas[i].Velocity = LocalTransform.InverseTransformVector(JointDatas[i].Velocity);
			JointDatas[i].AngularVelocity = CalcAngularVelocity(LastJoints[i], BoneTM, DeltaTime);
			LastJoints[i] = BoneTM;
			BoneTM = BoneTM.GetRelativeTransform(LocalTransform);
			JointDatas[i].Postion = BoneTM.GetLocation();
			JointDatas[i].Rotation = BoneTM.GetRotation();
		}
	}
}

void FAnimNode_MotionSearching::GetMotionData(const FMotionKey* MotionKey,float PlayTime, TArray<FMotionJoint>& JointDatas)
{
	if (MotionDataBase)
	{
		const int BoneNum = MotionDataBase->MotionBones.Num();
		if (JointDatas.Num() != BoneNum)
		{
			JointDatas.SetNum(BoneNum);
		}

		for (int32 i = 0; i < BoneNum; i++)
		{
			const auto& BoneName = MotionDataBase->MotionBones[i];
			FMotionKeyUtils::GetAnimJointData(MotionKey->AnimSeq, PlayTime, BoneName, JointDatas[i]);
		}
	}
}

//#pragma optimize("",off)
void FAnimNode_MotionSearching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateAssetPlayer);
	SCOPE_CYCLE_COUNTER(STAT_MotionMatching);
	GetEvaluateGraphExposedInputs().Execute(Context);
	CacheTime += Context.GetDeltaTime();
	CacheDeltaTime = Context.GetDeltaTime();
	const auto SkelComp = Context.AnimInstanceProxy->GetSkelMeshComponent();
	ACharacter* Character = Cast<ACharacter>(SkelComp->GetOwner());

	if (MotionDataBase && MotionDataBase->GetNumMotionKey() > 0 && Character)
	{
		HistoryPoints.Add(TPair<float, FTransform>(CacheTime, SkelComp->GetBoneTransform(0)));
		const FMotionKey* MotionKey = MotionDataBase->GetMotionKey(MotionKeyIndex);

		float Position = 0;
		if (MotionKey && MotionKey->AnimSeq)
		{
			Position = GetAccumulatedTime() + Context.GetDeltaTime() * MotionKey->AnimSeq->RateScale;
		}

		if (MotionKey == nullptr || Position < MotionKey->StartTime || Position >= MotionKey->EndTime)
		{
			TArray<FTransform>	TrajectoryPoints;
			MakeGoal(Context, Context.GetDeltaTime(), TrajectoryPoints);
			TArray<FMotionJoint> JointDatas;
			if (MotionKey)
				GetMotionData(MotionKey, Position, JointDatas);
			else
				GetMotionData(SkelComp, Context.GetDeltaTime(), JointDatas);

			const int Winner = GetMotionKey(JointDatas, TrajectoryPoints);

			if (const FMotionKey* NewMotionKey = MotionDataBase->GetMotionKey(Winner))
			{
				//GEngine->AddOnScreenDebugMessage(INDEX_NONE, 5, FColor::Red, FString::Printf(TEXT("%d   %s  %f"), Winner, *NewMotionKey->AnimSeq->GetName(), NewMotionKey->StartTime));
				if (const auto NewSequence = NewMotionKey->AnimSeq)
				{
					if(!(MotionKey && NewMotionKey->AnimSeq == MotionKey->AnimSeq
						&& Position > NewMotionKey->StartTime && Position < NewMotionKey->EndTime))
					{
						if (bInertialBlend)
						{
							bInertializeTransition = true;
							SequencePlayerTracks.SetNum(1);
						}
						else
						{
							SequencePlayerTracks.AddDefaulted_GetRef();
						}
						auto& NewTrack = SequencePlayerTracks.Last();
						NewTrack.SequencePlayer.SetLoopAnimation(false);
						NewTrack.SequencePlayer.SetSequence(const_cast<UAnimSequence*>(NewSequence));
						NewTrack.SequencePlayer.SetAccumulatedTime(NewMotionKey->StartTime);
						NewTrack.StartTime = NewMotionKey->StartTime;
						NewTrack.RootYawOffset = 0;/*FMath::ClampAngle(FMath::FindDeltaAngleDegrees(
							NewMotionKey->TrajectoryPoints.Last().GetLocation().GetSafeNormal2D().ToOrientationRotator().Yaw,
							TrajectoryPoints.Last().GetLocation().GetSafeNormal2D().ToOrientationRotator().Yaw), -30, 30);*/
					}

					MotionKeyIndex = Winner;
					//MotionKeyAccel = NewAccel;
#if WITH_EDITORONLY_DATA
					MatchingComponentTransform = SkelComp->GetComponentTransform();
					MatchingRootBoneTransform = SkelComp->GetBoneTransform(0);
#endif
				}
			}
		}

		if (SequencePlayerTracks.Num()>0)
		{
			if (!bInertialBlend)
			{
				SequencePlayerTracks[0].BlendWeight = 1;
				for (int i = 1; i < SequencePlayerTracks.Num(); i++)
				{
					FSequencePlayerTrack& Track = SequencePlayerTracks[i];
					Track.BlendWeight = BlendTime <= 0 ? 1 : FMath::Clamp((Track.SequencePlayer.GetAccumulatedTime() + Context.GetDeltaTime() - Track.StartTime) / BlendTime, 0, 1.f);
				}

				TArray<float> Weights;
				Weights.AddZeroed(SequencePlayerTracks.Num());
				float RestWeight = 1;
				int FirstIndex = SequencePlayerTracks.Num() - 1;
				while (true)
				{
					Weights[FirstIndex] = SequencePlayerTracks[FirstIndex].BlendWeight * RestWeight;
					RestWeight -= Weights[FirstIndex];
					if (FirstIndex == 0 || RestWeight <= 0)
					{
						break;
					}

					FirstIndex--;
				}

				if (FirstIndex > 0)
				{
					SequencePlayerTracks.RemoveAt(0, FirstIndex);
					Weights.RemoveAt(0, FirstIndex);
				}
				check(RestWeight == 0);

				//GEngine->AddOnScreenDebugMessage(INDEX_NONE, 0, FColor::Red, FString::Printf(TEXT("BlendSequenceNum: %d"), SequencePlayerTracks.Num()));

				for (int32 i = 0; i < SequencePlayerTracks.Num(); ++i)
				{
					SequencePlayerTracks[i].SequencePlayer.Update_AnyThread(Context.FractionalWeight(Weights[i]));
				}
			}
			else
			{
				SequencePlayerTracks[0].SequencePlayer.Update_AnyThread(Context.FractionalWeight(1));
			}
		}
	}
}

FORCEINLINE void ModifyRootYaw(FPoseContext& Output, float Yaw)
{
	const FCompactPoseBoneIndex CompactPoseRootIndex = Output.Pose.GetBoneContainer().MakeCompactPoseIndex(FMeshPoseBoneIndex(0));
	FTransform& RootTransform = Output.Pose[CompactPoseRootIndex];
	RootTransform.SetRotation(FRotator(0, Yaw, 0).Quaternion());
}

static void GetAnimPose(
	const FPoseContext& Output,
	const FAnimNode_SequencePlayer_Standalone& SequencePlayer, float Yaw,
	TArray<FInertializeBoneData>& BoneDatas, FBlendedHeapCurve& Curve)
{
	const float StepTime = 1.0 / 30;
	const double AnimPosition = SequencePlayer.GetAccumulatedTime();
	TArray<FTransform, FAnimStackAllocator> Transforms;
	{
		FPoseContext PoseContext(Output);
		FAnimationPoseData AnimPoseData(PoseContext);
		SequencePlayer.GetSequence()->GetAnimationPose(AnimPoseData, FAnimExtractContext(AnimPosition, Output.AnimInstanceProxy->ShouldExtractRootMotion()));
		AnimPoseData.GetPose().CopyBonesTo(Transforms);
		Curve.CopyFrom(PoseContext.Curve);
	}

	TArray<FTransform, FAnimStackAllocator> PreTransforms;
	{
		FPoseContext PoseContext(Output);
		FAnimationPoseData AnimPoseData(PoseContext);
		SequencePlayer.GetSequence()->GetAnimationPose(AnimPoseData, FAnimExtractContext(AnimPosition - StepTime, Output.AnimInstanceProxy->ShouldExtractRootMotion()));
		AnimPoseData.GetPose().CopyBonesTo(PreTransforms);
	}

	const int BoneNum = Output.Pose.GetNumBones();
	BoneDatas.SetNum(BoneNum);

	for (int i = 0; i < BoneNum; i++)
	{
		auto& BoneData = BoneDatas[i];
		BoneData.Location = Transforms[i].GetLocation();
		BoneData.Rotation = /*i == 0? FRotator(0, Yaw, 0).Quaternion() : */Transforms[i].GetRotation();
		BoneData.LinearVelocity = (Transforms[i].GetLocation() - PreTransforms[i].GetLocation()) / StepTime;
		BoneData.AngularVelocity = CalcAngularVelocity(PreTransforms[i], Transforms[i], StepTime);
	}
}

void FAnimNode_MotionSearching::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	const int BoneNum = Output.Pose.GetNumBones();
	const int32 NumPoses = SequencePlayerTracks.Num();
	if (NumPoses > 0)
	{
		if (bInertialBlend)
		{
			TArray<FInertializeBoneData> DstBoneDatas;
			FBlendedHeapCurve DstCurve;
			GetAnimPose(Output, SequencePlayerTracks[0].SequencePlayer, SequencePlayerTracks[0].RootYawOffset, DstBoneDatas, DstCurve);
		
			if (OffsetData.IsEmpty())
			{
				bInertializeTransition = false;
				OffsetData.SetNum(BoneNum);
				SrcBoneDatas = DstBoneDatas;
				TArray<FTransform, FAnimStackAllocator> BoneTransforms;
				BoneTransforms.SetNum(BoneNum);
				for (int i = 0; i < BoneNum; i++)
				{
					BoneTransforms[i].SetLocation(DstBoneDatas[i].Location);
					BoneTransforms[i].SetRotation(DstBoneDatas[i].Rotation);
				}

				Output.Pose.CopyBonesFrom(BoneTransforms);
				BlendCurve.CopyFrom(DstCurve);
				Output.Curve.CopyFrom(DstCurve);
			}
			else
			{
				if (bInertializeTransition)
				{
					bInertializeTransition = false;
					for (int i = 0; i < BoneNum; i++)
					{
						InertializeTransition(OffsetData[i].Location, OffsetData[i].LinearVelocity, 
							SrcBoneDatas[i].Location, SrcBoneDatas[i].LinearVelocity, 
							DstBoneDatas[i].Location, DstBoneDatas[i].LinearVelocity);
						InertializeTransition(OffsetData[i].Rotation, OffsetData[i].AngularVelocity,
							SrcBoneDatas[i].Rotation, SrcBoneDatas[i].AngularVelocity, 
							DstBoneDatas[i].Rotation, DstBoneDatas[i].AngularVelocity);
					}
				}

				TArray<FTransform, FAnimStackAllocator> BoneTransforms;
				BoneTransforms.SetNum(BoneNum);
				for (int i = 0; i < BoneNum; i++)
				{
					FVector BoneVelocity;
					FVector BoneTranslation;
					FQuat BoneRotation;
					FVector BoneAngularVelocity;
					InertializeUpdate(BoneTranslation, BoneVelocity,
						OffsetData[i].Location, OffsetData[i].LinearVelocity,
						DstBoneDatas[i].Location, DstBoneDatas[i].LinearVelocity,
						PoseBlendHalfLife, CacheDeltaTime);
					InertializeUpdate(BoneRotation, BoneAngularVelocity,
						OffsetData[i].Rotation, OffsetData[i].AngularVelocity,
						DstBoneDatas[i].Rotation, DstBoneDatas[i].AngularVelocity,
						PoseBlendHalfLife, CacheDeltaTime);

					BoneTransforms[i].SetLocation(BoneTranslation);
					BoneTransforms[i].SetRotation(BoneRotation);
				}

				Output.Pose.CopyBonesFrom(BoneTransforms);
				BlendCurve.LerpTo(DstCurve, CacheDeltaTime * 10);
				Output.Curve.CopyFrom(BlendCurve);
				SrcBoneDatas = DstBoneDatas;
			}
		}
		else
		{
			FAnimationPoseData OutAnimationPoseData(Output);
			SequencePlayerTracks[0].SequencePlayer.Evaluate_AnyThread(Output);
			ModifyRootYaw(Output, SequencePlayerTracks[0].RootYawOffset);
			for (int32 i = 1; i < NumPoses; ++i)
			{
				if (SequencePlayerTracks[i].BlendWeight > 0)
				{
					FPoseContext EvaluateContext(Output);
					SequencePlayerTracks[i].SequencePlayer.Evaluate_AnyThread(EvaluateContext);
					ModifyRootYaw(EvaluateContext, SequencePlayerTracks[i].RootYawOffset);

					FPoseContext LastContext(Output);
					LastContext = Output;
					FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(EvaluateContext), FAnimationPoseData(LastContext),
						SequencePlayerTracks[i].BlendWeight, OutAnimationPoseData);
				}
			}
		}
	}
	else
	{
		Output.ResetToRefPose();
	}
}

void FAnimNode_MotionSearching::MakeGoal(const FAnimationUpdateContext& Context, float DeltaTime, TArray<FTransform>& TrajectoryPoints)
{
	const auto SkelComp = Context.AnimInstanceProxy->GetSkelMeshComponent();
	const ACharacter* Character = Cast<ACharacter>(SkelComp->GetOwner());
	const FTransform& RefTransform = SkelComp->GetComponentTransform();
	int FirstHistory = HistoryPoints.Num();
	for (int j = 0, TrajectoryIndex = 0; j < HistoryPoints.Num() && TrajectoryIndex < MotionDataBase->TrajectoryTimes.Num(); j++)
	{
		const float HistoryTime = CacheTime + MotionDataBase->TrajectoryTimes[TrajectoryIndex];
		if (HistoryPoints[j].Key >= HistoryTime)
		{
			TrajectoryPoints.Add(HistoryPoints[j].Value.GetRelativeTransform(RefTransform));
			if (TrajectoryIndex == 0)
			{
				FirstHistory = j;
			}
			TrajectoryIndex++;
		}
	}

	if (FirstHistory > 0)
		HistoryPoints.RemoveAt(0, FirstHistory);

#if WITH_EDITORONLY_DATA
	DebugPathPoint.Empty();
	if (bDebugDraw)
	{
		for (auto& it : HistoryPoints)
		{
			DebugPathPoint.Add(it.Value.GetLocation());
		}
	}
#endif

	bool bOrientRotationToMovement = true;
	const UCharacterMovementComponent* MoveComponent = Character->GetCharacterMovement();
	const FVector DesireDir = MoveComponent->GetCurrentAcceleration().GetSafeNormal2D();

	const float MaxSpeed = MoveComponent->GetMaxSpeed();
	bOrientRotationToMovement = MoveComponent->bOrientRotationToMovement;

	auto CurRot = Character->GetActorQuat();
	const FVector DesireVelocity = DesireDir * MaxSpeed;
	const auto DesireRotation = bOrientRotationToMovement ?
		(DesireVelocity.IsNearlyZero() ? CurRot : DesireVelocity.ToOrientationQuat())
		: FRotator(0, Character->GetControlRotation().Yaw, 0).Quaternion();

	FVector Velocity = Character->GetVelocity();
	FVector Position = Character->GetActorLocation();
	FVector Acceleration = MoveComponent->GetCurrentAcceleration();

	/*const bool bZeroAcceleration = Acceleration.IsZero();
	const float Friction = Acceleration.IsZero() && MoveComponent->bUseSeparateBrakingFriction ? MoveComponent->BrakingFriction : MoveComponent->GroundFriction;
	const float BrakingDeceleration = MoveComponent->GetMaxBrakingDeceleration();*/

	float LastTrajectoryTime = 0;
	for (int i = 0; i < MotionDataBase->TrajectoryTimes.Num(); i++)
	{
		const float TrajectoryTime = MotionDataBase->TrajectoryTimes[i];
		if (TrajectoryTime > 0)
		{
			const int  IterCount = (TrajectoryTime - LastTrajectoryTime) * 60;
			const float IterTime = (TrajectoryTime - LastTrajectoryTime) / IterCount;
			LastTrajectoryTime = TrajectoryTime;
			FTransform CurrentTM;
			for (float j = 0; j < IterCount; j++)
			{
				SimulationPositionsUpdate(Position, Velocity, Acceleration, DesireVelocity, VelocityHalfLife, IterTime);
				CurRot = FMath::Lerp(CurRot, DesireRotation, IterTime * 3);
				CurrentTM.SetLocation(Position);
				CurrentTM.SetRotation(CurRot);
				CurrentTM = SkelComp->GetRelativeTransform() * CurrentTM;
#if WITH_EDITORONLY_DATA
				DebugPathPoint.Add(CurrentTM.GetLocation());
#endif
			}

			CurrentTM = CurrentTM.GetRelativeTransform(RefTransform);
			TrajectoryPoints.Add(CurrentTM);
		}
	}

	if(TrajectoryPoints.Num() < MotionDataBase->TrajectoryTimes.Num())
	{
		TrajectoryPoints.Insert(TrajectoryPoints[0], MotionDataBase->TrajectoryTimes.Num() - TrajectoryPoints.Num());
	}

	check(TrajectoryPoints.Num() == MotionDataBase->TrajectoryTimes.Num());
#if WITH_EDITORONLY_DATA
	if (bDebugDraw)
	{
		DebugTrajectoryPoints = TrajectoryPoints;
	}
#endif
}

int FAnimNode_MotionSearching::GetMotionKey(const TArray<FMotionJoint>& JointDatas, const TArray<FTransform>& TrajectoryPoints) const
{
	int WinnerIndex = INDEX_NONE;

	if (MotionDataBase && TrajectoryPoints.Num() > 0 && MotionDataBase->GetNumMotionKey() > 0)
	{
#if WITH_EDITORONLY_DATA
		float MinPoseCost = 0;
		float MinTrajectoryCost = 0;
		float MinFaceCost = 0;
#endif
		float LowestCost = FLT_MAX;
		const FMotionKey* LastMotionKey = MotionDataBase->GetMotionKey(MotionKeyIndex);
		for (int i = 0; i < MotionDataBase->GetNumMotionKey(); i++)
		{
			const FMotionKey& MotionKey = MotionDataBase->GetMotionKeyChecked(i);
			if (LastMotionKey && LastMotionKey->AnimSeq == MotionKey.AnimSeq
				&& MotionKey.StartTime < LastMotionKey->EndTime 
				&& MotionKey.StartTime > LastMotionKey->StartTime - 0.5)
			{
				continue;
			}

			float CurrentCost = 0;

			float TrajectoryCost = 0;
			for (int j = 0; j < TrajectoryPoints.Num(); j++)
			{
				TrajectoryCost += FVector::Dist(MotionKey.TrajectoryPoints[j].GetLocation(), TrajectoryPoints[j].GetLocation());
			}

			CurrentCost += TrajectoryCost * TrajectoryWeight;
			if (CurrentCost > LowestCost)
			{
				continue;
			}

			float PoseCost = 0.0f;
			const float StepTime = MotionDataBase->TimeStep;
			for (int j = 0; j < JointDatas.Num() && CurrentCost < LowestCost; j++)
			{
				float JointCost = 0;
				const auto& A = JointDatas[j];
				const auto& B = MotionKey.MotionJointData[j];
				JointCost += FVector::Dist(A.Postion, B.Postion);
				JointCost += FVector::Dist(A.Postion + A.Velocity * StepTime, B.Postion + B.Velocity * StepTime) * JointVelocityWeight;
				PoseCost += JointCost;
				
				CurrentCost += JointCost * PoseWeight;
			}

			if (CurrentCost >= LowestCost)
			{
				continue;
			}

			float FaceDirCost = 0;
			if (TrajectoryFaceWeight > 0)
			{
				for (int j = 0; j < TrajectoryPoints.Num(); j++)
				{
					FaceDirCost += FMath::Abs(FMath::FindDeltaAngleDegrees(TrajectoryPoints[j].Rotator().Yaw, MotionKey.TrajectoryPoints[j].Rotator().Yaw));
				}
				CurrentCost += FaceDirCost * TrajectoryFaceWeight;
			}

			if (CurrentCost < LowestCost)
			{
				WinnerIndex = i;
				LowestCost = CurrentCost;
#if WITH_EDITORONLY_DATA
				MinPoseCost = PoseCost;
				MinTrajectoryCost = TrajectoryCost;
				MinFaceCost = FaceDirCost;
#endif
			}
		}

#if WITH_EDITORONLY_DATA
		if (bShowPoseCost)
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Red, FString::Printf(TEXT("PoseCost:	%.2f"), MinPoseCost));

		if (bShowTrajectoryCost)
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Red, FString::Printf(TEXT("TrajectoryCost:	%.f"), MinTrajectoryCost));

		if (bShowFaceCost)
			GEngine->AddOnScreenDebugMessage(INDEX_NONE, 2, FColor::Red, FString::Printf(TEXT("FaceCost:	%f"), MinFaceCost));
#endif
	}

	return WinnerIndex;
}
//#pragma optimize("",on)
// FAnimNode_AssetPlayerBase interface
float FAnimNode_MotionSearching::GetAccumulatedTime() const
{
	return SequencePlayerTracks.Num() > 0? SequencePlayerTracks.Last().SequencePlayer.GetAccumulatedTime() : 0;
}

UAnimationAsset* FAnimNode_MotionSearching::GetAnimAsset() const
{
	return SequencePlayerTracks.Num() > 0 ? SequencePlayerTracks.Last().SequencePlayer.GetAnimAsset() : nullptr;
}

float FAnimNode_MotionSearching::GetCurrentAssetLength() const
{
	return SequencePlayerTracks.Num() > 0 ? SequencePlayerTracks.Last().SequencePlayer.GetCurrentAssetLength() : 0;
}

float FAnimNode_MotionSearching::GetCurrentAssetTime() const
{
	return SequencePlayerTracks.Num() > 0 ? SequencePlayerTracks.Last().SequencePlayer.GetCurrentAssetLength() : 0;
}

float FAnimNode_MotionSearching::GetCurrentAssetTimePlayRateAdjusted() const
{
	return SequencePlayerTracks.Num() > 0 ? SequencePlayerTracks.Last().SequencePlayer.GetCurrentAssetTimePlayRateAdjusted() : 0;
}

bool FAnimNode_MotionSearching::GetIgnoreForRelevancyTest() const
{
	return GET_ANIM_NODE_DATA(bool, bIgnoreForRelevancyTest);
}

bool FAnimNode_MotionSearching::SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest)
{
#if WITH_EDITORONLY_DATA
	bIgnoreForRelevancyTest = bInIgnoreForRelevancyTest;
#endif

	if (bool* bIgnoreForRelevancyTestPtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(bool, bIgnoreForRelevancyTest))
	{
		*bIgnoreForRelevancyTestPtr = bInIgnoreForRelevancyTest;
		return true;
	}

	return false;
}