// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Abilities/POTAbilitySystemGlobals.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTargetTypes.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffectUIData.h"
#include "Abilities/POTAttributeSetInitter.h"
#include "TitanAssetManager.h"
#include "ITypes.h"
#include "Player/IBaseCharacter.h"
#include "IGameplayStatics.h"
#include "IProjectSettings.h"
#include "Online/IPlayerState.h"
#include "IGameInstance.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "Modding/IModData.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

void UPOTAbilitySystemGlobals::InitGlobalData()
{
	Super::InitGlobalData();

	WeightRatioDamageMultiplierCurve = Cast<UCurveFloat>(WeightRatioDamageMultiplierCurveName.TryLoad());
	ensureMsgf(WeightRatioDamageMultiplierCurve != nullptr, TEXT("Ability config value WeightRatioDamageMultiplierCurveName is not a valid object name."));

	InCombatEffect = InCombatEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(InCombatEffect != nullptr, TEXT("Ability config value InCombatEffectName is not a valid class name."));

	InHomecaveEffect = InHomecaveEffectName.TryLoadClass<UGameplayEffect>();
	//ensureMsgf(InHomecaveEffect != nullptr, TEXT("Ability config value InHomecaveEffectName is not a valid class name."));

	InGroupEffect = InGroupEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(InGroupEffect != nullptr, TEXT("Ability config value InGroupEffectName is not a valid class name."));
	
	HomecaveExitBuffEffect = HomecaveExitBuffEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(HomecaveExitBuffEffect != nullptr, TEXT("Ability config value HomecaveExitBuffEffectName is not a valid class name."));

	HomecaveCampingDebuffEffect = HomecaveCampingDebuffEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(HomecaveCampingDebuffEffect != nullptr, TEXT("Ability config value HomecaveCampingDebuffEffectName is not a valid class name."));

	GroupBuffEffect = GroupBuffEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(GroupBuffEffect != nullptr, TEXT("Ability config value GroupBuffEffectName is not a valid class name."));

	RAFGroupBuffEffect = RAFGroupBuffEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(RAFGroupBuffEffect != nullptr, TEXT("Ability config value RAFGroupBuffEffectName is not a valid class name."));

	LoginEffect = LoginEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(RAFGroupBuffEffect != nullptr, TEXT("Ability config value LoginEffectName is not a valid class name."));

	GrowthRewardGameplayEffect = GrowthRewardGameplayEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(GrowthRewardGameplayEffect != nullptr, TEXT("Ability config value GrowthRewardGameplayEffectName is not a valid class name."));

	GrowthDefaultGameplayEffect = GrowthDefaultGameplayEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(GrowthDefaultGameplayEffect != nullptr, TEXT("Ability config value GrowthDefaultGameplayEffectName is not a valid class name."));

	GrowthInhibitedGameplayEffect = GrowthInhibitedGameplayEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(GrowthInhibitedGameplayEffect != nullptr, TEXT("Ability config value GrowthInhibitedGameplayEffectName is not a valid class name."));

	WaystoneInvitePendingGameplayEffect = WaystoneInvitePendingGameplayEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(WaystoneInvitePendingGameplayEffect != nullptr, TEXT("Ability config value WaystoneInvitePendingGameplayEffectName is not a valid class name."));

	WaystoneInviteChargingGameplayEffect = WaystoneInviteChargingGameplayEffectName.TryLoadClass<UGameplayEffect>();
	ensureMsgf(WaystoneInviteChargingGameplayEffect != nullptr, TEXT("Ability config value WaystoneInviteChargingGameplayEffectName is not a valid class name."));

	IAlderonDatabase::SetCustomEffectFilterFunction(&UPOTAbilitySystemGlobals::ShouldFilterEffect);
}

void UPOTAbilitySystemGlobals::ReloadAttributeDefaults()
{
	Super::ReloadAttributeDefaults();
	UpdateModAttributes();
}

FVector UPOTAbilitySystemGlobals::CalculateForceDirection(AActor* DamagedActor, AActor* Instigator,
	const FHitResult DamageHitResult, EKnockbackMode KnockbackMode) const
{
	switch (KnockbackMode)
	{
	case EKnockbackMode::KM_Precise:
		return -DamageHitResult.Normal;
	case EKnockbackMode::KM_PreciseNoZ:
		return FVector(-DamageHitResult.Normal.X, -DamageHitResult.Normal.Y, 0.f);
	case EKnockbackMode::KM_Up:
		return FVector::UpVector;
	case EKnockbackMode::KM_TraceDirection:
		return (DamageHitResult.TraceEnd - DamageHitResult.TraceStart).GetSafeNormal();
	case EKnockbackMode::KM_Down:
		return FVector(0.f, 0.f, -1.f);
	default:
		break;
	}


	if (Instigator != nullptr)
	{
		if (KnockbackMode == EKnockbackMode::KM_AttackerAway)
		{
			return(DamagedActor->GetActorLocation() - Instigator->GetActorLocation()).GetSafeNormal();
		}
		else if (KnockbackMode == EKnockbackMode::KM_AttackerForward)
		{
			return Instigator->GetActorForwardVector();
		}
		else if (KnockbackMode == EKnockbackMode::KM_AttackerForwardNoZ)
		{
			FVector FV = Instigator->GetActorForwardVector();
			return FV.GetSafeNormal2D();
		}
		else if (KnockbackMode == EKnockbackMode::KM_AttackerAwayNoZ)
		{
			FVector Dir = DamagedActor->GetActorLocation() - Instigator->GetActorLocation();
			return Dir.GetSafeNormal2D();
		}
	}

	return FVector::ZeroVector;
}

void UPOTAbilitySystemGlobals::AddGASMod(const FString& ModID)
{
	FModPak LoadedPak;
	IAlderonCommon& AlderonModule = IAlderonCommon::Get();
	
	if (AlderonModule.GetUGCInterface().GetLoadedModPak(ModID, LoadedPak))
	{
		if (!LoadedPak.bLoaded)
		{
			return;
		}

		ProcessModData(ModID, Cast<UIModData>(LoadedPak.ModData));
	}
}

