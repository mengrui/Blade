// Fill out your copyright notice in the Description page of Project Settings.


#include "AttackNotifyState.h"
#include "BladeCharacter.h"
#include "BladeWeapon.h"

void UAttackNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (const ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		if(Character->Weapons.IsValidIndex(WeaponIndex))
		{
			Character->Weapons[WeaponIndex]->Damage = Damage;
			Character->Weapons[WeaponIndex]->SetDamageType(DamageType);
			Character->Weapons[WeaponIndex]->HitCameraShake = HitCameraShake;
			Character->Weapons[WeaponIndex]->HitPlayRateCurve = HitPlayRateCurve;
			Character->PredictAttackHit(Animation, EventReference.GetNotify()->GetTriggerTime(), EventReference.GetNotify()->GetEndTriggerTime(),WeaponIndex);
		}
	}
}

void UAttackNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (const ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		if (Character->Weapons.IsValidIndex(WeaponIndex))
		{
			Character->Weapons[WeaponIndex]->Damage = 0;
			Character->Weapons[WeaponIndex]->SetDamageType(nullptr);
			Character->Weapons[WeaponIndex]->HitCameraShake = nullptr;
			Character->Weapons[WeaponIndex]->HitPlayRateCurve = nullptr;
		}
	}
}
