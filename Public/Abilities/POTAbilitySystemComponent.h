// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "POTAbilityTypes.h"
#include "POTAbilitySystemComponentBase.h"
#include "POTAbilitySystemComponent.generated.h"

/************************************************************************/
/* These are the moves that are required for the character to function. */
/************************************************************************/

class UPOTGameplayAbility;

USTRUCT()
struct FPOTGameplayAbilityRepAnimMontage
{
	GENERATED_USTRUCT_BODY()

	FPOTGameplayAbilityRepAnimMontage() {}

	// Create a new FPOTGameplayAbilityRepAnimMontage with a new Unique Id
	static FPOTGameplayAbilityRepAnimMontage Generate();

	// Need a unique ID to ensure montages are replicated properly. Montages aren't getting replicated correctly when 
	// a new one is added at the same time as an old one is removed, because the values can be exactly the same (despite needing to be reset)
	UPROPERTY()
	uint32 UniqueId = 0;

	UPROPERTY()
	FGameplayAbilityRepAnimMontage InnerMontage;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

};

template<>
struct TStructOpsTypeTraits<FPOTGameplayAbilityRepAnimMontage> : public TStructOpsTypeTraitsBase2<FPOTGameplayAbilityRepAnimMontage>
{
	enum
	{
		WithNetSerializer = true,
	};
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttackAbilitySignature, UPOTGameplayAbility*, AttackAbility, const bool, bCancelled);

// Damn UE4 and this workarounds ..
USTRUCT(BlueprintType)
struct PATHOFTITANS_API FAttributeMagnitudes
{
	GENERATED_BODY()
public:
	FAttributeMagnitudes()
	{

	}

	FAttributeMagnitudes(const TMap<FGameplayAttribute, float>& InAttributeMagnitudes)
		: AttributeMagnitudesMap(InAttributeMagnitudes)
	{}

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<FGameplayAttribute, float> AttributeMagnitudesMap;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCostApplied, const UGameplayAbility*, Ability, const FAttributeMagnitudes&, AttributeMagnitudes);

// STATUS_UPDATE_MARKER - Add a field for the new status.

USTRUCT(BlueprintType)
struct FBoneDamageModifier 
{
	GENERATED_BODY()
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float DamageMultiplier;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BleedMultiplier;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float PoisonMultiplier;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float VenomMultiplier;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BoneBreakChanceMultiplier;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float BoneBreakDamageMultiplier;	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float SpikesDamageMultiplier;

public:

	FBoneDamageModifier()
		: DamageMultiplier(1.f)
		, BleedMultiplier(1.f)
		, PoisonMultiplier(1.f)
		, VenomMultiplier(1.f)
		, BoneBreakChanceMultiplier(1.f)
		, BoneBreakDamageMultiplier(1.f)
		, SpikesDamageMultiplier(0.f)
	{}

	uint32 GetTypeHash() const
	{
		return FCrc::MemCrc32(this, sizeof(FBoneDamageModifier));
	}

	FORCEINLINE bool operator==(const FBoneDamageModifier& Rhs) const
	{
		return DamageMultiplier			== Rhs.DamageMultiplier &&
			BleedMultiplier				== Rhs.BleedMultiplier &&
			PoisonMultiplier			== Rhs.PoisonMultiplier &&
			VenomMultiplier				== Rhs.VenomMultiplier &&
			BoneBreakChanceMultiplier	== Rhs.BoneBreakChanceMultiplier &&
			SpikesDamageMultiplier		== Rhs.SpikesDamageMultiplier &&
			BoneBreakDamageMultiplier	== Rhs.BoneBreakDamageMultiplier;
	}

	FORCEINLINE bool operator!=(const FBoneDamageModifier& Rhs) const
	{
		return !(*this == Rhs);
	}

