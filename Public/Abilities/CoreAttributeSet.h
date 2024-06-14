// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "POTAbilitySystemComponent.h"
#include "POTAbilityTypes.h"
#include "AttributeSet.h"
#include "AttributeMacros.h"
#include "Engine/NetDriver.h"
#include "Net/RepLayout.h"
#include "CoreAttributeSet.generated.h"

struct POTDamageStatics;
struct POTSurvivalStatics;
struct POTStaminaStatics;

class UCoreAttributeSet;

USTRUCT()
struct FPOTAdjustForMaxAttribute
{
	GENERATED_BODY()

	FPOTAdjustForMaxAttribute()
	{
	}

	FPOTAdjustForMaxAttribute(FGameplayAttribute InAffectedAttributeProperty)
	{
		AffectedAttributeProperty = InAffectedAttributeProperty;
	}

	FGameplayAttribute AffectedAttributeProperty;
};

UENUM()
enum class EPOTAdjustMethod
{
	AM_None,
	AM_Clamp,
	AM_Max
};

USTRUCT()
struct FPOTAdjustCurrentAttribute
{
	GENERATED_BODY()

	
	FPOTAdjustCurrentAttribute()
	{
		AdjustMethod = EPOTAdjustMethod::AM_None;
	}

	FPOTAdjustCurrentAttribute(FGameplayAttributeData* InMaxAttribute)
	{
		MaxAttribute = InMaxAttribute;
		AdjustMethod = EPOTAdjustMethod::AM_Clamp;
	}

	FPOTAdjustCurrentAttribute(const float InMinValue)
	{
		MinValue = InMinValue;
		AdjustMethod = EPOTAdjustMethod::AM_Max;
	}

	FPOTAdjustCurrentAttribute(const float InMinValue, const float InMaxValue)
	{
		MinValue = InMinValue;
		MaxValue = InMaxValue;
		AdjustMethod = EPOTAdjustMethod::AM_Clamp;
	}

	FPOTAdjustCurrentAttribute(const float InMinValue, bool Fetch)
	{
		MinValue = InMinValue;
		AdjustMethod = EPOTAdjustMethod::AM_Clamp;
	}

	FPOTAdjustCurrentAttribute(FGameplayAttributeData* InMaxAttribute, const FName* InDebuffName)
	{
		MaxAttribute = InMaxAttribute;
		DebuffName = InDebuffName;
		AdjustMethod = EPOTAdjustMethod::AM_Clamp;
	}
	
	FPOTAdjustCurrentAttribute(FGameplayAttributeData* InMaxAttribute, const FName* InDebuffName, const float InMinValue, bool InFetchFromConfig)
	{
		MaxAttribute = InMaxAttribute;
		MinValue = InMinValue;
		DebuffName = InDebuffName;
		FetchMaxFromConfig = InFetchFromConfig;
		AdjustMethod = EPOTAdjustMethod::AM_Clamp;
	}
	
	FPOTAdjustCurrentAttribute(FGameplayAttributeData* InMaxAttribute, const FName* InDebuffName, const float InMinValue, const float InMaxValue = 1.f)
	{
		MaxAttribute = InMaxAttribute;
		MinValue = InMinValue;
		MaxValue = InMaxValue;
		DebuffName = InDebuffName;
		AdjustMethod = EPOTAdjustMethod::AM_Clamp;
	}

	FPOTAdjustCurrentAttribute(const FName* InDebuffName, const float InMinValue = 0.f)
	{
		MinValue = InMinValue;
		DebuffName = InDebuffName;
		AdjustMethod = EPOTAdjustMethod::AM_Max;
	}

	float MinValue = 0.f;
	float MaxValue = 0.f;
	bool FetchMaxFromConfig = false;
	FGameplayAttributeData* MaxAttribute = nullptr;
	const FName* DebuffName = nullptr;
	EPOTAdjustMethod AdjustMethod;
};

USTRUCT()
struct FPOTStatusHandling
{
	GENERATED_BODY()

