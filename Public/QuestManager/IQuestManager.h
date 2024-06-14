// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ITypes.h"
#include "Quests/IQuest.h"
#include "World/IWaterManager.h"
#include "World/IWaystoneManager.h"
#include "IQuestManager.generated.h"

USTRUCT(BlueprintType)
struct FDeliveryLocationData
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FName DeliveryTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame)
	FVector DeliveryLocation;

	FDeliveryLocationData()
	{
		DeliveryTag = NAME_None;
		DeliveryLocation = FVector(EForceInit::ForceInitToZero);
	}

	FDeliveryLocationData(FName NewDeliveryTag, FVector NewDeliveryLocation)
	{
		DeliveryTag = NewDeliveryTag;
		DeliveryLocation = NewDeliveryLocation;
	}
};

DECLARE_DELEGATE_OneParam(FQuestDataLoaded, UQuestData*);
DECLARE_DELEGATE_OneParam(FQuestsDataLoaded, TArray<UQuestData*>);
DECLARE_DELEGATE_OneParam(FQuestIDLoaded, FPrimaryAssetId);
DECLARE_DELEGATE_OneParam(FQuestsIDsLoaded, TArray<FPrimaryAssetId>);

USTRUCT()
struct FQuestSave
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(SaveGame)
	FString BaseQuestObject;

	UPROPERTY(SaveGame)
	TArray<FString> QuestTasks;
};

USTRUCT(BlueprintType)
struct FQuestCooldown
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY(BlueprintReadOnly, SaveGame)
	int64 UnixTimestamp = 0;
	
	UPROPERTY(BlueprintReadOnly)
	float Timestamp = 0.0f;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	FPrimaryAssetId QuestId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestContribution)
	FAlderonUID CharacterID;

	bool IsExpired(int64 CurrentUnixTime, float CooldownSeconds) const
	{
		FDateTime TimestampTime = FDateTime::FromUnixTimestamp(UnixTimestamp);
		FDateTime NowTime = FDateTime::FromUnixTimestamp(CurrentUnixTime);
		FTimespan CooldownTimespan = FTimespan(0, 0, CooldownSeconds);
		FDateTime CooldownExpiry = TimestampTime + CooldownTimespan;
		return NowTime >= CooldownExpiry;
	}

	FQuestCooldown() 
	{
		UnixTimestamp = 0;
	}

	FQuestCooldown(FPrimaryAssetId NewQuestId, FAlderonUID NewCharacterID, int64 NewTimestamp)
	{
		QuestId = NewQuestId;
		CharacterID = NewCharacterID;
		UnixTimestamp = NewTimestamp;
	}

	FQuestCooldown(FPrimaryAssetId NewQuestId, FAlderonUID NewCharacterID, float NewTimestamp)
	{
		QuestId = NewQuestId;
		CharacterID = NewCharacterID;
		Timestamp = NewTimestamp;
	}

	friend bool operator==(const FQuestCooldown& LHS, const FQuestCooldown& RHS)
	{
		return LHS.QuestId == RHS.QuestId && (RHS.CharacterID == FAlderonUID(-1) || LHS.CharacterID == RHS.CharacterID);
	}
};


USTRUCT(BlueprintType)
struct FQuestContribution
{
	GENERATED_USTRUCT_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestContribution)
	FAlderonUID CharacterID;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestContribution)
	int32 ContributionAmount = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestContribution)
	FPrimaryAssetId QuestId;

	// Whether the player was rewarded already
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestContribution)
	bool bRewarded = false;

	// Time when the quest was completed to allow a certain amount of time before this is cleared out
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestContribution)
	float Timestamp = 0;
};

USTRUCT(BlueprintType)
struct FRecentQuest
{
	GENERATED_BODY()
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = Quest)
	FPrimaryAssetId QuestId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = Quest)
	EQuestType QuestType = EQuestType::MAX;

	FRecentQuest()
	{
		QuestId = FPrimaryAssetId();
		QuestType = EQuestType::MAX;
	}

	FRecentQuest(FPrimaryAssetId NewQuestId, EQuestType NewQuestType)
	{
		QuestId = NewQuestId;
		QuestType = NewQuestType;
	}
};

