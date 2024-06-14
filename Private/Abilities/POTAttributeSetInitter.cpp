// Copyright 2018-2020 Alderon Games Ltd. All rights reserved.

#include "Abilities/POTAttributeSetInitter.h"
#include "AbilitySystemLog.h"
#include "AbilitySystemComponent.h"
#include "Serialization/Csv/CsvParser.h"

TSubclassOf<UAttributeSet> FPOTAttributeSetInitter::FindBestAttributeClass(TArray<TSubclassOf<UAttributeSet> >& ClassList, FString PartialName)
{
	for (auto Class : ClassList)
	{
		if (Class->GetName().Contains(PartialName))
		{
			return Class;
		}
	}

	return nullptr;
}

void FPOTAttributeSetInitter::PreloadAttributeSetData(const TArray<UCurveTable*>& CurveData)
{
	PreloadCurveData(CurveData, Defaults);
}

void FPOTAttributeSetInitter::PreloadModAttributeSetData(const TArray<UCurveTable*>& CurveData)
{
	ModDefaults.Empty();
	PreloadCurveData(CurveData, ModDefaults);
}

void FPOTAttributeSetInitter::PreloadAttributeSetDataFromCSV(const FString& CSV)
{
	PreloadCurveDataFromCSV(CSV, Defaults);
}

void FPOTAttributeSetInitter::InitAttributeSetDefaults(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, int32 Level, bool bInitialInit) const
{
	check(AbilitySystemComponent != nullptr);

	const FPOTAttributeSetDefaultsCollection* Collection = Defaults.Find(GroupName);
	const FPOTAttributeSetDefaultsCollection* ModCollection = ModDefaults.Find(GroupName);

	if (!Collection)
	{
		ABILITY_LOG(Warning, TEXT("Unable to find DefaultAttributeSet Group %s. Falling back to Defaults"), *GroupName.ToString());
		Collection = Defaults.Find(FName(TEXT("Default")));
		if (!Collection)
		{
			ABILITY_LOG(Error, TEXT("FAttributeSetInitterDiscreteLevels::InitAttributeSetDefaults Default DefaultAttributeSet not found! Skipping Initialization"));
			return;
		}
	}

	if (!Collection->LevelData.IsValidIndex(Level - 1))
	{
		// We could eventually extrapolate values outside of the max defined levels
		ABILITY_LOG(Warning, TEXT("Attribute defaults for Level %d are not defined! Skipping"), Level);
		return;
	}

	const FPOTAttributeSetDefaults& SetDefaults = Collection->LevelData[Level - 1];
	for (const UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes()) // this might need to be GetSpawnedAttributes_Mutable
	{
		const FPOTAttributeDefaultValueList* DefaultDataList = SetDefaults.DataMap.Find(Set->GetClass());
		const FPOTAttributeDefaultValueList* ModDefaultDataList = nullptr;

		if (ModCollection != nullptr && ModCollection->LevelData.IsValidIndex(Level - 1))
		{
			const FPOTAttributeSetDefaults& ModSetDefaults = ModCollection->LevelData[Level - 1];
			ModDefaultDataList = ModSetDefaults.DataMap.Find(Set->GetClass());
		}

		if (DefaultDataList)
		{
			ABILITY_LOG(Log, TEXT("Initializing Set %s"), *Set->GetName());

			for (int32 j = 0; j < DefaultDataList->List.Num(); j++)
			{
				auto DataPair = DefaultDataList->List[j];

				if (ModDefaultDataList && ModDefaultDataList->List.IsValidIndex(j))
				{
					DataPair = ModDefaultDataList->List[j];
				}

				check(DataPair.Property);

				if (Set->ShouldInitProperty(bInitialInit, DataPair.Property))
				{
					FGameplayAttribute AttributeToModify(DataPair.Property);
					AbilitySystemComponent->SetNumericAttributeBase(AttributeToModify, DataPair.Value);
				}

			}
		}
	}

	AbilitySystemComponent->ForceReplication();
}

