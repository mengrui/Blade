// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "NextComboState.generated.h"

USTRUCT(BlueprintType)
struct FActionCommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	class UInputAction*	InputAction = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UAnimMontage*	Animation = nullptr;

	FActionCommand() {}

	FActionCommand(UInputAction* InAction, UAnimMontage* Anim)
		:InputAction(InAction), Animation(Anim) {}

	FORCEINLINE bool operator ==(const FActionCommand& Other) const
	{
		return InputAction == Other.InputAction && Animation == Other.Animation;
	}
};

/**
 * 
 */
UCLASS()
class BLADE_API UNextComboState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FActionCommand> ActionCommands;
};
