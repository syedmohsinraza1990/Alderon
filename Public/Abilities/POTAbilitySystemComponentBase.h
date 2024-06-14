// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "POTAbilityTypes.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "ITypes.h"
#include "POTAbilitySystemComponentBase.generated.h"

USTRUCT(BlueprintType)
struct FSavedGameplayEffectData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	const UGameplayEffect* Effect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	float Duration;

public:
	FSavedGameplayEffectData()
	{
	}

	FSavedGameplayEffectData(const UGameplayEffect* InEffect, float InDuration)
		: Effect(InEffect)
		, Duration(InDuration)
	{
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIPausedEffectRemoved, const UGameplayEffect*, Effect);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnUIEffectUpdated, FActiveGameplayEffectHandle, Handle, float, Duration, int32, Stacks);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUIEffectRemoved, const FActiveGameplayEffect&, ActiveEffect);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnUIEffectHandleUpdated, const FActiveGameplayEffectHandle&, OldHandle, const FActiveGameplayEffectHandle&, NewHandle);


/**
 * 
 */
UCLASS(ClassGroup = POT, hidecategories = (Object, LOD, Lighting, Transform, Sockets, TextureStreaming), editinlinenew, meta = (BlueprintSpawnableComponent))
class PATHOFTITANS_API UPOTAbilitySystemComponentBase : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	//The attribute sets that this character has.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TArray<TSubclassOf<class UAttributeSet>> AttributeSets;

	template <class T>
	T* GetAttributeSet_Mutable()
	{
		for (UAttributeSet* AttrSet : GetSpawnedAttributes())
		{
			if (T* CustomAttrSet = Cast<T>(AttrSet))
			{
				return CustomAttrSet;
			}
		}
		return nullptr;
	}

	//Whether or not this component has finished initializing.
	bool bInitialized;


	UPROPERTY(EditAnywhere, Category = "Character")
	FName NameOverride;

	// Usually Instigator is the same as the owner of AbilitySystemComponent but doesn't have to be
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	bool bForceDeflectOnInstigator;

	bool bAttributesLoaded;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TArray<TSubclassOf<UGameplayEffect>> InitialGameplayEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character")
	TArray<TSubclassOf<UGameplayAbility>> InitialGameplayAbilities;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> InstaKillEffect;
	
	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> LegDamageEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> WellRestedEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> JumpCostEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> BuckingTargetEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> DeadBodyFoodDecayEffect;

public:
	UPROPERTY(BlueprintAssignable, Category = "Wa Character")
	FOnUIEffectUpdated OnUIEffectUpdated;
	UPROPERTY(BlueprintAssignable, Category = "Wa Character")
	FOnUIEffectRemoved OnUIEffectRemoved;
	UPROPERTY(BlueprintAssignable, Category = "Wa Character")
	FOnUIEffectHandleUpdated OnUIEffectHandleUpdated;

	UPROPERTY()
	FOnUIPausedEffectRemoved OnUIPausedEffectRemoved;

	UFUNCTION(Client, Reliable)
	void Client_OnEffectRemoved(const FActiveGameplayEffect& ActiveEffect);