void FPOTAttributeSetInitter::ApplyAttributeDefault(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute& InAttribute, FName GroupName, int32 Level) const
{
	const FPOTAttributeSetDefaultsCollection* Collection = Defaults.Find(GroupName);
	const FPOTAttributeSetDefaultsCollection* ModCollection = ModDefaults.Find(GroupName);

	if (!Collection)
	{
		ABILITY_LOG(Warning, TEXT("Unable to find DefaultAttributeSet Group %s. Falling back to Defaults"), *GroupName.ToString());
		Collection = Defaults.Find(FName(TEXT("Default")));
		if (!Collection)
		{
			ABILITY_LOG(Error, TEXT("FAttributeSetInitterDiscreteLevels::InitAttributeSetDefaults Default DefaultAttributeSet not found! Skipping Initialization"));
			return;
		}
	}

	if (!Collection->LevelData.IsValidIndex(Level - 1))
	{
		// We could eventually extrapolate values outside of the max defined levels
		ABILITY_LOG(Warning, TEXT("Attribute defaults for Level %d are not defined! Skipping"), Level);
		return;
	}

	const FPOTAttributeSetDefaults& SetDefaults = Collection->LevelData[Level - 1];
	for (const UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes()) // this might need to be GetSpawnedAttributes_Mutable
	{
		const FPOTAttributeDefaultValueList* DefaultDataList = SetDefaults.DataMap.Find(Set->GetClass());
		const FPOTAttributeDefaultValueList* ModDefaultDataList = nullptr;

		if (ModCollection != nullptr && ModCollection->LevelData.IsValidIndex(Level - 1))
		{
			const FPOTAttributeSetDefaults& ModSetDefaults = ModCollection->LevelData[Level - 1];
			ModDefaultDataList = ModSetDefaults.DataMap.Find(Set->GetClass());
		}

		if (DefaultDataList)
		{
			ABILITY_LOG(Log, TEXT("Initializing Set %s"), *Set->GetName());

			for (int32 j = 0; j < DefaultDataList->List.Num(); j++)
			{
				auto DataPair = DefaultDataList->List[j];

				if (ModDefaultDataList && ModDefaultDataList->List.IsValidIndex(j))
				{
					DataPair = ModDefaultDataList->List[j];
				}

				check(DataPair.Property);

				if (DataPair.Property == InAttribute.GetUProperty())
				{
					FGameplayAttribute AttributeToModify(DataPair.Property);
					AbilitySystemComponent->SetNumericAttributeBase(AttributeToModify, DataPair.Value);
				}
				
			}
		}
	}

	AbilitySystemComponent->ForceReplication();
}

TArray<float> FPOTAttributeSetInitter::GetAttributeSetValues(UClass* AttributeSetClass, FProperty* AttributeProperty, FName GroupName) const
{
	TArray<float> AttributeSetValues;
	const FPOTAttributeSetDefaultsCollection* Collection = Defaults.Find(GroupName);
	const FPOTAttributeSetDefaultsCollection* ModCollection = ModDefaults.Find(GroupName);

	if (!Collection)
	{
		ABILITY_LOG(Error, TEXT("FAttributeSetInitterDiscreteLevels::InitAttributeSetDefaults Default DefaultAttributeSet not found! Skipping Initialization"));
		return TArray<float>();
	}

	for (int32 i = 0; i < Collection->LevelData.Num(); i++)
	{
		const FPOTAttributeSetDefaults& SetDefaults = Collection->LevelData[i];
		
		const FPOTAttributeDefaultValueList* DefaultDataList = SetDefaults.DataMap.Find(AttributeSetClass);
		const FPOTAttributeDefaultValueList* ModDefaultDataList = nullptr;

		if (ModCollection != nullptr && ModCollection->LevelData.IsValidIndex(i))
		{
			const FPOTAttributeSetDefaults& ModSetDefaults = ModCollection->LevelData[i];
			ModDefaultDataList = ModSetDefaults.DataMap.Find(AttributeSetClass);
		}

		if (DefaultDataList)
		{
			for(int32 j = 0; j < DefaultDataList->List.Num(); j++)
			{
				auto DataPair = DefaultDataList->List[j];

				if (ModDefaultDataList && ModDefaultDataList->List.IsValidIndex(j))
				{
					DataPair = ModDefaultDataList->List[j];
				}

				check(DataPair.Property);
				if (DataPair.Property == AttributeProperty)
				{
					AttributeSetValues.Add(DataPair.Value);
				}
				
			}
		}
	}
	return AttributeSetValues;
}

