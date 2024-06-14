// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Quests/IQuestManager.h"
#include "Engine/AssetManager.h"
#include "Quests/IQuest.h"
#include "Player/IBaseCharacter.h"
#include "Player/IPlayerController.h"
#include "Online/IPlayerState.h"
#include "IGameplayStatics.h"
#include "GameMode/IGameMode.h"
#include "Online/IGameState.h"
#include "TitanAssetManager.h"
#include "AI/Fish/IFish.h"
#include "ITypes.h"
#include "Abilities/POTAbilitySystemGlobals.h"
#include "Quests/IPointOfInterest.h"
#include "AI/Fish/IFishSpawner.h"
#include "Online/IPlayerGroupActor.h"
#include "World/IWaterManager.h"
#include "World/SaveGame_QuestContributions.h"
#include "Online/IGameSession.h"
#include "World/IWaystoneManager.h"
#include "IGameInstance.h"
#include "IWorldSettings.h"
#include "EngineUtils.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "Stats/IStats.h"
#include "CaveSystem/IHatchlingCave.h"
#include "CaveSystem/IPlayerCaveBase.h"
#include "Quests/IPOI.h"
#include "Critters/ICritterPawn.h"
#include "AlderonCritterController.h"
#include "UI/IGameHUD.h"

AIQuestManager::AIQuestManager()
{
	//PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(false);
	bAlwaysRelevant = true;

	NetUpdateFrequency = 5.0f;

#if WITH_EDITORONLY_DATA
	bIsSpatiallyLoaded = false;
#endif
}

void AIQuestManager::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);

		if (AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession()))
		{
			SetMaxUnclaimedRewards(Session->MaxUnclaimedRewards);
			SetShouldEnableMaxUnclaimedRewards(Session->bEnableMaxUnclaimedRewards);
			SetShouldEnableTrophyQuests(Session->bTrophyQuests);

			if (Session->bOverrideQuestContributionCleanup)
			{
				QuestContributionCleanupDelay = Session->QuestContributionCleanup;
			}

			if (Session->bOverrideLocalQuestCooldown)
			{
				LocalQuestCooldownDelay = Session->LocalQuestCooldown;
			}

			if (Session->bOverrideLocationQuestCooldown)
			{
				LocationCompletedCleanupDelay = Session->LocationQuestCooldown;
			}

			if (Session->bOverrideGroupQuestCleanup)
			{
				GroupQuestCleanupDelay = Session->GroupQuestCleanup;
			}

			if (Session->bOverrideTrophyQuestCooldown)
			{
				TrophyQuestCooldownDelay = Session->TrophyQuestCooldown;
			}

			if (Session->bOverrideGroupMeetQuestCooldown)
			{
				GroupMeetQuestCooldownDelay = Session->GroupMeetQuestCooldown;
			}

			if (Session->bOverrideMaxCompleteQuestsInLocation)
			{
				SetMaxCompleteQuestsInLocation(Session->MaxCompleteQuestsInLocation);
			}
		}

		FSlowHeartBeatScope SuspendHeartBeat;

		UE_LOG(TitansQuests, Log, TEXT("QuestManager: Preloading Quests and Quest Tasks for Dedicated Server Usage"));

		LoadQuestAssets();

		TArray<AActor*> POIs;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIPointOfInterest::StaticClass(), POIs);

		for (int32 i = 0; i < POIs.Num(); i++)
		{
			if (AIPointOfInterest* POI = Cast<AIPointOfInterest>(POIs[i]))
			{
				AllPointsOfInterest.Add(POI);
			}
		}

		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIPOI::StaticClass(), POIs);
		AllPointsOfInterest.Reserve(POIs.Num());

		for (int32 i = 0; i < POIs.Num(); i++)
		{
			if (AIPOI* POI = Cast<AIPOI>(POIs[i]))
			{
				AllPointsOfInterest.Add(POI);
			}
		}

		TArray<AActor*> FishSpawners;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), AIFishSpawner::StaticClass(), FishSpawners);
		FishSpawnLocations.Reserve(FishSpawners.Num());

		for (int32 i = 0; i < FishSpawners.Num(); i++)
		{
			if (AIFishSpawner* FishSpawner = Cast<AIFishSpawner>(FishSpawners[i]))
			{
				FishSpawnLocations.Add(FishSpawner);
			}
		}

		// Preload All Quest Data on dedicated servers
		// this avoid quest data blocking the main thread

		// Start Quest Countdown Timers
		LoadContributionData();

		float TockTime = (UE_BUILD_SHIPPING) ? 60.f : 10.0f;
		GetWorldTimerManager().SetTimer(TimerHandle_QuestTick, this, &AIQuestManager::QuestTick, 1.0f, true);
		GetWorldTimerManager().SetTimer(TimerHandle_QuestTock, this, &AIQuestManager::QuestTock, TockTime, true);
		GetWorldTimerManager().SetTimer(TimerHandle_ContributionTick, this, &AIQuestManager::ContributionTick, TockTime, true);

		if (LocalQuestCooldownDelay > 0.0f || GroupMeetQuestCooldownDelay > 0.0f || GroupQuestCleanupDelay > 0.0f || LocationCompletedCleanupDelay > 0.0f)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_CooldownTick, this, &AIQuestManager::CooldownTick, TockTime, true);
		}
	}
}

void AIQuestManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	if (HasAuthority())
	{
		SaveContributionData();
	}
}

void AIQuestManager::LoadContributionData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::LoadContributionData"))

	const USaveGame_QuestContributions* const QuestContributionsSave = Cast<USaveGame_QuestContributions>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(), 0));
	if (QuestContributionsSave && QuestContributionsSave->Version == IAlderonCommon::Get().GetFullVersion())
	{
		GetQuestContributions_Mutable() = QuestContributionsSave->SavedQuestContributions;
		ActiveWaterRestorationQuestTags = QuestContributionsSave->SavedWaterRestorationQuestTags;
		ActiveWaystoneRestoreQuestTags = QuestContributionsSave->SavedWaystoneRestoreQuestTags;
	}

	// Ignore timestamps from a save
	// TODO: (Erlite) Note: this makes a copy, the code below does literally nothing. As such, I disabled it. Kept it for future review though.
	// TArray<FQuestContribution> RelevantQuestContributions = QuestContributions; <--- COPY, NOT BY REF, BAD
	// TArray<FPrimaryAssetId> GroupQuests = GetLoadedQuestAssetIDsFromType(EQuestFilter::GROUP_ALL, nullptr);
	// for (int32 i = RelevantQuestContributions.Num(); i-- > 0;)
	// {
	// 	bool bIsGroupContribution = false;
	// 	FQuestContribution& QuestContribution = RelevantQuestContributions[i];
	//
	// 	for (FPrimaryAssetId GroupQuest : GroupQuests)
	// 	{
	// 		if (QuestContribution.QuestId == GroupQuest)
	// 		{
	// 			RelevantQuestContributions.RemoveAt(i);
	// 			bIsGroupContribution = true;
	// 			break;
	// 		}
	// 	}
	//
	// 	if (bIsGroupContribution) continue;
	//
	// 	if (QuestContribution.Timestamp > 1.0f)
	// 	{
	// 		QuestContribution.Timestamp = 1.0f;
	// 	}
	// }
}

void AIQuestManager::SaveContributionData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::SaveContributionData"))

	FQuestsIDsLoaded QuestsDataloaded;
	TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
	QuestsDataloaded.BindLambda([WeakThis](TArray<FPrimaryAssetId> QuestAssetIds)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnSaveContributionData(QuestAssetIds);
		}
	});

	GetLoadedQuestAssetIDsFromType(EQuestFilter::GROUP_ALL, nullptr, QuestsDataloaded);
}

void AIQuestManager::OnSaveContributionData(TArray<FPrimaryAssetId> QuestAssetIds)
{
	USaveGame_QuestContributions* QuestContributionsSave = Cast<USaveGame_QuestContributions>(UGameplayStatics::CreateSaveGameObject(USaveGame_QuestContributions::StaticClass()));
	QuestContributionsSave->Version = IAlderonCommon::Get().GetFullVersion();

	// Reset timestamps back to 0 that have been completed to allow player time to relog in to claim their contribution.
	TArray<FQuestContribution> RelevantQuestContributions = GetQuestContributions();

	for (int32 i = RelevantQuestContributions.Num(); i-- > 0;)
	{
		bool bIsGroupContribution = false;
		FQuestContribution& QuestContribution = RelevantQuestContributions[i];

		for (const FPrimaryAssetId& GroupQuestId : QuestAssetIds)
		{
			if (QuestContribution.QuestId == GroupQuestId)
			{
				RelevantQuestContributions.RemoveAt(i);
				bIsGroupContribution = true;
				break;
			}
		}

		if (bIsGroupContribution) continue;

		if (QuestContribution.Timestamp != 0.0f)
		{
			QuestContribution.Timestamp = 1.0f;
		}
	}

	QuestContributionsSave->SavedQuestContributions = RelevantQuestContributions;
	QuestContributionsSave->SavedWaterRestorationQuestTags = ActiveWaterRestorationQuestTags;
	QuestContributionsSave->SavedWaystoneRestoreQuestTags = ActiveWaystoneRestoreQuestTags;

	const FString SaveSlotName = GetSaveSlotName();
	UGameplayStatics::SaveGameToSlot(QuestContributionsSave, SaveSlotName, 0);
}

FString AIQuestManager::GetSaveSlotName()
{
	return TEXT("UE5_QuestManagerContributions_") + GetMapName();
}

FString AIQuestManager::GetMapName()
{
	FString MapName = TEXT("Empty");
	if (UWorld* World = GetWorld())
	{
		MapName = World->GetMapName();
	}
	return MapName;
}

// Called once a second on authority only
void AIQuestManager::QuestTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::QuestTick"))

	SCOPE_CYCLE_COUNTER(STAT_QuestTick);
	const AIGameState* const IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	//save data for completed feed group member quests to call GroupQuestResult logic on
	AIPlayerGroupActor* FeedMemberCleanupGroupActor = nullptr;
	UIQuest* FeedMemberQuestToCleanup = nullptr;

	for (const APlayerState* const PlayerState : IGameState->PlayerArray)
	{
		if (!PlayerState) continue;

		const AIPlayerController* const OwningPlayerController = Cast<AIPlayerController>(PlayerState->GetOwner());
		if (!OwningPlayerController || !OwningPlayerController->IsValidLowLevel())
		{
			continue;
		}

		AIBaseCharacter* const OwningCharacter = OwningPlayerController->GetPawn<AIBaseCharacter>();
		if (!OwningCharacter || !OwningCharacter->IsValidLowLevel())
		{
			continue;
		}

		// Backwards For Loop as quests can be removed when they are completed
		// Intentionally backwards because you can't do this forwards without
		// invalidating the array or length
		for (int32 Index = OwningCharacter->GetActiveQuests().Num() - 1; Index >= 0; --Index)
		{
			UIQuest* const ActiveQuest = OwningCharacter->GetActiveQuests()[Index];
			if (!ActiveQuest || !ActiveQuest->IsValidLowLevel())
			{
				continue;
			}

			ActiveQuest->Update(OwningCharacter, ActiveQuest);

			const UQuestData* const QuestData = ActiveQuest->QuestData;
			if (!QuestData || !QuestData->IsValidLowLevel())
			{
				continue;
			}

			if ((ActiveQuest->GetPlayerGroupActor() && !ActiveQuest->GetPlayerGroupActor()->GetGroupQuests().Contains(ActiveQuest)) || !QuestData->bEnabled)
			{
				GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("AIQuestManager::QuestTick() - Group doesn't contain quest")));
				OnQuestFail(OwningCharacter, ActiveQuest);
				continue;
			}

			//fail quests if character no longer meets gameplay tag requirements
			if (!QuestData->RequiredGameplayTags.IsEmpty())
			{
				FGameplayTagContainer CharacterTags{};
				if (UAbilitySystemComponent* AbilitySystemComponent = OwningCharacter->GetAbilitySystemComponent())
				{
					AbilitySystemComponent->GetOwnedGameplayTags(CharacterTags);
					// certain quests should only be give to characters that can dive
					// Ability.CanDive simply causes bAquatic to be set to true, but dinos with bAquatic == true do not necessarily have the CanDive tag
					// if Character->IsAquatic() == true then the character can dive, this code block allows bp data assets to rely on Ability.CanDive as a required tag
					if (OwningCharacter->IsAquatic())
					{
						CharacterTags.AddTag(UPOTAbilitySystemGlobals::Get().CanDiveTag);
					}
				}

				if (!CharacterTags.HasAll(QuestData->RequiredGameplayTags))
				{
					GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("AIQuestManager::QuestTick() - does not meet gameplay tag requirements")));
					OnQuestFail(OwningCharacter, ActiveQuest);
					continue;
				}
			}
							
			//do not skip OnQuestCompleted calls for feed group member quests if the leader is the hungry player
			bool FeedMemberQuestWithTargetLeader = false;
			if (ActiveQuest->QuestData->QuestType == EQuestType::GroupSurvival
				&& ActiveQuest->GetPlayerGroupActor()
				&& ActiveQuest->GetPlayerGroupActor()->GetGroupLeader())
			{
				for (const UIQuestBaseTask* const QuestTask : ActiveQuest->GetQuestTasks())
				{
					const UIQuestFeedMember* const FeedMemberTask = Cast<UIQuestFeedMember>(QuestTask);
					if (!FeedMemberTask) continue;

					if (!IsValid(FeedMemberTask->TargetMember)) continue;
									
					const AIPlayerState* const IPlayerState = FeedMemberTask->TargetMember->GetPlayerState<AIPlayerState>();
					if (!IsValid(IPlayerState)) continue;

					if (IPlayerState == ActiveQuest->GetPlayerGroupActor()->GetGroupLeader())
					{
						FeedMemberQuestWithTargetLeader = true;
					}
				}
			}

			// Only process checks for Group Quests if it is from the Group Leader to save performance
			if (ActiveQuest->GetPlayerGroupActor() && ActiveQuest->GetPlayerGroupActor()->GetGroupLeader() && ActiveQuest->GetPlayerGroupActor()->GetGroupLeader() != PlayerState && !FeedMemberQuestWithTargetLeader) continue;

			if (!ActiveQuest->IsCompleted() || FeedMemberQuestWithTargetLeader)
			{
				// Check for quest completion
				ActiveQuest->CheckCompletion();

				if (ActiveQuest->IsCompleted())
				{
					// save data on feed member quest to cleanup after for loop
					if (FeedMemberQuestWithTargetLeader)
					{
						FeedMemberCleanupGroupActor = ActiveQuest->GetPlayerGroupActor();
						FeedMemberQuestToCleanup = ActiveQuest;

						FeedMemberCleanupGroupActor->RemoveGroupQuestFailure(FeedMemberQuestToCleanup->GetQuestId(), true);
					}

					OnQuestCompleted(OwningCharacter, ActiveQuest);
					continue;
				}

				// Handle timed quests and failure
				if (QuestData->TimeLimit > 0.0f && (!QuestData->bLeftAreaTimeLimit || ActiveQuest->IsFailureInbound()))
				{
					if (ActiveQuest->GetRemainingTime() > 0)
					{
						ActiveQuest->SetRemainingTime(ActiveQuest->GetRemainingTime() - 1);
					}
					else {
						ActiveQuest->SetRemainingTime(0);
						OnQuestFail(OwningCharacter, ActiveQuest);
					}
				}
			}
		}
	}

	//replicate the logic of GroupQuestResult without actually calling GroupQuestResult and completing the quest additional times
	if (FeedMemberCleanupGroupActor && FeedMemberQuestToCleanup)
	{
		FeedMemberCleanupGroupActor->RemoveGroupQuest(FeedMemberQuestToCleanup);

		FeedMemberCleanupGroupActor->AssignGroupQuests();
	}
}

// Called once a minute on authority only
void AIQuestManager::QuestTock()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::QuestTock"))

	SCOPE_CYCLE_COUNTER(STAT_QuestTock);
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	for (APlayerState* PlayerState : IGameState->PlayerArray)
	{
		if (!PlayerState) continue;

		AIPlayerController* OwningPlayerController = Cast<AIPlayerController>(PlayerState->GetOwner());
		if (OwningPlayerController && OwningPlayerController->IsValidLowLevel())
		{
			AIBaseCharacter* OwningCharacter = OwningPlayerController->GetPawn<AIBaseCharacter>();
			if (OwningCharacter && OwningCharacter->IsValidLowLevel() && OwningCharacter->HasLeftHatchlingCave())
			{
				// Backwards For Loop as quests can be removed when they are completed
				// Intentionally backwards because you can't do this forwards without
				// invalidating the array or length

				bool bHasSurvivalQuest = false;

				for (int32 Index = OwningCharacter->GetActiveQuests().Num() - 1; Index >= 0; --Index)
				{
					const UIQuest* const ActiveQuest = OwningCharacter->GetActiveQuests()[Index];
					if (ActiveQuest && ActiveQuest->IsValidLowLevel())
					{
						UQuestData* QuestData = ActiveQuest->QuestData;
						if (QuestData && QuestData->IsValidLowLevel())
						{
							if (QuestData->QuestShareType == EQuestShareType::Survival)
							{
								bHasSurvivalQuest = true;
								break;
							}
						}
					}
				}

				if (!bHasSurvivalQuest)
				{
					FQuestIDLoaded QuestDataloaded;
					TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
					TWeakObjectPtr<AIBaseCharacter> WeakOwningCharacter = MakeWeakObjectPtr(OwningCharacter);

					QuestDataloaded.BindLambda([WeakThis, WeakOwningCharacter](FPrimaryAssetId QuestAssetId) {
						if (WeakThis.IsValid() && WeakOwningCharacter.IsValid())
						{
							WeakThis->OnQuestTock(WeakOwningCharacter.Get(), QuestAssetId);
						}
					});

					GetRandomQuest(OwningCharacter, QuestDataloaded, EQuestShareType::Survival);
				}
			}
		}
	}
}

void AIQuestManager::OnQuestTock(AIBaseCharacter* OwningCharacter, FPrimaryAssetId QuestAssetId)
{
	if (QuestAssetId.IsValid())
	{
		AssignQuest(QuestAssetId, OwningCharacter, false);
	}
}

void AIQuestManager::ContributionTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::ContributionTick"))
	SCOPE_CYCLE_COUNTER(STAT_QuestContributionTick);
	
	if (!GetWorld())
	{
		return;
	}

	const float CleanupDelay = (UE_BUILD_SHIPPING) ? QuestContributionCleanupDelay : 60.0f;
	const float TimeSeconds = GetWorld()->TimeSeconds;
	
	bool bNeedsDirtying = false;
		
	for (int32 Index = GetQuestContributions().Num() - 1; Index >= 0; --Index)
	{
		const FQuestContribution& QuestContribution = GetQuestContributions()[Index];

		if (QuestContribution.Timestamp == 0.0f) continue;

		// Only keep contributions for 10 minutes after they have been rewarded to allow for them to log back into their character
		if ((QuestContribution.Timestamp + CleanupDelay) <= TimeSeconds)
		{
			QuestContributions.RemoveAt(Index);
			bNeedsDirtying = true;
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, QuestContributions, this);
	}
}

void AIQuestManager::CooldownTick()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::CooldownTick"))

	if (!GetWorld()) return;

	float CleanupDelay = (UE_BUILD_SHIPPING) ? LocalQuestCooldownDelay : 60.0f;
	float TimeSeconds = GetWorld()->TimeSeconds;
	int64 NowTimestamp = FDateTime::UtcNow().ToUnixTimestamp();

	TArray<FQuestCooldown> LocalCooldownWorldQuests = GetLocalWorldQuestsOnCooldown();

	if (!LocalCooldownWorldQuests.IsEmpty() && CleanupDelay > 0.0f)
	{
		for (int32 i = LocalCooldownWorldQuests.Num(); i-- > 0;)
		{
			const FQuestCooldown& LocalQuestCooldown = LocalCooldownWorldQuests[i];

			// Only keep contributions for 10 minutes after they have been rewarded to allow for them to log back into their character
			if (LocalQuestCooldown.IsExpired(NowTimestamp, CleanupDelay))
			{
				//UE_LOG(TitansLog, Log, TEXT("AIQuestManager::CooldownTick(): Expired! Removing"));
				RemoveLocalWorldQuestCooldown(LocalQuestCooldown);
			}
			//else
			//{
				//FTimespan TimeRemaining = (FDateTime::FromUnixTimestamp(LocalQuestCooldown.UnixTimestamp) + FTimespan(0, 0, CleanupDelay)) - FDateTime::UtcNow();
				//UE_LOG(TitansLog, Log, TEXT("AIQuestManager::CooldownTick(): Not Expired! Remaining time %i:%i:%i"), TimeRemaining.GetHours(), TimeRemaining.GetMinutes(), TimeRemaining.GetSeconds());
			//}
		}
	}

	CleanupDelay = (UE_BUILD_SHIPPING) ? GroupMeetQuestCooldownDelay : 60.0f;
	if (!GroupMeetQuestCooldowns.IsEmpty() && CleanupDelay > 0.0f)
	{
		for (int32 i = GroupMeetQuestCooldowns.Num(); i-- > 0;)
		{
			FQuestCooldown GroupMeetQuestCooldown = GroupMeetQuestCooldowns[i];

			// Only keep contributions for 10 minutes after they have been rewarded to allow for them to log back into their character
			if ((GroupMeetQuestCooldown.Timestamp + CleanupDelay) <= TimeSeconds)
			{
				GroupMeetQuestCooldowns.RemoveAt(i);
			}
		}
	}

	CleanupDelay = (UE_BUILD_SHIPPING) ? GroupQuestCleanupDelay : 60.0f;
	if (!GroupQuestsOnCooldown.IsEmpty() && CleanupDelay > 0.0f)
	{
		for (int32 i = GroupQuestsOnCooldown.Num(); i-- > 0;)
		{
			FQuestCooldown& GroupQuestCooldown = GroupQuestsOnCooldown[i];

			if ((GroupQuestCooldown.Timestamp + CleanupDelay) <= TimeSeconds)
			{
				GroupQuestsOnCooldown.RemoveAt(i);
			}
		}
	}

	CleanupDelay = (UE_BUILD_SHIPPING) ? LocationCompletedCleanupDelay : 60.0f;

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	check(IGameState);

	TArray<APlayerState*> PlayerArray = IGameState->PlayerArray;
	for (APlayerState* PlayerState : PlayerArray)
	{
		AIPlayerState* IPS = Cast<AIPlayerState>(PlayerState);
		if (!IsValid(IPS)) continue;

		AIPlayerController* IPC = Cast<AIPlayerController>(IPS->GetPlayerController());
		if (!IPC) continue;

		AIBaseCharacter* IBC = IPC->GetPawn<AIBaseCharacter>();
		if (!IBC) continue;

		TArray<FLocationProgress>& LocationsProgress = IBC->GetLocationsProgress_Mutable();

		bool bLocationArrayDirty = false;

		if (!LocationsProgress.IsEmpty() && CleanupDelay > 0.0f)
		{
			for (int32 i = LocationsProgress.Num(); i-- > 0;)
			{
				FLocationProgress& LocationProgress = LocationsProgress[i];

				if (LocationProgress.CompletedDateTime == FDateTime()) continue;

				const FTimespan TimeSinceCompleted = (FDateTime::UtcNow() - LocationProgress.CompletedDateTime);
				float SecondsSinceCompleted = TimeSinceCompleted.GetTotalSeconds();
				if (SecondsSinceCompleted >= CleanupDelay)
				{
					bool bInCompletedLocation = false;
					if (IBC->LastLocationEntered && IBC->LastLocationTag == LocationProgress.LocationTag)
					{
						bInCompletedLocation = true;
					}

					LocationsProgress.RemoveAt(i);
					bLocationArrayDirty = true;
					
					if (bInCompletedLocation)
					{
						AssignRandomLocalQuest(IBC);
					}
				}
			}
		}
	}
	
	CleanupDelay = (UE_BUILD_SHIPPING) ? TrophyQuestCooldownDelay : 60.0f;
	
	if (CleanupDelay > 0.0f)
	{
		bool bNeedsDirtying = false;

		for (int32 Index = GetTrophyQuestsOnCooldown().Num() - 1; Index >= 0; --Index)
		{
			const FQuestCooldown& QuestCooldown = GetTrophyQuestsOnCooldown()[Index];

			if (QuestCooldown.Timestamp + CleanupDelay <= TimeSeconds)
			{
				TrophyQuestsOnCooldown.RemoveAt(Index);
				bNeedsDirtying = true;
			}
		}

		if (bNeedsDirtying)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, TrophyQuestsOnCooldown, this);
		}
	}
}

//void AIQuestManager::Tick(float DeltaTime)
//{
//	Super::Tick(DeltaTime);
//}

bool AIQuestManager::IsPoiCompatibleForExploration(AActor* Poi, AIBaseCharacter* Character) const
{
	check(Poi && Character);
	if (!Poi || !Character)
	{
		UE_LOG(TitansLog, Error, TEXT("AIQuestManager::IsPoiCompatibleForExploration: Poi or Character is nullptr."));
		return false;
	}

	if (AIPOI* IPoi = Cast<AIPOI>(Poi))
	{
		if (IPoi->RequiredExplorationQuestTags.IsEmpty())
		{
			return true;
		}
		else
		{
			for (const FName& RequiredTag : IPoi->RequiredExplorationQuestTags)
			{
				if (Character->QuestTags.Contains(RequiredTag))
				{
					return true;
				}
			}
			return false;
		}
	}
	else if (AIPointOfInterest* IPointOfInterest = Cast<AIPointOfInterest>(Poi))
	{
		if (IPointOfInterest->RequiredExplorationQuestTags.IsEmpty())
		{
			return true;
		}
		else
		{
			for (const FName& RequiredTag : IPointOfInterest->RequiredExplorationQuestTags)
			{
				if (Character->QuestTags.Contains(RequiredTag))
				{
					return true;
				}
			}
			return false;
		}
	}
	else
	{
		return false;
	}
}

