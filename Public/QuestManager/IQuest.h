// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/NoExportTypes.h"
#include "Net/IObjectNet.h"
#include "AttributeSet.h"
#include "ITypes.h"
#include "IQuest.generated.h"

class AIBaseCharacter;
class AIPlayerGroupActor;

USTRUCT(BlueprintType)
struct FTutorialPrompt
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	FText Text = FText();
	UPROPERTY(BlueprintReadWrite)
	FText LeftTooltipText = FText();
	UPROPERTY(BlueprintReadWrite)
	FText RightTooltipText = FText();
	UPROPERTY(BlueprintReadWrite)
	TArray<class UInputAction*> LeftTooltipActions;
	UPROPERTY(BlueprintReadWrite)
	TArray<class UInputAction*> RightTooltipActions;
	UPROPERTY(BlueprintReadWrite)
	class UIQuest* IQuest = nullptr;
	UPROPERTY(BlueprintReadWrite)
	bool LeftNegate = false;
	UPROPERTY(BlueprintReadWrite)
	bool RightNegate = false;
};

UENUM(BlueprintType)
enum class EQuestNetType : uint8
{
	SingleplayerOnly	UMETA(DisplayName = "Singleplayer Only"),
	MultiplayerOnly		UMETA(DisplayName = "Multiplayer Only"),
	Both				UMETA(DisplayName = "Both"),
	MAX					UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EQuestShareType : uint8
{
	Personal		UMETA(DisplayName = "Personal"),
	Survival		UMETA(DisplayName = "Survival"),
	Group			UMETA(DisplayName = "Group"),
	LocalWorld		UMETA(DisplayName = "Local World"),
	GlobalWorld		UMETA(DisplayName = "Global World"),
	Guild			UMETA(DisplayName = "Guild"),
	Unknown			UMETA(DisplayName = "Unknown"),
	MAX				UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EQuestLocalType : uint8
{
	Water			UMETA(DisplayName = "Water Restoration"),
	Waystone		UMETA(DisplayName = "Waystone Restoration"),
	Unknown			UMETA(DisplayName = "Unknown"),
	MAX				UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EQuestType : uint8
{
	Social			UMETA(DisplayName = "Social"),
	Hunting			UMETA(DisplayName = "Hunting"),
	Exploration		UMETA(DisplayName = "Exploration"),
	MoveTo			UMETA(DisplayName = "Move To"),
	GroupMeet		UMETA(DisplayName = "GroupMeet"),
	Gathering		UMETA(DisplayName = "Gathering"),
	Protecting		UMETA(DisplayName = "Protecting"),
	Delivering		UMETA(DisplayName = "Delivering"),
	Survival		UMETA(DisplayName = "Survival"),
	Water			UMETA(DisplayName = "Water"),
	Waystone		UMETA(DisplayName = "Waystone"),
	Tutorial		UMETA(DisplayName = "Tutorial"),
	WorldTutorial	UMETA(DisplayName = "World Tutorial"),
	TrophyDelivery	UMETA(DisplayName = "Trophy Delivery"),
	GroupSurvival	UMETA(DisplayName = "Group Survival"),
	MAX				UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EQuestFilter : uint8
{
	MULTI_CARNI		UMETA(DisplayName = "Multiplayer Carnivore"),
	MULTI_HERB		UMETA(DisplayName = "Multiplayer Herbivore"),
	MULTI_ALL		UMETA(DisplayName = "Multiplayer ALL"),
	SINGLE_CARNI	UMETA(DisplayName = "Single Carnivore"),
	SINGLE_HERB		UMETA(DisplayName = "Single Herbivore"),
	SINGLE_ALL		UMETA(DisplayName = "Single ALL"),
	SURVIVAL		UMETA(DisplayName = "Survival"),
	WATERRESTORE	UMETA(DisplayName = "Water Restoration"),
	WAYSTONESTORE	UMETA(DisplayName = "Waystone Restoration"),
	LOCALWORLD		UMETA(DisplayName = "Local World"),
	GROUP_HERB		UMETA(DisplayName = "Group Herbivore"),
	GROUP_CARNI		UMETA(DisplayName = "Group Carnivore"),
	GROUP_ALL		UMETA(DisplayName = "Group ALL"),
	ALL				UMETA(DisplayName = "ALL"),
	MAX				UMETA(Hidden)
};

UENUM(BlueprintType)
enum class ERewardAction : uint8
{
	Kill		UMETA(DisplayName = "Kill"),
	Location	UMETA(DisplayName = "Location"),
	Gather		UMETA(DisplayName = "Gather"),
	Deliver		UMETA(DisplayName = "Deliver")
};

UENUM(BlueprintType)
enum class EQuestRewardClaimType : uint8
{
	Default     UMETA(DisplayName = "Default"),
	Auto        UMETA(DisplayName = "Auto"),
	Manual      UMETA(DisplayName = "Manual")
};

USTRUCT(BlueprintType)
struct FQuestItemData
{
	GENERATED_BODY()
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	FName QuestItemTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	FText QuestItemDisplayText;

	// Shown when the item is first hovered
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	FText QuestItemDescriptiveText;
};

USTRUCT(BlueprintType)
struct FQuestHomecaveDecorationReward
{
	GENERATED_BODY()
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = Quest)
	TSoftObjectPtr<class UHomeCaveDecorationDataAsset> Decoration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = Quest)
	int Amount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, Category = Quest, meta = (ClampMin = 0, ClampMax = 100))
	float Chance = 100;

	// We now check the reward before we claim to see if the player gets it
	// All future rewards will have the check done
	// But already unclaimed rewards won't have the check done.
	// This is to check whether the check should also be done when we claim the rewards to work with rewards that already exist.
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = Quest)
	bool bHasCheckBeenDone = false;
};

