// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Abilities/GameplayAbilityTypes.h"
#include "POTTargetType.generated.h"

class AActor;
struct FGameplayEventData;


/**
 * Class that is used to determine targeting for abilities
 * It is meant to be blueprinted to run target logic
 * This does not subclass GameplayAbilityTargetActor because this class is never instanced into the world
 * This can be used as a basis for a game-specific targeting blueprint
 * If your targeting is more complicated you may need to instance into the world once or as a pooled actor
 */
UCLASS(Blueprintable, meta = (ShowWorldContextPin))
class PATHOFTITANS_API UPOTTargetType : public UObject
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UPOTTargetType() {}

	/** Called to determine targets to apply gameplay effects to */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void GetTargets(AActor* TargetingActor, 
		const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults, 
		TArray<AActor*>& OutActors) const;

};

/** Trivial target type that uses the owner */
UCLASS(NotBlueprintable)
class PATHOFTITANS_API UWaTargetType_UseOwner : public UPOTTargetType
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UWaTargetType_UseOwner() {}

	/** Uses the passed in event data */
	virtual void GetTargets_Implementation(AActor* TargetingActor, 
		const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults,
		TArray<AActor*>& OutActors) const override;
};

/** Trivial target type that pulls the target out of the event data */
UCLASS(NotBlueprintable)
class PATHOFTITANS_API UPOTTargetType_UseEventData : public UPOTTargetType
{
	GENERATED_BODY()

public:
	// Constructor and overrides
	UPOTTargetType_UseEventData() {}

	/** Uses the passed in event data */
	virtual void GetTargets_Implementation(AActor* TargetingActor, 
		const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults,
		TArray<AActor*>& OutActors) const override;
};


/** Trivial target type that gets the event instigator */
UCLASS(NotBlueprintable)
class PATHOFTITANS_API UPOTTargetType_UseEventInstigator : public UPOTTargetType
{
	GENERATED_BODY()

	public:
	// Constructor and overrides
		UPOTTargetType_UseEventInstigator()
	{
	}

	/** Uses the passed in event data */
	virtual void GetTargets_Implementation(AActor* TargetingActor,
		const FGameplayEventData& EventData, TArray<FHitResult>& OutHitResults,
		TArray<AActor*>& OutActors) const override;
};