	FPOTStatusHandling()
	{
		DamageEffectType = EDamageEffectType::NONE;
		bShouldApplyCombatGE = false,
		bRefreshCombatGEAtEveryTick = false;
		bApplyParticleOnlyOnce = false;
	}
	FPOTStatusHandling(EDamageEffectType InDamageEffectType,
		bool InbShouldApplyCombatGE = false,
		bool InbRefreshCombatGEAtEveryTick = false,
		bool InbApplyParticleOnlyOnce = false)
	{
		DamageEffectType = InDamageEffectType;
		bShouldApplyCombatGE = InbShouldApplyCombatGE,
		bRefreshCombatGEAtEveryTick = InbRefreshCombatGEAtEveryTick;
		bApplyParticleOnlyOnce = InbApplyParticleOnlyOnce;
	}

	EDamageEffectType DamageEffectType;
	bool bShouldApplyCombatGE;
	bool bRefreshCombatGEAtEveryTick;
	bool bApplyParticleOnlyOnce;
};

USTRUCT()
struct FPOTIncomingStatusHandling
{
	GENERATED_BODY()

	FPOTIncomingStatusHandling()
		: RateSetterFunc(nullptr),
		  IncomingRateSetterFunc(nullptr)
	{
		DamageEffectType = EDamageEffectType::NONE;
	}

	FPOTIncomingStatusHandling(
		EDamageEffectType InDamageEffectType,
		FGameplayAttribute InIncomingAttribute,
		FGameplayAttribute InCurrentAttribute,
		void (UCoreAttributeSet::*InIncomingRateSetterFunc)(float),
		void (UCoreAttributeSet::*InRateSetterFunc)(float))
	{
		DamageEffectType = InDamageEffectType;
		IncomingAttribute = InIncomingAttribute;
		CurrentAttribute = InCurrentAttribute;
		RateSetterFunc = InRateSetterFunc;
		IncomingRateSetterFunc = InIncomingRateSetterFunc;
	}

	EDamageEffectType DamageEffectType;
	FGameplayAttribute IncomingAttribute;
	FGameplayAttribute CurrentAttribute;
	void (UCoreAttributeSet::*RateSetterFunc)(float);
	void (UCoreAttributeSet::*IncomingRateSetterFunc)(float);
};

/**
 *
 */
UCLASS(Config = Game)
class PATHOFTITANS_API UCoreAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

	/** While these friends have access to the backing fields for attributes, you *must* use the accessors! */
	friend POTDamageStatics;
	friend POTSurvivalStatics;
	friend POTStaminaStatics;
	