USTRUCT(BlueprintType)
struct FQuestLineItem
{
	GENERATED_BODY()
public:
	// Player must have atleast 1 of these tags in their "QuestTags"
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	TArray<FName> RequiredTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	FPrimaryAssetId QuestId;
};

// A questline to give quests one after another
USTRUCT(BlueprintType)
struct FQuestLine
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	TArray<FQuestLineItem> Quests;
};

/** Structure for optionally-assigned tasks which are assigned to a group quest when a set of conditions are met */
USTRUCT(BlueprintType)
struct FOptionalTaskData
{
	GENERATED_BODY()
public:
	/** The task that should be given under the following conditions. Note that at least one dinosaur in the group must meet all of the attached conditions for the task to be assigned*/
	UPROPERTY(EditAnywhere)
	TSoftClassPtr<UIQuestBaseTask> Task;

	/** Gameplay Tags that are required on at least one dino in the group to assign this task */
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer RequireGameplayTags;

	/** Quest Tags that are required on at least one dino in the group to assign this task */
	UPROPERTY(EditAnywhere)
	TArray<FName> RequireQuestTags;
};

UCLASS(BlueprintType)
class PATHOFTITANS_API UQuestData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UQuestData();

	// Whether the quest is actually enabled since we don't want to require the modder/developer to move quests out of a directory just to disable them.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Required")
	bool bEnabled = true;

	// Currently only used for Water and Waystone quests to ensure we prioritize those over other local world quests and always allow them to be active regardless
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Required")
	bool bPriority = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Required")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Required")
	FText Description;

	// The time limit on the quest will only be started when the player leaves the area
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	bool bLeftAreaTimeLimit = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	float TimeLimit = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	EQuestRewardClaimType RewardClaimType = EQuestRewardClaimType::Default;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	bool bRewardBasedOnContribution;

	// Growth Points rewarded each time the user contributions
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest, meta = (EditCondition = "bRewardBasedOnContribution"))
	float ContributionRewardGrowth;

	// Mark Points rewarded each time the user contributions
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest, meta = (EditCondition = "bRewardBasedOnContribution"))
	int32 ContributionRewardPoints;

	// Growth Points rewarded when completed
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	float RewardGrowth;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	TArray<FQuestHomecaveDecorationReward> HomeCaveDecorationReward;

	// Growth Points rewarded when completed
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	int32 RewardPoints;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	int32 FailurePoints = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Quest)
	FVector WorldLocation = FVector(ForceInit);

	// Name of the map this is allowed on. (Example: Panjura, MechanicsTestMap, etc)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	FName LevelName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	EQuestType QuestType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	EQuestNetType QuestNetType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	EQuestShareType QuestShareType = EQuestShareType::Unknown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest, meta = (EditCondition = "QuestShareType == EQuestShareType::LocalWorld"))
	EQuestLocalType QuestLocalType = EQuestLocalType::Unknown;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	ECharacterType QuestCharacterType = ECharacterType::UNKNOWN;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quest)
	TArray<TSoftClassPtr<UIQuestBaseTask>> QuestTasks;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quest)
	TArray<UIQuestBaseTask*> LoadedQuestTasks;

	// Quests Tasks which should only be assigned to a group under certain conditions
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quest)
	TArray<FOptionalTaskData> OptionalTasks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	bool bInOrderQuestTasks = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestSelection)
	FPrimaryAssetId RestrictCharacterAssetId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	FName LocationTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	FName QuestTag = FName("Generic");

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest, meta = (ClampMin = 0.0f, ClampMax = 1.0f))
	float GrowthRequirement = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	bool bShowPopupImmediately = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	FGameplayTagContainer RequiredGameplayTags;