class AIBaseCharacter;
class AICritterPawn;
class AIPlayerGroupActor;
class UIQuest;
class UIQuestBaseTask;
class UQuestData;

UCLASS()
class PATHOFTITANS_API AIQuestManager : public AActor
{
	GENERATED_BODY()

public:
	
	AIQuestManager();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//virtual void Tick(float DeltaTime) override;

	void LoadContributionData();
	void SaveContributionData();

	void OnSaveContributionData(TArray<FPrimaryAssetId> QuestAssetIds);

	FString GetSaveSlotName();
	FString GetMapName();

protected:
	void QuestTick();
	void QuestTock();
	void OnQuestTock(AIBaseCharacter* OwningCharacter, FPrimaryAssetId QuestAssetId);
	void ContributionTick();
	void CooldownTick();

	FTimerHandle TimerHandle_QuestTick;
	FTimerHandle TimerHandle_QuestTock;
	FTimerHandle TimerHandle_ContributionTick;
	FTimerHandle TimerHandle_CooldownTick;

public:

	bool IsPoiCompatibleForExploration(AActor* Poi, AIBaseCharacter* Character) const;

	void GetRandomQuest(AIBaseCharacter* Character, FQuestIDLoaded QuestIDLoaded, EQuestShareType PreferredType = EQuestShareType::Unknown);
	bool HasRoomForQuest(const AIBaseCharacter* Character, const EQuestShareType PreferredType, EQuestType& ActivePersonalQuestType);
	void OnGetRandomQuest(AIBaseCharacter* Character, EQuestShareType PreferredType, EQuestType ActivePersonalQuestType, TArray<FPrimaryAssetId> Quests, FQuestIDLoaded QuestIDLoaded);
	void GetRandomGroupQuest(AIPlayerGroupActor* PlayerGroupActor, FQuestIDLoaded QuestIDLoaded);
	void OnGetRandomGroupQuest(EQuestFilter QuestFilter, TArray<FPrimaryAssetId> Quests, AIPlayerGroupActor* PlayerGroupActor, FQuestIDLoaded QuestIDLoaded, float MinGrowthInGroup, TArray<FName, TInlineAllocator<3>> QuestTags);
	void GetLocalWorldQuests(AIBaseCharacter* Character, FQuestsDataLoaded OnLoadAssetsDelegate);

	void FilterOnLocalWorldQuestsLoaded(TArray<UQuestData*> QuestsData, AIBaseCharacter* Character, FQuestsDataLoaded OnLoadAssetsDelegate);
	void GetQuestByName(const FString& DisplayName, FQuestIDLoaded QuestIDLoaded, const AIBaseCharacter* Character = nullptr);

	void OnGetQuestByName(TArray<FPrimaryAssetId> QuestAssetIds, FQuestIDLoaded QuestIDLoaded, const FString TrimmedDisplayName);

	bool GetQuestDisplayName(const FPrimaryAssetId Quest, FText& OutDisplayName) const;

	void AssignRandomPersonalQuests(AIBaseCharacter* TargetCharacter);
	void AssignRandomQuest(AIBaseCharacter* TargetCharacter, bool bRefreshNotice = false, AIPlayerGroupActor* PlayerGroupActor = nullptr, bool bForGroup = false);
	void OnAssignRandomQuest(AIBaseCharacter* TargetCharacter, bool bRefreshNotice, AIPlayerGroupActor* PlayerGroupActor, bool bForGroup, FPrimaryAssetId QuestAssetId);
	void AssignNextHatchlingTutorialQuest(AIBaseCharacter* TargetCharacter);
	bool ShouldAutoTrack(UQuestData* QuestData, AIBaseCharacter* Character);
	UFUNCTION(BlueprintCallable, Category = Quests)
	void AssignQuest(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter, bool bRefreshNotice = false, AIPlayerGroupActor* PlayerGroupActor = nullptr, bool bForGroup = false);
	void AssignQuestLoaded(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter, UIQuest* NewQuest, UQuestData* QuestData, bool bRefreshNotice = false, AIPlayerGroupActor* PlayerGroupActor = nullptr, bool bForGroup = false);

