// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "BladeDamageType.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AttackNotifyState.generated.h"

/**
 * 
 */
UCLASS()
class BLADE_API UAttackNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int WeaponIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UBladeDamageType> DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UCurveFloat* HitPlayRateCurve; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UCameraShakeBase>  HitCameraShake;
};
