// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Quests/IQuest.h"
#include "ITypes.h"
#include "IGameplayStatics.h"
#include "IGameInstance.h"
#include "Engine/ActorChannel.h"
#include "Player/IBaseCharacter.h"
#include "Online/IPlayerGroupActor.h"
#include "World/IWaterManager.h"
#include "IWorldSettings.h"
#include "World/IWaystoneManager.h"
#include "Online/IPlayerGroupActor.h"
#include "Online/IGameState.h"
#include "Quests/IMoveToQuest.h"

#define LOCTEXT_NAMESPACE "PathOfTitans.QuestData"

UIQuest::UIQuest()
{
	bCompleted = false;
	bTrack = false;
	bFailureInbound = false;
}

UQuestData::UQuestData()
{
	QuestShareType = EQuestShareType::Unknown;
	QuestLocalType = EQuestLocalType::Unknown;
	QuestCharacterType = ECharacterType::UNKNOWN;
}

FText UQuestData::GetDisplayName() const
{
	if (QuestLocalType == EQuestLocalType::Water)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Location"), DisplayName);
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("ReplenishLocation")), Arguments);
	}

	return DisplayName;
}

void UIQuestBaseTask::SetParentQuest(UIQuest* NewParentQuest)
{
	ParentQuest = NewParentQuest;
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestBaseTask, ParentQuest, this);
}

void UIQuestBaseTask::SetWorldLocation(const FVector_NetQuantize& NewWorldLocation)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestBaseTask, WorldLocation, NewWorldLocation, this);
}

TArray<FVector_NetQuantize>& UIQuestBaseTask::GetExtraMapMarkers_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestBaseTask, ExtraMapMarkers, this);
	return ExtraMapMarkers;
}

void UIQuestBaseTask::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestBaseTask, ParentQuest, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestBaseTask, WorldLocation, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestBaseTask, ExtraMapMarkers, Params);
}

bool UIQuestPersonalStat::MeetsStartRequirements(AIBaseCharacter* QuestOwner)
{
	if (!QuestOwner) return false;

	UAbilitySystemComponent* AbilitySystem = QuestOwner->GetAbilitySystemComponent();
	if (AbilitySystem)
	{
		if (!TargetAttributeValue.IsValid() || !TargetAttributeMax.IsValid())
		{
			return false;
		}

		float Value = AbilitySystem->GetNumericAttribute(TargetAttributeValue);
		float MaxValue = AbilitySystem->GetNumericAttribute(TargetAttributeMax);
		float CurrentPercentage = 0.0f;

		if (MaxValue <= 0) return false;

		if (Value > 0)
		{
			CurrentPercentage = Value / MaxValue;
		}

		if (CheckLessThan)
		{
			if (CurrentPercentage >= StartPercentage)
			{
				return true;
			}
		}
		else
		{
			if (CurrentPercentage <= StartPercentage)
			{
				return true;
			}
		}
	}

	return false;
}

// Called on Server Authoritive Only
void UIQuestPersonalStat::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	if (!QuestOwner) return;

	if (bCompleted) return;

	// Check if the player has met the requirements to complete the quest
	if (!bCompleted)
	{
		UAbilitySystemComponent* AbilitySystem = QuestOwner->GetAbilitySystemComponent();
		if (AbilitySystem)
		{
			if (!TargetAttributeValue.IsValid() || !TargetAttributeMax.IsValid())
			{
				return;
			}

			float Value = AbilitySystem->GetNumericAttribute(TargetAttributeValue);
			float MaxValue = AbilitySystem->GetNumericAttribute(TargetAttributeMax);
			float CurrentPercentage = 0.0f;

			if (Value > 0 && MaxValue > 0)
			{
				CurrentPercentage = Value / MaxValue;
			}

			if (CheckLessThan)
			{
				if (CurrentPercentage <= CompletePercentage)
				{
					SetIsCompleted(true);
					OnRep_Completed();
				}
			}
			else
			{
				if (CurrentPercentage >= CompletePercentage)
				{
					SetIsCompleted(true);
					OnRep_Completed();
				}
			}
		}
	}
}

void UIQuestPersonalStat::SetIsCompleted(bool bNewCompleted)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestPersonalStat, bCompleted, bNewCompleted, this);
}

void UIQuestPersonalStat::OnRep_Completed()
{
	if (!ParentQuest) return;
	ParentQuest->OnTaskUpdated();
}

