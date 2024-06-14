// Copyright 2019-2023 Alderon Games Pty Ltd, All Rights Reserved.


#include "Abilities/POTGameplayAbility_Clamp.h"
#include "Player/IBaseCharacter.h"
#include "Kismet/KismetMathLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/CoreAttributeSet.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Abilities/CoreAttributeSet.h"

bool UPOTGameplayAbility_Clamp::CanCarry_Implementation(const AIBaseCharacter* PotentialTarget) const
{
	if (!ensureAlways(OwningPOTCharacter) ||
		!ensureAlways(PotentialTarget) ||
		ICarryInterface::Execute_IsCarried(PotentialTarget) ||
		PotentialTarget->IsHomecaveBuffActive())
	{
		return false;
	}

	const float CarryingCapacity = OwningPOTCharacter->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetCarryCapacityAttribute());

	return CarryingCapacity >= PotentialTarget->GetCombatWeight();
}

void UPOTGameplayAbility_Clamp::K2_PostMontageActivateAbility_Implementation()
{
	Super::K2_PostMontageActivateAbility_Implementation();

	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	UPOTAbilitySystemComponent* const POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(OwningPOTCharacter->GetAbilitySystemComponent());
	if (!ensureAlways(POTAbilitySystemComponent))
	{
		return;
	}

	POTAbilitySystemComponent->bClampJustFailed = false;

	if (AIBaseCharacter* CurrentlyCarriedCharacter = GetCarriedCharacter())
	{
		// If we are attached then end the ability early. Only detach serverside.
		if (OwningPOTCharacter->HasAuthority())
		{
			DoDetach(CurrentlyCarriedCharacter);
		}
		K2_EndAbility();
		return;
	}

	AbilityTaskWaitGameplayEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		WeaponHitGameplayTag,
		nullptr,
		false,
		true
	);

	if (!ensureAlways(AbilityTaskWaitGameplayEvent))
	{
		return;
	}

	AbilityTaskWaitGameplayEvent->EventReceived.AddDynamic(this, &UPOTGameplayAbility_Clamp::WeaponHitGameplayEvent);
	AbilityTaskWaitGameplayEvent->Activate();
}

void UPOTGameplayAbility_Clamp::DoAttach_Implementation(AIBaseCharacter* Target)
{
	if (!ensureAlways(OwningPOTCharacter) ||
		!ensureAlways(OwningPOTCharacter->HasAuthority()))
	{
		return;
	}

	if (AIBaseCharacter* CurrentlyCarriedCharacter = GetCarriedCharacter())
	{
		if (CurrentlyCarriedCharacter == Target)
		{
			// Already attached, do nothing
			return;
		}

		// Detach from old target and attach to new one.
		DoDetach(CurrentlyCarriedCharacter);
	}

	if (!OwningPOTCharacter->TryCarriableObject(Target))
	{
		return;
	}

	OwningPOTCharacter->OnOtherDetached.AddUniqueDynamic(this, &UPOTGameplayAbility_Clamp::AutoDetach);
	
	if (!ensureAlways(CarryCostEffectClass.Get()))
	{
		return;
	}

	ClampGEHandle = BP_ApplyGameplayEffectToOwner(CarryCostEffectClass, OwningPOTCharacter->GetGrowthLevel());
}

void UPOTGameplayAbility_Clamp::AutoDetach_Implementation(AIBaseCharacter* OldTarget, const EAttachType AttachType)
{
	// Extra AttachType check to fix a situation where the victim 
	// gets unclamped when a latched (from pounce) dino delatches from the clamping dino.
	if (!ensureAlways(OwningPOTCharacter) || 
		!ensureAlways(OwningPOTCharacter->HasAuthority()) ||
		AttachType != EAttachType::AT_Carry)
	{
		return;
	}

	DoDetach(OldTarget);
	K2_EndAbility();
}

void UPOTGameplayAbility_Clamp::DoDetach_Implementation(AIBaseCharacter* Target)
{
	if (!ensureAlways(OwningPOTCharacter) ||
		!ensureAlways(OwningPOTCharacter->HasAuthority()))
	{
		return;
	}

	if (ClampGEHandle.IsValid())
	{
		BP_RemoveGameplayEffectFromOwnerWithHandle(ClampGEHandle);
		ClampGEHandle.Invalidate();
	}

	OwningPOTCharacter->OnOtherDetached.RemoveDynamic(this, &UPOTGameplayAbility_Clamp::AutoDetach);

	// We CANNOT use "ICarryInterface::Execute_IsCarriedBy" here because the Target clears its carry parent when unexpectedly detaching, therefore will fail! 
	// We need to check the parent's carry state. -Poncho
	if (GetCarriedCharacter() == Target)
	{
		// If the Target is carried by the ability's Owner, then tell the ability owner to try stop carrying.
		OwningPOTCharacter->TryCarriableObject(Target, true);
	}
}

void UPOTGameplayAbility_Clamp::WeaponHitGameplayEvent_Implementation(FGameplayEventData Payload)
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	const FHitResult HitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Payload.TargetData, 0);

	AIBaseCharacter* const HitIBaseCharacter = Cast<AIBaseCharacter>(HitResult.GetActor());
	if (!HitIBaseCharacter)
	{
		return;
	}

	if (AbilityTaskWaitGameplayEvent)
	{
		AbilityTaskWaitGameplayEvent->EndTask();
		AbilityTaskWaitGameplayEvent = nullptr;
	}

	if (!CanCarry(HitIBaseCharacter))
	{
		UPOTAbilitySystemComponent* const POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(OwningPOTCharacter->GetAbilitySystemComponent());
		if (ensureAlways(POTAbilitySystemComponent))
		{
			POTAbilitySystemComponent->bClampJustFailed = true;
		}
		return;
	}

	if (!OwningPOTCharacter->HasAuthority())
	{
		// Only attach server side.
		return;
	}

	DoAttach(HitIBaseCharacter);
}

void UPOTGameplayAbility_Clamp::OnTagChanged_Implementation(const FGameplayTag Tag, int32 NewCount)
{
	Super::OnTagChanged_Implementation(Tag, NewCount);
	
	if (!ensureAlways(OwningPOTCharacter) ||
		!OwningPOTCharacter->HasAuthority() ||
		!Tag.MatchesTagExact(StaminaExhaustedGameplayTag) ||
		NewCount <= 0)
	{
		return;
	}

	K2_EndAbility();
}

void UPOTGameplayAbility_Clamp::K2_EndAbility()
{
	Super::K2_EndAbility();

	if (AbilityTaskWaitGameplayEvent)
	{
		AbilityTaskWaitGameplayEvent->EndTask();
		AbilityTaskWaitGameplayEvent = nullptr;
	}

	AIBaseCharacter* const CurrentlyCarriedCharacter = GetCarriedCharacter();
	if (CurrentlyCarriedCharacter && OwningPOTCharacter->HasAuthority())
	{
		DoDetach(CurrentlyCarriedCharacter);
	}
}

AIBaseCharacter* UPOTGameplayAbility_Clamp::GetCarriedCharacter() const
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return nullptr;
	}

	return OwningPOTCharacter->GetCarriedCharacter();
}