public:

	/**
	 * Attribute Accessors
	 */

	// BEGIN - STATUS_UPDATE_MARKER
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, Wetness)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, LegDamage)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, LegHealRate)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, BleedingRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, BleedingHealRate)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, PoisonRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, PoisonHealRate)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, VenomRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, VenomHealRate)
	// END - STATUS_UPDATE_MARKER
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, Health)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, MaxHealth)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, HealthRecoveryRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, HealthRecoveryMultiplier)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, Stamina)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, MaxStamina)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaRecoveryRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaRecoveryMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, CombatWeight)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, Armor)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, MovementSpeedMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, TurnRadiusMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, TurnInPlaceRadiusMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, SprintingSpeedMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, TrottingSpeedMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, JumpForceMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, AirControlMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, BodyFoodAmount)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, CurrentBodyFoodAmount)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, BodyFoodCorpseThreshold)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, Hunger)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, MaxHunger)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, HungerDepletionRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, FoodConsumptionRate)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, Thirst)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, MaxThirst)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, ThirstDepletionRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, ThirstReplenishRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, WaterConsumptionRate)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, Oxygen)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, MaxOxygen)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, OxygenDepletionRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, OxygenRecoveryRate)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, FallDeathSpeed)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, FallingLegDamage)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, LimpHealthThreshold)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, KnockbackToDelatchThreshold)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, KnockbackToDecarryThreshold)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, KnockbackToCancelAttackThreshold)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, CarryCapacity)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, HungerDamage)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, ThirstDamage)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, OxygenDamage)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, GrowthPerSecond)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, GrowthPerSecondMultiplier)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, WellRestedBonusMultiplier)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, WellRestedBonusStartedGrowth)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, WellRestedBonusEndGrowth)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, Growth)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, WaterVision)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, WetnessDurationMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, BuffDurationMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, SpikeDamageMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, KnockbackMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, GroundAccelerationMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, GroundPreciseAccelerationMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, KnockbackTractionMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, SwimmingAccelerationMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaJumpCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaSprintCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaSwimCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaTrotSwimCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaFastSwimCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaDiveCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaTrotDiveCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaFastDiveCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaFlyCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, StaminaFastFlyCostMultiplier)
	POT_ATTRIBUTE_ACCESSORS_CONDITIONALREP(UCoreAttributeSet, CooldownDurationMultiplier)

	ATTRIBUTE_ACCESSORS(UCoreAttributeSet, RapidStrikesModifier)
	
	// BEGIN - META ATTRIBUTES
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, AttackDamage)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, BoneBreakChance)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, IncomingDamage)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, IncomingSurvivalDamage)
	
	// STATUS_UPDATE_MARKER
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, IncomingBoneBreakAmount)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, BoneBreakAmount)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, IncomingBleedingRate)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, BleedAmount)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, IncomingPoisonRate)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, PoisonAmount)
	
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, IncomingVenomRate)
	POT_ATTRIBUTE_ACCESSORS(UCoreAttributeSet, VenomAmount)
	// END - META ATTRIBUTES