bool FPOTAttributeSetInitter::IsSupportedProperty(FProperty* Property) const
{
	return (Property && (CastField<FNumericProperty>(Property) || FGameplayAttribute::IsGameplayAttributeDataProperty(Property)));
}

void FPOTAttributeSetInitter::SanitizeGroupName(FName& InName) const
{
	FString GroupString = InName.ToString();

	FString Preffix;
	FString Name;
	FString Suffix;
	FString Temp;

	if (GroupString.Split(TEXT("_"), &Preffix, &Temp))
	{
		Temp.Split(TEXT("_"), &Name, &Suffix);
		InName = FName(*Name);
	}




}


void FPOTAttributeSetInitter::PreloadCurveData(const TArray<UCurveTable*>& CurveData, TMap<FName, FPOTAttributeSetDefaultsCollection>& InDefaults)
{
	if (!ensure(CurveData.Num() > 0))
	{
		return;
	}



	/**
	*	Loop through CurveData table and build sets of Defaults that keyed off of Name + Level
	*/
	for (const UCurveTable* CurTable : CurveData)
	{
		if (CurTable == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Found nullptr curve in CurveData."));
			continue;
		}
		
		for (const TPair<FName, FRealCurve*>& CurveRow : CurTable->GetRowMap())
		{
			FString RowName = CurveRow.Key.ToString();
			FString ClassName;
			FString SetName;
			FString AttributeName;
			FString Temp;

			RowName.Split(TEXT("."), &ClassName, &Temp);
			Temp.Split(TEXT("."), &SetName, &AttributeName);

			if (!ensure(!ClassName.IsEmpty() && !SetName.IsEmpty() && !AttributeName.IsEmpty()))
			{
				ABILITY_LOG(Verbose, TEXT("FAttributeSetInitterDiscreteLevels::PreloadAttributeSetData Unable to parse row %s in %s"), *RowName, *CurTable->GetName());
				continue;
			}

			PreloadRow(RowName, ClassName, SetName, AttributeName, CurveRow.Value, InDefaults);
		}
	}
}

void FPOTAttributeSetInitter::PreloadCurveDataFromCSV(const FString& CSV, TMap<FName, FPOTAttributeSetDefaultsCollection>& InDefaults)
{
	FCsvParser Parser(CSV);
	
	TArray<FString> CSVData;
	TArray<TArray<const TCHAR*>> Rows = Parser.GetRows();
	for (TArray<const TCHAR*>& SingleRow : Rows)
	{
		for (const TCHAR*& RowData : SingleRow)
		{
			CSVData.Add(RowData);
		}
	}

	int Key = 0;
	for (int i = 0; i < CSVData.Num(); i++)
	{
		if (i % 6 == 0)
		{
			if (Key != 0)
			{

				FString RowName = FString(*CSVData[i]);
				FString ClassName;
				FString SetName;
				FString AttributeName;
				FString Temp;

				RowName.Split(TEXT("."), &ClassName, &Temp);
				Temp.Split(TEXT("."), &SetName, &AttributeName);

				if (!ensure(!ClassName.IsEmpty() && !SetName.IsEmpty() && !AttributeName.IsEmpty()))
				{
					ABILITY_LOG(Verbose, TEXT("FAttributeSetInitterDiscreteLevels::PreloadAttributeSetData Unable to parse row %s in CSV"), *RowName);
					continue;
				}


				FSimpleCurve* Curve = new FSimpleCurve();
				
				Curve->AddKey(1.f, FCString::Atof(*CSVData[i + 1]));
				Curve->AddKey(2.f, FCString::Atof(*CSVData[i + 2]));
				Curve->AddKey(3.f, FCString::Atof(*CSVData[i + 3]));
				Curve->AddKey(4.f, FCString::Atof(*CSVData[i + 4]));
				Curve->AddKey(5.f, FCString::Atof(*CSVData[i + 5]));

				PreloadRow(RowName, ClassName, SetName, AttributeName, Curve, InDefaults);
			}

			Key++;
		}
	}
}

