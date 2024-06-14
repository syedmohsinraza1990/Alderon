// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "Abilities/POTAbilityTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayEffect.h"
#include "IGameplayStatics.h"
#include "POTGameplayAbility.generated.h"


#if WITH_EDITOR

class UPOTAbilitySystemComponent;

struct FMontageAbilityGeneration {

public:
	UAnimMontage* CharacterMontage;
	TSubclassOf<UGameplayEffect> CooldownGameplayEffectClass;
	TSubclassOf<UGameplayEffect> DamageGameplayEffectClass;
	TSubclassOf<UGameplayEffect> CostGameplayEffectClass;

	FMontageAbilityGeneration() 
		: CharacterMontage(nullptr)
// 		, CooldownGameplayEffectClass(nullptr)
// 		, DamageGameplayEffectClass(nullptr)
		, bIsReady(false)
	{}

	void Prepare(
		UAnimMontage* InCharacterMontage,
		TSubclassOf<UGameplayEffect> InCooldownGameplayEffectClass,
		TSubclassOf<UGameplayEffect> InDamageGameplayEffectClass,
		TSubclassOf<UGameplayEffect> InCostGameplayEffectClass
	)
	{
		CharacterMontage = InCharacterMontage;
		CooldownGameplayEffectClass = InCooldownGameplayEffectClass;
		DamageGameplayEffectClass = InDamageGameplayEffectClass;
		CostGameplayEffectClass = InCostGameplayEffectClass;
		bIsReady = true;
	}

	void Clear(UClass* ObjectClass)
	{
		if (CharacterMontage != nullptr)
		{
			RemoveTemporaryPropertyFlags(ObjectClass, "AbilityMontageSet");
			RemoveTemporaryPropertyFlags(ObjectClass, "TimedTraceSections");
			RemoveTemporaryPropertyFlags(ObjectClass, "PredictionTimedTraceSections");
			RemoveTemporaryPropertyFlags(ObjectClass, "SectionDamageNotifyCount");
			RemoveTemporaryPropertyFlags(ObjectClass, "TimedOverlapConfigurations");
		}

		if (CooldownGameplayEffectClass.Get() != nullptr)
		{
			RemoveTemporaryPropertyFlags(ObjectClass, "CooldownGameplayEffectClass");
		}

		if (DamageGameplayEffectClass.Get() != nullptr)
		{
			RemoveTemporaryPropertyFlags(ObjectClass, "EffectContainerMap");
		}

		if (CostGameplayEffectClass.Get() != nullptr)
		{
			RemoveTemporaryPropertyFlags(ObjectClass, "CostGameplayEffectClass");
		}

		CharacterMontage = nullptr;
		CooldownGameplayEffectClass = nullptr;
		DamageGameplayEffectClass = nullptr;
		bIsReady = false;
	}

	FORCEINLINE bool IsReady() const { return bIsReady;  }

private:
	bool bIsReady;

private:
	void RemoveTemporaryPropertyFlags(UClass* ObjectClass, FName PropertyName)
	{
		FProperty* Property = ObjectClass->FindPropertyByName(PropertyName);
		if (Property != nullptr)
		{
			Property->ClearPropertyFlags(CPF_DuplicateTransient);
			Property->ClearPropertyFlags(CPF_ContainsInstancedReference);
		}
	}
};

#endif // WITH_EDITOR

struct FSocketsToRecord
{
	// Array has been replaced with set but left for compatibility issues if any ..
	TArray<FName> SocketNames;
	TSet<FName> SocketNamesSet;

	FSocketsToRecord() {}

	FSocketsToRecord(const TArray<FName>& InNames)
		: SocketNames(InNames)
		, SocketNamesSet(InNames)
	{
	}

	FSocketsToRecord(const FGameplayTagContainer& Tags)
	{
		for (auto It = Tags.CreateConstIterator(); It; ++It)
		{
			const FGameplayTag& Tag = *It;
			if (Tag.IsValid())
			{
				SocketNames.Add(UIGameplayStatics::GetTagLeafName(Tag));
				SocketNamesSet.Emplace(UIGameplayStatics::GetTagLeafName(Tag));
			}
		}
	}