protected:
	/**
	 * Backing Fields for Attributes
	 * Always use accessors to access these, regardless of if you're inside this class or a derived one.
	 * If you don't, the attribute won't be dirtied and it won't replicate.
	 * >:[
	 */

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_Health)
	FGameplayAttributeData Health;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_MaxHealth)
	FGameplayAttributeData MaxHealth;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_HealthRecoveryRate)
	FGameplayAttributeData HealthRecoveryRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_HealthRecoveryMultiplier)
	FGameplayAttributeData HealthRecoveryMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_Stamina)
	FGameplayAttributeData Stamina;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_MaxStamina)
	FGameplayAttributeData MaxStamina;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaRecoveryRate)
	FGameplayAttributeData StaminaRecoveryRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_StaminaRecoveryMultiplier)
	FGameplayAttributeData StaminaRecoveryMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_CombatWeight)
	FGameplayAttributeData CombatWeight;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_Armor)
	FGameplayAttributeData Armor;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_MovementSpeedMultiplier)
	FGameplayAttributeData MovementSpeedMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_TurnRadiusMultiplier)
    FGameplayAttributeData TurnRadiusMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_TurnInPlaceRadiusMultiplier)
	FGameplayAttributeData TurnInPlaceRadiusMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_SprintingSpeedMultiplier)
	FGameplayAttributeData SprintingSpeedMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_TrottingSpeedMultiplier)
	FGameplayAttributeData TrottingSpeedMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_JumpForceMultiplier)
	FGameplayAttributeData JumpForceMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_AirControlMultiplier)
	FGameplayAttributeData AirControlMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_BodyFoodAmount)
	FGameplayAttributeData BodyFoodAmount;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_CurrentBodyFoodAmount)
	FGameplayAttributeData CurrentBodyFoodAmount;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_BodyFoodCorpseThreshold)
	FGameplayAttributeData BodyFoodCorpseThreshold;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_Hunger)
	FGameplayAttributeData Hunger;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_MaxHunger)
	FGameplayAttributeData MaxHunger;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_HungerDepletionRate)
	FGameplayAttributeData HungerDepletionRate;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_FoodConsumptionRate)
	FGameplayAttributeData FoodConsumptionRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_Thirst)
	FGameplayAttributeData Thirst;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_MaxThirst)
	FGameplayAttributeData MaxThirst;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_ThirstDepletionRate)
	FGameplayAttributeData ThirstDepletionRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_ThirstReplenishRate)
	FGameplayAttributeData ThirstReplenishRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_WaterConsumptionRate)
	FGameplayAttributeData WaterConsumptionRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_Oxygen)
	FGameplayAttributeData Oxygen;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_MaxOxygen)
	FGameplayAttributeData MaxOxygen;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_OxygenDepletionRate)
	FGameplayAttributeData OxygenDepletionRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_OxygenRecoveryRate)
	FGameplayAttributeData OxygenRecoveryRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_FallDeathSpeed)
	FGameplayAttributeData FallDeathSpeed;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_FallingLegDamage)
	FGameplayAttributeData FallingLegDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_LimpHealthThreshold)
	FGameplayAttributeData LimpHealthThreshold;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_KnockbackToDelatchThreshold)
	FGameplayAttributeData KnockbackToDelatchThreshold;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_KnockbackToDecarryThreshold)
	FGameplayAttributeData KnockbackToDecarryThreshold;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_KnockbackToCancelAttackThreshold)
	FGameplayAttributeData KnockbackToCancelAttackThreshold;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_CarryCapacity)
	FGameplayAttributeData CarryCapacity;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_HungerDamage)
	FGameplayAttributeData HungerDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_ThirstDamage)
	FGameplayAttributeData ThirstDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_OxygenDamage)
	FGameplayAttributeData OxygenDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_GrowthPerSecond)
	FGameplayAttributeData GrowthPerSecond;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_GrowthPerSecondMultiplier)
	FGameplayAttributeData GrowthPerSecondMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_WellRestedBonusMultiplier)
	FGameplayAttributeData WellRestedBonusMultiplier;

	// What growth the player was at when the WellRestedBonusMultiplier started
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_WellRestedBonusStartedGrowth)
	FGameplayAttributeData WellRestedBonusStartedGrowth;

	// What growth the player will be at when the WellRestedBonusMultiplier should end
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_WellRestedBonusEndGrowth)
	FGameplayAttributeData WellRestedBonusEndGrowth;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_Growth)
	FGameplayAttributeData Growth;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_WaterVision)
	FGameplayAttributeData WaterVision;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_WetnessDurationMultiplier)
	FGameplayAttributeData WetnessDurationMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_BuffDurationMultiplier)
	FGameplayAttributeData BuffDurationMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_SpikeDamageMultiplier)
	FGameplayAttributeData SpikeDamageMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_KnockbackMultiplier)
	FGameplayAttributeData KnockbackMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_GroundAccelerationMultiplier)
	FGameplayAttributeData GroundAccelerationMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_GroundPreciseAccelerationMultiplier)
	FGameplayAttributeData GroundPreciseAccelerationMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_KnockbackTractionMultiplier)
	FGameplayAttributeData KnockbackTractionMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_SwimmingAccelerationMultiplier)
	FGameplayAttributeData SwimmingAccelerationMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaJumpCostMultiplier)
	FGameplayAttributeData StaminaJumpCostMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaSprintCostMultiplier)
	FGameplayAttributeData StaminaSprintCostMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaSwimCostMultiplier)
	FGameplayAttributeData StaminaSwimCostMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaTrotSwimCostMultiplier)
	FGameplayAttributeData StaminaTrotSwimCostMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaFastSwimCostMultiplier)
	FGameplayAttributeData StaminaFastSwimCostMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaDiveCostMultiplier)
	FGameplayAttributeData StaminaDiveCostMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaTrotDiveCostMultiplier)
	FGameplayAttributeData StaminaTrotDiveCostMultiplier;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaFastDiveCostMultiplier)
	FGameplayAttributeData StaminaFastDiveCostMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaFlyCostMultiplier)
	FGameplayAttributeData StaminaFlyCostMultiplier;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_StaminaFastFlyCostMultiplier)
	FGameplayAttributeData StaminaFastFlyCostMultiplier;

	//Multiplier amount which can be applied to GE Cooldown durations
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_CooldownDurationMultiplier)
	FGameplayAttributeData CooldownDurationMultiplier;

	// BEGIN - STATUS_UPDATE_MARKER

	// Status - BROKENBONE / LEGDAMAGE
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_LegDamage)
	FGameplayAttributeData LegDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_LegHealRate)
	FGameplayAttributeData LegHealRate;

	// The outgoing bone break amount applied if BoneBreakChance succeeds.
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData BoneBreakAmount;

	// The outgoing chance to break the other character's bones
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData BoneBreakChance;

	/** IncomingDamage is a 'temporary' attribute used by the DamageExecution to calculate final damage, which then turns into -Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData IncomingBoneBreakAmount;
	
	
	// Status - BLEEDING
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_BleedingRate)
	FGameplayAttributeData BleedingRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_BleedingHealRate)
	FGameplayAttributeData BleedingHealRate;

	// The amount of bleed to apply
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData BleedAmount;

	/** IncomingDamage is a 'temporary' attribute used by the DamageExecution to calculate final damage, which then turns into -Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData IncomingBleedingRate;
	
	// Status - POISON
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_PoisonRate)
	FGameplayAttributeData PoisonRate;
	
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_PoisonHealRate)
	FGameplayAttributeData PoisonHealRate;
	
	// The amount of poison to apply
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData PoisonAmount;
	
	/** IncomingDamage is a 'temporary' attribute used by the DamageExecution to calculate final damage, which then turns into -Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData IncomingPoisonRate;
	
	// Status - VENOM
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", SaveGame, ReplicatedUsing = OnRep_VenomRate)
	FGameplayAttributeData VenomRate;

	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_VenomHealRate)
	FGameplayAttributeData VenomHealRate;

	/** IncomingDamage is a 'temporary' attribute used by the DamageExecution to calculate final damage, which then turns into -Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData IncomingVenomRate;

	// The amount of venom to apply
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData VenomAmount;
	
	// Status - Wet (has to be refactored)
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", ReplicatedUsing = OnRep_Wetness)
	FGameplayAttributeData Wetness;
	// END - STATUS_UPDATE_MARKER

	//META ATTRIBUTES
	
	// The outgoing damage of the current attack
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData AttackDamage;
	
	/** IncomingDamage is a 'temporary' attribute used by the DamageExecution to calculate final damage, which then turns into -Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData IncomingDamage;

	/** IncomingSurvivalDamage is a 'temporary' attribute used by the DamageExecution to calculate final damage, which then turns into -Health */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData IncomingSurvivalDamage;
	
	/** RapidStrikesModifier is used by a custom modifier magnitude calculation class to affect the damage output of an attack when applying damage */
	UPROPERTY(BlueprintReadOnly, Category = "Attributes|Core", meta = (HideFromLevelInfos))
	FGameplayAttributeData RapidStrikesModifier;
