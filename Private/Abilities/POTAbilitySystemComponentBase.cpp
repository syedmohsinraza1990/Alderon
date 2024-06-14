// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Abilities/POTAbilitySystemComponentBase.h"
#include "Abilities/POTAbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "GameFramework/Character.h"
#include "Abilities/CoreAttributeSet.h"
#include "GameplayEffect.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/POTAttributeSetInitter.h"
#include "AttributeSet.h"
#include "IGameplayStatics.h"
#include "Components/ICharacterMovementComponent.h"
#include "Online/IGameState.h"
#include "World/IGameSingleton.h"
#include "Engine/StreamableManager.h"
#include "Player/IBaseCharacter.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "Components/CapsuleComponent.h"
#include "IProjectSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Abilities/POTGameplayEffect.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "UI/StatusEffectEntry.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

#define GROWTH_IMPEDIMENT_CHECK_INTERVAL 1.f

UPOTAbilitySystemComponentBase::UPOTAbilitySystemComponentBase()
	: bInitialized(false)
	, bForceDeflectOnInstigator(true)
	, bAttributesLoaded(false)
{
	AttributeSets.AddUnique(UCoreAttributeSet::StaticClass());

	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void UPOTAbilitySystemComponentBase::OnRegister()
{
	Super::OnRegister();

	InitAbilityActorInfo(GetOwner(), GetOwner());
	for (TSubclassOf<UAttributeSet>& Set : AttributeSets)
	{
		InitStats(Set, nullptr);
	}

	OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &UPOTAbilitySystemComponentBase::OnGameplayEffectAppliedToSelf);
}



void UPOTAbilitySystemComponentBase::BeginPlay()
{
	Super::BeginPlay();

	UPOTAbilitySystemGlobals& ASG = UPOTAbilitySystemGlobals::Get();
	ASG.OnAttributeSetDataReloaded.AddUniqueDynamic(this, &UPOTAbilitySystemComponentBase::OnAttributeSetDataReloaded);

	// Add night/day tags before applying initial gameplay effects so night/day effects arent immediately inhibited
	if (const AIGameState* const GameState = UIGameplayStatics::GetIGameState(this))
	{
		NotifyTimeOfDay(GameState->GetGlobalTimeOfDay());
	}
}

void UPOTAbilitySystemComponentBase::OnGiveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	Super::OnGiveAbility(AbilitySpec);

	if (!AbilitySpec.Ability)
	{
		return;
	}

	const UPOTGameplayAbility* GameplayAbility = Cast<UPOTGameplayAbility>(AbilitySpec.Ability);
	if (!ensureAlways(GameplayAbility) || !GameplayAbility->bActivateOnGranted)
	{
		return;
	}

	if (!TryActivateAbility(AbilitySpec.Handle))
	{
		UE_LOG(LogTemp, Warning, TEXT("UPOTAbilitySystemComponentBase::OnGiveAbility: Ability with bActivateOnGranted failed activation! %s"), *GameplayAbility->GetName());
	}
}

