// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

// ----------------------------------------------------------------------------------------------------------------
// This header is for Ability-specific structures and enums that are shared across a project
// Every game will probably need a file like this to handle their extensions to the system
// This file is a good place for subclasses of FGameplayEffectContext and FGameplayAbilityTargetData
// ----------------------------------------------------------------------------------------------------------------

#include "GameplayEffectTypes.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "POTTargetType.h"
#include "Engine/CurveTable.h"
#include "ITypes.h"
#include "GameplayEffect.h"
#include "Misc/EnumClassFlags.h"
#include "POTAbilityTypes.generated.h"


class UGameplayEffect;
class UPOTTargetType;


USTRUCT(BlueprintType)
struct FPOTGameplayEffectEntry

{
	GENERATED_BODY()

public:
	/** List of gameplay effects to apply to the targets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TArray<TSubclassOf<UGameplayEffect>> TargetGameplayEffectClasses;

	/** Tag requirements for the source of these GEs */
	UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	FGameplayTagRequirements	SourceTags;
	/** Tag requirements for the target of these GEs */
	//These were removed because if these are in, targets need to be stored per GE set instead of shared for the
	//entire application. This is a lot more computationally expensive and if it's not needed, no need to implement it
	/*UPROPERTY(EditDefaultsOnly, Category = GameplayModifier)
	FGameplayTagRequirements	TargetTags;*/

public:

	FPOTGameplayEffectEntry()
	{
	}

	FPOTGameplayEffectEntry(const TArray<TSubclassOf<UGameplayEffect>>& InEffectsArray)
		: TargetGameplayEffectClasses(InEffectsArray)
	{
	}
	
};

USTRUCT(BlueprintType)
struct FPOTGameplayEffectContainerEntry
{

	GENERATED_BODY()

public:
	/** Sets the way that targeting happens */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TSubclassOf<UPOTTargetType> TargetType;

	/** List of gameplay effects to apply to the targets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TArray<FPOTGameplayEffectEntry> TargetGameplayEffects;

public:
	FPOTGameplayEffectContainerEntry()
		: TargetType(UPOTTargetType_UseEventData::StaticClass())
	{

	}

	FPOTGameplayEffectContainerEntry(const TSubclassOf<UPOTTargetType> InTargetType,
		const TArray<FPOTGameplayEffectEntry>& InTargetGameplayEffects)
		: TargetType(InTargetType)
		, TargetGameplayEffects(InTargetGameplayEffects)
	{
	}

	FPOTGameplayEffectContainerEntry(const TArray<TSubclassOf<UGameplayEffect>>& InEffectsArray)
		: TargetType(UPOTTargetType_UseEventData::StaticClass())
	{
		TargetGameplayEffects.Emplace(FPOTGameplayEffectEntry(InEffectsArray));
	}

};

/**
 * Struct defining a list of gameplay effects, a tag, and targeting info
 * These containers are defined statically in blueprints or assets and then turn into Specs at runtime
 */
USTRUCT(BlueprintType)
struct FPOTGameplayEffectContainer
{
	GENERATED_BODY()

public:
	FPOTGameplayEffectContainer() 
	{}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TArray<FPOTGameplayEffectContainerEntry> ContainerEntries;


	bool GetGameplayCueTags(FGameplayTagContainer& OutCueTags) const;
};

/** A "processed" version of RPGGameplayEffectContainer that can be passed around and eventually applied */
USTRUCT(BlueprintType)
struct FPOTGameplayEffectContainerSpec
{
	GENERATED_BODY()

public:
	FPOTGameplayEffectContainerSpec() {}

	/** Computed target data */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	FGameplayAbilityTargetDataHandle TargetData;

	/** List of gameplay effects to apply to the targets */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GameplayEffectContainer)
	TArray<FGameplayEffectSpecHandle> TargetGameplayEffectSpecs;

	/** Returns true if this has any valid effect specs */
	bool HasValidEffects() const;

	/** Returns true if this has any valid targets */
	bool HasValidTargets() const;

	/** Adds new targets to target data */
	void AddTargets(const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors, const bool bReset = false);

	UPROPERTY(Transient)
	TSubclassOf<UPOTTargetType> TargetType;
};

UENUM(meta = (Bitflags))
enum class EGEContextRepBits : uint16
{
	None = 0,
	Instigator = 1 << 0,
	EffectCauser = 1 << 1,
	AbilityCDO = 1 << 2,
	SourceObject = 1 << 3,
	Actors = 1 << 4,
	HitResult = 1 << 5,
	WorldOrigin = 1 << 6,
	Direction = 1 << 7,
	EventMagnitude = 1 << 8,
	DamageType = 1 << 9,
	BrokeBone = 1 << 10,
	KnockbackMode = 1 << 11,
	KnockbackForce = 1 << 12,
	ContextTag = 1 << 13,
	Max = 1 << 15
};
ENUM_CLASS_FLAGS(EGEContextRepBits);

USTRUCT()
struct FPOTGameplayEffectContext : public FGameplayEffectContext