	FORCEINLINE FBoneDamageModifier operator*(const FBoneDamageModifier& Rhs) const
	{
		FBoneDamageModifier NewValue{};
		NewValue.DamageMultiplier			= DamageMultiplier * Rhs.DamageMultiplier;
		NewValue.BleedMultiplier			= BleedMultiplier * Rhs.BleedMultiplier;
		NewValue.PoisonMultiplier			= PoisonMultiplier * Rhs.PoisonMultiplier;
		NewValue.VenomMultiplier			= VenomMultiplier * Rhs.VenomMultiplier;
		NewValue.BoneBreakChanceMultiplier	= BoneBreakChanceMultiplier * Rhs.BoneBreakChanceMultiplier;
		NewValue.BoneBreakDamageMultiplier	= BoneBreakDamageMultiplier * Rhs.BoneBreakDamageMultiplier;
		NewValue.SpikesDamageMultiplier		= SpikesDamageMultiplier * Rhs.SpikesDamageMultiplier;
		return NewValue;
	}

	FORCEINLINE FBoneDamageModifier operator/(const FBoneDamageModifier& Rhs) const
	{
		FBoneDamageModifier NewValue{};
		NewValue.DamageMultiplier			= DamageMultiplier / Rhs.DamageMultiplier;
		NewValue.BleedMultiplier			= BleedMultiplier / Rhs.BleedMultiplier;
		NewValue.PoisonMultiplier			= PoisonMultiplier / Rhs.PoisonMultiplier;
		NewValue.VenomMultiplier			= VenomMultiplier / Rhs.VenomMultiplier;
		NewValue.BoneBreakChanceMultiplier	= BoneBreakChanceMultiplier / Rhs.BoneBreakChanceMultiplier;
		NewValue.BoneBreakDamageMultiplier	= BoneBreakDamageMultiplier / Rhs.BoneBreakDamageMultiplier;
		NewValue.SpikesDamageMultiplier		= SpikesDamageMultiplier / Rhs.SpikesDamageMultiplier;
		return NewValue;
	}


	FORCEINLINE FBoneDamageModifier& operator*=(const FBoneDamageModifier& Rhs)
	{
		*this = *this * Rhs;
		return *this;
	}

	FORCEINLINE FBoneDamageModifier& operator/=(const FBoneDamageModifier& Rhs)
	{
		*this = *this / Rhs;
		return *this;
	}

	FORCEINLINE float GetMultiplierByDamageEffectType(const EDamageEffectType& DamageEffectType)
	{
		switch (DamageEffectType) {
			case EDamageEffectType::BLEED:
				return BleedMultiplier;
			case EDamageEffectType::POISONED:
				return PoisonMultiplier;
			case EDamageEffectType::VENOM:
				return VenomMultiplier;
			case EDamageEffectType::NONE:
			case EDamageEffectType::BROKENBONE:
			case EDamageEffectType::BROKENBONEONGOING:
				return BoneBreakDamageMultiplier;
			case EDamageEffectType::DAMAGED:
				return DamageMultiplier;
			default:
				return 1.f;
		}
	}
};

FORCEINLINE uint32 GetTypeHash(const FBoneDamageModifier& Other)
{
	return Other.GetTypeHash();
}

//This is technically an _almost_ copy of ECustomMovementType, probably a good idea to consolidate these.
UENUM(BlueprintType)
enum class ELocomotionState : uint8
{
	LS_IDLE					UMETA(Display = "Idle"),
	LS_WALKING				UMETA(Display = "Walking"),
	LS_SPRINTING			UMETA(Display = "Sprinting"),
	LS_TROTTING				UMETA(Display = "Trotting"),
	LS_SWIMMING				UMETA(Display = "Swimming"),
	LS_TROTSWIMMING			UMETA(Display = "Trot Swimming"),
	LS_FASTSWIMMING			UMETA(Display = "Fast Swimming"),
	LS_DIVING				UMETA(Display = "Diving"),
	LS_TROTDIVING			UMETA(Display = "TrotDiving"),
	LS_FASTDIVING			UMETA(Display = "Fast Diving"),
	LS_CROUCHING			UMETA(Display = "Crouching"),
	LS_CROUCHWALKING		UMETA(Display = "Crouch Walking"),
	LS_JUMPING				UMETA(Display = "Jumping"),
	LS_FLYING				UMETA(Display = "Flying"),
	LS_FASTFLYING			UMETA(Display = "Fast Flying"),
	LS_LATCHED				UMETA(Display = "Latched"),
	LS_LATCHEDUNDERWATER	UMETA(Display = "Latched Underwater"),
	LS_CARRIED				UMETA(Display = "Carried"),
	LS_CARRIEDUNDERWATER	UMETA(Display = "Carried Underwater"),
};