void UPOTAbilitySystemComponentBase::InitializeAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor, const bool bPreviewOnly)
{
	if (GetOwner()->HasAuthority())
	{
		InitOwnerTeamAgentInterface(InAvatarActor);

		InitAttributes(true);

		for (TSubclassOf<UGameplayEffect> GEClass : InitialGameplayEffects)
		{
			if (GEClass != nullptr)
			{
				FGameplayEffectSpecHandle GESpecHandle = MakeOutgoingSpec(GEClass, 1.f, MakeEffectContext());
				ApplyGameplayEffectSpecToSelf(*GESpecHandle.Data);
			}
		}

		for (TSubclassOf<UGameplayAbility> GAClass : InitialGameplayAbilities)
		{
			if (GAClass != nullptr)
			{
				GiveAbility(FGameplayAbilitySpec(GAClass, 1, INDEX_NONE, GetOwner()));
			}
		}

		ActiveGameplayEffects.OnActiveGameplayEffectRemovedDelegate.AddUObject(this, &UPOTAbilitySystemComponentBase::OnActiveEffectRemoved);
	}

	if (bPreviewOnly)
	{
		bAttributesLoaded = false;
	}
	else
	{
		bInitialized = true;
	}

	InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

void UPOTAbilitySystemComponentBase::InitOwnerTeamAgentInterface(AActor* InOwnerActor)
{
	ACharacter* OwnerChar = Cast<ACharacter>(InOwnerActor);
	if (OwnerChar != nullptr)
	{
		OwnerTeamAgentInterface = TScriptInterface<IGenericTeamAgentInterface>(OwnerChar->GetController());
	}
	
	if (OwnerTeamAgentInterface == nullptr)
	{
		OwnerTeamAgentInterface = TScriptInterface<IGenericTeamAgentInterface>(InOwnerActor);
	}
}

void UPOTAbilitySystemComponentBase::InitAttributes(bool bInitialInit /*= true*/)
{
	// Nick 1.22.21: May want to remove this check, but not sure if this would cause issues
	// Pre-growth it wasn't necessary to reload attributes, however with growth we do this frequently

	//Damir 3.2.21: Removed the checks as runtime reloads from downloaded CSV files will necesitate reloads
	/*if (bAttributesLoaded && !UIGameplayStatics::IsGrowthEnabled(this))
	{
		return;
	}*/

	if (bInitialInit && bAttributesLoaded) return;


	UAbilitySystemGlobals* ASG = IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals();
	if (ASG != nullptr && ASG->IsAbilitySystemGlobalsInitialized())
	{
		FAttributeSetInitter* ASI = ASG->GetAttributeSetInitter();

		if (ASI != nullptr)
		{
			FName AttributeName = GetAttributeName();
			FPOTAttributeSetInitter* PASI = static_cast<FPOTAttributeSetInitter*>(ASI);

			PASI->InitAttributeSetDefaultsGradient(this,
				AttributeName,
				GetLevelFloat(), bInitialInit);

			/*else
			{
				ASI->InitAttributeSetDefaults(this,
					AttributeName,
					GetLevel(), bInitialInit);
			}*/

// 			SetNumericAttributeBase(UCoreAttributeSet::GetMovementSpeedMultiplierAttribute(), 1.f);
		}
	}


	bAttributesLoaded = true;
}

bool UPOTAbilitySystemComponentBase::IsAlive() const
{
	return GetNumericAttributeChecked(UCoreAttributeSet::GetHealthAttribute()) > 0.f;
}


ETeamAttitude::Type GetTeamAttitudeBetween(TWeakInterfacePtr<IGenericTeamAgentInterface> OwnerTeamAgentInterface, AActor* Target)
{
	return OwnerTeamAgentInterface.IsValid() && Target != nullptr ?
		OwnerTeamAgentInterface->GetTeamAttitudeTowards(*Target) : ETeamAttitude::Neutral;
}

bool UPOTAbilitySystemComponentBase::IsHostile(AActor* Target)
{
	if (OwnerTeamAgentInterface.GetObject() == nullptr)
	{
		InitOwnerTeamAgentInterface(GetOwner());
		//UE_LOG(LogTemp, Error, TEXT("OwnerTeamAgentInterface is null!"));
		if (OwnerTeamAgentInterface.GetObject() == nullptr)
		{
			return false;
		}
	}

	//UE_LOG(LogTemp, Log, TEXT("IsHostile %s and %s"), *OwnerTeamAgentInterface.GetObject()->GetName(), (Target == nullptr ? TEXT("NULL") : *Target->GetName()));
	return GetTeamAttitudeBetween(OwnerTeamAgentInterface.GetObject(), Target) == ETeamAttitude::Hostile;
}

bool UPOTAbilitySystemComponentBase::IsNeutral(AActor* Target)
{
	if (OwnerTeamAgentInterface.GetObject() == nullptr)
	{
		InitOwnerTeamAgentInterface(GetOwner());
		//UE_LOG(LogTemp, Error, TEXT("OwnerTeamAgentInterface is null!"));
		if (OwnerTeamAgentInterface.GetObject() == nullptr)
		{
			return false;
		}
	}

	return GetTeamAttitudeBetween(OwnerTeamAgentInterface.GetObject(), Target) == ETeamAttitude::Neutral;
}

bool UPOTAbilitySystemComponentBase::IsFriendly(AActor* Target)
{
	if (OwnerTeamAgentInterface.GetObject() == nullptr)
	{
		InitOwnerTeamAgentInterface(GetOwner());
		//UE_LOG(LogTemp, Error, TEXT("OwnerTeamAgentInterface is null!"));
		if (OwnerTeamAgentInterface.GetObject() == nullptr)
		{
			return false;
		}
	}

	return GetTeamAttitudeBetween(OwnerTeamAgentInterface.GetObject(), Target) == ETeamAttitude::Friendly;
}

void UPOTAbilitySystemComponentBase::PostDeath()
{
	SetComponentTickEnabled(false);
	AddLooseGameplayTag(UPOTAbilitySystemGlobals::Get().DeadTag);
}

FName UPOTAbilitySystemComponentBase::GetAttributeName() const
{
	if (NameOverride != NAME_None)
	{
		return NameOverride;
	}
	else
	{
		return GetOwner()->GetFName();
	}
	
}


int32 UPOTAbilitySystemComponentBase::GetLevel() const
{
	return 1;
}

float UPOTAbilitySystemComponentBase::GetLevelFloat() const
{
	return 1.f;
}

void UPOTAbilitySystemComponentBase::ExecuteGameplayCueAtHit(const FGameplayTag GameplayCue, const FHitResult& HitResult)
{
	FGameplayCueParameters GCParams;
	UPOTAbilitySystemGlobals& WASG = UPOTAbilitySystemGlobals::Get();

	WASG.InitGameplayCueParameters_HitResult(GCParams, this, HitResult);
	GCParams.OriginalTag = GameplayCue;
	ExecuteGameplayCue(GameplayCue, GCParams);
}

void UPOTAbilitySystemComponentBase::ExecuteGameplayCueAtTransform(const FGameplayTag GameplayCue, const FTransform& CueTransform)
{
	FGameplayCueParameters GCParams;
	UPOTAbilitySystemGlobals& WASG = UPOTAbilitySystemGlobals::Get();


	WASG.InitGameplayCueParameters_Transform(GCParams, this, CueTransform);
	GCParams.OriginalTag = GameplayCue;
	ExecuteGameplayCue(GameplayCue, GCParams);
}

void UPOTAbilitySystemComponentBase::ApplyEffectContainerSpec(const FPOTGameplayEffectContainerSpec& ContainerSpec)
{

	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{

		FGameplayEffectSpec	SpecToApply(*SpecHandle.Data.Get());
		FGameplayEffectContextHandle EffectContext = SpecToApply.GetContext().Duplicate();
		SpecToApply.SetContext(EffectContext);

		TWeakObjectPtr<AActor> ThisActorPtr(GetOwner());
		EffectContext.AddActors(TArray<TWeakObjectPtr<AActor>>{ThisActorPtr});

		if (UAbilitySystemComponent* ASC = EffectContext.GetInstigatorAbilitySystemComponent())
		{
			if (ASC->GetOwner() != nullptr && ASC->GetOwner()->IsValidLowLevel())
			{
				ASC->ApplyGameplayEffectSpecToTarget(SpecToApply, this, GetPredictionKeyForNewAction());
			}
		}
	}	
}

float UPOTAbilitySystemComponentBase::GetCooldownTimeRemaining(const TSubclassOf<UGameplayAbility>& AbilityClass) const
{
	if (!AbilityClass.Get()) return 0.0f;

	if (UGameplayAbility* GADefault = AbilityClass->GetDefaultObject<UGameplayAbility>())
	{
		//FGameplayAbilityActorInfo ActorInfo;
		//ActorInfo.AbilitySystemComponent = this;
		//ActorInfo.InitFromActor(GetOwner(), GetOwner(), this);
		//return GADefault->GetCooldownTimeRemaining(&ActorInfo);

		const FGameplayTagContainer* CooldownTags = GADefault->GetCooldownTags();
		if (CooldownTags && CooldownTags->Num() > 0)
		{
			FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
			float CurrentTime = GetWorld()->GetTimeSeconds();

			TArray< float > Durations;
			for (const FActiveGameplayEffect& ActiveCooldownEffect : CurrentCooldownEffects)
			{
				if (!ActiveCooldownEffect.Spec.Def) continue;
				if (CooldownTags->HasAny(ActiveCooldownEffect.Spec.Def.Get()->GetGrantedTags()))
				{					
					Durations.Add(ActiveCooldownEffect.GetTimeRemaining(CurrentTime));
				}
			}

			FTimerManager& TimerManager = GetWorld()->GetTimerManager();

			for (TPair<const UGameplayEffect*, FTimerHandle> Pair : CurrentCooldownDelays)
			{
				if (!Pair.Key) continue;
				if (CooldownTags->HasAny(Pair.Key->GetGrantedTags()))
				{
					Durations.Add(TimerManager.GetTimerRemaining(Pair.Value));
				}
			}

			//TArray< float > Durations = GetActiveEffectsTimeRemaining(Query);
			if (Durations.Num() > 0)
			{
				Durations.Sort();
				return Durations[Durations.Num() - 1];
			}
		}
	}

	return 0.f;
}

bool UPOTAbilitySystemComponentBase::CanActivateAbility(const TSubclassOf<UGameplayAbility>& InAbility) const
{
	const UGameplayAbility* const InAbilityCDO = InAbility.GetDefaultObject();

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability == InAbilityCDO)
		{
			// If it's instance once the instanced ability will be set, otherwise it will be null
			UGameplayAbility* InstancedAbility = Spec.GetPrimaryInstance();
			const UGameplayAbility* CanActivateAbilitySource = InstancedAbility ? InstancedAbility : InAbilityCDO;

			const FGameplayTagContainer* SourceTags = nullptr;
			const FGameplayTagContainer* TargetTags = nullptr;

			const FGameplayAbilityActorInfo* ActorInfo = AbilityActorInfo.Get();

			FGameplayTagContainer FailureTags;

			bool bSuccess = CanActivateAbilitySource->CanActivateAbility(Spec.Handle, ActorInfo, SourceTags, TargetTags, &FailureTags);
			return bSuccess;
		}
	}

	return false;
}