void UPOTAbilitySystemGlobals::RemoveGASMod(const FString& ModID)
{
	ModCurveTables.Remove(ModID);
	UpdateModAttributes();

	if (ModAbilities.Contains(ModID))
	{
		const FModAbilityRedirect& Redirects = ModAbilities[ModID];

		if (Redirects.Redirects.Num() > 0)
		{
			UTitanAssetManager* AssetMgr = Cast<UTitanAssetManager>(UAssetManager::GetIfInitialized());
			check(AssetMgr);

			for (const auto& KVP : Redirects.Redirects)
			{
				AssetMgr->RemoveRedirectForAsset(KVP.Key);
			}
		}

		ModAbilities.Remove(ModID);
	}

	FCurveTableOverrideArray& CombatDataOverrideArray = CombatValueOverrides.FindOrAdd(ModID);
	for (const FCurveTableRowOverrideInfo& Info : CombatDataOverrideArray.OverrideInfoArray)
	{
		RestoreCurve(Info);
	}

	CombatDataOverrideArray.OverrideInfoArray.Empty();
}

void UPOTAbilitySystemGlobals::UpdateGASMods()
{
	IAlderonCommon& AlderonModule = IAlderonCommon::Get();
	TMap<FString, FModPak> LoadedMods = AlderonModule.GetUGCInterface().GetLoadedModPaks();

	for (auto& Elem : LoadedMods)
	{
		// Mod Data
		FString ModID = Elem.Key;
		FModPak ModPak = Elem.Value;

		// Don't load from mods that are not bound
		if (!ModPak.bLoaded) continue;

		ProcessModData(ModID, Cast<UIModData>(ModPak.ModData));
	}
}

void UPOTAbilitySystemGlobals::BP_TestModLoad(const FString& ModID, TSubclassOf<UIModData> ModDataClass)
{
	UIModData* ModData = NewObject<UIModData>(&Get(), ModDataClass);
	Get().TestModLoad(ModID, ModData);
}

void UPOTAbilitySystemGlobals::TestModLoad(const FString& ModID, UIModData* IModData)
{
	ProcessModData(ModID, IModData);
}

void UPOTAbilitySystemGlobals::BP_ReloadAttributesFromCSV(const FString& CSV)
{
	Get().ReloadAttributesFromCSV(CSV);
}

void UPOTAbilitySystemGlobals::ReloadAttributesFromCSV(const FString& CSV)
{
	if (!IsAbilitySystemGlobalsInitialized())
	{
		return;
	}

	FAttributeSetInitter* ASI = GetAttributeSetInitter();

	if (ASI != nullptr)
	{
		FPOTAttributeSetInitter* PASI = static_cast<FPOTAttributeSetInitter*>(ASI);
		PASI->PreloadAttributeSetDataFromCSV(CSV);

		OnAttributeSetDataReloaded.Broadcast(true, false);
	}
}

void UPOTAbilitySystemGlobals::AllocAttributeSetInitter()
{
	GlobalAttributeSetInitter = TSharedPtr<FAttributeSetInitter>(new FPOTAttributeSetInitter());
}

FGameplayEffectContext* UPOTAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FPOTGameplayEffectContext();
}

void UPOTAbilitySystemGlobals::InitGameplayCueParameters_Transform(FGameplayCueParameters& CueParameters,
	UGameplayAbility* Ability, const FTransform& DestinationTransform)
{
	if (Ability == nullptr)
	{
		return;
	}

	FGameplayAbilityActorInfo CurrentActorInfo = Ability->GetActorInfo();
	check(CurrentActorInfo.AbilitySystemComponent.IsValid());

	CueParameters.AbilityLevel = Ability->GetAbilityLevel();
	CueParameters.EffectCauser = CurrentActorInfo.AvatarActor;
	CueParameters.EffectContext = CurrentActorInfo.AbilitySystemComponent->MakeEffectContext();
	CueParameters.Instigator = CurrentActorInfo.OwnerActor;
	CueParameters.SourceObject = Ability;
	CueParameters.PhysicalMaterial = GEngine->DefaultPhysMaterial;
	CueParameters.Location = DestinationTransform.GetLocation();
	CueParameters.Normal = DestinationTransform.GetUnitAxis(EAxis::X);
}


void UPOTAbilitySystemGlobals::InitGameplayCueParameters_Transform(FGameplayCueParameters& CueParameters, 
	UAbilitySystemComponent* ASC, const FTransform& DestinationTransform)
{
	if (ASC == nullptr)
	{
		return;
	}

	CueParameters.EffectCauser = ASC->GetAvatarActor();
	CueParameters.EffectContext = ASC->MakeEffectContext();
	CueParameters.Instigator = ASC->GetOwner();
	CueParameters.SourceObject = ASC;
	CueParameters.PhysicalMaterial = GEngine->DefaultPhysMaterial;
	CueParameters.Location = DestinationTransform.GetLocation();
	CueParameters.Normal = DestinationTransform.GetUnitAxis(EAxis::X);
}

void UPOTAbilitySystemGlobals::InitGameplayCueParameters_HitResult(FGameplayCueParameters& CueParameters,
	UGameplayAbility* Ability, const FHitResult& HitResult)
{
	if (Ability == nullptr)
	{
		return;
	}

	FGameplayAbilityActorInfo CurrentActorInfo = Ability->GetActorInfo();
	check(CurrentActorInfo.AbilitySystemComponent.IsValid());

	CueParameters.AbilityLevel = Ability->GetAbilityLevel();
	CueParameters.EffectCauser = CurrentActorInfo.AvatarActor;
	CueParameters.EffectContext = CurrentActorInfo.AbilitySystemComponent->MakeEffectContext();
	CueParameters.Instigator = CurrentActorInfo.OwnerActor;
	CueParameters.SourceObject = Ability;

	CueParameters.Location = HitResult.Location;
	CueParameters.Normal = HitResult.ImpactNormal;
	CueParameters.TargetAttachComponent = HitResult.GetComponent();
	CueParameters.PhysicalMaterial = HitResult.PhysMaterial;
	CueParameters.EffectContext.AddHitResult(HitResult);
}