USTRUCT(BlueprintType)
struct FStateEffects 
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Resting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Sleeping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Standing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Walking;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Trotting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Sprinting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Swimming;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> TrotSwimming;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> FastSwimming;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Diving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> TrotDiving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> FastDiving;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Crouching;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> CrouchWalking;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Jumping;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Flying;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> FastFlying;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Latched;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> LatchedUnderwater;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> Carried;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UGameplayEffect> CarriedUnderwater;


public:
	FStateEffects()
		: Resting(nullptr)
		, Sleeping(nullptr)
		, Standing(nullptr)
		, Walking(nullptr)
		, Trotting(nullptr)
		, Sprinting(nullptr)
		, Swimming(nullptr)
		, TrotSwimming(nullptr)
		, FastSwimming(nullptr)
		, Diving(nullptr)
		, TrotDiving(nullptr)
		, FastDiving(nullptr)
		, Crouching(nullptr)
		, CrouchWalking(nullptr)
		, Jumping(nullptr)
		, Flying(nullptr)
		, FastFlying(nullptr)
		, Latched(nullptr)
		, LatchedUnderwater(nullptr)
		, Carried(nullptr)
		, CarriedUnderwater(nullptr)
	{}

	bool IsValid() const
	{
		return Resting != nullptr &&
			Sleeping != nullptr &&
			Standing != nullptr &&
			Walking != nullptr &&
			Trotting != nullptr &&
			Sprinting != nullptr &&
			Swimming != nullptr &&
			FastSwimming != nullptr &&
			Diving != nullptr &&
			FastDiving != nullptr &&
			Crouching != nullptr &&
			CrouchWalking != nullptr &&
			Jumping != nullptr;
	}
};


/**
 * 
 */
UCLASS()
class PATHOFTITANS_API UPOTAbilitySystemComponent : public UPOTAbilitySystemComponentBase
{
	GENERATED_BODY()

public:

	UPROPERTY(Transient, DuplicateTransient)
	class AIBaseCharacter* ParentPOTCharacter;

	UPROPERTY(Replicated)
	bool bCurrentAttackAbilityUsesCharge;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//These are the persistent clearances added to this component when it enters combat. 
	//Useful for stuff like boss phases.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wa Combat")
	FGameplayTagContainer PersistentClearancesOnCombatStart;
	
	//This number defines how many consequent attacks there have been so far. Resets at the end of the animation cycle.
	UPROPERTY(BlueprintReadOnly, Category = "Wa Combat")
	int32 ConsequentAttacks;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TMap<FName, FBoneDamageModifier> BoneDamageMultiplier;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FStateEffects StateEffects;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSubclassOf<UGameplayEffect>> MeshEffects;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> StaminaExhaustedEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> PassiveGrowthEffect;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Character")
	TSubclassOf<UGameplayEffect> WetnessEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay)
	float LocomotionStateUpdateRate;

	// Ignore events prevent from receiving double damage (if ability uses more bodies/weapons at the same time)
	// but it is prone to a lot of bugs :/
	// It can have different uses as events are checked inside HandleGameplayEvent
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bAreIgnoreEventsEnabled;

	// Set to determine whether to play the clamp failed animation or not
	UPROPERTY(BlueprintReadOnly)
	bool bClampJustFailed = false;

public:

	UPROPERTY(BlueprintAssignable, Category = "Wa Combat")
	FOnAttackAbilitySignature OnAttackAbilityStart;
	UPROPERTY(BlueprintAssignable, Category = "Wa Combat")
	FOnAttackAbilitySignature OnAttackAbilityEnd;
	UPROPERTY(BlueprintAssignable, Category = "Wa Combat")
	FOnCostApplied OnCostApplied;

