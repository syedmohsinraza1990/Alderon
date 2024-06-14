// Copyright 2019-2023 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/POTGameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "POTGameplayAbility_Clamp.generated.h"

class AIBaseCharacter;
class UAbilityTask_WaitGameplayEvent;

UCLASS(Blueprintable)
class PATHOFTITANS_API UPOTGameplayAbility_Clamp : public UPOTGameplayAbility
{
	GENERATED_BODY()

public:

	void CancelClamp() { if (OwningPOTCharacter) K2_EndAbility(); }

protected:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool CanCarry(const AIBaseCharacter* PotentialTarget) const;
	virtual bool CanCarry_Implementation(const AIBaseCharacter* PotentialTarget) const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void DoAttach(AIBaseCharacter* Target);
	virtual void DoAttach_Implementation(AIBaseCharacter* Target);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void DoDetach(AIBaseCharacter* Target);
	virtual void DoDetach_Implementation(AIBaseCharacter* Target);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AutoDetach(AIBaseCharacter* OldTarget, const EAttachType AttachType);
	virtual void AutoDetach_Implementation(AIBaseCharacter* OldTarget, const EAttachType AttachType);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void WeaponHitGameplayEvent(FGameplayEventData Payload);
	virtual void WeaponHitGameplayEvent_Implementation(FGameplayEventData Payload);

	UFUNCTION(BlueprintCallable)
	AIBaseCharacter* GetCarriedCharacter() const;

	virtual void K2_PostMontageActivateAbility_Implementation() override;

	virtual void OnTagChanged_Implementation(const FGameplayTag Tag, int32 NewCount) override;

	virtual void K2_EndAbility() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag WeaponHitGameplayTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Shared.WeaponHit"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag StaminaExhaustedGameplayTag = FGameplayTag::RequestGameplayTag(TEXT("Debuff.StaminaExhausted"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> CarryCostEffectClass = nullptr;
	
	UPROPERTY(BlueprintReadWrite)
	FActiveGameplayEffectHandle ClampGEHandle = FActiveGameplayEffectHandle();

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* AbilityTaskWaitGameplayEvent = nullptr;
};