void UIQuestPersonalStat::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestPersonalStat, bCompleted, Params);
}

void UIQuestKillTask::Increment()
{
	if (GetCurrentKillCount() < GetTotalKillCount())
	{
		SetCurrentKillCount(GetCurrentKillCount() + 1);
		OnRep_CurrentKillCount();
	}
}

FText UIQuestKillTask::GetTaskText(bool bShowProgress /*= true*/)
{
	if (bShowProgress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CharacterName"), TaskText);
		Arguments.Add(TEXT("Current"), FText::AsNumber(GetCurrentKillCount()));
		Arguments.Add(TEXT("Total"), FText::AsNumber(GetTotalKillCount()));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestKillFormatStr")), Arguments);
	}
	else
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CharacterName"), TaskText);
		Arguments.Add(TEXT("Total"), FText::AsNumber(GetTotalKillCount()));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestKillFormatStrNoProgress")), Arguments);
	}
}

void UIQuestKillTask::OnRep_CurrentKillCount()
{
	if (!ParentQuest) return;
	ParentQuest->OnTaskUpdated();
}

void UIQuestKillTask::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, CharacterAssetIds, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, CurrentKillCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, TotalKillCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, RewardGrowth, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, bRewardBasedUponSizeDifference, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, bIgnoreCharacterAssetIdsAndUseDietType, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestKillTask, DietaryRequirements, Params);
}

void UIQuestKillTask::SetCurrentKillCount(uint8 NewKillCount)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestKillTask, CurrentKillCount, NewKillCount, this);
}

FText UIQuestFishTask::GetTaskText(bool bShowProgress /*= true*/)
{
	if (GetFishSize() == ECarriableSize::NONE)
	{
		return FText::FromString(TEXT("Fish Size not set!"));
	}

	FString FishSizeAsString;

	switch (GetFishSize())
	{
	case ECarriableSize::SMALL:
	{
		FishSizeAsString = TEXT("Small");
		break;
	}
	case ECarriableSize::MEDIUM:
	{
		FishSizeAsString = TEXT("Medium");
		break;
	}
	case ECarriableSize::LARGE:
	{
		FishSizeAsString = TEXT("Large");
		break;
	}
	}

	if (bShowProgress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("FishSize"), FText::FromString(FishSizeAsString));
		Arguments.Add(TEXT("Current"), FText::AsNumber(GetCurrentKillCount()));
		Arguments.Add(TEXT("Total"), FText::AsNumber(GetTotalKillCount()));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestFishFormatStr")), Arguments);
	}
	else
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("FishSize"), FText::FromString(FishSizeAsString));
		Arguments.Add(TEXT("Total"), FText::AsNumber(GetTotalKillCount()));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestFishFormatStrNoProgress")), Arguments);
	}
}

void UIQuestFishTask::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestFishTask, FishSize, Params);
}

void UIQuestExploreTask::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	Super::Update(QuestOwner, ActiveQuest);

	if (IsCompleted() || !QuestOwner || !ActiveQuest || !ActiveQuest->QuestData) return;

	if (ActiveQuest->GetPlayerGroupActor())
	{
		SetIsGroupMeet(true);
		int32 NewCurrentCount = 0;
		for (AIPlayerState* GroupMemberPS : ActiveQuest->GetPlayerGroupActor()->GetGroupMembers())
		{
			AIBaseCharacter* IBC = GroupMemberPS->GetPawn<AIBaseCharacter>();
			if (!IBC) continue;

			if (IBC->LastLocationEntered && IBC->LastLocationTag == Tag)
			{
				NewCurrentCount++;
				continue;
			}

			for (AIMoveToQuest* POI : IBC->ActiveMoveToQuestPOIs)
			{
				if (POI->GetLocationTag() == Tag)
				{
					NewCurrentCount++;
					break;
				}
			}
		}

		SetCurrentCount(NewCurrentCount);
		SetTotalCount(ActiveQuest->GetPlayerGroupActor()->GetGroupMembers().Num());
	}
}