public:
	UFUNCTION(BlueprintCallable, Category = Quest)
	virtual FText GetDisplayName() const;
};

UCLASS(BlueprintType, Transient, Abstract)
class PATHOFTITANS_API UIQuestBaseTask : public UIObjectNet
{
	GENERATED_BODY()

public:
	
	FORCEINLINE UIQuest* GetParentQuest() const { return ParentQuest; }

	void SetParentQuest(UIQuest* NewParentQuest);
	
protected:
		
	UPROPERTY(BlueprintReadOnly, Replicated, Category = BaseTask)
	UIQuest* ParentQuest = nullptr;

public:
	
	// Unique Tag that needs to be used
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = BaseTask)
	FName Tag;

	// Display Name used in the Quest
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = BaseTask)
	FText DisplayName;

	FORCEINLINE const FVector_NetQuantize& GetWorldLocation() const { return WorldLocation; }

	void SetWorldLocation(const FVector_NetQuantize& NewWorldLocation);

	FORCEINLINE const TArray<FVector_NetQuantize>& GetExtraMapMarkers() const { return ExtraMapMarkers; }

	TArray<FVector_NetQuantize>& GetExtraMapMarkers_Mutable();

protected:

	UPROPERTY(BlueprintReadWrite, Replicated)
	FVector_NetQuantize WorldLocation;
	
	UPROPERTY(BlueprintReadWrite, Replicated)
	TArray<FVector_NetQuantize> ExtraMapMarkers;

public:

	virtual bool IsCompleted() const PURE_VIRTUAL(UIQuestBaseTask::IsCompleted, return false;)

	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) {};
	virtual void Setup() override;

	UFUNCTION(BlueprintCallable, Category = Quest)
	virtual FText GetTaskText(bool bShowProgress = true) { return FText::FromString("Unknown"); }

	UFUNCTION(BlueprintCallable, Category = Quest)
	virtual float GetTaskCompletion() { return ((float)GetProgressCount() / (float)GetProgressTotal()) * 100.0f;  }

	bool bCompletedEventSent = false;

	// Task blueprint events only supported for solo quests atm, not group quests
	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = Quest)
	void OnTaskAssigned(AIBaseCharacter* Character);

	UFUNCTION(BlueprintCallable, BlueprintImplementableEvent, Category = Quest)
	void OnTaskCompleted(AIBaseCharacter* Character);

	UPROPERTY(EditAnywhere, Category = Quest)
	TArray<FTutorialHighlight> TutorialHighlights;

	UFUNCTION(BlueprintCallable, Category = Quest)
	virtual int GetProgressCount() { return IsCompleted() ? 1 : 0; }

	UFUNCTION(BlueprintCallable, Category = Quest)
	virtual int GetProgressTotal() { return 1; }

	UPROPERTY(SaveGame, BlueprintReadOnly)
	int LastNotifiedProgressCount = 0;
};

