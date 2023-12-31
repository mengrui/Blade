// Fill out your copyright notice in the Description page of Project Settings.


#include "BladeAnimInstance.h"

#include "MotionWarpingComponent.h"
#include "BladeCharacter.h"
#include "Animation/AnimationPoseData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "BladeUtility.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "AnimCharacterMovementLibrary.h"

FName DefaultSlot = FName(TEXT("DefaultSlot"));

UBladeAnimInstance::UBladeAnimInstance()
{
}

void UBladeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
}

void UBladeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	ABladeCharacter* Character = Cast<ABladeCharacter>(GetOwningActor());
	if (!Character) return;

	ActorYaw = Character->GetActorRotation().Yaw;
	bRagdoll = Character->bRagdoll;
	bFaceUp = GetOwningComponent()->GetBoneAxis(PelvisBone, EAxis::Y).Z > 0;
	FTransform ActorTransform = Character->GetActorTransform();
	FTransform RootTransform = GetOwningComponent()->GetBoneTransform(0);
	Velocity = Character->GetVelocity();
	Speed = Velocity.Size2D();
	const UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(Character->GetMovementComponent());
	MovementMode = MovementComponent->MovementMode;
	Acceleration = MovementComponent->GetCurrentAcceleration();
	FVector AccelDir = Acceleration.GetSafeNormal2D();
	FVector LocalAccelVector = ActorTransform.InverseTransformVector(AccelDir);
	//Strafe = MovementMode == EMovementMode::MOVE_None ? FVector2D::ZeroVector : FVector2D(LocalAccelVector);
	FVector LocalVelDir = ActorTransform.InverseTransformVector(Velocity).GetSafeNormal2D();
	DirectionIndex = FRotator::ClampAxis(LocalVelDir.ToOrientationRotator().Yaw + 45) / 90;
	LocalVelDir = LocalVelDir /FMath::Max(FMath::Abs(LocalVelDir.X), FMath::Abs(LocalVelDir.Y));
	LocalVelDir *= Speed / Character->RunSpeed;
	Strafe = FVector2D(LocalVelDir);
	IsMoveForward = (LocalAccelVector | FVector(1, 0, 0)) > 0.99f;
	IsPlayingRootMotion = GetRootMotionMontageInstance() && !GetRootMotionMontageInstance()->IsRootMotionDisabled();
	bInputMove = !Acceleration.IsZero() && !IsPlayingRootMotion;
	bSprint = Character->IsSprint()&& Speed > Character->RunSpeed + 10;
	SprintYawOffset = FMath::ClampAngle(LocalVelDir.Rotation().Yaw, -45, 45);

	FVector AimDir = FRotationMatrix::MakeFromX(RootTransform.GetUnitAxis(EAxis::Y)).InverseTransformVector(Character->GetViewRotation().Vector());
	AimOffset.X = AimDir.Rotation().Yaw;
	AimOffset.Y = AimDir.Rotation().Pitch;
}

void UBladeAnimInstance::StopMontage(float InBlendOutTime, const FName& SlotNodeName)
{
	// stop temporary montage
	// when terminate (in the Montage_Advance), we have to lose reference to the temporary montage
	if (SlotNodeName != NAME_None)
	{
		for (int32 InstanceIndex = 0; InstanceIndex < MontageInstances.Num(); InstanceIndex++)
		{
			// check if this is playing
			FAnimMontageInstance* MontageInstance = MontageInstances[InstanceIndex];
			// make sure what is active right now is transient that we created by request
			if (MontageInstance && MontageInstance->IsActive() && MontageInstance->IsPlaying())
			{
				if (UAnimMontage* CurMontage = MontageInstance->Montage)
				{
					// Check each track, in practice there should only be one on these
					for (int32 SlotTrackIndex = 0; SlotTrackIndex < CurMontage->SlotAnimTracks.Num(); SlotTrackIndex++)
					{
						const FSlotAnimationTrack* AnimTrack = &CurMontage->SlotAnimTracks[SlotTrackIndex];
						if (AnimTrack && AnimTrack->SlotName == SlotNodeName)
						{
							// Found it
							MontageInstance->Stop(FAlphaBlend(InBlendOutTime));
							break;
						}
					}
				}
			}
		}
	}
	else
	{
		// Stop all
		Montage_Stop(InBlendOutTime);
	}
}

void UBladeAnimInstance::BeginIdle(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference)
{
	LastIdleYaw = ActorYaw;
	TurnInplaceYawOffset = 0;
}

void UBladeAnimInstance::UpdateIdle(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference)
{
	float DeltaYaw = FMath::FindDeltaAngleDegrees(LastIdleYaw, ActorYaw);
	TurnInplaceYawOffset -= DeltaYaw;
	LastIdleYaw = ActorYaw;
}

void UBladeAnimInstance::BeginTurnInplace(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference)
{
	if (FAnimNode_SequencePlayer* SequencePlayer = AnimNodeReference.GetAnimNodePtr<FAnimNode_SequencePlayer>())
	{
		if (TurnInplaces.Num() == 4)
		{
			if (TurnInplaceYawOffset > 120)
			{
				SequencePlayer->SetSequence(TurnInplaces[0]);
			}
			else if (TurnInplaceYawOffset >= 0)
			{
				SequencePlayer->SetSequence(TurnInplaces[1]);
			}
			else if (TurnInplaceYawOffset < 0)
			{
				SequencePlayer->SetSequence(TurnInplaces[2]);
			}
			else if (TurnInplaceYawOffset < -120)
			{
				SequencePlayer->SetSequence(TurnInplaces[3]);
			}
		}
	}
}