	void AssignWaterReplenishQuest(AIBaseCharacter* TargetCharacter, FName WaterTag);
	void AssignRandomLocalQuest(AIBaseCharacter* TargetCharacter);

	void OnAssignRandomLocalQuest(TArray<UQuestData*> PossibleQuestAssets, AIBaseCharacter* TargetCharacter);
	void AssignLocalWorldQuest(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter);
	void AssignLocalWorldQuestLoaded(FPrimaryAssetId QuestAssetId, AIBaseCharacter* TargetCharacter, UIQuest* NewQuest, UQuestData* QuestData);

	bool HasLocalQuest(AIBaseCharacter* TargetCharacter) const;
	bool HasTutorialQuest(const AIBaseCharacter* TargetCharacter) const;
	void ClearTutorialQuests(AIBaseCharacter* TargetCharacter);
	void ClearNonTutorialQuests(AIBaseCharacter* TargetCharacter);

	bool GroupMemberHasGroupMeetCooldown(AIPlayerGroupActor* PlayerGroupActor) const;
	bool ShouldAssignGroupMeetQuest(AIPlayerGroupActor* PlayerGroupActor) const;
	void AssignGroupMeetQuest(AIPlayerGroupActor* PlayerGroupActor);

	bool DoGroupMembersContainQuestTag(TArrayView<const AIBaseCharacter*> GroupMemberCharacters, const FName& QuestTag) const;

	void UpdateGroupQuestsOnCooldown(FAlderonUID CharacterID, FPrimaryAssetId QuestId);
	void ClearCompletedQuests(AIBaseCharacter* TargetCharacter);
	void UpdateCompletedLocationQuests(UIQuest* TargetQuest, AIBaseCharacter* TargetCharacter);

	UFUNCTION(BlueprintCallable)
	float GetLocationCompletionPercentage(FName LocationTag, AIBaseCharacter* IBaseCharacter);

	bool HasTrophyQuestCooldown(AIBaseCharacter* TargetCharacter);

	bool HasCompletedLocation(FName LocationTag, AIBaseCharacter* TargetCharacter);
	bool HasCompletedQuest(FPrimaryAssetId QuestId, AIBaseCharacter* TargetCharacter);
	bool HasCompletedGroupQuest(FPrimaryAssetId QuestId, AIPlayerGroupActor* PlayerGroupActor = nullptr);
	EQuestType GetLastQuestType(AIBaseCharacter* TargetCharacter);
	TArray<FRecentQuest> GetRecentCompletedGroupQuests(AIPlayerGroupActor* PlayerGroupActor = nullptr);

	bool HasFailedQuest(FPrimaryAssetId QuestId, AIBaseCharacter* TargetCharacter);
	bool HasFailedGroupQuest(FPrimaryAssetId QuestId, AIPlayerGroupActor* PlayerGroupActor = nullptr);
	TArray<FRecentQuest> GetRecentFailedGroupQuests(AIPlayerGroupActor* PlayerGroupActor = nullptr);

	void UpdateTaskLocation(UIQuestBaseTask* QuestTask);
	
	static void LoadQuestsData(const TArray<FPrimaryAssetId>& QuestAssetIds, FQuestsDataLoaded OnLoaded);

	UFUNCTION(BlueprintCallable, Category = Quests)
	static UQuestData* LoadQuestData(const FPrimaryAssetId& QuestAssetId);

	void LoadQuestData(const FPrimaryAssetId& QuestAssetId, FQuestDataLoaded OnLoaded);
	void OnQuestDataLoaded(FPrimaryAssetId QuestAssetId, FQuestDataLoaded OnLoaded);

	UIQuest* LoadQuest(AIBaseCharacter* Character, const FQuestSave& QuestSave, bool bMapHasChanged, bool IsActiveQuest);
	void LoadQuests(AIBaseCharacter* Character, bool bMapHasChanged = false);
	static void SaveQuest(AIBaseCharacter* Character);
	void ResetQuest(AIBaseCharacter* TargetCharacter, UIQuest* QuestToReset);

	void OnQuestRefresh(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest);

	UFUNCTION(BlueprintCallable, Category = Quests)
	void OnQuestFail(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bFromTeleport = false, bool bFromDisband = false, bool bFromDeath = false);
	void OnQuestProgress(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bNotification = true);

