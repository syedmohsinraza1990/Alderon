// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.
#include "Abilities/CoreAttributeSet.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectExtension.h"
#include "Abilities/POTAbilityTypes.h"
#include "Abilities/POTAbilitySystemGlobals.h"
#include "AbilitySystemComponent.h"
#include "Player/IBaseCharacter.h"
#include "AttributeSet.h"
#include "IGameInstance.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Net/Core/PropertyConditions/RepChangedPropertyTracker.h"
#include "Online/IGameSession.h"

//#if UE_SERVER
//	static_assert(WITH_PUSH_MODEL == 1);
//#endif

UCoreAttributeSet::UCoreAttributeSet()
	: Health(1.f)
	, MaxHealth(1.f)
	, HealthRecoveryMultiplier(1.f)
	, StaminaRecoveryMultiplier(1.f)
	, MovementSpeedMultiplier(1)
	, TurnRadiusMultiplier(1)
	, TurnInPlaceRadiusMultiplier(1)
	, SprintingSpeedMultiplier(1)
	, TrottingSpeedMultiplier(1)
	, JumpForceMultiplier(1)
	, AirControlMultiplier(1)
	, BodyFoodAmount(1)
	, CurrentBodyFoodAmount(1)
	, FallDeathSpeed(4000.f)
	, KnockbackToDelatchThreshold(100.f)
	, KnockbackToDecarryThreshold(100.f)
	, KnockbackToCancelAttackThreshold(100.f)
	, CarryCapacity(500.f)
	, GrowthPerSecondMultiplier(0.f)
	, WellRestedBonusMultiplier(2.f)
	, WellRestedBonusStartedGrowth(0.f)
	, WellRestedBonusEndGrowth(0.f)
	, Growth(MIN_GROWTH)
	, WaterVision(1)
	, WetnessDurationMultiplier(1.f)
	, BuffDurationMultiplier(1.0f)
	, SpikeDamageMultiplier(1.0f)
	, KnockbackMultiplier(1.0f)
	, GroundAccelerationMultiplier(1.f)
	, GroundPreciseAccelerationMultiplier(1.0f)
	, KnockbackTractionMultiplier(1.0f)
	, SwimmingAccelerationMultiplier(1.0f)
	, StaminaJumpCostMultiplier(1.0f)
	, StaminaSprintCostMultiplier(1.0f)
	, StaminaSwimCostMultiplier(1.0f)
	, StaminaTrotSwimCostMultiplier(1.0f)
	, StaminaFastSwimCostMultiplier(1.0f)
	, StaminaDiveCostMultiplier(1.0f)
	, StaminaTrotDiveCostMultiplier(1.0f)
	, StaminaFastDiveCostMultiplier(1.0f)
	, StaminaFlyCostMultiplier(1.0f)
	, StaminaFastFlyCostMultiplier(1.0f)
	, CooldownDurationMultiplier(1.0f)
{
	InitWetness(0.f);
	
#if WITH_SERVER_CODE

	// Called when the MaxAttribute is getting changed. Will adjust the Affected Attribute
	AdjustForMaxAttributes = {
		{ GetMaxHealthAttribute(), FPOTAdjustForMaxAttribute(GetHealthAttribute()) },
		{ GetMaxHungerAttribute(), FPOTAdjustForMaxAttribute(GetHungerAttribute()) },
		{ GetMaxThirstAttribute(), FPOTAdjustForMaxAttribute(GetThirstAttribute()) },
		{ GetMaxOxygenAttribute(), FPOTAdjustForMaxAttribute(GetOxygenAttribute()) },
		{ GetMaxStaminaAttribute(), FPOTAdjustForMaxAttribute(GetStaminaAttribute()) },
		{ GetBodyFoodAmountAttribute(), FPOTAdjustForMaxAttribute(GetCurrentBodyFoodAmountAttribute()) }
	};

	// STATUS_UPDATE_MARKER - Add any new configuration in those table when adding a new Status

	// Called whenever a "Current" Attribute gets changed. Can passe an optional tag to be applied when it will change
	// Will use the MaxAttribute for clamping
	AdjustForCurrentAttributes = {
		{ GetHealthAttribute(), FPOTAdjustCurrentAttribute(&MaxHealth, &NAME_DebuffDead) },
		{ GetStaminaAttribute(), FPOTAdjustCurrentAttribute(&MaxStamina) },
		{ GetCurrentBodyFoodAmountAttribute(), FPOTAdjustCurrentAttribute(&BodyFoodAmount) },
		{ GetHungerAttribute(), FPOTAdjustCurrentAttribute(&MaxHunger) },
		{ GetThirstAttribute(), FPOTAdjustCurrentAttribute(&MaxThirst) },
		{ GetOxygenAttribute(), FPOTAdjustCurrentAttribute(&MaxOxygen) },
		{ GetMovementSpeedMultiplierAttribute(), FPOTAdjustCurrentAttribute(0.1f) },
		{ GetWetnessAttribute(), FPOTAdjustCurrentAttribute(0.f) },
		{ GetGrowthAttribute(), FPOTAdjustCurrentAttribute(0.f, 1.f) },
		// For Statuses: don't have a Max Attribute, so we pass the Maximum and Minimum directly
		{ GetLegDamageAttribute(), FPOTAdjustCurrentAttribute(nullptr, &NAME_DebuffBrokenBone, 0.f, true) },
		{ GetBleedingRateAttribute(), FPOTAdjustCurrentAttribute(nullptr, &NAME_DebuffBleeding, 0.f, true) },
		{ GetPoisonRateAttribute(), FPOTAdjustCurrentAttribute(nullptr, &NAME_DebuffPoisoned, 0.f, true) },
		{ GetVenomRateAttribute(),  FPOTAdjustCurrentAttribute(nullptr, &NAME_DebuffEnvenomed, 0.f, true)},
	};

	// Status Handling: Will call all relevant methods assiociated with the status.
	CallForStatusHandling = {
		{ GetBleedingRateAttribute(), FPOTStatusHandling(EDamageEffectType::BLEED, true, true) },
		{ GetVenomRateAttribute(), FPOTStatusHandling(EDamageEffectType::VENOM) },
		{ GetPoisonRateAttribute(), FPOTStatusHandling(EDamageEffectType::POISONED) }
	};

	// Handling of Incoming<Attribute>Rate changes.
	// Will call all relevant method, and will look into the CallForStatusHandling
	CallForIncomingStatusHandling = {
		{ GetIncomingBleedingRateAttribute(), FPOTIncomingStatusHandling(EDamageEffectType::BLEED, GetIncomingBleedingRateAttribute(), GetBleedingRateAttribute(), &UCoreAttributeSet::SetIncomingBleedingRate,  &UCoreAttributeSet::SetBleedingRate) },
		{ GetIncomingPoisonRateAttribute(), FPOTIncomingStatusHandling(EDamageEffectType::POISONED, GetIncomingPoisonRateAttribute(), GetPoisonRateAttribute(), &UCoreAttributeSet::SetIncomingPoisonRate, &UCoreAttributeSet::SetPoisonRate) },
		{ GetIncomingVenomRateAttribute(), FPOTIncomingStatusHandling(EDamageEffectType::VENOM, GetIncomingVenomRateAttribute(), GetVenomRateAttribute(),&UCoreAttributeSet::SetIncomingVenomRate, &UCoreAttributeSet::SetVenomRate) }
	};	
	
#endif
}