float UPOTAbilitySystemComponentBase::GetTotalHealthBleed(const bool bRatio /*= false*/) const
{
	// @TODO: New Status might impact this logic. Should make it more dynamic.
	float BleedRate = GetNumericAttribute(UCoreAttributeSet::GetBleedingRateAttribute());
	if (BleedRate == 0.f)
	{
		return 0.f;
	}

	const float BleedHealRate = GetNumericAttribute(UCoreAttributeSet::GetBleedingHealRateAttribute());

	if (BleedHealRate <= 0.f)
	{
		return bRatio ? 1.f : GetNumericAttribute(UCoreAttributeSet::GetHealthAttribute());
	}
	
	float DamageTotal = 0.f;
	while (BleedRate > 0.f)
	{
		DamageTotal += BleedRate;
		BleedRate -= BleedHealRate;
	}

	if (bRatio)
	{
		const float MaxValue = GetNumericAttribute(UCoreAttributeSet::GetMaxHealthAttribute());
		return DamageTotal / MaxValue;
	}

	return DamageTotal;
}

float UPOTAbilitySystemComponentBase::GetTotalStaminaDrain(const bool bRatio /*= false*/) const
{
	return 0.f;
}

void UPOTAbilitySystemComponentBase::ModifyActiveGameplayEffectDuration(FGameplayEffectQuery Query, float Delta, float DeltaPercentage /*= 0.f*/)
{
	if (!GetOwner()->HasAuthority()) return;
	TArray<FActiveGameplayEffectHandle> ActiveEffects = GetActiveEffects(Query);

	for (const FActiveGameplayEffectHandle& Handle : ActiveEffects)
	{
		ModifyActiveGameplayEffectDurationByHandle(Handle, Delta, DeltaPercentage);
	}
}

void UPOTAbilitySystemComponentBase::ModifyActiveGameplayEffectDurationByHandle(FActiveGameplayEffectHandle Handle, float Delta, float DeltaPercentage /*= 0.f*/)
{
	if (!Handle.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[UWaAbilitySystemComponent::SupplementActiveGameplayEffectDurationByHandle] ActiveGameplayEffectHandle is invalid. Aborting."));
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("World was null in UWaAbilitySystemComponent::ReduceActiveGameplayEffectDuration"));
		return;
	}

	FTimerManager& WTM = World->GetTimerManager();

	FActiveGameplayEffect& Effect = *ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
	float TimeRemaining = WTM.GetTimerRemaining(Effect.DurationHandle);

	float ActualDelta = !FMath::IsNearlyZero(DeltaPercentage) ? Effect.GetDuration() * DeltaPercentage : Delta;

	float NewTimeRemaining = TimeRemaining + ActualDelta;

	if (NewTimeRemaining <= 0.f)
	{
		RemoveActiveGameplayEffect(Handle, -1);
	}
	else
	{
		ActiveGameplayEffects.ModifyActiveEffectStartTime(Handle, ActualDelta);
	}
}

void UPOTAbilitySystemComponentBase::ApplySoftClassPtrEffects(const TArray<TSoftClassPtr<UGameplayEffect>>& Effects)
{
	FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);

	for (const TSoftClassPtr<UGameplayEffect>& GameplayEffect : Effects)
	{
		const uint64 AsyncRequestId = NextAsyncGameplayEffectRequestId++;
		const FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::OnGEAsyncLoaded, GameplayEffect, AsyncRequestId);
		
		if (const TSharedPtr<FStreamableHandle> Handle = Streamable.RequestAsyncLoad(GameplayEffect.ToSoftObjectPath(), Delegate, FStreamableManager::AsyncLoadHighPriority))
		{
			AsyncGameplayEffectHandles.Add(AsyncRequestId, Handle);
		}
	}
}

float UPOTAbilitySystemComponentBase::GetHealthPercentage()
{
	return GetCurrentHealth() / GetMaxHealth();
}

float UPOTAbilitySystemComponentBase::GetMaxHealth()
{
	return GetNumericAttribute(UCoreAttributeSet::GetMaxHealthAttribute());
}

float UPOTAbilitySystemComponentBase::GetCurrentHealth()
{
	return GetNumericAttribute(UCoreAttributeSet::GetHealthAttribute());
}


void UPOTAbilitySystemComponentBase::OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	const UGameplayEffectUIData* UIData = UPOTAbilitySystemGlobals::GetActiveGameplayEffectUIData(ActiveHandle);
	if (UIData || GetOwner()->HasAuthority())
	{
		if (!IsRunningDedicatedServer() && UIData != nullptr)
		{
			if (const UPOTGameplayEffect* POTGameplayEffect = Cast<UPOTGameplayEffect>(SpecApplied.Def))
			{
				if (POTGameplayEffect->bTrackSourcesCount)
				{
					FGameplayTagContainer Tags;
					SpecApplied.GetAllAssetTags(Tags);
					FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAllEffectTags(Tags);
					if (Target->GetActiveEffects(Query).Num() > POTGameplayEffect->MaxSourcesCount)
					{
						// Do not populate additional events. We do not want to trigger Removed events if the application of the effect is not valid.
						Target->RemoveActiveGameplayEffect(ActiveHandle, SpecApplied.GetStackCount());
						return;
					}
				}
			}

			UClass* GEClass = SpecApplied.Def != nullptr ? SpecApplied.Def->GetClass() : nullptr;
			if (GEClass != nullptr && TrackedClientUIEffects.Contains(GEClass))
			{
				if (TrackedClientUIEffects[GEClass] != ActiveHandle)
				{
					OnUIEffectHandleUpdated.Broadcast(TrackedClientUIEffects[GEClass], ActiveHandle);
					TrackedClientUIEffects[GEClass] = ActiveHandle;
				}
			}
			else
			{
				TrackedClientUIEffects.Add(SpecApplied.Def->GetClass(), ActiveHandle);
			}
			OnUIEffectUpdated.Broadcast(ActiveHandle, SpecApplied.GetDuration(), SpecApplied.GetStackCount());
			
		}

		if (FActiveGameplayEffectEvents* EffectSet = GetActiveEffectEventSet(ActiveHandle))
		{
			EffectSet->OnStackChanged.AddUObject(this, &UPOTAbilitySystemComponentBase::OnActiveEffectStackChange);
			EffectSet->OnTimeChanged.AddUObject(this, &UPOTAbilitySystemComponentBase::OnActiveEffectTimeChange);
		}
	}
	
	AdjustGameplayEffectFoodTypes();
	AdjustGameplayEffectAdditionalAbilities();
	PotentiallyStartGrowthInhibitionCheck();
}

void UPOTAbilitySystemComponentBase::OnActiveEffectRemoved(const FActiveGameplayEffect& ActiveEffect)
{
	MaintainedStackDurations.Remove(ActiveEffect.Handle);

	Client_OnEffectRemoved(ActiveEffect);

	AdjustGameplayEffectFoodTypes();
	AdjustGameplayEffectAdditionalAbilities();
}