void UBladeAnimInstance::UpdateTurnInplace(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference)
{
	float DeltaSeconds = UpdateContext.GetContext()->GetDeltaTime();
	if (FAnimNode_SequencePlayer* SequencePlayer = AnimNodeReference.GetAnimNodePtr<FAnimNode_SequencePlayer>())
	{
		float TurnInplaceTime = SequencePlayer->GetAccumulatedTime();

		//UKismetSystemLibrary::PrintString(GetOwningActor(), FString::Printf(TEXT("TurnInplaceTime:   %f"), TurnInplaceTime), false, true);
		if (UAnimSequence* AnimSeq = Cast<UAnimSequence>(SequencePlayer->GetSequence()))
		{
			FTransform DeltaTransform = ConvertLocalRootMotionToWorld(AnimSeq->ExtractRootMotionFromRange(TurnInplaceTime, TurnInplaceTime + DeltaSeconds), GetOwningComponent()->GetRelativeTransform());
			//TurnInplaceYawOffset = FMath::Clamp(TurnInplaceYawOffset + DeltaTransform.Rotator().Yaw, -90, 90);
			TurnInplaceYawOffset += DeltaTransform.Rotator().Yaw;
			//UKismetSystemLibrary::PrintString(GetOwningActor(), FString::Printf(TEXT("TurnInplaceYawOffset:   %f"), TurnInplaceYawOffset), false, true);
		}
	}
}

#pragma optimize("", off)
void UBladeAnimInstance::BeginStop(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference)
{
	FAnimNode_SequenceEvaluator* SequenceEvaluator = AnimNodeReference.GetAnimNodePtr<FAnimNode_SequenceEvaluator>();
	if (SequenceEvaluator)
	{
		StopBeginYaw = GetOwningActor()->GetActorRotation().Yaw;
		FMarkerSyncAnimPosition SyncWalkPos = GetSyncGroupPosition(TEXT("walk"));
		bool bLeftFootUp = SyncWalkPos.NextMarkerName == TEXT("L");
		if (bSprint)
		{
			SequenceEvaluator->SetSequence(bLeftFootUp? SprintStopLU : SprintStopRU);
		}
		else
		{
			const auto& StopAnims = bLeftFootUp ? StrafeStopLU : StrafeStopRU;
			if (StopAnims.IsValidIndex(DirectionIndex))
			{
				SequenceEvaluator->SetSequence(StopAnims[DirectionIndex]);
			}
		}

		SequenceEvaluator->SetExplicitTime(0);

		const ACharacter* Character = Cast<ACharacter>(GetOwningActor());
		const UCharacterMovementComponent* MovementComponent = Character->GetCharacterMovement();

		FVector StopLocation = UAnimCharacterMovementLibrary::PredictGroundMovementStopLocation(
			MovementComponent->Velocity,
			MovementComponent->bUseSeparateBrakingFriction,
			MovementComponent->BrakingFriction,
			MovementComponent->GroundFriction,
			MovementComponent->BrakingFrictionFactor,
			MovementComponent->BrakingDecelerationWalking);

		StopDistRemain = StopLocation.Size2D();

		//UKismetSystemLibrary::DrawDebugSphere(GetOwningComponent(), CurrentLocation + StopLocation, 5, 12, FLinearColor::Red, 5, 1);
	}
}

void UBladeAnimInstance::UpdateStop(const FAnimUpdateContext& UpdateContext, const FAnimNodeReference& AnimNodeReference)
{
	FAnimNode_SequenceEvaluator* SequenceEvaluator = AnimNodeReference.GetAnimNodePtr<FAnimNode_SequenceEvaluator>();
	;
	if (UAnimSequence* StopAnim = Cast<UAnimSequence>(SequenceEvaluator->GetSequence()))
	{
		float DeltaSeconds = UpdateContext.GetContext()->GetDeltaTime();
		float ExplicitTime = SequenceEvaluator->GetExplicitTime();
		float NewAnimTime = ExplicitTime;
		if (Speed > 0)
			StopDistRemain = FMath::Max(StopDistRemain - Speed * DeltaSeconds, 0);
		else
			StopDistRemain = 0;

		if (StopDistRemain > 0)
		{
			float PredictedStopDist = StopDistRemain;

			float LastTime = 0;
			FVector AnimStopRootPosition = StopAnim->ExtractRootMotionFromRange(0, StopAnim->GetPlayLength()).GetLocation();
			float LastDist = AnimStopRootPosition.Size();
			for (float SampleTime = 0; SampleTime <= StopAnim->GetPlayLength(); SampleTime += 1.f / 30)
			{
				FVector CurRootPos = StopAnim->ExtractRootMotionFromRange(0, SampleTime).GetLocation();
				float CurDist = FVector::Dist2D(AnimStopRootPosition, CurRootPos);

				if (CurDist <= PredictedStopDist)
				{
					if (CurDist < PredictedStopDist && LastDist > PredictedStopDist)
					{
						NewAnimTime = FMath::Lerp(SampleTime, LastTime, (PredictedStopDist - CurDist) / (LastDist - CurDist));
					}
					else
						NewAnimTime = SampleTime;

					break;
				}

				LastTime = SampleTime;
				LastDist = CurDist;
			}
		}
		else
			NewAnimTime += DeltaSeconds;

		if(ExplicitTime > 0)
			NewAnimTime = FMath::Clamp(NewAnimTime, ExplicitTime + DeltaSeconds * 0.7, ExplicitTime + DeltaSeconds * 1.3);
		SequenceEvaluator->SetExplicitTime(NewAnimTime);
		//UKismetSystemLibrary::PrintString(GetOwningActor(), FString::Printf(TEXT("StopAnimTime: %f  StopDistRemain: %f"), NewAnimTime, StopDistRemain), true, false);
	}
}
#pragma optimize("", on)