{
	GENERATED_USTRUCT_BODY()
	
public:

	FPOTGameplayEffectContext()
		: Super()
	{
		KnockbackMode = EKnockbackMode::KM_None;
		KnockbackForce = 0.f;
		Direction = FVector::ZeroVector;
		bHasDirection = false;
		EventMagnitude = 1.f;
		DamageType = EDamageType::DT_GENERIC;
		bBrokeBone = false;
		ChargedDuration = 0;
		MovementSpeed = 0;
	}

	virtual UScriptStruct* GetScriptStruct() const
	{
		return FPOTGameplayEffectContext::StaticStruct();
	}

	/** Creates a copy of this context, used to duplicate for later modifications */
	virtual FGameplayEffectContext* Duplicate() const override
	{
		FPOTGameplayEffectContext* NewContext = new FPOTGameplayEffectContext();
		*NewContext = *this;
		NewContext->AddActors(Actors);
		if (GetHitResult())
		{
			// Does a deep copy of the hit result
			NewContext->AddHitResult(*GetHitResult(), true);
		}

		NewContext->EventMagnitude = EventMagnitude;
		NewContext->DamageType = DamageType;
		NewContext->bBrokeBone = bBrokeBone;

		NewContext->KnockbackForce = KnockbackForce;
		NewContext->KnockbackMode = KnockbackMode;

		NewContext->ChargedDuration = ChargedDuration;

		return NewContext;
	}


	const FVector& GetDirection() const
	{
		return Direction;
	}

	float GetEventMagnitude() const
	{
		return EventMagnitude;
	}

	FPOTDamageInfo GetDamageInfo() const
	{
		FPOTDamageInfo OutInfo;

		OutInfo.DamageType = GetDamageType();
		OutInfo.bBoneBreak = GetBrokeBone();

		return OutInfo;
	}

	EDamageType GetDamageType() const
	{
		return DamageType;
	}

	bool GetBrokeBone() const
	{
		return bBrokeBone;
	}

	EKnockbackMode GetKnockbackMode() const
	{
		return KnockbackMode;
	}

	float GetKnockbackForce() const
	{
		return KnockbackForce;
	}

	float GetChargedDuration() const
	{
		return ChargedDuration;
	}

	float GetMovementSpeed() const
	{
		return MovementSpeed;
	}

	float GetSmoothMovementSpeed() const
	{
		return SmoothMovementSpeed;
	}

	void SetDirection(const FVector& InDirection)
	{
		bHasDirection = true;
		Direction = InDirection;
	}

	void SetEventMagnitude(const float Magnitude)
	{
		EventMagnitude = Magnitude;
	}

	void SetDamageType(const EDamageType InDamageType)
	{
		DamageType = InDamageType;
	}

	void SetBrokeBone(bool bBroke)
	{
		bBrokeBone = bBroke;
	}

	void SetKnockbackData(EKnockbackMode InMode, float InForce)
	{
		KnockbackMode = InMode;
		KnockbackForce = InForce;
	}

	void SetKnockbackMode(EKnockbackMode InMode)
	{
		KnockbackMode = InMode;
	}

	void SetKnockbackForce(float InForce)
	{
		KnockbackForce = InForce;
	}

	void SetContextTag(const FGameplayTag& Tag)
	{
		ContextTag = Tag;
	}

	void SetChargedDuration(float Duration)
	{
		ChargedDuration = Duration;
	}

	void SetMovementSpeed(float Speed)
	{
		MovementSpeed = Speed;
	}

	void SetSmoothMovementSpeed(float Speed)
	{
		SmoothMovementSpeed = Speed;
	}

	/** Custom serialization, subclasses must override this */
	virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;
	virtual void GetOwnedGameplayTags(OUT FGameplayTagContainer& ActorTagContainer, OUT FGameplayTagContainer& SpecTagContainer) const;

protected:
	UPROPERTY()
	FVector Direction;

	UPROPERTY()
	bool bHasDirection;

	//If this came from a gameplay event, the magnitude of the event will be here
	UPROPERTY()
	float EventMagnitude;

	UPROPERTY()
	EDamageType DamageType;

	UPROPERTY()
	bool bBrokeBone;

	UPROPERTY()
	EKnockbackMode KnockbackMode = EKnockbackMode::KM_None;

	UPROPERTY()
	float KnockbackForce;

	UPROPERTY()
	FGameplayTag ContextTag;

	// How long the ability was held before released
	UPROPERTY()
	float ChargedDuration;

	UPROPERTY()
	float MovementSpeed;

	UPROPERTY()
	float SmoothMovementSpeed;
};

template<>
struct TStructOpsTypeTraits< FPOTGameplayEffectContext > : public TStructOpsTypeTraitsBase2< FPOTGameplayEffectContext >
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true		// Necessary so that TSharedPtr<FHitResult> Data is copied around
	};
};

USTRUCT(BlueprintType)
struct FTimedTraceBoneGroup
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	float MontageTime;

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	TArray<FName> LocalBones;