FText UIQuestExploreTask::GetTaskText(bool bShowProgress /*= true*/)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Location"), DisplayName);
	Arguments.Add(TEXT("Current"), CurrentCount);
	Arguments.Add(TEXT("Total"), TotalCount);

	FText FormatString;

	if (IsGroupMeet())
	{
		if (DisplayName.IsEmpty())
		{
			FormatString = FText::FromStringTable(FName("ST_Quests"), TEXT("QuestGroupMeetFormatStrNoLocationName"));
		}
		else
		{
			FormatString = FText::FromStringTable(FName("ST_Quests"), TEXT("QuestGroupMeetFormatStr"));
		}
	}
	else if (DisplayName.IsEmpty())
	{
		FormatString = FText::FromStringTable(FName("ST_Quests"), TEXT("QuestMoveToFormatStrNoLocationName"));
	}
	else
	{
		if (bIsMoveToQuest)
		{
			FormatString = FText::FromStringTable(FName("ST_Quests"), TEXT("QuestMoveToFormatStr"));
		}
		else
		{
			FormatString = FText::FromStringTable(FName("ST_Quests"), TEXT("QuestLocFormatStr"));
		}
	}

	return FText::Format(FormatString, Arguments);
}

bool UIQuestExploreTask::IsCompleted() const
{
	if (IsGroupMeet())
	{
		return CurrentCount == TotalCount && TotalCount > 1;
	}

	return bCompleted;
}

void UIQuestExploreTask::SetIsCompleted(bool bSetCompleted)
{
	if (bSetCompleted != bCompleted)
	{
		bCompleted = bSetCompleted;
		MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestExploreTask, bCompleted, this);
		OnRep_Completed();
	}
}

void UIQuestExploreTask::Decrement()
{
	--CurrentCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestExploreTask, CurrentCount, this);
}

void UIQuestExploreTask::Increment()
{
	++CurrentCount;
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestExploreTask, CurrentCount, this);
}

void UIQuestExploreTask::SetCurrentCount(uint8 NewCurrentCount)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestExploreTask, CurrentCount, NewCurrentCount, this);
}

void UIQuestExploreTask::SetTotalCount(uint8 NewTotalCount)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestExploreTask, TotalCount, NewTotalCount, this);
}

void UIQuestExploreTask::SetIsGroupMeet(bool bNewGroupMeet)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestExploreTask, bGroupMeet, bNewGroupMeet, this);
}

void UIQuestExploreTask::OnRep_Completed()
{
	if (!ParentQuest) return;
	ParentQuest->OnTaskUpdated();
}

void UIQuestExploreTask::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestExploreTask, bCompleted, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestExploreTask, CurrentCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestExploreTask, TotalCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestExploreTask, bGroupMeet, Params);
}

void UIQuestItemTask::Increment()
{
	if (CurrentCount < TotalCount)
	{
		++CurrentCount;
		MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestItemTask, CurrentCount, this);
		OnRep_CurrentCount();
	}
}

void UIQuestItemTask::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	Super::Update(QuestOwner, ActiveQuest);

	if (!QuestOwner || !ActiveQuest || !ActiveQuest->QuestData) return;

	SetDietType(QuestOwner->DietRequirements);

	// Check if the player has met the requirements to complete the quest
	SetCurrentPercentage(((float)CurrentCount / (float)TotalCount) * 100.0f);
}

FText UIQuestItemTask::GetTaskText(bool bShowProgress /*= true*/)
{
	TArray<FText> QuestItemDisplayTexts;
	if (GetDietType() == EDietaryRequirements::CARNIVORE)
	{
		for (FQuestItemData QuestItemData : CarnivoreQuestTags)
		{
			QuestItemDisplayTexts.Add(QuestItemData.QuestItemDisplayText);
		}
	}
	else if (GetDietType() == EDietaryRequirements::HERBIVORE)
	{
		for (FQuestItemData QuestItemData : HerbivoreQuestTags)
		{
			QuestItemDisplayTexts.Add(QuestItemData.QuestItemDisplayText);
		}
	}
	else if (GetDietType() == EDietaryRequirements::OMNIVORE)
	{
		for (FQuestItemData QuestItemData : OmnivoreQuestTags)
		{
			QuestItemDisplayTexts.Add(QuestItemData.QuestItemDisplayText);
		}
	}

	FString ItemsString;
	TArray<FString> ItemsAsString;

	int i = 0;
	for (FText ItemText : QuestItemDisplayTexts)
	{
		i++;

		FString ItemString = ItemText.ToString();
		if (i == 1)
		{
			ItemsString = ItemString;
		}
		else
		{
			if (i == (QuestItemDisplayTexts.Num()) || QuestItemDisplayTexts.Num() == 2)
			{
				ItemsString.Append(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestAnd")).ToString());
				ItemsString.Append(ItemString);
			}
			else
			{
				ItemsString.Append(TEXT(", "));
				ItemsString.Append(ItemString);
			}
		}
	}

	return FText::FromString(ItemsString);
}

bool UIQuestItemTask::GetItemDescriptiveText(FName QuestItemTag, FText& DescriptiveText)
{
	TArray<FQuestItemData> QuestTags = (GetDietType() == EDietaryRequirements::CARNIVORE) ? CarnivoreQuestTags : ((GetDietType() == EDietaryRequirements::HERBIVORE) ? HerbivoreQuestTags : OmnivoreQuestTags);
	
	if (QuestTags.Num() == 0)
	{
		if (QuestItemTag != Tag) return false;
		DescriptiveText = QuestItemDescriptiveText;
		if (!DescriptiveText.IsEmpty()) return true;
		return false;
	}
	
	for (FQuestItemData QuestItemData : QuestTags)
	{
		if (QuestItemData.QuestItemTag == QuestItemTag)
		{
			DescriptiveText = QuestItemData.QuestItemDescriptiveText;
			return true;
		}
	}

	return false;
}

void UIQuestItemTask::OnRep_CurrentCount()
{
	if (!ParentQuest) return;
	ParentQuest->OnTaskUpdated();
}

void UIQuestItemTask::SetDietType(EDietaryRequirements NewDietType)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestItemTask, DietType, NewDietType, this);
}