void UPOTAbilitySystemGlobals::InitGameplayCueParameters_HitResult(FGameplayCueParameters& CueParameters, 
	UAbilitySystemComponent* ASC, const FHitResult& HitResult)
{
	if (ASC == nullptr)
	{
		return;
	}

	CueParameters.EffectCauser = ASC->GetAvatarActor();
	CueParameters.EffectContext = ASC->MakeEffectContext();
	CueParameters.Instigator = ASC->GetOwner();
	CueParameters.SourceObject = ASC;

	CueParameters.Location = HitResult.Location;
	CueParameters.Normal = HitResult.ImpactNormal;
	CueParameters.TargetAttachComponent = HitResult.GetComponent();
	CueParameters.PhysicalMaterial = HitResult.PhysMaterial;
	CueParameters.EffectContext.AddHitResult(HitResult);
}

void UPOTAbilitySystemGlobals::InitGameplayCueParameters_Actor(FGameplayCueParameters& CueParameters, 
	UGameplayAbility* Ability, const AActor* InTargetActor)
{
	if (Ability == nullptr || InTargetActor == nullptr)
	{
		return;
	}

	FGameplayAbilityActorInfo CurrentActorInfo = Ability->GetActorInfo();
	check(CurrentActorInfo.AbilitySystemComponent.IsValid());

	CueParameters.AbilityLevel = Ability->GetAbilityLevel();
	CueParameters.EffectCauser = CurrentActorInfo.AvatarActor;
	CueParameters.EffectContext = CurrentActorInfo.AbilitySystemComponent->MakeEffectContext();
	CueParameters.Instigator = CurrentActorInfo.OwnerActor;
	CueParameters.SourceObject = Ability;

	CueParameters.Location = InTargetActor->GetActorLocation();
	CueParameters.Normal = (CurrentActorInfo.AvatarActor->GetActorLocation() - CueParameters.Location).GetSafeNormal();

	CueParameters.TargetAttachComponent = InTargetActor->GetRootComponent();
	if (UPrimitiveComponent* TargetPrimitive = Cast<UPrimitiveComponent>(CueParameters.TargetAttachComponent))
	{

		UMaterialInterface* MInt = (TargetPrimitive->GetNumMaterials() > 0) ?
			TargetPrimitive->GetMaterial(0) : nullptr;

		if (MInt != nullptr)
		{
			CueParameters.PhysicalMaterial = TWeakObjectPtr<UPhysicalMaterial>(MInt->GetPhysicalMaterial());
		}
	}

	if (!CueParameters.PhysicalMaterial.IsValid())
	{
		CueParameters.PhysicalMaterial = GEngine->DefaultPhysMaterial;
	}
}

void UPOTAbilitySystemGlobals::InitGameplayCueParameters_Actor(FGameplayCueParameters& CueParameters, 
	UAbilitySystemComponent* ASC, const AActor* InTargetActor)
{
	if (ASC == nullptr)
	{
		return;
	}

	CueParameters.EffectCauser = ASC->GetAvatarActor();
	CueParameters.EffectContext = ASC->MakeEffectContext();
	CueParameters.Instigator = ASC->GetOwner();
	CueParameters.SourceObject = ASC;

	CueParameters.Location = InTargetActor->GetActorLocation();
	if (CueParameters.EffectCauser != nullptr)
	{
		CueParameters.Normal =
			(CueParameters.EffectCauser->GetActorLocation() - CueParameters.Location).GetSafeNormal();
	}
	else
	{
		CueParameters.Normal = FVector(0,0,1);
	}

	CueParameters.TargetAttachComponent = InTargetActor->GetRootComponent();
	if (UPrimitiveComponent* TargetPrimitive = Cast<UPrimitiveComponent>(CueParameters.TargetAttachComponent))
	{

		UMaterialInterface* MInt = (TargetPrimitive->GetNumMaterials() > 0) ?
			TargetPrimitive->GetMaterial(0) : nullptr;

		if (MInt != nullptr)
		{
			CueParameters.PhysicalMaterial = TWeakObjectPtr<UPhysicalMaterial>(MInt->GetPhysicalMaterial());
		}
	}

	if (!CueParameters.PhysicalMaterial.IsValid())
	{
		CueParameters.PhysicalMaterial = GEngine->DefaultPhysMaterial;
	}
}

UPOTAbilitySystemComponent* UPOTAbilitySystemGlobals::GetAbilitySystemComponentFromActor(const AActor* Actor, bool bLookForComponent /*= false*/)
{
	return Cast<UPOTAbilitySystemComponent>(Super::GetAbilitySystemComponentFromActor(Actor, bLookForComponent));
}

class UPOTAbilitySystemComponentBase* UPOTAbilitySystemGlobals::GetAbilitySystemComponentBaseFromActor(const AActor* Actor, bool bLookForComponent /*= false*/)
{
	return Cast<UPOTAbilitySystemComponentBase>(Super::GetAbilitySystemComponentFromActor(Actor, bLookForComponent));
}

class AActor* UPOTAbilitySystemGlobals::GetInstigatorFromGameplayEffectSpec(const FGameplayEffectSpec& InSpec)
{
	const FGameplayEffectContextHandle& CHandle = InSpec.GetEffectContext();
	return CHandle.GetInstigator();
}

TArray<FActiveGameplayEffectHandle> UPOTAbilitySystemGlobals::ApplyExternalEffectContainerSpec(const FPOTGameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;

	// Iterate list of gameplay effects
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		if (SpecHandle.IsValid())
		{
			// If effect is valid, iterate list of targets and apply to all
			for (TSharedPtr<FGameplayAbilityTargetData> Data : ContainerSpec.TargetData.Data)
			{
				AllEffects.Append(Data->ApplyGameplayEffectSpec(*SpecHandle.Data.Get()));
			}
		}
	}
	
	return AllEffects;
}

bool UPOTAbilitySystemGlobals::DoesEffectContainerSpecHaveEffects(const FPOTGameplayEffectContainerSpec& ContainerSpec)
{
	return ContainerSpec.HasValidEffects();
}

bool UPOTAbilitySystemGlobals::DoesEffectContainerSpecHaveTargets(const FPOTGameplayEffectContainerSpec& ContainerSpec)
{
	return ContainerSpec.HasValidTargets();
}

