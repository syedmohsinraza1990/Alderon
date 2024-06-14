// Copyright 2018-2020 Alderon Games Ltd. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"


/**
 * This emulates the default discrete levels initter but allows for FGameplayAttributeData
 * Unfortunately since most of the stuff in FAttributeSetInitterDiscreteLevels is private
 * 99% of it needs to be re-created and we can't just subclass it.
 */
class PATHOFTITANS_API FPOTAttributeSetInitter : public FAttributeSetInitter
{
public:
	virtual void PreloadAttributeSetData(const TArray<UCurveTable*>& CurveData) override;
	void PreloadModAttributeSetData(const TArray<UCurveTable*>& CurveData);
	void PreloadAttributeSetDataFromCSV(const FString& CSV);

	virtual void InitAttributeSetDefaults(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, int32 Level, bool bInitialInit) const override;
	virtual void ApplyAttributeDefault(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute& InAttribute, FName GroupName, int32 Level) const override;

	virtual TArray<float> GetAttributeSetValues(UClass* AttributeSetClass, FProperty* AttributeProperty, FName GroupName) const override;

	bool IsSupportedProperty(FProperty* Property) const;
	virtual void InitAttributeSetDefaultsGradient(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, float Level, bool bInitialInit) const;
	virtual void ApplyAttributeDefaultGradient(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute& InAttribute, FName GroupName, float Level) const;

	

protected:
	TSubclassOf<UAttributeSet> FindBestAttributeClass(TArray<TSubclassOf<UAttributeSet> >& ClassList, FString PartialName);

	struct FPOTAttributeDefaultValueList
	{
		void UpdateOrAddPair(FProperty* InProperty, float InValue)
		{
			for (FOffsetValuePair& Pair : List)
			{
				if (Pair.Property == InProperty)
				{
					Pair.Value = InValue;
					return;
				}
			}

			List.Add(FOffsetValuePair(InProperty, InValue));
		}

		struct FOffsetValuePair
		{
			FOffsetValuePair(FProperty* InProperty, float InValue)
				: Property(InProperty), Value(InValue)
			{
			}

			FProperty* Property;
			float		Value;
		};

		TArray<FOffsetValuePair>	List;
	};

	struct FPOTAttributeSetDefaults
	{
		TMap<TSubclassOf<UAttributeSet>, FPOTAttributeDefaultValueList> DataMap;
	};

	struct FPOTAttributeSetDefaultsCollection
	{
		TArray<FPOTAttributeSetDefaults>		LevelData;
	};

	TMap<FName, FPOTAttributeSetDefaultsCollection>	Defaults;
	TMap<FName, FPOTAttributeSetDefaultsCollection> ModDefaults;

protected:
	void SanitizeGroupName(FName& InName) const;

	void PreloadCurveData(const TArray<UCurveTable*>& CurveData, TMap<FName, FPOTAttributeSetDefaultsCollection>& InDefaults);
	void PreloadCurveDataFromCSV(const FString& CSV, TMap<FName, FPOTAttributeSetDefaultsCollection>& InDefaults);

private:
	bool PreloadRow(const FString& RowName, const FString& ClassName, const FString& SetName, const FString& AttributeName, const FRealCurve* Curve, TMap<FName, FPOTAttributeSetDefaultsCollection>& InDefaults);
};