void UIQuestItemTask::SetCurrentPercentage(float NewCurrentPercentage)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuestItemTask, CurrentPercentage, NewCurrentPercentage, this);
}

void UIQuestItemTask::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestItemTask, CurrentCount, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestItemTask, DietType, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestItemTask, CurrentPercentage, Params);
}

FText UIQuestGatherTask::GetTaskText(bool bShowProgress /*= true*/)
{
	if (bShowProgress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CollectName"), DisplayName);
		Arguments.Add(TEXT("Current"), FText::AsNumber(CurrentCount));
		Arguments.Add(TEXT("Total"), FText::AsNumber(TotalCount));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestCollectFormatStr")), Arguments);
	}
	else
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("CollectName"), DisplayName);
		Arguments.Add(TEXT("Total"), FText::AsNumber(TotalCount));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestCollectFormatStrNoProgress")), Arguments);
	}
}

FText UIQuestDeliverTask::GetTaskText(bool bShowProgress /*= true*/)
{
	if (bShowProgress)
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("DeliverName"), DisplayName);
		Arguments.Add(TEXT("Current"), FText::AsNumber(CurrentCount));
		Arguments.Add(TEXT("Total"), FText::AsNumber(TotalCount));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestDeliverFormatStr")), Arguments);
	}
	else
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("DeliverName"), DisplayName);
		Arguments.Add(TEXT("Total"), FText::AsNumber(TotalCount));
		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestDeliverFormatStrNoProgress")), Arguments);

	}
}

void UIQuestWaterRestoreTask::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	Super::Update(QuestOwner, ActiveQuest);

	if (!QuestOwner || !ActiveQuest || !ActiveQuest->QuestData) return;

	// Check if the player has met the requirements to complete the quest
	if (AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(QuestOwner))
	{
		if (AIWaterManager* WM = WS->WaterManager)
		{
			SetCurrentPercentage(WM->GetWaterQuality(Tag) * 100.0f);
		}
	}
}

FText UIQuestWaterRestoreTask::GetTaskText(bool bShowProgress /*= true*/)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Location"), DisplayName);
	Arguments.Add(TEXT("Items"), Super::GetTaskText());
	return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestRestoreWaterFormatStr")), Arguments);
}

FText UIQuestTutorialRestTask::GetTaskText(bool bShowProgress /*= true*/)
{
	return DisplayName;
}

FText UIQuestGenericTask::GetTaskText(bool bShowProgress /*= true*/)
{
	return DisplayName;
}

void UIQuestWaystoneCooldownTask::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	Super::Update(QuestOwner, ActiveQuest);

	if (!QuestOwner || !ActiveQuest || !ActiveQuest->QuestData) return;

	// Check if the player has met the requirements to complete the quest
	if (AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(QuestOwner))
	{
		if (AIWaystoneManager* WM = WS->WaystoneManager)
		{
			SetCurrentPercentage(WM->GetWaystoneCooldownProgress(Tag) * 100.0f);
		}
	}
}

FText UIQuestWaystoneCooldownTask::GetTaskText(bool bShowProgress /*= true*/)
{
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Location"), DisplayName);
	Arguments.Add(TEXT("Items"), Super::GetTaskText());
	return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestRestoreWaystoneFormatStr")), Arguments);
}