FPOTGameplayEffectContainerSpec UPOTAbilitySystemGlobals::AddTargetsToEffectContainerSpec(const FPOTGameplayEffectContainerSpec& ContainerSpec, const TArray<FHitResult>& HitResults, const TArray<AActor*>& TargetActors)
{
	FPOTGameplayEffectContainerSpec NewSpec = ContainerSpec;
	NewSpec.AddTargets(HitResults, TargetActors);
	return NewSpec;
}

void UPOTAbilitySystemGlobals::SetEndPointsInTargetData(FGameplayAbilityTargetDataHandle& TargetData, 
	const FGameplayAbilityTargetingLocationInfo& EndPoint)
{
	for (TSharedPtr<FGameplayAbilityTargetData>& DataPtr : TargetData.Data)
	{
		if (DataPtr.IsValid() && DataPtr->GetScriptStruct() == FGameplayAbilityTargetData_LocationInfo::StaticStruct())
		{
			FGameplayAbilityTargetData_LocationInfo* LocData = 
				static_cast<FGameplayAbilityTargetData_LocationInfo*>(DataPtr.Get());

			LocData->TargetLocation = EndPoint;
		}
	}
}

FName UPOTAbilitySystemGlobals::GetTargetDataSourceSocketName(FGameplayAbilityTargetDataHandle& TargetData)
{
	for (TSharedPtr<FGameplayAbilityTargetData>& DataPtr : TargetData.Data)
	{
		if (DataPtr.IsValid() && DataPtr->GetScriptStruct() == FGameplayAbilityTargetData_LocationInfo::StaticStruct())
		{
			FGameplayAbilityTargetData_LocationInfo* LocData =
				static_cast<FGameplayAbilityTargetData_LocationInfo*>(DataPtr.Get());

			return LocData->SourceLocation.SourceSocketName;
		}
	}
	
	return FName();
}

float UPOTAbilitySystemGlobals::GetValueAtLevel(const FScalableFloat& ScalableFloat,
	const float Level /*= 0.f*/, const FString ContextString /*= ""*/)
{
	return ScalableFloat.GetValueAtLevel(Level, &ContextString);
}

bool UPOTAbilitySystemGlobals::IsValidAbilityHandle(UAbilitySystemComponent* ASC,
                                                    const FGameplayAbilitySpecHandle& Handle)
{
	return Handle.IsValid() && ASC->FindAbilitySpecFromHandle(Handle) != nullptr;
}

void UPOTAbilitySystemGlobals::OverrideTargetDataInGameplayEffectContainerSpec(FPOTGameplayEffectContainerSpec& ContainerSpec, FGameplayAbilityTargetDataHandle TargetData)
{
	ContainerSpec.TargetData = TargetData;
}

void UPOTAbilitySystemGlobals::OverrideTargetGameplayEffectSpecInGameplayEffectContainerSpec(FPOTGameplayEffectContainerSpec& ContainerSpec, TArray<FGameplayEffectSpecHandle> SpecHandles)
{
	ContainerSpec.TargetGameplayEffectSpecs = SpecHandles;
}

void UPOTAbilitySystemGlobals::OverrideContextHandleInEventData(FGameplayEventData& EventData, FGameplayEffectContextHandle ContextHandle)
{
	EventData.ContextHandle = ContextHandle;
}

void UPOTAbilitySystemGlobals::EffectContextOverrideEventMagnitude(FGameplayEffectContextHandle& GEContextHandle, const float EventMagnitude)
{
	if (GEContextHandle.Get())
	{
		FPOTGameplayEffectContext* Data = static_cast<FPOTGameplayEffectContext*>(GEContextHandle.Get());
		Data->SetEventMagnitude(EventMagnitude);
	}
}

void UPOTAbilitySystemGlobals::EffectContextOverrideCauser(FGameplayEffectContextHandle& GEContextHandle, AActor* NewCauser)
{
	if (FGameplayEffectContext* Data = GEContextHandle.Get())
	{
		Data->SetEffectCauser(NewCauser);
	}
}


void UPOTAbilitySystemGlobals::GameplayEffectSpecHandleOverridePeriod(FGameplayEffectSpecHandle& GESpecHandle, const float NewPeriod)
{
	if (GESpecHandle.Data.IsValid())
	{
		GESpecHandle.Data->Period = NewPeriod;
	}
}

bool UPOTAbilitySystemGlobals::GetActiveGameplayEffectInfo(const FActiveGameplayEffectHandle Handle,
	float& Duration, float& RemainingTime, int32& StackCount, FGameplayEffectContextHandle& EffectContext)
{
	if (Handle.IsValid())
	{
		if (const UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent())
		{
			EffectContext = const_cast<UAbilitySystemComponent*>(ASC)->GetEffectContextFromActiveGEHandle(Handle);
			if (const FActiveGameplayEffect* Effect = ASC->GetActiveGameplayEffect(Handle))
			{
				Duration = Effect->GetDuration();
				RemainingTime = Effect->GetTimeRemaining(ASC->GetWorld()->GetTimeSeconds());
				StackCount = Effect->ClientCachedStackCount;
				// EffectContextMagnitude = ASC->GetGameplayEffectMa
				return true;
			}
		}
	}

	return false;
}

const FActiveGameplayEffect* UPOTAbilitySystemGlobals::GetActiveGameplayEffect(const FActiveGameplayEffectHandle& Handle)
{
	if (!Handle.IsValid())
	{
		return nullptr;
	}

	const UAbilitySystemComponent* const ASC = Handle.GetOwningAbilitySystemComponent();
	if (!ASC)
	{
		return nullptr;
	}

	return ASC->GetActiveGameplayEffect(Handle);
}

TArray<FActiveGameplayEffectHandle> UPOTAbilitySystemGlobals::GetActiveGameplayEffectsByClass(const UAbilitySystemComponent* const AbilitySystemComp, const TSubclassOf<UGameplayEffect> EffectClass)
{
	if (!AbilitySystemComp)
	{
		return {};
	}

	FGameplayEffectQuery Query{};
	Query.EffectDefinition = EffectClass;
	return AbilitySystemComp->GetActiveEffects(Query);
}