	void Emplace(const TArray<FName>& InNames)
	{
		SocketNames.Append(InNames);
		SocketNamesSet.Append(InNames);
	}

	void Emplace(const FGameplayTag& Tag)
	{
		if (Tag.IsValid())
		{
			SocketNames.Add(UIGameplayStatics::GetTagLeafName(Tag));
			SocketNamesSet.Emplace(UIGameplayStatics::GetTagLeafName(Tag));
		}
	}
};

//A struct for temporary holding of actors (and transforms) of actors that we hit
//that don't have an ASC. Used for environment impact GameplayCues.
struct FNonAbilityTarget
{
	FGameplayTagContainer CueContainer;
	TWeakObjectPtr<AActor> TargetActor;
	FHitResult TargetHitResult;
	bool bHasHitResult;

public:
	FNonAbilityTarget()
		: CueContainer(FGameplayTagContainer())
		, TargetActor(nullptr)
		, TargetHitResult(FHitResult(ENoInit::NoInit))
		, bHasHitResult(false)
	{
	}

	FNonAbilityTarget(const FGameplayTagContainer& InCueTags, const FHitResult& InResult)
		: CueContainer(InCueTags)
		, TargetActor(TWeakObjectPtr<AActor>(InResult.GetActor()))
		, TargetHitResult(InResult)
		, bHasHitResult(true)
	{
	}

	FNonAbilityTarget(const FGameplayTagContainer& InCueTags, AActor* InActor)
		: CueContainer(InCueTags)
		, TargetActor(TWeakObjectPtr<AActor>(InActor))
		, TargetHitResult(FHitResult(ENoInit::NoInit))
		, bHasHitResult(false)
	{
	}
};

USTRUCT(BlueprintType)
struct FAbilityMontageSet
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* CharacterMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	UAnimMontage* WeaponMontage;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	float AnimRootMotionTranslationScale;
	// If True, constrain the root motion movement to character forward direction
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite)
	bool bOnlyForwardRootMotion;

public:
	FAbilityMontageSet()
		: CharacterMontage(nullptr)
		, WeaponMontage(nullptr)
		, AnimRootMotionTranslationScale(1.0f)
		, bOnlyForwardRootMotion(false)
	{
	}

	FAbilityMontageSet(UAnimMontage* InCharacterMontage)
		: CharacterMontage(InCharacterMontage)
		, WeaponMontage(nullptr)
		, AnimRootMotionTranslationScale(1.0f)
		, bOnlyForwardRootMotion(false)
	{
	}

};

//UENUM(BlueprintType)
//enum class 

/**
 * Subclass of ability blueprint type with game-specific data
 * This class uses GameplayEffectContainers to allow easier execution of gameplay effects based on a triggering tag
 * Most games will need to implement a subclass to support their game-specific code
 */