// BEGIN - STATUS_UPDATE_MARKER - Macro Attribute Accessors
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, Wetness)

POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, LegDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, LegHealRate)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, BoneBreakAmount)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, IncomingBoneBreakAmount)

POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, BleedingRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, BleedingHealRate)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, BleedAmount)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, IncomingBleedingRate)

POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, PoisonRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, PoisonHealRate)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, PoisonAmount)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, IncomingPoisonRate)

POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, VenomRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, VenomHealRate)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, VenomAmount)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, IncomingVenomRate)
// END - STATUS_UPDATE_MARKER - Macro Attribute Accessors

POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, Health)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, CurrentBodyFoodAmount)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, Growth)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, WellRestedBonusMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, WellRestedBonusStartedGrowth)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, WellRestedBonusEndGrowth)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, Hunger)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, Thirst)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, Oxygen)
POT_ATTRIBUTE_ACCESSORS_BODY(UCoreAttributeSet, Stamina)

POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, MaxHealth)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, HealthRecoveryRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, HealthRecoveryMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, CooldownDurationMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaFastFlyCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaFlyCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaFastDiveCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaTrotDiveCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaDiveCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaFastSwimCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaTrotSwimCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaSwimCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaSprintCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaJumpCostMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, SwimmingAccelerationMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, KnockbackTractionMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, GroundPreciseAccelerationMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, GroundAccelerationMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, KnockbackMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, SpikeDamageMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, BuffDurationMultiplier)

POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, WetnessDurationMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, WaterVision)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, GrowthPerSecondMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, GrowthPerSecond)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, OxygenDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, ThirstDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, HungerDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, KnockbackToCancelAttackThreshold)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, KnockbackToDecarryThreshold)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, KnockbackToDelatchThreshold)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, CarryCapacity)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, LimpHealthThreshold)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, FallingLegDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, FallDeathSpeed)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, OxygenRecoveryRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, OxygenDepletionRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, MaxOxygen)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, WaterConsumptionRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, ThirstReplenishRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, ThirstDepletionRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, MaxThirst)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, FoodConsumptionRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, HungerDepletionRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, MaxHunger)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, BodyFoodCorpseThreshold)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, BodyFoodAmount)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, AirControlMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, JumpForceMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, TrottingSpeedMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, SprintingSpeedMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, TurnInPlaceRadiusMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, TurnRadiusMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, MovementSpeedMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, Armor)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, CombatWeight)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaRecoveryRate)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, StaminaRecoveryMultiplier)
POT_ATTRIBUTE_ACCESSORS_BODY_CONDITONAL(UCoreAttributeSet, MaxStamina)

POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, AttackDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, BoneBreakChance)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, IncomingDamage)
POT_ATTRIBUTE_ACCESSORS_BODY_NO_REP(UCoreAttributeSet, IncomingSurvivalDamage)


void UCoreAttributeSet::AdjustAttributeForMaxChange(const FGameplayAttribute& MaxAttributeProperty, const float NewMaxValue,
	const FGameplayAttribute& AffectedAttributeProperty)
{
	UAbilitySystemComponent* AbilityComp = GetOwningAbilitySystemComponent();

	if (!ensureAlways(AbilityComp) ||
		!ensureAlways(AbilityComp->GetOwner()) ||
		!AbilityComp->GetOwner()->HasAuthority())
	{
		// Only scale attributes if we are authority. -Poncho
		return;
	}

	const float CurrentMaxValue = MaxAttributeProperty.GetNumericValue(this);
	if (!FMath::IsNearlyEqual(CurrentMaxValue, NewMaxValue) && AbilityComp)
	{
		// Change current value to maintain the current Val / Max percent
		const float CurrentValue = AffectedAttributeProperty.GetGameplayAttributeData(this)->GetCurrentValue();
		const float NewDelta = (CurrentMaxValue > 0.f)
			? (CurrentValue * NewMaxValue / CurrentMaxValue) - CurrentValue 
			: NewMaxValue;

		//This is a little hacky but should serve its purpose to prevent the clamping 
		MaxAttributeProperty.GetGameplayAttributeData(this)->SetCurrentValue(NewMaxValue);
		AbilityComp->ApplyModToAttributeUnsafe(AffectedAttributeProperty, EGameplayModOp::Additive, NewDelta);
	}
}