void UPOTAbilitySystemComponentBase::Client_OnEffectRemoved_Implementation(const FActiveGameplayEffect& ActiveEffect)
{
	const UWorld* World = GetWorld();

	if (!ActiveEffect.Spec.Def)
	{
		return;
	}

	if (!IsRunningDedicatedServer())
	{
		OnUIEffectRemoved.Broadcast(ActiveEffect);
	}

	const TSubclassOf<UGameplayEffect> EffectClass = ActiveEffect.Spec.Def->GetClass();
	if (!TrackedClientUIEffects.Contains(EffectClass))
	{
		return;
	}
	
	bool bRemove = false;
	const TArray<FActiveGameplayEffectHandle> QueryEffects = UPOTAbilitySystemGlobals::GetActiveGameplayEffectsByClass(this, EffectClass);
	const FActiveGameplayEffectHandle& EffectHandle = TrackedClientUIEffects[EffectClass];

	if (QueryEffects.IsEmpty())
	{
		bRemove = true;
	}
	else
	{
		int32 NumRemovedEffects = 0;
		const float CurrentTime = World->GetTimeSeconds();
		for (FActiveGameplayEffectHandle QueryEffectHandle : QueryEffects)
		{
			const FActiveGameplayEffect* QueryActiveEffect = GetActiveGameplayEffect(QueryEffectHandle);
			if (!QueryActiveEffect || QueryActiveEffect->IsPendingRemove)
			{
				NumRemovedEffects++;
				continue;
			}

			const float RemainingTime = QueryActiveEffect->GetTimeRemaining(CurrentTime);
			if ((RemainingTime != FGameplayEffectConstants::INFINITE_DURATION && RemainingTime <= UStatusEffectEntry::TimeRemainingRemoveFromUI))
			{
				NumRemovedEffects++;
				continue;
			}

			if (QueryEffectHandle != EffectHandle)
			{
				// Override the effect handle with the one we found, since the old one no longer exists and this one shouldnt be removed.
				TrackedClientUIEffects[EffectClass] = QueryEffectHandle;
				break;
			}
		}

		// All effects are null - remove the effect as a tracked UI effect.
		if (NumRemovedEffects == QueryEffects.Num())
		{
			bRemove = true;
		}
	}

	if (bRemove)
	{
		TrackedClientUIEffects.Remove(EffectClass);
	}
}

void UPOTAbilitySystemComponentBase::OnActiveEffectStackChange(FActiveGameplayEffectHandle ActiveHandle, int32 NewStackCount, int32 PreviousStackCount)
{	
	if (const FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(ActiveHandle))
	{		
		if (const UPOTGameplayEffect* POTGameplayEffect = Cast<UPOTGameplayEffect>(ActiveEffect->Spec.Def))
		{
			if (POTGameplayEffect->bMaintainOldStackDuration)
			{
				bool bClientPredict = ActiveEffect->PredictionKey.IsValidForMorePrediction();
				FTimerManager& TimerManager = GetWorld()->GetTimerManager();
				if (bClientPredict || GetOwnerActor()->HasAuthority())
				{
					if (NewStackCount > PreviousStackCount)  // stack added
					{

						float TimeRemaining = ActiveEffect->GetTimeRemaining(GetWorld()->GetTimeSeconds());
						float Duration = ActiveEffect->GetDuration();

						if (TimeRemaining > 0.f && Duration > 0.f)
						{
							float TimeElapsed = Duration - TimeRemaining;

							ActiveGameplayEffects.ModifyActiveEffectStartTime(ActiveHandle, TimeElapsed); // set duration full again

							int StacksAdded = NewStackCount - PreviousStackCount;
							for (int Index = 0; Index < StacksAdded; Index++)
							{
								FTimerDelegate Del;
								Del.BindUObject(this, &UPOTAbilitySystemComponentBase::MaintainedStackExpire, ActiveHandle, 1);
								FTimerHandle Handle;
								TimerManager.SetTimer(Handle, Del, TimeRemaining, false);
								MaintainedStackDurations.FindOrAdd(ActiveHandle).Add(Handle);
							}
						}
					}
					else if (NewStackCount < PreviousStackCount) // stack removed
					{
						int StacksRemoved = PreviousStackCount - NewStackCount;

						for (int Index = 0; Index < StacksRemoved; Index++)
						{
							if (TArray<FTimerHandle>* HandleArray = MaintainedStackDurations.Find(ActiveHandle))
							{
								if (HandleArray->IsValidIndex(0))
								{
									TimerManager.ClearTimer((*HandleArray)[0]);
									HandleArray->RemoveAt(0);
								}
							}
						}
					}
				}
			}
		}

		if (!IsRunningDedicatedServer() && ActiveEffect->Spec.Def != nullptr && UPOTAbilitySystemGlobals::GetActiveGameplayEffectUIData(ActiveHandle))
		{
			OnUIEffectUpdated.Broadcast(ActiveHandle, ActiveEffect->GetDuration(), NewStackCount);
		}
	}
}

void UPOTAbilitySystemComponentBase::MaintainedStackExpire(FActiveGameplayEffectHandle ActiveHandle, int Stacks)
{
	if (!ensure(Stacks > 0))
	{
		UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemComponentBase::MaintainedStackExpire: StacksAdded = %i"), Stacks);
		return;
	}
	RemoveActiveGameplayEffect(ActiveHandle, Stacks);
}


void UPOTAbilitySystemComponentBase::OnActiveEffectTimeChange(FActiveGameplayEffectHandle ActiveHandle, float NewStartTime, float NewDuration)
{
	if (const FActiveGameplayEffect* ActiveEffect = GetActiveGameplayEffect(ActiveHandle))
	{
		if (!IsRunningDedicatedServer() && ActiveEffect->Spec.Def != nullptr && UPOTAbilitySystemGlobals::GetActiveGameplayEffectUIData(ActiveHandle))
		{
			OnUIEffectUpdated.Broadcast(ActiveHandle, NewDuration, ActiveEffect->ClientCachedStackCount);
		}
		
	}
}

bool UPOTAbilitySystemComponentBase::AttemptToFixGEHandles(TSubclassOf<UGameplayEffect> EffectToCompare /*= nullptr*/)
{
	for (auto& KVP : TrackedClientUIEffects)
	{
		const FActiveGameplayEffect* ExistingGE = GetActiveGameplayEffect(KVP.Value);
		if (!ExistingGE || (EffectToCompare != nullptr && ExistingGE->Spec.Def  != nullptr && ExistingGE->Spec.Def->GetClass() == EffectToCompare))
		{
			for (auto It = ActiveGameplayEffects.CreateConstIterator(); It; ++It)
			{
				const FActiveGameplayEffect& ActiveGE = *It;
				if (ActiveGE.Spec.Def != nullptr && ActiveGE.Spec.Def->GetClass() == KVP.Key)
				{
					OnUIEffectHandleUpdated.Broadcast(TrackedClientUIEffects[KVP.Key], ActiveGE.Handle);
					TrackedClientUIEffects[KVP.Key] = ActiveGE.Handle;
					return true;
				}
			}
		}
		/*else if (ExistingGE->IsPendingRemove || ExistingGE->GetTimeRemaining(GetWorld()->GetTimeSeconds()) || ExistingGE->ClientCachedStackCount <= 0)
		{
			OnUIEffectRemoved.Broadcast(ExistingGE->Spec.GetContext().GetSourceObject());
			TrackedClientUIEffects.Remove(KVP.Key);
		}*/
	}

	return false;
}

void UPOTAbilitySystemComponentBase::AdjustGameplayEffectFoodTypes()
{
	AIBaseCharacter* const OwnerChar = Cast<AIBaseCharacter>(GetOwner());
	if (OwnerChar == nullptr)
	{
		return;
	}
	
	TArray<FName> CDOFoodTags{};
	if (const AIBaseCharacter* OwnerCDO = GetOwner()->GetClass()->GetDefaultObject<AIBaseCharacter>())
	{
		CDOFoodTags = OwnerCDO->FoodTags;
		OwnerChar->FoodTags = CDOFoodTags;
	}

	FGameplayTagContainer FoodTags{ UPOTAbilitySystemGlobals::Get().FoodTypeTag };
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAllOwningTags(FoodTags);
	const TArray<FActiveGameplayEffectHandle> FoodTypeGEs = GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : FoodTypeGEs)
	{
		if (const UGameplayEffect* Def = GetGameplayEffectDefForHandle(Handle))
		{
			FGameplayTagContainer FoodTypeTags = Def->GetGrantedTags().Filter(FoodTags);
			for (const FGameplayTag& FoodTag : FoodTypeTags)
			{
				const FName FoodName = UIGameplayStatics::GetTagLeafName(FoodTag);
				OwnerChar->FoodTags.AddUnique(FoodName);
			}
		}
	}
}