USTRUCT(BlueprintType)
struct FQuestTaskProgress
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly)
	UIQuestBaseTask* QuestTask = nullptr;

	UPROPERTY(BlueprintReadOnly)
	int OldProgress = 0;

	UPROPERTY(BlueprintReadOnly)
	int NewProgress = 0;
};

USTRUCT(BlueprintType)
struct FQuestReward
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintReadOnly, SaveGame)
	bool bIsValid = false;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	int32 Points = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	float Growth = 0;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	TArray<FQuestHomecaveDecorationReward> HomeCaveDecorationReward;

	bool ContainsAnyRewards() const;
	bool ContainsCollectableRewards() const;
};

// Quest Task: Survival
// - Character Row Name = Row Name from the Datatable of Characters to kill
// - Current Kill Count = Used for state tracking, should always be 0 in the data table
// - Total Kill Count = Total Required Number of things to kill
UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestPersonalStat : public UIQuestBaseTask
{
	GENERATED_BODY()
	
public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestPersonalStat)
	FText TaskText;

	// Target Attribute: eg Health
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestPersonalStat)
	FGameplayAttribute TargetAttributeValue;

	// Target Attribute: eg MaxHealth
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestPersonalStat)
	FGameplayAttribute TargetAttributeMax;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestPersonalStat)
	bool CheckLessThan = false;

	// 0.25 = 25%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestPersonalStat)
	float StartPercentage;

	// 0.75 = 75%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestPersonalStat)
	float CompletePercentage;

	virtual bool MeetsStartRequirements(AIBaseCharacter* QuestOwner);

	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) override;

	virtual FText GetTaskText(bool bShowProgress = true) override { return TaskText; }
	
	virtual bool IsCompleted() const override { return bCompleted; }

	void SetIsCompleted(bool bNewCompleted);
	
protected:
	
	UPROPERTY(ReplicatedUsing = OnRep_Completed)
	uint8 bCompleted : 1;

	UFUNCTION()
	void OnRep_Completed();
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestFeedMember : public UIQuestPersonalStat
{
	GENERATED_BODY()

public:
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(BlueprintReadOnly, Category = QuestFeedMember)
	AIBaseCharacter* TargetMember = nullptr;

	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) override;

	virtual void Setup() { UIQuestBaseTask::Setup(); }

	virtual void Setup(AIBaseCharacter* TargetCharacter);

	virtual FText GetTaskText(bool bShowProgress = true) override { return FormattedTaskText; }

protected:
	
	UPROPERTY(ReplicatedUsing = OnRep_UpdateText)
	FText FormattedTaskText;

	UFUNCTION()
	void OnRep_UpdateText();
};

// Quest Task: Kill
// - Character Row Name = Row Name from the Datatable of Characters to kill
// - Current Kill Count = Used for state tracking, should always be 0 in the data table
// - Total Kill Count = Total Required Number of things to kill
UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestKillTask : public UIQuestBaseTask
{
	GENERATED_BODY()
	
public:
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FORCEINLINE const TArray<FPrimaryAssetId>& GetCharacterAssetIds() const { return CharacterAssetIds; }

	FORCEINLINE uint8 GetCurrentKillCount() const { return CurrentKillCount; }

	void SetCurrentKillCount(uint8 NewKillCount);

	FORCEINLINE uint8 GetTotalKillCount() const { return TotalKillCount; }

	FORCEINLINE float GetRewardGrowth() const { return RewardGrowth; }

	FORCEINLINE bool IsRewardBasedUponSizeDifference() const { return bRewardBasedUponSizeDifference; }
	
	FORCEINLINE bool ShouldIgnoreCharacterAssetIdsAndUseDietType() const { return bIgnoreCharacterAssetIdsAndUseDietType; }

	FORCEINLINE EDietaryRequirements GetDietaryRequirements() const { return DietaryRequirements; }

protected:
	
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = QuestKill)
	TArray<FPrimaryAssetId> CharacterAssetIds;
	
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = QuestKill)
	bool bIgnoreCharacterAssetIdsAndUseDietType = false;
	
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = QuestKill)
	EDietaryRequirements DietaryRequirements = EDietaryRequirements::HERBIVORE;

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestKill)
	FText TaskText;