void AIQuestManager::GetRandomQuest(AIBaseCharacter* Character, FQuestIDLoaded QuestIDLoaded, EQuestShareType PreferredType)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetRandomQuest"))

	EQuestType ActivePersonalQuestType = EQuestType::MAX;

	if (!HasRoomForQuest(Character, PreferredType, ActivePersonalQuestType)) return;

	SCOPE_CYCLE_COUNTER(STAT_GetRandomQuest);

	EQuestFilter QuestFilter = EQuestFilter::SURVIVAL;
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	check(IGameState);
	if (!IGameState) return;

	if (PreferredType != EQuestShareType::Survival)
	{
		bool bIsMultiplayer = (IGameState->PlayerArray.Num() > 1);

		if (!bIsMultiplayer)
		{
			if (Character->DietRequirements == EDietaryRequirements::CARNIVORE)
			{
				QuestFilter = EQuestFilter::SINGLE_CARNI;
			}
			else if (Character->DietRequirements == EDietaryRequirements::HERBIVORE)
			{
				QuestFilter = EQuestFilter::SINGLE_HERB;
			}
			else if (Character->DietRequirements == EDietaryRequirements::OMNIVORE)
			{
				QuestFilter = EQuestFilter::SINGLE_ALL;
			}
		}
		else if (HasAuthority())
		{
			if (Character->DietRequirements == EDietaryRequirements::CARNIVORE)
			{
				QuestFilter = EQuestFilter::MULTI_CARNI;
			}
			else if (Character->DietRequirements == EDietaryRequirements::HERBIVORE)
			{
				QuestFilter = EQuestFilter::MULTI_HERB;
			}
			else if (Character->DietRequirements == EDietaryRequirements::OMNIVORE)
			{
				QuestFilter = EQuestFilter::MULTI_ALL;
			}
		}
	}
	FQuestsIDsLoaded LoadAssetIDsDelegate;
	TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
	TWeakObjectPtr<AIBaseCharacter> WeakCharacter = MakeWeakObjectPtr(Character);
	LoadAssetIDsDelegate.BindLambda([WeakThis, WeakCharacter, PreferredType, ActivePersonalQuestType, QuestIDLoaded](TArray<FPrimaryAssetId> QuestAssetIds)
	{
		if (WeakThis.IsValid() && WeakCharacter.IsValid())
		{
			WeakThis->OnGetRandomQuest(WeakCharacter.Get(), PreferredType, ActivePersonalQuestType, QuestAssetIds, QuestIDLoaded);
		}
	});

	GetLoadedQuestAssetIDsFromType(QuestFilter, Character, LoadAssetIDsDelegate);
}

bool AIQuestManager::HasRoomForQuest(const AIBaseCharacter* Character, const EQuestShareType PreferredType, EQuestType& ActivePersonalQuestType)
{
	// Ensure we don't already have a personal or survival quest
	if (Character->GetActiveQuests().IsEmpty()) return true;

	int32 CurrentPersonalQuests = 0;
	bool bNoRoomForQuest = false;

	for (UIQuest* CharacterQuest : Character->GetActiveQuests())
	{
		if (!CharacterQuest)
		{
			continue;
		}

		UQuestData* CharacterQuestData = CharacterQuest->QuestData;
		if (!CharacterQuestData)
		{
			continue;
		}

		if (CharacterQuestData->QuestShareType == EQuestShareType::Personal)
		{
			CurrentPersonalQuests++;
			ActivePersonalQuestType = CharacterQuestData->QuestType;

			if (CurrentPersonalQuests >= 2)
			{
				bNoRoomForQuest = true;
				break;
			}
		}
		else if (CharacterQuestData->QuestShareType == EQuestShareType::Survival && PreferredType == EQuestShareType::Survival)
		{
			// Skip assigning a random quest as we already have one of this type
			bNoRoomForQuest = true;
			break;
		}
	}

	return !bNoRoomForQuest;
}

void AIQuestManager::OnGetRandomQuest(AIBaseCharacter* Character, EQuestShareType PreferredType, EQuestType ActivePersonalQuestType, TArray<FPrimaryAssetId> Quests, FQuestIDLoaded QuestIDLoaded)
{
	// Because of the Async nature of quest loading we need to re-check this, or else we can end up with multiple quests beyond the limit.
	if (!HasRoomForQuest(Character, PreferredType, ActivePersonalQuestType)) return;

	UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
	check(GI);

	AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
	check(Session);

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	check(IGameState);
	if (!IGameState)
	{
		return;
	}

	FPrimaryAssetId SelectedQuest = FPrimaryAssetId();

	// No Quests Found
	if (Quests.IsEmpty())
	{
		return;
	}

	bool bMoreThanOneQuest = Quests.Num() > 1;
	bool bLimitType = (PreferredType != EQuestShareType::Unknown);

	FPrimaryAssetId ClosestPOIQuest;
	int32 ClosestPOIDistance = 99999999;
	FVector CharacterLocation = Character->GetActorLocation();

	if (AIPlayerCaveBase* IPlayerCave = Character->GetCurrentInstance())
	{
		FInstanceLogoutSaveableInfo* InstanceLogoutSaveableInfo = Character->GetInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));
		if (InstanceLogoutSaveableInfo)
		{
			CharacterLocation = InstanceLogoutSaveableInfo->CaveReturnLocation;
		}
	}

	int32 NumAttempts = 0;
	const int32 MaxAttempts = Quests.Num();
	const int32 RandomStartingQuestIndex = FMath::RandRange(0, (MaxAttempts - 1));

	if (bMoreThanOneQuest)
	{
		ShuffleQuestArray(Quests);
	}

	while (true)
	{
		const int32 RequestedQuestIndex = (bMoreThanOneQuest) ? RandomStartingQuestIndex + NumAttempts : RandomStartingQuestIndex;

		int32 QuestIndex = RequestedQuestIndex;

		if (bMoreThanOneQuest)
		{
			if (RequestedQuestIndex > (MaxAttempts - 1))
			{
				QuestIndex = (RequestedQuestIndex - MaxAttempts);
			}
		}

		check(Quests.IsValidIndex(QuestIndex));

		FPrimaryAssetId RandomQuestSelection;

		if (Quests.IsValidIndex(QuestIndex))
		{
			RandomQuestSelection = Quests[QuestIndex];
		}

		// If we have tried too hard to find a random quest we need to exit early to stop blocking the main thread :(
		if (NumAttempts >= MaxAttempts)
		{
			if (!SelectedQuest.IsValid() && PreferredType == EQuestShareType::Personal)
			{
				// GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("!SelectedQuest.IsValid() && PreferredType == EQuestShareType::Personal")));
				if (ClosestPOIQuest.IsValid())
				{
					QuestIDLoaded.ExecuteIfBound(ClosestPOIQuest);
				}
				return;
			}

			if (SelectedQuest.IsValid())
			{
				QuestIDLoaded.ExecuteIfBound(SelectedQuest);
			}
			return;
		}

		NumAttempts++;

		// Ensure the quest is valid
		if (!RandomQuestSelection.IsValid()) continue;

		// Ensure we are not giving the player a quest they already have
		bool bQuestIsDuplicateId = false;

		for (UIQuest* Quest : Character->GetActiveQuests())
		{
			if (!Quest) continue;

			if (RandomQuestSelection == Quest->GetQuestId())
			{
				// Skip assigning a random quest as we already have one of this type
				bQuestIsDuplicateId = true;
				break;
			}
		}

		if (bQuestIsDuplicateId) continue;

		// Load Quest Data for more complicated quest type picking
		UQuestData* QuestData = LoadQuestData(RandomQuestSelection);
		if (!QuestData || !QuestData->bEnabled) continue;
		check(QuestData);

		if (ActivePersonalQuestType != EQuestType::MAX && ActivePersonalQuestType == QuestData->QuestType) continue;

		// Skip picking this quest if it is the same type as the one we just completed
		// allow duplicate types of survival however
		//if (!bAllowRepeatQuestTypes && QuestData->QuestShareType != EQuestShareType::Survival)
		//{
		//	if (QuestData->QuestType == GetLastQuestType(Character, nullptr, false)) continue;
		//}

		if (UIGameplayStatics::IsGrowthEnabled(this) && Character->GetGrowthPercent() < QuestData->GrowthRequirement)
		{
			continue;
		}

		// If we have a quest tag setup on this character then skip if the tag isn't the same. Otherwise continue.
		bool bHasQuestTag = true;
		if (!Character->QuestTags.IsEmpty() && QuestData->QuestTag != NAME_QuestTagNone)
		{
			bHasQuestTag = false;
			for (int32 i = 0; i < Character->QuestTags.Num(); i++)
			{
				if (QuestData->QuestTag == Character->QuestTags[i])
				{
					bHasQuestTag = true;
					break;
				}
			}
		}
		if (!bHasQuestTag) continue;

		if (!QuestData->RequiredGameplayTags.IsEmpty())
		{
			FGameplayTagContainer CharacterTags{};
			if (UAbilitySystemComponent* AbilitySystemComponent = Character->GetAbilitySystemComponent())
			{
				AbilitySystemComponent->GetOwnedGameplayTags(CharacterTags);
				// certain quests should only be give to characters that can dive
				// Ability.CanDive simply causes bAquatic to be set to true, but dinos with bAquatic == true do not necessarily have the CanDive tag
				// if Character->IsAquatic() == true then the character can dive, this code block allows bp data assets to rely on Ability.CanDive as a required tag
				if (Character->IsAquatic())
				{
					CharacterTags.AddTag(UPOTAbilitySystemGlobals::Get().CanDiveTag);
				}
			}

			if (!CharacterTags.HasAll(QuestData->RequiredGameplayTags))
			{
				continue;
			}
		}

		if (QuestData->QuestType == EQuestType::Exploration)
		{
			const FTimespan TimeSinceLastCompletedExplorationQuest = (FDateTime::UtcNow() - Character->LastCompletedExplorationQuestTime);
			if (TimeSinceLastCompletedExplorationQuest.GetMinutes() < Session->ServerMinTimeBetweenExplorationQuest)
			{
				continue;
			}

			// Don't give a location you are already inside
			if (!QuestData->QuestTasks.IsEmpty())
			{
				bool bSkipLocationAlreadyInside = false;
				bool bFoundLocation = false;

				for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
				{
					QuestSoftPtr.LoadSynchronous();
					if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
					{
						UIQuestExploreTask* ExploreTaskClass = NewObject<UIQuestExploreTask>(Character, QuestBaseTaskClass);
						if (ExploreTaskClass)
						{
							if (ExploreTaskClass->bSkipIfAlreadyInside)
							{
								if (Character->LastLocationEntered && Character->LastLocationTag != NAME_None && ExploreTaskClass->Tag == Character->LastLocationTag)
								{
									bSkipLocationAlreadyInside = true;
									break;
								}
							}

							// Try to find a location within range, as a last resort we will just find the closest POI we aren't in at the top of this functions while loop
							for (AActor* POI : AllPointsOfInterest)
							{
								if (!IsPoiCompatibleForExploration(POI, Character))
								{
									//UE_LOG(TitansLog, Log, TEXT("AIQuestManager::GetRandomQuest: Skipping Exploration quest assign (%s) due to incompatible quest tags."), *QuestData->GetDisplayName().ToString());
									continue;
								}

								AIPointOfInterest* SpherePOI = Cast<AIPointOfInterest>(POI);
								AIPOI* MeshPOI = Cast<AIPOI>(POI);

								if ((MeshPOI && MeshPOI->GetLocationTag() == ExploreTaskClass->Tag) || (SpherePOI && SpherePOI->GetLocationTag() == ExploreTaskClass->Tag))
								{
									if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
									{
										const FVector POILocation = POI->GetActorLocation();

										//FName POIName;
										//if (MeshPOI)
										//{
										//	POIName = MeshPOI->GetLocationTag();
										//}
										//
										//if (SpherePOI)
										//{
										//	POIName = SpherePOI->GetLocationTag();
										//}

										if (!IWorldSettings->IsInWorldBounds(POILocation))
										{
											//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Red, FString::Printf(TEXT("AIQuestManager::GetRandomQuest - POI OutOfBounds: %s"), *POIName.ToString()));
											break;
										}
										else if ((MeshPOI && MeshPOI->IsDisabled()) || (SpherePOI && SpherePOI->IsDisabled()))
										{
											//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIQuestManager::GetRandomQuest - POI Disabled: %s"), *POIName.ToString()));
											break;
										}

										bFoundLocation = true;
										//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIQuestManager::GetRandomQuest - POI Closest: %s"), *POIName.ToString()));
										int32 POIDistance = (POILocation - CharacterLocation).Size();
										ClosestPOIDistance = POIDistance;
										ClosestPOIQuest = RandomQuestSelection;
										break;
									}
								}
							}
						}
					}
				}

				if (!bFoundLocation)
				{
					//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Yellow, FString::Printf(TEXT("AIQuestManager::GetRandomQuest - Location Not Found")));
				}

				if (bSkipLocationAlreadyInside || ClosestPOIDistance > 200000 || !bFoundLocation) continue;
			}
		}

		// Make sure survival stat quests meet requirements
		if (QuestData->QuestShareType == EQuestShareType::Survival)
		{
			bool bSkipIfDoesntMeetRequirements = false;

			for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
			{
				QuestSoftPtr.LoadSynchronous();
				if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
				{
					if (!QuestBaseTaskClass->IsChildOf(UIQuestPersonalStat::StaticClass()))
					{
						UE_LOG(TitansLog, Warning, TEXT("AIQuestManager::GetRandomQuest: Quest Task %s is not a child of UIQuestPersonalStat!"), *QuestBaseTaskClass->GetDefaultObjectName().ToString());
						continue;
					}

					UIQuestPersonalStat* StatTaskClass = NewObject<UIQuestPersonalStat>(Character, QuestBaseTaskClass);
					if (StatTaskClass)
					{
						bool bMeetsRequirements = StatTaskClass->MeetsStartRequirements(Character);

						if (!bMeetsRequirements)
						{
							bSkipIfDoesntMeetRequirements = true;
							break;
						}
					}
				}
			}

			if (bSkipIfDoesntMeetRequirements) continue;
		}
		else if (QuestData->QuestType == EQuestType::Hunting)
		{
			bool bMeetsRequirement = false;

			TArray<FPrimaryAssetId> CharacterAssedIds;
			TArray<AIBaseCharacter*> Characters;
			for (APlayerState* PlayerState : IGameState->PlayerArray)
			{
				AIPlayerState* RemotePlayerState = Cast<AIPlayerState>(PlayerState);
				if (!RemotePlayerState || !RemotePlayerState->GetCharacterAssetId().IsValid()) continue;

				AIBaseCharacter* RemotePawn = Cast<AIBaseCharacter>(RemotePlayerState->GetPawn());
				if (!RemotePawn) continue;

				CharacterAssedIds.AddUnique(RemotePawn->CharacterDataAssetId);
				Characters.Add(RemotePawn);
			}

			for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
			{
				QuestSoftPtr.LoadSynchronous();
				if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
				{
					UIQuestKillTask* KillTaskClass = NewObject<UIQuestKillTask>(Character, QuestBaseTaskClass);
					if (!KillTaskClass) continue;

					if (const UIQuestFishTask* const FishTaskClass = Cast<UIQuestFishTask>(KillTaskClass))
					{
						bMeetsRequirement = FishTaskClass->GetFishSize() <= Character->GetMaxCarriableSize();
						break;
					}

					if (KillTaskClass->bIsCritter)
					{
						bMeetsRequirement = IGameState->GetGameStateFlags().bCritters;
						break;
					}

					if (!CharacterAssedIds.IsEmpty())
					{
						if (KillTaskClass->ShouldIgnoreCharacterAssetIdsAndUseDietType())
						{
							for (AIBaseCharacter* RemotePawn : Characters)
							{
								if (!RemotePawn) continue;

								bool bMeetsRequirements = RemotePawn->DietRequirements == KillTaskClass->GetDietaryRequirements();

								if (bMeetsRequirements)
								{
									bMeetsRequirement = true;
									break;
								}
							}
						}
						else
						{
							for (const FPrimaryAssetId& CharacterAssetId : CharacterAssedIds)
							{
								bool bMeetsRequirements = KillTaskClass->GetCharacterAssetIds().Contains(CharacterAssetId);

								if (bMeetsRequirements)
								{
									bMeetsRequirement = true;
									break;
								}
							}
						}
					}
					else
					{
						bMeetsRequirement = true;
					}

					if (bMeetsRequirement) break;
				}
			}

			if (!bMeetsRequirement) continue;
		}
		else if (QuestData->QuestType == EQuestType::MoveTo)
		{
			float DistanceFromQuest = FVector::Distance(QuestData->WorldLocation, Character->GetActorLocation());
			bool bMeetsRequirement = DistanceFromQuest < MaxQuestMoveToDistance;

			if (!bMeetsRequirement) continue;
		}

		// Skip picking this personal quest if we have just completed it recently
		// The amount of recently completed quests is tracked by MaxRecentCompletedQuests variable in header
		if (!bAllowRepeatQuests && PreferredType == EQuestShareType::Personal)
		{
			if (HasCompletedQuest(RandomQuestSelection, Character)) continue;
			if (HasFailedQuest(RandomQuestSelection, Character))
			{
				GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("HasFailedQuest"));
				continue;
			}
		}

		// If we got this far we got a valid quest so return it
		SelectedQuest = RandomQuestSelection;

		GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIQuestManager::GetRandomQuest - SelectedQuest: %s"), *SelectedQuest.ToString()));
		QuestIDLoaded.ExecuteIfBound(SelectedQuest);
		return;
	}

	QuestIDLoaded.ExecuteIfBound(SelectedQuest);
	return;
}

void AIQuestManager::GetRandomGroupQuest(AIPlayerGroupActor* PlayerGroupActor, FQuestIDLoaded QuestIDLoaded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetRandomGroupQuest"))

	EQuestFilter QuestFilter = EQuestFilter::GROUP_ALL;

	if (PlayerGroupActor->GroupCharacterType == ECharacterType::CARNIVORE)
	{
		QuestFilter = EQuestFilter::GROUP_CARNI;
	}
	else if (PlayerGroupActor->GroupCharacterType == ECharacterType::HERBIVORE)
	{
		QuestFilter = EQuestFilter::GROUP_HERB;
	}
	else
	{
		QuestFilter = EQuestFilter::GROUP_ALL;
	}

	float MinGrowthInGroup = 1.0f;
	if (UIGameplayStatics::IsGrowthEnabled(this))
	{
		for (AIPlayerState* IPS : PlayerGroupActor->GetGroupMembers())
		{
			if (AIBaseCharacter* IBC = IPS->GetPawn<AIBaseCharacter>())
			{
				if (IBC->GetGrowthPercent() < MinGrowthInGroup)
				{
					MinGrowthInGroup = IBC->GetGrowthPercent();
				}
			}
		}
	}

	// Collect possible tags from each player
	TArray<FName, TInlineAllocator<3>> QuestTags;
	for (AIPlayerState* IPS : PlayerGroupActor->GetGroupMembers())
	{
		if (AIBaseCharacter* IBC = IPS->GetPawn<AIBaseCharacter>())
		{
			for (FName QuestTag : IBC->QuestTags)
			{
				if (!QuestTags.Contains(QuestTag))
				{
					QuestTags.Add(QuestTag);
				}
			}
		}
	}

	// Ensure every player has this tag, otherwise remove it
	for (int32 i = QuestTags.Num(); i-- > 0;)
	{
		FName QuestTag = QuestTags[i];
		for (AIPlayerState* IPS : PlayerGroupActor->GetGroupMembers())
		{
			if (AIBaseCharacter* IBC = IPS->GetPawn<AIBaseCharacter>())
			{
				if (!IBC->QuestTags.Contains(QuestTag))
				{
					QuestTags.RemoveAt(i);
					break;
				}
			}
		}
	}

	// determine if the group should be assigned a FeedGroupMemberQuest
	bool bGetFeedQuest = false;
	//ensure that there is a hungry group member
	for (AIPlayerState* IPS : PlayerGroupActor->GetGroupMembers())
	{
		if (AIBaseCharacter* IBC = IPS->GetPawn<AIBaseCharacter>())
		{
			if ((IBC->GetHunger() / IBC->GetMaxHunger()) <= 0.25f)
			{
				bGetFeedQuest = true;
			}
		}
	}
	// do not get a feed member quest if the group recently completed one
	if (bGetFeedQuest)
	{
		TArray<FRecentQuest> RecentQuests = GetRecentCompletedGroupQuests(PlayerGroupActor);
		for (const FRecentQuest& RecentQuest : RecentQuests)
		{
			if (RecentQuest.QuestType == EQuestType::GroupSurvival)
			{
				bGetFeedQuest = false;
				break;
			}
		}
	}
	//do not get a feed member quest if the group currently has one
	if (bGetFeedQuest)
	{
		for (UIQuest* GroupQuest : PlayerGroupActor->GetGroupQuests())
		{
			if (GroupQuest->QuestData->QuestType == EQuestType::GroupSurvival)
			{
				bGetFeedQuest = false;
			}
		}
	}

	TArray<FPrimaryAssetId> ActiveGroupQuestsOnCooldown = PlayerGroupActor->GetGroupQuestsOnCooldown();

	// check for cooldowns of group survival quests
	if (bGetFeedQuest)
	{
		int32 cooldowns = 0;
		for (const FPrimaryAssetId& GroupSurvivalQuest : GroupSurvivalQuests)
		{
			for (const FPrimaryAssetId& QuestCooldownId : ActiveGroupQuestsOnCooldown)
			{
				if (QuestCooldownId == GroupSurvivalQuest)
				{
					++cooldowns;
					break;
				}
			}
		}

		if (cooldowns == GroupSurvivalQuests.Num())
		{
			bGetFeedQuest = false;
		}
	}

	if (bGetFeedQuest)
	{
		OnGetRandomGroupQuest(QuestFilter, GroupSurvivalQuests, PlayerGroupActor, QuestIDLoaded, MinGrowthInGroup, QuestTags);
	}
	else
	{
		FQuestsIDsLoaded QuestsDataloaded;
		TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
		TWeakObjectPtr<AIPlayerGroupActor> WeakPlayerGroupActor = MakeWeakObjectPtr(PlayerGroupActor);
		QuestsDataloaded.BindLambda([WeakThis, QuestFilter, WeakPlayerGroupActor, QuestIDLoaded, MinGrowthInGroup, QuestTags](TArray<FPrimaryAssetId> QuestAssetIds) {
			if (WeakThis.IsValid() && WeakPlayerGroupActor.IsValid())
			{
				WeakThis->OnGetRandomGroupQuest(QuestFilter, QuestAssetIds, WeakPlayerGroupActor.Get(), QuestIDLoaded, MinGrowthInGroup, QuestTags);
			}
		});

		GetLoadedQuestAssetIDsFromType(QuestFilter, nullptr, QuestsDataloaded);
	}
}

void AIQuestManager::OnGetRandomGroupQuest(EQuestFilter QuestFilter, TArray<FPrimaryAssetId> Quests, AIPlayerGroupActor* PlayerGroupActor, FQuestIDLoaded QuestIDLoaded, float MinGrowthInGroup, TArray<FName, TInlineAllocator<3>> QuestTags)
{
	FPrimaryAssetId SelectedQuest;

	bool bMoreThanOneQuest = Quests.Num() > 1;

	TArray<FPrimaryAssetId> ActiveGroupQuestsOnCooldown = PlayerGroupActor->GetGroupQuestsOnCooldown();

	if (bMoreThanOneQuest)
	{
		ShuffleQuestArray(Quests);

		int32 NumAttempts = 0;
		int32 MaxAttempts = 100;
		while (true)
		{
			// Increment Number of Attempts
			NumAttempts++;

			FPrimaryAssetId RandomQuestSelection;

			// Select a Random Quest out of an Array
			int32 RandomQuestIndex = FMath::RandRange(0, Quests.Num() - 1);
			RandomQuestSelection = Quests[RandomQuestIndex];

			// If we have tried too hard to find a random quest we need to exit early to stop blocking the main thread :(
			if (NumAttempts > MaxAttempts)
			{
				QuestIDLoaded.ExecuteIfBound(SelectedQuest);
				return;
			}

			if (!RandomQuestSelection.IsValid()) continue;

			if (!ActiveGroupQuestsOnCooldown.IsEmpty())
			{
				bool bCooldown = false;
				for (const FPrimaryAssetId& QuestCooldownId : ActiveGroupQuestsOnCooldown)
				{
					if (QuestCooldownId == RandomQuestSelection)
					{
						bCooldown = true;
						break;
					}
				}

				if (bCooldown) continue;
			}

			// Ensure we are not giving the player a quest they already have
			bool bQuestIsDuplicateId = false;

			if (PlayerGroupActor)
			{
				for (UIQuest* GroupQuest : PlayerGroupActor->GetGroupQuests())
				{
					if (RandomQuestSelection == GroupQuest->GetQuestId())
					{
						// Skip assigning a random quest as we already have one of this type
						bQuestIsDuplicateId = true;
						break;
					}
				}
			}

			if (bQuestIsDuplicateId) continue;

			// Load Quest Data for more complicated quest type picking
			UQuestData* QuestData = LoadQuestData(RandomQuestSelection);
			if (!QuestData || !QuestData->bEnabled)
			{
				// Failed to load Quest Data
				continue;
			}
			check(QuestData);

			if (UIGameplayStatics::IsGrowthEnabled(this) && MinGrowthInGroup < QuestData->GrowthRequirement)
			{
				continue;
			}

			// If we have a quest tag then skip if the tag isn't the same. Otherwise continue.
			bool bHasQuestTag = true;
			if (!QuestTags.IsEmpty())
			{
				bHasQuestTag = false;

				for (int32 i = 0; i < QuestTags.Num(); i++)
				{
					if (QuestData->QuestTag == QuestTags[i])
					{
						bHasQuestTag = true;
						break;
					}
				}
			}
			if (!bHasQuestTag) continue;

			if (AIPlayerController* RemotePlayerController = PlayerGroupActor->GetGroupLeader()->GetOwner<AIPlayerController>())
			{
				if (AIBaseCharacter* RemoteCharacter = RemotePlayerController->GetPawn<AIBaseCharacter>())
				{
					if (QuestData->LocationTag != NAME_None)
					{
						// Location Quests: Don't give group a location quest that it's leader isn't inside
						if (RemoteCharacter->LastLocationTag != NAME_None)
						{
							bool bIsInLocation = false;

							if (QuestData->LocationTag == RemoteCharacter->LastLocationTag && !HasCompletedLocation(QuestData->LocationTag, RemoteCharacter))
							{
								bIsInLocation = true;
							}

							if (!bIsInLocation) continue;
						}
					}
					else
					{
						// Location Quests: Don't give group a location quest that it's leader isn't inside
						if (RemoteCharacter->LastLocationTag != NAME_None)
						{
							if (!QuestData->QuestTasks.IsEmpty())
							{
								bool bIsInLocation = false;

								for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
								{
									QuestSoftPtr.LoadSynchronous();
									if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
									{
										if (UIQuestItemTask* ItemTaskClass = NewObject<UIQuestItemTask>(RemoteCharacter, QuestBaseTaskClass))
										{
											if (ItemTaskClass->Tag != RemoteCharacter->LastLocationTag || HasCompletedLocation(ItemTaskClass->Tag, RemoteCharacter)) continue;

											bIsInLocation = true;
											break;
										}
									}
								}

								if (!bIsInLocation) continue;
							}
						}
					}
				}
			}

			// If we got this far we got a valid quest so return it
			SelectedQuest = RandomQuestSelection;
			break;
		}
	}
	else if (Quests.Num() == 1)
	{
		// Return One and Only Quest
		SelectedQuest = Quests[0];
	}

	QuestIDLoaded.ExecuteIfBound(SelectedQuest);
}

