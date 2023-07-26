// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Animation/AnimSequence.h"
#include "AnimationProcessLibrary.generated.h"

/**
 * 
 */
UCLASS()
class BLADE_API UAnimationProcessLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "CopyBones")
	static void CopyBoneTransform(UAnimSequence* Animation, const FName& SourceBone, const FName& DestBone);

	UFUNCTION(BlueprintCallable, Category = "CopyBones")
	static void AddSyncMarker(UAnimSequence* Animation, const FName& SyncMarkerNotifyTrackName,
		const FName& LeftBoneName, const FName& RightBoneName, const FName& LeftMarkerName, const FName& RightMarkerName, float FootHeightThreshold);
#endif
};
