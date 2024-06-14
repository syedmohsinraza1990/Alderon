// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Abilities/POTAbilityTypes.h"

#include "Abilities/POTAbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "Abilities/GameplayAbilityTargetTypes.h"


bool FPOTGameplayEffectContainer::GetGameplayCueTags(FGameplayTagContainer& OutCueTags) const
{

	for (const FPOTGameplayEffectContainerEntry& Entry : ContainerEntries)
	{
		for (const FPOTGameplayEffectEntry& EffectEntry : Entry.TargetGameplayEffects)
		{
			for (const TSubclassOf<UGameplayEffect>& GEClass : EffectEntry.TargetGameplayEffectClasses)
			{
				if (GEClass.Get() != nullptr)
				{
					for (const FGameplayEffectCue& Cue : GEClass.GetDefaultObject()->GameplayCues)
					{
						OutCueTags.AppendTags(Cue.GameplayCueTags);
					}
				}
			}

		}
	}


	return OutCueTags.Num() > 0;
}

bool FPOTGameplayEffectContainerSpec::HasValidEffects() const
{
	return TargetGameplayEffectSpecs.Num() > 0;
}

bool FPOTGameplayEffectContainerSpec::HasValidTargets() const
{
	return TargetData.Num() > 0;
}

void FPOTGameplayEffectContainerSpec::AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors, const bool bReset /*= false*/)
{
	if (bReset)
	{
		TargetData.Data.Reset(HitResults.Num() + TargetActors.Num());
	}

	for (const FHitResult& HitResult : HitResults)
	{
		FGameplayAbilityTargetData_SingleTargetHit* NewData = new FGameplayAbilityTargetData_SingleTargetHit(HitResult);
		TargetData.Add(NewData);
	}

	if (TargetActors.Num() > 0)
	{
		FGameplayAbilityTargetData_ActorArray* NewData = new FGameplayAbilityTargetData_ActorArray();
		NewData->TargetActorArray.Append(TargetActors);
		TargetData.Add(NewData);
	}
}



bool FPOTGameplayEffectContext::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	EGEContextRepBits RepBits = EGEContextRepBits::None;
	if (Ar.IsSaving())
	{
		if (Instigator.IsValid())
		{
			RepBits |= EGEContextRepBits::Instigator;
		}
		if (EffectCauser.IsValid())
		{
			RepBits |= EGEContextRepBits::EffectCauser;
		}
		if (AbilityCDO.IsValid())
		{
			RepBits |= EGEContextRepBits::AbilityCDO;
		}
		if (bReplicateSourceObject && SourceObject.IsValid())
		{
			RepBits |= EGEContextRepBits::SourceObject;
		}
		if (Actors.Num() > 0)
		{
			RepBits |= EGEContextRepBits::Actors;
		}
		if (HitResult.IsValid())
		{
			RepBits |= EGEContextRepBits::HitResult;
		}
		if (bHasWorldOrigin)
		{
			RepBits |= EGEContextRepBits::WorldOrigin;
		}

		if (bHasDirection)
		{
			RepBits |= EGEContextRepBits::Direction;
		}

		RepBits |= EGEContextRepBits::EventMagnitude;

		if (DamageType != EDamageType::DT_GENERIC)
		{
			RepBits |= EGEContextRepBits::DamageType;
		}

		RepBits |= EGEContextRepBits::BrokeBone;
		RepBits |= EGEContextRepBits::KnockbackMode;
		RepBits |= EGEContextRepBits::KnockbackForce;

		if (ContextTag.IsValid())
		{
			RepBits |= EGEContextRepBits::ContextTag;
		}
	}
	Ar.SerializeBits(&RepBits, sizeof(EGEContextRepBits) * 8);

	if ((RepBits & EGEContextRepBits::Instigator) != EGEContextRepBits::None)
	{
		Ar << Instigator;
	}
	if ((RepBits & EGEContextRepBits::EffectCauser) != EGEContextRepBits::None)
	{
		Ar << EffectCauser;
	}
	if ((RepBits & EGEContextRepBits::AbilityCDO) != EGEContextRepBits::None)
	{
		Ar << AbilityCDO;
	}
	if ((RepBits & EGEContextRepBits::SourceObject) != EGEContextRepBits::None)
	{
		Ar << SourceObject;
	}
	if ((RepBits & EGEContextRepBits::Actors) != EGEContextRepBits::None)
	{
		SafeNetSerializeTArray_Default<31>(Ar, Actors);
	}
	if ((RepBits & EGEContextRepBits::HitResult) != EGEContextRepBits::None)
	{
		if (Ar.IsLoading())
		{
			if (!HitResult.IsValid())
			{
				HitResult = TSharedPtr<FHitResult>(new FHitResult());
			}
		}
		HitResult->NetSerialize(Ar, Map, bOutSuccess);
	}

	bHasWorldOrigin = ((RepBits & EGEContextRepBits::WorldOrigin) != EGEContextRepBits::None);
	if (bHasWorldOrigin)
	{
		Ar << WorldOrigin;
	}

	bHasDirection = ((RepBits & EGEContextRepBits::Direction) != EGEContextRepBits::None);
	if (bHasDirection)
	{
		Ar << Direction;
	}

	if ((RepBits & EGEContextRepBits::EventMagnitude) != EGEContextRepBits::None)
	{
		Ar << EventMagnitude;
	}

	if ((RepBits & EGEContextRepBits::DamageType) != EGEContextRepBits::None)
	{
		Ar << DamageType;
	}

	if ((RepBits & EGEContextRepBits::BrokeBone) != EGEContextRepBits::None)
	{
		Ar << bBrokeBone;
	}

	if ((RepBits & EGEContextRepBits::KnockbackMode) != EGEContextRepBits::None)
	{
		Ar << KnockbackMode;
	}

	if ((RepBits & EGEContextRepBits::KnockbackForce) != EGEContextRepBits::None)
	{
		Ar << KnockbackForce;
	}

	if ((RepBits & EGEContextRepBits::ContextTag) != EGEContextRepBits::None)
	{
		Ar << ContextTag;
	}

	if (Ar.IsLoading())
	{
		AddInstigator(Instigator.Get(), EffectCauser.Get()); // Just to initialize InstigatorAbilitySystemComponent
	}

	bOutSuccess = true;
	return true;
}

void FPOTGameplayEffectContext::GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const
{
	Super::GetOwnedGameplayTags(ActorTagContainer, SpecTagContainer);
	if (ContextTag.IsValid())
	{
		SpecTagContainer.AddTagFast(ContextTag);
	}
	
}