void AIQuestManager::GetLocalWorldQuests(AIBaseCharacter* Character, FQuestsDataLoaded OnLoadAssetsDelegate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetLocalWorldQuests"))

	SCOPE_CYCLE_COUNTER(STAT_GetLocalWorldQuest);
	check(Character);

	TArray<FPrimaryAssetId> Quests = WaterRestoreQuests;

	Quests.Append(WaystoneRestoreQuests);
	Quests.Append(LocalWorldQuests);

	if (Quests.IsEmpty()) return;
	if (!Character->LastLocationEntered || Character->LastLocationTag == NAME_None) return;

	ShuffleQuestArray(Quests);
	TArray<FQuestCooldown> LocalCooldownWorldQuests = GetLocalWorldQuestsOnCooldown();

	Quests.Reserve(5);

	auto FilteredQuests = Quests.FilterByPredicate([&, CharacterID = Character->GetCharacterID()](const FPrimaryAssetId& QuestSelected) -> bool
	{
		if (!QuestSelected.IsValid())
		{
			return true;
		}

		// Ensure this player doesn't have an active cooldown for this quest
		if (LocalQuestCooldownDelay > 0.0f)
		{
			if (LocalCooldownWorldQuests.Contains(FQuestCooldown(QuestSelected, CharacterID, 0.0f)))
			{
				UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::GetLocalWorldQuests() - LocalWorldQuestsOnCooldown Contains | QuestSelected %s | Character->CharacterID %s"), *QuestSelected.ToString(), *CharacterID.ToString());
				return true;
			}
		}
		return false;
	});

	// Load Quest Data for more complicated quest type picking

	FQuestsDataLoaded LoadAssetsDelegate;
	TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
	TWeakObjectPtr<AIBaseCharacter> WeakCharacter = MakeWeakObjectPtr(Character);
	LoadAssetsDelegate.BindLambda([WeakThis, WeakCharacter, OnLoadAssetsDelegate](TArray<UQuestData*> QuestsData)
	{
		if (WeakThis.IsValid() && WeakCharacter.IsValid())
		{
			WeakThis->FilterOnLocalWorldQuestsLoaded(QuestsData, WeakCharacter.Get(), OnLoadAssetsDelegate);
		}
	});

	LoadQuestsData(Quests, LoadAssetsDelegate);
}

void AIQuestManager::FilterOnLocalWorldQuestsLoaded(TArray<UQuestData*> QuestsData, AIBaseCharacter* Character, FQuestsDataLoaded OnLoadAssetsDelegate)
{
	const TArray<FSlottedAbilityEffects>& SlottedEffects = Character->GetSlottedActiveAndPassiveEffects();
	const UAbilitySystemComponent* const AbilitySystemComponent = Character->GetAbilitySystemComponent();
	const FGameplayTag& DiveTag = UPOTAbilitySystemGlobals::Get().CanDiveTag;
	TArray<UQuestData*> SelectedQuests{};

	for (UQuestData* QuestData : QuestsData)
	{
		if (!QuestData || !QuestData->bEnabled)
		{
			continue;
		}
		check(QuestData);

		if (UIGameplayStatics::IsGrowthEnabled(this) && Character->GetGrowthPercent() < QuestData->GrowthRequirement)
		{
			continue;
		}

		// If we have a quest tag setup on this character then skip if the tag isn't the same. Otherwise continue.
		bool bHasQuestTag = true;
		if (!Character->QuestTags.IsEmpty())
		{
			bHasQuestTag = false;
			for (int32 i = 0; i < Character->QuestTags.Num(); i++)
			{
				if (QuestData->QuestTag == Character->QuestTags[i])
				{
					bHasQuestTag = true;
					break;
				}
			}
		}

		if (!bHasQuestTag && QuestData->QuestTag == NAME_Aquatic)
		{
			for (const FSlottedAbilityEffects& Effect : SlottedEffects)
			{			
				for (const FActiveGameplayEffectHandle& Passive : Effect.PassiveEffects)
				{					
					if (!Passive.IsValid())
					{
						continue;
					}

					const FActiveGameplayEffect* GameplayEffect = AbilitySystemComponent->GetActiveGameplayEffect(Passive);
					if (!GameplayEffect)
					{
						continue;
					}

					FGameplayTagContainer GrantedTags{};
					GameplayEffect->Spec.GetAllGrantedTags(GrantedTags);

					if (GrantedTags.HasTag(DiveTag))
					{
						bHasQuestTag = true;
						break;
					}					
				}
				
				if (bHasQuestTag) break;
			}
		}

		if (!bHasQuestTag) continue;

		// Make sure water or waystone task is available
		bool bSkipIfDoesNotContain = false;

		if (QuestData->QuestLocalType == EQuestLocalType::Water)
		{
			for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
			{
				QuestSoftPtr.LoadSynchronous();
				if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
				{
					if (QuestBaseTaskClass->IsChildOf(UIQuestWaterRestoreTask::StaticClass()))
					{
						if (UIQuestWaterRestoreTask* WaterTaskClass = NewObject<UIQuestWaterRestoreTask>(Character, QuestBaseTaskClass))
						{
							if (!ActiveWaterRestorationQuestTags.Contains(WaterTaskClass->Tag))
							{
								bSkipIfDoesNotContain = true;
								break;
							}
						}
					}
				}
			}
		}
		else if (QuestData->QuestLocalType == EQuestLocalType::Waystone)
		{
			for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
			{
				QuestSoftPtr.LoadSynchronous();
				if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
				{
					if (QuestBaseTaskClass->IsChildOf(UIQuestWaystoneCooldownTask::StaticClass()))
					{
						if (UIQuestWaystoneCooldownTask* WaystoneTaskClass = NewObject<UIQuestWaystoneCooldownTask>(Character, QuestBaseTaskClass))
						{
							if (!ActiveWaystoneRestoreQuestTags.Contains(WaystoneTaskClass->Tag))
							{
								bSkipIfDoesNotContain = true;
								break;
							}
						}
					}
				}
			}
		}

		if (bSkipIfDoesNotContain) continue;

		// Check if users has this priority quest assigned
		// Basically water and waystone local world quests should always be assigned even if the player already has a local world quest active
		if (QuestData->bPriority)
		{
			bool bAlreadyExists = false;

			for (UIQuest* CharacterQuest : Character->GetActiveQuests())
			{
				if (!CharacterQuest) continue;
				if (QuestData->GetPrimaryAssetId() != CharacterQuest->GetQuestId()) continue;

				UQuestData* CharacterQuestData = CharacterQuest->QuestData;
				if (!CharacterQuestData) continue;

				// Skip assigning a random quest as we already have one of this type
				bAlreadyExists = true;
				break;
			}

			if (bAlreadyExists) continue;
		}

		// Ensure we're in the quests location
		if (!QuestData->QuestTasks.IsEmpty())
		{
			bool bIsInLocation = false;

			// QuestData might have the tag set, otherwise check if one of the tasks has a tag in this location.
			if (QuestData->LocationTag == Character->LastLocationTag && !HasCompletedLocation(QuestData->LocationTag, Character))
			{
				bIsInLocation = true;
			}
			else
			{
				for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
				{
					QuestSoftPtr.LoadSynchronous();
					UClass* QuestBaseTaskClass = QuestSoftPtr.Get();
					if (!QuestBaseTaskClass) continue;

					UIQuestItemTask* ItemTaskClass = Cast<UIQuestItemTask>(QuestBaseTaskClass->GetDefaultObject());
					if (!ItemTaskClass) continue;

					if (ItemTaskClass->Tag != Character->LastLocationTag || HasCompletedLocation(ItemTaskClass->Tag, Character)) continue;

					bIsInLocation = true;
					break;
				}
			}

			if (!bIsInLocation) continue;
		}
		SelectedQuests.Add(QuestData);
	}

	if (!SelectedQuests.IsEmpty())
	{
		OnLoadAssetsDelegate.ExecuteIfBound(SelectedQuests);
	}
}

void AIQuestManager::GetQuestByName(const FString& DisplayName, FQuestIDLoaded QuestIDLoaded, const AIBaseCharacter* Character)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetQuestByName"))

	const FString TrimmedDisplayName = DisplayName.TrimStartAndEnd().ToLower();

	FQuestsIDsLoaded LoadAssetIDsDelegate;
	TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
	LoadAssetIDsDelegate.BindLambda([WeakThis, QuestIDLoaded, DisplayName](TArray<FPrimaryAssetId> QuestAssetIds)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnGetQuestByName(QuestAssetIds, QuestIDLoaded, DisplayName.TrimStartAndEnd().ToLower());
		}
	});

	GetLoadedQuestAssetIDsFromType(EQuestFilter::ALL, Character, LoadAssetIDsDelegate);
}

void AIQuestManager::OnGetQuestByName(TArray<FPrimaryAssetId> QuestAssetIds, FQuestIDLoaded QuestIDLoaded, const FString TrimmedDisplayName)
{
	for (const FPrimaryAssetId& QuestAssetId : QuestAssetIds)
	{
		FText QuestName = FText::GetEmpty();
		if (!GetQuestDisplayName(QuestAssetId, QuestName))
		{
			continue;
		}
		FString QuestNameString = QuestName.ToString().TrimStartAndEnd().ToLower();
		if (TrimmedDisplayName == QuestNameString)
		{
			QuestIDLoaded.ExecuteIfBound(QuestAssetId);
			return;
		}

		QuestNameString.RemoveSpacesInline();
		if (TrimmedDisplayName == QuestNameString)
		{
			QuestIDLoaded.ExecuteIfBound(QuestAssetId);
			return;
		}
	}
	QuestIDLoaded.ExecuteIfBound(FPrimaryAssetId());
}

bool AIQuestManager::GetQuestDisplayName(const FPrimaryAssetId Quest, FText& OutDisplayName) const
{
	UTitanAssetManager& AssetMgr = UTitanAssetManager::Get();
	UQuestData* QuestData = Cast<UQuestData>(AssetMgr.GetPrimaryAssetObject(Quest));
	if (QuestData)
	{
		OutDisplayName = QuestData->DisplayName;
		return true;
	}
	else
	{
		QuestData = AIQuestManager::LoadQuestData(Quest);
		if (QuestData)
		{
			OutDisplayName = QuestData->DisplayName;
			return true;
		}
	}
	return false;
}

void AIQuestManager::AssignNextHatchlingTutorialQuest(AIBaseCharacter* TargetCharacter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignNextHatchlingTutorialQuest"))

	check(TargetCharacter);
	if (!TargetCharacter)
	{
		UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::AssignNextHatchlingTutorialQuest: TargetCharacter nullptr"));
		return;
	}

	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	AIHatchlingCave* IHatchlingCave = Cast<AIHatchlingCave>(TargetCharacter->GetCurrentInstance());
	if (!IHatchlingCave)
	{
		UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::AssignNextHatchlingTutorialQuest: IHatchlingCave nullptr"));
		return;
	}

	FQuestLineItem QuestLineItem = IHatchlingCave->GetTutorialQuest(TargetCharacter->HatchlingTutorialIndex);

	if (!QuestLineItem.QuestId.IsValid())
	{
		UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::AssignNextHatchlingTutorialQuest: QuestId invalid"));
	}

	bool bHasTags = !QuestLineItem.RequiredTags.IsEmpty();
	bool bPlayerHasOneTag = false;
	for (FName& Tag : QuestLineItem.RequiredTags)
	{
		if (TargetCharacter->QuestTags.Contains(Tag))
		{
			bPlayerHasOneTag = true;
			break;
		}
	}

	if (bHasTags && !bPlayerHasOneTag)
	{
		// Quest is not compatible, so increment tutorial index
		TargetCharacter->HatchlingTutorialIndex++;
		AssignNextHatchlingTutorialQuest(TargetCharacter);
		return;
	}

	AssignQuest(QuestLineItem.QuestId, TargetCharacter, true);
	

	SaveQuest(TargetCharacter);
}

void AIQuestManager::AssignRandomPersonalQuests(AIBaseCharacter* TargetCharacter)
{
	check(IsValid(TargetCharacter));
	
	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	const int32 MaxPersonalQuests = 2;
	int32 CurrentPersonalQuests = 0;

	for (UIQuest* ActiveQuest : TargetCharacter->GetActiveQuests())
	{
		if (!ActiveQuest || !ActiveQuest->QuestData) continue;

		if (ActiveQuest->QuestData->QuestShareType == EQuestShareType::Personal) CurrentPersonalQuests++;
	}

	const int32 PersonalQuestsNeeded = (MaxPersonalQuests - CurrentPersonalQuests);

	if (PersonalQuestsNeeded == 0) return;

	for (int32 i = 0; i < PersonalQuestsNeeded; i++)
	{
		AssignRandomQuest(TargetCharacter);
	}
}

void AIQuestManager::AssignRandomQuest(AIBaseCharacter* TargetCharacter, bool bRefreshNotice /*= false*/, AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/, bool bForGroup /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignRandomQuest"))

	SCOPE_CYCLE_COUNTER(STAT_QuestAssignRandom);

	if (!bForGroup && TargetCharacter && !TargetCharacter->CanHaveQuests())
	{
		return;
	}

	if (bForGroup)
	{
		check(PlayerGroupActor);
	}
	else
	{
		// Warning: Should never pass in null variables here!!
		check(IsValid(TargetCharacter));
	}

	FQuestIDLoaded QuestDataloaded;
	TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
	TWeakObjectPtr<AIBaseCharacter> WeakTargetCharacter = MakeWeakObjectPtr(TargetCharacter);
	TWeakObjectPtr<AIPlayerGroupActor> WeakPlayerGroupActor = MakeWeakObjectPtr(PlayerGroupActor);

	QuestDataloaded.BindLambda([WeakThis, WeakTargetCharacter, bRefreshNotice, WeakPlayerGroupActor, bForGroup](FPrimaryAssetId QuestAssetId) {
		if (bForGroup && WeakThis.IsValid() && WeakTargetCharacter.IsValid() && WeakPlayerGroupActor.IsValid())
		{
			WeakThis->OnAssignRandomQuest(WeakTargetCharacter.Get(), bRefreshNotice, WeakPlayerGroupActor.Get(), bForGroup, QuestAssetId);
		}
		else if (WeakThis.IsValid() && WeakTargetCharacter.IsValid())
		{
			WeakThis->OnAssignRandomQuest(WeakTargetCharacter.Get(), bRefreshNotice, nullptr, bForGroup, QuestAssetId);
		}
	});

	if (bForGroup)
	{
		GetRandomGroupQuest(PlayerGroupActor, QuestDataloaded);
	}
	else
	{
		GetRandomQuest(TargetCharacter, QuestDataloaded, EQuestShareType::Personal);
	}
}

void AIQuestManager::OnAssignRandomQuest(AIBaseCharacter* TargetCharacter, bool bRefreshNotice /*= false*/, AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/, bool bForGroup /*= false*/, FPrimaryAssetId QuestAssetId)
{
	if (!QuestAssetId.IsValid())
	{
		if (TargetCharacter)
		{
			//UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::AssignRandomQuest() - !QuestAssetId.IsValid() for TargetCharacter %s"), *TargetCharacter->GetName());
		}

		// no quests found
		return;
	}

	AssignQuest(QuestAssetId, TargetCharacter, bRefreshNotice, PlayerGroupActor, bForGroup);
}

bool AIQuestManager::ShouldAutoTrack(UQuestData* QuestData, AIBaseCharacter* Character)
{
	if (!Character) return true;

	AIPlayerState* IPlayerState = Character->GetPlayerState<AIPlayerState>();
	if (!IPlayerState) return true;

	if (IPlayerState->GetPlatform() == EPlatformType::PT_ANDROID || IPlayerState->GetPlatform() == EPlatformType::PT_IOS)
	{
		return QuestData->QuestType == EQuestType::Tutorial || QuestData->QuestShareType == EQuestShareType::Survival;
	}
	else
	{
		return true;
	}
}

void AIQuestManager::AssignQuest(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter, bool bRefreshNotice /*= false*/, AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/, bool bForGroup /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignQuest"))

	SCOPE_CYCLE_COUNTER(STAT_QuestAssign);

	if (!bForGroup && TargetCharacter && !TargetCharacter->CanHaveQuests())
	{
		return;
	}

	if (bForGroup)
	{
		// Warning: Should never pass in null variables here!!
		if (!IsValid(PlayerGroupActor))
		{
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignQuest: bForGroup is true but PlayerGroupActor is nullptr"));
			return;
		}

		if (!PlayerGroupActor->ValidateAndPropagateGroupMemberQuestAvailabilityStatus())
		{
			return;
		}
	}
	else
	{
		// Warning: Should never pass in null variables here!!
		check(IsValid(TargetCharacter));
	}

	if (!QuestAssetId.IsValid())
	{
		// no quests found
		UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignQuest: QuestAssetId Invalid: %s"), *QuestAssetId.ToString());
		return;
	}

	// Attempt to Load the related Quest Data
	UQuestData* QuestData = LoadQuestData(QuestAssetId);
	check(QuestData)
	if (!QuestData)
	{
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignQuest: Failed to load QuestData for QuestAssetId: %s"), *QuestAssetId.ToString());
		return;
	}

	// Create new Quest Object and Init it
	UIQuest* NewQuest = nullptr;
	if (bForGroup)
	{
		NewQuest = NewObject<UIQuest>(PlayerGroupActor, UIQuest::StaticClass());
	}
	else
	{
		NewQuest = NewObject<UIQuest>(TargetCharacter, UIQuest::StaticClass());
	}

	check(NewQuest);
	NewQuest->Setup();
	NewQuest->SetQuestId(QuestAssetId);
	NewQuest->QuestData = QuestData;
	NewQuest->SetRemainingTime(QuestData->TimeLimit);
	NewQuest->SetPlayerGroupActor(PlayerGroupActor);
	NewQuest->SetIsTracked(ShouldAutoTrack(QuestData, TargetCharacter));

	TArray<TSoftClassPtr<UIQuestBaseTask>> TasksToAdd{};
	NewQuest->ComputeValidTasks(TasksToAdd);
	if (!TasksToAdd.IsEmpty())
	{
		TArray<FSoftObjectPath> AsyncObjects;

		for (const TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : TasksToAdd)
		{
			if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
			{
				UIQuestBaseTask* QuestTask = nullptr;
				if (bForGroup)
				{
					QuestTask = NewObject<UIQuestBaseTask>(PlayerGroupActor, QuestBaseTaskClass);
				}
				else
				{
					QuestTask = NewObject<UIQuestBaseTask>(TargetCharacter, QuestBaseTaskClass);
				}

				if (QuestTask)
				{
					QuestTask->SetParentQuest(NewQuest);
					if (UIQuestFeedMember* FeedMemberTask = Cast<UIQuestFeedMember>(QuestTask))
					{
						//get a valid target group member
						AIBaseCharacter* TargetMember = nullptr;
						for (AIPlayerState* IPS : PlayerGroupActor->GetGroupMembers())
						{
							if (AIBaseCharacter* IBC = IPS->GetPawn<AIBaseCharacter>())
							{
								if ((IBC->GetHunger() / IBC->GetMaxHunger()) <= 0.25f)
								{
									TargetMember = IBC;
								}
							}
						}
						FeedMemberTask->Setup(TargetMember);
					}
					else
					{
						QuestTask->Setup();
					}
					NewQuest->GetQuestTasks_Mutable().Add(QuestTask);
					UpdateTaskLocation(QuestTask);
				}
			}
			else {
				FSoftObjectPath SoftPath = QuestSoftPtr.ToSoftObjectPath();
				AsyncObjects.Add(SoftPath);

				UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignRandomQuest: Load Soft Path: %s"), *SoftPath.ToString());
			}
		}

		if (!AsyncObjects.IsEmpty())
		{
			FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
			NewQuest->AddToRoot(); // prevent quest from being garbage collected in between loads
			FStreamableDelegate PostLoad = FStreamableDelegate::CreateUObject(this, &AIQuestManager::AssignQuestLoaded, QuestAssetId, TargetCharacter, NewQuest, QuestData, bRefreshNotice, PlayerGroupActor, bForGroup);
			AssetLoader.RequestAsyncLoad(AsyncObjects, PostLoad, FStreamableManager::AsyncLoadHighPriority, false);
		}
		else {
			if (NewQuest->IsValidLowLevel())
			{
				if (bForGroup)
				{
					PlayerGroupActor->AddGroupQuest(NewQuest);
					//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("AIQuestManager::AssignQuest - PlayerGroupActor->GroupQuests.Add")));
					PlayerGroupActor->OnRep_GroupQuest();
				}
				else
				{
					TargetCharacter->GetActiveQuests_Mutable().Add(NewQuest);
#if !UE_SERVER
					if (!IsRunningDedicatedServer())
					{
						TargetCharacter->OnRep_ActiveQuests();
					}
#endif

					if (bRefreshNotice)
					{
						AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
						if (IPlayerController)
						{
							IPlayerController->OnQuestRefresh(NewQuest, NewQuest->GetQuestId());
						}
					}
				}
			}
			else {
				UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignRandomQuest: Error Quest Invalid."));
				return;
			}
		}
	}
}

void AIQuestManager::AssignQuestLoaded(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter, UIQuest* NewQuest, UQuestData* QuestData, bool bRefreshNotice /*= false*/, AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/, bool bForGroup /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignQuestLoaded"))

	if (NewQuest == nullptr)
	{
		// Quest got garbage collected or missing?
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignQuestLoaded: %s NewQuest is nullptr"), *QuestAssetId.ToString());
		return;
	}

	NewQuest->RemoveFromRoot();

	if (bForGroup)
	{
		if (PlayerGroupActor == nullptr)
		{
			// PlayerGroupActor went mia while spawning quest
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignQuestLoaded: %s PlayerGroupActor is nullptr"), *QuestAssetId.ToString());
			return;
		}
	}
	else
	{
		if (TargetCharacter == nullptr || !TargetCharacter->CanHaveQuests())
		{
			// Character went mia while spawning quest
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignQuestLoaded: %s TargetCharacter is nullptr, or cannot have quests"), *QuestAssetId.ToString());
			return;
		}
	}

	if (QuestData == nullptr)
	{
		// Quest Data is invalid
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignQuestLoaded: %s QuestData is nullptr"), *QuestAssetId.ToString());
		return;
	}

	int32 NumQuestTasks = QuestData->QuestTasks.Num();
	if (NumQuestTasks == 0) return; // No Quest Tasks

	TArray<UIQuestBaseTask*> NewQuestTasks;
	NewQuestTasks.Reserve(NumQuestTasks);

	for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
	{
		UClass* QuestBaseTaskClass = QuestSoftPtr.Get();
		if (QuestBaseTaskClass != nullptr)
		{
			UIQuestBaseTask* QuestTask;
			if (bForGroup)
			{
				QuestTask = NewObject<UIQuestBaseTask>(PlayerGroupActor, QuestBaseTaskClass);
			}
			else
			{
				QuestTask = NewObject<UIQuestBaseTask>(TargetCharacter, QuestBaseTaskClass);
			}

			if (QuestTask)
			{
				QuestTask->SetParentQuest(NewQuest);
				QuestTask->Setup();
				NewQuestTasks.Add(QuestTask);
				UpdateTaskLocation(QuestTask);
			}
		}
		else {
			UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignRandomQuest: QuestBaseTaskClass is nullptr."));
		}
	}

	if (NewQuest->IsValidLowLevel())
	{
		NewQuest->GetQuestTasks_Mutable() = NewQuestTasks;

		if (bForGroup)
		{
			//UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::AssignQuestLoaded() - bForGroup"));
			PlayerGroupActor->AddGroupQuest(NewQuest);
			//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Green, FString::Printf(TEXT("AIQuestManager::AssignQuestLoaded - PlayerGroupActor->GroupQuests.Add")));
			PlayerGroupActor->OnRep_GroupQuest();
		}
		else
		{
			TargetCharacter->GetActiveQuests_Mutable().Add(NewQuest);

#if !UE_SERVER
			if (!IsRunningDedicatedServer())
			{
				TargetCharacter->OnRep_ActiveQuests();
			}
#endif
		}

		if (bRefreshNotice && !bForGroup)
		{
			AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
			if (IPlayerController)
			{
				IPlayerController->OnQuestRefresh(NewQuest, NewQuest->GetQuestId());
			}
		}
	}
	else {
		UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignQuestLoaded: Error Quest Invalid."));
	}
}