protected:

	UPROPERTY(ReplicatedUsing = OnRep_CurrentKillCount, BlueprintReadWrite, Category = QuestKill)
	uint8 CurrentKillCount = 0;
	
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = QuestKill)
	uint8 TotalKillCount = 0;
	
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = QuestKill)
	float RewardGrowth = 0.0f;

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = Quest)
	uint8 RewardPoints;

protected:
	
	UPROPERTY(EditDefaultsOnly, Replicated, BlueprintReadOnly, Category = QuestKill)
	uint8 bRewardBasedUponSizeDifference : 1;

public:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestKill)
	uint8 bIsCritter : 1;

	void Increment();

	virtual FText GetTaskText(bool bShowProgress = true) override;
	virtual int GetProgressCount() override { return CurrentKillCount; }
	virtual int GetProgressTotal() override { return TotalKillCount; }
	virtual bool IsCompleted() const override { return CurrentKillCount >= TotalKillCount; }

	UFUNCTION()
	void OnRep_CurrentKillCount();
};

// Quest Task: Kill Fish
// - Size = Fish size to catch
// - Current Kill Count = Used for state tracking, should always be 0 in the data table
// - Total Kill Count = Total Required Number of things to kill
UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestFishTask : public UIQuestKillTask
{
	GENERATED_BODY()
	
public:
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual FText GetTaskText(bool bShowProgress = true) override;

	FORCEINLINE ECarriableSize GetFishSize() const { return FishSize; }

protected:
	
	UPROPERTY(EditDefaultsOnly, Replicated, SaveGame, BlueprintReadOnly, Category = QuestKill)
	ECarriableSize FishSize;
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestExploreTask : public UIQuestBaseTask
{
	GENERATED_BODY()

public:
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestExplore)
	bool bSkipIfAlreadyInside = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestExplore)
	bool bIsMoveToQuest = false;
	
	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) override;
	
	virtual int GetProgressCount() override { return IsGroupMeet() ? CurrentCount : (bCompleted ? 1 : 0); }
	
	virtual int GetProgressTotal() override { return IsGroupMeet() ? TotalCount : 1; }
	
	virtual FText GetTaskText(bool bShowProgress = true) override;
	
	virtual bool IsCompleted() const override;
	
	void SetIsCompleted(bool bSetCompleted);
	
	void Decrement();
	
	void Increment();

	void SetCurrentCount(uint8 NewCurrentCount);

	void SetTotalCount(uint8 NewTotalCount);

	FORCEINLINE bool IsGroupMeet() const { return bGroupMeet; }

	void SetIsGroupMeet(bool bNewGroupMeet);

protected:

	UPROPERTY(ReplicatedUsing = OnRep_Completed)
	uint8 bCompleted : 1;

	// Current Amount of Group Members in POI
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly)
	uint8 CurrentCount = 0;

	// Total Amount of Group Members Needed
	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly)
	uint8 TotalCount = 0;

	UPROPERTY(Replicated, SaveGame, BlueprintReadOnly)
	uint8 bGroupMeet : 1;

public:

	UFUNCTION()
	void OnRep_Completed();
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestItemTask : public UIQuestBaseTask
{
	GENERATED_BODY()

public:

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	// Quest Items a Herbivore can Deliver to restore the lake
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Item)
	TArray<FQuestItemData> HerbivoreQuestTags;

	// Quest Items a Carnivore can Deliver to restore the lake
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Item)
	TArray<FQuestItemData> CarnivoreQuestTags;

	// Quest Items a Omnivore can Deliver to restore the lake
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Item)
	TArray<FQuestItemData> OmnivoreQuestTags;

