// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Quests/IPOI.h"
#include "Player/IBaseCharacter.h"
#include "Quests/IQuestManager.h"
#include "IWorldSettings.h"
#include "Player/IPlayerController.h"

AIPOI::AIPOI(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	if (Mesh)
	{
		RootComponent = Mesh;
		Mesh->bHiddenInGame = true;
		Mesh->SetVisibility(true, true);
		Mesh->SetCollisionProfileName(TEXT("POI"), false);
		Mesh->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
		Mesh->SetGenerateOverlapEvents(true);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
		Mesh->SetEnableGravity(false);
	}
}

void AIPOI::PostInitializeComponents()
{
	Super::PostInitializeComponents();

#if UE_SERVER
	if (IsRunningDedicatedServer())
	{
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
#else
	OnActorBeginOverlap.AddDynamic(this, &AIPOI::PointOverlapBeginClient);
	OnActorEndOverlap.AddDynamic(this, &AIPOI::PointOverlapEndClient);
#endif
}

void AIPOI::BeginPlay()
{
	Super::BeginPlay();

	if (bDisabled)
	{
		Destroy();
	}
}

void AIPOI::PointOverlapBeginClient(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPOI::PointOverlapBeginClient | %s, %s"), *OtherActor->GetName(), *LocationDisplayName.ToString()));

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
				IPlayerController->ServerNotifyPOIOverlap(true, this);
			}
			else // can happen during login if Controller hasn't possessed character or replicated yet
			{
				// try again after a delay
				IBaseCharacter->DelayedServerNotifyPOIOverlap(this);
			}
		}
	}
}

void AIPOI::PointOverlapEndClient(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPOI::PointOverlapEndClient | %s, %s"), *OtherActor->GetName(), *LocationDisplayName.ToString()));

	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(OtherActor);
	if (IBaseCharacter && IBaseCharacter->IsAlive())
	{
		// Do not update location if character is teleporting
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
				IPlayerController->ServerNotifyPOIOverlap(false, this);
			}
		}
	}
}

void AIPOI::PointOverlapBeginServer(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPOI::PointOverlapBeginServer | %s, %s"), *OtherActor->GetName(), *LocationDisplayName.ToString()));
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

void AIPOI::PointOverlapEndServer(AActor* OverlappedActor, AActor* OtherActor)
{
	// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIPOI::PointOverlapEndServer | %s, %s"), *OtherActor->GetName(), *LocationDisplayName.ToString()));
	// Only Progress Quest Checks on the Server

	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(OtherActor);
	if (IBaseCharacter && IBaseCharacter->IsAlive())
	{
		IBaseCharacter->LocationDisplayName = LocationDisplayName;

		// When teleporting, a new location can be entered before the old location is departed, make sure character location matches this poi before updating
		if (LocationDisplayName.EqualToCaseIgnored(IBaseCharacter->LocationDisplayName))
		{
			if (!IBaseCharacter->IsTeleporting())
			{
				IBaseCharacter->LastLocationTag = LocationTag;
				IBaseCharacter->LastLocationEntered = false;
			}
		}

		if (AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager)
		{
			QuestMgr->OnLocationDiscovered(LocationTag, LocationDisplayName, IBaseCharacter, false, true, IBaseCharacter->IsTeleporting());
		}
	}
}
