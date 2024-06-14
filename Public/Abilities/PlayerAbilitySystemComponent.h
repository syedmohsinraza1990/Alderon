// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "POTAbilitySystemComponent.h"
#include "PlayerAbilitySystemComponent.generated.h"

/**
 * 
 */
UCLASS()
class PATHOFTITANS_API UPlayerAbilitySystemComponent : public UPOTAbilitySystemComponent
{
	GENERATED_BODY()


public:
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	virtual void InitializeComponent() override;
	virtual void PostDeath() override;


};