protected:

	// Current Amount of Items Delivered
	UPROPERTY(EditDefaultsOnly, ReplicatedUsing = OnRep_CurrentCount, SaveGame, BlueprintReadWrite, Category = Item)
	uint8 CurrentCount = 0;

public:
	
	UFUNCTION()
	void OnRep_CurrentCount();

	// Total Items To Deliver
	UPROPERTY(EditDefaultsOnly, SaveGame, BlueprintReadOnly, Category = Item)
	uint8 TotalCount = 0;

	FORCEINLINE EDietaryRequirements GetDietType() const { return DietType; }

	void SetDietType(EDietaryRequirements NewDietType);

	void SetCurrentPercentage(float NewCurrentPercentage);

protected:

	UPROPERTY(Replicated, BlueprintReadOnly, Category = Item)
	EDietaryRequirements DietType = EDietaryRequirements::MAX;
	
	// What percentage the quest is currently at
	UPROPERTY(Replicated, BlueprintReadOnly, Category = Item)
	float CurrentPercentage = 0.0f;

public:	

	// What percentage the quest needs to be to be considered complete
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Item)
	float CompletePercentage = 100.0f;

	// Only used if there isn't an valid QuestItemDescriptiveText set in the applicable questtag entries above
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Item)
	FText QuestItemDescriptiveText;

	void Increment();
	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) override;
	virtual FText GetTaskText(bool bShowProgress = true) override;
	UFUNCTION(BlueprintCallable, Category = Quest)
	virtual bool GetItemDescriptiveText(FName QuestItemTag, FText& DescriptiveText);
	virtual float GetTaskCompletion() override { return CurrentPercentage; }
	virtual bool IsCompleted() const override { return CurrentPercentage == CompletePercentage; }

	virtual int GetProgressCount() override { return static_cast<int>(CurrentCount); }
	virtual int GetProgressTotal() override { return static_cast<int>(TotalCount); }
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestGatherTask : public UIQuestItemTask
{
	GENERATED_BODY()

public:

	virtual FText GetTaskText(bool bShowProgress = true) override;
	virtual bool IsCompleted() const override { return CurrentCount == TotalCount; }
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestDeliverTask : public UIQuestItemTask
{
	GENERATED_BODY()

public:

	virtual FText GetTaskText(bool bShowProgress = true) override;
	virtual bool IsCompleted() const override { return CurrentCount == TotalCount; }
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestTutorialRestTask : public UIQuestBaseTask
{
	GENERATED_BODY()

public:
	virtual FText GetTaskText(bool bShowProgress = true) override;
	virtual bool IsCompleted() const override { return bComplete; }
	void SetIsCompleted(bool bSetCompleted) { bComplete = bSetCompleted; }
protected:
	bool bComplete = false;
};
UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestWaterRestoreTask : public UIQuestItemTask
{
	GENERATED_BODY()

public:

	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) override;
	virtual FText GetTaskText(bool bShowProgress = true) override;
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestWaystoneCooldownTask : public UIQuestItemTask
{
	GENERATED_BODY()

public:

	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest) override;
	virtual FText GetTaskText(bool bShowProgress = true) override;
};

UCLASS(BlueprintType, Blueprintable)
class PATHOFTITANS_API UIQuestGenericTask : public UIQuestBaseTask
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly)
	FName TaskKey = TEXT("None");

	virtual FText GetTaskText(bool bShowProgress = true) override;
	virtual bool IsCompleted() const override { return bComplete; }
	
	UFUNCTION(BlueprintCallable)
	void SetIsCompleted(bool bSetCompleted) { bComplete = bSetCompleted; }
protected:
	bool bComplete = false;
};

/**
 * 
 */