public:
	FTimedTraceBoneGroup()
		: MontageTime(0.f)
		, LocalBones(TArray<FName>())
	{
	}

	FTimedTraceBoneGroup(const float InMontageTime, const TArray<FName>& InTransforms)
		: MontageTime(InMontageTime)
		, LocalBones(InTransforms)
	{
	}

	friend bool operator==(const FTimedTraceBoneGroup& LHS, const FTimedTraceBoneGroup& RHS)
	{
		return LHS.MontageTime == RHS.MontageTime && LHS.LocalBones.Num() == RHS.LocalBones.Num();
	}

	friend bool operator!=(const FTimedTraceBoneGroup& LHS, const FTimedTraceBoneGroup& RHS)
	{
		return LHS.MontageTime != RHS.MontageTime || LHS.LocalBones.Num() != RHS.LocalBones.Num();
	}
};

USTRUCT(BlueprintType)
struct FTimedTraceBoneSection
{

	GENERATED_BODY()

	public:
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	TArray<FTimedTraceBoneGroup> TimedTraceBoneGroups;
};

USTRUCT(BlueprintType)
struct FWeaponSlotConfiguration
{
	GENERATED_BODY()
public:
	// The event tag for this hit. This is passed down to the gameplay event when the weapon hits something.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Event"))
	FGameplayTag EventGameplayTag;

	// This is useful for turning off physics bodies that you don't want to be damage tracing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Damage Body Filter"))
	TArray<FName> DmgBodyFilter;

	// Should be used when the bone, which will be tracing, is being manipulated manually (e.g. in animgraph "modify bone") 
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bUseSocketRuntimeOffset;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCanSendRevengeEvents;

	FWeaponSlotConfiguration(
		const bool bInUsesOwnerPhysicsBody, const FGameplayTag& InWeaponSlot,
		const FGameplayTag& InEventTag, const TArray<FName>& InDamageBodyFilter, bool bInUseSocketRuntimeOffset)
		: EventGameplayTag(InEventTag)
		, DmgBodyFilter(InDamageBodyFilter)
		, bUseSocketRuntimeOffset(bInUseSocketRuntimeOffset)
		, bCanSendRevengeEvents(true)
	{}

	FWeaponSlotConfiguration()
		: EventGameplayTag(FGameplayTag::RequestGameplayTag(NAME_EventMontageSharedWeaponHit))
		, bUseSocketRuntimeOffset(true)
		, bCanSendRevengeEvents(true)
	{
		DmgBodyFilter.Reserve(1);
	}
};

//Overlap Notify Objects
USTRUCT(BlueprintType)
struct FAnimOverlapConfiguration
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Damage Configuration"))
	FWeaponDamageConfiguration DamageConfiguration;

	//Used to determine the location and radius of the overlap shape
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Overlap Location Information"))
	TSubclassOf<UPOTTargetType> AOETargetType;

	//NOTE: if you change this value, also update the "ClampMin" value for the OverlapRecordInterval variable directly below.
	static constexpr float MaxAnimOverlapInterval = 0.04f;

	//Timing interval to determine when to check for overlaps after this notify event begins
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.04", DisplayName = "Overlap Record Interval"))
	float OverlapRecordInterval;

	//Force an overlap check when the notify ends
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Check Overlap on Notify End"))
	bool bCheckOverlapOnNotifyEnd;

	FAnimOverlapConfiguration(
		const FWeaponDamageConfiguration& InDamageConfiguration, const float InOverlapRecordInterval, const bool bInCheckOverlapOnNotifyEnd)
		: DamageConfiguration(InDamageConfiguration)
		, OverlapRecordInterval(InOverlapRecordInterval)
		, bCheckOverlapOnNotifyEnd(bInCheckOverlapOnNotifyEnd)
	{}

	FAnimOverlapConfiguration()
		: DamageConfiguration()
		, OverlapRecordInterval(MaxAnimOverlapInterval)
		, bCheckOverlapOnNotifyEnd(false)
	{}
};

USTRUCT(BlueprintType)
struct FTimedOverlapConfiguration
{
	GENERATED_BODY()

public:
	//Configuration is found in the animation's Shape Overlap Notify State
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	FAnimOverlapConfiguration AnimOverlapConfiguration;

	//Overlap Trigger Timestamps are generated from the animation's Shape Overlap Notify State
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly)
	TArray<float> OverlapTriggerTimestamps;

	int LastTriggeredIndex = -1;

	void ResetTriggerIndex()
	{
		LastTriggeredIndex = -1;
	};

	//Checks montage time to see if the next timestamp should be triggered
	bool TriggerNextTimestamp(float CurrentTime)
	{
		bool bNextTriggerFound = false;
		const int StartingIndex = LastTriggeredIndex + 1;
		for (int i = StartingIndex; i < OverlapTriggerTimestamps.Num(); i++)
		{
			if (OverlapTriggerTimestamps[i] > CurrentTime)
			{
				break;
			}

			LastTriggeredIndex = i;
			bNextTriggerFound = true;
		}

		return bNextTriggerFound;
	};
};