const UGameplayEffectUIData* UPOTAbilitySystemGlobals::GetActiveGameplayEffectUIDataFromActiveEffect(const FActiveGameplayEffect* ActiveEffect)
{
	if (ActiveEffect && ActiveEffect->Spec.Def)
	{
		return Cast<UGameplayEffectUIData>(ActiveEffect->Spec.Def->FindComponent(UGameplayEffectUIData::StaticClass()));
	}

	return nullptr;
}

const UGameplayEffectUIData* UPOTAbilitySystemGlobals::GetActiveGameplayEffectUIData(const FActiveGameplayEffectHandle& Handle)
{
	const UAbilitySystemComponent* ASC = Handle.GetOwningAbilitySystemComponent();
	if (ASC && Handle.IsValid())
	{
		return GetActiveGameplayEffectUIDataFromActiveEffect(ASC->GetActiveGameplayEffect(Handle));
	}
	
	return nullptr;
}

bool UPOTAbilitySystemGlobals::HasGameplayEffectUIDataFromActiveEffect(const FActiveGameplayEffect* ActiveEffect)
{
	return GetActiveGameplayEffectUIDataFromActiveEffect(ActiveEffect) != nullptr;
}


bool UPOTAbilitySystemGlobals::HasGameplayEffectUIData(const FActiveGameplayEffectHandle& Handle)
{
	return GetActiveGameplayEffectUIData(Handle) != nullptr;
}

float UPOTAbilitySystemGlobals::EffectContextGetEventMagnitude(FGameplayEffectContextHandle& GEContextHandle)
{
	if (GEContextHandle.Get())
	{
		FPOTGameplayEffectContext* Data = static_cast<FPOTGameplayEffectContext*>(GEContextHandle.Get());
		return Data->GetEventMagnitude();
	}
	
	return 0.0f;
}

void UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(class AActor* InActor, const FGameplayAttribute& Attribute, 
	const float Magnitude, EGameplayModOp::Type ModOp /*= EGameplayModOp::Additive*/, 
	EDamageType DamageTypeOverride /*= EDamageType::DT_GENERIC*/)
{
	if (InActor == nullptr)
	{
		return;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(InActor);
	if (ASC == nullptr)
	{
		return;
	}

	// Create a dynamic instant Gameplay Effect to give the bounties
	UGameplayEffect* GeModifyAttribute = NewObject<UGameplayEffect>(GetTransientPackage(), FName(TEXT("ModifyAttribute")));
	GeModifyAttribute->DurationPolicy = EGameplayEffectDurationType::Instant;

	int32 Idx = GeModifyAttribute->Modifiers.Num();
	GeModifyAttribute->Modifiers.SetNum(Idx + 1);

	FGameplayModifierInfo& AttributeRestoreMod = GeModifyAttribute->Modifiers[Idx];
	AttributeRestoreMod.ModifierMagnitude = FScalableFloat(Magnitude);
	AttributeRestoreMod.ModifierOp = ModOp;
	AttributeRestoreMod.Attribute = Attribute;

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();

	if (/*ContextTag.IsValid() || */DamageTypeOverride != EDamageType::DT_GENERIC)
	{
		FPOTGameplayEffectContext* POTEffectContext = StaticCast<FPOTGameplayEffectContext*>(ContextHandle.Get());
		/*if (ContextTag.IsValid())
		{
			POTEffectContext->SetContextTag(ContextTag);
		}*/

		if(DamageTypeOverride != EDamageType::DT_GENERIC)
		{
			POTEffectContext->SetDamageType(DamageTypeOverride);
		}
	}

	ASC->ApplyGameplayEffectToSelf(GeModifyAttribute, 1.0f, ContextHandle);
}

void UPOTAbilitySystemGlobals::ClearGrowth(AActor* Actor)
{
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC || !UIGameplayStatics::IsGrowthEnabled(Actor)) return;

	const TSubclassOf<UGameplayEffect> GrowthGE = UIProjectSettings::GetGrowthRewardGameplayEffect();
	FGameplayEffectQuery Query;
	Query.EffectDefinition = GrowthGE;

	ASC->RemoveActiveEffects(Query);
}

FActiveGameplayEffectHandle UPOTAbilitySystemGlobals::RewardGrowth(AActor* Actor, float TotalGrowth, float DurationSeconds)
{
	if (TotalGrowth == 0.f || DurationSeconds == 0.f || Actor == nullptr) { return FActiveGameplayEffectHandle(); }

	UWorld* World = Actor->GetWorld();
	if(World == nullptr) return FActiveGameplayEffectHandle();

	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor);
	if (!ASC || !UIGameplayStatics::IsGrowthEnabled(Actor)) { return FActiveGameplayEffectHandle(); }

	// If already at max growth, don't reward more growth
	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(Actor);
	if (IBaseCharacter)
	{
		if (IBaseCharacter->GetGrowthPercent() >= 1.0f)
		{
			return FActiveGameplayEffectHandle();
		}
	}

	// It would be convenient to use a dynamic GE here, however, as far as I know you cannot make dynamic GE's that have durations
	const TSubclassOf<UGameplayEffect> GrowthGE = UIProjectSettings::GetGrowthRewardGameplayEffect();
	FGameplayEffectSpecHandle GrowthSpec = ASC->MakeOutgoingSpec(GrowthGE, 1, ASC->MakeEffectContext());

	FGameplayEffectQuery Query;
	Query.EffectDefinition = GrowthGE;

	float WorldTime = World->GetTimeSeconds();

	TArray<FActiveGameplayEffectHandle> ActiveGrowthEffects = ASC->GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : ActiveGrowthEffects)
	{
		if (const FActiveGameplayEffect* ActiveEffect = ASC->GetActiveGameplayEffect(Handle))
		{
			float RemainingTime = ActiveEffect->GetTimeRemaining(WorldTime);
			float GrowthPerSecond = ActiveEffect->Spec.GetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_SetByCallerGrowthPerSecond));

			TotalGrowth += (GrowthPerSecond * RemainingTime);
			DurationSeconds += RemainingTime;

			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	GrowthSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_SetByCallerDuration), DurationSeconds);
	GrowthSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_SetByCallerGrowthPerSecond), TotalGrowth/DurationSeconds);
	return ASC->ApplyGameplayEffectSpecToSelf(*GrowthSpec.Data);
}