void UPOTAbilitySystemComponentBase::AdjustGameplayEffectAdditionalAbilities()
{
	AIDinosaurCharacter* const OwnerChar = Cast<AIDinosaurCharacter>(GetOwner());
	if (OwnerChar == nullptr)
	{
		return;
	}
	
	bool bCDOCanJump = false;
	bool bCDOCanDive = false;
	const AIDinosaurCharacter* const OwnerCDO = OwnerChar->GetClass()->GetDefaultObject<AIDinosaurCharacter>();
	if (ensureAlways(OwnerCDO))
	{
		bCDOCanJump = OwnerCDO->JumpMaxCount > 0 && OwnerCDO->GetCharacterMovement()->MovementState.bCanJump && OwnerCDO->GetCharacterMovement()->NavAgentProps.bCanJump;
		bCDOCanDive = OwnerCDO->IsAquatic();
	}

	if (!bCDOCanJump)
	{
		OwnerChar->JumpMaxCount = 0;
		OwnerChar->GetCharacterMovement()->MovementState.bCanJump = false;
		OwnerChar->GetCharacterMovement()->NavAgentProps.bCanJump = false;
	}

	if (!bCDOCanDive)
	{
		OwnerChar->SetAquatic(false);
		OwnerChar->SetCanUsePreciseSwimming(false);
	}

	FGameplayTagContainer TagContainer;
	TagContainer.AddTagFast(UPOTAbilitySystemGlobals::Get().CanDiveTag);
	TagContainer.AddTagFast(UPOTAbilitySystemGlobals::Get().CanJumpTag);
	const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TagContainer);

	const TArray<FActiveGameplayEffectHandle> FoodTypeGEs = GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : FoodTypeGEs)
	{
		const UGameplayEffect* Def = GetGameplayEffectDefForHandle(Handle);
		if (!Def)
		{
			continue;
		}

		FGameplayTagContainer AdditionalAbilityTags = Def->GetGrantedTags().Filter(TagContainer);
		for (auto It = AdditionalAbilityTags.CreateConstIterator(); It; ++It)
		{
			if (!bCDOCanDive && (*It).MatchesTag(UPOTAbilitySystemGlobals::Get().CanDiveTag))
			{
				OwnerChar->SetAquatic(true);
				OwnerChar->SetCanUsePreciseSwimming(true);
			}

			if (!bCDOCanJump && (*It).MatchesTag(UPOTAbilitySystemGlobals::Get().CanJumpTag))
			{
				OwnerChar->JumpMaxCount = 1;
				OwnerChar->GetCharacterMovement()->MovementState.bCanJump = true;
				OwnerChar->GetCharacterMovement()->NavAgentProps.bCanJump = true;
			}
		}
	}
}

void UPOTAbilitySystemComponentBase::PotentiallyStartGrowthInhibitionCheck()
{
	AIBaseCharacter* OwnerChar = Cast<AIBaseCharacter>(GetOwner());
	if (OwnerChar == nullptr || !OwnerChar->IsGrowing() || OwnerChar->bIsCharacterEditorPreviewCharacter || OwnerChar->GetGrowthPercent() >= 1.f)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	World->GetTimerManager().SetTimer(GrowthInhibitionTimerHandle, this, &UPOTAbilitySystemComponentBase::OnInhibitionTimer, GROWTH_IMPEDIMENT_CHECK_INTERVAL, true);
	CheckGrowthInhibition(true);
}

void UPOTAbilitySystemComponentBase::OnAttributeSetDataReloaded(bool bIsFromCSV, bool bIsModData)
{
	InitAttributes(false);
}

void UPOTAbilitySystemComponentBase::OnGEAsyncLoaded(TSoftClassPtr<UGameplayEffect> GEC, uint64 AsyncRequestId)
{
	TSharedPtr<FStreamableHandle> Handle = nullptr;
	AsyncGameplayEffectHandles.RemoveAndCopyValue(AsyncRequestId, Handle);
	
	if (const UClass* const EffectClass = GEC.Get())
	{
		ApplyGameplayEffectToSelf(EffectClass->GetDefaultObject<UGameplayEffect>(), 1, MakeEffectContext());
	}
}

void UPOTAbilitySystemComponentBase::OnInhibitionTimer()
{
	CheckGrowthInhibition();
}

void UPOTAbilitySystemComponentBase::CheckGrowthInhibition(bool bInitialTest /*= false*/)
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		bGrowthInhibited = false;
		UnblockGrowth();
		return;
	}

	AIBaseCharacter* OwnerChar = Cast<AIBaseCharacter>(GetOwner());
	if (OwnerChar == nullptr || OwnerChar->bIsCharacterEditorPreviewCharacter || (OwnerChar->GetGrowthPercent() >= 1.f && !bGrowthInhibited))
	{
		bGrowthInhibited = false;
		World->GetTimerManager().ClearTimer(GrowthInhibitionTimerHandle);
		UnblockGrowth();
		return;
	}

	if(!OwnerChar->IsGrowing() && !bGrowthInhibited)
	{
		bGrowthInhibited = false;
		UnblockGrowth();
		World->GetTimerManager().ClearTimer(GrowthInhibitionTimerHandle);
		return;
	}

	if(bGrowthForceInhibited)
	{
		bGrowthInhibited = true;
		if(!bInitialTest)
		{
			BlockGrowth();
		}
		return;
		
	}


	static FGameplayEffectQuery Query = Query.MakeQuery_MatchAllOwningTags(FGameplayTagContainer(FGameplayTag::RequestGameplayTag(NAME_DebuffGrowthDisabled)));
	if(GetActiveEffects(Query).Num() > 0 && !GrowthInhibitionHandle.IsValid())
	{
		bGrowthInhibited = true;
		if (!bInitialTest)
		{
			BlockGrowth();
		}
		return;
	}


	UCapsuleComponent* const CapsuleComp = OwnerChar->GetCapsuleComponent();
	if(CapsuleComp == nullptr || !CapsuleComp->IsQueryCollisionEnabled())
	{
		bGrowthInhibited = false;
		UnblockGrowth();
		return;
	}

	float Inflation = CapsuleComp->GetScaledCapsuleRadius() * GrowthInflationRatio;

	bool bFoundBlockingHit = false;
	TArray<FOverlapResult> Overlaps;
	ECollisionChannel const BlockingChannel = CapsuleComp->GetCollisionObjectType();
	FCollisionShape const CollisionShape = CapsuleComp->GetCollisionShape(Inflation);

	FVector TestLocation = OwnerChar->GetActorLocation();
	TestLocation.Z += (CapsuleComp->GetScaledCapsuleHalfHeight() * GrowthInflationRatio);
	
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CheckGrowthInhibition), false, OwnerChar);
	FCollisionResponseParams ResponseParams;
	CapsuleComp->InitSweepCollisionParams(Params, ResponseParams);

	TArray<AActor*> IgnoreActors { OwnerChar };
	if(UCharacterMovementComponent* CMC = OwnerChar->GetCharacterMovement())
	{
		if(CMC->GetMovementBase() != nullptr)
		{
			IgnoreActors.Add(CMC->GetMovementBase()->GetOwner());
		}
	}

	Params.AddIgnoredActors(IgnoreActors);
	bFoundBlockingHit = World->OverlapMultiByChannel(Overlaps, TestLocation, OwnerChar->GetActorRotation().Quaternion(), BlockingChannel, CollisionShape, Params, ResponseParams);
	
	if (bFoundBlockingHit)
	{
		DrawDebugCapsule(World, TestLocation, CollisionShape.GetCapsuleHalfHeight(), CollisionShape.GetCapsuleRadius(),
			OwnerChar->GetActorRotation().Quaternion(), bFoundBlockingHit ? FColor::Red : FColor::Green, GROWTH_IMPEDIMENT_CHECK_INTERVAL, 0, 5.f);
		
#if !UE_BUILD_SHIPPING
		for (const FOverlapResult& Result : Overlaps)
		{
			if (Result.bBlockingHit)
			{
				UE_LOG(LogTemp, Log, TEXT("Block overlap on %s - %s"), *Result.GetActor()->GetName(), *Result.GetComponent()->GetName());
			}
		}
#endif
	}
	

	bGrowthInhibited = bFoundBlockingHit;
	if(bGrowthInhibited && !bInitialTest)
	{
		BlockGrowth();
	}
	else if(!bGrowthInhibited && !bInitialTest)
	{
		UnblockGrowth();
	}
}