void UIQuest::SetUncollectedReward(const FQuestReward& NewUncollectedReward)
{
	UncollectedReward = NewUncollectedReward;
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuest, UncollectedReward, this);
}

UIQuestBaseTask* UIQuest::GetActiveTask()
{
	for (UIQuestBaseTask* Task : GetQuestTasks())
	{
		if (Task && !Task->IsCompleted())
		{
			return Task;
		}
	}

	return nullptr;
}

void UIQuest::CheckCompletion()
{
	if (IsCompleted()) return;

	int NumTasksCompleted = 0;

	for (UIQuestBaseTask* Task : GetQuestTasks())
	{
		if (Task->IsCompleted())
		{
			if (!Task->bCompletedEventSent)
			{
				// Make sure completed event is sent only once
				Task->bCompletedEventSent = true;
				if (AIBaseCharacter* Character = Cast<AIBaseCharacter>(GetOuter()))
				{
					Task->OnTaskCompleted(Character);
				}
			}
			NumTasksCompleted++;
		}
	}

	if (NumTasksCompleted == GetQuestTasks().Num())
	{
		SetIsCompleted(true);
	}
}

FText UIQuest::GetRemainingTimeText()
{
	if (QuestData != nullptr && QuestData->TimeLimit > 0.0f)
	{
		int32 TempRemainingTime = GetRemainingTime();
		int RemainingTimeMinutes = TempRemainingTime / 60;
		int RemainingTimeSeconds = (int)TempRemainingTime % 60;

		FNumberFormattingOptions NumberFormatOptions;
		NumberFormatOptions.AlwaysSign = false;
		NumberFormatOptions.UseGrouping = false;
		NumberFormatOptions.MinimumIntegralDigits = 2;
		NumberFormatOptions.MaximumIntegralDigits = 324;

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Minutes"), FText::AsNumber(RemainingTimeMinutes));
		Arguments.Add(TEXT("Seconds"), FText::AsNumber(RemainingTimeSeconds, &NumberFormatOptions));

		return FText::Format(FText::FromString(TEXT("{Minutes}:{Seconds}")), Arguments);
	}
	else {
		return FText::GetEmpty();
	}
}

float UIQuest::GetContributionAsNumber(AIBaseCharacter* TargetCharacter /*= nullptr*/)
{
	if (QuestData != nullptr && QuestData->bRewardBasedOnContribution)
	{
		// Override Reward and Contribution text if there is a group assigned as those values aren't calculated properly before hand.
		AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(TargetCharacter);
		check(WS);

		AIQuestManager* QM = WS->QuestManager;
		check(QM);

		const float QuestContributionPercentage = QM->GetContributionPercentage(TargetCharacter, this);
		const float ContributionPercentage = (QuestContributionPercentage * 100.0f);

		return ContributionPercentage;
	}
	else {
		return 0.0f;
	}
}

FText UIQuest::GetContributionText(AIBaseCharacter* TargetCharacter /*= nullptr*/)
{
	if (QuestData != nullptr && QuestData->bRewardBasedOnContribution)
	{
		FText ProgressText;
		FText RewardText;
		FText ContributionText;

		FNumberFormattingOptions NumberFormatOptions;
		NumberFormatOptions.AlwaysSign = false;
		NumberFormatOptions.UseGrouping = true;
		NumberFormatOptions.RoundingMode = ERoundingMode::ToZero;
		NumberFormatOptions.MinimumIntegralDigits = 1;
		NumberFormatOptions.MaximumIntegralDigits = 3;
		NumberFormatOptions.MinimumFractionalDigits = 0;
		NumberFormatOptions.MaximumFractionalDigits = 0;

		const float TotalCompletion = GetCompletionAsNumber(TargetCharacter);
		ProgressText = FText::AsNumber(TotalCompletion, &NumberFormatOptions);

		// Override Reward and Contribution text if there is a group assigned as those values aren't calculated properly before hand.
		AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(TargetCharacter);
		check(WS);

		AIQuestManager* QM = WS->QuestManager;
		check(QM);

		const float QuestContributionPercentage = QM->GetContributionPercentage(TargetCharacter, this);
		const float ContributionPercentage = (QuestContributionPercentage * 100.0f);

		ContributionText = FText::AsNumber(ContributionPercentage, &NumberFormatOptions);

		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Completion"), ProgressText);
		Arguments.Add(TEXT("Contribution"), ContributionText);

		if (QuestData->QuestShareType == EQuestShareType::LocalWorld)
		{
			return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestContributionLocal")), Arguments);
		}
		else
		{
			return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestContribution")), Arguments);
		}

		return FText::Format(FText::FromStringTable(FName("ST_Quests"), TEXT("QuestContribution")), Arguments);
	}
	else {
		return FText::GetEmpty();
	}
}