void AIQuestManager::AssignWaterReplenishQuest(AIBaseCharacter* TargetCharacter, FName WaterTag)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignWaterReplenishQuest"))

	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	if (HasAuthority())
	{
		UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);

		AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
		check(Session);

		if (Session->bServerLocalWorldQuests)
		{
			// Warning: Should never pass in null variables here!!
			check(IsValid(TargetCharacter));

			FPrimaryAssetId QuestAssetId;

			TArray<FPrimaryAssetId> Quests = WaterRestoreQuests;

			if (!Quests.IsEmpty() || !TargetCharacter->LastLocationEntered || TargetCharacter->LastLocationTag != WaterTag)
			{
				for (const FPrimaryAssetId& QuestSelected : Quests)
				{
					if (!QuestSelected.IsValid())
					{
						continue;
					}

					// Load Quest Data for more complicated quest type picking
					UQuestData* QuestData = LoadQuestData(QuestSelected);
					if (!QuestData || !QuestData->bEnabled)
					{
						// Failed to load Quest Data
						continue;
					}
					check(QuestData);

					if (TargetCharacter)
					{
						if (QuestData->LocationTag != NAME_None)
						{
							// Location Quests: Don't give group a location quest that it's leader isn't inside
							if (TargetCharacter->LastLocationTag != NAME_None)
							{
								bool bIsInLocation = false;

								if (QuestData->LocationTag == TargetCharacter->LastLocationTag)
								{
									bIsInLocation = true;
								}

								if (!bIsInLocation) continue;
							}
						}
						else
						{
							// Location Quests: Don't give group a location quest that it's leader isn't inside
							if (TargetCharacter->LastLocationTag != NAME_None)
							{
								if (!QuestData->QuestTasks.IsEmpty())
								{
									bool bIsInLocation = false;

									for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
									{
										QuestSoftPtr.LoadSynchronous();
										if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
										{
											if (UIQuestItemTask* ItemTaskClass = NewObject<UIQuestItemTask>(TargetCharacter, QuestBaseTaskClass))
											{
												if (ItemTaskClass->Tag == TargetCharacter->LastLocationTag)
												{
													bIsInLocation = true;
													break;
												}
											}
										}
									}

									if (!bIsInLocation) continue;
								}
							}
						}
					}

					// If we got this far we got a valid quest so return it
					QuestAssetId = QuestSelected;
					break;
				}
			}

			if (!QuestAssetId.IsValid())
			{
				// no quests found
				return;
			}

			AssignLocalWorldQuest(QuestAssetId, TargetCharacter);
		}
	}
}

void AIQuestManager::AssignRandomLocalQuest(AIBaseCharacter* TargetCharacter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignRandomLocalQuest"))

	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	if (HasAuthority())
	{
		UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);

		AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
		check(Session);

		if (Session->bServerLocalWorldQuests)
		{
			// Warning: Should never pass in null variables here!!
			check(IsValid(TargetCharacter));
			
			if (TargetCharacter->HasLeftHatchlingCave())
			{
				UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::AssignRandomLocalQuest() - TargetCharacter = %s"), *TargetCharacter->GetName());

				// Load Quest Data for more complicated quest type picking
				FQuestsDataLoaded LoadAssetsDelegate;
				TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
				TWeakObjectPtr<AIBaseCharacter> WeakTargetCharacter = MakeWeakObjectPtr(TargetCharacter);

				LoadAssetsDelegate.BindLambda([WeakThis, WeakTargetCharacter](TArray<UQuestData*> QuestsData)
				{
					if (WeakThis.IsValid() && WeakTargetCharacter.IsValid())
					{
						WeakThis->OnAssignRandomLocalQuest(QuestsData, WeakTargetCharacter.Get());
					}
				});

				GetLocalWorldQuests(TargetCharacter, LoadAssetsDelegate);
			}
		}
	}
}

void AIQuestManager::OnAssignRandomLocalQuest(TArray<UQuestData*> PossibleQuestAssets, AIBaseCharacter* TargetCharacter)
{
	UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::AssignRandomLocalQuest() - PossibleQuestAssets = %i"), PossibleQuestAssets.Num());

	bool bHasNonPriority = false;
	const TArray<UIQuest*>& QuestsToCheck = TargetCharacter->GetActiveQuests();
	if (!QuestsToCheck.IsEmpty())
	{
		for (UIQuest* Quest : QuestsToCheck)
		{
			if (Quest->QuestData && Quest->QuestData->QuestShareType == EQuestShareType::LocalWorld && !Quest->QuestData->bPriority)
			{
				UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::AssignRandomLocalQuest() - bHasNonPriority | DisplayName %s"), *Quest->QuestData->DisplayName.ToString());
				bHasNonPriority = true;
				break;
			}
		}
	}

	for (UQuestData* QuestData : PossibleQuestAssets)
	{
		check(QuestData);

		if (!bHasNonPriority || QuestData->bPriority)
		{
			AssignLocalWorldQuest(QuestData->GetPrimaryAssetId(), TargetCharacter);
		}
		else if (bHasNonPriority)
		{
			UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::AssignRandomLocalQuest() - AssignLocalWorldQuest not called because bHasNonPriority"));
		}
		else if (!QuestData->bPriority)
		{
			UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::AssignRandomLocalQuest() - AssignLocalWorldQuest not called because !QuestData->bPriority"));
		}

		if (!QuestData->bPriority && !bHasNonPriority)
		{
			bHasNonPriority = true;
			UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::AssignRandomLocalQuest() - bHasNonPriority set to true as a none priority quest has been assigned!"));
		}
	}
}

void AIQuestManager::AssignLocalWorldQuest(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignLocalWorldQuest"))

	// Warning: Should never pass in null variables here!!
	check(IsValid(TargetCharacter));
	
	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	if (!QuestAssetId.IsValid()) return;

	// Attempt to Load the related Quest Data
	UQuestData* QuestData = LoadQuestData(QuestAssetId);
	check(QuestData);

	// Create new Quest Object and Init it
	UIQuest* NewQuest = nullptr;
	NewQuest = NewObject<UIQuest>(TargetCharacter, UIQuest::StaticClass());

	check(NewQuest);
	NewQuest->Setup();
	NewQuest->SetQuestId(QuestAssetId);
	NewQuest->QuestData = QuestData;
	NewQuest->SetRemainingTime(QuestData->TimeLimit);
	NewQuest->SetIsTracked(ShouldAutoTrack(QuestData, TargetCharacter));

	if (!QuestData->QuestTasks.IsEmpty())
	{
		TArray<FSoftObjectPath> AsyncObjects;

		for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
		{
			if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
			{
				if (UIQuestBaseTask* QuestTask = QuestTask = NewObject<UIQuestBaseTask>(TargetCharacter, QuestBaseTaskClass))
				{
					QuestTask->SetParentQuest(NewQuest);
					QuestTask->Setup();
					NewQuest->GetQuestTasks_Mutable().Add(QuestTask);
					UpdateTaskLocation(QuestTask);
				}
			}
			else {
				FSoftObjectPath SoftPath = QuestSoftPtr.ToSoftObjectPath();
				AsyncObjects.Add(SoftPath);

				UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignLocalWorldQuest: Load Soft Path: %s"), *SoftPath.ToString());
			}
		}

		if (!AsyncObjects.IsEmpty())
		{
			FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
			NewQuest->AddToRoot(); // prevent quest from being garbage collected in between loads
			FStreamableDelegate PostLoad = FStreamableDelegate::CreateUObject(this, &AIQuestManager::AssignLocalWorldQuestLoaded, QuestAssetId, TargetCharacter, NewQuest, QuestData);
			AssetLoader.RequestAsyncLoad(AsyncObjects, PostLoad, FStreamableManager::AsyncLoadHighPriority, false);
		}
		else {
			if (NewQuest->IsValidLowLevel())
			{
				TargetCharacter->GetActiveQuests_Mutable().Add(NewQuest);

#if !UE_SERVER
				if (!IsRunningDedicatedServer())
				{
					TargetCharacter->OnRep_ActiveQuests();
				}
#endif
			}
			else {
				UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignLocalWorldQuest: Error Quest Invalid."));
				return;
			}
		}
	}
}

void AIQuestManager::AssignLocalWorldQuestLoaded(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter, UIQuest* NewQuest, UQuestData* QuestData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::AssignLocalWorldQuestLoaded"))

	if (NewQuest == nullptr)
	{
		// Quest got garbage collected or missing?
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignLocalWorldQuestLoaded: %s NewQuest is nullptr"), *QuestAssetId.ToString());
		return;
	}

	NewQuest->RemoveFromRoot();

	if (TargetCharacter == nullptr)
	{
		// Character went mia while spawning quest
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignLocalWorldQuestLoaded: %s TargetCharacter is nullptr"), *QuestAssetId.ToString());
		return;
	}

	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	if (QuestData == nullptr)
	{
		// Quest Data is invalid
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:AssignLocalWorldQuestLoaded: %s QuestData is nullptr"), *QuestAssetId.ToString());
		return;
	}

	int32 NumQuestTasks = QuestData->QuestTasks.Num();
	if (NumQuestTasks == 0) return; // No Quest Tasks

	TArray<UIQuestBaseTask*> NewQuestTasks;
	NewQuestTasks.Reserve(NumQuestTasks);

	for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
	{
		UClass* QuestBaseTaskClass = QuestSoftPtr.Get();
		if (QuestBaseTaskClass != nullptr)
		{
			if (UIQuestBaseTask* QuestTask = NewObject<UIQuestBaseTask>(TargetCharacter, QuestBaseTaskClass))
			{
				QuestTask->SetParentQuest(NewQuest);
				QuestTask->Setup();
				NewQuestTasks.Add(QuestTask);
				UpdateTaskLocation(QuestTask);
			}
		}
		else {
			UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignLocalWorldQuestLoaded: QuestBaseTaskClass is nullptr."));
		}
	}

	if (NewQuest->IsValidLowLevel())
	{
		NewQuest->GetQuestTasks_Mutable() = NewQuestTasks;

		TargetCharacter->GetActiveQuests_Mutable().Add(NewQuest);

#if !UE_SERVER
		if (!IsRunningDedicatedServer())
		{
			TargetCharacter->OnRep_ActiveQuests();
		}
#endif
	}
	else {
		UE_LOG(TitansQuests, Log, TEXT("QuestManager:AssignRandomQuest: Error Quest Invalid."));
	}
}

bool AIQuestManager::HasLocalQuest(AIBaseCharacter* TargetCharacter) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::HasLocalQuest"))

	bool bLocalWorldQuestExists = false;

	if (TargetCharacter && TargetCharacter->IsValidLowLevel())
	{
		for (UIQuest* ActiveQuest : TargetCharacter->GetActiveQuests())
		{
			if (ActiveQuest && ActiveQuest->IsValidLowLevel() && ActiveQuest->QuestData->QuestShareType == EQuestShareType::LocalWorld)
			{
				bLocalWorldQuestExists = true;
				break;
			}
		}
	}

	return bLocalWorldQuestExists;
}

bool AIQuestManager::HasTutorialQuest(const AIBaseCharacter* TargetCharacter) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::HasTutorialQuest"))

	bool bTutorialQuestExists = false;

	if (TargetCharacter && TargetCharacter->IsValidLowLevel())
	{
		for (UIQuest* ActiveQuest : TargetCharacter->GetActiveQuests())
		{
			if (ActiveQuest && ActiveQuest->IsValidLowLevel() && ActiveQuest->QuestData && ActiveQuest->QuestData->QuestType == EQuestType::Tutorial)
			{
				bTutorialQuestExists = true;
				break;
			}
		}
	}

	return bTutorialQuestExists;
}

void AIQuestManager::ClearTutorialQuests(AIBaseCharacter* TargetCharacter)
{
	if (TargetCharacter && TargetCharacter->IsValidLowLevel())
	{
		// Backwards For Loop as quests can be removed when they are completed
		// Intentionally backwards because you can't do this forwards without
		// invalidating the array or length
		for (int32 i = TargetCharacter->GetActiveQuests().Num(); i-- > 0;)
		{
			UIQuest* ActiveQuest = TargetCharacter->GetActiveQuests()[i];

			if (ActiveQuest && ActiveQuest->IsValidLowLevel() && ActiveQuest->QuestData->QuestType == EQuestType::Tutorial)
			{
				OnQuestCompleted(TargetCharacter, ActiveQuest);
			}
		}
		
		TargetCharacter->QueueTutorialPrompt(FTutorialPrompt()); // Clear tutorial prompt
	}
}

void AIQuestManager::ClearNonTutorialQuests(AIBaseCharacter* TargetCharacter)
{
	if (TargetCharacter && TargetCharacter->IsValidLowLevel())
	{
		// Backwards For Loop as quests can be removed when they are completed
		// Intentionally backwards because you can't do this forwards without
		// invalidating the array or length
		for (int32 i = TargetCharacter->GetActiveQuests().Num(); i-- > 0;)
		{
			UIQuest* ActiveQuest = TargetCharacter->GetActiveQuests()[i];

			if (ActiveQuest && ActiveQuest->IsValidLowLevel() && ActiveQuest->QuestData->QuestType != EQuestType::Tutorial)
			{
				OnQuestFail(TargetCharacter, ActiveQuest, false);
			}
		}
	}
}

bool AIQuestManager::GroupMemberHasGroupMeetCooldown(AIPlayerGroupActor* PlayerGroupActor) const
{
	if (!PlayerGroupActor) return true;

	const TArray<AIPlayerState*> GroupMembers = PlayerGroupActor->GetGroupMembers();

	for (AIPlayerState* GroupMember : GroupMembers)
	{
		if (!GroupMember) continue;
		AIBaseCharacter* IBaseChar = GroupMember->GetPawn<AIBaseCharacter>();
		if (!IBaseChar) continue;

		if (GroupMeetQuestCooldowns.Contains(FQuestCooldown(FPrimaryAssetId(), IBaseChar->GetCharacterID(), 0.0f))) return true;
	}

	return false;
}

bool AIQuestManager::ShouldAssignGroupMeetQuest(AIPlayerGroupActor* PlayerGroupActor) const
{
	if (!PlayerGroupActor || !PlayerGroupActor->GetGroupLeader() || PlayerGroupActor->GetGroupMembers().IsEmpty()) return false;
	// 2500 Meters
	const int32 DistanceRequired = 150000;
	const float DistantMembersPercentageReq = 0.4f;
	const int32 MemberReq = 2;
	const int32 MemberCount = PlayerGroupActor->GetGroupMembers().Num();

	if (MemberCount < MemberReq) return false;

	if (AIBaseCharacter* IBC = PlayerGroupActor->GetGroupLeader()->GetPawn<AIBaseCharacter>())
	{
		int32 DistantMembers = 0;
		FVector LeaderLocation = IBC->GetActorLocation();

		if (IBC->GetCurrentInstance())
		{
			FInstanceLogoutSaveableInfo* InstanceLogoutSaveableInfo = IBC->GetInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));
			if (InstanceLogoutSaveableInfo) LeaderLocation = InstanceLogoutSaveableInfo->CaveReturnLocation;	
		}
		
		bool bLeaderLocationEntered = IBC->LastLocationEntered;
		FName LeaderLocationTag = IBC->LastLocationTag;

		for (AIPlayerState* GroupMember : PlayerGroupActor->GetGroupMembers())
		{
			IBC = GroupMember->GetPawn<AIBaseCharacter>();
			if (!IBC) continue;

			// Don't assign group meet quest while any member is in the hatchling cave
			if (!IBC->HasLeftHatchlingCave() && UIGameplayStatics::AreHatchlingCavesEnabled(this))
			{
				return false;
			}

			// If we have a cooldown on one of our members return false
			if (GroupMeetQuestCooldownDelay > 0.0f)
			{
				for (FQuestCooldown GroupMeetQuestCooldown : GroupMeetQuestCooldowns)
				{
					if (GroupMeetQuestCooldown.CharacterID == IBC->GetCharacterID())
					{
						return false;
					}
				}
			}

			if (GroupMember == PlayerGroupActor->GetGroupLeader()) continue;

			// Don't attempt to find distance if they're in the same POI
			if (bLeaderLocationEntered && IBC->LastLocationEntered && LeaderLocationTag == IBC->LastLocationTag) continue;
			
			FVector MemberLocation = IBC->GetActorLocation();

			if (IBC->GetCurrentInstance())
			{
				FInstanceLogoutSaveableInfo* InstanceLogoutSaveableInfo = IBC->GetInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));
				if (InstanceLogoutSaveableInfo) MemberLocation = InstanceLogoutSaveableInfo->CaveReturnLocation;	
			}
			
			float MemberDistance = (LeaderLocation - MemberLocation).Size();
			if (MemberDistance >= DistanceRequired)
			{
				DistantMembers++;
				continue;
			}
		}

		float DistantMembersPercentage = (float)((float)DistantMembers / (float)MemberCount);

		//GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString::Printf(TEXT("AIQuestManager::ShouldAssignGroupMeetQuest - %f >= %f"), DistantMembersPercentage, DistantMembersPercentageReq));
		return (DistantMembersPercentage >= DistantMembersPercentageReq);
	}

	return false;
}

void AIQuestManager::AssignGroupMeetQuest(AIPlayerGroupActor* PlayerGroupActor)
{
	check(PlayerGroupActor);

	FPrimaryAssetId SelectedGroupMeetQuest = FPrimaryAssetId();
	int32 ClosestPOIDistance = TNumericLimits<int32>::Max();

	TArray<AIBaseCharacter*> GroupMembersCharacters = PlayerGroupActor->GetGroupMembersCharacters();

	AIBaseCharacter* GroupLeaderChar = PlayerGroupActor->GetGroupLeader()->GetPawn<AIBaseCharacter>();
	check(GroupLeaderChar);

	FVector GroupLeaderLocation = GroupLeaderChar->GetActorLocation();
	FInstanceLogoutSaveableInfo* InstanceLogoutSaveableInfo = GroupLeaderChar->GetInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));

	if (InstanceLogoutSaveableInfo) GroupLeaderLocation = InstanceLogoutSaveableInfo->CaveReturnLocation;

	for (const FPrimaryAssetId& GroupMeetQuest : GroupMeetQuests)
	{
		// Load Quest Data for more complicated quest type picking
		UQuestData* const QuestData = LoadQuestData(GroupMeetQuest);
		if (!QuestData || !QuestData->bEnabled) continue;
		check(QuestData);

		// If the dinos on the group don't have the necessary tags, don't even try to assign this quest to them
		TArrayView<const AIBaseCharacter*> GroupCharactersView = MakeArrayView(const_cast<const AIBaseCharacter**>(GroupMembersCharacters.GetData()), GroupMembersCharacters.Num());
		if (!DoGroupMembersContainQuestTag(GroupCharactersView, QuestData->QuestTag))
		{
			continue;
		}

		for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
		{
			QuestSoftPtr.LoadSynchronous();
			if (UClass* QuestBaseTaskClass = QuestSoftPtr.Get())
			{
				UIQuestExploreTask* ExploreTaskClass = NewObject<UIQuestExploreTask>(PlayerGroupActor, QuestBaseTaskClass);
				if (ExploreTaskClass)
				{
					for (AActor* POI : AllPointsOfInterest)
					{
						AIPointOfInterest* SpherePOI = Cast<AIPointOfInterest>(POI);
						AIPOI* MeshPOI = Cast<AIPOI>(POI);

						if ((MeshPOI && MeshPOI->GetLocationTag() == ExploreTaskClass->Tag) || (SpherePOI && SpherePOI->GetLocationTag() == ExploreTaskClass->Tag))
						{
							if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
							{
								const FVector POILocation = POI->GetActorLocation();

								if (!IWorldSettings->IsInWorldBounds(POILocation)) continue;

								int32 POIDistance = (POILocation - GroupLeaderLocation).Size();
								if (POIDistance < ClosestPOIDistance)
								{
									ClosestPOIDistance = POIDistance;
									SelectedGroupMeetQuest = GroupMeetQuest;
								}
								break;
							}
						}
					}
				}
			}
		}
	}

	if (!SelectedGroupMeetQuest.IsValid())
	{
		// no quests found
		return;
	}

	AssignQuest(SelectedGroupMeetQuest, nullptr, false, PlayerGroupActor, true);
}

bool AIQuestManager::DoGroupMembersContainQuestTag(TArrayView<const AIBaseCharacter*> GroupMemberCharacters, const FName& QuestTag) const
{
	for (const AIBaseCharacter* const Character : GroupMemberCharacters)
	{
		if (QuestTag != NAME_QuestTagNone && !Character->QuestTags.Contains(QuestTag))
		{
			return false;
		}
	}
	return true;
}

void AIQuestManager::UpdateGroupQuestsOnCooldown(FAlderonUID CharacterID, FPrimaryAssetId QuestId)
{
	float WorldTimeSeconds = GetWorld()->TimeSeconds;
	FQuestCooldown NewQuestCooldown(QuestId, CharacterID, WorldTimeSeconds);

	bool bFoundExisiting = false;
	for (FQuestCooldown& QuestCooldown : GroupQuestsOnCooldown)
	{
		if (QuestCooldown == NewQuestCooldown)
		{
			QuestCooldown.Timestamp = WorldTimeSeconds;
			bFoundExisiting = true;
			break;
		}
	}

	if (!bFoundExisiting)
	{
		GroupQuestsOnCooldown.Add(NewQuestCooldown);
	}
}

void AIQuestManager::ClearCompletedQuests(AIBaseCharacter* TargetCharacter)
{
	if (!TargetCharacter) return;
	TargetCharacter->RecentCompletedQuests.Empty();
}

void AIQuestManager::UpdateCompletedLocationQuests(UIQuest* TargetQuest, AIBaseCharacter* TargetCharacter)
{
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	if (!TargetCharacter) return;
	if (TargetQuest->QuestData->LocationTag == NAME_None) return;

	FAlderonUID CharacterID = TargetCharacter->GetCharacterID();
	FLocationProgress NewLocationProgress = FLocationProgress(CharacterID, TargetQuest->QuestData->LocationTag);
	NewLocationProgress.CompletedDateTime = FDateTime::UtcNow();

	bool bFound = false;

	AIPlayerState* IPS = Cast<AIPlayerState>(TargetCharacter->GetPlayerState());
	if (!IsValid(IPS)) return;

	TArray<FLocationProgress>& LocationsProgressRef = TargetCharacter->GetLocationsProgress_Mutable();

	for (FLocationProgress& LocationProgress : LocationsProgressRef)
	{
		if (LocationProgress == NewLocationProgress)
		{
			bFound = true;
			LocationProgress.QuestsCompleted++;
			
			LocationProgress.CompletedDateTime = FDateTime::UtcNow();

			// If location has been completed, attempt to assign a exploration quest
			const float Progress = ((float)LocationProgress.QuestsCompleted / (float)GetMaxCompleteQuestsInLocation()) * 100.0f;
			if (Progress >= 100.0f)
			{
				AssignRandomPersonalQuests(TargetCharacter);
			}

			break;
		}
	}

	if (!bFound)
	{
		LocationsProgressRef.Add(NewLocationProgress);
	}
	
	if (AIGameHUD* const IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(IPS)))
	{
		IGameHUD->UpdateQuestCompletionPercentage();
	}
}

float AIQuestManager::GetLocationCompletionPercentage(FName LocationTag, AIBaseCharacter* IBaseCharacter)
{
	if (!IBaseCharacter) return 0.0f;

	if (!IBaseCharacter->CanHaveQuests())
	{
		return 0.0f;
	}

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return 0.0f;

	AIPlayerState* IPS = IBaseCharacter->GetPlayerState<AIPlayerState>();
	if (!IPS) return 0.0f;

	const TArray<FLocationProgress>& LocationsProgress = IBaseCharacter->GetLocationsProgress();

	FLocationProgress NewLocationProgress = FLocationProgress(IBaseCharacter->GetCharacterID(), LocationTag);

	for (const FLocationProgress& LocationProgress : LocationsProgress)
	{
		if (LocationProgress == NewLocationProgress)
		{
			const float Progress = ((float)LocationProgress.QuestsCompleted / (float)GetMaxCompleteQuestsInLocation()) * 100.0f;
			return Progress;
		}
	}

	return 0.0f;
}

bool AIQuestManager::HasTrophyQuestCooldown(AIBaseCharacter* TargetCharacter)
{
	if (!TargetCharacter) return false;

	if (!TargetCharacter->CanHaveQuests())
	{
		return false;
	}

	const FAlderonUID CharacterID = TargetCharacter->GetCharacterID();
	for (const FQuestCooldown& TrophyQuestCooldown : GetTrophyQuestsOnCooldown())
	{
		if (TrophyQuestCooldown.CharacterID == CharacterID)
		{
			return true;
		}
	}

	return false;
}

bool AIQuestManager::HasCompletedLocation(FName LocationTag, AIBaseCharacter* TargetCharacter)
{
	if (!TargetCharacter) return false;

	if (!TargetCharacter->CanHaveQuests())
	{
		return false;
	}

	if (GetLocationCompletionPercentage(LocationTag, TargetCharacter) >= 100.0f)
	{
		return true;
	}

	return false;
}

bool AIQuestManager::HasCompletedQuest(FPrimaryAssetId QuestId, AIBaseCharacter* TargetCharacter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::HasCompletedQuest"))
	
	if(!TargetCharacter) return false;

	TConstArrayView<FRecentQuest> RecentCompletedQuests = TargetCharacter->GetRecentCompletedQuests();

	if (!RecentCompletedQuests.IsEmpty())
	{
		for (const FRecentQuest& RecentCompletedQuest : RecentCompletedQuests)
		{
			if (RecentCompletedQuest.QuestId == QuestId) return true;
		}
	}

	return false;
}

bool AIQuestManager::HasCompletedGroupQuest(FPrimaryAssetId QuestId, AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/)
{
	TArray<FRecentQuest> RecentCompletedQuests = GetRecentCompletedGroupQuests(PlayerGroupActor);

	if (!RecentCompletedQuests.IsEmpty())
	{
		for (const FRecentQuest& RecentCompletedQuest : RecentCompletedQuests)
		{
			if (RecentCompletedQuest.QuestId == QuestId) return true;
		}
	}

	return false;
}

EQuestType AIQuestManager::GetLastQuestType(AIBaseCharacter* TargetCharacter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetLastQuestType"))
	
	if(!TargetCharacter) return EQuestType::MAX;

	TConstArrayView<FRecentQuest> RecentCompletedQuests = TargetCharacter->GetRecentCompletedQuests();

	if (!RecentCompletedQuests.IsEmpty())
	{
		return RecentCompletedQuests[(RecentCompletedQuests.Num() - 1)].QuestType;
	}

	return EQuestType::MAX;
}

TArray<FRecentQuest> AIQuestManager::GetRecentCompletedGroupQuests(AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/)
{
	TArray<FRecentQuest> RecentlyCompletedGroupQuests;
	if (PlayerGroupActor)
	{
		return PlayerGroupActor->GetRecentCompletedQuestsFromAllMembers();
	}

	return RecentlyCompletedGroupQuests;
}