public:
	UPOTAbilitySystemComponent();

	virtual void InitializeAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor, const bool bPreviewOnly = false) override;
	virtual void InitAttributes(bool bInitialInit /* = true */) override;

	void SetupAttributeDelegates();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, 
		FActorComponentTickFunction *ThisTickFunction) override;

private:
	float TrackedSpeed = 0.f;
	float SmoothedTrackedSpeed = 0.f;
	bool bHasBoundAttributeDelegates = false;

public:
	float GetTrackedSpeed() const { return TrackedSpeed; }
	UFUNCTION(BlueprintCallable, BlueprintPure)
	float GetSmoothedTrackedSpeed() const { return SmoothedTrackedSpeed; }

	virtual int32 GetLevel() const override;
	virtual float GetLevelFloat() const override;

	void GetActiveAbilitiesWithTag(const FGameplayTagContainer& GameplayTagContainer, 
		TArray<class UPOTGameplayAbility*>& ActiveAbilities);

	void RefreshPassiveAbilities();

protected:
	virtual void OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle) override;
	virtual void OnActiveEffectRemoved(const FActiveGameplayEffect& ActiveEffect) override;

public:
	virtual void InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor) override;
	virtual FName GetAttributeName() const override;

	void SetOwnerCollision(UPrimitiveComponent* Component);
	void SetOwnerSkeletalMesh(USkeletalMeshComponent* Mesh);

	virtual TArray<UAnimMontage*> GetActiveMontages() const override;

	/** Returns montage that is currently playing */
	UFUNCTION(BlueprintCallable, Category = "Character")
	UAnimMontage* POTGetCurrentMontage() const;

	/** Returns the current animating ability */
	UFUNCTION(BlueprintCallable, Category = "Character")
	UGameplayAbility* POTGetAnimatingAbility();

	UFUNCTION(BlueprintCallable, Category = "Animation")
	virtual float PlayMontage(UGameplayAbility* AnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage, float InPlayRate, FName StartSectionName = NAME_None, float StartTimeSeconds = 0.0f) override;
	
	UFUNCTION(BlueprintCallable, Category = "Animation")
	virtual void StopMontage(UGameplayAbility* InAnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage);

	void UpdateReplicatedAbilityMontages(bool bForceNetUpdate, UAnimMontage* MontageToUpdate = nullptr);

	void UpdateAutoCancelMovementType(ECustomMovementType MovementType);

	// Get all exclusive and non-exclusive active abilities
	TArray<UPOTGameplayAbility*> GetAllActiveGameplayAbilities() const;
	// Get all exclusive and non-exclusive active abilities
	bool HasAnyActiveGameplayAbility() const;

	void CancelActiveAbilitiesOnBecomeCarried(); 
	
	void CancelAllActiveAbilities();

	virtual bool GetShouldTick() const override;

	virtual void OnCharacterHitWall(const FHitResult& HitResult);

	UFUNCTION(Server, Reliable)
	void Server_HitWallEvent(const FHitResult& HitResult);
	void Server_HitWallEvent_Implementation(const FHitResult& HitResult);

	FORCEINLINE const TArray<FPOTGameplayAbilityRepAnimMontage>& GetAbilityMontages() const { return AbilityMontages; }

	TArray<FPOTGameplayAbilityRepAnimMontage>& GetAbilityMontages_Mutable();

protected:
	
	UPROPERTY(ReplicatedUsing = OnRep_AbilityMontages)
	TArray<FPOTGameplayAbilityRepAnimMontage> AbilityMontages;

	UFUNCTION()
	void OnRep_AbilityMontages();

