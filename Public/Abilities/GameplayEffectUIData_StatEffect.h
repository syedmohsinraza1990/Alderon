// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectUIData.h"
#include "Engine/Texture2D.h"
#include "GameplayEffectUIData_StatEffect.generated.h"

class UStatusEffectEntry;

/**
 * 
 */
UCLASS()
class PATHOFTITANS_API UGameplayEffectUIData_StatEffect : public UGameplayEffectUIData
{
	GENERATED_BODY()

public:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data, meta = (MultiLine = "false"))
	FText Title;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data, meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	TSoftObjectPtr<UTexture2D> Icon;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	bool bHideCooldown = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	bool bInvertFade = false;

	/** Stacks icons for any instances that match all tags with this Effect AND associated Ability (one stack per Effect) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Data)
	bool bCombineStacks = false;
};