UCLASS( )
class PATHOFTITANS_API UPOTGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	static FMontageAbilityGeneration MontageAbilityGeneration;
#endif

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Wa Ability")
	FAbilityMontageSet AbilityMontageSet;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Block Ability")
	bool bBlockAbility;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Block Ability", meta=(EditCondition="bBlockAbility", EditConditionHides))
	TArray<EDamageWoundCategory> BlockingRegions;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	TMap<EDamageEffectType, FCurveTableRowHandle> BlockScalarPerDamageType;
	
	/**
	 * Deprecated, replaced by BlockScalarPerDamageType
	 */
	UE_DEPRECATED(5.3, "Deprecated, replaced by BlockScalarPerDamageType")
	UPROPERTY(BlueprintReadOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	FCurveTableRowHandle BlockScalarBaseDamage;

	/**
	 * Deprecated, replaced by BlockScalarPerDamageType
	 */
	UE_DEPRECATED(5.3, "Deprecated, replaced by BlockScalarPerDamageType")
	UPROPERTY(BlueprintReadOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	FCurveTableRowHandle BlockScalarBleed;

	/**
	 * Deprecated, replaced by BlockScalarPerDamageType
	 */
	UE_DEPRECATED(5.3, "Deprecated, replaced by BlockScalarPerDamageType")
	UPROPERTY(BlueprintReadOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	FCurveTableRowHandle BlockScalarPoison;
	
	/**
	 * Deprecated, replaced by BlockScalarPerDamageType
	 */
	UE_DEPRECATED(5.3, "Deprecated, replaced by BlockScalarPerDamageType")
	UPROPERTY(BlueprintReadOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	FCurveTableRowHandle BlockScalarVenom;
	
	/**
	 * Deprecated, replaced by BlockScalarPerDamageType
	 */
	UE_DEPRECATED(5.3, "Deprecated, replaced by BlockScalarPerDamageType")
	UPROPERTY(BlueprintReadOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	FCurveTableRowHandle BlockScalarKnockback;
	
	/**
	 * Deprecated, replaced by BlockScalarPerDamageType
	 */
	UE_DEPRECATED(5.3, "Deprecated, replaced by BlockScalarPerDamageType")
	UPROPERTY(BlueprintReadOnly, Category = "Block Ability", meta=(ForceInlineRow, EditCondition="bBlockAbility", EditConditionHides))
	FCurveTableRowHandle BlockScalarBoneBreak;

	bool IsBlockAbility() const;
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	UE_DEPRECATED(5.3, "Deprecated, replaced by GetBlockScalarForDamageType")
	float GetBlockScalarBaseDamage() const;
	UE_DEPRECATED(5.3, "Deprecated, replaced by GetBlockScalarForDamageType")
	float GetBlockScalarBleed() const;
	UE_DEPRECATED(5.3, "Deprecated, replaced by GetBlockScalarForDamageType")
	float GetBlockScalarPoison() const;
	UE_DEPRECATED(5.3, "Deprecated, replaced by GetBlockScalarForDamageType")
	float GetBlockScalarVenom() const;
	UE_DEPRECATED(5.3, "Deprecated, replaced by GetBlockScalarForDamageType")
	float GetBlockScalarKnockback() const;
	UE_DEPRECATED(5.3, "Deprecated, replaced by GetBlockScalarForDamageType")
	float GetBlockScalarBoneBreak() const;
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	UFUNCTION(BlueprintPure)
	float GetBlockScalarForDamageType(const EDamageEffectType DamageEffectType) const;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bStartMontageManually;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bDontEndAbilityOnMontageEnd;

	// Useful for abilities that should persist after being canceled, such as Ano fortify
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bDontEndAbilityOnCancel;

	// Useful for abilities that should end when released such as alberta charge movement
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bEndAbilityOnInputRelease;

	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bActivateOnGranted;

	// If true, the ability will not activate while in the air.
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bMustBeGrounded;

	// Usefull to prevent this ability from blocking things such as picking up items
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bPreventUsingAbilityDetection;

	// Used to flag if somehow an ability can be interrupted after the start ability runs
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bInterruptable = false;

	// If interrupted, give information about its cooldown
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	TSubclassOf<UGameplayEffect> InterruptCooldownEffectClass;

	bool bIsInterrupted = false;

	//Can this ability force dinosaurs to cancel attacks regardless of its knockback?
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	bool bCanForceAbilityInterrupt = false;

	//Which montage section(s) should this move play? 
	//If two consecutive moves have the same montage but different sections, it will just queue this section up.
	//If more than one section is defined, it will be chose at random (e.g. an interrupt "move" having 2-3 different
	//variations to it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wa Ability")
	TArray<FName> MontageSections;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wa Ability")
	float PlayRate;

	// Should the Effects with tags inside EffectContainerMap be removed when this ability is ended
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wa Ability")
	bool bRemoveEffectContainerOnEnd = false;
	// Should the Effects with tags inside SpecificActivationEffectTags be applied when this ability is activated
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wa Ability")
	bool bApplyEffectContainerOnActivate = false;

	// If bApplyEffectContainerOnActivate = true, only the tags in SpecificActivationEffectTags will be applied.
	// If this is empty, all the tags will be applied.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wa Ability")
	FGameplayTagContainer SpecificActivationEffectTags;

	// Map of gameplay tags to gameplay effect containers
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wa Ability"/*, meta = (Categories = "Event")*/)
	TMap<FGameplayTag, FPOTGameplayEffectContainer> EffectContainerMap;

	// GameplayEffects that should be applied to the character using this ability, after the ability connects with a target. Used for GEs that require net damage dealt to target, which is only available after an attack connects.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wa Ability")
	TArray<TSubclassOf<UGameplayEffect>> EffectsAppliedToSelfAfterDamage;

	//The knockback mode for this move
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wa Ability")
	EKnockbackMode KnockbackMode;
	//The knockback mode for this move
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wa Ability")
	EDamageType OverrideDamageType;
	//The multiplier applied to the weapon knockback force.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wa Ability")
	float KnockbackForceMultiplier;
	//Is movement disabled for the duration of this ability?
	UPROPERTY(EditAnywhere, Category = "Wa Ability")
	uint32 bDisableMovement : 1;
	//Is rotation disabled for the duration of this ability? Characters can still be "manually" rotated via animnotifies.
	UPROPERTY(EditAnywhere, Category = "Wa Ability")
	uint32 bDisableRotation : 1;
	//Does this ability allow friendly fire?
	UPROPERTY(EditAnywhere, Category = "Wa Ability")
	uint32 bFriendlyFire : 1;
	// By default the cooldown timer starts when the ability activates. If this is true, it will start as montage ends
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wa Ability", AdvancedDisplay)
	uint32 bStartCooldownWhenAbilityEnds : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wa Ability", AdvancedDisplay)
	uint32 bAutoCommitAbility : 1;
	//Whether to use dinosaur growth as level for the ability.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wa Ability", AdvancedDisplay)
	uint32 bUseGrowthAsLevel : 1;
	// If true then activating this ability will cancel any current ability
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wa Ability")
	uint32 bIsExclusive : 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wa Ability")
	uint32 bMustHaveTarget : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenCarryingObject : 1;
	//Can this attack catch a carriable (probably fish) during its attack?
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bCanCatchCarriable : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenInCombat : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenResting : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenSleeping : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenLatchedToCharacter : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenCarried : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowWhenCarryingCharacter : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowedWhenLegsBroken : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bAllowedWhenHomecaveBuffActive : 1;
	UPROPERTY(EditAnywhere, Category = "Ability")
	uint32 bStopPreciseMovement : 1;
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, meta = (Categories = "Lock"), Category = "Wa Ability")
	FGameplayTag MovementLockTag;
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, meta = (Categories = "Lock"), Category = "Wa Ability")
	FGameplayTag RotationLockTag;
	UPROPERTY(EditAnywhere, Category = "Ability")
	TArray<ECustomMovementType> AutoCancelMovementModes;
	//If the character moves slower than this speed for the duration of AutoCancelMovementSpeedDuration it will cancel the ability.
	UPROPERTY(EditAnywhere, Category = "Ability")
	float AutoCancelMovementSpeedThreshold;
	///If the character moves slower than AutoCancelMovementSpeedThreshold speed for this duration it will cancel the ability.
	UPROPERTY(EditAnywhere, Category = "Ability")
	float AutoCancelMovementSpeedDuration;

	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bShouldCancelOnCharacterHitWall = false;

	// If true, will apply DamageConfigOnHitWall as a damage Gameplay event.
	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bDealDamageOnHitWall = false;
	UPROPERTY(EditAnywhere, Category = "Ability")
	FWeaponDamageConfiguration DamageConfigOnHitWall;

	//These are the timed transforms for specific sockets in the montage. This is used for more accurate weapon traces.
	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	TArray<FTimedTraceBoneSection> TimedTraceSections;

	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	TArray<int32> SectionDamageNotifyCount;

	//The time it takes between trace position recordings.
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	float TraceTransformRecordInterval;

	virtual void InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;
	virtual void InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

protected:
	void ReleaseAbility(UPOTAbilitySystemComponent* AbilitySystem);

#if WITH_EDITOR

	virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
	
#endif
	
	void CheckBlockScalarMigration();
	virtual void PostInitProperties() override;
public:

	// these gameplay tags will be applied when ability is pressed, and removed when the ability is released.
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	TArray<FGameplayTag> InputTags;

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsPressed() const;

	float GetChargedDuration() const;

	// If true, the character movement component will be forced to the max speed defined by GetMaxMovementMultiplierSpeed .
	UPROPERTY(EditAnywhere, Category = "Ability Speed")
	bool bApplyMovementOverride = false;

	UPROPERTY(EditAnywhere, Category = "Ability Speed")
	bool bAllowVerticalControlInWater = false;

	// If true, the character will not "stick" to water surface.
	UPROPERTY(EditAnywhere, Category = "Ability Speed")
	bool bOverrideMovementImmersionDepth = false;

	// SetByCaller.MovementMultiplier will be 0 at 0 speed and 1 at MaxMovementMultiplierSpeed.
	// Multiply damage by this value in the damage execution for speed based damage.
	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Ability Speed")
	float GetMaxMovementMultiplierSpeed() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Ability Speed")
	bool HasForcedMovementRotation() const;

	UFUNCTION(BlueprintImplementableEvent, BlueprintPure, Category = "Ability Speed")
	FRotator GetForcedMovementRotation() const;

	// If true, SetByCaller.ChargedMultiplier will be a multiplier defined by the following properties.
	// If false, it will be 1.0
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	bool bShouldUseChargeDuration = false;

	// The seconds since activating the ability that the ChargedMultiplier will begin increasing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	float MinChargeDuration = 0.f;

	// The seconds since activating the ability that the ChargedMultiplier will stop increasing.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	float MaxChargeDuration = 0.f;

	// The amount the ChargedMultiplier will be equal to when the Duration = MinChargeDuration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	float MinChargeMultiplier = 0.f;

	// The amount the ChargedMultiplier will be equal to when the Duration = MaxChargeDuration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	float MaxChargeMultiplier = 1.f;

	// If true and the ChargedDuration is less than ChargeCancelThreshold, the ability will be canceled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	bool bShouldCancelBeforeDuration = false;

	// If Ability is released before this amount of seconds, it will be canceled.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	float ChargeCancelThreshold = 0.f;

	// If true, the ability will be released automatically after holding with full charge for the AutoReleaseDuration.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	bool bShouldAutoRelease = false;

	// The amount of time after the ability is fully charged that it will release automatically.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	float AutoReleaseDuration = 0.f;

	// If true then AnimNotify sounds will be faded out. Does not prevent new AnimNotify sounds from triggering after release.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability Charging")
	bool bCancelSoundsOnRelease = false;
private:
	bool bIsPressed = false;
	float ChargedDuration = 0.f;
	float PressedTime = 0.f;
	FTimerHandle AutoReleaseTimerHandle;

public:
	// If greater than 0, the ability will trigger every time this period passes.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Triggers)
	float PeriodicActivation = 0.f;

	// OnTagChanged will be called whenever any of these tags are changed.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Triggers)
	TArray<FGameplayTag> MonitoredTags;

private:
	FTimerHandle PeriodicActivationTimerHandle;
	TMap<FGameplayTag, FDelegateHandle> MonitoredTagHandles;

	void PeriodicActivate();

public:
	// Useful for abilities that use cost for checking if activation is allowed, but then apply the cost in a different way.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Costs")
	bool bDontApplyCost = false;

	// Constructor and overrides
	UPOTGameplayAbility();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */, const FGameplayTagContainer* TargetTags /* = nullptr */, OUT FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const override;

	virtual void PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate, const FGameplayEventData* TriggerEventData) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* OwnerInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	void ActivateAbility_Stage2(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* OwnerInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData);

	virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;


	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "On Remove Ability")
	void BPRemoveAbility(const FGameplayAbilityActorInfo& ActorInfo);
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "On Give Ability")
	void BPGiveAbility(const FGameplayAbilityActorInfo& ActorInfo);

	virtual void OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

	int32 GetAbilityLevelFromGrowth() const;
	void RegisterEventsHandlers();

	UFUNCTION(BlueprintCallable)
	virtual UGameplayEffect* GetCooldownGameplayEffect() const override;

	virtual bool CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool ForceCooldown, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) override;
	virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
	virtual bool CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, OUT FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	
	virtual void CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) override;

	UFUNCTION(BlueprintCallable, Category = "Wa Ability", DisplayName = "Play Montage")
	void K2_PlayMontage(UPOTAbilitySystemComponent* WAC, const FGameplayAbilityActivationInfo ActivationInfo, UAnimInstance* WaAnimInstance);
	virtual void PlayMontage(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* OwnerInfo, 
		UPOTAbilitySystemComponent* WAC, const FGameplayAbilityActivationInfo ActivationInfo, UAnimInstance* WaAnimInstance);

	// Transition to another montage but keep this ability active
	void TransitionMontage(UAnimMontage* const NewMontage, UAnimInstance* const AnimInstance, const FName Section = NAME_None);

	bool IsUsingTransitionMontage() const;

	UFUNCTION(BlueprintCallable, Category = "Wa Ability")
	void StopCurrentAbilityMontage();