bool UCoreAttributeSet::CanBeDamaged(const AIBaseCharacter* TargetCharacter)
{
	return TargetCharacter && !(TargetCharacter->GetGodmode() || TargetCharacter->IsHomecaveBuffActive());
}

void UCoreAttributeSet::HandleIncomingStatusAttributeChange(const float DeltaValue, const FPOTIncomingStatusHandling& StatusHandlingInformation, AIBaseCharacter* TargetCharacter, const FGameplayEffectModCallbackData& Data, const FHitResult& HitResult, const FGameplayTagContainer& SourceTags)
{
	if (TargetCharacter && DeltaValue != 0.f)
	{
		const float LocalRate = CanBeDamaged(TargetCharacter) ? StatusHandlingInformation.IncomingAttribute.GetNumericValue(this) : 0.f;
		if (StatusHandlingInformation.IncomingRateSetterFunc)
		{
			(this->*StatusHandlingInformation.IncomingRateSetterFunc)(0.f);
		}

		const float CurrentRate = StatusHandlingInformation.CurrentAttribute.GetNumericValue(this) + LocalRate;
		if (StatusHandlingInformation.RateSetterFunc)
		{
			(this->*StatusHandlingInformation.RateSetterFunc)(CurrentRate);
		}

		if (const FPOTStatusHandling* CallForHandling = CallForStatusHandling.Find(StatusHandlingInformation.CurrentAttribute))
		{
			const FVector HitRelativeLocation = HitResult.Location - TargetCharacter->GetActorLocation();
			if (!CallForHandling->bApplyParticleOnlyOnce || CurrentRate <= 0)
			{
				TargetCharacter->ReplicateParticle(FDamageParticleInfo(StatusHandlingInformation.DamageEffectType, HitRelativeLocation, TargetCharacter->GetActorRotation()));
			}
			
			TargetCharacter->HandleStatusRateChange(StatusHandlingInformation.CurrentAttribute,
				*CallForHandling,
				DeltaValue,
				Data.EffectSpec,
				SourceTags,
				false,
				HitResult);
		}
	}
}

float UCoreAttributeSet::FindAttributeCapFromConfig(const FGameplayAttribute& Attribute)
{
	return AttributeCaps.FindRef(FName(Attribute.GetName()));
}

void UCoreAttributeSet::OnRep_AttributeCapsConfig()
{
	if (!AttributeCaps.IsEmpty())
	{
		return;
	}
	
	for (const FAttributeCapData& CapsConfig : AttributeCapsConfig)
	{
		AttributeCaps.Emplace(CapsConfig.AttributeName, CapsConfig.MaximumValue);
	}
}

void UCoreAttributeSet::OnRep_Health(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Health, OldValue);
}

void UCoreAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, MaxHealth, OldValue);
}

void UCoreAttributeSet::OnRep_HealthRecoveryRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, HealthRecoveryRate, OldValue);
}

void UCoreAttributeSet::OnRep_Stamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Stamina, OldValue);
}

// BEGIN STATUS_UPDATE_MARKER - OnRep Notify Body
// Status - Venom
void UCoreAttributeSet::OnRep_VenomRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, VenomRate, OldValue);
}
void UCoreAttributeSet::OnRep_VenomHealRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, VenomHealRate, OldValue);
}

// Status - Poison
void UCoreAttributeSet::OnRep_PoisonRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, PoisonRate, OldValue);
}

void UCoreAttributeSet::OnRep_PoisonHealRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, PoisonHealRate, OldValue);
}

// Status - BrokenBone / Leg Damage
void UCoreAttributeSet::OnRep_LegDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, LegDamage, OldValue);
}

void UCoreAttributeSet::OnRep_LegHealRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, LegHealRate, OldValue);
}

// Status - Bleeding
void UCoreAttributeSet::OnRep_BleedingRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, BleedingRate, OldValue);
}

void UCoreAttributeSet::OnRep_BleedingHealRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, BleedingHealRate, OldValue);
}
// END STATUS_UPDATE_MARKER - OnRep Notify Body

void UCoreAttributeSet::OnRep_CurrentBodyFoodAmount(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, CurrentBodyFoodAmount, OldValue);
}

void UCoreAttributeSet::OnRep_Hunger(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Hunger, OldValue);
}

void UCoreAttributeSet::OnRep_Thirst(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Thirst, OldValue);
}

void UCoreAttributeSet::OnRep_Oxygen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Oxygen, OldValue);
}


void UCoreAttributeSet::OnRep_WellRestedBonusMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, WellRestedBonusMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_WellRestedBonusStartedGrowth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, WellRestedBonusStartedGrowth, OldValue);
}

void UCoreAttributeSet::OnRep_WellRestedBonusEndGrowth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, WellRestedBonusEndGrowth, OldValue);
}

void UCoreAttributeSet::OnRep_Growth(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Growth, OldValue);
}

void UCoreAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, MaxStamina, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaRecoveryRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaRecoveryRate, OldValue);
}

void UCoreAttributeSet::OnRep_CombatWeight(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, CombatWeight, OldValue);
}

void UCoreAttributeSet::OnRep_Armor(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Armor, OldValue);
}

void UCoreAttributeSet::OnRep_MovementSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, MovementSpeedMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_TurnRadiusMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, TurnRadiusMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_TurnInPlaceRadiusMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, TurnInPlaceRadiusMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_SprintingSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, SprintingSpeedMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_TrottingSpeedMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, TrottingSpeedMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_JumpForceMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, JumpForceMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_AirControlMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, AirControlMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_BodyFoodAmount(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, BodyFoodAmount, OldValue);
}

