// Fill out your copyright notice in the Description page of Project Settings.


#include "AttackNotifyState.h"
#include "BladeCharacter.h"
#include "BladeWeapon.h"
#include "Kismet/KismetSystemLibrary.h"

void UAttackNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	if (const ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		if(Character->WeaponSlots.IsValidIndex(WeaponIndex))
		{
			Character->WeaponSlots[WeaponIndex].Weapon->Damage = Damage;
			Character->WeaponSlots[WeaponIndex].Weapon->SetDamageType(DamageType);
			Character->WeaponSlots[WeaponIndex].Weapon->HitCameraShake = HitCameraShake;
			Character->WeaponSlots[WeaponIndex].Weapon->HitPlayRateCurve = HitPlayRateCurve;
			/*float StartTime = EventReference.GetNotify()->GetTriggerTime();
			float EndTime = EventReference.GetNotify()->GetEndTriggerTime();
			Character->PredictAttackHit(Animation, StartTime, EndTime,	WeaponIndex);*/
		}
	}
}

void UAttackNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	if (const ABladeCharacter* Character = Cast<ABladeCharacter>(MeshComp->GetOwner()))
	{
		if (Character->WeaponSlots.IsValidIndex(WeaponIndex))
		{
			Character->WeaponSlots[WeaponIndex].Weapon->Damage = 0;
			Character->WeaponSlots[WeaponIndex].Weapon->SetDamageType(nullptr);
			Character->WeaponSlots[WeaponIndex].Weapon->HitCameraShake = nullptr;
			Character->WeaponSlots[WeaponIndex].Weapon->HitPlayRateCurve = nullptr;
		}
	}
}
