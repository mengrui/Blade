// Fill out your copyright notice in the Description page of Project Settings.


#include "RotToTargetState.h"

#include "EngineUtils.h"
#include "BladeCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Widgets/Text/ISlateEditableTextWidget.h"

void URotToTargetState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if(ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		Character->GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = true;
	}
}

void URotToTargetState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		Character->GetCharacterMovement()->bAllowPhysicsRotationDuringAnimRootMotion = false;
	}
}
