// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "BladeAnimInstance.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "BladeWeapon.generated.h"

UCLASS()
class BLADE_API ABladeWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ABladeWeapon();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USkeletalMeshComponent>		Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UPhysicsConstraintComponent*		Constraint;

	UFUNCTION(BlueprintCallable)
	virtual void SetSimulatePhysics(bool bTrue);

	UPROPERTY(BlueprintReadWrite)
	TSubclassOf<UBladeDamageType>			DamageTypeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Damage = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMelee = true;

	UFUNCTION(BlueprintImplementableEvent)
	void OnTraceBegin();

	UFUNCTION(BlueprintImplementableEvent)
	void SetupPhysics();

	UPROPERTY(BlueprintReadWrite, Transient)
	UCurveFloat* HitPlayRateCurve;

	UPROPERTY(BlueprintReadWrite, Transient)
	TSubclassOf<UCameraShakeBase> HitCameraShake;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TMap<TEnumAsByte<EPhysicalSurface>, class UFXSystemAsset*>		HitEffectMap;

protected:
	UPROPERTY(Transient)
	TArray<AActor*> IgnoreActorList;

public:	
	UFUNCTION(BlueprintCallable)
	virtual void SetDamageType(TSubclassOf<UBladeDamageType> InDamageTypeClass);

	bool SweepTraceCharacter(TArray<FHitResult>& Hits, const FTransform& BeginTransform, const FTransform& EndTransform) const;
};
