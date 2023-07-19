// Fill out your copyright notice in the Description page of Project Settings.


#include "BladeAnimInstance.h"

#include "MotionWarpingComponent.h"
#include "BladeCharacter.h"
#include "Animation/AnimationPoseData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "BladeUtility.h"

FName DefaultSlot = FName(TEXT("DefaultSlot"));

UBladeAnimInstance::UBladeAnimInstance()
{
}

void UBladeAnimInstance::OnSmoothTeleport(const FTransform& InPelvisOffset, float Time)
{
	SmoothTeleportTime = Time;
	SmoothTeleportAlpha = 1;
	PelvisOffset = InPelvisOffset;
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

	bRagdoll = Character->bRagdoll;
	bFaceUp = GetOwningComponent()->GetBoneAxis(PelvisBone, EAxis::Y).Z > 0;
	FTransform ActorTransform = Character->GetActorTransform();
	FTransform RootTransform = GetOwningComponent()->GetBoneTransform(0);
	Velocity = Character->GetVelocity();
	Speed = Velocity.Size2D();
	const UCharacterMovementComponent* MovementComponent = Cast<UCharacterMovementComponent>(Character->GetMovementComponent());
	MovementMode = MovementComponent->MovementMode;
	Strafe = MovementMode == EMovementMode::MOVE_None? FVector2D::ZeroVector : FVector2D(Character->GetTransform().InverseTransformVectorNoScale(Velocity) ) / MovementComponent->GetMaxSpeed();
	Acceleration = MovementComponent->GetCurrentAcceleration();
	bInputMove = !Acceleration.IsZero();
	FVector AccelDir = Acceleration.GetSafeNormal2D();
	FVector LocalAccelVector = ActorTransform.InverseTransformVector(AccelDir);
	IsMoveForward = (LocalAccelVector | FVector(1, 0, 0)) > 0.99f;
	IsPlayingRootMotion = GetRootMotionMontageInstance() && !GetRootMotionMontageInstance()->IsRootMotionDisabled();
	if (SmoothTeleportTime > 0)
	{
		SmoothTeleportAlpha -= DeltaSeconds / SmoothTeleportTime;
	}
	else
	{
		SmoothTeleportAlpha = 0;
	}
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