// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Quests/IPointOfInterest.h"
#include "Player/IBaseCharacter.h"
#include "Quests/IQuestManager.h"
#include "Components/ShapeComponent.h"
#include "IWorldSettings.h"
#include "Player/IPlayerController.h"

AIPointOfInterest::AIPointOfInterest(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	if (UShapeComponent* CollisionComp = GetCollisionComponent())
	{
		CollisionComp->SetCollisionProfileName(TEXT("POI"), false);
		CollisionComp->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
		CollisionComp->SetGenerateOverlapEvents(true);
		CollisionComp->SetCanEverAffectNavigation(false);
	}
}

void AIPointOfInterest::PostInitializeComponents()
{
	Super::PostInitializeComponents();

}

void AIPointOfInterest::BeginPlay()
{
	Super::BeginPlay();

	if (bDisabled)
	{
		Destroy();
		return;
	}

#if UE_SERVER
	if (IsRunningDedicatedServer())
	{
		if (UShapeComponent* CollisionComp = GetCollisionComponent())
		{
			CollisionComp->SetGenerateOverlapEvents(false);
			CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
#else
	OnActorBeginOverlap.AddDynamic(this, &AIPointOfInterest::PointOverlapBeginClient);
	OnActorEndOverlap.AddDynamic(this, &AIPointOfInterest::PointOverlapEndClient);
#endif
}

void AIPointOfInterest::PointOverlapBeginClient(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPointOfInterest::PointOverlapBeginClient | %s, %s"), OtherActor ? *OtherActor->GetName() : TEXT("NULL"), *LocationDisplayName.ToString()));
	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(OtherActor);
	if (IBaseCharacter && IBaseCharacter->IsAlive())
	{
		IBaseCharacter->LocationDisplayName = LocationDisplayName;
		bool bLocalServer = (IBaseCharacter->IsLocallyControlled() && HasAuthority());
		IBaseCharacter->LastLocationTag = LocationTag;
		IBaseCharacter->LastLocationEntered = true;

		if (bLocalServer)
		{
			if (AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager)
			{
				QuestMgr->OnLocationDiscovered(LocationTag, LocationDisplayName, IBaseCharacter, true, true, IBaseCharacter->IsTeleporting());
			}
		}
		else
		{
			if (AIPlayerController* IPlayerController = Cast<AIPlayerController>(IBaseCharacter->GetController()))
			{
				IPlayerController->ServerNotifyPointOfInterestOverlap(true, this);
			}
		}
	}
}

void AIPointOfInterest::PointOverlapEndClient(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPointOfInterest::PointOverlapEndClient | %s, %s"), *OtherActor->GetName(), *LocationDisplayName.ToString()));

	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(OtherActor);
	if (IBaseCharacter && IBaseCharacter->IsAlive())
	{
		const bool bLocalServer = (IBaseCharacter->IsLocallyControlled() && HasAuthority());

		if (!IBaseCharacter->IsTeleporting())
		{
			IBaseCharacter->LocationDisplayName = LocationDisplayName;
			IBaseCharacter->LastLocationTag = LocationTag;
			IBaseCharacter->LastLocationEntered = false;
		}

		if (bLocalServer)
		{
			if (AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager)
			{
				QuestMgr->OnLocationDiscovered(LocationTag, LocationDisplayName, IBaseCharacter, false, true, IBaseCharacter->IsTeleporting());
			}
		}
		else
		{
			if (AIPlayerController* IPlayerController = Cast<AIPlayerController>(IBaseCharacter->GetController()))
			{
				IPlayerController->ServerNotifyPointOfInterestOverlap(false, this);
			}
		}
	}
}

void AIPointOfInterest::PointOverlapBeginServer(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPointOfInterest::PointOverlapBeginServer | %s, %s"), OtherActor ? *OtherActor->GetName() : TEXT("NULL"), *LocationDisplayName.ToString()));
	// Only Progress Quest Checks on the Server

	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(OtherActor);
	if (IBaseCharacter && IBaseCharacter->IsAlive())
	{
		IBaseCharacter->LocationDisplayName = LocationDisplayName;
		IBaseCharacter->LastLocationTag = LocationTag;
		IBaseCharacter->LastLocationEntered = true;

		if (AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager)
		{
			QuestMgr->OnLocationDiscovered(LocationTag, LocationDisplayName, IBaseCharacter, true, true, IBaseCharacter->IsTeleporting());
		}
	}
}

void AIPointOfInterest::PointOverlapEndServer(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPointOfInterest::PointOverlapEndServer | %s, %s"), *OtherActor->GetName(), *LocationDisplayName.ToString()));
	// Only Progress Quest Checks on the Server

	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(OtherActor);
	if (IBaseCharacter && IBaseCharacter->IsAlive())
	{
		if (!IBaseCharacter->IsTeleporting())
		{
			IBaseCharacter->LocationDisplayName = LocationDisplayName;
			IBaseCharacter->LastLocationTag = LocationTag;
			IBaseCharacter->LastLocationEntered = false;
		}

		if (AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager)
		{
			QuestMgr->OnLocationDiscovered(LocationTag, LocationDisplayName, IBaseCharacter, false, true, IBaseCharacter->IsTeleporting());
		}
	}
}
