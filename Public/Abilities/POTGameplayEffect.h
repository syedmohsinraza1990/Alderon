// Copyright 2015-2020 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "POTGameplayEffect.generated.h"

/**
 * 
 */
UCLASS()
class PATHOFTITANS_API UPOTGameplayEffect : public UGameplayEffect
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	bool bMaintainOldStackDuration = false;

	/** Determines whether to track the number of sources that are applying this effect. To be used with MaxSourcesCount. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	bool bTrackSourcesCount = false;
	/** Maximum number of sources that can apply this gameplay effect */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Stacking)
	int32 MaxSourcesCount;

	/** Modifies bone damage multipliers while ability is active. Stacks multiplicatively with base character BoneDamageMultipliers */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameplayEffect, meta = (DisplayAfter = "Executions"))
	TMap<FName, FBoneDamageModifier> BoneDamageMultipliers;
};