FActiveGameplayEffectHandle UPOTAbilitySystemGlobals::RewardGrowthConstantRate(AActor* Actor, float TotalGrowth, float GrowthPerSecond)
{
	if (TotalGrowth == 0.f || GrowthPerSecond == 0.f) { return FActiveGameplayEffectHandle(); }

	return RewardGrowth(Actor, TotalGrowth, TotalGrowth/GrowthPerSecond);
}

FActiveGameplayEffectHandle UPOTAbilitySystemGlobals::RewardWellRestedBonus(AActor* Actor, float WellRestedStart, float WellRestedEnd)
{
	if (WellRestedStart == 0.f || WellRestedEnd == 0.f || WellRestedStart == WellRestedEnd) { return FActiveGameplayEffectHandle(); }

	return FActiveGameplayEffectHandle();
	//return RewardGrowth(Actor, TotalGrowth, TotalGrowth / GrowthPerSecond);
}

void UPOTAbilitySystemGlobals::ReplaceGEBlueprintRowNameText(UObject* GEObj, FString TextToSearch, FString ReplaceText, UCurveTable* NewCurve)
{
	if (UBlueprint* GEBP = Cast<UBlueprint>(GEObj))
	{
		if (UClass* Class = GEBP->GeneratedClass)
		{
			return ReplaceGERowNameText(Class->GetDefaultObject<UGameplayEffect>(), TextToSearch, ReplaceText, NewCurve);
		}
	}
}

void UPOTAbilitySystemGlobals::ReplaceGERowNameText(UGameplayEffect* GE, FString TextToSearch, FString ReplaceText, UCurveTable* NewCurve)
{

#if WITH_EDITOR
	/*if (GE == nullptr || TextToSearch.IsEmpty() || ReplaceText.IsEmpty() || NewCurve == nullptr)
	{
		return;
	}

	PropertyAccessUtil::
	const FProperty* ObjectProp = PropertyAccessUtil::FindPropertyByName(PropertyName, GE->GetClass());
	if (ObjectProp)
	{
		ObjectProp->
		P_NATIVE_BEGIN;
		bResult = Generic_SetEditorProperty(Object, ObjectProp, ValuePtr, ValueProp, ChangeNotifyMode);
		P_NATIVE_END;
	}

	TSharedPtr<IPropertyHandle> Handle = PropertyEditorHelpers::GetPropertyHandle(PropertyNode.ToSharedRef(), OwnerTreeNode.Pin()->GetDetailsView()->GetNotifyHook(), OwnerTreeNode.Pin()->GetDetailsView()->GetPropertyUtilities());

	FString Value;
	if (Handle->GetValueAsFormattedString(Value, PPF_Copy) == FPropertyAccess::Success)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Value);
	}*/

#endif
}


void UPOTAbilitySystemGlobals::SetDamageTypeInEventData(FGameplayEventData& EventData, EDamageType NewDamageType)
{
	if (FGameplayEffectContext* Context = EventData.ContextHandle.Get())
	{
		FPOTGameplayEffectContext* POTEffectContext = StaticCast<FPOTGameplayEffectContext*>(Context);
		POTEffectContext->SetDamageType(NewDamageType);
	}
}

// Can be deleted after merging to unstable. See: https://github.com/Alderon-Games/pot-development/issues/4063 for more information.
// TArray<FGameplayModifierInfo> UPOTAbilitySystemGlobals::GetModifiersWithReplacedRowNames(TSubclassOf<UGameplayEffect> GEClass, FString TextToSearch, FString ReplaceText)
// {
// 	if (GEClass == nullptr || TextToSearch.IsEmpty() || ReplaceText.IsEmpty())
// 	{
// 		return {};
// 	}
//
// 	if (UGameplayEffect* GE = GEClass->GetDefaultObject<UGameplayEffect>())
// 	{
// 		TArray<FGameplayModifierInfo> RetInfo = GE->Modifiers;
//
// 		for (FGameplayModifierInfo& ModInfo : RetInfo)
// 		{
// 			FString RowName = ModInfo.ModifierMagnitude.Curve.RowName.ToString();
// 			RowName.ReplaceInline(*TextToSearch, *ReplaceText);
// 			ModInfo.Magnitude.Curve.RowName = FName(*RowName);
// 		}
//
// 		return RetInfo;
// 	}
//
// 	return {};
// }

void UPOTAbilitySystemGlobals::SetScalableFloatInModifier(FGameplayModifierInfo& ModInfo, const FScalableFloat& Float)
{
	FStructProperty* ScalableFloatProp = CastField<FStructProperty>(FGameplayEffectModifierMagnitude::StaticStruct()->FindPropertyByName(TEXT("ScalableFloatMagnitude")));
	if (FScalableFloat* ScalableFloatPropValue = ScalableFloatProp->ContainerPtrToValuePtr<FScalableFloat>(&(ModInfo.ModifierMagnitude)))
	{
		ScalableFloatPropValue->Curve = Float.Curve;
		ScalableFloatPropValue->Value = Float.Value;
	}	
}

FScalableFloat UPOTAbilitySystemGlobals::GetScalableFloatInModifier(const FGameplayModifierInfo& ModInfo)
{
	FStructProperty* ScalableFloatProp = CastField<FStructProperty>(FGameplayEffectModifierMagnitude::StaticStruct()->FindPropertyByName(TEXT("ScalableFloatMagnitude")));
	if (const FScalableFloat* ScalableFloatPropValue = ScalableFloatProp->ContainerPtrToValuePtr<FScalableFloat>(&(ModInfo.ModifierMagnitude)))
	{
		return *ScalableFloatPropValue;
	}
	
	return FScalableFloat();
}


UGameplayEffect* UPOTAbilitySystemGlobals::GetGameplayEffectDefinitionCDO(TSubclassOf<UGameplayEffect> GEClass)
{
	if (GEClass != nullptr)
	{
		return GEClass->GetDefaultObject<UGameplayEffect>();
	}

	return nullptr;
}