void UCoreAttributeSet::OnRep_BodyFoodCorpseThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, BodyFoodCorpseThreshold, OldValue);
}

void UCoreAttributeSet::OnRep_MaxHunger(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, MaxHunger, OldValue);
}

void UCoreAttributeSet::OnRep_HungerDepletionRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, HungerDepletionRate, OldValue);
}

void UCoreAttributeSet::OnRep_FoodConsumptionRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, FoodConsumptionRate, OldValue);
}

void UCoreAttributeSet::OnRep_MaxThirst(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, MaxThirst, OldValue);
}

void UCoreAttributeSet::OnRep_ThirstDepletionRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, ThirstDepletionRate, OldValue);
}

void UCoreAttributeSet::OnRep_ThirstReplenishRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, ThirstReplenishRate, OldValue);
}

void UCoreAttributeSet::OnRep_WaterConsumptionRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, WaterConsumptionRate, OldValue);
}

void UCoreAttributeSet::OnRep_MaxOxygen(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, MaxOxygen, OldValue);
}

void UCoreAttributeSet::OnRep_OxygenDepletionRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, OxygenDepletionRate, OldValue);
}

void UCoreAttributeSet::OnRep_OxygenRecoveryRate(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, OxygenRecoveryRate, OldValue);
}

void UCoreAttributeSet::OnRep_FallDeathSpeed(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, FallDeathSpeed, OldValue);
}

void UCoreAttributeSet::OnRep_FallingLegDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, FallingLegDamage, OldValue);
}

void UCoreAttributeSet::OnRep_LimpHealthThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, LimpHealthThreshold, OldValue);
}

void UCoreAttributeSet::OnRep_KnockbackToDelatchThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, KnockbackToDelatchThreshold, OldValue);
}

void UCoreAttributeSet::OnRep_KnockbackToDecarryThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, KnockbackToDecarryThreshold, OldValue);
}

void UCoreAttributeSet::OnRep_KnockbackToCancelAttackThreshold(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, KnockbackToCancelAttackThreshold, OldValue);
}

void UCoreAttributeSet::OnRep_CarryCapacity(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, CarryCapacity, OldValue);
}

void UCoreAttributeSet::OnRep_HungerDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, HungerDamage, OldValue);
}

void UCoreAttributeSet::OnRep_ThirstDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, ThirstDamage, OldValue);
}

void UCoreAttributeSet::OnRep_OxygenDamage(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, OxygenDamage, OldValue);
}

void UCoreAttributeSet::OnRep_GrowthPerSecond(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, GrowthPerSecond, OldValue);
}

void UCoreAttributeSet::OnRep_GrowthPerSecondMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, GrowthPerSecondMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_WaterVision(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, WaterVision, OldValue);
}

void UCoreAttributeSet::OnRep_WetnessDurationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, WetnessDurationMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_Wetness(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, Wetness, OldValue);
}

void UCoreAttributeSet::OnRep_BuffDurationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, BuffDurationMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_SpikeDamageMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, SpikeDamageMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_KnockbackMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, KnockbackMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_GroundAccelerationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, GroundAccelerationMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_GroundPreciseAccelerationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, GroundPreciseAccelerationMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_KnockbackTractionMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, KnockbackTractionMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_SwimmingAccelerationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, SwimmingAccelerationMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaJumpCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaJumpCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaSprintCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaSprintCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaSwimCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaSwimCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaTrotSwimCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaTrotSwimCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaFastSwimCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaFastSwimCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaDiveCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaDiveCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaTrotDiveCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaTrotDiveCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaFastDiveCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaFastDiveCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaFlyCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaFlyCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaFastFlyCostMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaFastFlyCostMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_CooldownDurationMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, CooldownDurationMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_StaminaRecoveryMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, StaminaRecoveryMultiplier, OldValue);
}

void UCoreAttributeSet::OnRep_HealthRecoveryMultiplier(const FGameplayAttributeData& OldValue)
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UCoreAttributeSet, HealthRecoveryMultiplier, OldValue);
}

void UCoreAttributeSet::UpdateTagBasedOnAttribute(const FGameplayAttribute& Attribute, const float NewValue, const FName& TagName)
{
	if (AActor* OwningActor = GetOwningActor())
	{
		if (AIBaseCharacter* BaseOwningCharacter = Cast<AIBaseCharacter>(OwningActor))
		{
			FGameplayTagContainer GameplayTags = BaseOwningCharacter->CharacterTags;
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TagName);

			if (!Tag.IsValid())
			{
				UE_LOG(TitansLog, Error, TEXT("UCoreAttributeSet::UpdateTagBasedOnAttribute - No Tag found for %s, abort."), *TagName.ToString());
				return;
					}

			if (Attribute == GetHealthAttribute())
			{
				// Debuff.Dead
				if (NewValue <= 0 && !GameplayTags.HasTag(Tag))
				{
					GameplayTags.AddTagFast(Tag);
					
					//Also remove all debuff status tags since we have died
					//@TODO Might want to remove buff as well
					
					for (const FName& Name : DebuffTagsToRemove)
					{
						FGameplayTag TagToRemove = FGameplayTag::RequestGameplayTag(Name);
						if (TagToRemove.IsValid() && GameplayTags.HasTag(TagToRemove))
						{
							GameplayTags.RemoveTag(TagToRemove);
						}
					}
				}
				else if (NewValue > 0 && GameplayTags.HasTag(Tag))
				{
					GameplayTags.RemoveTag(Tag);
				}
			}
			else if (Attribute.IsValid() && Tag.IsValid())
			{
				// These all apply a tag when their attribute is > 0 and remove when <= 0, so we can clump them together
				if (NewValue > 0 && !GameplayTags.HasTag(Tag))
				{
					if (BaseOwningCharacter->GetHealth() > 0)
					{
						GameplayTags.AddTagFast(Tag);
					}
				}
				else if (NewValue <= 0 && GameplayTags.HasTag(Tag))
				{
					GameplayTags.RemoveTag(Tag);
				}
			}

			BaseOwningCharacter->CharacterTags = GameplayTags;
		}
	}
}

void UCoreAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	// This is called whenever attributes change, so for max health/mana we want to scale the current totals to match
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetGrowthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, 1.f);
		if (UPOTAbilitySystemComponent* PotAsc = Cast<UPOTAbilitySystemComponent>(GetOwningAbilitySystemComponent()))
		{
			PotAsc->ReevaluateEffectsBasedOnAttribute(Attribute, NewValue * 5.f); // Growth curve tables are based on 0 - 5, but the growth attribute is 0 - 1
		}
		return;
	}

	if (const FPOTAdjustForMaxAttribute* MaxAdjust = AdjustForMaxAttributes.Find(Attribute))
	{
		AdjustAttributeForMaxChange(Attribute, NewValue, MaxAdjust->AffectedAttributeProperty);
		return;
	}

	if (const FPOTAdjustCurrentAttribute* CurrentAdjust = AdjustForCurrentAttributes.Find(Attribute))
	{
		GetNewAttributeVsCapValue(*CurrentAdjust, Attribute, NewValue);

		if (CurrentAdjust->DebuffName)
		{
			UpdateTagBasedOnAttribute(Attribute, NewValue, *CurrentAdjust->DebuffName);
		}
	}
}

void UCoreAttributeSet::GetNewAttributeVsCapValue(const FPOTAdjustCurrentAttribute& CurrentAdjust, const FGameplayAttribute& Attribute, float& NewValue)
{
	switch (CurrentAdjust.AdjustMethod)
	{
		case EPOTAdjustMethod::AM_Clamp:
		{
			float Max = 0.f;
			if (CurrentAdjust.MaxAttribute)
			{
				Max = CurrentAdjust.MaxAttribute->GetCurrentValue();
			} else if (CurrentAdjust.FetchMaxFromConfig)
			{
				Max = FindAttributeCapFromConfig(Attribute);
			} else
			{
				Max = CurrentAdjust.MaxValue;
			}
			
			NewValue = FMath::Clamp(NewValue, 0.f, Max);
		}

		case EPOTAdjustMethod::AM_Max:
		{
			NewValue = FMath::Max(NewValue, CurrentAdjust.MinValue);
		}
		
		case EPOTAdjustMethod::AM_None:
		default:
		{
			break;
		}
	}
}

void UCoreAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);

	//This was originally BaseValue way back when but there was a bug for which the solution was to change
	//this to use GetCurrentValue instead. It's causing issues with the body food amount so that one will
	//be reverted to GetBaseValue

	if (const FPOTAdjustCurrentAttribute* CurrentAdjust = AdjustForCurrentAttributes.Find(Attribute))
	{
		GetNewAttributeVsCapValue(*CurrentAdjust, Attribute, NewValue);
	}
}


bool UCoreAttributeSet::PreGameplayEffectExecute(FGameplayEffectModCallbackData &Data)
{
	if (Data.EvaluatedData.ModifierOp == EGameplayModOp::Type::Override)
	{
		PreOverrideValue = Data.EvaluatedData.Attribute.GetNumericValue(this);
	}

	return Super::PreGameplayEffectExecute(Data);
}

void UCoreAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FGameplayEffectContextHandle Context = Data.EffectSpec.GetContext();
	UAbilitySystemComponent* SourceASC = Context.GetOriginalInstigatorAbilitySystemComponent();
	const FGameplayTagContainer& SourceTags = *Data.EffectSpec.CapturedSourceTags.GetAggregatedTags();

	FGameplayTagContainer AssetTags;
	Data.EffectSpec.GetAllAssetTags(AssetTags);

	FPOTGameplayEffectContext* POTEffectContext = Context.Get() != nullptr
		? StaticCast<FPOTGameplayEffectContext*>(Context.Get())
		: nullptr;
	const bool POTEffectContextIsAssigned = POTEffectContext != nullptr;

	// Compute the delta between old and new, if it is available
	float DeltaValue = 0;

	if (Data.EvaluatedData.ModifierOp == EGameplayModOp::Type::Additive)
	{
		// If this was additive, store the raw delta value to be passed along later
		DeltaValue = Data.EvaluatedData.Magnitude;
	}
	else if (Data.EvaluatedData.ModifierOp == EGameplayModOp::Type::Override)
	{
		DeltaValue = Data.EvaluatedData.Magnitude - PreOverrideValue;
	}

	// Get the Target actor, which should be our owner
	AActor* TargetActor = nullptr;
	AIBaseCharacter* TargetCharacter = nullptr;

	const FGameplayTagContainer& TargetTags = *Data.EffectSpec.CapturedTargetTags.GetAggregatedTags();
	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		TargetActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		TargetCharacter = Cast<AIBaseCharacter>(TargetActor);
	}

	const bool bTargetCharacterIsAssigned = TargetCharacter != nullptr;

	if (!TargetActor)
	{
		return;
	}

	// Try to extract a hit result
	FHitResult HitResult;
	if (Context.GetHitResult() != nullptr)
	{
		HitResult = *Context.GetHitResult();
	}

	// Get the Source actor
	AActor* SourceActor = nullptr;
	AController* SourceController = nullptr;
	AIBaseCharacter* Instigator = nullptr;

	if (SourceASC && SourceASC->AbilityActorInfo.IsValid() && SourceASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		SourceActor = SourceASC->AbilityActorInfo->AvatarActor.Get();
		SourceController = SourceASC->AbilityActorInfo->PlayerController.Get();
		if (!SourceController && SourceActor)
		{
			if (APawn* Pawn = Cast<APawn>(SourceActor))
			{
				SourceController = Pawn->GetController();
			}
		}

		// Use the controller to find the source pawn
		if (SourceController)
		{
			Instigator = Cast<AIBaseCharacter>(SourceController->GetPawn());
		}
		else
		{
			Instigator = Cast<AIBaseCharacter>(SourceActor);
		}

		// Set the causer actor based on context if it's set
		if (Context.GetEffectCauser())
		{
			SourceActor = Context.GetEffectCauser();
		}
	}

	const UPOTAbilitySystemGlobals& WASG = UPOTAbilitySystemGlobals::Get();

	const bool bHomecaveBuff = bTargetCharacterIsAssigned && TargetCharacter->IsHomecaveBuffActive();
	const bool bGodmode = bTargetCharacterIsAssigned && TargetCharacter->GetGodmode();
	const bool bCanBeDamaged = bGodmode || bHomecaveBuff;
	const bool bImmuneToKnockBack = bCanBeDamaged || TargetTags.HasTag(WASG.KnockbackImmunityTag);
	
	if (TargetActor->HasAuthority())
	{
		// ----- STATUS CURRENT VALUES HANDLING -----
		if (const FPOTStatusHandling* const StatusHandlingInformation = CallForStatusHandling.Find(Data.EvaluatedData.Attribute))
		{
			TargetCharacter->HandleStatusRateChange(Data.EvaluatedData.Attribute,
				*StatusHandlingInformation,
				DeltaValue,
				Data.EffectSpec,
				SourceTags);
			return;
		}

		// ----- STATUS INCOMING VALUES HANDLING -----
		if (const FPOTIncomingStatusHandling* const StatusHandlingInformation = CallForIncomingStatusHandling.Find(Data.EvaluatedData.Attribute))
		{
			HandleIncomingStatusAttributeChange(
				DeltaValue,
				*StatusHandlingInformation,
				TargetCharacter,
				Data,
				HitResult,
				SourceTags);
			return;
		}

		if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute() || Data.EvaluatedData.Attribute == GetIncomingSurvivalDamageAttribute())
		{
			const float OldHealth = GetHealth();
			if (OldHealth == 0.f)
			{
				// Don't do anything if we're already dead
				return;
			}

			if (bTargetCharacterIsAssigned && POTEffectContextIsAssigned && POTEffectContext->GetKnockbackMode() != EKnockbackMode::KM_None && !bImmuneToKnockBack)
			{
				float KnockbackForce = POTEffectContext->GetKnockbackForce();
				if (SourceASC)
				{
					KnockbackForce *= SourceASC->GetNumericAttribute(UCoreAttributeSet::GetKnockbackMultiplierAttribute());					
				}

				float SourceWeight = Instigator ? Instigator->GetCombatWeight() : 1000.f;
				float TargetWeight = TargetCharacter->GetCombatWeight();

				float WeightRatio = SourceWeight / TargetWeight;

				KnockbackForce *= WeightRatio;

				if (KnockbackForce > 0.f)
				{
					FVector ForceDirection = UPOTAbilitySystemGlobals::Get().CalculateForceDirection(TargetActor, Instigator,
						HitResult, POTEffectContext->GetKnockbackMode());
					TargetCharacter->HandleKnockbackForce(ForceDirection, KnockbackForce);				
				}
			}

			// Store a local copy of the amount of damage done and clear the damage attribute

			const bool bDamageIsFallDamage = POTEffectContextIsAssigned && POTEffectContext->GetDamageType() == EDamageType::DT_BREAKLEGS;
			const bool bDamageIsOxygenDamage = POTEffectContextIsAssigned && POTEffectContext->GetDamageType() == EDamageType::DT_OXYGEN;
			const bool bIgnoreDamage = bGodmode || (bHomecaveBuff && !bDamageIsFallDamage && !bDamageIsOxygenDamage);
			float ActualIncomingDamage = (bTargetCharacterIsAssigned && TargetCharacter->IsHomecaveCampingDebuffActive()) ? GetIncomingDamage() * 2.0f : GetIncomingDamage();

			if (Instigator && Instigator != TargetCharacter)
			{
				if (Instigator->IsHomecaveBuffActive())
				{
					ActualIncomingDamage *= 0.0f;
				}
				else if (Instigator->IsHomecaveCampingDebuffActive())
				{
					ActualIncomingDamage *= 0.2f;
				}
			}

			const float LocalDamageDone = bIgnoreDamage ? 0.f : ActualIncomingDamage + GetIncomingSurvivalDamage();

			SetIncomingDamage(0.f);
			SetIncomingSurvivalDamage(0.f);

			if (LocalDamageDone > 0
				&& TargetActor
				&& !TargetTags.HasTag(WASG.FullDamageImmunityTag)
				&& TargetActor->CanBeDamaged())
			{
				// UE_LOG(LogTemp, Log, TEXT("LocalDamageDone = %f"), LocalDamageDone);
				float NewHealth = OldHealth - LocalDamageDone;

				if (NewHealth <= 0.f && TargetTags.HasTag(WASG.LastHitDamageImmunityTag))
				{
					NewHealth = 1.f;
					// if (TargetCharacterIsAssigned)
					// {
					// 	TargetCharacter->OnImmunityLastHit.Broadcast();
					// }
				}

				SetHealth(NewHealth);

				if (bTargetCharacterIsAssigned)
				{
					// This is proper damage
					TargetCharacter->HandleDamage(LocalDamageDone, HitResult, Data.EffectSpec, SourceTags, SourceController, SourceActor);

					// Call for all health changes
					TargetCharacter->HandleHealthChange(-LocalDamageDone, HitResult, Data.EffectSpec, SourceTags, SourceController, SourceActor);
				}
			}
		}
		else if (Data.EvaluatedData.Attribute == GetHealthAttribute())
		{
			// Handle other health changes such as from healing or direct modifiers
			if (bTargetCharacterIsAssigned && DeltaValue != 0.f)
			{
				// Call for all health changes
				TargetCharacter->HandleHealthChange(DeltaValue, HitResult, Data.EffectSpec, SourceTags, nullptr, nullptr);
			}
		}
		else if (Data.EvaluatedData.Attribute == GetIncomingBoneBreakAmountAttribute())
		{
			const float LocalDamageDone = bCanBeDamaged ? 0.f : GetIncomingBoneBreakAmount();
			SetIncomingBoneBreakAmount(0.f);

			if (LocalDamageDone > 0.f)
			{
				const float ExistingLegDamage = GetLegDamage();
				float NewLegDamage = ExistingLegDamage + LocalDamageDone;
				SetLegDamage(NewLegDamage);
				if (bTargetCharacterIsAssigned)
				{
					if (ExistingLegDamage <= 0.f)
					{
						const FVector HitRelativeLocation = HitResult.Location - TargetCharacter->GetActorLocation();
						TargetCharacter->ReplicateParticle(FDamageParticleInfo(EDamageEffectType::BROKENBONEONGOING, HitRelativeLocation, TargetCharacter->GetActorRotation()));
					}

					TargetCharacter->HandleBrokenBone(SourceTags);
				}
			}
		}
		else if (Data.EvaluatedData.Attribute == GetWellRestedBonusMultiplierAttribute())
		{
			if (bTargetCharacterIsAssigned && GetWellRestedBonusMultiplier() > 0.f)
			{
				TargetCharacter->HandleWellRested(SourceTags);
			}
		}
		else if (Data.EvaluatedData.Attribute == GetLegDamageAttribute())
		{
			if (bTargetCharacterIsAssigned && DeltaValue != 0.f)
			{
				TargetCharacter->HandleLegDamageChange(DeltaValue, SourceTags);
			}
		}
		else if (Data.EvaluatedData.Attribute == GetCurrentBodyFoodAmountAttribute())
		{
			// Handle other health changes such as from healing or direct modifiers
			if (bTargetCharacterIsAssigned && DeltaValue != 0.f)
			{
				// Call for all health changes
				TargetCharacter->HandleBodyFoodChange(Data.EvaluatedData.Magnitude, SourceTags);
			}
		}
	}
	else
	{
		if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute() || Data.EvaluatedData.Attribute == GetIncomingSurvivalDamageAttribute())
		{
			if (bTargetCharacterIsAssigned && POTEffectContextIsAssigned && POTEffectContext->GetKnockbackMode() != EKnockbackMode::KM_None && bImmuneToKnockBack)
			{
				float KnockbackForce = POTEffectContext->GetKnockbackForce();

				float SourceWeight = Instigator ? Instigator->GetCombatWeight() : 1000.f;
				float TargetWeight = TargetCharacter->GetCombatWeight();

				float WeightRatio = SourceWeight / TargetWeight;

				KnockbackForce *= WeightRatio;

				if (KnockbackForce > 0.f)
				{
					FVector ForceDirection = UPOTAbilitySystemGlobals::Get().CalculateForceDirection(TargetActor, Instigator,
						HitResult, POTEffectContext->GetKnockbackMode());
					TargetCharacter->HandleKnockbackForce(ForceDirection, KnockbackForce);
				}
			}
		}
	}
}

