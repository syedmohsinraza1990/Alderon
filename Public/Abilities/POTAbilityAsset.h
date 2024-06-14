// Copyright 2013-2018 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "Engine/DataAsset.h"
#include "Styling/SlateBrush.h"
#include "POTGameplayAbility.h"
#include "TitanAssetManager.h"
#include "POTAbilityAsset.generated.h"

class URPGGameplayAbility;

UENUM(BlueprintType)
enum class EAbilityType : uint8
{
	AT_GENERIC	UMETA(DisplayName = "Generic")
};

UENUM(BlueprintType)
enum class EAbilityCategory : uint8
{
	AC_NONE			UMETA(DisplayName = "None"),
	AC_HEAD			UMETA(DisplayName = "Head"),
	AC_SENSES		UMETA(DisplayName = "Senses"),
	AC_FRONTLIMB	UMETA(DisplayName = "Front Limb"),
	AC_METABOLISM	UMETA(DisplayName = "Metabolism"),
	AC_HIDE			UMETA(DisplayName = "Hide"),
	AC_LEGS			UMETA(DisplayName = "Legs"),
	AC_BACKLIMB		UMETA(DisplayName = "Back Limb"),
	AC_TAIL			UMETA(DisplayName = "Tail"),
	AC_VOICE		UMETA(DisplayName = "Voice"),
	MAX				UMETA(Hidden)
};


UCLASS(BlueprintType)
class PATHOFTITANS_API UPOTAbilityAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Constructor */
	UPOTAbilityAsset()
		: Type(UTitanAssetManager::AbilityAssetType)
	{}

	/** Type of this item, set in native parent class */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Item)
	FPrimaryAssetType Type;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Item)
	EAbilityType AbilityType;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Item)
	EAbilityCategory AbilityCategory;

	/** User-visible short name */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Item)
	FText Name;

	/** User-visible long description */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Item)
	FText Description;

	/** Icon to display */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Item, meta = (AssetBundles = "UI"))
	FSlateBrush Icon;

	/** Ability to grant if this item is slotted */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Abilities)
	TSubclassOf<UPOTGameplayAbility> GrantedAbility;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Abilities)
	FPrimaryAssetId ParentCharacterDataId;

protected:
	/** Passive effect to grant if this item is slotted */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Abilities|Deprecated", meta = (DeprecatedProperty, DeprecationMessage = "GrantedPassiveEffects must now be used."))
	TSubclassOf<UGameplayEffect> GrantedPassiveEffect;

public:
	/** Passive effect to grant if this item is slotted */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = Abilities)
	TArray<TSubclassOf<UGameplayEffect>> GrantedPassiveEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Abilities)
	int32 UnlockCost;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Abilities)
	uint32 bInitiallyUnlocked : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Abilities)
	uint32 bLoseOnDeath : 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Abilities)
	FString RequiredRemoteConfig;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Abilities)
	uint32 bDisabled : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Abilities)
	uint32 bEnabledInDevelopment : 1;

	/** Returns the logical name, equivalent to the primary asset id */
	UFUNCTION(BlueprintCallable, Category = Item)
	FString GetIdentifierString() const;

	/** Overridden to use saved type */
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	virtual bool IsEnabled() const;

#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif
};