	// Remove quest without doing anything else like failure or giving a new quest. It does not handle group quests atm
	void RemoveQuest(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest);

	void UpdateLocalQuestFailure(UIQuest* Quest, AIBaseCharacter* Character, bool bRemove);
	void UpdateGroupQuestFailure(UIQuest* Quest, AIPlayerGroupActor* PlayerGroupActor, bool bRemove, bool bFailed = false);

	UFUNCTION(BlueprintCallable, Category = Quests)
	void OnQuestCompleted(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest);

	void AwardGrowth(AIBaseCharacter* TargetCharacter, float Growth);

	void CollectQuestReward(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest);

	void OnQuestUpdated(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bProgressGained);

	// Hooks to Update Quest / Achievement Events
	void OnCharacterKilled(AIBaseCharacter* Killer, AIBaseCharacter* Victim);

	// Hooks to Update Quest / Achievement Events
	void OnFishKilled(AIBaseCharacter* Killer, AIFish* Fish);

	// Hooks to Update Quest / Achievement Events
	void OnCritterKilled(AIBaseCharacter* Killer, AICritterPawn* Critter);

	// Hooks for Point of Interest / Location Quests
	void OnLocationDiscovered(FName LocationTag, const FText& LocationDisplayName, AIBaseCharacter* Character, bool bEntered, bool bNotification = true, bool bFromTeleport = false);

	// Hooks for Collect Quests
	void OnItemCollected(FName ItemTag, AIBaseCharacter* Character, int32 Count = 1);

	// Hooks for Delivery Quests
	void OnItemDelivered(FName ItemTag, AIBaseCharacter* Character, UIQuest* Quest, int32 Count = 1);

	void OnContributeRestore(FName ContributionTag, float RestoreValue, AIBaseCharacter* Character, UIQuest* Quest);

	// Hooks for Restore Water Quests
	void OnWaterRestore(FName WaterTag, float RestoreValue, AIBaseCharacter* Character, UIQuest* Quest);

	void OnWaterQualityDroppedBelowThreshold(FWaterData WaterData, FWaterLocationData WaterLocationData);
	void OnWaterQualityRestored(FWaterData WaterData, FWaterLocationData WaterLocationData);

	// Hooks for Restore Water Quests
	void OnWaystoneRestore(FName WaystoneTag, int32 RestoreValue, AIBaseCharacter* Character, UIQuest* Quest);

	void OnWaystoneCooldownActivated(FName WaystoneTag);
	void OnWaystoneCooldownDeactivated(FName WaystoneTag, bool bFromPlayer = false, AIBaseCharacter* Character = nullptr);

	// Hook for when player does generic thing like opening a menu
	void OnPlayerSubmitGenericTask(AIBaseCharacter* IBaseCharacter, FName TaskKey);

	UFUNCTION(BlueprintCallable, Category = QuestManager)
	float GetContributionPercentage(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest);

	void RemoveRewardedQuestContributionEntry(AIBaseCharacter* TargetCharacter, UIQuest* TargetQuest, bool bForceRemove = false);

	// Max Recently Completed Quests to keep track of in order to prevent giving the player the same one
	UPROPERTY(EditDefaultsOnly, Category = QuestManager, meta = (ClampMin = "1", ClampMax = "15", UIMin = "1", UIMax = "15"))
	int32 MaxRecentCompletedQuests = 5;

	// Max Recently Completed Quests to keep track of in order to prevent giving the player the same one
	UPROPERTY(EditDefaultsOnly, Category = QuestManager, meta = (ClampMin = "1", ClampMax = "15", UIMin = "1", UIMax = "15"))
	int32 MaxRecentFailedQuests = 1;

	// Max Distance to allow a POI to be from the player if trying to assign it, Distance in Unreal Units. (Default = 1000 meters)
	// Last resort will use the closest POI if one isn't found within this distance
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	int32 MaxQuestPOIDistance = 100000;

	//The max distance for Move To quests, so that far away "move to" quests don't get assigned to the player.
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	int32 MaxQuestMoveToDistance = 100000;

