// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/POTAbilityTypes.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffect.h"
#include "Engine/CurveTable.h"
#include "ITypes.h"
#include "POTAbilitySystemGlobals.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttributeSetDataReloaded, bool, bIsFromCSV, bool, bIsModData);


USTRUCT()
struct FModAbilityRedirect
{

	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, FPrimaryAssetId> Redirects;

public:
	FModAbilityRedirect()
	{}

	FModAbilityRedirect(const TMap<FPrimaryAssetId, FPrimaryAssetId>& InRedirects)
		: Redirects(InRedirects)
	{}
};

USTRUCT()
struct FCurveTableRowOverrideInfo
{
	GENERATED_BODY()

public:

	UPROPERTY()
	UCurveTable* Table = nullptr;
	UPROPERTY()
	FName RowName;
	
	FRealCurve* OriginalCurve;

};

USTRUCT()
struct FCurveTableOverrideArray
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FCurveTableRowOverrideInfo> OverrideInfoArray;
};

/**
 * 
 */
UCLASS()
class PATHOFTITANS_API UPOTAbilitySystemGlobals : public UAbilitySystemGlobals
{
	GENERATED_BODY()
	
public:
	UPROPERTY(Config)
	FSoftObjectPath WeightRatioDamageMultiplierCurveName;

	UPROPERTY(BlueprintReadOnly)
	UCurveFloat* WeightRatioDamageMultiplierCurve;

	UPROPERTY(Config)
	FSoftClassPath InCombatEffectName;

	UPROPERTY(Config)
	FSoftClassPath InHomecaveEffectName;

	UPROPERTY(Config)
	FSoftClassPath InGroupEffectName;
	
	UPROPERTY(Config)
	FSoftClassPath HomecaveExitBuffEffectName;

	UPROPERTY(Config)
	FSoftClassPath HomecaveCampingDebuffEffectName;

	UPROPERTY(Config)
	FSoftClassPath GroupBuffEffectName;

	UPROPERTY(Config)
	FSoftClassPath RAFGroupBuffEffectName;

	UPROPERTY(Config)
	FSoftClassPath LoginEffectName;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> InCombatEffect;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> InHomecaveEffect;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> InGroupEffect;
	
	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> HomecaveExitBuffEffect;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> HomecaveCampingDebuffEffect;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> GroupBuffEffect;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> RAFGroupBuffEffect;

	UPROPERTY(BlueprintReadOnly)
	TSubclassOf<UGameplayEffect> LoginEffect;
	
	UPROPERTY(Config)
	FSoftClassPath GrowthRewardGameplayEffectName;

	UPROPERTY(Config)
	FSoftClassPath GrowthDefaultGameplayEffectName;

	UPROPERTY(Config)
	FSoftClassPath GrowthInhibitedGameplayEffectName;

	UPROPERTY(Config)
	FSoftClassPath WaystoneInvitePendingGameplayEffectName;

	UPROPERTY(Config)
	FSoftClassPath WaystoneInviteChargingGameplayEffectName;

	/**
	* Should be a gameplay effect with SetByCaller magnitudes for duration and GrowthPerSecond
	*/
	UPROPERTY()
	TSubclassOf<UGameplayEffect> GrowthRewardGameplayEffect;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> GrowthDefaultGameplayEffect;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> GrowthInhibitedGameplayEffect;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> WaystoneInvitePendingGameplayEffect;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> WaystoneInviteChargingGameplayEffect;

	/** Curve tables from mods containing all sorts of overrides */
	UPROPERTY(Transient)
	TMap<FString, UCurveTable*> ModCurveTables;

	UPROPERTY(Transient)
	TMap<FString, FModAbilityRedirect> ModAbilities;

	UPROPERTY(Transient)
	TMap<FString, FCurveTableOverrideArray> CombatValueOverrides;

public:
	UPROPERTY(BlueprintAssignable)
	FOnAttributeSetDataReloaded OnAttributeSetDataReloaded;
	

public:

	static UPOTAbilitySystemGlobals& Get()
	{
		return *static_cast<UPOTAbilitySystemGlobals*>(IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals());
	}

	virtual void InitGlobalData() override;
	virtual void ReloadAttributeDefaults() override;