public:
	UCoreAttributeSet();

	// STATUS_UPDATE_MARKER - Add a NameDebuff for the new status
	inline static const TArray<FName> DebuffTagsToRemove{
		NAME_DebuffBrokenBone,
		NAME_DebuffBleeding,
		NAME_DebuffPoisoned,
		NAME_DebuffEnvenomed,
		NAME_DebuffLatched
	};

	TMap<const FGameplayAttribute, FPOTAdjustForMaxAttribute> AdjustForMaxAttributes;
	TMap<const FGameplayAttribute, FPOTAdjustCurrentAttribute> AdjustForCurrentAttributes;
	TMap<const FGameplayAttribute, FPOTIncomingStatusHandling> CallForIncomingStatusHandling;
	TMap<const FGameplayAttribute, FPOTStatusHandling> CallForStatusHandling;

	static bool CanBeDamaged(const AIBaseCharacter* TargetCharacter);
	
	void HandleIncomingStatusAttributeChange(
		const float DeltaValue,
		const FPOTIncomingStatusHandling& StatusHandlingInformation,
		AIBaseCharacter* TargetCharacter,
		const FGameplayEffectModCallbackData& Data,
		const FHitResult& HitResult,
		const FGameplayTagContainer& SourceTags
		);

	UFUNCTION(BlueprintCallable)
	static float FindAttributeCapFromConfig(const FGameplayAttribute& Attribute);

	/** Helper function to proportionally adjust the value of an attribute when it's associated max attribute changes. (i.e. When MaxHealth increases, Health increases by an amount that maintains the same percentage as before) */
	void AdjustAttributeForMaxChange(const FGameplayAttribute& MaxAttributeProperty, const float NewMaxValue, const FGameplayAttribute& AffectedAttributeProperty);

	virtual void UpdateTagBasedOnAttribute(const FGameplayAttribute& Attribute, const float NewValue, const FName& TagName);

	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	static void GetNewAttributeVsCapValue(const FPOTAdjustCurrentAttribute& CurrentAdjust, const FGameplayAttribute& Attribute, float& NewValue);
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual bool PreGameplayEffectExecute(struct FGameplayEffectModCallbackData& Data) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

	virtual bool ShouldInitProperty(bool FirstInit, FProperty* PropertyToInit) const override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void PreReplicate();
	virtual void PostInitProperties() override;
	void SetConditionalAttributeReplication(const FString& AttributeName, bool bEnabled);

	bool bHasDirtyConditionalProperty = true;
	bool bHasDoneInitialReplication = false;

