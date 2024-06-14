// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Abilities/PlayerAbilitySystemComponent.h"

void UPlayerAbilitySystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	/*if (UWaGameplayAbility* WGA = GetCurrentAttackAbility())
	{
		if (WGA->CanBeCanceled() && ParentPlayerCharacter->HasAnyInput(true))
		{
			UE_LOG(LogTemp, Log, TEXT("CANCELLED ABILITY!"));
			WGA->K2_CancelAbility();
		}
	}*/
}

void UPlayerAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	
}

void UPlayerAbilitySystemComponent::PostDeath()
{
// 	ParentPlayerCharacter->DisableInput(ParentPlayerCharacter->GetWaPlayerControllerSafe());
// 	auto& WASG = UPOTAbilitySystemGlobals::Get();
// 	ParentPlayerCharacter->AddLock(ELockType::LT_MOVEMENT, WASG.MovementLockTag);
// 	ParentPlayerCharacter->AddLock(ELockType::LT_JUMP, WASG.JumpLockTag);
// 	ParentPlayerCharacter->AddLock(ELockType::LT_ABILITIES, WASG.AbilitiesLockTag);
}