protected:
	float LastCooldownPing = 0;

	bool bUsingTransitionMontage = false;
public:

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsPassive() const;

	UFUNCTION(BlueprintNativeEvent, Category = Ability, DisplayName = "On Tag Changed")
	void OnTagChanged(const FGameplayTag Tag, int32 NewCount);
	virtual void OnTagChanged_Implementation(const FGameplayTag Tag, int32 NewCount) {}

	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "Cancel Ability")
	void BPCancelAbility();

	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
		bool bReplicateCancelAbility) override;

	void CancelAbility_Stage2(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateCancelAbility);

	virtual bool CanBeCanceled() const override;

	//If an ability is activated via this event it will still fire all the play montage stuff, instead of replacing it.
	UFUNCTION(BlueprintNativeEvent, Category = Ability, DisplayName = "Post Montage Activate Ability", meta = (ScriptName = "post_montage_activate_ability"))
	void K2_PostMontageActivateAbility();
	virtual void K2_PostMontageActivateAbility_Implementation() {};

	//If an ability is activated via this event it will still fire all the play montage stuff, instead of replacing it.
	UFUNCTION(BlueprintImplementableEvent, Category = Ability, DisplayName = "Pre-Montage Activate Ability", meta = (ScriptName = "pre_montage_activate_ability"))
	void K2_PreMontageActivateAbility();
	 
	virtual void K2_EndAbility() override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Wa Ability")
	void GetMontageSet(const FGameplayAbilitySpecHandle Handle,
			const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
		FAbilityMontageSet& OutMontageSet) const;

	virtual void GetMontageSet_Implementation(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo,
		FAbilityMontageSet& OutMontageSet) const;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// End the ability without canceling it, using a prediction key.
	UFUNCTION(BlueprintCallable, Category = Ability)
	void FinishAbility();

	UFUNCTION()
	virtual void OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, 
		class UPOTAbilitySystemComponent* AbilitySystemComponent, int Key);

	int MontageEndKey = 0;

	UFUNCTION()
	void OnAnyMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UFUNCTION(BlueprintImplementableEvent, Category = "Wa Ability", DisplayName = "On Character Montage Ended", meta = (ScriptName = "OnCharacterMontageEnded"))
	void K2_OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, class UPOTAbilitySystemComponent* AbilitySystemComponent) const;

	/** Make gameplay effect container spec to be applied later, using the passed in container */
	UFUNCTION(BlueprintCallable, Category = Ability, meta=(AutoCreateRefTerm = "EventData,SetByCallerMagnitudes"))
	virtual TArray<FPOTGameplayEffectContainerSpec> MakeEffectContainerSpecFromContainer(
		const FPOTGameplayEffectContainer& Container, 
		const FGameplayEventData& EventData, const TMap<FGameplayTag, float>& SetByCallerMagnitudes, int32 OverrideGameplayLevel = -1);

	/** Search for and make a gameplay effect container spec to be applied later, from the EffectContainerMap */
	UFUNCTION(BlueprintCallable, Category = Ability, meta = (AutoCreateRefTerm = "EventData,SetByCallerMagnitudes"))
	virtual TArray<FPOTGameplayEffectContainerSpec> MakeEffectContainerSpec(FGameplayTag ContainerTag, 
		const FGameplayEventData& EventData,
		const TMap<FGameplayTag, float>& SetByCallerMagnitudes,
		int32 OverrideGameplayLevel = -1);

	UFUNCTION(BlueprintCallable, Category = Ability, meta = (AutoCreateRefTerm = "EventData,SetByCallerMagnitudes"))
	virtual TArray<FPOTGameplayEffectContainerSpec> MakeSpecificEffectContainerSpec(FGameplayTag ContainerTag,
		const FGameplayEventData& EventData,
		TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap,
		const TMap<FGameplayTag, float>& SetByCallerMagnitudes,
		int32 OverrideGameplayLevel = -1);

	/** Applies a gameplay effect container spec that was previously created */
	UFUNCTION(BlueprintCallable, Category = Ability)
	virtual TArray<FActiveGameplayEffectHandle> ApplyEffectContainerSpec(
		const FPOTGameplayEffectContainerSpec& ContainerSpec);

	/** Applies a gameplay effect container, by creating and then applying the spec */
	UFUNCTION(BlueprintCallable, Category = Ability, meta = (AutoCreateRefTerm = "EventData,SetByCallerMagnitudes"))
	virtual TArray<FActiveGameplayEffectHandle> ApplyEffectContainer(FGameplayTag ContainerTag, 
		const FGameplayEventData& EventData, 
		const TMap<FGameplayTag, float>& SetByCallerMagnitudes, 
		int32 OverrideGameplayLevel = -1);

	UFUNCTION(BlueprintCallable, Category = Ability, meta = (AutoCreateRefTerm = "EventData,SetByCallerMagnitudes"))
	virtual TArray<FActiveGameplayEffectHandle> ApplySpecifiedEffectContainer(FGameplayTag ContainerTag,
		const FGameplayEventData& EventData,
		TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap,
		const TMap<FGameplayTag, float>& SetByCallerMagnitudes,
		int32 OverrideGameplayLevel = -1);

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool ExecuteEffectContainerCuesAtTransform(const FGameplayTag& ContainerTag, 
		const FTransform& DestinationTransform,
		TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap);

	UFUNCTION(BlueprintCallable, Category = Ability)
	bool ExecuteEffectContainerCuesAtHit(const FGameplayTag& ContainerTag,
		const FHitResult& HitResult,
		TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap);

	UFUNCTION(BlueprintNativeEvent, Category = Ability)
	float GetPlayRate() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = Ability)
	FGameplayTagContainer GetAbilityTriggerTags() const;

	TArray<FAbilityTriggerData>& GetAbilityTriggers();

	UFUNCTION(BlueprintCallable, Category = Ability)
	void RemoveEffectContainerByTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = Ability)
	void RemoveEffectContainerByTagActorInfo(FGameplayTag Tag, const FGameplayAbilityActorInfo& ActorInfo);

	virtual FGameplayEffectContextHandle GetContextFromOwner(FGameplayAbilityTargetDataHandle OptionalTargetData) const override;

	UFUNCTION(BlueprintPure, Category = Ability)
	FORCEINLINE class AIBaseCharacter*  GetOwnerPOTCharacter() const
	{
		return OwningPOTCharacter;
	}

	UFUNCTION(BlueprintPure, Category = Ability)
	bool HasCurrentActorInfo() const 
	{ 
		return CurrentActorInfo != nullptr; 
	}

	FORCEINLINE bool AcceptsTargetedEvents() const 
	{ 
		return bAcceptsTargetedEvents; 
	}

	virtual FGameplayEffectContextHandle MakeEffectContext(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo *ActorInfo) const;

	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, 
		OUT FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const override;

	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

	UFUNCTION(BlueprintCallable, Category = "Wa Ability")
	void RemoveActiveCooldown();

	
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;

	void GetTimedTraceTransforms(TArray<FTimedTraceBoneGroup>& OutBoneGroups) const;

	void GetTimedTraceTransformGroup(float Start, float End, const FSocketsToRecord& CurrentSocketGroup, USkeleton* Skel, TArray<FTimedTraceBoneSection>& TraceSections, UAnimMontage* Montage);

	FORCEINLINE TArray<struct FAnimNotifyEvent> GetDoDamageNotifies() const
	{
		return DoDamageNotifies;
	}

	FORCEINLINE float GetLastDoDamageNotifyEndTime() const
	{
		return LastDoDamageNotifyEndTime;
	}

	FORCEINLINE FAnimNotifyEvent GetNotifyEventForCurrentTraceGroup() const
	{
		return DoDamageNotifies[TraceGroup];
	}

	FORCEINLINE class UAnimNotifyState_DoDamage* GetDoDamageNotifyForCurrentTraceGroup() const;
	