	FVector CalculateForceDirection(AActor* DamagedActor, AActor* Instigator, const FHitResult DamageHitResult, EKnockbackMode KnockbackMode) const;

	void AddGASMod(const FString& ModID);
	void RemoveGASMod(const FString& ModID);
	void UpdateGASMods();

	UFUNCTION(BlueprintCallable, DisplayName = "Test Load Mod")
	static void BP_TestModLoad(const FString& ModID, TSubclassOf<UIModData> ModDataClass);
	void TestModLoad(const FString& ModID, UIModData* IModData);

	UFUNCTION(BlueprintCallable, DisplayName = "Reload Attributes From CSV")
	static void BP_ReloadAttributesFromCSV(const FString& CSV);
	void ReloadAttributesFromCSV(const FString& CSV);


	virtual void AllocAttributeSetInitter() override;
	virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;

	virtual void InitGameplayCueParameters_Transform(FGameplayCueParameters& CueParameters, UGameplayAbility* Ability, 
		const FTransform& DestinationTransform);
	virtual void InitGameplayCueParameters_Transform(FGameplayCueParameters& CueParameters, UAbilitySystemComponent* ASC,
		const FTransform& DestinationTransform);
	virtual void InitGameplayCueParameters_HitResult(FGameplayCueParameters& CueParameters, UGameplayAbility* Ability, 
		const FHitResult& HitResult);
	virtual void InitGameplayCueParameters_HitResult(FGameplayCueParameters& CueParameters, UAbilitySystemComponent* ASC,
		const FHitResult& HitResult);
	virtual void InitGameplayCueParameters_Actor(FGameplayCueParameters& CueParameters, UGameplayAbility* Ability, 
		const AActor* InTargetActor);
	virtual void InitGameplayCueParameters_Actor(FGameplayCueParameters& CueParameters, UAbilitySystemComponent* ASC,
		const AActor* InTargetActor);
	
	UFUNCTION(BlueprintPure, Category = "Wa Abilities")
	static class UPOTAbilitySystemComponent* GetAbilitySystemComponentFromActor(const AActor* Actor, 
		bool bLookForComponent = false);

	UFUNCTION(BlueprintPure, Category = "Wa Abilities")
	static class UPOTAbilitySystemComponentBase* GetAbilitySystemComponentBaseFromActor(const AActor* Actor,
		bool bLookForComponent = false);

	UFUNCTION(BlueprintPure, Category = "Wa Abilities")
	static class AActor* GetInstigatorFromGameplayEffectSpec(const FGameplayEffectSpec& InSpec);

	UFUNCTION(BlueprintCallable, Category = "Wa Abilities")
	static TArray<FActiveGameplayEffectHandle> ApplyExternalEffectContainerSpec(const FPOTGameplayEffectContainerSpec& ContainerSpec);
	UFUNCTION(BlueprintPure, Category = "Wa Abilities")
	static bool DoesEffectContainerSpecHaveEffects(const FPOTGameplayEffectContainerSpec& ContainerSpec);
	UFUNCTION(BlueprintPure, Category = "Wa Abilities")
	static bool DoesEffectContainerSpecHaveTargets(const FPOTGameplayEffectContainerSpec& ContainerSpec);
	UFUNCTION(BlueprintCallable, Category = "Wa Abilities", meta = (AutoCreateRefTerm = "HitResults,TargetActors"))
	FPOTGameplayEffectContainerSpec AddTargetsToEffectContainerSpec(const FPOTGameplayEffectContainerSpec& ContainerSpec, const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors);
	UFUNCTION(BlueprintCallable, Category = "Wa Abilities")
	static void SetEndPointsInTargetData(UPARAM(Ref)FGameplayAbilityTargetDataHandle& TargetData, const FGameplayAbilityTargetingLocationInfo& EndPoint);
	UFUNCTION(BlueprintPure, Category = "Wa Abilities")
	static FName GetTargetDataSourceSocketName(UPARAM(Ref)FGameplayAbilityTargetDataHandle& TargetData);

	UFUNCTION(BlueprintPure, Category = "Wa Abilities", meta = (AdvancedDisplay = "2"))
	static float GetValueAtLevel(const FScalableFloat& ScalableFloat,
		const float Level = 0.f, const FString ContextString = "");

