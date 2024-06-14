#include "Abilities/POTAbilityAsset.h"
#include "UObject/ObjectSaveContext.h"

FString UPOTAbilityAsset::GetIdentifierString() const
{
	return GetPrimaryAssetId().ToString();
}

FPrimaryAssetId UPOTAbilityAsset::GetPrimaryAssetId() const
{
	// This is a DataAsset and not a blueprint so we can just use the raw FName
	// For blueprints you need to handle stripping the _C suffix
	return FPrimaryAssetId(Type, GetFName());
}

bool UPOTAbilityAsset::IsEnabled() const
{
	// Check remote config flag
	if (!bDisabled && RequiredRemoteConfig != "")
	{
		return IAlderonCommon::Get().GetRemoteConfig().GetRemoteConfigFlag(RequiredRemoteConfig);
	}

	return !bDisabled;
};

#if WITH_EDITOR

void UPOTAbilityAsset::PostLoad()
{
	// If GrantedPassiveEffects does not contain deprecated GrantedPassiveEffect, add it.

	if (GrantedPassiveEffect.Get())
	{
		for (TSubclassOf<UGameplayEffect> ArrayEffect : GrantedPassiveEffects)
		{
			if (ArrayEffect == GrantedPassiveEffect)
			{
				Super::PostLoad();
				return;
			}
		}
		GrantedPassiveEffects.Add(GrantedPassiveEffect);
	}


	Super::PostLoad();
}

void UPOTAbilityAsset::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	// If GrantedPassiveEffects does not contain deprecated GrantedPassiveEffect, add it.
	
	if (GrantedPassiveEffect.Get())
	{
		for (TSubclassOf<UGameplayEffect> ArrayEffect : GrantedPassiveEffects)
		{
			if (ArrayEffect == GrantedPassiveEffect)
			{
				Super::PreSave(ObjectSaveContext);
				return;
			}
		}
		GrantedPassiveEffects.Add(GrantedPassiveEffect);
	}



	Super::PreSave(ObjectSaveContext);
}

#endif //WITH_EDITOR