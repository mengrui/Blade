// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/DamageType.h"
#include "BladeDamageType.generated.h"

UENUM(BlueprintType)
enum EDamageType
{
	Melee_Light,
	Melee_Heavy,
	Melee_Float,
	Melee_Knock,
	Bullet_Light
};

/**
 * 
 */
UCLASS()
class BLADE_API UBladeDamageType : public UDamageType
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<EDamageType> DamageType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float ImpulsePitch;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool ImpulseAtFullBody = false;
};