UCLASS(BlueprintType)
class PATHOFTITANS_API UIQuest : public UIObjectNet
{
	GENERATED_BODY()
	
public:
	
	UIQuest();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FORCEINLINE const FPrimaryAssetId& GetQuestId() const { return QuestId; }

	void SetQuestId(const FPrimaryAssetId& NewQuestId);

	FORCEINLINE const TArray<UIQuestBaseTask*>& GetQuestTasks() const { return QuestTasks; }

	TArray<UIQuestBaseTask*>& GetQuestTasks_Mutable();

	FORCEINLINE AIPlayerGroupActor* GetPlayerGroupActor() const { return PlayerGroupActor; }

	void SetPlayerGroupActor(AIPlayerGroupActor* NewPlayerGroupActor);

protected:
	
	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated, Category = Quest)
	FPrimaryAssetId QuestId;

public:

	// This can easily be reloaded by the FPrimaryAssetID (Which has to save)
	UPROPERTY(BlueprintReadWrite, Transient, Category = Quest)
	UQuestData* QuestData = nullptr;

protected:
	
	// This is saved and re-created as it stores state
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Quest)
	TArray<UIQuestBaseTask*> QuestTasks;
	
	// This is a reference to the group owning this quest if there is one
	UPROPERTY(BlueprintReadWrite, Replicated, Category = Quest)
	AIPlayerGroupActor* PlayerGroupActor;

public:

	int32 GetRemainingTime();
	void SetRemainingTime(int32 NewTime);

	FORCEINLINE bool IsCompleted() const { return bCompleted; }

	void SetIsCompleted(bool bNewCompleted);

	void SetIsTracked(bool bNewTracked);
	
protected:
	
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = Quest)
	int32 RemainingTime = 0;

	// Client replicated expiration variable to prevent having to replicate every second when RemainingTime changes
	UPROPERTY(BlueprintReadOnly, Replicated, Category = Quest)
	int32 WorldEndTime = 0;
	
	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated, Category = Quest)
	uint8 bCompleted : 1;
	
	UPROPERTY(BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_Track, Category = Quest)
	uint8 bTrack : 1;

public:


	UFUNCTION()
	void OnRep_Track();

	// TODO: This function can be used as an event to update 
	// quest UI that currently uses a timer to do refreshes
	void OnTaskUpdated();

	bool IsTracked() const;

	FORCEINLINE bool IsFailureInbound() const { return bFailureInbound; }

	void SetIsFailureInbound(bool bNewFailureInbound);

protected:

	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated, Category = Quest)
	uint8 bFailureInbound : 1;

public:

	// Homecave Decorations given specifically for this instance of the quest
	UPROPERTY(BlueprintReadWrite, SaveGame, Category = Quest)
	TArray<FQuestHomecaveDecorationReward> HomecaveDecorationRewards;

	FORCEINLINE const FQuestReward& GetUncollectedReward() const { return UncollectedReward; }

	void SetUncollectedReward(const FQuestReward& NewUncollectedReward);

protected:
	
	// Only valid after quest has been completed
	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated, Category = Quest)
	FQuestReward UncollectedReward;

public:

	UIQuestBaseTask* GetActiveTask();

	// Authority Only
	void CheckCompletion();

	FText GetRemainingTimeText();
	float GetContributionAsNumber(AIBaseCharacter* TargetCharacter = nullptr);
	FText GetContributionText(AIBaseCharacter* TargetCharacter = nullptr);
	float GetCompletionAsNumber(AIBaseCharacter* TargetCharacter = nullptr);

	// generates an array of all tasks for this quest which are suitable for a group - i.e. with any unfit Optional Tasks filtered out.
	void ComputeValidTasks(TArray<TSoftClassPtr<UIQuestBaseTask>>& OutTasks) const;
	// ensures that this Quest's tasks are appropriate for the group, adding or removing tasks as necessary. Should only be done when team composition changes.
	void UpdateTasksForValidityOnGroupChanged();

	virtual void Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest);

	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	virtual void BeginDestroy() override;
};