bool AIQuestManager::HasFailedQuest(FPrimaryAssetId QuestId, AIBaseCharacter* TargetCharacter)
{
	if(!TargetCharacter) return false;

	TConstArrayView<FRecentQuest> RecentFailedQuests = TargetCharacter->GetRecentFailedQuests();

	if (!RecentFailedQuests.IsEmpty())
	{
		for (const FRecentQuest& RecentFailedQuest : RecentFailedQuests)
		{
			if (RecentFailedQuest.QuestId == QuestId) return true;
		}
	}

	return false;
}

bool AIQuestManager::HasFailedGroupQuest(FPrimaryAssetId QuestId, AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/)
{
	TArray<FRecentQuest> RecentFailedQuests = GetRecentFailedGroupQuests(PlayerGroupActor);

	if (!RecentFailedQuests.IsEmpty())
	{
		for (const FRecentQuest& RecentFailedQuest : RecentFailedQuests)
		{
			if (RecentFailedQuest.QuestId == QuestId) return true;
		}
	}

	return false;
}

TArray<FRecentQuest> AIQuestManager::GetRecentFailedGroupQuests(AIPlayerGroupActor* PlayerGroupActor /*= nullptr*/)
{
	TArray<FRecentQuest> RecentlyFailedGroupQuests;
	if (PlayerGroupActor)
	{
		return PlayerGroupActor->GetRecentFailedQuestsFromAllMembers();
	}

	return RecentlyFailedGroupQuests;
}

void AIQuestManager::UpdateTaskLocation(UIQuestBaseTask* QuestTask)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::UpdateTaskLocation"))

	if (UIQuestItemTask* QuestContributionTask = Cast<UIQuestItemTask>(QuestTask))
	{
		// Make Delivery Task the Closest Delivery Possible.
		if (!DeliveryLocationData.IsEmpty())
		{
			for (const FDeliveryLocationData& DeliveryLocation : DeliveryLocationData)
			{
				if (DeliveryLocation.DeliveryTag != NAME_None && DeliveryLocation.DeliveryLocation != FVector(EForceInit::ForceInitToZero))
				{
					if (DeliveryLocation.DeliveryTag == QuestContributionTask->Tag)
					{
						QuestContributionTask->SetWorldLocation(DeliveryLocation.DeliveryLocation);
						return;
					}
				}
			}
		}
	}
	else if (UIQuestExploreTask* QuestExploreTask = Cast<UIQuestExploreTask>(QuestTask))
	{
		if (!AllPointsOfInterest.IsEmpty())
		{
			for (AActor* POI : AllPointsOfInterest)
			{
				AIPointOfInterest* SpherePOI = Cast<AIPointOfInterest>(POI);
				AIPOI* MeshPOI = Cast<AIPOI>(POI);

				if ((MeshPOI && MeshPOI->GetLocationTag() == QuestExploreTask->Tag) || (SpherePOI && SpherePOI->GetLocationTag() == QuestExploreTask->Tag))
				{
					QuestExploreTask->SetWorldLocation(POI->GetActorLocation());
					return;
				}
			}
		}
	}
}

void AIQuestManager::LoadQuestsData(const TArray<FPrimaryAssetId>& QuestAssetIds, FQuestsDataLoaded OnLoaded)
{
	UTitanAssetManager& AssetMgr = UTitanAssetManager::Get();
	FStreamableManager& StreamableMgr = AssetMgr.GetStreamableManager();

	auto OnAssetsLoaded = [QuestAssetIds, OnLoaded]()
	{
		UTitanAssetManager& AssetMgr = UTitanAssetManager::Get();

		TArray<UQuestData*> QuestsData{};

		for (const FPrimaryAssetId& QuestAssetId : QuestAssetIds)
		{
			check(QuestAssetId.IsValid());
			UQuestData* QuestData = Cast<UQuestData>(AssetMgr.GetPrimaryAssetObject(QuestAssetId));
			QuestsData.Add(QuestData);
		}

		OnLoaded.ExecuteIfBound(QuestsData);
	};

	// Load assets asynchronously
	AssetMgr.LoadPrimaryAssets(QuestAssetIds, TArray<FName>(), FStreamableDelegate::CreateLambda(OnAssetsLoaded), 100);
}

void AIQuestManager::LoadQuestData(const FPrimaryAssetId& QuestAssetId, FQuestDataLoaded OnLoaded)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::LoadQuestData"))

	SCOPE_CYCLE_COUNTER(STAT_LoadQuestData);

	UTitanAssetManager& AssetMgr = UTitanAssetManager::Get();

	// Invalid Quest Asset Id - Early Out
	if (!QuestAssetId.IsValid())
	{
		OnLoaded.ExecuteIfBound(nullptr);
		return;
	}

	// Check if a asset is loaded or not
	UQuestData* QuestData = Cast<UQuestData>(AssetMgr.GetPrimaryAssetObject(QuestAssetId));

	// Already Loaded - Early Out
	if (QuestData)
	{
		OnLoaded.ExecuteIfBound(QuestData);
		return;
	}

	FStreamableDelegate PostLoad = FStreamableDelegate::CreateUObject(this, &AIQuestManager::OnQuestDataLoaded, QuestAssetId, OnLoaded);

	// Fetch Handle of Loading a Primary Asset
	TSharedPtr<FStreamableHandle> Handle = AssetMgr.LoadPrimaryAsset(QuestAssetId, TArray<FName>(), PostLoad, 100);

	// Invalid Handle
	if (!Handle.IsValid())
	{
		UE_LOG(TitansQuests, Error, TEXT("QuestManager:LoadQuestData: Invalid Handle Spawning %s"), *QuestAssetId.ToString());
		OnLoaded.ExecuteIfBound(nullptr);
		return;
	}
}

UQuestData* AIQuestManager::LoadQuestData(const FPrimaryAssetId& QuestAssetId)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::LoadQuestData"))

	UTitanAssetManager& AssetMgr = UTitanAssetManager::Get();

	// Check if a asset is loaded or not
	UQuestData* QuestData = Cast<UQuestData>(AssetMgr.GetPrimaryAssetObject(QuestAssetId));

	// Load Asset 1If Needed
	if (!QuestData)
	{
		if (!QuestAssetId.IsValid())
		{
			return nullptr;
		}

		// Fetch Handle of Loading a Primary Asset
		TSharedPtr<FStreamableHandle> Handle = AssetMgr.LoadPrimaryAsset(QuestAssetId, TArray<FName>(), FStreamableDelegate(), 100);

		// Invalid FPrimaryAssetId?
		if (!Handle.IsValid())
		{
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:LoadQuestData: Invalid Handle Spawning %s"), *QuestAssetId.ToString());
			return nullptr;
		}

		{
			START_PERF_TIME()
			Handle->WaitUntilComplete();
			END_PERF_TIME()
			ERROR_BLOCK_THREAD_STATIC()
		}

		// Check if the Asset was loaded successfully
		if (!Handle->HasLoadCompleted())
		{
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:LoadQuestData: Failed to load primary asset: %s"), *QuestAssetId.ToString());
			return nullptr;
		}

		// Check if the Item data was loaded successfully
		QuestData = Cast<UQuestData>(Handle->GetLoadedAsset());
		if (!QuestData)
		{
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:LoadQuestData: Failed to load asset data."));
			return nullptr;
		}
	}

	return QuestData;
}

void AIQuestManager::OnQuestDataLoaded(FPrimaryAssetId QuestAssetId, FQuestDataLoaded OnLoaded)
{
	UTitanAssetManager& AssetMgr = UTitanAssetManager::Get();
	check(QuestAssetId.IsValid());
	UQuestData* QuestData = Cast<UQuestData>(AssetMgr.GetPrimaryAssetObject(QuestAssetId));
	OnLoaded.ExecuteIfBound(QuestData);
}

UIQuest* AIQuestManager::LoadQuest(AIBaseCharacter* Character, const FQuestSave& QuestSave, bool bMapHasChanged, bool IsActiveQuest)
{
	if (QuestSave.BaseQuestObject.IsEmpty())
	{
		// Player has no saved quest
		return nullptr;
	}

	UIQuest* SavedQuest = NewObject<UIQuest>(Character, UIQuest::StaticClass());
	check(SavedQuest);

	FString QuestSaveString = QuestSave.BaseQuestObject;

	IAlderonDatabase::DeserializeObject(SavedQuest, QuestSaveString);

	// Attempt to Load the related Quest Data
	SavedQuest->QuestData = LoadQuestData(SavedQuest->GetQuestId());
	if (!SavedQuest->QuestData)
	{
		return nullptr;
	}

	const bool bAllowHatchlingCaveTutorialQuests = UIGameplayStatics::AreHatchlingCavesEnabled(this);

	if (SavedQuest->QuestData->QuestType == EQuestType::Tutorial && !bAllowHatchlingCaveTutorialQuests)
	{
		return nullptr;
	}

	if (IsActiveQuest && bMapHasChanged && SavedQuest->QuestData->QuestType != EQuestType::WorldTutorial)
	{
		// Only load WorldTutorial quests on map change.
		return nullptr;
	}

	SavedQuest->Setup();

	// Reserve the Quest Tasks array to avoid resizing
	TArray<UIQuestBaseTask*>& SavedQuestTasks = SavedQuest->GetQuestTasks_Mutable();
	SavedQuestTasks.Reserve(SavedQuest->QuestData->QuestTasks.Num());

	// Setup Quest Task Objects
	int32 i = 0;

	bool bInvalidLocationQuest = true;
	for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : SavedQuest->QuestData->QuestTasks)
	{
		if (!QuestSave.QuestTasks.IsValidIndex(i)) break;

		UClass* QuestBaseTaskClass = QuestSoftPtr.Get();

		if (!QuestBaseTaskClass)
		{
			START_PERF_TIME()
				QuestBaseTaskClass = QuestSoftPtr.LoadSynchronous();
			END_PERF_TIME()
				WARN_PERF_TIME_STATIC(1)
		}

		if (QuestBaseTaskClass)
		{
			if (UIQuestBaseTask* QuestTask = NewObject<UIQuestBaseTask>(Character, QuestBaseTaskClass))
			{
				if (IsActiveQuest && SavedQuest->QuestData->QuestType == EQuestType::Exploration)
				{
					if (UIQuestExploreTask* ExploreTaskClass = NewObject<UIQuestExploreTask>(Character, QuestBaseTaskClass))
					{
						if (ExploreTaskClass->bSkipIfAlreadyInside)
						{
							if (Character->LastLocationEntered && Character->LastLocationTag != NAME_None && ExploreTaskClass->Tag == Character->LastLocationTag)
							{
								bInvalidLocationQuest = true;
								break;
							}
						}

						// Try to find a location
						for (AActor* POI : AllPointsOfInterest)
						{
							AIPointOfInterest* SpherePOI = Cast<AIPointOfInterest>(POI);
							AIPOI* MeshPOI = Cast<AIPOI>(POI);

							if ((MeshPOI && MeshPOI->GetLocationTag() == ExploreTaskClass->Tag) || (SpherePOI && SpherePOI->GetLocationTag() == ExploreTaskClass->Tag))
							{
								if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
								{
									const FVector POILocation = POI->GetActorLocation();

									if (!IWorldSettings->IsInWorldBounds(POILocation))
									{
										bInvalidLocationQuest = true;
									}
									else if ((MeshPOI && MeshPOI->IsDisabled()) || (SpherePOI && SpherePOI->IsDisabled()))
									{
										bInvalidLocationQuest = true;
									}
									else
									{
										bInvalidLocationQuest = false;
									}

									break;
								}
							}
						}
					}

					if (bInvalidLocationQuest && SavedQuest->QuestData->QuestType == EQuestType::Exploration) break;
				}

				QuestTask->SetParentQuest(SavedQuest);
				if (IsActiveQuest)
				{
					QuestTask->Setup();
				}
				FString QuestSaveStringTask = QuestSave.QuestTasks[i];
				IAlderonDatabase::DeserializeObject(QuestTask, QuestSaveStringTask);
				SavedQuestTasks.Add(QuestTask);
				if (IsActiveQuest)
				{
					UpdateTaskLocation(QuestTask);
				}
			}
		}

		i++;
	}

	if (IsActiveQuest && bInvalidLocationQuest && SavedQuest->QuestData->QuestType == EQuestType::Exploration)
	{
		return nullptr;
	}

	return SavedQuest;
}

void AIQuestManager::LoadQuests(AIBaseCharacter* Character, bool bMapHasChanged)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::LoadQuest"))

	if (!Character)
	{
		return;
	}

	if (!Character->CanHaveQuests())
	{
		return;
	}

	for (const FQuestSave& QuestSave : Character->QuestSaves)
	{
		UIQuest* SavedQuest = LoadQuest(Character, QuestSave, bMapHasChanged, true);
		if (SavedQuest)
		{
			Character->GetActiveQuests_Mutable().Add(SavedQuest);
		}
	}

	TArray<UIQuest*>& MutableUncollectedRewardQuests = Character->GetUncollectedRewardQuests_Mutable();
	for (const FQuestSave& QuestSave : Character->UncollectedRewardQuestSaves)
	{
		UIQuest* SavedQuest = LoadQuest(Character, QuestSave, bMapHasChanged, false);
		if (SavedQuest)
		{
			MutableUncollectedRewardQuests.Add(SavedQuest);
		}
	}
	
	if (Character->GroupMeetupTimeSpent > 0.0f)
	{
		// We remove GroupMeetupTimeSpent from the TimeSeconds since the calculation is the Timestamp + Cooldown.
		// This removes the time they have waited from the new quest timestamp
		const float NewTimestamp = GetWorld()->TimeSeconds - Character->GroupMeetupTimeSpent;
		GroupMeetQuestCooldowns.Add(FQuestCooldown(FPrimaryAssetId(), Character->GetCharacterID(), NewTimestamp));

		// Time Remaining will be set again for save if the player logs out with a time remaining
		Character->GroupMeetupTimeSpent = 0.0f;
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (!Character->GetActiveQuests().IsEmpty())
		{
			Character->OnRep_ActiveQuests();
		}

		if (!Character->GetUncollectedRewardQuests().IsEmpty())
		{
			Character->OnRep_UncollectedRewardQuests();
		}
	}
#endif
}

void AIQuestManager::SaveQuest(AIBaseCharacter* Character)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::SaveQuest"))

	// Skip saving if we have no quest / invalid character etc
	if (!Character) return;
	if (!Character->IsValidLowLevel()) return;

	Character->QuestSaves.Empty(Character->GetActiveQuests().Num());

	for (UIQuest* Quest : Character->GetActiveQuests())
	{
		if (!Quest || !Quest->IsValidLowLevel())
		{
			continue;
		}

		UClass* QuestClass = Quest->GetClass();
		if (!QuestClass)
		{
			// Active Quest is valid but has invalid class.
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:SaveQuest: Active Quest is valid but has invalid class."));
			continue;
		}

		// If Quest is not a personal/survival quest then don't bother saving it as we will generate new ones next time they spawn
		if (Quest->QuestData && Quest->QuestData->QuestShareType != EQuestShareType::Survival && Quest->QuestData->QuestShareType != EQuestShareType::Personal && Quest->QuestData->QuestType != EQuestType::WorldTutorial)
		{
			continue;
		}

		FQuestSave QuestSave;
		bool bValidSave = IAlderonDatabase::SerializeObject(Quest, QuestSave.BaseQuestObject);
		if (!bValidSave)
		{
			continue;
		}

		const TArray<UIQuestBaseTask*>& QuestTasksToSave = Quest->GetQuestTasks();
		QuestSave.QuestTasks.Reserve(QuestTasksToSave.Num());

		for (int32 i = 0; i < QuestTasksToSave.Num(); i++)
		{
			FString Json;
			UIQuestBaseTask* QuestBaseTask = QuestTasksToSave[i];
			if (QuestBaseTask)
			{
				bool bValidTaskSave = IAlderonDatabase::SerializeObject(QuestBaseTask, Json);
				if (bValidTaskSave)
				{
					QuestSave.QuestTasks.Add(Json);
				}
			}
		}
		Character->QuestSaves.Add(QuestSave);
	}

	Character->UncollectedRewardQuestSaves.Empty(Character->GetUncollectedRewardQuests().Num());

	for (UIQuest* Quest : Character->GetUncollectedRewardQuests())
	{
		if (!Quest || !Quest->IsValidLowLevel())
		{
			continue;
		}

		UClass* QuestClass = Quest->GetClass();
		if (!QuestClass)
		{
			// Active Quest is valid but has invalid class.
			UE_LOG(TitansQuests, Error, TEXT("QuestManager:SaveQuest: Uncollected Rewards Quest is valid but has invalid class."));
			continue;
		}

		FQuestSave QuestSave;
		bool bValidSave = IAlderonDatabase::SerializeObject(Quest, QuestSave.BaseQuestObject);
		if (!bValidSave)
		{
			continue;
		}

		QuestSave.QuestTasks.Reserve(Quest->GetQuestTasks().Num());

		for (UIQuestBaseTask* QuestBaseTask : Quest->GetQuestTasks())
		{
			FString Json;
			if (QuestBaseTask)
			{
				bool bValidTaskSave = IAlderonDatabase::SerializeObject(QuestBaseTask, Json);
				if (bValidTaskSave)
				{
					QuestSave.QuestTasks.Add(Json);
				}
			}
		}

		Character->UncollectedRewardQuestSaves.Add(QuestSave);
	}
}

void AIQuestManager::ResetQuest(AIBaseCharacter* TargetCharacter, UIQuest* QuestToReset)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::ResetQuest"))

	if (!TargetCharacter) return;
	if (!TargetCharacter->IsValidLowLevel()) return;

	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	bool bRefreshNotice = false;

	if (QuestToReset)
	{
		bRefreshNotice = true;

		// Penalty for Reseting a Quest if you have one
		TargetCharacter->AddMarks(-10);

		// Flag last completed quest as the previous one before the reset so we don't get the exact same one again
		//TargetCharacter->LastQuestId = QuestToReset->QuestId;
		TargetCharacter->UpdateCompletedQuests(QuestToReset->GetQuestId(), QuestToReset->QuestData->QuestType, MaxRecentCompletedQuests);

		//UQuestData* ActiveQuestData = QuestToReset->QuestData;
		//if (ActiveQuestData && ActiveQuestData->IsValidLowLevel())
		//{
		//	TargetCharacter->LastCompletedQuestType = ActiveQuestData->QuestType;
		//}

		// Remove from ActiveQuests List
		if (TargetCharacter->GetActiveQuests().Contains(QuestToReset))
		{
			TargetCharacter->GetActiveQuests_Mutable().Remove(QuestToReset);
		}

		// Destroy Quest
		if (QuestToReset->IsValidLowLevel())
		{
			QuestToReset->ConditionalBeginDestroy();
		}
		QuestToReset = nullptr;

#if !UE_SERVER
		if (!IsRunningDedicatedServer())
		{
			TargetCharacter->OnRep_ActiveQuests();
		}
#endif
	}

	if (TargetCharacter->HasLeftHatchlingCave())
	{
		AssignRandomPersonalQuests(TargetCharacter);
		//AssignRandomQuest(TargetCharacter, bRefreshNotice);
	}
}

void AIQuestManager::OnQuestRefresh(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest)
{
	AIPlayerState* IPlayerState = Cast<AIPlayerState>(TargetCharacter->GetPlayerState());
	if (!IPlayerState) return;

	check(TargetQuest);
	check(TargetQuest->QuestData);

	AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
	if (IPlayerController)
	{
		IPlayerController->OnQuestRefresh(TargetQuest, TargetQuest->GetQuestId());
	}
}

void AIQuestManager::RemoveQuest(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest)
{
	AIPlayerState* IPlayerState = Cast<AIPlayerState>(TargetCharacter->GetPlayerState());
	if (!IPlayerState) return;

	check(TargetQuest);
	if (!TargetQuest) return;

	check(TargetQuest->QuestData);
	if (!TargetQuest->QuestData) return;

	// Remove Quest from Active Quests
	if (TargetCharacter->GetActiveQuests().Contains(TargetQuest))
	{
		TargetCharacter->GetActiveQuests_Mutable().Remove(TargetQuest);
	}

	// Let player controller know the quest failed
	AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
	if (IPlayerController)
	{
		IPlayerController->OnQuestFail(TargetQuest, TargetQuest->GetUniqueID(), TargetQuest->GetQuestId(), false);
	}

	RemoveRewardedQuestContributionEntry(TargetCharacter, TargetQuest, true);
	EQuestShareType QuestShareType = TargetQuest->QuestData->QuestShareType;
	TargetQuest->ConditionalBeginDestroy();
	TargetQuest = nullptr;

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		TargetCharacter->OnRep_ActiveQuests();
	}
#endif

	// Re-save quests
	SaveQuest(TargetCharacter);

	// Save Character / Marks
	AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
	if (IPlayerController && IGameMode)
	{
		IGameMode->SaveAll(IPlayerController, ESavePriority::Medium);
	}
}

void AIQuestManager::UpdateLocalQuestFailure(UIQuest* Quest, AIBaseCharacter* Character, bool bRemove)
{
	if (!Character) return;

	bool bAlreadyExists = false;
	if (bRemove)
	{
		bAlreadyExists = Character->RemoveLocalQuestFailure(Quest->GetQuestId());
	}
	else
	{
		bAlreadyExists = Character->AddLocalQuestFailure(Quest->GetQuestId());
	}

	Quest->SetIsFailureInbound(!bRemove);

	//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("AIQuestManager::UpdateLocalQuestFailure - bRemove: %s | bAlreadyExists: %s"), bRemove ? TEXT("True") : TEXT("False"), bAlreadyExists ? TEXT("True") : TEXT("False")));
	if (bRemove || (!bRemove && !bAlreadyExists))
	{
		Quest->SetRemainingTime(Quest->QuestData->TimeLimit);
	}
}

void AIQuestManager::UpdateGroupQuestFailure(UIQuest* Quest, AIPlayerGroupActor* PlayerGroupActor, bool bRemove, bool bFailed /*= false*/)
{
	bool bAlreadyExists = false;
	if (bRemove)
	{
		bAlreadyExists = PlayerGroupActor->RemoveGroupQuestFailure(Quest->GetQuestId(), bFailed);
	}
	else
	{
		bAlreadyExists = PlayerGroupActor->AddGroupQuestFailure(Quest->GetQuestId());
	}

	Quest->SetIsFailureInbound(!bRemove);

	//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, FString::Printf(TEXT("AIQuestManager::UpdateGroupQuestFailure - bRemove: %s | bAlreadyExists: %s"), bRemove ? TEXT("True") : TEXT("False"), bAlreadyExists ? TEXT("True") : TEXT("False")));
	if (bRemove || (!bRemove && !bAlreadyExists))
	{
		Quest->SetRemainingTime(Quest->QuestData->TimeLimit);
	}
}

void AIQuestManager::OnQuestFail(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bFromTeleport /*= false*/, bool bFromDisband /*= false*/, bool bFromDeath /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnQuestFail"))
	check(TargetCharacter);
	if (!TargetCharacter) return;

	AIPlayerState* IPlayerState = Cast<AIPlayerState>(TargetCharacter->GetPlayerState());
	if (!IPlayerState)
	{
		// UE-Log OnQuestComplete Invalid Player State!
		// This can happen when quests are being completed before the character spawns in
		// It can also occur if quests are being failed due to Combat AI death.
		if (TargetCharacter->GetActiveQuests().Contains(TargetQuest))
		{
			TargetCharacter->GetActiveQuests_Mutable().Remove(TargetQuest);
		}
		
		if (TargetCharacter->GetUncollectedRewardQuests().Contains(TargetQuest))
		{
			TargetCharacter->GetUncollectedRewardQuests_Mutable().Remove(TargetQuest);
		}

		return;
	}

	check(TargetQuest);
	if (!TargetQuest) return;

	check(TargetQuest->QuestData);
	if (!TargetQuest->QuestData) return;

	if (!TargetCharacter->HasLeftHatchlingCave())
	{
		UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::OnQuestFail - %s hasn't left hatching cave but has failed %s quest?"), *TargetCharacter->GetCharacterID().ToString(), *TargetQuest->QuestData->DisplayName.ToString());
	}

	if (TargetQuest->QuestData->QuestType == EQuestType::Tutorial)
	{
		// Disabling failing tutorial quests on death
		return;
	}

	bool bGroupFailed = false;

	UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
	check(GI);

	AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
	check(Session);

	if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group)
	{
		if (TargetQuest->QuestData->QuestType == EQuestType::GroupMeet)
		{
			GroupMeetQuestCooldowns.Add(FQuestCooldown(TargetQuest->GetQuestId(), TargetCharacter->GetCharacterID(), (float)GetWorld()->TimeSeconds));
		}
		else if (GroupQuestCleanupDelay > 0.0f)
		{
			UpdateGroupQuestsOnCooldown(TargetCharacter->GetCharacterID(), TargetQuest->GetQuestId());
		}

		if (AIPlayerGroupActor* const IPlayerGroupActor = IPlayerState->GetPlayerGroupActor())
		{
			if (IPlayerState == IPlayerState->GetPlayerGroupActor()->GetGroupLeader())
			{
				UpdateGroupQuestFailure(TargetQuest, IPlayerGroupActor, true);
				IPlayerGroupActor->GroupQuestResult(TargetQuest, TargetCharacter, false, bFromDisband);
			}
		}
		bGroupFailed = true;
	}

	if (AIGameSession::UseWebHooks(WEBHOOK_PlayerQuestFailed))
	{
		TMap<FString, TSharedPtr<FJsonValue>> WebHookProperties
		{
			{ TEXT("Quest"), MakeShareable(new FJsonValueString(TargetQuest->QuestData->DisplayName.ToString()))},
			{ TEXT("PlayerName"), MakeShareable(new FJsonValueString(IPlayerState->GetPlayerName()))},
			{ TEXT("PlayerAlderonId"), MakeShareable(new FJsonValueString(IPlayerState->GetAlderonID().ToDisplayString()))},
			{ TEXT("GroupQuest"), MakeShareable(new FJsonValueString(FString(bGroupFailed ? TEXT("True") : TEXT("False"))))},
		};
		
		AIGameSession::TriggerWebHookFromContext(this, WEBHOOK_PlayerQuestFailed, WebHookProperties);
	}

	// Remove Quest from Active Quests
	if (TargetCharacter->GetActiveQuests().Contains(TargetQuest))
	{
		TargetCharacter->GetActiveQuests_Mutable().Remove(TargetQuest);
	}

	// Remove Quest from Uncollected Reward Quests
	if (TargetCharacter->GetUncollectedRewardQuests().Contains(TargetQuest))
	{
		TargetCharacter->GetUncollectedRewardQuests_Mutable().Remove(TargetQuest);
	}

	// Let player controller know the quest failed
	AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
	if (IPlayerController)
	{
		IPlayerController->OnQuestFail(TargetQuest, TargetQuest->GetUniqueID(), TargetQuest->GetQuestId());
	}

	// Add Failure Penalty

	int32 FailurePoints = TargetQuest->QuestData->FailurePoints;

	FailurePoints *= Session->QuestMarksMultiplier;

	TargetCharacter->AddMarks(FailurePoints);

	RemoveRewardedQuestContributionEntry(TargetCharacter, TargetQuest, true);
	EQuestShareType QuestShareType = TargetQuest->QuestData->QuestShareType;

	if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Personal)
	{
		TargetCharacter->UpdateFailedQuests(TargetQuest->GetQuestId(), TargetQuest->QuestData->QuestType, MaxRecentFailedQuests);
	}

	if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group)
	{
		if (const AIPlayerGroupActor* const IPlayerGroupActor = IPlayerState->GetPlayerGroupActor())
		{
			if (IPlayerState == IPlayerState->GetPlayerGroupActor()->GetGroupLeader())
			{
				TargetQuest->ConditionalBeginDestroy();
				TargetQuest = nullptr;
			}
		}
	}
	else
	{
		TargetQuest->ConditionalBeginDestroy();
		TargetQuest = nullptr;
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		TargetCharacter->OnRep_ActiveQuests();
		TargetCharacter->OnRep_UncollectedRewardQuests();
	}
