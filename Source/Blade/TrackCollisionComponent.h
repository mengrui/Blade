// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "TrackCollisionComponent.generated.h"

/**
 * 
 */
UCLASS(ClassGroup = Utility, hidecategories = (Lighting, Mobile), config = Engine, editinlinenew, meta = (BlueprintSpawnableComponent))
class BLADE_API UTrackCollisionComponent : public UBoxComponent
{
	GENERATED_BODY()
	
public:
	UTrackCollisionComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrackCollisionComponent)
	bool bDebugDraw = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrackCollisionComponent)
	int  SectionLength = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = TrackCollisionComponent)
	TArray<TEnumAsByte<EObjectTypeQuery> >  TraceChannels;
private:
	bool bTrace = false;
	FTransform LastTransform;

public:
	UFUNCTION(BlueprintCallable, Category = "Trace")
	void StartTrace();

	UFUNCTION(BlueprintCallable, Category = "Trace")
	void EndTrace();

	UFUNCTION(BlueprintPure, Category = "Trace")
	bool IsTracing() const { return bTrace;  }
	
	//virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

	UPROPERTY(Transient)
	TArray<UPrimitiveComponent*> IgnoreComponents;
};