public:
	
	bool GetCurrentAttackAbilityUsesCharge() const;
	
	void ReevaluateEffectsBasedOnAttribute(FGameplayAttribute Attribute, float NewLevel);

	void RemoveBoneDamageMultipliersOnEffectRemoved(const FActiveGameplayEffect& RemovedEffect);

	virtual void RemoveAllBuffs();

	virtual void NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, 
		bool bWasCancelled) override;

	bool GetDamageMultiplierForBone(const FName& BoneName, FBoneDamageModifier& OutDamageModifier) const;

	void UpdateStanceEffect();
	void UpdateSkinMeshEffect();

	virtual void ApplyExternalEffect(const TSubclassOf<UGameplayEffect>& Effect);

	UFUNCTION()
	void OnOwnerBoneBreak();

	UFUNCTION(BlueprintPure, Category = "Wa Combat")
	FORCEINLINE_DEBUGGABLE USkeletalMeshComponent* GetOwnerSkeletalMesh() const
	{
		return OwnerMesh;
	}

	UFUNCTION(BlueprintPure, Category = "Wa Combat")
	FORCEINLINE_DEBUGGABLE UPrimitiveComponent* GetOwnerCollision() const
	{
		return OwnerCollision;
	}

	// THIS DOESN'T HAVE TO BE "ATTACKING" ABILITY !
	UFUNCTION(BlueprintPure, Category = "Wa Combat")
	virtual UPOTGameplayAbility* GetCurrentAttackAbility() const override
	{
		return CurrentAttackAbility;
	}

	virtual UAnimMontage* GetCurrentAttackMontage() const override;

	virtual bool CurrentAbilityIsExclusive() const override;

	virtual bool CurrentAbilityExists() const override;

	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	void CancelCurrentAttackAbility();
	
	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	bool TryActivateAbilityByDerivedClass(TSubclassOf<UGameplayAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	void CancelAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass);

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool HasReplicatedLooseGameplayTag(const FGameplayTag& Tag);

	bool GetAggregatedTags(FGameplayTagContainer& OutTags) const;

	void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;



	virtual void ClearConsequentAttacks(const bool bClearTotal);

	virtual void SetAbilityInput(FGameplayAbilitySpec& Spec, bool bPressed);


	UFUNCTION(BlueprintPure)
	FORCEINLINE float GetTimeInCurrentAttackAbility() const
	{
		return TimeInCurrentAttackAbility;
	}

	float GetCurrentMoveMontageRateScale() const;


	//Montage time can be scaled because of its RateScale
	float GetMontageTimeInCurrentMove() const;


	virtual void OnOwnerMovementModeChanged(class UCharacterMovementComponent* CharMovement, 
		EMovementMode PreviousMovementMode, uint8 PreviousCustomMode);

	
	virtual FGameplayEffectContextHandle MakeEffectContext() const override;
	virtual void PostDeath() override;

	// Execute instant effects such as charge knockback to simulated proxy
	virtual void ExecuteEffectClientPredicted(FGameplayEffectSpec& Spec);

	void ResetPeriodicEventTime()
	{
		LastPeriodEventTime = -1.f;
	}

	float GetLastPeriodicEventTime() const
	{
		return LastPeriodEventTime;
	}

	void MarkPeriodicEventTime()
	{
		LastPeriodEventTime = GetTimeInCurrentAttackAbility();
	}

	UFUNCTION(BlueprintPure, DisplayName = "Get Next Attribute Change Time")
	float K2_GetNextAttributeChangeTime(const FGameplayAttribute Attribute) const;
	float GetNextAttributeChangeTime(const FGameplayAttribute& Attribute) const;
	
	UFUNCTION(BlueprintPure, Category = "Gameplay Utilities")
	float GetActiveEffectLevel(struct FActiveGameplayEffectHandle ActiveHandle) const;

	UFUNCTION(BlueprintCallable, Category = "Wa Combat", DisplayName = "Add Event Tag To Ignore List")
	FGameplayTagContainer K2_AddEventTagToIgnoreList(FGameplayTag EventTag);
	FGameplayTagContainer AddEventTagToIgnoreList(const FGameplayTag& EventTag);
	UFUNCTION(BlueprintCallable, Category = "Wa Combat", DisplayName = "Remove Event Tag From Ignore List")
	void K2_RemoveEventTagFromIgnoreList(FGameplayTag EventTag);
	void RemoveEventTagFromIgnoreList(const FGameplayTag& EventTag);

	bool OverrideMovementImmersionDepth() const;
	float GetAbilityForcedMovementSpeed() const;
	bool HasAbilityForcedMovementSpeed() const;
	bool HasAbilityVerticalControlInWater() const;
	bool HasAbilityForcedRotation() const;
	FRotator GetAbilityForcedRotation() const;

	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	void FinishAbilityWithMontage(UAnimMontage* Montage);

	// Get an active GameplayAbility with the specified montage as it's current montage.
	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	UPOTGameplayAbility* GetActiveAbilityFromMontage(const UAnimMontage* Montage) const;

	// Get an active GameplayAbility with the specified spec.
	UPOTGameplayAbility* GetActiveAbilityFromSpec(const FGameplayAbilitySpec* Spec) const;

	// Get an active GameplayAbility with the specified spec handle.
	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	UPOTGameplayAbility* GetActiveAbilityFromSpecHandle(FGameplayAbilitySpecHandle Handle);

	// Get an active GameplayAbility with the specified class default object.
	UFUNCTION(BlueprintCallable, Category = "Wa Combat")
	UPOTGameplayAbility* GetActiveAbilityFromCDO(const UPOTGameplayAbility* CDO) const;

	// Used to apply a GameplayEffect that requires the amount of damage dealt to the victim of an attack as input.
	void ApplyPostDamageEffect(const TSubclassOf<UGameplayEffect> EffectClass, float NetDamage);