#endif

	// Re-save quests
	SaveQuest(TargetCharacter);

	if (!bFromDeath)
	{
		if (TargetCharacter->HasLeftHatchlingCave())
		{
			// Auto Assign New Quests
			if (QuestShareType == EQuestShareType::LocalWorld)
			{
				if (!bFromTeleport) AssignRandomLocalQuest(TargetCharacter);
			}
			else if (QuestShareType == EQuestShareType::Personal)
			{
				AssignRandomPersonalQuests(TargetCharacter);
				//AssignRandomQuest(TargetCharacter, false);
			}
		}

		// Save Character / Marks, if not from death. Character is saved on death anyway.
		AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
		if (IPlayerController && IGameMode)
		{
			IGameMode->SaveAll(IPlayerController, ESavePriority::Medium);
		}
	}
}

void AIQuestManager::OnQuestProgress(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bNotification /*= true*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnQuestProgress"))

	if (!TargetCharacter) return;

	AIPlayerState* IPlayerState = Cast<AIPlayerState>(TargetCharacter->GetPlayerState());
	if (!IPlayerState)
	{
		// UE-Log OnQuestComplete Invalid Player State!
		return;
	}

	if (!TargetCharacter->CanHaveQuests())
	{
		return;
	}

	check(TargetQuest);
	check(TargetQuest->QuestData);

	UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
	check(GI);

	AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
	check(Session);

	AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());

	int32 RewardPoints = TargetQuest->QuestData->ContributionRewardPoints;
	const float RewardGrowth = TargetQuest->QuestData->ContributionRewardGrowth;

	// Quest will reward players based on their contribution to the task. (Example: Quest rewards 100 points but they only contributed to 20% of the total so they will only receive 20 points)
	if (TargetQuest->QuestData->bRewardBasedOnContribution)
	{
		if (RewardPoints > 0)
		{
			UCharacterDataAsset* CharacterDataAsset = UIGameInstance::LoadCharacterData(TargetCharacter->CharacterDataAssetId);
			check(CharacterDataAsset);

			RewardPoints *= Session->QuestMarksMultiplier;

			RewardPoints *= CharacterDataAsset->QuestRewaredMultiplier;

			// Add Reward Marks
			TargetCharacter->AddMarks(RewardPoints);
		}

		if (RewardGrowth > 0)
		{
			if (Session->QuestGrowthMultiplier > 0.f)
			{
				// Growth is applied at the same rate but different durations, depending on growth amount
				// Set to grow at a rate of up to 0.1 growth over 1 minute
				const float GrowthReward = TargetCharacter->GetGrowthPercent() <= Session->HatchlingCaveExitGrowth && !TargetCharacter->HasLeftHatchlingCave() ? BabyGrowthRewardRatePerMinute : GrowthRewardRatePerMinute;
				UPOTAbilitySystemGlobals::RewardGrowthConstantRate(TargetCharacter, RewardGrowth * Session->QuestGrowthMultiplier, GrowthReward / 60.f);
			}
		}

		// Removed: As we are saving too often
		// Save Character / Marks
		//AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
		//if (IPlayerController && IGameMode)
		//{
		//	IGameMode->SaveAll(IPlayerController);
		//}
	}

	if (bNotification)
	{
		if (IPlayerController)
		{
			if (TargetQuest)
			{
				TargetQuest->Update(TargetCharacter, TargetQuest);

				// Need to send task progress because it might not have been replicated yet on client
				TArray<FQuestTaskProgress> QuestTaskProgresses;
				for (UIQuestBaseTask* QuestTask : TargetQuest->GetQuestTasks())
				{
					if ((TargetQuest->QuestData->bInOrderQuestTasks || QuestTask->GetProgressTotal() == 1) && QuestTask->LastNotifiedProgressCount == QuestTask->GetProgressCount())
					{
						continue;
					}

					FQuestTaskProgress Progress;
					Progress.QuestTask = QuestTask;
					Progress.OldProgress = QuestTask->LastNotifiedProgressCount;
					Progress.NewProgress = QuestTask->GetProgressCount();
					QuestTaskProgresses.Add(Progress);
				}

				IPlayerController->OnQuestProgress(TargetQuest, TargetQuest->GetUniqueID(), TargetQuest->GetQuestId(), RewardPoints, QuestTaskProgresses);

				for (UIQuestBaseTask* QuestTask : TargetQuest->GetQuestTasks())
				{
					QuestTask->LastNotifiedProgressCount = QuestTask->GetProgressCount();
				}
			}
		}
	}
}

void AIQuestManager::AwardGrowth(AIBaseCharacter* TargetCharacter, float Growth)
{
	UIGameInstance* IGameInstance = Cast<UIGameInstance>(GetGameInstance());
	check(IGameInstance);
	if (!IGameInstance) return;

	AIGameSession* Session = Cast<AIGameSession>(IGameInstance->GetGameSession());
	check(Session);
	if (!Session) return;

	if (Session->QuestGrowthMultiplier > 0.f)
	{
		const float GrowthReward = TargetCharacter->GetGrowthPercent() <= Session->HatchlingCaveExitGrowth && !TargetCharacter->HasLeftHatchlingCave() ? BabyGrowthRewardRatePerMinute : GrowthRewardRatePerMinute;

		// Growth is applied at the same rate but different durations, depending on growth amount
		// Set to grow at a rate of up to 0.1 growth over 1 minute
		UPOTAbilitySystemGlobals::RewardGrowthConstantRate(TargetCharacter, Growth * Session->QuestGrowthMultiplier, GrowthReward / 60);
	}
}

void AIQuestManager::CollectQuestReward(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest)
{
	if (!TargetCharacter->GetUncollectedRewardQuests().Contains(TargetQuest))
	{
		return;
	}

	if (!TargetQuest->GetUncollectedReward().bIsValid)
	{
		TargetCharacter->GetUncollectedRewardQuests_Mutable().Remove(TargetQuest);
		return;
	}

	TargetCharacter->AddMarks(TargetQuest->GetUncollectedReward().Points);

	if (TargetQuest->GetUncollectedReward().Growth > 0.f)
	{
		AwardGrowth(TargetCharacter, TargetQuest->GetUncollectedReward().Growth);
	}

	for (FQuestHomecaveDecorationReward HomecaveDecorationReward : TargetQuest->GetUncollectedReward().HomeCaveDecorationReward)
	{
		// The Quest Reward now checks before it gets auto claimed or added to the UI.  
		// This is to handle Rewards that were save before this was implemented
		const float RandomChance = FMath::RandRange(0.f, 100.f);
		if (HomecaveDecorationReward.bHasCheckBeenDone || RandomChance <= HomecaveDecorationReward.Chance)
		{
			TargetCharacter->AwardHomeCaveDecoration(HomecaveDecorationReward);
		}
	}

	TargetCharacter->GetUncollectedRewardQuests_Mutable().Remove(TargetQuest);

	FQuestReward Uncollected = TargetQuest->GetUncollectedReward();
	Uncollected.bIsValid = false;
	TargetQuest->SetUncollectedReward(Uncollected);

	TargetQuest->ConditionalBeginDestroy();
	TargetQuest = nullptr;

	// Reset Quest Save
	SaveQuest(TargetCharacter);

	// Save Character / Marks
	AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
	AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
	if (IPlayerController && IGameMode)
	{
		IGameMode->SaveAll(IPlayerController, ESavePriority::Medium);
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		TargetCharacter->OnRep_UncollectedRewardQuests();
	}
#endif
}

void AIQuestManager::OnQuestCompleted(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnQuestCompleted"))
	check(TargetCharacter);
	if (!TargetCharacter) return;

	AIPlayerState* IPlayerState = Cast<AIPlayerState>(TargetCharacter->GetPlayerState());
	if (!IPlayerState)
	{
		// UE-Log OnQuestComplete Invalid Player State!
		// This can happen when quests are being completed before the character spawns in
		if (TargetCharacter->GetActiveQuests().Contains(TargetQuest))
		{
			TargetCharacter->GetActiveQuests_Mutable().Remove(TargetQuest);
		}

		TargetQuest->ConditionalBeginDestroy();
		TargetQuest = nullptr;
		return;
	}

	check(TargetQuest);
	if (!TargetQuest) return;

	check(TargetQuest->QuestData);
	if (!TargetQuest->QuestData) return;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	IGameInstance->AddReplayEvent(EReplayHighlight::QuestCompleted);

	// Updated Completed Location and Personal Quests
	if ((TargetQuest->QuestData->QuestShareType == EQuestShareType::LocalWorld || TargetQuest->QuestData->QuestShareType == EQuestShareType::Group) && LocationCompletedCleanupDelay > 0.0f)
	{
		AddLocalWorldQuestCooldown(FQuestCooldown(TargetQuest->GetQuestId(), TargetCharacter->GetCharacterID(), FDateTime::UtcNow().ToUnixTimestamp()));
		UpdateCompletedLocationQuests(TargetQuest, TargetCharacter);
	}
	else if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Personal)
	{
		TargetCharacter->UpdateCompletedQuests(TargetQuest->GetQuestId(), TargetQuest->QuestData->QuestType, MaxRecentCompletedQuests);

		if (TargetQuest->QuestData->QuestType == EQuestType::Exploration)
		{
			TargetCharacter->LastCompletedExplorationQuestTime = FDateTime::UtcNow();
		}
	}

	if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group)
	{
		if (TargetQuest->QuestData->QuestType == EQuestType::GroupMeet)
		{
			GroupMeetQuestCooldowns.Add(FQuestCooldown(TargetQuest->GetQuestId(), TargetCharacter->GetCharacterID(), (float)GetWorld()->TimeSeconds));
		}
		else if (GroupQuestCleanupDelay > 0.0f)
		{
			UpdateGroupQuestsOnCooldown(TargetCharacter->GetCharacterID(), TargetQuest->GetQuestId());
		}
	}

	if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group)
	{
		if (IPlayerState->GetPlayerGroupActor() && IPlayerState == IPlayerState->GetPlayerGroupActor()->GetGroupLeader())
		{
			// This player is the leader of the group, let all other members call OnQuestCompleted() first
			// so that they can duplicate the quest for reward collection
			UpdateGroupQuestFailure(TargetQuest, IPlayerState->GetPlayerGroupActor(), true);
			IPlayerState->GetPlayerGroupActor()->GroupQuestResult(TargetQuest, TargetCharacter, true);
		}
	}

	UpdateLocalQuestFailure(TargetQuest, TargetCharacter, true);

	if (TargetQuest->QuestData->QuestType == EQuestType::TrophyDelivery)
	{
		GetTrophyQuestsOnCooldown_Mutable().Add(FQuestCooldown(TargetQuest->GetQuestId(), TargetCharacter->GetCharacterID(), (float)GetWorld()->TimeSeconds));
	}

	// Cache Reward Points to reward the player after the quest has been destroyed
	// to avoid duplication
	FQuestReward QuestReward;
	QuestReward.bIsValid = true;
	bool bTryClaimUniqueRewards = true;

	if (TargetQuest->QuestData->bRewardBasedOnContribution)
	{
		const float RewardContributionPercentage = GetContributionPercentage(TargetCharacter, TargetQuest);
		UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::OnQuestCompleted - RewardContributionPercentage = %f"), RewardContributionPercentage);
		QuestReward.Points = TargetQuest->QuestData->RewardPoints * RewardContributionPercentage;
		QuestReward.Growth = TargetQuest->QuestData->RewardGrowth * RewardContributionPercentage;
		RemoveRewardedQuestContributionEntry(TargetCharacter, TargetQuest);

		bTryClaimUniqueRewards = RewardContributionPercentage > 0;
	}
	else
	{
		QuestReward.Points = TargetQuest->QuestData->RewardPoints;
		QuestReward.Growth = TargetQuest->QuestData->RewardGrowth;
	}

	// See if we have successfully claimed the rewards before adding them to the QuestReward
	// If we aren't successful we can auto claim the reward otherwise the reward will be saved to the UI
	if (bTryClaimUniqueRewards)
	{
		TArray<FQuestHomecaveDecorationReward> HomeCaveRewards;
		
		HomeCaveRewards.Append(TargetQuest->QuestData->HomeCaveDecorationReward);
		HomeCaveRewards.Append(TargetQuest->HomecaveDecorationRewards);
		
		for (FQuestHomecaveDecorationReward& Reward : HomeCaveRewards)
		{
			if (FMath::RandRange(0, 100) <= Reward.Chance)
			{
				Reward.bHasCheckBeenDone = true;
				QuestReward.HomeCaveDecorationReward.Add(Reward);
			}
		}
	}

	UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
	check(GI);

	AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
	check(Session);

	QuestReward.Points *= Session->QuestMarksMultiplier;

	UCharacterDataAsset* CharacterDataAsset = UIGameInstance::LoadCharacterData(TargetCharacter->CharacterDataAssetId);
	if (CharacterDataAsset)
	{
		QuestReward.Points *= CharacterDataAsset->QuestRewaredMultiplier;
	} else {
		UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::OnQuestCompleted: Invalid CharacterDataAsset %s is nullptr"), *TargetCharacter->CharacterDataAssetId.ToString());
	}

	if (AIGameSession::UseWebHooks(WEBHOOK_PlayerQuestComplete))
	{
		bool bGroupComplete = false;
		if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group)
		{
			bGroupComplete = true;
		}
		
		TMap<FString, TSharedPtr<FJsonValue>> WebHookProperties
		{
			{ TEXT("Quest"), MakeShareable(new FJsonValueString(TargetQuest->QuestData->DisplayName.ToString()))},
			{ TEXT("PlayerName"), MakeShareable(new FJsonValueString(IPlayerState->GetPlayerName()))},
			{ TEXT("PlayerAlderonId"), MakeShareable(new FJsonValueString(IPlayerState->GetAlderonID().ToDisplayString())) },
			{ TEXT("GroupQuest"), MakeShareable(new FJsonValueString(FString(bGroupComplete ? TEXT("True") : TEXT("False")))) },
		};
		
		AIGameSession::TriggerWebHookFromContext(this, WEBHOOK_PlayerQuestComplete, WebHookProperties);
	}

	AIPlayerController* IPlayerController = Cast<AIPlayerController>(TargetCharacter->GetController());
	if (IPlayerController)
	{
		// Need to send task progress because it might not have been replicated yet on client
		TArray<FQuestTaskProgress> QuestTaskProgresses;
		for (UIQuestBaseTask* QuestTask : TargetQuest->GetQuestTasks())
		{
			if ((TargetQuest->QuestData->bInOrderQuestTasks || QuestTask->GetProgressTotal() == 1) && QuestTask->LastNotifiedProgressCount == QuestTask->GetProgressCount())
			{
				continue;
			}

			FQuestTaskProgress Progress;
			Progress.QuestTask = QuestTask;
			Progress.OldProgress = QuestTask->LastNotifiedProgressCount;
			Progress.NewProgress = QuestTask->GetProgressCount();
			QuestTaskProgresses.Add(Progress);
		}

		IPlayerController->OnQuestCompleted(TargetQuest, TargetQuest->GetUniqueID(), TargetQuest->GetQuestId(), QuestReward.Points, QuestTaskProgresses);

		for (UIQuestBaseTask* QuestTask : TargetQuest->GetQuestTasks())
		{
			QuestTask->LastNotifiedProgressCount = QuestTask->GetProgressCount();
		}
	}

	if (TargetCharacter->GetActiveQuests().Contains(TargetQuest))
	{
		TargetCharacter->GetActiveQuests_Mutable().Remove(TargetQuest);
	}

	if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group)
	{
		// We need to give all characters a fresh copy of the quest so they can collect their rewards later.
		// If any characters use the same quest, the quest may be destroyed via ConditionalBeginDestroy() once the group is disbanded. -Poncho
		UIQuest* NewQuest = DuplicateObject(TargetQuest, TargetCharacter);
		TArray<UIQuestBaseTask*>& NewQuestTasks = NewQuest->GetQuestTasks_Mutable();
		NewQuest->QuestData = TargetQuest->QuestData;
		NewQuestTasks.Empty(TargetQuest->GetQuestTasks().Num());
		for (const UIQuestBaseTask* const QuestTask : TargetQuest->GetQuestTasks())
		{
			NewQuestTasks.Add(DuplicateObject(QuestTask, TargetCharacter));
		}

		TargetQuest = NewQuest;		
	}

	// Claim growth automatically for World Tutorial Quests
	if (TargetQuest->QuestData->QuestType == EQuestType::WorldTutorial && QuestReward.Growth > 0.f)
	{
		AwardGrowth(TargetCharacter, QuestReward.Growth);
		QuestReward.Growth = 0.f;
	}

	TargetQuest->SetUncollectedReward(QuestReward);
	TargetCharacter->GetUncollectedRewardQuests_Mutable().Add(TargetQuest);

	// Auto Assign New Quests
	bool bAssignNewLocalQuest = false;
	bool bAssignPersonalQuest = TargetQuest->QuestData->QuestType != EQuestType::Tutorial && TargetQuest->QuestData->QuestShareType == EQuestShareType::Personal;
	EQuestShareType QuestTypeToLookFor = EQuestShareType::Unknown;

	if (!TargetCharacter->HasLeftHatchlingCave())
	{
		if (!Cast<AIHatchlingCave>(TargetCharacter->GetCurrentInstance()))
		{
			UE_LOG(TitansQuests, Error, TEXT("AIQuestManager::OnQuestCompleted - %s hasn't left hatching cave yet their current hatcling cave instance isn't valid?"), *TargetCharacter->GetCharacterID().ToString());
		}

		TargetCharacter->HatchlingTutorialIndex++;
		AssignNextHatchlingTutorialQuest(TargetCharacter);
	}
	else if (TargetQuest->QuestData->QuestShareType == EQuestShareType::LocalWorld)
	{
		if (!TargetCharacter->GetActiveQuests().IsEmpty())
		{
			bool bHasQuestType = false;

			for (UIQuest* Quest : TargetCharacter->GetActiveQuests())
			{
				if (!Quest || !Quest->QuestData) continue;
				if (Quest->QuestData->QuestShareType != EQuestShareType::LocalWorld) continue;

				bHasQuestType = true;
				break;
			}

			if (!bHasQuestType)
			{
				bAssignNewLocalQuest = true;
			}
		}
		else
		{
			bAssignNewLocalQuest = true;
		}
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		TargetCharacter->OnRep_ActiveQuests();
		TargetCharacter->OnRep_UncollectedRewardQuests();
	}
#endif

	// Determine if rewards should be automatically claimed
	bool ClaimReward = false;
	if (QuestReward.ContainsAnyRewards())
	{
		switch (TargetQuest->QuestData->RewardClaimType)
		{
			case EQuestRewardClaimType::Default:
			{
				// Auto claim tutorial quests by default
				// Auto Claim if the only rewards are Marks or Growth, if we have collectables don't auto claim.
				ClaimReward = (TargetQuest->QuestData->QuestType == EQuestType::Tutorial) || (!QuestReward.ContainsCollectableRewards());
				break;
			}
			case EQuestRewardClaimType::Auto:
			{
				ClaimReward = true;
				break;
			}
			case EQuestRewardClaimType::Manual:
			{
				ClaimReward = false;
				break;
			}
		}
	} 
	else
	{
		// No rewards, auto claim so it doesn't show in the quest menu
		ClaimReward = true;
	}

	if (ClaimReward)
	{
		CollectQuestReward(TargetCharacter, TargetQuest);
	}
	else
	{
		// Reset Quest Save
		SaveQuest(TargetCharacter);

		// Save Character / Marks
		AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
		if (IPlayerController && IGameMode)
		{
			IGameMode->SaveAll(IPlayerController, ESavePriority::Medium);
		}
	}

	if (bAssignNewLocalQuest)
	{
		AssignRandomLocalQuest(TargetCharacter);
	}
	else if (bAssignPersonalQuest)
	{
		AssignRandomPersonalQuests(TargetCharacter);
	}
}

void AIQuestManager::OnQuestUpdated(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bProgressGained)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnQuestUpdated"))

	TargetQuest->CheckCompletion();
	
	if (bProgressGained)
	{
		OnQuestProgress(TargetCharacter, TargetQuest, !TargetQuest->IsCompleted());
	}

	if (TargetQuest->IsCompleted())
	{
		if (TargetQuest->QuestData->QuestShareType == EQuestShareType::Group && TargetQuest->GetPlayerGroupActor())
		{
			AIBaseCharacter* GroupLeaderChar = TargetQuest->GetPlayerGroupActor()->GetGroupLeader()->GetPawn<AIBaseCharacter>();

			OnQuestCompleted(GroupLeaderChar, TargetQuest);
		}
		else
		{
			OnQuestCompleted(TargetCharacter, TargetQuest);
		}
	}
}

void AIQuestManager::OnCharacterKilled(AIBaseCharacter* Killer, AIBaseCharacter* Victim)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnCharacterKilled"))

	// Killer / Victim is Valid
	if (!IsValid(Killer))
	{
		// Crashed here in prod
		return;
	}

	if (Victim && Killer && Victim->GetCombatLogAlderonId() == Killer->GetCombatLogAlderonId())
	{
		// Don't award kill quests for killing yourself or combat log ai.
		return;
	}

	FPrimaryAssetId VictimCharacterDataAssetId = Victim->CharacterDataAssetId;
	if (!VictimCharacterDataAssetId.IsValid()) return;

	bool bProgressGained = false;

	UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
	check(GI);
	if (!GI) return;

	AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
	check(Session);
	if (!Session) return;

	// Killer has a active quest let's update it's tasks
	TArray<UIQuest*> QuestsToCheck = Killer->GetActiveQuests();
	for (UIQuest* KillersQuest : QuestsToCheck)
	{
		if (!KillersQuest) continue;

		for (UIQuestBaseTask* QuestTask : KillersQuest->GetQuestTasks())
		{
			// Process Only Kill Tasks
			UIQuestKillTask* KillTask = Cast<UIQuestKillTask>(QuestTask);
			if (!KillTask) continue;

			if ((KillTask->ShouldIgnoreCharacterAssetIdsAndUseDietType() && Victim->DietRequirements == KillTask->GetDietaryRequirements()) || (KillTask->GetCharacterAssetIds().Contains(VictimCharacterDataAssetId)))
			{
				KillTask->Increment();
				bProgressGained = true;

				if (KillersQuest->QuestData->bRewardBasedOnContribution)
				{
					OnContributeRestore(QuestTask->Tag, 1.0f, Killer, KillersQuest);
				}

				// Give Reward Per Kill
				float KillerGrowth = Killer->GetGrowthPercent();
				float VictimGrowth = Victim->GetGrowthPercent();

				float RewardMultiplier = KillTask->IsRewardBasedUponSizeDifference() ? VictimGrowth / KillerGrowth : 1.0f;

				int32 RewardPoints = KillTask->RewardPoints;
				float RewardGrowth = KillTask->GetRewardGrowth();

				if (RewardPoints > 0)
				{
					// Apply Reward Multiplier
					RewardPoints *= RewardMultiplier;

					UCharacterDataAsset* CharacterDataAsset = UIGameInstance::LoadCharacterData(Killer->CharacterDataAssetId);
					check(CharacterDataAsset);

					// Apply Server Quest Marks Reward Multiplier
					RewardPoints *= Session->QuestMarksMultiplier;

					// Apply Character Reward Multiplier
					RewardPoints *= CharacterDataAsset->QuestRewaredMultiplier;

					// Add Reward Marks
					Killer->AddMarks(RewardPoints);
				}

				if (RewardGrowth > 0)
				{
					// Apply Reward Multiplier
					RewardGrowth *= RewardMultiplier;

					if (Session->QuestGrowthMultiplier > 0.f)
					{
						// Growth is applied at the same rate but different durations, depending on growth amount
						// Set to grow at a rate of up to 0.1 growth over 1 minute
						const float GrowthReward = Killer->GetGrowthPercent() <= Session->HatchlingCaveExitGrowth && !Killer->HasLeftHatchlingCave() ? BabyGrowthRewardRatePerMinute : GrowthRewardRatePerMinute;
						UPOTAbilitySystemGlobals::RewardGrowthConstantRate(Killer, RewardGrowth * Session->QuestGrowthMultiplier, GrowthReward / 60.f);
					}
				}

				break;
			}
		}

		OnQuestUpdated(Killer, KillersQuest, bProgressGained);

		if (bProgressGained) break;
	}
}

void AIQuestManager::OnFishKilled(AIBaseCharacter* Killer, AIFish* Fish)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnFishKilled"))

	// Killer / Victim is Valid
	if (!IsValid(Killer) || !IsValid(Fish))
	{
		// Crashed here in prod
		return;
	}

	check(Killer);
	check(Fish);

	bool bProgressGained = false;

	// Since quests can be completed and removed from Killer->ActiveQuests, we need to create a temporary array for these to prevent a crash -Poncho
	TArray<UIQuest*> TempActiveQuests = Killer->GetActiveQuests();

	// Killer has a active quest let's update it's tasks
	for (UIQuest* KillersQuest : TempActiveQuests)
	{
		if (!KillersQuest) 
		{
			continue;
		}

		for (UIQuestBaseTask* QuestTask : KillersQuest->GetQuestTasks())
		{
			// Process Only Kill Tasks
			UIQuestFishTask* KillTask = Cast<UIQuestFishTask>(QuestTask);
			if (!KillTask) 
			{
				continue;
			}

			if (KillTask->GetFishSize() == Fish->CarryData.CarriableSize)
			{
				KillTask->Increment();
				bProgressGained = true;

				if (KillersQuest->QuestData->bRewardBasedOnContribution)
				{
					OnContributeRestore(QuestTask->Tag, 1.0f, Killer, KillersQuest);
				}
				break;
			}
		}

		OnQuestUpdated(Killer, KillersQuest, bProgressGained);
	}
}

void AIQuestManager::OnCritterKilled(AIBaseCharacter* Killer, AICritterPawn* Critter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnCritterKilled"))

	// Killer / Victim is Valid
	if (!IsValid(Killer) || !IsValid(Critter))
	{
		// Crashed here in prod
		return;
	}

	check(Killer);
	check(Critter);

	FPrimaryAssetId CritterProfile;
	if (AAlderonCritterController* AlderonCritterController = Cast<AAlderonCritterController>(Critter->GetController()))
	{
		if (AlderonCritterController->ActiveCritterProfile)
		{
			CritterProfile = AlderonCritterController->ActiveCritterProfile->GetPrimaryAssetId();
		}
	}

	bool bProgressGained = false;

	// Since quests can be completed and removed from Killer->ActiveQuests, we need to create a temporary array for these to prevent a crash -Poncho
	TArray<UIQuest*> TempActiveQuests = Killer->GetActiveQuests();

	// Killer has a active quest let's update it's tasks
	for (UIQuest* KillersQuest : TempActiveQuests)
	{
		if (!IsValid(KillersQuest)) continue;

		bool bIsKillQuest = false;

		for (UIQuestBaseTask* QuestTask : KillersQuest->GetQuestTasks())
		{
			// Process Only Kill Tasks
			UIQuestKillTask* KillTask = Cast<UIQuestKillTask>(QuestTask);
			if (!KillTask) 
			{
				continue;
			}

			if (!KillTask->GetCharacterAssetIds().Contains(CritterProfile))
			{
				continue;
			}

			bIsKillQuest = true;

			KillTask->Increment();
			bProgressGained = true;

			if (KillersQuest->QuestData->bRewardBasedOnContribution)
			{
				OnContributeRestore(QuestTask->Tag, 1.0f, Killer, KillersQuest);
			}
			break;
		}

		// Only show quest update notice for kill quest
		if (bIsKillQuest)
		{
			OnQuestUpdated(Killer, KillersQuest, bProgressGained);
		}
	}
}

void AIQuestManager::OnLocationDiscovered(FName LocationTag, const FText& LocationDisplayName, AIBaseCharacter* Character, bool bEntered, bool bNotification /*= true*/, bool bFromTeleport /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnLocationDiscovered"))

	check(Character);
	if (!Character) return;
	if (!Character->HasLeftHatchlingCave() && UIGameplayStatics::AreHatchlingCavesEnabled(this)) return;

	AIPlayerController* IPlayerController = Cast<AIPlayerController>(Character->GetController());
	if (!IPlayerController) return;
	AIPlayerState* IPlayerState = IPlayerController->GetPlayerState<AIPlayerState>();
	if (!IPlayerState) return;

	TArray<UIQuest*> QuestsToCheck = Character->GetActiveQuests();

	// Get Local World Quests assigned to the player
	for (UIQuest* ActiveQuest : QuestsToCheck)
	{
		if (!ActiveQuest || !ActiveQuest->QuestData || ActiveQuest->QuestData->QuestShareType != EQuestShareType::LocalWorld || ActiveQuest->QuestData->LocationTag != LocationTag) continue;

		if (bEntered)
		{
			UpdateLocalQuestFailure(ActiveQuest, Character, true);
		}
		else
		{
			UpdateLocalQuestFailure(ActiveQuest, Character, false);
		}
	}

	// Try to progress Exploration Quest Tasks
	for (UIQuest* ActiveQuest : QuestsToCheck)
	{
		if (ActiveQuest)
		{
			bool bProgressGained = false;

			UQuestData* QuestData = ActiveQuest->QuestData;
			if (!QuestData && QuestData->QuestShareType != EQuestShareType::Personal) continue;

			TArray<UIQuestBaseTask*> QuestTasks;

			if (QuestData->bInOrderQuestTasks) {
				QuestTasks.Add(ActiveQuest->GetActiveTask());
			} else {
				QuestTasks = ActiveQuest->GetQuestTasks();
			}

			for (UIQuestBaseTask* QuestTask : QuestTasks)
			{
				// Process Only Kill Tasks
				UIQuestExploreTask* ExploreTask = Cast<UIQuestExploreTask>(QuestTask);
				if (!ExploreTask) continue;

				if (ExploreTask->Tag == LocationTag)
				{
					ExploreTask->SetIsCompleted(true);
					bProgressGained = true;
					break;
				}
			}

			if (bProgressGained)
			{
				OnQuestUpdated(Character, ActiveQuest, bProgressGained);
				break;
			}
		}
	}

	// If we're in a group we should attempt to assign a new group quest upon entering
	// Otherwise start failing if we're leaving our group quests POI
	if (AIPlayerGroupActor* const PlayerGroupActor = IPlayerState->GetPlayerGroupActor())
	{
		if (PlayerGroupActor && PlayerGroupActor->GetGroupLeader() == IPlayerState)
		{
			const TArray<UIQuest*>& ActiveGroupQuests = PlayerGroupActor->GetGroupQuests();

			bool bLocationIsGroupQuest = false;
			for (UIQuest* ActiveGroupQuest : ActiveGroupQuests)
			{
				if (!ActiveGroupQuest || !ActiveGroupQuest->QuestData || ActiveGroupQuest->QuestData->QuestType == EQuestType::GroupMeet || ActiveGroupQuest->QuestData->LocationTag != LocationTag) continue;
				
				if (bEntered)
				{
					UpdateGroupQuestFailure(ActiveGroupQuest, PlayerGroupActor, true);
				}
				else
				{
					UpdateGroupQuestFailure(ActiveGroupQuest, PlayerGroupActor, false);
				}
			}

			if (bEntered)
			{
				if (ActiveGroupQuests.Num() < PlayerGroupActor->GetMaxGroupQuests())
				{
					PlayerGroupActor->AssignGroupQuests();
				}
			}
		}
	}

	if (bNotification)
	{
		const bool bSkipNotif = IPlayerState->GetInstanceType() == EInstanceType::IT_BabyCave;
		IPlayerController->OnLocationChanged(LocationTag, LocationDisplayName, bEntered, bSkipNotif);
	}

	// Don't give a quest if we are leaving a POI via a teleport. Overlap on the new POI will re-call this.
	if (!bEntered && bFromTeleport)
	{
		return;
	}

	UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::OnLocationDiscovered() - Assigning random quest for location %s with tag %s, Char last Loc %s"), *LocationTag.ToString(), *LocationDisplayName.ToString(), *Character->LastLocationTag.ToString());

	// If we are entering we should try to get local quests in this area
	// Otherwise we should start failing our active local quests
	if (bEntered)
	{
		AssignRandomLocalQuest(Character);
	}
	else
	{
		AssignRandomPersonalQuests(Character);
	}
}

