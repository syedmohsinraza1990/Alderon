// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Abilities/POTTargetType.h"
#include "Abilities/POTGameplayAbility.h"

void UPOTTargetType::GetTargets_Implementation(AActor* TargetingActor, 
	const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	return;
}

void UWaTargetType_UseOwner::GetTargets_Implementation(AActor* TargetingActor, 
	const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults,
	TArray<AActor*>& OutActors) const
{
	OutActors.Add(TargetingActor);
}

void UPOTTargetType_UseEventData::GetTargets_Implementation(AActor* TargetingActor, 
	const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults,
	TArray<AActor*>& OutActors) const
{
	const FHitResult* FoundHitResult = EventData.ContextHandle.GetHitResult();
	if (FoundHitResult != nullptr)
	{
		OutHitResults.Add(*FoundHitResult);
	}
	else if (EventData.Target)
	{
		OutActors.Add(const_cast<AActor*>(EventData.Target.Get()));
	}
}


void UPOTTargetType_UseEventInstigator::GetTargets_Implementation(AActor* TargetingActor, const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults, TArray<AActor*>& OutActors) const
{
	if (EventData.Instigator)
	{
		OutActors.Add(const_cast<AActor*>(EventData.Instigator.Get()));
	}
}