protected:
	//This value is used because ConsequentAttacks can (and has to) loop around, to make sure the correct
	//damage values etc. are being fetched. This value however is used in the chain attack chance as it
	//doesn't loop.
	int32 TotalConsequentAttacks;
	float TimeInCurrentAttackAbility;
	float TimeBelowThreshold;
	FVector OldLocation;
	
	ELocomotionState CurrentLocomotionState;

	FActiveGameplayEffectHandle LocomotionStateEffect;
	FActiveGameplayEffectHandle CarryLocomotionEffect;
	FActiveGameplayEffectHandle StanceStateEffect;
	FActiveGameplayEffectHandle ExhaustionEffect;

	FActiveGameplayEffectHandle SkinMeshEffect;
	bool IsExhausted() const;

protected:
	

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void InitializeComponent() override;

	virtual void AddGameplayTagToOwner(const FGameplayTag& InTag, const bool bFast = false);
	virtual void RemoveGameplayTagFromOwner(const FGameplayTag& InTag);

	void SetCurrentAttackAbilityUsesCharge(bool bShouldUseChargeDuration);

	void SetCurrentAttackingAbility(UPOTGameplayAbility* Ability);

	void AddNonExclusiveAbility(UPOTGameplayAbility* Ability);
	void RemoveNonExclusiveAbility(UPOTGameplayAbility* Ability);
	UPROPERTY()
	TArray<UPOTGameplayAbility*> NonExclusiveAbilities;

	bool GetActiveEffectsNextPeriodAndDuration(const FGameplayEffectQuery& Query, float& NextPeriod, float& Duration) const;

	void OnGrowthChanged(const FOnAttributeChangeData& ChangeData);

	void OnStaminaChanged(const FOnAttributeChangeData& ChangeData);

	virtual void UpdateLocomotionEffects();

	void ApplyLocomotionEffect(FActiveGameplayEffectHandle& CurrentEffectHandle, const UGameplayEffect* const NewEffect);

	virtual void OnActiveGameplayEffectAddedCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle);

private:
	UPROPERTY(Transient)
	USkeletalMeshComponent* OwnerMesh;
	UPROPERTY(Transient)
	UPrimitiveComponent* OwnerCollision;


	UPROPERTY(Transient)
	UPOTGameplayAbility* CurrentAttackAbility;

	float LastPeriodEventTime;

	bool bCanFireDamageFinishedEvent;

	// When parsing EventTags in HandleGameplayEvent if EventTag is found inside this container
	// it will not fire event
	FGameplayTagContainer EventsToIgnore;
private:

	FTimerHandle LocomotionUpdateTimerHandle;

	friend class UPOTGameplayAbility;
};