void AIQuestManager::OnItemCollected(FName ItemTag, AIBaseCharacter* Character, int32 Count)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnItemCollected"))

	// Character is Valid
	check(Character);

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	// Collector has an active quest let's update its tasks
	TArray<UIQuest*> QuestsToCheck = Character->GetActiveQuests();
	for (UIQuest* CollectorQuest : QuestsToCheck)
	{
		bool bProgressGained = false;

		if (!CollectorQuest || CollectorQuest->IsCompleted()) continue;

		// For group quests, Character must be in range of the group leader to contribute.
		UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);

		AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
		check(Session);

		if (Session->bMustCollectItemsWithinPOI && CollectorQuest->GetPlayerGroupActor())
		{
			if (!CollectorQuest->GetPlayerGroupActor()->GetGroupLeader()) continue;

			double DistanceToLeader = FVector::Distance(CollectorQuest->GetPlayerGroupActor()->GetGroupLeaderLocation(), Character->GetActorLocation());
			bool bWithinMaxGroupLeaderCommsDistance = DistanceToLeader <= IGameState->GetGameStateFlags().MaxGroupLeaderCommunicationDistance;
			AIBaseCharacter* GroupLeaderPawn = CollectorQuest->GetPlayerGroupActor()->GetGroupLeader()->GetPawn<AIBaseCharacter>();
			if (GroupLeaderPawn && (GroupLeaderPawn != Character) && !bWithinMaxGroupLeaderCommsDistance)
			{
				continue;
			}
		}

		TArray<UIQuestBaseTask*> QuestTasks;
		
		if (CollectorQuest->QuestData->bInOrderQuestTasks) 
		{
			QuestTasks.Add(CollectorQuest->GetActiveTask());
		} 
		else 
		{
			QuestTasks = CollectorQuest->GetQuestTasks();
		}

		for (UIQuestBaseTask* QuestTask : QuestTasks)
		{
			UIQuestGatherTask* GatherTask = Cast<UIQuestGatherTask>(QuestTask);
			if (!GatherTask || GatherTask->IsCompleted()) continue;

			if (GatherTask->Tag == ItemTag)
			{
				for (int32 i = 0; i < Count; i++)
				{
					GatherTask->Increment();

					if (CollectorQuest->QuestData->bRewardBasedOnContribution)
					{
						OnContributeRestore(ItemTag, (float)Count, Character, CollectorQuest);
					}
				}
				bProgressGained = true;
			}
		}

		if (bProgressGained)
		{
			OnQuestUpdated(Character, CollectorQuest, bProgressGained);
		}
	}
}

