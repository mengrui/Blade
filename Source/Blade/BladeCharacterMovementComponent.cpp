// Fill out your copyright notice in the Description page of Project Settings.
#include "BladeCharacterMovementComponent.h"
#include "BladeCharacter.h"

FRotator UBladeCharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	if (ABladeCharacter* Character =  Cast<ABladeCharacter>(PawnOwner))
	{
		FVector TargetVector = Character->bMoveAble? Character->GetInputVector() : Character->GetActorForwardVector();

		if (TargetVector.SizeSquared() < UE_KINDA_SMALL_NUMBER)
		{
			// AI path following request can orient us in that direction (it's effectively an acceleration)
			if (bHasRequestedVelocity && RequestedVelocity.SizeSquared() > UE_KINDA_SMALL_NUMBER)
			{
				return RequestedVelocity.GetSafeNormal().Rotation();
			}

			// Don't change rotation if there is no acceleration.
			return CurrentRotation;
		}

		// Rotate toward direction of acceleration.
		return TargetVector.GetSafeNormal().Rotation();
	}

	return CurrentRotation;
}