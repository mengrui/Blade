// Fill out your copyright notice in the Description page of Project Settings.

#include "TrackCollisionComponent.h"
#include "Engine.h"
#include "BladeWeapon.h"
#include "Kismet/KismetStringLibrary.h"

UTrackCollisionComponent::UTrackCollisionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	PrimaryComponentTick.bTickEvenWhenPaused = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UTrackCollisionComponent::StartTrace()
{
	bTrace = true;
	LastTransform = GetComponentTransform();
}

void UTrackCollisionComponent::EndTrace()
{
	bTrace = false;
	IgnoreComponents.Empty();
}

void UTrackCollisionComponent::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport)
{
	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);

	if (!bTrace) return;

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(GetOwner()->GetOwner());
	const FVector ScaledExtent = GetScaledBoxExtent();
	int NumSection = ScaledExtent.X * 2 / SectionLength;
	const FVector Extent = GetUnscaledBoxExtent();
	const FVector StepVec = FVector(Extent.X*2, 0, 0) / static_cast<float>(NumSection);
	for (int32 i = 0; i < NumSection; i++)
	{
		const FVector LocalPos = FVector(-Extent.X, 0, 0) + StepVec * (0.5f + i);
		const FVector CurPos = GetComponentTransform().TransformPosition(LocalPos);
		const FVector LastPos = LastTransform.TransformPosition(LocalPos);
		TArray<FHitResult> Hits;
		if (UKismetSystemLibrary::BoxTraceMultiForObjects(
			this,
			LastPos, CurPos, FVector(ScaledExtent.X / static_cast<float>(NumSection), ScaledExtent.Y, ScaledExtent.Z),
			GetComponentTransform().GetRotation().Rotator(),
			TraceChannels,
			false, IgnoreActors,
			bDebugDraw ? EDrawDebugTrace::Type::ForDuration : EDrawDebugTrace::None,
			Hits, true))
		{
			for (size_t j = 0; j < Hits.Num(); j++)
			{
				if (!IgnoreComponents.Contains(Hits[j].GetComponent()))
				{
					OnComponentHit.Broadcast(this, Hits[j].GetActor(), Hits[j].GetComponent(),(Hits[j].TraceEnd - Hits[j].TraceStart).GetSafeNormal(), Hits[j]);
					Hits[j].GetComponent()->OnComponentHit.Broadcast(Hits[j].GetComponent(), GetOwner(), this, (Hits[j].TraceEnd - Hits[j].TraceStart).GetSafeNormal(), Hits[j]);
					IgnoreComponents.Add(Hits[j].GetComponent());
				}
			}
		}
	}

	LastTransform = GetComponentTransform();
}