bool UCoreAttributeSet::ShouldInitProperty(bool FirstInit, FProperty* PropertyToInit) const
{
	return PropertyToInit != GetHealthAttribute().GetUProperty()
		&& PropertyToInit != GetHungerAttribute().GetUProperty()
		&& PropertyToInit != GetThirstAttribute().GetUProperty()
		&& PropertyToInit != GetOxygenAttribute().GetUProperty()
		&& PropertyToInit != GetStaminaAttribute().GetUProperty()
		&& PropertyToInit != GetGrowthAttribute().GetUProperty()
		// STATUS_UPDATE_MARKER - Add any new current status attribute here.
		&& PropertyToInit != GetLegDamageAttribute().GetUProperty()
		&& PropertyToInit != GetBleedingRateAttribute().GetUProperty()
		&& PropertyToInit != GetPoisonRateAttribute().GetUProperty()
		&& PropertyToInit != GetVenomRateAttribute().GetUProperty();
}

void UCoreAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Health, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, CurrentBodyFoodAmount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Growth, Params);

	FDoRepLifetimeParams OwnerOnlyParams{};
	OwnerOnlyParams.bIsPushBased = true;
	OwnerOnlyParams.Condition = COND_OwnerOnly;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, WellRestedBonusMultiplier, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, WellRestedBonusStartedGrowth, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, WellRestedBonusEndGrowth, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Hunger, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Thirst, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Oxygen, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Stamina, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, AttributeCapsConfig, OwnerOnlyParams);

	// STATUS_UPDATE_MARKER - DOREPLIFETIME
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, LegDamage, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, BleedingRate, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, VenomRate, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, PoisonRate, OwnerOnlyParams);
	
	FDoRepLifetimeParams CustomParams{};
	CustomParams.bIsPushBased = true;
	CustomParams.Condition = COND_Custom;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, MaxHealth, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, HealthRecoveryRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, HealthRecoveryMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, MaxStamina, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaRecoveryRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaRecoveryMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, CombatWeight, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Armor, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, MovementSpeedMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, TurnRadiusMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, TurnInPlaceRadiusMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, SprintingSpeedMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, TrottingSpeedMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, JumpForceMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, AirControlMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, BodyFoodAmount, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, BodyFoodCorpseThreshold, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, MaxHunger, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, HungerDepletionRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, FoodConsumptionRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, MaxThirst, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, ThirstDepletionRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, ThirstReplenishRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, WaterConsumptionRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, MaxOxygen, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, OxygenDepletionRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, OxygenRecoveryRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, FallDeathSpeed, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, FallingLegDamage, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, LimpHealthThreshold, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, KnockbackToDelatchThreshold, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, KnockbackToDecarryThreshold, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, KnockbackToCancelAttackThreshold, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, CarryCapacity, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, HungerDamage, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, ThirstDamage, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, OxygenDamage, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, GrowthPerSecond, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, GrowthPerSecondMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, WaterVision, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, WetnessDurationMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, BuffDurationMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, SpikeDamageMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, KnockbackMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, GroundAccelerationMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, GroundPreciseAccelerationMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, KnockbackTractionMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, SwimmingAccelerationMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaJumpCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaSprintCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaSwimCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaTrotSwimCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaFastSwimCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaDiveCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaTrotDiveCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaFastDiveCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaFlyCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, StaminaFastFlyCostMultiplier, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, CooldownDurationMultiplier, CustomParams);
	
	// STATUS_UPDATE_MARKER - DOREPLIFETIME
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, Wetness, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, BleedingHealRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, PoisonHealRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, VenomHealRate, CustomParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(UCoreAttributeSet, LegHealRate, CustomParams);
}