	UFUNCTION(BlueprintPure)
	static bool IsValidAbilityHandle(UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void OverrideTargetDataInGameplayEffectContainerSpec(UPARAM(Ref)FPOTGameplayEffectContainerSpec& ContainerSpec, FGameplayAbilityTargetDataHandle TargetData);
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void OverrideTargetGameplayEffectSpecInGameplayEffectContainerSpec(UPARAM(Ref)FPOTGameplayEffectContainerSpec& ContainerSpec, TArray<FGameplayEffectSpecHandle> SpecHandles);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void OverrideContextHandleInEventData(UPARAM(Ref)FGameplayEventData& EventData, FGameplayEffectContextHandle ContextHandle);

	UFUNCTION(BlueprintCallable, Category = "Abilities", meta = (DisplayName = "Set Event Magnitude in EffectContext"))
	static void EffectContextOverrideEventMagnitude(UPARAM(Ref)FGameplayEffectContextHandle& GEContextHandle, const float EventMagnitude);

	UFUNCTION(BlueprintCallable, Category = "Abilities", meta = (DisplayName = "Set Causer in EffectContext"))
	static void EffectContextOverrideCauser(UPARAM(Ref)FGameplayEffectContextHandle& GEContextHandle, AActor* NewCauser);


	UFUNCTION(BlueprintCallable, Category = "Abilities", meta = (DisplayName = "Set Period in EffectSpec"))
	static void GameplayEffectSpecHandleOverridePeriod(UPARAM(Ref)FGameplayEffectSpecHandle& GESpecHandle, const float NewPeriod);

	UFUNCTION(BlueprintPure, Category = "Abilities")
	static bool GetActiveGameplayEffectInfo(const FActiveGameplayEffectHandle Handle, float& Duration, float& RemainingTime, int32& StackCount, FGameplayEffectContextHandle& EffectContext);

	static const FActiveGameplayEffect* GetActiveGameplayEffect(const FActiveGameplayEffectHandle& Handle);

	UFUNCTION(BlueprintPure, Category = "Abilities", meta = (DisplayName = "Get Effects Using Effect Class"))
	static TArray<FActiveGameplayEffectHandle> GetActiveGameplayEffectsByClass(const UAbilitySystemComponent* const AbilitySystemComp, const TSubclassOf<UGameplayEffect> EffectClass);
	
	UFUNCTION(BlueprintPure, Category = "Abilities")
	static const UGameplayEffectUIData* GetActiveGameplayEffectUIData(const FActiveGameplayEffectHandle& Handle);
	
	static bool HasGameplayEffectUIDataFromActiveEffect(const FActiveGameplayEffect* ActiveEffect);

	UFUNCTION(BlueprintPure, Category = "Abilities")
	static bool HasGameplayEffectUIData(const FActiveGameplayEffectHandle& Handle);

	static const UGameplayEffectUIData* GetActiveGameplayEffectUIDataFromActiveEffect(const FActiveGameplayEffect* ActiveEffect);

	UFUNCTION(BlueprintPure, Category = "Abilities", meta = (DisplayName = "Get Event Magnitude in EffectContext"))
	static float EffectContextGetEventMagnitude(UPARAM(Ref)FGameplayEffectContextHandle& GEContextHandle);
	
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void ApplyDynamicGameplayEffect(class AActor* InActor, const FGameplayAttribute& Attribute, const float Magnitude, EGameplayModOp::Type ModOp = EGameplayModOp::Additive, EDamageType DamageTypeOverride = EDamageType::DT_GENERIC);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void ClearGrowth(AActor* Actor);

	/**
	 * Reward the given amount of growth over the given amount of time
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static FActiveGameplayEffectHandle RewardGrowth(AActor* Actor, float TotalGrowth, float DurationSeconds);

	/**
	 * Reward the given amount of growth at the given rate
	 */
	UFUNCTION(BlueprintCallable, Category = "Abilities")
    static FActiveGameplayEffectHandle RewardGrowthConstantRate(AActor* Actor, float TotalGrowth, float GrowthPerSecond);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static FActiveGameplayEffectHandle RewardWellRestedBonus(AActor* Actor, float WellRestedStart, float WellRestedEnd);

	static void ReplaceGEBlueprintRowNameText(class UObject* GEObj, FString TextToSearch, FString ReplaceText, UCurveTable* NewCurve);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void ReplaceGERowNameText(UGameplayEffect* GE, FString TextToSearch, FString ReplaceText, UCurveTable* NewCurve);

	UFUNCTION(BlueprintCallable)
	static void SetDamageTypeInEventData(UPARAM(Ref) FGameplayEventData& EventData, EDamageType NewDamageType);

	// Can be deleted after merging to unstable. See: https://github.com/Alderon-Games/pot-development/issues/4063 for more information.
	// UFUNCTION(BlueprintCallable, Category = "Abilities")
	// static TArray<FGameplayModifierInfo> GetModifiersWithReplacedRowNames(TSubclassOf<UGameplayEffect> GEClass, FString TextToSearch, FString ReplaceText);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void SetScalableFloatInModifier(UPARAM(ref)FGameplayModifierInfo& ModInfo, const FScalableFloat& Float);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static FScalableFloat GetScalableFloatInModifier(const FGameplayModifierInfo& ModInfo);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	static class UCurveTable* CastToCurveTable(UObject* Object);

	UFUNCTION(BlueprintPure, Category = "Abilities")
	static UGameplayEffect* GetGameplayEffectDefinitionCDO(TSubclassOf<UGameplayEffect> GEClass);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void DebugUpdateCurve(const FName RowName, const UCurveFloat* NewCurve);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	static void DebugRestoreCurve();

	void ApplyServerSettingCurves(const TArray<FCurveOverrideData>& Data);
	void RestoreServerSettingCurves();

private:
	void UpdateModAttributes();

	bool UpdateCurve(const FName RowName, const FRealCurve* NewCurve, FCurveTableRowOverrideInfo& OutOverrideInfo);
	bool UpdateCurve(const FName RowName, const TArray<float>& Values, FCurveTableRowOverrideInfo& OutOverrideInfo);
	void RestoreCurve(const FCurveTableRowOverrideInfo& OverrideInfo);

	UFUNCTION()
	static bool ShouldFilterEffect(const UAbilitySystemComponent* ASC, const TSubclassOf<UGameplayEffect> EffectClass);

	void ProcessModData(const FString& ModID, class UIModData* IModData);

public:
	UPROPERTY()
	FGameplayTag FullDamageImmunityTag;
	UPROPERTY(Config)
	FName FullDamageImmunityTagName;
	UPROPERTY()
	FGameplayTag KnockbackImmunityTag;
	UPROPERTY(Config)
	FName KnockbackImmunityTagName;
	UPROPERTY()
	FGameplayTag LastHitDamageImmunityTag;
	UPROPERTY(Config)
	FName LastHitDamageImmunityTagName;
	UPROPERTY()
	FGameplayTag PassThroughDamageImmunityTag;
	UPROPERTY(Config)
	FName PassThroughDamageImmunityTagName;
	UPROPERTY()
	FGameplayTag CombatTag;
	UPROPERTY(Config)
	FName CombatTagName;
	UPROPERTY()
	FGameplayTag UnderwaterTag;
	UPROPERTY(Config)
	FName UnderwaterTagName;
	UPROPERTY()
	FGameplayTag StaminaExhaustionTag;
	UPROPERTY(Config)
	FName StaminaExhaustionTagName;
	UPROPERTY()
	FGameplayTag BuckingTag;
	UPROPERTY(Config)
	FName BuckingTagName;
	UPROPERTY()
	FGameplayTag SurvivalDisabledTag;
	UPROPERTY(Config)
	FName SurvivalDisabledTagName;
	UPROPERTY()
	FGameplayTag NoGrowTag;
	UPROPERTY(Config)
	FName NoGrowTagName;
	UPROPERTY()
	FGameplayTag DeadTag;
	UPROPERTY(Config)
	FName DeadTagName;
	UPROPERTY()
	FGameplayTag GodmodeTag;
	UPROPERTY(Config)
	FName GodmodeTagName;
	UPROPERTY()
	FGameplayTag GroundedTag;
	UPROPERTY(Config)
	FName GroundedTagName;
	UPROPERTY()
	FGameplayTag FoodTypeTag;
	UPROPERTY(Config)
	FName FoodTypeTagName;
	UPROPERTY()
	FGameplayTag CanJumpTag;
	UPROPERTY(Config)
	FName CanJumpTagName;
	UPROPERTY()
	FGameplayTag CanDiveTag;
	UPROPERTY(Config)
	FName CanDiveTagName;
	UPROPERTY()
	FGameplayTag FallDamageTag;
	UPROPERTY(Config)
	FName FallDamageTagName;
	UPROPERTY()
	FGameplayTag FriendlyInstanceTag;
	UPROPERTY(Config)
	FName FriendlyInstanceTagName;
	UPROPERTY()
	FGameplayTag EnvironmentalEffectTag;
	UPROPERTY(Config)
	FName EnvironmentalEffectTagName;

	virtual void InitGlobalTags() override
	{
		Super::InitGlobalTags();

		if (FullDamageImmunityTagName != NAME_None)
		{
			FullDamageImmunityTag = FGameplayTag::RequestGameplayTag(FullDamageImmunityTagName);
		}

		if (KnockbackImmunityTagName != NAME_None)
		{
			KnockbackImmunityTag = FGameplayTag::RequestGameplayTag(KnockbackImmunityTagName);
		}

		if (LastHitDamageImmunityTagName != NAME_None)
		{
			LastHitDamageImmunityTag = FGameplayTag::RequestGameplayTag(LastHitDamageImmunityTagName);
		}

		if (PassThroughDamageImmunityTagName != NAME_None)
		{
			PassThroughDamageImmunityTag = FGameplayTag::RequestGameplayTag(PassThroughDamageImmunityTagName);
		}

		if (CombatTagName != NAME_None)
		{
			CombatTag = FGameplayTag::RequestGameplayTag(CombatTagName);
		}

		if (UnderwaterTagName != NAME_None)
		{
			UnderwaterTag = FGameplayTag::RequestGameplayTag(UnderwaterTagName);
		}

		if (StaminaExhaustionTagName != NAME_None)
		{
			StaminaExhaustionTag = FGameplayTag::RequestGameplayTag(StaminaExhaustionTagName);
		}

		if (BuckingTagName != NAME_None)
		{
			BuckingTag = FGameplayTag::RequestGameplayTag(BuckingTagName);
		}

		if (SurvivalDisabledTagName != NAME_None)
		{
			SurvivalDisabledTag = FGameplayTag::RequestGameplayTag(SurvivalDisabledTagName);
		}

		if (NoGrowTagName != NAME_None)
		{
			NoGrowTag = FGameplayTag::RequestGameplayTag(NoGrowTagName);
		}

		if (DeadTagName != NAME_None)
		{
			DeadTag = FGameplayTag::RequestGameplayTag(DeadTagName);
		}

		if (GodmodeTagName != NAME_None)
		{
			GodmodeTag = FGameplayTag::RequestGameplayTag(GodmodeTagName);
		}

		if (GroundedTagName != NAME_None)
		{
			GroundedTag = FGameplayTag::RequestGameplayTag(GroundedTagName);
		}
		
		if (FoodTypeTagName != NAME_None)
		{
			FoodTypeTag = FGameplayTag::RequestGameplayTag(FoodTypeTagName);
		}

		if (CanJumpTagName != NAME_None)
		{
			CanJumpTag = FGameplayTag::RequestGameplayTag(CanJumpTagName);
		}

		if (CanDiveTagName != NAME_None)
		{
			CanDiveTag = FGameplayTag::RequestGameplayTag(CanDiveTagName);
		}

		if (FallDamageTagName != NAME_None)
		{
			FallDamageTag = FGameplayTag::RequestGameplayTag(FallDamageTagName);
		}

		if (FriendlyInstanceTagName != NAME_None)
		{
			FriendlyInstanceTag = FGameplayTag::RequestGameplayTag(FriendlyInstanceTagName);
		}

		if (EnvironmentalEffectTagName != NAME_None)
		{
			EnvironmentalEffectTag = FGameplayTag::RequestGameplayTag(EnvironmentalEffectTagName);
		}
	}
};