	/**
	 * How quickly the growth given is rewarded
	 * Constant rate of growth reward given over a variable amount of time
	 */
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float GrowthRewardRatePerMinute = 0.1;

	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float BabyGrowthRewardRatePerMinute = 0.3;

	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	bool bAllowRepeatQuests = false;

	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	bool bAllowRepeatQuestTypes = false;

	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	bool bAllowQuestRefresh = true;

	// Time before quest contribution will be removed after the quest has been completed.
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float QuestContributionCleanupDelay = 600.f;

	// Time before a local world quest can be reassigned to a individual
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float LocalQuestCooldownDelay = 600.f;

	// Time before a group quest can be assigned to another group again.
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float GroupQuestCleanupDelay = 300.0f;

	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float TrophyQuestCooldownDelay = 1800.0f;

	// Time before a location quest can be assigned to another group again.
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float LocationCompletedCleanupDelay = 3600.0f;

	// Time before a group quest can be assigned to another group again.
	UPROPERTY(EditDefaultsOnly, Category = QuestManager)
	float GroupMeetQuestCooldownDelay = 300.0f;

	FORCEINLINE int32 GetMaxCompleteQuestsInLocation() const { return MaxCompleteQuestsInLocation; }

	void SetMaxCompleteQuestsInLocation(int32 NewMaxCompleteQuestsInLocation);

	FORCEINLINE int32 GetMaxUnclaimedRewards() const { return MaxUnclaimedRewards; }

	void SetMaxUnclaimedRewards(int32 NewMaxUnclaimedRewards);

	FORCEINLINE bool ShouldEnableMaxUnclaimedRewards() const { return bEnableMaxUnclaimedRewards; }

	void SetShouldEnableMaxUnclaimedRewards(bool bNewShouldEnableMaxUnclaimedRewards);

	FORCEINLINE bool ShouldEnableTrophyQuests() const { return bTrophyQuests; }

	UFUNCTION(BlueprintCallable, Category = QuestManager)
	void SetShouldEnableTrophyQuests(bool bNewShouldEnableTrophyQuests);

protected:

	UPROPERTY(EditAnywhere, Replicated, Category = QuestManager)
	int32 MaxCompleteQuestsInLocation = 3;

	UPROPERTY(BlueprintReadOnly,Replicated, Category = QuestManager)
	int32 MaxUnclaimedRewards = 10;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = QuestManager)
	bool bEnableMaxUnclaimedRewards = true;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = QuestManager)
	bool bTrophyQuests = true;

public:

	bool bQuestsLoaded = false;

	void LoadQuestAssets();
	void OnLoadQuestAssets(TArray<UQuestData*> QuestsData);
	void FindAndLoadAssetsFromPath(FString Directory, TArray<FPrimaryAssetId>& CategoryAssets, TArray<FPrimaryAssetId>& AllAssets);
	void FindAndLoadDynamicAssets(FString Directory, FString Parent, FString MapName, TArray<FString>& UsableFolders);

	void GetLoadedQuestAssetIDsFromType(EQuestFilter QuestFilter, const AIBaseCharacter* Character, FQuestsIDsLoaded OnLoadAssetsDelegate);

	void OnLoadedQuestAssetIDsFromType(TArray<UQuestData*> QuestsData, EQuestFilter QuestFilter, const AIBaseCharacter* Character, FQuestsIDsLoaded OnLoadAssetsDelegate);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<FPrimaryAssetId> ConvertObjectsToAssets(TArray<UObject*> QuestObjects) const;

	void ShuffleQuestArray(TArray<FPrimaryAssetId>& ArrayToShuffle);

	// Only Multiplayer Quests
	TArray<FPrimaryAssetId> MultiCarnivoreQuests;
	TArray<FPrimaryAssetId> MultiHerbivoreQuests;
	TArray<FPrimaryAssetId> MultiAnyQuests;

	// Only Singleplayer Quests
	TArray<FPrimaryAssetId> SingleCarnivoreQuests;
	TArray<FPrimaryAssetId> SingleHerbivoreQuests;
	TArray<FPrimaryAssetId> SingleAnyQuests;

	// Both Single and Multiplayer Quests
	TArray<FPrimaryAssetId> BothCarnivoreQuests;
	TArray<FPrimaryAssetId> BothHerbivoreQuests;
	TArray<FPrimaryAssetId> BothAnyQuests;

	// Other
	TArray<FPrimaryAssetId> SurvivalQuests;
	TArray<FPrimaryAssetId> WaterRestoreQuests;
	TArray<FPrimaryAssetId> WaystoneRestoreQuests;
	TArray<FPrimaryAssetId> LocalWorldQuests;

	// Dinosaur Unique Quests
	TArray<FPrimaryAssetId> SingleUniqueQuests;
	TArray<FPrimaryAssetId> MultiUniqueQuests;
	TArray<FPrimaryAssetId> BothUniqueQuests;

	// Group Quests
	TArray<FPrimaryAssetId> GroupCarnivoreQuests;
	TArray<FPrimaryAssetId> GroupHerbivoreQuests;
	TArray<FPrimaryAssetId> GroupAnyQuests;
	TArray<FPrimaryAssetId> GroupMeetQuests;
	TArray<FPrimaryAssetId> GroupSurvivalQuests;

	UPROPERTY(BlueprintReadOnly, Category = QuestManager)
	TArray<FName> ActiveWaterRestorationQuestTags;

	UPROPERTY(BlueprintReadOnly, Category = QuestManager)
	TArray<FName> ActiveWaystoneRestoreQuestTags;

	FORCEINLINE const TArray<FQuestContribution>& GetQuestContributions() const { return QuestContributions; }

	TArray<FQuestContribution>& GetQuestContributions_Mutable();

