// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.


#include "Items/IQuestItem.h"
#include "IWorldSettings.h"
#include "Quests/IQuestManager.h"
#include "Player/IBaseCharacter.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "Online/IPlayerState.h"
#include "Online/IGameState.h"

bool AIQuestItem::Notify_TryCarry_Implementation(class AIBaseCharacter* User)
{
	// Disabled Temporarily
	if (AIDinosaurCharacter* DinoUser = Cast<AIDinosaurCharacter>(User))
	{
		if (!IsPendingKillPending() && IUsableInterface::Execute_CanCarryBeUsedBy(this, User))
		{
			// Can't carry while eating or drinking
			if (DinoUser->IsEatingOrDrinking() || DinoUser->IsResting() || DinoUser->IsCarryingObject())
			{
				UE_LOG(TitansLog, Error, TEXT("AIFish::Carry - Dinosaur Can't carry due to Eating, Drinking, Resting, or already carrying object!"));

				return false;
			}

			if (IsInteractionInProgress()) return false;

			if (HasAuthority())
			{
				SetIsInteractionInProgress(true);
				ForceNetUpdate();
			}

			return true;
		}
	}

	return false;
}

void AIQuestItem::PickedUp_Implementation(AIBaseCharacter* User)
{
	Super::PickedUp_Implementation(User);

	AIQuestManager* const QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager;
	if (!HasAuthority() || !QuestMgr || !User)
	{
		return;
	}

	QuestMgr->AssignQuest(Quest, User);
}

void AIQuestItem::Dropped_Implementation(AIBaseCharacter* User, FTransform DropPosition)
{
	Super::Dropped_Implementation(User, DropPosition);

	AIQuestManager* const QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager;
	if (!HasAuthority() || !QuestMgr || !User)
	{
		return;
	}

	for (UIQuest* const ActiveQuest : User->GetActiveQuests())
	{
		if (ActiveQuest->GetQuestId() == Quest)
		{
			QuestMgr->OnQuestFail(User, ActiveQuest);
			break;
		}
	}
}

bool AIQuestItem::CanCarryBeUsedBy_Implementation(class AIBaseCharacter* User)
{
	if (User)
	{
		AIPlayerState* IPS = Cast<AIPlayerState>(User->GetPlayerState());
		if (IPS && IPS->GetAlderonID() == AlderonID) return false;

		AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager;
		if (!QuestMgr) return false;

		if (QuestMgr->HasTrophyQuestCooldown(User)) return false;

		// If the User alderon ID is the same as the ID who created this trophy, the characterID must be the same.
		if (User->GetCharacterID() != CharacterID && IPS->GetAlderonID() == CreatorAlderonID) return false;
	}

	return User && !IsInteractionInProgress() && ((!ICarryInterface::Execute_IsCarried(this) && CanPickupItem(User)) || ICarryInterface::Execute_IsCarriedBy(this, User));
}

void AIQuestItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{ COND_None, REPNOTIFY_OnChanged, true };

	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestItem, AlderonID, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestItem, CharacterID, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIQuestItem, CreatorAlderonID, Params);
}

FText AIQuestItem::GetNoInteractionReason_Implementation(class AIBaseCharacter* User)
{
	if (User)
	{
		AIPlayerState* IPS = Cast<AIPlayerState>(User->GetPlayerState());

		AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager;
		if (!QuestMgr) return InteractionPromptData.InteractionFailureReasonFallback;

		if (QuestMgr->HasTrophyQuestCooldown(User)) return InteractionPromptData.InteractionFailureReasonOne;
		if (User->GetCharacterID() != CharacterID && IPS->GetAlderonID() == CreatorAlderonID) return InteractionPromptData.InteractionFailureReasonTwo;

	}

	return InteractionPromptData.InteractionFailureReasonFallback;
}

void AIQuestItem::SetTrophyAlderonId(FAlderonPlayerID NewPlayerId)
{
	if (AlderonID != NewPlayerId)
	{
		AlderonID = NewPlayerId;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, AlderonID, this);
	}
}

void AIQuestItem::SetTrophyCharacterId(FAlderonUID NewCharacterID)
{
	if (CharacterID != NewCharacterID)
	{
		CharacterID = NewCharacterID;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CharacterID, this);
	}
}

void AIQuestItem::SetTrophyCreatorAlderonId(FAlderonPlayerID NewCreatorAlderonID)
{
	if (CreatorAlderonID != NewCreatorAlderonID)
	{
		CreatorAlderonID = NewCreatorAlderonID;
		MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, CreatorAlderonID, this);
	}
}