void UPOTAbilitySystemGlobals::DebugUpdateCurve(const FName RowName, const UCurveFloat* NewCurve)
{
	if (NewCurve == nullptr || RowName == NAME_None)
	{
		return;
	}

	FCurveTableOverrideArray& CombatDataOverrideArray = Get().CombatValueOverrides.FindOrAdd("Debug");
	
	FCurveTableRowOverrideInfo OverrideInfo;
	if (Get().UpdateCurve(RowName, &NewCurve->FloatCurve, OverrideInfo))
	{
		CombatDataOverrideArray.OverrideInfoArray.Add(OverrideInfo);
	}
}

void UPOTAbilitySystemGlobals::DebugRestoreCurve()
{
	FCurveTableOverrideArray& CombatDataOverrideArray = Get().CombatValueOverrides.FindOrAdd("Debug");
	for (const FCurveTableRowOverrideInfo& Info : CombatDataOverrideArray.OverrideInfoArray)
	{
		Get().RestoreCurve(Info);
	}

	CombatDataOverrideArray.OverrideInfoArray.Empty();
}

void UPOTAbilitySystemGlobals::ApplyServerSettingCurves(const TArray<FCurveOverrideData>& Data)
{
	//UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemGlobals::ApplyServerSettingCurves"));
	RestoreServerSettingCurves();
	FCurveTableOverrideArray& CombatDataOverrideArray = Get().CombatValueOverrides.FindOrAdd("ServerSettings");
	for (const FCurveOverrideData& CurveData : Data)
	{
		if (!CurveData.Values.IsValidIndex(0))
		{
			UE_LOG(TitansLog, Error, TEXT("UPOTAbilitySystemGlobals::ApplyServerSettingCurves: Curve: %s has invalid values skipping"), *CurveData.CurveName.ToString());
			continue;
		}

		FCurveTableRowOverrideInfo OverrideInfo;
		if (UpdateCurve(CurveData.CurveName, CurveData.Values, OverrideInfo))
		{
			UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemGlobals::ApplyServerSettingCurves: UpdateCurve Success %s - %f"), *CurveData.CurveName.ToString(), CurveData.Values[0]);
			CombatDataOverrideArray.OverrideInfoArray.Add(OverrideInfo);
		}
		else
		{
			UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemGlobals::ApplyServerSettingCurves: UpdateCurve FAILED %s, loading curve tables with asset registry and retrying"), *CurveData.CurveName.ToString());
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			TArray<FAssetData> AssetData;
			AssetRegistryModule.Get().GetAssetsByClass(UCurveTable::StaticClass()->GetClassPathName(), AssetData);
			bool bFoundAsset = false;
			for (FAssetData& Asset : AssetData)
			{
				UCurveTable* CurveTable = Cast<UCurveTable>(Asset.GetAsset());
				if (!CurveTable)
				{
					continue;
				}
				if (CurveTable->FindCurve(CurveData.CurveName, "UPOTAbilitySystemGlobals::UpdateCurve", false))
				{
					if (UpdateCurve(CurveData.CurveName, CurveData.Values, OverrideInfo))
					{
						UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemGlobals::ApplyServerSettingCurves: UpdateCurve Success %s - %f"), *CurveData.CurveName.ToString(), CurveData.Values[0]);
						CombatDataOverrideArray.OverrideInfoArray.Add(OverrideInfo);
					}
					bFoundAsset = true;
					break;
				}
			}

			if (!bFoundAsset)
			{
				UE_LOG(TitansLog, Error, TEXT("UPOTAbilitySystemGlobals::ApplyServerSettingCurves: UpdateCurve FAILED %s - %f"), *CurveData.CurveName.ToString(), CurveData.Values[0]);
			}
		}
	}

	ReloadAttributeDefaults();
}

void UPOTAbilitySystemGlobals::RestoreServerSettingCurves()
{
	//UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemGlobals::RestoreServerSettingCurves"));
	FCurveTableOverrideArray& CombatDataOverrideArray = CombatValueOverrides.FindOrAdd("ServerSettings");
	for (const FCurveTableRowOverrideInfo& Info : CombatDataOverrideArray.OverrideInfoArray)
	{
		RestoreCurve(Info);
	}

	ReloadAttributeDefaults();

	CombatDataOverrideArray.OverrideInfoArray.Empty();
}

void UPOTAbilitySystemGlobals::UpdateModAttributes()
{
	FAttributeSetInitter* ASI = GetAttributeSetInitter();
	if (ASI != nullptr)
	{
		FPOTAttributeSetInitter* PASI = static_cast<FPOTAttributeSetInitter*>(ASI);

		TArray<UCurveTable*> ValueArray;
		ModCurveTables.GenerateValueArray(ValueArray);

		if (ValueArray.Num() > 0)
		{
			PASI->PreloadModAttributeSetData(ValueArray);
		}
	}
}

bool UPOTAbilitySystemGlobals::UpdateCurve(const FName RowName, const FRealCurve* NewCurve, FCurveTableRowOverrideInfo& OutOverrideInfo)
{
	//UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemGlobals::UpdateCurve"));
	if (RowName == NAME_None || NewCurve == nullptr)
	{
		return false;
	}

	TArray<UCurveTable>	CurveTableList;
	for (TObjectIterator<UCurveTable> TableIt; TableIt; ++TableIt)
	{
		if (UCurveTable* Table = *TableIt)
		{
			if (FRealCurve* TableCurve = Table->FindCurve(RowName, "UPOTAbilitySystemGlobals::UpdateCurve", false))
			{
				if (TableCurve == NewCurve)
				{
					continue;
				}
				OutOverrideInfo.Table = Table;
				OutOverrideInfo.RowName = RowName;
				OutOverrideInfo.OriginalCurve = (FRealCurve*)TableCurve->Duplicate();

				TableCurve->Reset();

				for (auto It = NewCurve->GetKeyHandleIterator(); It; ++It)
				{
					const FKeyHandle& Handle = *It;
					TPair<float, float> TimeValue = NewCurve->GetKeyTimeValuePair(Handle);

					FKeyHandle NewHandle = TableCurve->AddKey(TimeValue.Key, TimeValue.Value);

					ERichCurveInterpMode InterpMode = NewCurve->GetKeyInterpMode(Handle);
					if (InterpMode != RCIM_Cubic)
					{
						TableCurve->SetKeyInterpMode(NewHandle, InterpMode);
					}
				}

				return true;
			}
		}
	}

	return false;
}

