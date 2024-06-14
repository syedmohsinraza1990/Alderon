// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// PhysicsVolume:  a bounding volume which affects actor physics
// Each  AActor  is affected at any time by one PhysicsVolume
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Engine/TriggerSphere.h"
#include "IPointOfInterest.generated.h"

/**
 * Volume that causes damage over time to any Actor that overlaps its collision.
 */
UCLASS()
class PATHOFTITANS_API AIPointOfInterest : public ATriggerSphere
{
	GENERATED_UCLASS_BODY()

public:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION()
	void PointOverlapBeginClient(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void PointOverlapEndClient(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void PointOverlapBeginServer(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	void PointOverlapEndServer(AActor* OverlappedActor, AActor* OtherActor);

	FText GetLocationDisplayName() { return LocationDisplayName; };

	FName GetLocationTag() { return LocationTag; };

	bool IsDisabled() { return bDisabled; };

	// Exploration quests will only consider this POI if the Character has any of these Tags, or there are no tags.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = POI)
	TArray<FName> RequiredExplorationQuestTags;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = POI)
	FName LocationTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = POI)
	FText LocationDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = POI)
	bool bDisabled = false;
};