float UIQuest::GetCompletionAsNumber(AIBaseCharacter* TargetCharacter /*= nullptr*/)
{
	if (QuestData != nullptr && QuestData->bRewardBasedOnContribution)
	{
		float TotalCompletion = 0.0f;

		for (UIQuestBaseTask* IQuestBaseTask : GetQuestTasks())
		{
			if (!IQuestBaseTask) continue;

			TotalCompletion += IQuestBaseTask->GetTaskCompletion() / GetQuestTasks().Num();
		}

		return TotalCompletion;
	}
	else {
		return 0.0f;
	}
}

void UIQuest::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	if (IsValid(QuestData))
	{
		if (QuestData->bInOrderQuestTasks)
		{
			UIQuestBaseTask* ActiveTask = GetActiveTask();
			if (ActiveTask)
			{
				ActiveTask->Update(QuestOwner, ActiveQuest);
			}
		}
		else
		{
			for (UIQuestBaseTask* QuestBaseTask : GetQuestTasks())
			{
				if (QuestBaseTask)
				{
					QuestBaseTask->Update(QuestOwner, ActiveQuest);
				}
			}
		}
	}
}

bool UIQuest::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWrite = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (UIQuestBaseTask* QuestTask : GetQuestTasks())
	{
		if (QuestTask != nullptr && QuestTask->IsValidLowLevel())
		{
			bWrite |= Channel->ReplicateSubobject(QuestTask, *Bunch, *RepFlags);
			if (IsValid(QuestTask))
			{
				bWrite |= QuestTask->ReplicateSubobjects(Channel, Bunch, RepFlags);
			}
		}
	}

	return bWrite;
}

void UIQuest::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, QuestId, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, QuestTasks, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, PlayerGroupActor, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, WorldEndTime, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, bCompleted, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, bFailureInbound, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, UncollectedReward, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuest, bTrack, Params);
}

void UIQuest::SetQuestId(const FPrimaryAssetId& NewQuestId)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuest, QuestId, NewQuestId, this);
}

TArray<UIQuestBaseTask*>& UIQuest::GetQuestTasks_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuest, QuestTasks, this);
	return QuestTasks;
}

void UIQuest::SetPlayerGroupActor(AIPlayerGroupActor* NewPlayerGroupActor)
{
	PlayerGroupActor = NewPlayerGroupActor;
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuest, PlayerGroupActor, this);
}

bool UIQuest::IsTracked() const
{
	if (GetPlayerGroupActor())
	{
		AIBaseCharacter* IBaseCharacter = UIGameplayStatics::GetIPlayerPawn(this);
		if (IBaseCharacter)
		{
			TOptional<bool> GroupTracking;

			for (int Index = IBaseCharacter->TrackedGroupQuests.Num() - 1; Index >= 0; Index--)
			{
				FLocalQuestTrackingItem& GroupTrackingItem = IBaseCharacter->TrackedGroupQuests[Index];
				if (GroupTrackingItem.Quest == this)
				{
					GroupTracking = GroupTrackingItem.bTrack;
				}
				else if (GroupTrackingItem.Quest == nullptr)
				{
					// Clean up any entries that might be invalid
					IBaseCharacter->TrackedGroupQuests.RemoveAt(Index);
				}
			}

			if (GroupTracking.IsSet())
			{
				return GroupTracking.GetValue();
			}
		}
	}

	return bTrack;
}

void UIQuest::SetIsFailureInbound(bool bNewFailureInbound)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuest, bFailureInbound, bNewFailureInbound, this);
}

void UIQuest::OnRep_Track()
{
	if (GetPlayerGroupActor())
	{
		// On initial bTrack replication for group quests, add this quest to the player's tracked group quests.
		AIBaseCharacter* IBaseCharacter = UIGameplayStatics::GetIPlayerPawn(GetPlayerGroupActor());
		if (IBaseCharacter)
		{
			for (FLocalQuestTrackingItem& GroupTrackingItem : IBaseCharacter->TrackedGroupQuests)
			{
				if (GroupTrackingItem.Quest == this)
				{
					return;
				}
			}
			IBaseCharacter->TrackedGroupQuests.Add(FLocalQuestTrackingItem({ this, (bool)bTrack }));
		}
	}
}