bool FPOTAttributeSetInitter::PreloadRow(const FString& RowName, const FString& ClassName, const FString& SetName, const FString& AttributeName, const FRealCurve* Curve, TMap<FName, FPOTAttributeSetDefaultsCollection>& InDefaults)
{
	/**
	*	Get list of AttributeSet classes loaded
	*/
	static TArray<TSubclassOf<UAttributeSet> >	ClassList;
	if (ClassList.Num() == 0)
	{
		for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
		{
			UClass* TestClass = *ClassIt;
			if (TestClass->IsChildOf(UAttributeSet::StaticClass()))
			{
				ClassList.Add(TestClass);
			}
		}
	}
	

	TSubclassOf<UAttributeSet> Set = FindBestAttributeClass(ClassList, SetName);
	if (!Set)
	{
		// This is ok, we may have rows in here that don't correspond directly to attributes
		ABILITY_LOG(Verbose, TEXT("FAttributeSetInitterDiscreteLevels::PreloadAttributeSetData Unable to match AttributeSet from %s (row: %s)"), *SetName, *RowName);
		return false;
	}

	// Find the FProperty
	FProperty* Property = FindFProperty<FProperty>(*Set, *AttributeName);
	if (!IsSupportedProperty(Property))
	{
		ABILITY_LOG(Verbose, TEXT("FAttributeSetInitterDiscreteLevels::PreloadAttributeSetData Unable to match Attribute from %s (row: %s)"), *AttributeName, *RowName);
		return false;
	}

	FName ClassFName = FName(*ClassName);
	FPOTAttributeSetDefaultsCollection& DefaultCollection = InDefaults.FindOrAdd(ClassFName);

	// Check our curve to make sure the keys match the expected format
	bool bShouldSkip = false;
	for (auto KeyIter = Curve->GetKeyHandleIterator(); KeyIter; ++KeyIter)
	{
		const FKeyHandle& KeyHandle = *KeyIter;
		if (KeyHandle == FKeyHandle::Invalid())
		{
			ABILITY_LOG(Verbose, TEXT("FAttributeSetInitterDiscreteLevels::PreloadAttributeSetData Data contains an invalid key handle (row: %s)"), *RowName);
			bShouldSkip = true;
			break;
		}
	}

	if (bShouldSkip)
	{
		return false;
	}

	int32 LastLevel = Curve->GetKeyTime(Curve->GetLastKeyHandle());
	DefaultCollection.LevelData.SetNum(FMath::Max(LastLevel, DefaultCollection.LevelData.Num()));

	for (int32 i = 0; i < LastLevel; i++)
	{
		int32 Level = i + 1;
		float Value = Curve->Eval((float)Level);

		FPOTAttributeSetDefaults& SetDefaults = DefaultCollection.LevelData[Level - 1];

		FPOTAttributeDefaultValueList* DefaultDataList = SetDefaults.DataMap.Find(Set);
		if (DefaultDataList == nullptr)
		{
			ABILITY_LOG(Verbose, TEXT("Initializing new default set for %s[%d]. PropertySize: %d.. DefaultSize: %d"), *Set->GetName(), Level, Set->GetPropertiesSize(), UAttributeSet::StaticClass()->GetPropertiesSize());

			DefaultDataList = &SetDefaults.DataMap.Add(Set);
		}

		// Import curve value into default data

		check(DefaultDataList);
		DefaultDataList->UpdateOrAddPair(Property, Value);
	}

	return true;
}