void UPOTAbilitySystemComponentBase::BlockGrowth()
{
	FGameplayEffectQuery Query;
	Query.EffectDefinition = UIProjectSettings::GetGrowthRewardGameplayEffect();
	ModifyActiveGameplayEffectDuration(Query, GROWTH_IMPEDIMENT_CHECK_INTERVAL);

	if(!GrowthInhibitionHandle.IsValid() || GetCurrentStackCount(GrowthInhibitionHandle) <= 0)
	{
		const TSubclassOf<UGameplayEffect> GrowthInhibitionGE = UPOTAbilitySystemGlobals::Get().GrowthInhibitedGameplayEffect;
		FGameplayEffectSpecHandle InhibitionSpec = MakeOutgoingSpec(GrowthInhibitionGE, 1, MakeEffectContext());
		if (InhibitionSpec.IsValid())
		{
			GrowthInhibitionHandle = ApplyGameplayEffectSpecToSelf(*InhibitionSpec.Data);
		}
	}
	
}

void UPOTAbilitySystemComponentBase::UnblockGrowth()
{
	if(GrowthInhibitionHandle.IsValid() || GetCurrentStackCount(GrowthInhibitionHandle) > 0)
	{
		RemoveActiveGameplayEffect(GrowthInhibitionHandle);
		GrowthInhibitionHandle.Invalidate();
	}

	if (AIDinosaurCharacter* IDinoOwner = Cast<AIDinosaurCharacter>(GetOwner()))
	{
		IDinoOwner->InterpolateGrowth();
	}
}

void UPOTAbilitySystemComponentBase::AddNewCooldownEffect(FActiveGameplayEffectHandle& CooldownEffectHandle)
{
	FActiveGameplayEffect* ActiveEffectPtr = ActiveGameplayEffects.GetActiveGameplayEffect(CooldownEffectHandle);

	check(ActiveEffectPtr);
	if (!ActiveEffectPtr)
	{
		UE_LOG(TitansLog, Error, TEXT("UPOTAbilitySystemComponentBase::AddNewCooldownEffect: ActiveEffectPtr nullptr"));
		return;
	}

	FActiveGameplayEffect CooldownEffect = *ActiveEffectPtr;

	CurrentCooldownEffects.Add(CooldownEffect);

	float CurrentTime = GetWorld()->GetTimeSeconds();

	FTimerHandle ExpireTimerHandle;
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::ExpireOldCooldownEffect, CooldownEffect);
	GetWorld()->GetTimerManager().SetTimer(ExpireTimerHandle, Del, CooldownEffect.GetTimeRemaining(CurrentTime), false);

}

const TArray<FActiveGameplayEffect>& UPOTAbilitySystemComponentBase::GetCurrentCooldownEffects() const
{
	return CurrentCooldownEffects;
}

const TArray<TPair<const UGameplayEffect*, FTimerHandle>>& UPOTAbilitySystemComponentBase::GetCurrentCooldownTimers() const
{
	return CurrentCooldownDelays;
}

void UPOTAbilitySystemComponentBase::ExpireOldCooldownEffect(FActiveGameplayEffect Effect)
{
	CurrentCooldownEffects.Remove(Effect);
}

void UPOTAbilitySystemComponentBase::NotifyTimeOfDay(float InTime)
{
	FGameplayTag NewTag = FGameplayTag::EmptyTag;

	if (InTime > 600.f && InTime < 1800.f)
	{
		NewTag = FGameplayTag::RequestGameplayTag(NAME_AbilityTimeOfDay_Day);
	}
	else
	{
		NewTag = FGameplayTag::RequestGameplayTag(NAME_AbilityTimeOfDay_Night);
	}

	if (NewTag != CurrentTimeOfDay)
	{
		RemoveLooseGameplayTag(CurrentTimeOfDay);
		AddLooseGameplayTag(NewTag);
		
		CurrentTimeOfDay = NewTag;
	}
}

void UPOTAbilitySystemComponentBase::ResetAllCooldowns(AInfo* Caller)
{
	if (!Caller->HasAuthority())
	{
		UE_LOG(TitansLog, Warning, TEXT("UPOTAbilitySystemComponentBase::ResetAllCooldowns: Non-Authoritative attempt to reset abilities cooldown."));
		return;
	}

	RemoveAllCooldowns();
}

void UPOTAbilitySystemComponentBase::RemoveAllCooldowns()
{
	TArray<FActiveGameplayEffect> CopiedEffects = CurrentCooldownEffects;
	for (FActiveGameplayEffect& CooldownEffect : CopiedEffects)
	{
		RemoveActiveGameplayEffect(CooldownEffect.Handle, -1);
	}
	CurrentCooldownEffects.Empty();
	CurrentCooldownDelays.Empty();
	Client_RemoveAllCooldowns();
}

float UPOTAbilitySystemComponentBase::GetCurrentAttackMontageTimeRemaining()
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance)
	{
		if (UAnimMontage* Montage = GetCurrentAttackMontage())
		{
			if (AnimInstance->Montage_IsPlaying(Montage))
			{
				float AnimPosition = AnimInstance->Montage_GetPosition(Montage);
				float AnimPlayRate = AnimInstance->Montage_GetPlayRate(Montage);
				float AnimLength = Montage->GetPlayLength();
				float RemainingTimeUnscaled = AnimLength - AnimPosition;
				float RemainingTimeScaled = RemainingTimeUnscaled / Montage->RateScale;	
				//UE_LOG(TitansLog, Log, TEXT("montage time pos %f, play rate %f, length %f, unscaled %f, scaled %f"), AnimPosition, AnimPlayRate, AnimLength, RemainingTimeUnscaled, RemainingTimeScaled);
				return RemainingTimeScaled;
				
			}
			else
			{
				//UE_LOG(TitansLog, Log, TEXT("Ability but no montage"));
			}
		}
	}
	return 0.0f;
}

