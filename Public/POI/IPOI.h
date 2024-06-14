// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// PhysicsVolume:  a bounding volume which affects actor physics
// Each  AActor  is affected at any time by one PhysicsVolume
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IPOI.generated.h"

class UStaticMeshComponent;

UCLASS(ClassGroup = Common)
class PATHOFTITANS_API AIPOI : public AActor
{
	GENERATED_UCLASS_BODY()

private:
	/** The main skeletal mesh associated with this Character (optional sub-object). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	UStaticMeshComponent* Mesh;

public:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	UFUNCTION()
	virtual void PointOverlapBeginClient(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	virtual void PointOverlapEndClient(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	virtual void PointOverlapBeginServer(AActor* OverlappedActor, AActor* OtherActor);

	UFUNCTION()
	virtual void PointOverlapEndServer(AActor* OverlappedActor, AActor* OtherActor);

	FText GetLocationDisplayName() { return LocationDisplayName; };

	FName GetLocationTag() { return LocationTag; };

	bool IsDisabled() { return bDisabled; };

	const UStaticMeshComponent* GetMesh() const
	{
		return Mesh;
	}

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