void FPOTAttributeSetInitter::InitAttributeSetDefaultsGradient(UAbilitySystemComponent* AbilitySystemComponent, FName GroupName, float Level, bool bInitialInit) const
{
	check(AbilitySystemComponent != nullptr);


	const FPOTAttributeSetDefaultsCollection* Collection = Defaults.Find(GroupName);
	const FPOTAttributeSetDefaultsCollection* ModCollection = ModDefaults.Find(GroupName);

	if (!Collection)
	{
		if (ModCollection)
		{
			Collection = ModCollection;
		}
		else
		{
			ABILITY_LOG(Warning, TEXT("Unable to find DefaultAttributeSet Group %s. Falling back to Defaults"), *GroupName.ToString());
			Collection = Defaults.Find(FName(TEXT("Default")));
			if (!Collection)
			{
				ABILITY_LOG(Error, TEXT("FAttributeSetInitterDiscreteLevels::InitAttributeSetDefaults Default DefaultAttributeSet not found! Skipping Initialization"));
				return;
			}
		}
		
	}

	int32 BottomLevel = FMath::FloorToInt(Level);
	int32 TopLevel = FMath::CeilToInt(Level);

	if (!Collection->LevelData.IsValidIndex(BottomLevel - 1) || !Collection->LevelData.IsValidIndex(TopLevel - 1))
	{
		// We could eventually extrapolate values outside of the max defined levels
		ABILITY_LOG(Warning, TEXT("Attribute defaults for Level %d are not defined! Skipping"), Level);
		return;
	}

	const FPOTAttributeSetDefaults& BottomSetDefaults = Collection->LevelData[BottomLevel - 1];
	const FPOTAttributeSetDefaults& TopSetDefaults = Collection->LevelData[TopLevel - 1];

	for (const UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes()) // this might need to be GetSpawnedAttributes_Mutable
	{
		if (!Set)
		{
			continue;
		}

		const FPOTAttributeDefaultValueList* DefaultDataList = BottomSetDefaults.DataMap.Find(Set->GetClass());
		const FPOTAttributeDefaultValueList* TopDefaultDataList = TopSetDefaults.DataMap.Find(Set->GetClass());

		const FPOTAttributeDefaultValueList* ModDefaultDataList = nullptr;
		const FPOTAttributeDefaultValueList* ModTopDefaultDataList = nullptr;

		if (ModCollection != nullptr && ModCollection->LevelData.IsValidIndex(BottomLevel - 1) && ModCollection->LevelData.IsValidIndex(TopLevel - 1))
		{
			const FPOTAttributeSetDefaults& ModBottomSetDefaults = ModCollection->LevelData[BottomLevel - 1];
			const FPOTAttributeSetDefaults& ModTopSetDefaults = ModCollection->LevelData[TopLevel - 1];

			ModDefaultDataList = ModBottomSetDefaults.DataMap.Find(Set->GetClass());
			ModTopDefaultDataList = ModTopSetDefaults.DataMap.Find(Set->GetClass());
		}

		if (DefaultDataList && TopDefaultDataList)
		{
			ABILITY_LOG(Log, TEXT("Initializing Set %s"), *Set->GetName());

			for (int32 i = 0; i < DefaultDataList->List.Num(); i++)
			{

				auto DataPairBottom = DefaultDataList->List[i];
				auto DataPairTop = TopDefaultDataList->List[i];

				if (ModDefaultDataList != nullptr && ModTopDefaultDataList != nullptr 
					&& ModDefaultDataList->List.IsValidIndex(i) && ModTopDefaultDataList->List.IsValidIndex(i))
				{
					DataPairBottom = ModDefaultDataList->List[i];
					DataPairTop = ModTopDefaultDataList->List[i];
				}

				
				check(DataPairBottom.Property && DataPairTop.Property);

				if (Set->ShouldInitProperty(bInitialInit, DataPairBottom.Property))
				{
					FGameplayAttribute AttributeToModify(DataPairBottom.Property);
					float ActualValue = FMath::Lerp(DataPairBottom.Value, DataPairTop.Value, FMath::Frac(Level));

					AbilitySystemComponent->SetNumericAttributeBase(AttributeToModify, ActualValue);
				}
			}
		}
	}

	AbilitySystemComponent->ForceReplication();
}