bool UPOTAbilitySystemComponentBase::TryEndExclusiveAbility(FGameplayAbilitySpecHandle Handle, const UPOTGameplayAbility* POTGameplayAbility, const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags)
{
	if (!POTGameplayAbility)
	{
		return false;
	}
	if (POTGameplayAbility->bIsExclusive && !POTGameplayAbility->CanActivateAbility(Handle, AbilityActorInfo.Get(), SourceTags, TargetTags, nullptr))
	{
		UPOTGameplayAbility* CurrentAttackAbility = GetCurrentAttackAbility();
		if (CurrentAttackAbility && CurrentAttackAbility->bIsExclusive)
		{
			if (AbilityActorInfo->IsNetAuthority())
			{
				//UE_LOG(LogTemp, Log, TEXT("ending double exclusive ability"));
			}
			CurrentAttackAbility->EndAbility(Handle, AbilityActorInfo.Get(), CurrentAttackAbility->GetCurrentActivationInfo(), true, false);
			return true;
		}
	}
	return false;
}

void UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(FGameplayAbilitySpecHandle Handle, bool InputPressed, const FPredictionKey& PredictionKey, const FGameplayEventData* TriggerEventData)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, TriggerEventData);
		return;
	}

	const UGameplayAbility* AbilityToActivate = Spec->Ability;

	if (!ensure(AbilityToActivate))
	{
		Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, TriggerEventData);
		return;
	}

	const UPOTGameplayAbility* POTGameplayAbility = Cast<UPOTGameplayAbility>(AbilityToActivate);

	if (CurrentAbilityIsExclusive() && POTGameplayAbility->bIsExclusive)
	{
		return;
	}

	float RemainingCooldown = GetCooldownTimeRemaining(AbilityToActivate->GetClass());
	
	if (POTGameplayAbility && (!POTGameplayAbility->bIsExclusive || !CurrentAbilityIsExclusive()) && RemainingCooldown <= 0)
	{
		//UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(): Skip"));
		Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, TriggerEventData);
		return;
	}

	float MontageTimeRemaining = GetCurrentAttackMontageTimeRemaining();	

	if (RemainingCooldown < MontageTimeRemaining)
	{
		//UE_LOG(TitansLog, Log, TEXT("Using remaining montage time %f"), MontageTimeRemaining);
		RemainingCooldown = MontageTimeRemaining;
	}

	//UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(): Test %f seconds"), RemainingCooldown);

	const FGameplayTagContainer* SourceTags = nullptr;
	const FGameplayTagContainer* TargetTags = nullptr;
	if (TriggerEventData != nullptr)
	{
		SourceTags = &TriggerEventData->InstigatorTags;
		TargetTags = &TriggerEventData->TargetTags;
	}

	if (RemainingCooldown <= 0.1f)
	{
		bool bCanActivateNow = TryEndExclusiveAbility(Handle, POTGameplayAbility, SourceTags, TargetTags);
		
		if (bCanActivateNow)
		{
			Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, TriggerEventData);
			GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Reactivation);
			return;
		}
	}

	if (RemainingCooldown > 5.0f)
	{
		// The remaining cooldown is too large, fail the activation.
		
		//UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(): remaining cooldown: %f seconds"), RemainingCooldown); 
		Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, TriggerEventData);
		Client_RemoveCooldown(Handle);
		GetWorld()->GetTimerManager().ClearTimer(TimerHandle_Reactivation);
		return;
	}
	else if (RemainingCooldown < 0.01f)
	{
		RemainingCooldown = 0.01f;
	}

	// If there is remaining cooldown, we can delay this activation until the cooldown is expired, then play the ability. The client will catch back up under normal circumstances.
	//UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(): Delaying by: %f seconds"), RemainingCooldown);
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::RetryInternalServerTryActivateAbility, Handle, InputPressed, PredictionKey, TriggerEventData ? *TriggerEventData : FGameplayEventData(), TriggerEventData != nullptr, false);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_Reactivation, Del, RemainingCooldown, false);
}

void UPOTAbilitySystemComponentBase::RetryInternalServerTryActivateAbility(FGameplayAbilitySpecHandle Handle, bool InputPressed, const FPredictionKey PredictionKey, FGameplayEventData TriggerEventData, bool bHasTriggerEventData, bool bSkipTick)
{
	if (!bSkipTick)
	{
		// Occasionally the ability will not have ended yet because the montage end delegate has not been called, so need to wait 1 more tick.
		
		//UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(): Trying in 1 tick"));
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::RetryInternalServerTryActivateAbility, Handle, InputPressed, PredictionKey, TriggerEventData, bHasTriggerEventData, true);
		GetWorld()->GetTimerManager().SetTimerForNextTick(Del);
	}
	else
	{
		float RemainingCooldown = 0.0f;
		FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
		if (Spec)
		{
			const UGameplayAbility* AbilityToActivate = Spec->Ability;
			if (AbilityToActivate)
			{
				RemainingCooldown = GetCooldownTimeRemaining(AbilityToActivate->GetClass());
				float MontageTimeRemaining = GetCurrentAttackMontageTimeRemaining();

				if (RemainingCooldown < MontageTimeRemaining)
				{
					//UE_LOG(TitansLog, Log, TEXT("Using remaining montage time %f"), MontageTimeRemaining);
					RemainingCooldown = MontageTimeRemaining;
				}

				if (RemainingCooldown <= 0.1f)
				{
					const UPOTGameplayAbility* POTGameplayAbility = Cast<UPOTGameplayAbility>(AbilityToActivate);
					if (POTGameplayAbility)
					{
						bool bCanActivateNow = TryEndExclusiveAbility(Handle, POTGameplayAbility, bHasTriggerEventData ? &TriggerEventData.InstigatorTags : nullptr, bHasTriggerEventData ? &TriggerEventData.TargetTags : nullptr);
						//UE_LOG(TitansLog, Log, TEXT("bCanActivateNow %s"), (bCanActivateNow ? TEXT("true") : TEXT("false")));
					}
				}
			}
		}

		//UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::InternalServerTryActivateAbility(): Retrying"));
		if (bHasTriggerEventData)
		{
			if (AbilityActorInfo->IsNetAuthority())
			{
				//UE_LOG(LogTemp, Log, TEXT("trying event data (Time remaining %f )"), RemainingCooldown);
			}
			Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, &TriggerEventData);
		}
		else
		{
			//UE_LOG(LogTemp, Log, TEXT("trying no event data (Time remaining %f )"), RemainingCooldown);
			Super::InternalServerTryActivateAbility(Handle, InputPressed, PredictionKey, nullptr);
		}
	}
}

void UPOTAbilitySystemComponentBase::Client_RemoveCooldown_Implementation(FGameplayAbilitySpecHandle Handle)
{
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_RemoveCooldown_Implementation(): Spec nullptr"));
		return;
	}

	const UGameplayAbility* AbilityToActivate = Spec->Ability;

	if (!AbilityToActivate)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_RemoveCooldown_Implementation(): AbilityToActivate nullptr"));
		return;
	}

	if (const UGameplayEffect* Effect = AbilityToActivate->GetCooldownGameplayEffect())
	{
		for (const FActiveGameplayEffect& Cooldown : GetCurrentCooldownEffects())
		{
			if (Cooldown.Spec.Def == Effect)
			{
				ExpireOldCooldownEffect(Cooldown);
				break;
			}
		}
	}
}