#if WITH_EDITOR
	DECLARE_MULTICAST_DELEGATE(FOnMoveAnyPropertyChangedMulticaster)
	FOnMoveAnyPropertyChangedMulticaster OnMoveAnyPropertyChanged;
	typedef FOnMoveAnyPropertyChangedMulticaster::FDelegate FOnMoveAnyPropertyChanged;


	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	FDelegateHandle RegisterOnMoveAnyPropertyChanged(const FOnMoveAnyPropertyChanged& Delegate);
	void UnregisterOnMoveAnyPropertyChanged(FDelegateHandle Handle);

	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	FORCEINLINE_DEBUGGABLE int32 IncrementTraceGroup()
	{
		return ++TraceGroup;
	}

	FORCEINLINE_DEBUGGABLE int32 AddToTraceGroup(int32 Delta)
	{
		return TraceGroup += Delta;
	}

	void ResetTracingData();

	UFUNCTION(BlueprintPure, Category = "Wa Ability")
	FORCEINLINE_DEBUGGABLE int32 GetTraceGroup() const
	{
		return TraceGroup;
	}

	// Should be called only from DoDamageNotify::NotifyEnd
	void SetNextTraceGroup();

	FORCEINLINE_DEBUGGABLE FDelegateHandle GetEventHandle() const
	{
		return EventHandle;
	}