void FPOTAttributeSetInitter::ApplyAttributeDefaultGradient(UAbilitySystemComponent* AbilitySystemComponent, FGameplayAttribute& InAttribute, FName GroupName, float Level) const
{

	const FPOTAttributeSetDefaultsCollection* Collection = Defaults.Find(GroupName);
	const FPOTAttributeSetDefaultsCollection* ModCollection = ModDefaults.Find(GroupName);

	if (!Collection)
	{
		ABILITY_LOG(Warning, TEXT("Unable to find DefaultAttributeSet Group %s. Falling back to Defaults"), *GroupName.ToString());
		Collection = Defaults.Find(FName(TEXT("Default")));
		if (!Collection)
		{
			ABILITY_LOG(Error, TEXT("FAttributeSetInitterDiscreteLevels::InitAttributeSetDefaults Default DefaultAttributeSet not found! Skipping Initialization"));
			return;
		}
	}
	
	int32 BottomLevel = FMath::FloorToInt(Level);
	int32 TopLevel = FMath::CeilToInt(Level);

	if (!Collection->LevelData.IsValidIndex(BottomLevel - 1) || !Collection->LevelData.IsValidIndex(TopLevel - 1))
	{
		// We could eventually extrapolate values outside of the max defined levels
		ABILITY_LOG(Warning, TEXT("Attribute defaults for Level %d are not defined! Skipping"), Level);
		return;
	}

	const FPOTAttributeSetDefaults& BottomSetDefaults = Collection->LevelData[BottomLevel - 1];
	const FPOTAttributeSetDefaults& TopSetDefaults = Collection->LevelData[TopLevel - 1];

	for (const UAttributeSet* Set : AbilitySystemComponent->GetSpawnedAttributes()) // this might need to be GetSpawnedAttributes_Mutable
	{
		const FPOTAttributeDefaultValueList* DefaultDataList = BottomSetDefaults.DataMap.Find(Set->GetClass());
		const FPOTAttributeDefaultValueList* TopDefaultDataList = TopSetDefaults.DataMap.Find(Set->GetClass());

		const FPOTAttributeDefaultValueList* ModDefaultDataList = nullptr;
		const FPOTAttributeDefaultValueList* ModTopDefaultDataList = nullptr;

		if (ModCollection != nullptr && ModCollection->LevelData.IsValidIndex(BottomLevel - 1) && ModCollection->LevelData.IsValidIndex(TopLevel - 1))
		{
			const FPOTAttributeSetDefaults& ModBottomSetDefaults = ModCollection->LevelData[BottomLevel - 1];
			const FPOTAttributeSetDefaults& ModTopSetDefaults = ModCollection->LevelData[TopLevel - 1];

			ModDefaultDataList = ModBottomSetDefaults.DataMap.Find(Set->GetClass());
			ModTopDefaultDataList = ModTopSetDefaults.DataMap.Find(Set->GetClass());
		}
		
		if (DefaultDataList && TopDefaultDataList)
		{
			ABILITY_LOG(Log, TEXT("Initializing Set %s"), *Set->GetName());

			for(int32 i = 0; i < DefaultDataList->List.Num(); i++)
			{
				auto DataPairBottom = DefaultDataList->List[i];
				auto DataPairTop = TopDefaultDataList->List[i];

				if (ModDefaultDataList != nullptr && ModTopDefaultDataList != nullptr
					&& ModDefaultDataList->List.IsValidIndex(i) && ModTopDefaultDataList->List.IsValidIndex(i))
				{
					DataPairBottom = ModDefaultDataList->List[i];
					DataPairTop = ModTopDefaultDataList->List[i];
				}

				check(DataPairBottom.Property && DataPairTop.Property);

				if (DataPairBottom.Property == InAttribute.GetUProperty())
				{
					FGameplayAttribute AttributeToModify(DataPairBottom.Property);

					float ActualValue = FMath::Lerp(DataPairBottom.Value, DataPairTop.Value, FMath::Frac(Level));
					AbilitySystemComponent->SetNumericAttributeBase(AttributeToModify, ActualValue);
				}
			}
		}
	}

	AbilitySystemComponent->ForceReplication();
}