void UPOTAbilitySystemComponentBase::Client_RemoveAllCooldowns_Implementation()
{
	CurrentCooldownEffects.Empty();
	CurrentCooldownDelays.Empty();
}

void UPOTAbilitySystemComponentBase::Client_OnAbilityDelayed_Implementation(FGameplayAbilitySpecHandle Handle, float DelayAmount)
{
	UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_OnAbilityDelayed_Implementation(): Received extra cooldown %f seconds"), DelayAmount);

	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_OnAbilityDelayed_Implementation(): Spec nullptr"));
		return;
	}

	const UGameplayAbility* AbilityToActivate = Spec->Ability;

	if (!AbilityToActivate)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_OnAbilityDelayed_Implementation(): AbilityToActivate nullptr"));
		return;
	}	

	if (const UGameplayEffect* Effect = AbilityToActivate->GetCooldownGameplayEffect())
	{
		float RemainingTime = GetCooldownTimeRemaining(AbilityToActivate->GetClass());

		FTimerHandle NewCooldownDelayHandle;
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::ClearDelayedCooldown, Effect);
		GetWorld()->GetTimerManager().SetTimer(NewCooldownDelayHandle, Del, RemainingTime + DelayAmount, false);

		CurrentCooldownDelays.Add({ Effect, NewCooldownDelayHandle });
	}	
}

void UPOTAbilitySystemComponentBase::Client_OnRestoreCooldown_Implementation(FGameplayAbilitySpecHandle Handle, float CooldownAmount)
{
	UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_OnRestoreCooldown_Implementation(): Restored %f seconds"), CooldownAmount);

	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle);
	if (!Spec)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_OnRestoreCooldown_Implementation(): Spec nullptr"));
		return;
	}

	const UGameplayAbility* AbilityToActivate = Spec->Ability;

	if (!AbilityToActivate)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponentBase::Client_OnRestoreCooldown_Implementation(): AbilityToActivate nullptr"));
		return;
	}

	if (const UGameplayEffect* Effect = AbilityToActivate->GetCooldownGameplayEffect())
	{
		FTimerHandle NewCooldownDelayHandle;
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::ClearDelayedCooldown, Effect);
		GetWorld()->GetTimerManager().SetTimer(NewCooldownDelayHandle, Del, CooldownAmount, false);

		CurrentCooldownDelays.Add({ Effect, NewCooldownDelayHandle });
	}
}

void UPOTAbilitySystemComponentBase::ClearDelayedCooldown(const UGameplayEffect* Cooldown)
{
	check(Cooldown)
	if (!Cooldown)
	{
		UE_LOG(TitansLog, Error, TEXT("UPOTAbilitySystemComponentBase::ClearDelayedCooldown: Cooldown nullptr"));
		return;
	}
	for (int Index = 0; Index < CurrentCooldownDelays.Num(); Index++)
	{
		if (CurrentCooldownDelays[Index].Key == Cooldown)
		{
			CurrentCooldownDelays.RemoveAt(Index);
			return;
		}
	}
}

void UPOTAbilitySystemComponentBase::AddCooldownDelay(const UGameplayEffect* CooldownEffect, float Duration)
{
	check(CooldownEffect);
	if (!CooldownEffect)
	{
		UE_LOG(TitansLog, Error, TEXT("UPOTAbilitySystemComponentBase::AddCooldownDelay: CooldownEffect nullptr"));
		return;
	}

	FTimerHandle NewCooldownDelayHandle;
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UPOTAbilitySystemComponentBase::ClearDelayedCooldown, CooldownEffect);
	GetWorld()->GetTimerManager().SetTimer(NewCooldownDelayHandle, Del, Duration, false);

	CurrentCooldownDelays.Add({ CooldownEffect, NewCooldownDelayHandle });
}

void UPOTAbilitySystemComponentBase::ApplyAbilityCooldown(const TSubclassOf<UGameplayAbility>& InAbility, float CooldownTime)
{
	AIBaseCharacter* OwnerChar = Cast<AIBaseCharacter>(GetOwner());
	check(OwnerChar && OwnerChar->HasAuthority());
	check(InAbility.Get());
	check(CooldownTime > 0);
	if (!OwnerChar || !OwnerChar->HasAuthority()) return;
	if (!InAbility.Get()) return;
	if (CooldownTime <= 0) return;

	const UGameplayAbility* const InAbilityCDO = InAbility.GetDefaultObject();

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (Spec.Ability == InAbilityCDO)
		{
			FActiveGameplayEffect* ActiveCooldown = GetActiveCooldownEffect(InAbility);
			if (!ActiveCooldown) // If cooldown doesnt exist, add it
			{
				FGameplayAbilityActivationInfo NewActivationInfo (GetOwner());
				FGameplayAbilityActorInfo ActorInfo;
				ActorInfo.InitFromActor(GetOwner(), GetOwner(), this);

				InAbilityCDO->ApplyCooldown(Spec.Handle, &ActorInfo, NewActivationInfo);
				ActiveCooldown = GetActiveCooldownEffect(InAbility);
			}

			// Should have an active cooldown at this point
			check(ActiveCooldown);
			if (ActiveCooldown)
			{
				CooldownTime = FMath::Min(ActiveCooldown->GetDuration(), CooldownTime);
				float WorldTime = GetWorld()->GetTimeSeconds();
				float Delta = CooldownTime - ActiveCooldown->GetTimeRemaining(WorldTime);
				ActiveCooldown->StartServerWorldTime += Delta;
				ActiveCooldown->StartWorldTime += Delta;

				if (!OwnerChar->IsLocallyControlled()) // Replicate to client that this cooldown was adjusted, to ensure the UI is updated correctly
				{
					Client_OnRestoreCooldown(Spec.Handle, CooldownTime);
				}
			}
			return;
		}
	}

	ensure(false); // Ability was not in ActivatableAbilities, this should not happen unless ability assets are changed
}

FActiveGameplayEffect* UPOTAbilitySystemComponentBase::GetActiveCooldownEffect(const TSubclassOf<UGameplayAbility>& AbilityClass)
{
	UGameplayAbility* GADefault = AbilityClass->GetDefaultObject<UGameplayAbility>();
	if (!GADefault)
	{
		return nullptr;
	}
	
	UGameplayEffect* CooldownEffect = GADefault->GetCooldownGameplayEffect();
	if (!CooldownEffect)
	{
		return nullptr;
	}

	for (FActiveGameplayEffect& ActiveEffect : CurrentCooldownEffects)
	{
		if (ActiveEffect.Spec.Def == CooldownEffect)
		{
			return &ActiveEffect;
		}
	}

	return nullptr;
}

FActiveGameplayEffect UPOTAbilitySystemComponentBase::GetActiveEffect(const TSubclassOf<UGameplayEffect>& EffectClass)
{
	for (auto It = ActiveGameplayEffects.CreateConstIterator(); It; ++It)
	{
		const FActiveGameplayEffect& ActiveGE = *It;
		if (ActiveGE.Spec.Def != nullptr && ActiveGE.Spec.Def->GetClass() == EffectClass)
		{
			return ActiveGE;
		}
	}

	return FActiveGameplayEffect();
}

void UPOTAbilitySystemComponentBase::ClearCurrentCooldownDelays()
{
	CurrentCooldownDelays.Empty();
}