bool UPOTAbilitySystemGlobals::UpdateCurve(const FName RowName, const TArray<float>& Values, FCurveTableRowOverrideInfo& OutOverrideInfo)
{
	//UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemGlobals::UpdateCurve"));
	if (RowName == NAME_None || Values.Num() == 0)
	{
		return false;
	}

	TArray<UCurveTable>	CurveTableList;
	for (TObjectIterator<UCurveTable> TableIt; TableIt; ++TableIt)
	{
		if (UCurveTable* Table = *TableIt)
		{
			if (FRealCurve* TableCurve = Table->FindCurve(RowName, "UPOTAbilitySystemGlobals::UpdateCurve", false))
			{
				OutOverrideInfo.Table = Table;
				OutOverrideInfo.RowName = RowName;
				OutOverrideInfo.OriginalCurve = (FRealCurve*)TableCurve->Duplicate();

				TableCurve->Reset();

				for (int32 i = 0; i < Values.Num(); i++)
				{
					FKeyHandle NewHandle = TableCurve->AddKey(i + 1.f, Values[i]);
					TableCurve->SetKeyInterpMode(NewHandle, ERichCurveInterpMode::RCIM_Linear);
				}
				
				return true;
			}
		}
	}

	return false;
}

void UPOTAbilitySystemGlobals::RestoreCurve(const FCurveTableRowOverrideInfo& OverrideInfo)
{
	//UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemGlobals::RestoreCurve"));
	if (OverrideInfo.Table == nullptr)
	{
		return;
	}

	if (FRealCurve* TableCurve = OverrideInfo.Table->FindCurve(OverrideInfo.RowName, "UPOTAbilitySystemGlobals::RestoreCurve", false))
	{
		TableCurve->Reset();

		for (auto It = OverrideInfo.OriginalCurve->GetKeyHandleIterator(); It; ++It)
		{
			const FKeyHandle& Handle = *It;
			TPair<float, float> TimeValue = OverrideInfo.OriginalCurve->GetKeyTimeValuePair(Handle);

			FKeyHandle NewHandle = TableCurve->AddKey(TimeValue.Key, TimeValue.Value);
			TableCurve->SetKeyInterpMode(NewHandle, OverrideInfo.OriginalCurve->GetKeyInterpMode(Handle));
		}

	}
}

bool UPOTAbilitySystemGlobals::ShouldFilterEffect(const UAbilitySystemComponent* ASC, const TSubclassOf<UGameplayEffect> EffectClass)
{
	if (ASC == nullptr || EffectClass == nullptr)
	{
		return false;
	}
	
	if (const UPOTAbilitySystemComponent* POTASC = Cast<UPOTAbilitySystemComponent>(ASC))
	{
		if (POTASC->MeshEffects.Contains(EffectClass) ||
			POTASC->InitialGameplayEffects.Contains(EffectClass) ||
			POTASC->StateEffects.Resting == EffectClass ||
			POTASC->StateEffects.Sleeping == EffectClass ||
			POTASC->StateEffects.Standing == EffectClass ||
			POTASC->StateEffects.Walking == EffectClass ||
			POTASC->StateEffects.Trotting == EffectClass ||
			POTASC->StateEffects.Sprinting == EffectClass ||
			POTASC->StateEffects.Swimming == EffectClass ||
			POTASC->StateEffects.FastSwimming == EffectClass ||
			POTASC->StateEffects.Diving == EffectClass ||
			POTASC->StateEffects.FastDiving == EffectClass ||
			POTASC->StateEffects.Crouching == EffectClass ||
			POTASC->StateEffects.CrouchWalking == EffectClass ||
			POTASC->StateEffects.Jumping == EffectClass)
		{
			return true;
		}
	}

	return false;
}

void UPOTAbilitySystemGlobals::ProcessModData(const FString& ModID, class UIModData* IModData)
{
	if (IModData == nullptr || !IModData->IsValidLowLevel())
	{
		UE_LOG(TitansCharacter, Error, TEXT("IGameInstance: Mod ID %s Failed to load attribute data due."), *ModID);
		return;
	}

	for (const FSoftObjectPath& AttribDefaultTableName : IModData->OverrideAttributeTables)
	{
		if (AttribDefaultTableName.IsValid())
		{
			ModCurveTables.FindOrAdd(ModID) = Cast<UCurveTable>(AttribDefaultTableName.TryLoad());
		}
	}

	UpdateModAttributes();

	if (IModData->OverrideAbilities.Num() > 0)
	{
		FModAbilityRedirect& Redirects = ModAbilities.FindOrAdd(ModID);
		Redirects = FModAbilityRedirect(IModData->OverrideAbilities);

		UTitanAssetManager* AssetMgr = Cast<UTitanAssetManager>(UAssetManager::GetIfInitialized());
		check(AssetMgr);

		for (auto& OverrideElem : IModData->OverrideAbilities)
		{
			AssetMgr->AddRedirectForAsset(OverrideElem.Key, OverrideElem.Value);
		}
	}

	FCurveTableOverrideArray& CombatDataOverrideArray = CombatValueOverrides.FindOrAdd(ModID);
	for (const FSoftObjectPath& CombatTableName : IModData->OverrideCombatValueCurves)
	{
		if (CombatTableName.IsValid())
		{
			if (UCurveTable* Table = Cast<UCurveTable>(CombatTableName.TryLoad()))
			{
				const TMap<FName, FRealCurve*>& Curves = Table->GetRowMap();
				for (const auto& KVP : Curves)
				{
					FCurveTableRowOverrideInfo OverrideInfo;
					if (UpdateCurve(KVP.Key, KVP.Value, OverrideInfo))
					{
						CombatDataOverrideArray.OverrideInfoArray.Add(OverrideInfo);
					}
				}
			}
		}
	}
}

UCurveTable* UPOTAbilitySystemGlobals::CastToCurveTable(UObject* Object)
{
	return Cast<UCurveTable>(Object);
}
