// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Items/IBaseItem.h"
#include "IQuestItem.generated.h"

/**
 * 
 */
UCLASS()
class PATHOFTITANS_API AIQuestItem : public AIBaseItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestItem)
	TSoftObjectPtr<class UHomeCaveDecorationDataAsset> Decoration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = QuestItem)
	FPrimaryAssetId Quest;

	UPROPERTY(BlueprintReadWrite, Replicated)
	FAlderonPlayerID AlderonID;

	// ID of the character who took this trophy
	UPROPERTY(BlueprintReadWrite, Replicated)
	FAlderonUID CharacterID;

	// ID of the player who took this trophy
	UPROPERTY(BlueprintReadWrite, Replicated)
	FAlderonPlayerID CreatorAlderonID;

	virtual bool Notify_TryCarry_Implementation(class AIBaseCharacter* User) override;
	virtual void PickedUp_Implementation(AIBaseCharacter* User) override;
	virtual void Dropped_Implementation(AIBaseCharacter* User, FTransform DropPosition) override;

	virtual bool CanPrimaryBeUsedBy_Implementation(class AIBaseCharacter* User) override { return false; };
	virtual bool CanSecondaryBeUsedBy_Implementation(class AIBaseCharacter* User) override { return false; };
	virtual bool CanCarryBeUsedBy_Implementation(class AIBaseCharacter* User) override;
	virtual FText GetNoInteractionReason_Implementation(class AIBaseCharacter* User) override;

	void SetTrophyAlderonId(FAlderonPlayerID NewPlayerId);
	void SetTrophyCharacterId(FAlderonUID NewCharacterID);
	void SetTrophyCreatorAlderonId(FAlderonPlayerID NewCreatorAlderonID);
};