protected:

	UPROPERTY(Transient, BlueprintReadOnly)
	class AIBaseCharacter* OwningPOTCharacter;


	TArray<FNonAbilityTarget, TInlineAllocator<1>> NonAbilityTargets;

	//Receive events where owner is the target
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Wa Ability")
	bool bAcceptsTargetedEvents;

	// index of current trace group
	int32 TraceGroup;

	// Used for prediction traces
	UPROPERTY()
	TArray<FAnimNotifyEvent> DoDamageNotifies;

	UPROPERTY()
	TArray<FGuid> DoDamageNotifyIDs;

	float LastDoDamageNotifyEndTime;

protected:

	virtual void OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload);

private:
	FDelegateHandle EventHandle;
	FGameplayTagContainer EventTagsFilter;

	//Also populated by OnGameplayEvent
	float CurrentEventMagnitude;
	
	FAbilityMontageSet CurrentMontageSet;

private:
	FQuat OriginalRootRotation;
	float CurrentKnockbackForce;


#if WITH_EDITOR
	// Used by MontageBluetilities to seed WaGameplayAbility attributes
	void GenerateFromScript();
	void SetTemporaryFlags(const TArray<FString>& Properties);
#endif

public:
	void GatherWeaponTraceData(USkeletalMeshComponent* OwnerMesh, UAnimMontage* OverrideMontage = nullptr);

private:
	void ExtractBoneTransform(USkeleton* Skel, const FReferenceSkeleton& RefSkel, FName BoneName, UAnimSequence* AnimSeq, float AnimTime, bool bUseRawDataOnly, const bool bExtractRootMotion, FTransform& GlobalTransform);
	void GetBonePoseForTime(const UAnimSequence* AnimationSequence, FName BoneName, float Time, bool bExtractRootMotion, FTransform& Pose);
	void GetBonePosesForTime(const UAnimSequence* AnimationSequence, TArray<FName> BoneNames, float Time, bool bExtractRootMotion, TArray<FTransform>& Poses);
	bool IsValidTimeInternal(const UAnimSequence* AnimationSequence, const float Time);

public:
	//Holds all montage timings for triggering overlap checks
	UPROPERTY(VisibleDefaultsOnly, AdvancedDisplay, BlueprintReadOnly, Category = "Wa Ability")
	TArray<FTimedOverlapConfiguration> TimedOverlapConfigurations;

	void ResetOverlapTiming();

protected:
	UPROPERTY()
	TArray<FGuid> OverlapNotifyIDs;

	void GatherOverlapNotifyData();
};