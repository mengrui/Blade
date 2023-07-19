// Fill out your copyright notice in the Description page of Project Settings.
#include "NextComboState.h"
#include "BladeCharacter.h"

void UNextComboState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime, const FAnimNotifyEventReference& EventReference)
{
	if (ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		for (const auto& Cmd : ActionCommands)
		{
			Character->PushActionCommand(Cmd);
		}
	}
}

void UNextComboState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		for (const auto& Cmd : ActionCommands)
		{
			Character->RemoveActionCommand(Cmd);
		}
	}
}