void UIQuest::OnTaskUpdated()
{
	AIBaseCharacter* Character = Cast<AIBaseCharacter>(GetOwner());
	if (!Character) return;

	Character->UpdateQuestMarker();
}

int32 UIQuest::GetRemainingTime()
{
	check(GetOwner());
	if (!GetOwner())
	{
		return -1;
	}

	bool bAuth = GetOwner()->HasAuthority();

	if (bAuth)
	{
		return RemainingTime;
	}
	else
	{
		if (!GetWorld())
		{
			return -1;
		}
		AIGameState* IGameState = UIGameplayStatics::GetIGameState(GetOwner());
		check(IGameState);
		if (!IGameState)
		{
			return -1;
		}

		return WorldEndTime - (int32)IGameState->GetServerWorldTimeSeconds();
	}
}

void UIQuest::SetRemainingTime(int32 NewTime)
{
	AActor* CurrentQuestOwner = GetOwner();
	check(CurrentQuestOwner);
	if (!CurrentQuestOwner)
	{
		return;
	}

	check(GetOwner()->HasAuthority());
	if (!GetOwner()->HasAuthority())
	{
		return;
	}

	RemainingTime = NewTime;
	float NewEndTime = GetWorld()->GetTimeSeconds() + NewTime;
	if (FMath::Abs(NewEndTime - WorldEndTime) > 0.5f)
	{	// Only update if the new end time is 0.5 seconds away from the current time, to prevent changing due to floating errors
		COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuest, WorldEndTime, (int32)NewEndTime, this);
	}
}

void UIQuest::SetIsCompleted(bool bNewCompleted)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuest, bCompleted, bNewCompleted, this);
}

void UIQuest::SetIsTracked(bool bNewTracked)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UIQuest, bTrack, bNewTracked, this);
}

void UIQuest::ComputeValidTasks(TArray<TSoftClassPtr<UIQuestBaseTask>>& OutTasks) const
{
	START_PERF_TIME();
	{
		// "normal" tasks are always valid
		OutTasks.Empty();
		OutTasks.Reserve(QuestData->QuestTasks.Num() + QuestData->OptionalTasks.Num());
		OutTasks.Append(QuestData->QuestTasks);

		// add suitable optional tasks 
		for (const FOptionalTaskData& OptionalTask : QuestData->OptionalTasks)
		{
			// task will not be added unless at least one dino in the group matches all conditions
			for (const AIPlayerState* GroupMemberState : GetPlayerGroupActor()->GetGroupMembers())
			{
				if (const AIBaseCharacter* GroupMemberCharacter = Cast<AIBaseCharacter>(GroupMemberState->GetPawn()))
				{
					FGameplayTagContainer GroupMemberTagContainer = FGameplayTagContainer();
					GroupMemberCharacter->AbilitySystem->GetOwnedGameplayTags(GroupMemberTagContainer);

					if (!OptionalTask.RequireGameplayTags.HasTag(GroupMemberCharacter->CharacterTag) &&
						!GroupMemberCharacter->CharacterTags.HasAny(OptionalTask.RequireGameplayTags) &&
						!GroupMemberTagContainer.HasAny(OptionalTask.RequireGameplayTags)) 
					{
						continue;
					}
					bool bDoesGroupMemberMatchQuestTags = true;
					for (const FName& QuestTag : OptionalTask.RequireQuestTags)
					{
						if (!GroupMemberCharacter->QuestTags.Contains(QuestTag))
						{
							bDoesGroupMemberMatchQuestTags = false;
							break;
						}
					}
					if (bDoesGroupMemberMatchQuestTags)
					{
						OutTasks.Add(OptionalTask.Task);
						break;
					}
				}
			}
		}
	}
	END_PERF_TIME();
	WARN_PERF_TIME_STATIC_WARNONLY(0.1);
}