protected:
	UPROPERTY(ReplicatedUsing = OnRep_AttributeCapsConfig)
	TArray<FAttributeCapData> AttributeCapsConfig;
	
	inline static TMap<FName, float> AttributeCaps{};

	UFUNCTION()
	virtual void OnRep_AttributeCapsConfig();

	// BEGIN - STATUS_UPDATE_MARKER
	
	UFUNCTION()
	virtual void OnRep_BleedingRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_BleedingHealRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_BleedingHealRate(bool bActive);
	
	UFUNCTION()
	virtual void OnRep_VenomRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_VenomHealRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_VenomHealRate(bool bActive);

	UFUNCTION()
	virtual void OnRep_PoisonRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_PoisonHealRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_PoisonHealRate(bool bActive);
	
	UFUNCTION()
	virtual void OnRep_LegDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_LegHealRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_LegHealRate(bool bActive);

	UFUNCTION()
	virtual void OnRep_Wetness(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_Wetness(bool bActive);
	// END - STATUS_UPDATE_MARKER
	
	UFUNCTION()
	virtual void OnRep_Health(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_Stamina(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_CurrentBodyFoodAmount(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_Hunger(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_Thirst(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_Oxygen(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_WellRestedBonusMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_WellRestedBonusStartedGrowth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_WellRestedBonusEndGrowth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_Growth(const FGameplayAttributeData& OldValue);

	UFUNCTION()
	virtual void OnRep_MaxHealth(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_MaxHealth(bool bActive);
	UFUNCTION()
	virtual void OnRep_HealthRecoveryRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_HealthRecoveryRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_MaxStamina(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_MaxStamina(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaRecoveryRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaRecoveryRate(bool bActive);
	UFUNCTION()
	void CondRep_StaminaRecoveryMultiplier(bool bActive);
	UFUNCTION()
	void CondRep_HealthRecoveryMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_CombatWeight(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_CombatWeight(bool bActive);
	UFUNCTION()
	virtual void OnRep_Armor(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_Armor(bool bActive);
	UFUNCTION()
	virtual void OnRep_MovementSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_MovementSpeedMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_TurnRadiusMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_TurnRadiusMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_TurnInPlaceRadiusMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_TurnInPlaceRadiusMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_SprintingSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_SprintingSpeedMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_TrottingSpeedMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_TrottingSpeedMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_JumpForceMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_JumpForceMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_AirControlMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_AirControlMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_BodyFoodAmount(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_BodyFoodAmount(bool bActive);
	UFUNCTION()
	virtual void OnRep_BodyFoodCorpseThreshold(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_BodyFoodCorpseThreshold(bool bActive);
	UFUNCTION()
	virtual void OnRep_MaxHunger(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_MaxHunger(bool bActive);
	UFUNCTION()
	virtual void OnRep_HungerDepletionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_HungerDepletionRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_FoodConsumptionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_FoodConsumptionRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_MaxThirst(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_MaxThirst(bool bActive);
	UFUNCTION()
	virtual void OnRep_ThirstDepletionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_ThirstDepletionRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_ThirstReplenishRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_ThirstReplenishRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_WaterConsumptionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_WaterConsumptionRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_MaxOxygen(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_MaxOxygen(bool bActive);
	UFUNCTION()
	virtual void OnRep_OxygenDepletionRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_OxygenDepletionRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_OxygenRecoveryRate(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_OxygenRecoveryRate(bool bActive);
	UFUNCTION()
	virtual void OnRep_FallDeathSpeed(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_FallDeathSpeed(bool bActive);
	UFUNCTION()
	virtual void OnRep_FallingLegDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_FallingLegDamage(bool bActive);
	UFUNCTION()
	virtual void OnRep_LimpHealthThreshold(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_LimpHealthThreshold(bool bActive);
	UFUNCTION()
	virtual void OnRep_KnockbackToDelatchThreshold(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_KnockbackToDelatchThreshold(bool bActive);
	UFUNCTION()
	virtual void OnRep_KnockbackToDecarryThreshold(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_KnockbackToDecarryThreshold(bool bActive);
	UFUNCTION()
	virtual void OnRep_KnockbackToCancelAttackThreshold(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_KnockbackToCancelAttackThreshold(bool bActive);
	UFUNCTION()
	virtual void OnRep_CarryCapacity(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_CarryCapacity(bool bActive);
	UFUNCTION()
	virtual void OnRep_HungerDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_HungerDamage(bool bActive);
	UFUNCTION()
	virtual void OnRep_ThirstDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_ThirstDamage(bool bActive);
	UFUNCTION()
	virtual void OnRep_OxygenDamage(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_OxygenDamage(bool bActive);
	
	UFUNCTION()
	virtual void OnRep_GrowthPerSecond(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_GrowthPerSecond(bool bActive);
	UFUNCTION()
	virtual void OnRep_GrowthPerSecondMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_GrowthPerSecondMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_WaterVision(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_WaterVision(bool bActive);
	UFUNCTION()
	virtual void OnRep_WetnessDurationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_WetnessDurationMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_BuffDurationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_BuffDurationMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_SpikeDamageMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_SpikeDamageMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_KnockbackMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_KnockbackMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_GroundAccelerationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_GroundAccelerationMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_GroundPreciseAccelerationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_GroundPreciseAccelerationMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_KnockbackTractionMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_KnockbackTractionMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_SwimmingAccelerationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_SwimmingAccelerationMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaJumpCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaJumpCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaSprintCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaSprintCostMultiplier(bool bActive);	
	UFUNCTION()
	virtual void OnRep_StaminaSwimCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaSwimCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaTrotSwimCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaTrotSwimCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaFastSwimCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaFastSwimCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaDiveCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaDiveCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaTrotDiveCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaTrotDiveCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaFastDiveCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaFastDiveCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaFlyCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaFlyCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaFastFlyCostMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_StaminaFastFlyCostMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_CooldownDurationMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	void CondRep_CooldownDurationMultiplier(bool bActive);
	UFUNCTION()
	virtual void OnRep_StaminaRecoveryMultiplier(const FGameplayAttributeData& OldValue);
	UFUNCTION()
	virtual void OnRep_HealthRecoveryMultiplier(const FGameplayAttributeData& OldValue);

private:
	float PreOverrideValue = 0.0f;
};
