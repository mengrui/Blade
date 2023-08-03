// Fill out your copyright notice in the Description page of Project Settings.


#include "BladeWeapon.h"

#include "BladeCharacter.h"
#include "TrackCollisionComponent.h"
#include "Engine/DamageEvents.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Kismet/KismetSystemLibrary.h"

// Sets default values
ABladeWeapon::ABladeWeapon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	Mesh = CreateOptionalDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(Mesh);

	Constraint = CreateDefaultSubobject<UPhysicsConstraintComponent>(TEXT("WeaponConstraint"));
	Constraint->AttachToComponent(Mesh, FAttachmentTransformRules::KeepRelativeTransform);
}

void ABladeWeapon::SetSimulatePhysics(bool bTrue)
{
	if (ABladeCharacter* Charater = Cast<ABladeCharacter>(GetOwner()))
	{
		TArray<UMeshComponent*> MeshComponents;
		GetComponents(MeshComponents);
		for (auto MeshComp : MeshComponents)
		{
			MeshComp->SetSimulatePhysics(bTrue);
			MeshComp->SetCollisionEnabled(bTrue ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		}

		FName AttachSocketName = Charater->GetWeaponSocketName(this);
		if (bTrue)
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

			if (Charater->Health > 0)
			{
				FName ConstraintParentName = AttachSocketName;

				while (true)
				{
					if (FBodyInstance* BI = Charater->GetMesh()->GetBodyInstance(ConstraintParentName))
					{
						break;
					}
					else
					{
						ConstraintParentName = Charater->GetMesh()->GetParentBone(ConstraintParentName);
						if (ConstraintParentName == NAME_None)
						{
							break;
						}
					}
				}

				if (ConstraintParentName != NAME_None)
				{
					Constraint->SetConstrainedComponents(Charater->GetMesh(), ConstraintParentName, Mesh, NAME_None);
				}
			}
		}
		else
		{
			AttachToComponent(Charater->GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocketName);
			Constraint->TermComponentConstraint();
		}
	}
}

void ABladeWeapon::SetDamageType(TSubclassOf<UBladeDamageType> InDamageTypeClass)
{
	DamageTypeClass = InDamageTypeClass;
	TArray<UTrackCollisionComponent*> CollisionComponents;
	GetComponents(CollisionComponents);
	if (IsValid(DamageTypeClass))
	{
		for (auto CollisionComponent : CollisionComponents)
		{
			CollisionComponent->StartTrace();
		}

		OnTraceBegin();
	}
	else
	{
		for (auto CollisionComponent : CollisionComponents)
		{
			CollisionComponent->EndTrace();
		}
	}
}

bool ABladeWeapon::SweepTraceCharacter(TArray<FHitResult>& OutHits, const FTransform& BeginTransform, const FTransform& EndTransform) const
{
	TArray<UTrackCollisionComponent*> CollisionComponents;
	GetComponents(CollisionComponents);
	TArray<TEnumAsByte<EObjectTypeQuery> >  TraceChannels;
	TraceChannels.Add(UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn));
	for (auto CollisionComponent : CollisionComponents)
	{
		TArray<AActor*> IgnoreActors;
		IgnoreActors.Add(GetOwner());
		TArray<UPrimitiveComponent*> IgnoreComponents;
		const FVector ScaledExtent = CollisionComponent->GetScaledBoxExtent();
		const FVector Extent = CollisionComponent->GetUnscaledBoxExtent();
		const int NumSection = ScaledExtent.X * 2 / CollisionComponent->SectionLength;
		const FVector StepVec = FVector(Extent.X * 2, 0, 0) / static_cast<float>(NumSection);
		for (int32 i = 0; i < NumSection; i++)
		{
			const FTransform CompBeginTM = CollisionComponent->GetRelativeTransform() * BeginTransform;
			const FTransform CompEndTM = CollisionComponent->GetRelativeTransform() * EndTransform;
			const FVector LocalPos = FVector(-Extent.X, 0, 0) + StepVec * (0.5f + i);
			const FVector CurPos = CompEndTM.TransformPosition(LocalPos);
			const FVector LastPos = CompBeginTM.TransformPosition(LocalPos);
			TArray<FHitResult> Hits;
			if (UKismetSystemLibrary::BoxTraceMultiForObjects( this,
				LastPos, CurPos, FVector(ScaledExtent.X / static_cast<float>(NumSection), ScaledExtent.Y, ScaledExtent.Z),
				CompEndTM.GetRotation().Rotator(),
				TraceChannels, false, IgnoreActors,
				CollisionComponent->bDebugDraw? EDrawDebugTrace::Type::ForDuration : EDrawDebugTrace::None, Hits, true, FLinearColor::Blue))
			{
				for (size_t j = 0; j < Hits.Num(); j++)
				{
					if (!IgnoreComponents.Contains(Hits[j].GetComponent()))
					{
						if (Hits[j].GetActor()->IsA<ABladeCharacter>())
						{
							OutHits.Add(Hits[j]);
						}
						IgnoreComponents.Add(Hits[j].GetComponent());
					}
				}
			}
		}
	}

	return !OutHits.IsEmpty();
}