protected:

	UPROPERTY(Replicated, BlueprintReadOnly, Category = QuestManager)
	TArray<FQuestContribution> QuestContributions;

public:

	// Group Quests in this array will be slowly cooled down over a period of time, once removed from this array they will be allowed to regenerate for new groups
	UPROPERTY(BlueprintReadOnly, Category = QuestManager)
	TArray<FQuestCooldown> GroupQuestsOnCooldown;

	UPROPERTY(BlueprintReadOnly, Category = QuestManager)
	TArray<FQuestCooldown> GroupMeetQuestCooldowns;

	// Local World Quests in this array will be slowly cooled down over a period of time, once removed from this array they will be allowed to regenerate for specific players
	TArray<FQuestCooldown> GetLocalWorldQuestsOnCooldown();
	void AddLocalWorldQuestCooldown(const FQuestCooldown& Cooldown);
	void RemoveLocalWorldQuestCooldown(const FQuestCooldown& Cooldown);

	FORCEINLINE const TArray<FQuestCooldown>& GetTrophyQuestsOnCooldown() const { return TrophyQuestsOnCooldown; }

	TArray<FQuestCooldown>& GetTrophyQuestsOnCooldown_Mutable();

protected:

	UPROPERTY(BlueprintReadOnly, Replicated, Category = QuestManager)
	TArray<FQuestCooldown> TrophyQuestsOnCooldown;

public:

	UPROPERTY()
	TArray<UQuestData*> ServerLoadedQuests;

	UPROPERTY()
	TArray<TSubclassOf<UIQuestBaseTask>> ServerLoadedQuestTasks;

	UPROPERTY(BlueprintReadOnly)
	TArray<class AActor*> AllPointsOfInterest;

	//UPROPERTY(BlueprintReadOnly)
	//TArray<class AIDeliveryPoint*> AllDeliveryLocations;

	UPROPERTY()
	TArray<class AIFishSpawner*> FishSpawnLocations;

	UPROPERTY(BlueprintReadWrite, VisibleAnywhere, SaveGame)
	TArray<FDeliveryLocationData> DeliveryLocationData;

protected:

	// New Optimized Quest Loading
	void OnServerLoadAllQuests(TArray<UQuestData*> QuestsData, TMap<FPrimaryAssetId, UQuestData*>& OutQuests);
	FORCEINLINE void ServerAddQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId);
	FORCEINLINE void ServerAddLocalWorldQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId);
	FORCEINLINE void ServerAddGroupQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId);
	FORCEINLINE void ServerAddPersonalQuest(UQuestData* QuestData, const FPrimaryAssetId& QuestAssetId);
};