void AIQuestManager::OnItemDelivered(FName ItemTag, AIBaseCharacter* Character, UIQuest* Quest, int32 Count /*= 1*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnItemDelivered"))

	// Character is Valid
	check(Character);

	// Character has a active quest let's update it's tasks
	TArray<UIQuest*> QuestsToCheck = Character->GetActiveQuests();
	for (UIQuest* DeliveryQuest : QuestsToCheck)
	{
		bool bProgressGained = false;

		if (!DeliveryQuest || DeliveryQuest != Quest) continue;

		for (UIQuestBaseTask* QuestTask : DeliveryQuest->GetQuestTasks())
		{
			// Process Only Delivery Tasks
			UIQuestDeliverTask* DeliveryTask = Cast<UIQuestDeliverTask>(QuestTask);
			if (!DeliveryTask) continue;

			if (DeliveryTask->Tag == ItemTag)
			{
				for (int32 i = 0; i < Count; i++)
				{
					DeliveryTask->Increment();

					if (Quest->QuestData->bRewardBasedOnContribution)
					{
						OnContributeRestore(ItemTag, (float)Count, Character, Quest);
					}
				}
				bProgressGained = true;
				break;
			}
		}

		if (bProgressGained)
		{
			OnQuestUpdated(Character, DeliveryQuest, bProgressGained);
			break;
		}
	}
}

void AIQuestManager::OnContributeRestore(FName ContributionTag, float RestoreValue, AIBaseCharacter* Character, UIQuest* Quest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnContributeRestore"))

	if (!Character || !Quest || !Quest->QuestData) return;

	EQuestLocalType QuestLocalType = Quest->QuestData->QuestLocalType;

	if (QuestLocalType != EQuestLocalType::Water && QuestLocalType != EQuestLocalType::Waystone && !Quest->GetPlayerGroupActor()) return;

	bool bIsCharactersActiveQuest = false;
	for (UIQuestBaseTask* BaseTask : Quest->GetQuestTasks())
	{
		UIQuestBaseTask* QuestContributionTask = Cast<UIQuestBaseTask>(BaseTask);
		if (!QuestContributionTask) continue;

		if (QuestContributionTask->Tag == ContributionTag)
		{
			bIsCharactersActiveQuest = true;
			break;
		}
	}

	if (bIsCharactersActiveQuest)
	{
		bool bFoundExistingRestorationContribution = false;
		TArray<FQuestContribution>& MutableQuestContributes = GetQuestContributions_Mutable();
		
		for (FQuestContribution& QuestContribution : MutableQuestContributes)
		{
			if (QuestContribution.QuestId != Quest->GetQuestId()) continue;

			if (Character && QuestContribution.CharacterID == Character->GetCharacterID())
			{
				bFoundExistingRestorationContribution = true;
				QuestContribution.ContributionAmount += RestoreValue;
				break;
			}
		}

		if (!bFoundExistingRestorationContribution)
		{
			FQuestContribution NewContribution;
			NewContribution.ContributionAmount = RestoreValue;
			NewContribution.QuestId = Quest->GetQuestId();
			NewContribution.CharacterID = Character->GetCharacterID();
			MutableQuestContributes.Add(NewContribution);
			UE_LOG(TitansQuests, Verbose, TEXT("AIQuestManager::OnContributeRestore() - ContributionTag = %s | Quest->QuestId = %s"), *ContributionTag.ToString(), *Quest->GetQuestId().ToString());
		}

		if (QuestLocalType == EQuestLocalType::Waystone)
		{
			if (AIWaystoneManager* WaystoneMgr = AIWorldSettings::GetWorldSettings(this)->WaystoneManager)
			{
				OnQuestProgress(Character, Quest, (WaystoneMgr->GetWaystoneCooldownProgress(ContributionTag) != 1.0f));
			}
		}
		else if (QuestLocalType == EQuestLocalType::Water)
		{
			if (AIWaterManager* WaterMgr = AIWorldSettings::GetWorldSettings(this)->WaterManager)
			{
				OnQuestProgress(Character, Quest, (WaterMgr->GetWaterQuality(ContributionTag) != 1.0f));
			}
		}
		
		SaveContributionData();
	}
}

void AIQuestManager::OnWaterRestore(FName WaterTag, float RestoreValue, AIBaseCharacter* Character, UIQuest* Quest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnWaterRestore"))

	if (!Character || !Quest) return;

	if (AIWaterManager* WaterMgr = AIWorldSettings::GetWorldSettings(this)->WaterManager)
	{
		WaterMgr->UpdateBodyOfWater(WaterTag, RestoreValue, 0.0f, 0.0f);

		OnContributeRestore(WaterTag, RestoreValue, Character, Quest);
	}
}

void AIQuestManager::OnWaterQualityDroppedBelowThreshold(FWaterData WaterData, FWaterLocationData WaterLocationData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnWaterQualityDroppedBelowThreshold"))

	TArray<FPrimaryAssetId> Quests = WaterRestoreQuests;
	if (Quests.IsEmpty() || WaterData.WaterTag == NAME_None) return;

	for (FName WaterTag : ActiveWaterRestorationQuestTags)
	{
		if (WaterTag == WaterData.WaterTag) return;
	}

	ActiveWaterRestorationQuestTags.Add(WaterData.WaterTag);

	FPrimaryAssetId QuestId;
	for (const FPrimaryAssetId& QuestSelected : Quests)
	{
		// Load Quest Data for more complicated quest type picking
		UQuestData* QuestData = LoadQuestData(QuestSelected);
		if (!QuestData || !QuestData->bEnabled)
		{
			// Failed to load Quest Data
			continue;
		}
		check(QuestData);

		if (QuestData->QuestTag == WaterData.WaterTag)
		{
			QuestId = QuestSelected;
			break;
		}
	}

	// TODO: (Erlite) Note: another chunk of code that does nothing, because it modifies a bad copy of an array instead of a ref (or just modifying the original array heh). Commented out, review later.
	// TArray<FQuestContribution> TempQuestContributions = QuestContributions; <-- BAD WOLF
	// // Remove active saved contributions if we're creating a new one
	// if (QuestId.IsValid())
	// {
	// 	for (int32 i = TempQuestContributions.Num(); i-- > 0;)
	// 	{
	// 		if (TempQuestContributions[i].QuestId == QuestId)
	// 		{
	// 			TempQuestContributions.RemoveAt(i);
	// 		}
	// 	}
	// }
	//
	// SetQuestContributions(QuestContributions);

	// Try to assign characters in the zone with quest if they don't already have one.
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	for (APlayerState* PlayerState : IGameState->PlayerArray)
	{
		if (!PlayerState) continue;

		AIPlayerController* OwningPlayerController = Cast<AIPlayerController>(PlayerState->GetOwner());
		if (OwningPlayerController && OwningPlayerController->IsValidLowLevel())
		{
			AIBaseCharacter* OwningCharacter = OwningPlayerController->GetPawn<AIBaseCharacter>();
			if (OwningCharacter && OwningCharacter->IsValidLowLevel() && OwningCharacter->LastLocationEntered &&
				(WaterLocationData.WaterLocationTag == OwningCharacter->LastLocationTag) && OwningCharacter->HasLeftHatchlingCave())
			{
				AssignWaterReplenishQuest(OwningCharacter, WaterData.WaterTag);
			}
		}
	}

	SaveContributionData();
}

void AIQuestManager::OnWaterQualityRestored(FWaterData WaterData, FWaterLocationData WaterLocationData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnWaterQualityRestored"))

	if (!GetWorld()) return;
	TArray<FPrimaryAssetId> Quests = WaterRestoreQuests;
	if (Quests.IsEmpty() == 0 || WaterData.WaterTag == NAME_None) return;

	ActiveWaterRestorationQuestTags.Remove(WaterData.WaterTag);

	FPrimaryAssetId QuestId;
	for (const FPrimaryAssetId& QuestSelected : Quests)
	{
		// Load Quest Data for more complicated quest type picking
		UQuestData* QuestData = LoadQuestData(QuestSelected);
		if (!QuestData || !QuestData->bEnabled)
		{
			// Failed to load Quest Data
			continue;
		}
		check(QuestData);

		if (QuestData->QuestTag == WaterData.WaterTag)
		{
			QuestId = QuestSelected;
			break;
		}
	}

	const float TimeSeconds = GetWorld()->TimeSeconds;
	for (FQuestContribution& QuestContribution : GetQuestContributions_Mutable())
	{
		if (QuestContribution.QuestId == QuestId)
		{
			QuestContribution.Timestamp = TimeSeconds;
		}
	}
}

void AIQuestManager::OnWaystoneRestore(FName WaystoneTag, int32 RestoreValue, AIBaseCharacter* Character, UIQuest* Quest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnWaystoneRestore"))

	if (!Character || !Quest) return;

	if (AIWaystoneManager* WaystoneMgr = AIWorldSettings::GetWorldSettings(this)->WaystoneManager)
	{
		WaystoneMgr->ModifyWaystoneData(WaystoneTag, RestoreValue);

		OnContributeRestore(WaystoneTag, (float)RestoreValue, Character, Quest);
	}

	SaveContributionData();
}

void AIQuestManager::OnWaystoneCooldownActivated(FName WaystoneTag)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnWaystoneCooldownActivated"))

	TArray<FPrimaryAssetId> Quests = WaystoneRestoreQuests;
	if (Quests.IsEmpty() || WaystoneTag == NAME_None || ActiveWaystoneRestoreQuestTags.Contains(WaystoneTag)) return;

	ActiveWaystoneRestoreQuestTags.Add(WaystoneTag);

	FPrimaryAssetId QuestId;
	UQuestData* SavedQuestData = nullptr;
	for (const FPrimaryAssetId& QuestSelected : Quests)
	{
		// Load Quest Data for more complicated quest type picking
		UQuestData* QuestData = LoadQuestData(QuestSelected);
		if (QuestData && QuestData->bEnabled && QuestData->QuestTag == WaystoneTag)
		{
			SavedQuestData = QuestData;
			QuestId = QuestSelected;
			break;
		}

	}

	bool bNeedsDirtying = false;

	// Remove active saved contributions if we're creating a new one
	for (int32 Index = GetQuestContributions().Num() - 1; Index >= 0; --Index)
	{
		if (GetQuestContributions()[Index].QuestId == QuestId)
		{
			QuestContributions.RemoveAt(Index);
			bNeedsDirtying = true;
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, QuestContributions, this);
	}
	
	// Try to assign characters in the zone with quest if they don't already have one.
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	for (APlayerState* PlayerState : IGameState->PlayerArray)
	{
		if (!PlayerState) continue;

		AIPlayerController* OwningPlayerController = Cast<AIPlayerController>(PlayerState->GetOwner());
		if (OwningPlayerController && OwningPlayerController->IsValidLowLevel())
		{
			AIBaseCharacter* OwningCharacter = OwningPlayerController->GetPawn<AIBaseCharacter>();
			if (OwningCharacter && OwningCharacter->IsValidLowLevel() && OwningCharacter->HasLeftHatchlingCave())
			{
				bool bLocalWorldQuestExists = false;
				bool bLocalWaystoneQuestExists = false;
				for (UIQuest* ActiveQuest : OwningCharacter->GetActiveQuests())
				{
					if (IsValid(ActiveQuest) && ActiveQuest->QuestData->QuestShareType == EQuestShareType::LocalWorld)
					{
						bLocalWorldQuestExists = true;
					}
					if (IsValid(ActiveQuest) &&
						ActiveQuest->QuestData->QuestShareType == EQuestShareType::LocalWorld &&
						(ActiveQuest->QuestData->QuestLocalType == EQuestLocalType::Waystone
						&& SavedQuestData && ActiveQuest->QuestData == SavedQuestData))
					{
						bLocalWaystoneQuestExists = true;
					}
				}

				if (!bLocalWaystoneQuestExists || !bLocalWorldQuestExists)
				{
					AssignRandomLocalQuest(OwningCharacter);
				}
			}
		}
	}

	SaveContributionData();
}



void AIQuestManager::OnWaystoneCooldownDeactivated(FName WaystoneTag, bool bFromPlayer /*= false*/, AIBaseCharacter* Character /*= nullptr*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnWaystoneCooldownDeactivated"))

	if (!GetWorld()) return;
	TArray<FPrimaryAssetId> Quests = WaystoneRestoreQuests;
	if (Quests.IsEmpty() == 0 || WaystoneTag == NAME_None) return;

	ActiveWaystoneRestoreQuestTags.Remove(WaystoneTag);

	FPrimaryAssetId QuestId;
	for (const FPrimaryAssetId& QuestSelected : Quests)
	{
		// Load Quest Data for more complicated quest type picking
		UQuestData* QuestData = LoadQuestData(QuestSelected);
		if (!QuestData || !QuestData->bEnabled)
		{
			// Failed to load Quest Data
			continue;
		}
		check(QuestData);

		if (QuestData->QuestTag == WaystoneTag)
		{
			QuestId = QuestSelected;
			break;
		}
	}

	bool bNeedsDirtying = false;

	// Mark their contribution as already rewarded so they don't get rewarded
	if (bFromPlayer && Character)
	{
		for (int32 Index = 0; Index < GetQuestContributions().Num(); ++Index)
		{
			const FQuestContribution& QuestContribution = GetQuestContributions()[Index];
			if (QuestContribution.CharacterID == Character->GetCharacterID() && QuestContribution.QuestId == QuestId)
			{
				QuestContributions[Index].bRewarded = true;
				bNeedsDirtying = true;
			}
		}
	}

	const float TimeSeconds = GetWorld()->TimeSeconds;
	for (int32 Index = 0; Index < GetQuestContributions().Num(); ++Index)
	{
		const FQuestContribution& QuestContribution = GetQuestContributions()[Index];
		if (QuestContribution.QuestId == QuestId)
		{
			QuestContributions[Index].Timestamp = TimeSeconds;
			bNeedsDirtying = true;
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, QuestContributions, this);
	}
	
	SaveContributionData();
}

float AIQuestManager::GetContributionPercentage(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetContributionPercentage"))

	if (!TargetCharacter || !TargetQuest || !TargetQuest->QuestData) return 0.0f;

	EQuestLocalType QuestLocalType = TargetQuest->QuestData->QuestLocalType;

	bool bIsCharactersActiveQuest = false;
	for (UIQuest* Quest : TargetCharacter->GetActiveQuests())
	{
		if (TargetQuest == Quest)
		{
			bIsCharactersActiveQuest = true;
			break;
		}
	}

	float ContributionRatio = 1.0f;

	if (QuestLocalType != EQuestLocalType::Water && QuestLocalType != EQuestLocalType::Waystone && !TargetQuest->GetPlayerGroupActor())
	{
		return ContributionRatio;
	}
	else if (bIsCharactersActiveQuest)
	{
		FPrimaryAssetId QuestId = TargetQuest->GetQuestId();

		float TotalContributionsAmount = 0;
		TOptional<float> TargetCharactersContributionAmount{};

		TArray<FAlderonUID> GroupMemberIds{};

		if (TargetQuest->GetPlayerGroupActor())
		{
			for (AIPlayerState* IPlayerState : TargetQuest->GetPlayerGroupActor()->GetGroupMembers())
			{
				if (!IPlayerState) continue;

				GroupMemberIds.AddUnique(IPlayerState->GetLastSelectedCharacter());
			}
		}

		if (QuestId.IsValid())
		{
			for (const FQuestContribution& QuestContribution : GetQuestContributions())
			{
				if (QuestContribution.QuestId == QuestId)
				{
					if ((TargetQuest->GetPlayerGroupActor() && GroupMemberIds.Contains(QuestContribution.CharacterID)) || !TargetQuest->GetPlayerGroupActor())
					{
						if (QuestContribution.CharacterID == TargetCharacter->GetCharacterID())
						{
							if (QuestContribution.bRewarded)
							{
								continue;
							}

							TargetCharactersContributionAmount = QuestContribution.ContributionAmount;
						}

						TotalContributionsAmount += QuestContribution.ContributionAmount;
					}
				}
			}
		}

		if (!TargetCharactersContributionAmount.IsSet())
		{
			return 0.0f;
		}

		ContributionRatio = FMath::Clamp(TargetCharactersContributionAmount.GetValue() / TotalContributionsAmount, 0.0f, 1.0f);

		return ContributionRatio;
	}

	return 0.0f;
}

void AIQuestManager::RemoveRewardedQuestContributionEntry(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bForceRemove /*= false*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::RemoveRewardedQuestContributionEntry"))

	if (!TargetQuest || !TargetQuest->QuestData)
	{
		return;
	}
	
	const FPrimaryAssetId& QuestId = TargetQuest->GetQuestId();
	if (!QuestId.IsValid())
	{
		return;
	}

	// Check if all players have been rewarded with contribution tag and remove the entries if so
	// Otherwise we will hold onto them for a while incase the player logs back in to claim them
	bool bNeedsDirtying = false;
	if (bForceRemove)
	{
		for (int32 Index = GetQuestContributions().Num() - 1; Index >= 0; --Index)
		{
			const FQuestContribution& QuestContribution = GetQuestContributions()[Index];
			if (QuestContribution.QuestId == QuestId && QuestContribution.CharacterID == TargetCharacter->GetCharacterID())
			{
				QuestContributions.RemoveAt(Index);
				bNeedsDirtying = true;
			}
		}
	}
	
	bool bAllContributionsRewarded = true;
	for (int32 Index = 0; Index < GetQuestContributions().Num(); ++Index)
	{
		const FQuestContribution& QuestContribution = GetQuestContributions()[Index];
		if (QuestContribution.CharacterID == TargetCharacter->GetCharacterID() && QuestContribution.QuestId == QuestId)
		{
			QuestContributions[Index].bRewarded = true;
			bNeedsDirtying = true;
		}

		if (QuestContribution.QuestId == QuestId && !QuestContribution.bRewarded)
		{
			bAllContributionsRewarded = false;
		}
	}

	if (bAllContributionsRewarded)
	{
		for (int32 Index = GetQuestContributions().Num() - 1; Index >= 0; --Index)
		{
			if (GetQuestContributions()[Index].QuestId == QuestId)
			{
				QuestContributions.RemoveAt(Index);
				bNeedsDirtying = true;
			}
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, QuestContributions, this);
	}
}

void AIQuestManager::SetMaxCompleteQuestsInLocation(int32 NewMaxCompleteQuestsInLocation)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIQuestManager, MaxCompleteQuestsInLocation, NewMaxCompleteQuestsInLocation, this);
}

void AIQuestManager::SetMaxUnclaimedRewards(int32 NewMaxUnclaimedRewards)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIQuestManager, MaxUnclaimedRewards, NewMaxUnclaimedRewards, this);
}

void AIQuestManager::SetShouldEnableMaxUnclaimedRewards(bool bNewShouldEnableMaxUnclaimedRewards)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIQuestManager, bEnableMaxUnclaimedRewards, bNewShouldEnableMaxUnclaimedRewards, this);
}

void AIQuestManager::SetShouldEnableTrophyQuests(bool bNewShouldEnableTrophyQuests)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIQuestManager, bTrophyQuests, bNewShouldEnableTrophyQuests, this);
}

void AIQuestManager::LoadQuestAssets()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::LoadQuestAssets"))

	SCOPE_CYCLE_COUNTER(STAT_QuestLoadQuestAssets);

	bQuestsLoaded = false;

	TArray<FPrimaryAssetId> QuestAssetIds;

	if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
	{
		Manager->GetPrimaryAssetIdList(FPrimaryAssetType(TEXT("QuestData")), QuestAssetIds);
	}
	else
	{
		UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - AssetManager not valid"));
	}

	TMap<FPrimaryAssetId, UQuestData*> Quests{};

	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::ServerLoadAllQuests"))


	FQuestsDataLoaded LoadAssetsDelegate;
	TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
	LoadAssetsDelegate.BindLambda([WeakThis](TArray<UQuestData*> QuestsData)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->OnLoadQuestAssets(QuestsData);
		}
	});

	LoadQuestsData(QuestAssetIds, LoadAssetsDelegate);
}

void AIQuestManager::OnLoadQuestAssets(TArray<UQuestData*> QuestsData)
{
	const FName LevelName = FName(UGameplayStatics::GetCurrentLevelName(this));

#if !UE_BUILD_SHIPPING
	UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - Preloaded %d Quests"), QuestsData.Num());
#endif
	for (UQuestData* QuestData : QuestsData)
	{
		const FPrimaryAssetId& PrimaryAssetId = QuestData->GetPrimaryAssetId();

		if (PrimaryAssetId.IsValid())
		{
			check(QuestData);

			if (!QuestData)
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(TitansQuests, Warning, TEXT("AIQuestManager::LoadQuestAssets: Failed to Load QuestData for %s"), *PrimaryAssetId.ToString());
#endif
				continue;
			}

			if (!QuestData->bEnabled)
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets: Quest %s is disabled"), *PrimaryAssetId.ToString());
#endif
				continue;
			}

			if (QuestData->LevelName != LevelName)
			{
#if !UE_BUILD_SHIPPING
				UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets: Quest %s skipped due to level name mismatch %s != %s"), *PrimaryAssetId.ToString(), *QuestData->LevelName.ToString(), *LevelName.ToString());
#endif
				continue;
			}

#if !UE_BUILD_SHIPPING
			UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets: Using Quest %s"), *PrimaryAssetId.ToString());
#endif

			ServerAddQuest(QuestData, PrimaryAssetId);
		}
	}

	bQuestsLoaded = true;

#if !UE_BUILD_SHIPPING
	UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - QuestAssetIds: %s"), *FString::FromInt(QuestsData.Num()));
	UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - MultiCarnivoreQuests: %s"), *FString::FromInt(MultiCarnivoreQuests.Num()));
	UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - SurvivalQuests: %s"), *FString::FromInt(SurvivalQuests.Num()));
	UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - LocalWorldQuests: %s"), *FString::FromInt(LocalWorldQuests.Num()));
	UE_LOG(TitansQuests, Log, TEXT("AIQuestManager::LoadQuestAssets() - BothAnyQuests: %s"), *FString::FromInt(BothAnyQuests.Num()));
#endif

}

void AIQuestManager::FindAndLoadAssetsFromPath(FString Directory, TArray<FPrimaryAssetId>& CategoryAssets, TArray<FPrimaryAssetId>& AllAssets)
{
	TArray<UObject*> Objects;
	EngineUtils::FindOrLoadAssetsByPath(Directory, Objects, EngineUtils::ATL_Regular);
	TArray<FPrimaryAssetId> NewAssets = ConvertObjectsToAssets(Objects);
	CategoryAssets.Append(NewAssets);
	AllAssets.Append(NewAssets);
}

void AIQuestManager::FindAndLoadDynamicAssets(FString Directory, FString Parent, FString MapName, TArray<FString>& UsableFolders)
{
	UsableFolders.Empty();
	TArray<FString> Folders;
	IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
	FileManager.FindFilesRecursively(Folders, *Directory, nullptr);

	for (int32 i = Folders.Num(); i-- > 0;)
	{
		if (Folders[i].Contains(TEXT("Any"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) || Folders[i].Contains(TEXT("CarnivoreOnly"), ESearchCase::IgnoreCase, ESearchDir::FromEnd) || Folders[i].Contains(TEXT("HerbivoreOnly"), ESearchCase::IgnoreCase, ESearchDir::FromEnd))
		{
			Folders.RemoveAt(i);
			continue;
		}

		FString Left;
		FString Right;

		Folders[i].Split(TEXT("/"), &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
		Left.Split(MapName, &Left, &Right, ESearchCase::IgnoreCase, ESearchDir::FromStart);

		Folders[i] = Parent + MapName + Right;
		UsableFolders.AddUnique(Folders[i]);
	}
}

void AIQuestManager::GetLoadedQuestAssetIDsFromType(EQuestFilter QuestFilter, const AIBaseCharacter* Character, FQuestsIDsLoaded OnLoadAssetsDelegate)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::GetLoadedQuestAssetIDsFromType"))

	SCOPE_CYCLE_COUNTER(STAT_GetLoadedQuestAssetIDsFromType);

	const bool bSinglePlayerQuest = QuestFilter == EQuestFilter::SINGLE_HERB || QuestFilter == EQuestFilter::SINGLE_CARNI || QuestFilter == EQuestFilter::SINGLE_ALL;
	const bool bMultiPlayerQuest = QuestFilter == EQuestFilter::MULTI_HERB || QuestFilter == EQuestFilter::MULTI_CARNI || QuestFilter == EQuestFilter::MULTI_ALL;

	if ((bSinglePlayerQuest || bMultiPlayerQuest) && Character)
	{
		TArray<FPrimaryAssetId> UniqueQuestAssets = bSinglePlayerQuest ? SingleUniqueQuests : MultiUniqueQuests;
		UniqueQuestAssets.Append(BothUniqueQuests);

		FQuestsDataLoaded LoadAssetsDelegate;
		TWeakObjectPtr<AIQuestManager> WeakThis = MakeWeakObjectPtr(this);
		TWeakObjectPtr<const AIBaseCharacter> WeakCharacter = MakeWeakObjectPtr(Character);

		LoadAssetsDelegate.BindLambda([WeakThis, QuestFilter, WeakCharacter, OnLoadAssetsDelegate](TArray<UQuestData*> QuestsData)
		{
			if (WeakThis.IsValid() && WeakCharacter.IsValid())
			{
				WeakThis->OnLoadedQuestAssetIDsFromType(QuestsData, QuestFilter, WeakCharacter.Get(), OnLoadAssetsDelegate);
			}
		});

		LoadQuestsData(UniqueQuestAssets, LoadAssetsDelegate);
	}
	else
	{
		TArray<UQuestData*> QuestsData{};
		OnLoadedQuestAssetIDsFromType(QuestsData, QuestFilter, Character, OnLoadAssetsDelegate);
	}
}

void AIQuestManager::OnLoadedQuestAssetIDsFromType(TArray<UQuestData*> QuestsData, EQuestFilter QuestFilter, const AIBaseCharacter* Character, FQuestsIDsLoaded OnLoadAssetsDelegate)
{
	TArray<FPrimaryAssetId> QuestAssets;

	for (const UQuestData* QuestData : QuestsData)
	{
		if ((!QuestData || !QuestData->bEnabled))
		{
			continue;
		}
		check(QuestData);

		if (QuestData->RestrictCharacterAssetId == Character->CharacterDataAssetId)
		{
			QuestAssets.Add(QuestData->GetPrimaryAssetId());
		}
	}

	switch (QuestFilter)
	{
		case EQuestFilter::SINGLE_CARNI:
		{
			const int32 ReservedAmount = QuestAssets.Num()
				+ SingleCarnivoreQuests.Num()
				+ SingleAnyQuests.Num()
				+ BothCarnivoreQuests.Num()
				+ BothAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(SingleCarnivoreQuests);
			QuestAssets.Append(SingleAnyQuests);
			QuestAssets.Append(BothCarnivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			break;
		}
		case EQuestFilter::SINGLE_HERB:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ SingleHerbivoreQuests.Num() 
				+ SingleAnyQuests.Num() 
				+ BothHerbivoreQuests.Num() 
				+ BothAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(SingleHerbivoreQuests);
			QuestAssets.Append(SingleAnyQuests);
			QuestAssets.Append(BothHerbivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			break;
		}
		case EQuestFilter::SINGLE_ALL:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ SingleCarnivoreQuests.Num() 
				+ SingleHerbivoreQuests.Num() 
				+ SingleAnyQuests.Num() 
				+ BothCarnivoreQuests.Num() 
				+ BothHerbivoreQuests.Num() 
				+ BothAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(SingleCarnivoreQuests);
			QuestAssets.Append(SingleHerbivoreQuests);
			QuestAssets.Append(SingleAnyQuests);
			QuestAssets.Append(BothCarnivoreQuests);
			QuestAssets.Append(BothHerbivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			break;
		}
		case EQuestFilter::MULTI_CARNI:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ MultiCarnivoreQuests.Num() 
				+ MultiAnyQuests.Num() 
				+ BothCarnivoreQuests.Num() 
				+ BothAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(MultiCarnivoreQuests);
			QuestAssets.Append(MultiAnyQuests);
			QuestAssets.Append(BothCarnivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			break;
		}
		case EQuestFilter::MULTI_HERB:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ MultiHerbivoreQuests.Num() 
				+ MultiAnyQuests.Num() 
				+ BothHerbivoreQuests.Num() 
				+ BothAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(MultiHerbivoreQuests);
			QuestAssets.Append(MultiAnyQuests);
			QuestAssets.Append(BothHerbivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			break;
		}
		case EQuestFilter::MULTI_ALL:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ MultiCarnivoreQuests.Num() 
				+ MultiHerbivoreQuests.Num() 
				+ MultiAnyQuests.Num() 
				+ BothCarnivoreQuests.Num()
				+ BothHerbivoreQuests.Num() + BothAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(MultiCarnivoreQuests);
			QuestAssets.Append(MultiHerbivoreQuests);
			QuestAssets.Append(MultiAnyQuests);
			QuestAssets.Append(BothCarnivoreQuests);
			QuestAssets.Append(BothHerbivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			break;
		}
		case EQuestFilter::SURVIVAL:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ SurvivalQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(SurvivalQuests);
			break;
		}
		case EQuestFilter::WATERRESTORE:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ WaterRestoreQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(WaterRestoreQuests);
			break;
		}
		case EQuestFilter::WAYSTONESTORE:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ WaystoneRestoreQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(WaystoneRestoreQuests);
			break;
		}
		case EQuestFilter::LOCALWORLD:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ LocalWorldQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(LocalWorldQuests);
			break;
		}
		case EQuestFilter::GROUP_CARNI:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ GroupCarnivoreQuests.Num() 
				+ GroupAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(GroupCarnivoreQuests);
			QuestAssets.Append(GroupAnyQuests);
			break;
		}
		case EQuestFilter::GROUP_HERB:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ GroupHerbivoreQuests.Num() 
				+ GroupAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(GroupHerbivoreQuests);
			QuestAssets.Append(GroupAnyQuests);
			break;
		}
		case EQuestFilter::GROUP_ALL:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ GroupCarnivoreQuests.Num() 
				+ GroupHerbivoreQuests.Num() 
				+ GroupAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(GroupCarnivoreQuests);
			QuestAssets.Append(GroupHerbivoreQuests);
			QuestAssets.Append(GroupAnyQuests);
			break;
		}
		case EQuestFilter::ALL:
		{
			const int32 ReservedAmount = QuestAssets.Num() 
				+ SingleCarnivoreQuests.Num() 
				+ SingleHerbivoreQuests.Num() 
				+ SingleAnyQuests.Num() 
				+ MultiCarnivoreQuests.Num()
				+ MultiHerbivoreQuests.Num() 
				+ MultiAnyQuests.Num() 
				+ BothCarnivoreQuests.Num() 
				+ BothHerbivoreQuests.Num() 
				+ BothAnyQuests.Num() 
				+ LocalWorldQuests.Num()
				+ WaystoneRestoreQuests.Num() 
				+ WaterRestoreQuests.Num() 
				+ SurvivalQuests.Num() 
				+ GroupCarnivoreQuests.Num() 
				+ GroupHerbivoreQuests.Num() 
				+ GroupAnyQuests.Num();

			QuestAssets.Reserve(ReservedAmount);

			QuestAssets.Append(SingleCarnivoreQuests);
			QuestAssets.Append(SingleHerbivoreQuests);
			QuestAssets.Append(SingleAnyQuests);
			QuestAssets.Append(MultiCarnivoreQuests);
			QuestAssets.Append(MultiHerbivoreQuests);
			QuestAssets.Append(MultiAnyQuests);
			QuestAssets.Append(BothHerbivoreQuests);
			QuestAssets.Append(BothCarnivoreQuests);
			QuestAssets.Append(BothAnyQuests);
			QuestAssets.Append(LocalWorldQuests);
			QuestAssets.Append(WaystoneRestoreQuests);
			QuestAssets.Append(WaterRestoreQuests);
			QuestAssets.Append(SurvivalQuests);
			QuestAssets.Append(GroupCarnivoreQuests);
			QuestAssets.Append(GroupHerbivoreQuests);
			QuestAssets.Append(GroupAnyQuests);
			break;
		}
	}

	if (!QuestAssets.IsEmpty())
	{
		OnLoadAssetsDelegate.ExecuteIfBound(QuestAssets);
	}
}

TArray<FPrimaryAssetId> AIQuestManager::ConvertObjectsToAssets(TArray<UObject*> QuestObjects) const
{
	TArray<FPrimaryAssetId> QuestAssets;

	for (UObject* QuestObject : QuestObjects)
	{
		if (QuestObject) QuestAssets.Add(QuestObject->GetPrimaryAssetId());
	}

	return QuestAssets;
}

void AIQuestManager::ShuffleQuestArray(TArray<FPrimaryAssetId>& ArrayToShuffle)
{
	if (!ArrayToShuffle.IsEmpty())
	{
		int32 LastIndex = ArrayToShuffle.Num() - 1;
		for (int32 i = 0; i <= LastIndex; ++i)
		{
			int32 Index = FMath::RandRange(i, LastIndex);
			if (i != Index)
			{
				ArrayToShuffle.Swap(i, Index);
			}
		}
	}
}

void AIQuestManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestManager, QuestContributions, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestManager, TrophyQuestsOnCooldown, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestManager, MaxCompleteQuestsInLocation, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestManager, MaxUnclaimedRewards, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestManager, bEnableMaxUnclaimedRewards, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestManager, bTrophyQuests, Params);
}

// Hook for when player does generic thing like opening a menu
void AIQuestManager::OnPlayerSubmitGenericTask(AIBaseCharacter* IBaseCharacter, FName TaskKey)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIQuestManager::OnPlayerSubmitGenericTask"))

	check(IBaseCharacter);
	if (!IBaseCharacter) return;
	for (UIQuest* CharacterQuest : IBaseCharacter->GetActiveQuests())
	{
		if (!CharacterQuest) continue;

		for (UIQuestBaseTask* BaseTask : CharacterQuest->GetQuestTasks())
		{
			if (UIQuestGenericTask* GenericTask = Cast<UIQuestGenericTask>(BaseTask))
			{
				if (GenericTask->TaskKey == TaskKey)
				{
					GenericTask->SetIsCompleted(true);
					OnQuestUpdated(IBaseCharacter, CharacterQuest, false);
					return;
				}
			}
		}
	}
}

TArray<FQuestContribution>& AIQuestManager::GetQuestContributions_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, QuestContributions, this);
	return QuestContributions;
}

TArray<FQuestCooldown> AIQuestManager::GetLocalWorldQuestsOnCooldown()
{
	TArray<FQuestCooldown> Cooldowns;
	Cooldowns.Reserve(100);

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return Cooldowns;

	int64 NowTimestamp = FDateTime::UtcNow().ToUnixTimestamp();
	float CleanupDelay = (UE_BUILD_SHIPPING) ? LocalQuestCooldownDelay : 60.0f;

	for (APlayerState* PlayerState : IGameState->PlayerArray)
	{
		AIPlayerState* RemotePlayerState = Cast<AIPlayerState>(PlayerState);
		if (!RemotePlayerState || !RemotePlayerState->GetCharacterAssetId().IsValid()) continue;

		AIBaseCharacter* RemotePawn = Cast<AIBaseCharacter>(RemotePlayerState->GetPawn());
		if (!RemotePawn) continue;

		for (int32 Index = RemotePawn->QuestCooldowns.Num() - 1; Index >= 0; Index--)
		{
			if (RemotePawn->QuestCooldowns[Index].IsExpired(NowTimestamp, CleanupDelay))
			{
				RemotePawn->QuestCooldowns.RemoveAt(Index);
			}
			else
			{
				// Set the character id when loading from save. No need to save the characterID
				RemotePawn->QuestCooldowns[Index].CharacterID = RemotePawn->GetCharacterID();
			}
		}

		Cooldowns.Append(RemotePawn->QuestCooldowns);
	}

	return Cooldowns;
}

void AIQuestManager::AddLocalWorldQuestCooldown(const FQuestCooldown& Cooldown)
{
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	for (APlayerState* PlayerState : IGameState->PlayerArray)
	{
		AIPlayerState* RemotePlayerState = Cast<AIPlayerState>(PlayerState);
		if (!RemotePlayerState || !RemotePlayerState->GetCharacterAssetId().IsValid()) continue;

		AIBaseCharacter* RemotePawn = Cast<AIBaseCharacter>(RemotePlayerState->GetPawn());
		if (!RemotePawn) continue;

		if (RemotePawn->GetCharacterID() == Cooldown.CharacterID)
		{
			RemotePawn->QuestCooldowns.Add(Cooldown);
			return;
		}
	}
}

void AIQuestManager::RemoveLocalWorldQuestCooldown(const FQuestCooldown& Cooldown)
{
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (!IGameState) return;

	for (APlayerState* PlayerState : IGameState->PlayerArray)
	{
		AIPlayerState* RemotePlayerState = Cast<AIPlayerState>(PlayerState);
		if (!RemotePlayerState || !RemotePlayerState->GetCharacterAssetId().IsValid()) continue;

		AIBaseCharacter* RemotePawn = Cast<AIBaseCharacter>(RemotePlayerState->GetPawn());
		if (!RemotePawn) continue;

		if (RemotePawn->GetCharacterID() == Cooldown.CharacterID)
		{
			RemotePawn->QuestCooldowns.Remove(Cooldown);
			return;
		}
	}
}

TArray<FQuestCooldown>& AIQuestManager::GetTrophyQuestsOnCooldown_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIQuestManager, TrophyQuestsOnCooldown, this);
	return TrophyQuestsOnCooldown;
}

void AIQuestManager::OnServerLoadAllQuests(TArray<UQuestData*> QuestsData, TMap<FPrimaryAssetId, UQuestData*>& OutQuests)
{
	for (auto QuestData : QuestsData)
	{
		if (!QuestData)
		{
			continue;
		}

		const FPrimaryAssetId& QuestAssetId = QuestData->GetPrimaryAssetId();

		OutQuests.Emplace(QuestAssetId, QuestData);
	}
}

FORCEINLINE void AIQuestManager::ServerAddQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId)
{
	switch (QuestData->QuestShareType)
	{
		case EQuestShareType::Survival:
		{
			SurvivalQuests.Emplace(QuestAssetId);
			break;
		}
		case EQuestShareType::LocalWorld:
		{
			ServerAddLocalWorldQuest(QuestData, QuestAssetId);
			break;
		}
		case EQuestShareType::Group:
		{
			ServerAddGroupQuest(QuestData, QuestAssetId);
			break;
		}
		case EQuestShareType::Personal:
		{
			ServerAddPersonalQuest(QuestData, QuestAssetId);
			break;
		}
	}

	ServerLoadedQuests.Emplace(QuestData);

	for (TSoftClassPtr<UIQuestBaseTask>& QuestSoftPtr : QuestData->QuestTasks)
	{
		QuestSoftPtr.LoadSynchronous();

		TSubclassOf<UIQuestBaseTask> QuestBaseTaskClass = QuestSoftPtr.Get();
		if (QuestBaseTaskClass)
		{
			ServerLoadedQuestTasks.Emplace(QuestBaseTaskClass);
		}
	}
}

FORCEINLINE void AIQuestManager::ServerAddLocalWorldQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId)
{
	switch (QuestData->QuestLocalType)
	{
		case EQuestLocalType::Water:
		{
			WaterRestoreQuests.Emplace(QuestAssetId);
			break;
		}
		case EQuestLocalType::Waystone:
		{
			WaystoneRestoreQuests.Emplace(QuestAssetId);
			break;
		}
		default:
		{
			LocalWorldQuests.Emplace(QuestAssetId);
			break;
		}
	}
}

FORCEINLINE void AIQuestManager::ServerAddGroupQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId)
{
	switch (QuestData->QuestCharacterType)
	{
		case ECharacterType::CARNIVORE:
		{
			GroupCarnivoreQuests.Emplace(QuestAssetId);
			break;
		}
		case ECharacterType::HERBIVORE:
		{
			GroupHerbivoreQuests.Emplace(QuestAssetId);
			break;
		}
		case ECharacterType::UNKNOWN:
		{
			switch (QuestData->QuestType)
			{
				case EQuestType::GroupMeet:
				{
					GroupMeetQuests.Emplace(QuestAssetId);
					break;
				}
				case EQuestType::GroupSurvival:
				{
					GroupSurvivalQuests.Emplace(QuestAssetId);
					break;
				}
				default:
				{
					GroupAnyQuests.Emplace(QuestAssetId);
					break;
				}
			}
			break;
		}
	}
}

FORCEINLINE void AIQuestManager::ServerAddPersonalQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId)
{
	switch (QuestData->QuestCharacterType)
	{
		case ECharacterType::CARNIVORE:
		{
			if (QuestData->QuestNetType == EQuestNetType::SingleplayerOnly && !IsRunningDedicatedServer())
			{
				SingleCarnivoreQuests.Emplace(QuestAssetId);
			}
			else if (QuestData->QuestNetType == EQuestNetType::MultiplayerOnly && IsRunningDedicatedServer())
			{
				MultiCarnivoreQuests.Emplace(QuestAssetId);
			}
			else if (QuestData->QuestNetType == EQuestNetType::Both)
			{
				BothCarnivoreQuests.Emplace(QuestAssetId);
			}
			break;
		}
		case ECharacterType::HERBIVORE:
		{
			if (QuestData->QuestNetType == EQuestNetType::SingleplayerOnly && !IsRunningDedicatedServer())
			{
				SingleHerbivoreQuests.Emplace(QuestAssetId);
			}
			else if (QuestData->QuestNetType == EQuestNetType::MultiplayerOnly && IsRunningDedicatedServer())
			{
				MultiHerbivoreQuests.Emplace(QuestAssetId);
			}
			else
			{
				BothHerbivoreQuests.Emplace(QuestAssetId);
			}
			break;
		}
		case ECharacterType::UNKNOWN:
		{
			BothAnyQuests.Emplace(QuestAssetId);
			break;
		}
	}
}