void UCoreAttributeSet::PreReplicate()
{
	if (!bHasDirtyConditionalProperty && bHasDoneInitialReplication)
	{
		return;
	}

	UNetDriver* NetDriver = GetWorld()->GetNetDriver();
	if (!ensureAlways(NetDriver))
	{
		return;
	}

	if (NetDriver->GetNetMode() == ENetMode::NM_Client)
	{
		return;
	}

	FRepChangedPropertyTracker* ChangedPropertyTrackerPtr = UE::Net::Private::FNetPropertyConditionManager::Get().FindOrCreatePropertyTracker(this).Get();
	if (!ensureAlways(ChangedPropertyTrackerPtr))
	{
		return;
	}
	// Need the reference for 5.3's API
	FRepChangedPropertyTracker& ChangedPropertyTracker = *ChangedPropertyTrackerPtr;
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, MaxHealth);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, HealthRecoveryRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, HealthRecoveryMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, MaxStamina);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaRecoveryRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaRecoveryMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, CombatWeight);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, Armor);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, MovementSpeedMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, TurnRadiusMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, TurnInPlaceRadiusMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, SprintingSpeedMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, TrottingSpeedMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, JumpForceMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, AirControlMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, BodyFoodAmount);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, BodyFoodCorpseThreshold);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, MaxHunger);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, HungerDepletionRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, FoodConsumptionRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, MaxThirst);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, ThirstDepletionRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, ThirstReplenishRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, WaterConsumptionRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, MaxOxygen);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, OxygenDepletionRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, OxygenRecoveryRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, FallDeathSpeed);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, FallingLegDamage);
	
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, LimpHealthThreshold);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, KnockbackToDelatchThreshold);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, KnockbackToDecarryThreshold);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, KnockbackToCancelAttackThreshold);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, CarryCapacity);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, HungerDamage);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, ThirstDamage);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, OxygenDamage);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, GrowthPerSecond);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, GrowthPerSecondMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, WaterVision);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, WetnessDurationMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, BuffDurationMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, SpikeDamageMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, KnockbackMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, GroundAccelerationMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, GroundPreciseAccelerationMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, KnockbackTractionMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, SwimmingAccelerationMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaJumpCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaSprintCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaSwimCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaTrotSwimCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaFastSwimCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaDiveCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaTrotDiveCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaFastDiveCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaFlyCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, StaminaFastFlyCostMultiplier);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, CooldownDurationMultiplier);

	// STATUS_UPDATE_MARKER - DOREPLIFETIME
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, BleedingHealRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, PoisonHealRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, VenomHealRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, LegHealRate);
	DOREPLIFETIME_ACTIVE_OVERRIDE_POT_CONDITIONAL(this, UCoreAttributeSet, Wetness);

	if (!bHasDoneInitialReplication)
	{
		bHasDoneInitialReplication = true;
	}
	else
	{
		bHasDirtyConditionalProperty = false;
	}
}

void UCoreAttributeSet::PostInitProperties()
{
	Super::PostInitProperties();
	if(UObject::GetWorld() && AttributeCaps.IsEmpty())
	{
		const UIGameInstance* GI = Cast<UIGameInstance>(UObject::GetWorld()->GetGameInstance());
		if (!GI)
		{
			return;
		}
		if (const AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession()))
		{
			AttributeCapsConfig = Session->GetAttributeCapsConfig();
			OnRep_AttributeCapsConfig();
			MARK_PROPERTY_DIRTY_FROM_NAME(UCoreAttributeSet, AttributeCapsConfig, this);
		}
	}
}

void UCoreAttributeSet::SetConditionalAttributeReplication(const FString& AttributeName, bool bEnabled)
{
	const FString FuncName = FString::Printf(TEXT("CondRep_%s"), *AttributeName);
	UFunction* const ConditionalReplicator = FindUField<UFunction>(UCoreAttributeSet::StaticClass(), *FuncName);
	if (!ConditionalReplicator)
	{
		return;
	}
	uint8* const Buffer = (uint8*)FMemory_Alloca(ConditionalReplicator->ParmsSize);
	FMemory::Memzero(Buffer, ConditionalReplicator->ParmsSize);

	for (TFieldIterator<FProperty> It(ConditionalReplicator); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		const FProperty* const FunctionProperty = *It;
		const FString Type = FunctionProperty->GetCPPType();

		if (Type == TEXT("bool"))
		{
			*FunctionProperty->ContainerPtrToValuePtr<bool>(Buffer) = bEnabled;
			break;
		}
	}

	ProcessEvent(ConditionalReplicator, Buffer);
}