void UIQuest::UpdateTasksForValidityOnGroupChanged()
{
	TArray<TSoftClassPtr<UIQuestBaseTask>> ShouldHaveTasks{};
	ComputeValidTasks(ShouldHaveTasks);

	bool bTasksNeedDirtying = false;

	for (int32 i = QuestTasks.Num() - 1; i >= 0; --i)
	{
		// "check off" should-have tasks if QuestTasks already contains them, and if ShouldHaveTasks does not contain a QuestTask, remove it
		if (ShouldHaveTasks.Remove(QuestTasks[i]->GetClass()) == 0)
		{
			QuestTasks.RemoveAt(i);
			bTasksNeedDirtying = true;
		}
	}

	// add any tasks the quest should have but doesnt
	for (const TSoftClassPtr<UIQuestBaseTask>& ShouldHaveTask : ShouldHaveTasks)
	{
		UClass* QuestBaseTaskClass = ShouldHaveTask.Get();
		if (!QuestBaseTaskClass)
		{
			continue;
		}
		
		if (UIQuestBaseTask* NewTask = NewObject<UIQuestBaseTask>(GetPlayerGroupActor(), QuestBaseTaskClass))
		{
			NewTask->SetParentQuest(this);
			NewTask->Setup();
			QuestTasks.Add(NewTask);
			bTasksNeedDirtying = true;
		}
		else
		{
			UE_LOG(TitansQuests, Fatal, TEXT("Failed to create an instance of UIQuestBaseTask using NewObject()"));
		}
	}

	if (bTasksNeedDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(UIQuest, QuestTasks, this);
	}
}

void UIQuest::BeginDestroy()
{
	for (int i = 0; i < QuestTasks.Num(); i++)
	{
		UIQuestBaseTask* QuestTask = QuestTasks[i];
		if (!QuestTask)
		{
			continue;
		}

		UWorld* World = GetWorld();
		if (World)
		{
			World->GetLatentActionManager().RemoveActionsForObject(QuestTasks[i]);
		}

		QuestTask->ConditionalBeginDestroy();
	}
	
	QuestTasks.Reset();
	// TODO Erlite: Technically unnecessary, but not taking any chances. Check later?
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuest, QuestTasks, this);

	Super::BeginDestroy();
}


#undef LOCTEXT_NAMESPACE

void UIQuestBaseTask::Setup()
{
	Super::Setup();
	if (AIBaseCharacter* Character = Cast<AIBaseCharacter>(GetOuter()))
	{
		OnTaskAssigned(Character);
	}
}

void UIQuestFeedMember::Update(AIBaseCharacter* QuestOwner, UIQuest* ActiveQuest)
{
	if (!QuestOwner || !TargetMember) return;

	if (bCompleted) return;

	// Check if the player has met the requirements to complete the quest
	if (!bCompleted)
	{
		UAbilitySystemComponent* AbilitySystem = TargetMember->GetAbilitySystemComponent();
		if (AbilitySystem)
		{
			if (!TargetAttributeValue.IsValid() || !TargetAttributeMax.IsValid())
			{
				return;
			}

			float Value = AbilitySystem->GetNumericAttribute(TargetAttributeValue);
			float MaxValue = AbilitySystem->GetNumericAttribute(TargetAttributeMax);
			float CurrentPercentage = 0.0f;

			if (Value > 0 && MaxValue > 0)
			{
				CurrentPercentage = Value / MaxValue;
			}

			if (CheckLessThan)
			{
				if (CurrentPercentage <= CompletePercentage)
				{
					SetIsCompleted(true);
					OnRep_Completed();
				}
			}
			else
			{
				if (CurrentPercentage >= CompletePercentage)
				{
					SetIsCompleted(true);
					OnRep_Completed();
				}
			}
		}
	}
}

void UIQuestFeedMember::Setup(AIBaseCharacter* TargetCharacter)
{
	TargetMember = TargetCharacter;

	FormattedTaskText = FText::Format(TaskText, FText::FromString(TargetMember->GetPlayerState()->GetPlayerName()));
	MARK_PROPERTY_DIRTY_FROM_NAME(UIQuestFeedMember, FormattedTaskText, this);
	OnRep_UpdateText();

	UIQuestBaseTask::Setup();
}

void UIQuestFeedMember::OnRep_UpdateText()
{
	if (!ParentQuest) return;
	ParentQuest->OnTaskUpdated();
}

void UIQuestFeedMember::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UIQuestFeedMember, FormattedTaskText, Params);
}

bool FQuestReward::ContainsAnyRewards() const
{
	return ContainsCollectableRewards() || Points > 0 || Growth > 0.0f;
}

bool FQuestReward::ContainsCollectableRewards() const
{
	return HomeCaveDecorationReward.Num() > 0;
}