public:
	UPOTAbilitySystemComponentBase();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;

	virtual void OnGiveAbility(FGameplayAbilitySpec& AbilitySpec) override;

	virtual void InitializeAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor, const bool bPreviewOnly = false);

	void InitOwnerTeamAgentInterface(AActor* InOwnerActor);

	virtual void InitAttributes(bool bInitialInit = true);

	UFUNCTION(BlueprintPure, Category = "Combat", DisplayName = "Is Alive")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsHostile(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsNeutral(AActor* Target);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsFriendly(AActor* Target);

	virtual void PostDeath();
	virtual FName GetAttributeName() const;

	virtual int32 GetLevel() const;
	virtual float GetLevelFloat() const;

	UFUNCTION(BlueprintPure)
	float GetHealthPercentage();

	UFUNCTION(BlueprintPure)
	float GetMaxHealth();

	UFUNCTION(BlueprintPure)
	float GetCurrentHealth();

	UFUNCTION(BlueprintCallable, Category = Ability, meta = (Categories = "GameplayCue"))
	void ExecuteGameplayCueAtHit(const FGameplayTag GameplayCue,
		const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = Ability, meta = (Categories = "GameplayCue"))
	void ExecuteGameplayCueAtTransform(const FGameplayTag GameplayCue,
		const FTransform& CueTransform);


	UFUNCTION(BlueprintCallable, Category = Ability)
	void ApplyEffectContainerSpec(const FPOTGameplayEffectContainerSpec& ContainerSpec);

	//Will return 0 if ability is not on cooldown.
	UFUNCTION(BlueprintPure)
	float GetCooldownTimeRemaining(const TSubclassOf<UGameplayAbility>& AbilityClass) const;

	virtual TArray<UAnimMontage*> GetActiveMontages() const 
	{
		return TArray<UAnimMontage*>();
	}

	virtual UAnimMontage* GetCurrentAttackMontage() const
	{
		return nullptr;
	}

	virtual bool CurrentAbilityIsExclusive() const
	{
		return false;
	}

	virtual bool CurrentAbilityExists() const
	{
		return false;
	}

	virtual class UPOTGameplayAbility* GetCurrentAttackAbility() const
	{
		return nullptr;
	}

	FActiveGameplayEffect* GetActiveCooldownEffect(const TSubclassOf<UGameplayAbility>& AbilityClass);
	FActiveGameplayEffect GetActiveEffect(const TSubclassOf<UGameplayEffect>& EffectClass);

	UFUNCTION(BlueprintPure)
	bool CanActivateAbility(const TSubclassOf<UGameplayAbility>& InAbility) const;

	void ApplyAbilityCooldown(const TSubclassOf<UGameplayAbility>& InAbility, float CooldownTime);

	UFUNCTION(BlueprintPure)
	float GetTotalHealthBleed(const bool bRatio = false) const;

	UFUNCTION(BlueprintPure)
	float GetTotalStaminaDrain(const bool bRatio = false) const;

	/**
	 * @param Delta raw number
	 * @param DeltaPercentage - percentage of effect duration
	 * If DeltaPercentage is != 0 then Delta will be ignored
	 */
	UFUNCTION(BlueprintCallable, Category = "Wa Combat", meta = (AdvancedDisplay = 2))
	void ModifyActiveGameplayEffectDuration(FGameplayEffectQuery Query, float Delta, float DeltaPercentage = 0.f);
	/**
	 * @param Delta raw number
	 * @param DeltaPercentage - percentage of effect duration
	 * If DeltaPercentage is != 0 then Delta will be ignored
	 */
	UFUNCTION(BlueprintCallable, Category = "Wa Combat", meta = (AdvancedDisplay = 2))
	void ModifyActiveGameplayEffectDurationByHandle(FActiveGameplayEffectHandle Handle, float Delta, float DeltaPercentage = 0.f);

	UFUNCTION(BlueprintCallable)
	void SetGrowthForceInhibited(bool bInhibited)
	{
		bGrowthForceInhibited = bInhibited;
	}

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsGrowthInhibited() const
	{
		return bGrowthInhibited || bGrowthForceInhibited;
	}

	void AddNewCooldownEffect(FActiveGameplayEffectHandle& CooldownEffect);
	void AddCooldownDelay(const UGameplayEffect* CooldownEffect, float Duration);

	const TArray<TPair<const UGameplayEffect*, FTimerHandle>>& GetCurrentCooldownTimers() const;	
	const TArray<FActiveGameplayEffect>& GetCurrentCooldownEffects() const;

	void ClearCurrentCooldownDelays();

	void ExpireOldCooldownEffect(FActiveGameplayEffect Effect);

	/**
	* @brief SERVER ONLY: Reset all abilities' cooldown.
	* 
	* @Param Caller The Authoritative Info that forces the reset.
	*/
	UFUNCTION(BlueprintAuthorityOnly, BlueprintCallable)
	void ResetAllCooldowns(AInfo* Caller);

	void RemoveAllCooldowns();

	virtual void NotifyTimeOfDay(float InTime);

	UPROPERTY(VisibleAnywhere)
	FGameplayTag CurrentTimeOfDay = FGameplayTag();

protected:
	UPROPERTY(Config)
	float GrowthInflationRatio = 2.f;

protected:

	UPROPERTY(Transient)
	TScriptInterface<class IGenericTeamAgentInterface> OwnerTeamAgentInterface;

	void ApplySoftClassPtrEffects(const TArray<TSoftClassPtr<UGameplayEffect>>& Effects);

	uint64 NextAsyncGameplayEffectRequestId = 0;

	TMap<uint64, TSharedPtr<FStreamableHandle>> AsyncGameplayEffectHandles = {};

	UFUNCTION()
	virtual void OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);
	UFUNCTION()
	virtual void OnActiveEffectRemoved(const FActiveGameplayEffect& ActiveEffect);
	UFUNCTION()
	virtual void OnActiveEffectStackChange(FActiveGameplayEffectHandle ActiveHandle, int32 NewStackCount, int32 PreviousStackCount);
	UFUNCTION()
	virtual void OnActiveEffectTimeChange(FActiveGameplayEffectHandle ActiveHandle, float NewStartTime, float NewDuration);

	void MaintainedStackExpire(FActiveGameplayEffectHandle ActiveHandle, int Stacks);

	TMap<FActiveGameplayEffectHandle, TArray<FTimerHandle>> MaintainedStackDurations;


	friend class UPOTGameplayAbility;

	bool AttemptToFixGEHandles(TSubclassOf<UGameplayEffect> EffectToCompare = nullptr);

	virtual void AdjustGameplayEffectFoodTypes();
	virtual void AdjustGameplayEffectAdditionalAbilities();
	virtual void PotentiallyStartGrowthInhibitionCheck();

	FTimerHandle TimerHandle_Reactivation;
	virtual void InternalServerTryActivateAbility(FGameplayAbilitySpecHandle Handle, bool InputPressed, const FPredictionKey& PredictionKey, const FGameplayEventData* TriggerEventData) override;
	void RetryInternalServerTryActivateAbility(FGameplayAbilitySpecHandle Handle, bool InputPressed, const FPredictionKey PredictionKey, FGameplayEventData TriggerEventData, bool bHasTriggerEventData, bool bSkipTick);

	float GetCurrentAttackMontageTimeRemaining();
	bool TryEndExclusiveAbility(FGameplayAbilitySpecHandle Handle, const UPOTGameplayAbility* Ability, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags);

	UFUNCTION(Client, Reliable)
	void Client_OnAbilityDelayed(FGameplayAbilitySpecHandle Handle, float DelayAmount);
	void Client_OnAbilityDelayed_Implementation(FGameplayAbilitySpecHandle Handle, float DelayAmount);

	UFUNCTION(Client, Reliable)
	void Client_OnRestoreCooldown(FGameplayAbilitySpecHandle Handle, float CooldownAmount);
	void Client_OnRestoreCooldown_Implementation(FGameplayAbilitySpecHandle Handle, float CooldownAmount);

	UFUNCTION(Client, Reliable)
	void Client_RemoveCooldown(FGameplayAbilitySpecHandle Handle);
	void Client_RemoveCooldown_Implementation(FGameplayAbilitySpecHandle Handle);
	
	UFUNCTION(Client, Reliable)
	void Client_RemoveAllCooldowns();
	void Client_RemoveAllCooldowns_Implementation();

	UPROPERTY()
	TArray<FActiveGameplayEffect> CurrentCooldownEffects;

	TArray<TPair<const UGameplayEffect*, FTimerHandle>> CurrentCooldownDelays;

	void ClearDelayedCooldown(const UGameplayEffect* Cooldown);

private:
	bool bGrowthInhibited;
	bool bGrowthForceInhibited;

	FTimerHandle GrowthInhibitionTimerHandle;
	FActiveGameplayEffectHandle GrowthInhibitionHandle;
private:
	UPROPERTY(Transient)
	TMap<TSubclassOf<UGameplayEffect>, FActiveGameplayEffectHandle> TrackedClientUIEffects;

	UFUNCTION()
	void OnAttributeSetDataReloaded(bool bIsFromCSV, bool bIsModData);

	UFUNCTION()
	void OnGEAsyncLoaded(TSoftClassPtr<UGameplayEffect> GEC, uint64 AsyncRequestId);

	UFUNCTION()
	void OnInhibitionTimer();

	void CheckGrowthInhibition(bool bInitialTest = false);

	void BlockGrowth();
	void UnblockGrowth();
};
