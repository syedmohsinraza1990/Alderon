// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Abilities/POTAbilitySystemComponent.h"
#include "Abilities/POTGameplayAbility.h"
#include "Abilities/POTGameplayEffect.h"
#include "AbilitySystemGlobals.h"
#include "GenericTeamAgentInterface.h"
#include "TimerManager.h"
#include "Abilities/POTAbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "AIController.h"
#include "IGameplayStatics.h"
#include "Player/IBaseCharacter.h"
#include "Perception/AIPerceptionComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayEffectTypes.h"
#include "Abilities/CoreAttributeSet.h"
#include "Components/ICharacterMovementComponent.h"
#include "Stats/Stats.h"
#include "Stats/IStats.h"
#include "TitanAssetManager.h"
#include "Engine/AssetManager.h"
#include "Abilities/POTAbilityAsset.h"
#include "Components/CapsuleComponent.h"
#include "Online/IGameState.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "Online/IGameSession.h"
#include "IGameInstance.h"
#include "Animation/DinosaurAnimBlueprint.h"
#include "ITraceUtils.h"

UPOTAbilitySystemComponent::UPOTAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bCanFireDamageFinishedEvent = false;

	LocomotionStateUpdateRate = 0.5f;
	
	bCurrentAttackAbilityUsesCharge = false;
	bAreIgnoreEventsEnabled = false;
}

void UPOTAbilitySystemComponent::InitializeAbilitySystem(AActor* InOwnerActor, AActor* InAvatarActor, const bool bPreviewOnly)
{
	Super::InitializeAbilitySystem(InOwnerActor, InAvatarActor, bPreviewOnly);

	if (bPreviewOnly && !UIGameplayStatics::IsGrowthEnabled(this))
	{
		SetNumericAttributeBase(UCoreAttributeSet::GetGrowthAttribute(), 1.f);
		return;
	}

	SetupAttributeDelegates();

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		// Only server should execute the rest. Clients only need to add the attribute changed delegates.
		return;
	}

	OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &UPOTAbilitySystemComponent::OnActiveGameplayEffectAddedCallback);
	ActiveGameplayEffects.OnActiveGameplayEffectRemovedDelegate.AddUObject(this, &UPOTAbilitySystemComponent::RemoveBoneDamageMultipliersOnEffectRemoved);

	InAvatarActor->GetWorldTimerManager().SetTimer(LocomotionUpdateTimerHandle, this, &UPOTAbilitySystemComponent::UpdateLocomotionEffects, LocomotionStateUpdateRate, true);

	if (ParentPOTCharacter != nullptr)
	{
		ParentPOTCharacter->OnBrokenBone.AddUniqueDynamic(this, &UPOTAbilitySystemComponent::OnOwnerBoneBreak);
		ParentPOTCharacter->GetSlottedActiveAndPassiveEffects_Mutable().Empty();

		UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

		for (const FSlottedAbilities& SAbility : ParentPOTCharacter->GetSlottedAbilityAssetsArray())
		{
			for(int32 i = 0; i < SAbility.SlottedAbilities.Num(); i++)
			{
				const FPrimaryAssetId& AbilityId = SAbility.SlottedAbilities[i];
				
				if (UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(AbilityId))
				{
					if(LoadedAbility->GrantedAbility != nullptr)
					{
						FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(LoadedAbility->GrantedAbility, GetLevel(), INDEX_NONE, LoadedAbility);
						ParentPOTCharacter->GetSlottedActiveAndPassiveEffects_Mutable().Add(FSlottedAbilityEffects(AbilityId, GiveAbility(AbilitySpec)));
					}
	
					for (TSubclassOf<UGameplayEffect> PassiveEffect : LoadedAbility->GrantedPassiveEffects)
					{
						if (!ensureAlways(PassiveEffect.Get()))
						{
							continue;
						}

						const UGameplayEffect* Def = PassiveEffect->GetDefaultObject<UGameplayEffect>();
						FGameplayEffectContextHandle Context = MakeEffectContext();

						ParentPOTCharacter->GetSlottedActiveAndPassiveEffects_Mutable().Add(FSlottedAbilityEffects(AbilityId, ApplyGameplayEffectToSelf(Def, ParentPOTCharacter->GetGrowthLevel(), Context)));
					}
				}
			}
		}
		
	}

	UpdateStanceEffect();

	if (!UIGameplayStatics::IsGrowthEnabled(this) || bPreviewOnly)
	{
		AddLooseGameplayTag(UPOTAbilitySystemGlobals::Get().NoGrowTag);
	}

	if (PassiveGrowthEffect != nullptr && UIGameplayStatics::IsGrowthEnabled(this))
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);

		AIGameSession* Session = Cast<AIGameSession>(IGameInstance->GetGameSession());

		if (Session != nullptr && Session->GlobalPassiveGrowthPerMinute > 0.f)
		{
			FGameplayEffectSpecHandle PassiveGrowthSpec = MakeOutgoingSpec(PassiveGrowthEffect, 1, MakeEffectContext());
			PassiveGrowthSpec.Data->SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_SetByCallerGrowthPerSecond), Session->GlobalPassiveGrowthPerMinute / 60.f);
			ApplyGameplayEffectSpecToSelf(*PassiveGrowthSpec.Data);
		}
	}
}

void UPOTAbilitySystemComponent::SetupAttributeDelegates()
{
	if (bHasBoundAttributeDelegates)
	{
		return;
	}

	FOnGameplayAttributeValueChange& AttributeChangedDelegate = GetGameplayAttributeValueChangeDelegate(UCoreAttributeSet::GetGrowthAttribute());
	AttributeChangedDelegate.AddUObject(this, &UPOTAbilitySystemComponent::OnGrowthChanged);

	FOnGameplayAttributeValueChange& StaminaAttributeChangedDelegate = GetGameplayAttributeValueChangeDelegate(UCoreAttributeSet::GetStaminaAttribute());
	StaminaAttributeChangedDelegate.AddUObject(this, &UPOTAbilitySystemComponent::OnStaminaChanged);

	bHasBoundAttributeDelegates = true;
}

void UPOTAbilitySystemComponent::InitAttributes(bool bInitialInit /* = true */)
{
	if (!ParentPOTCharacter)
	{
		ParentPOTCharacter = Cast<AIBaseCharacter>(GetOwner());
	}
	
	Super::InitAttributes(bInitialInit);
}

void UPOTAbilitySystemComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (OldLocation.IsZero())
	{
		OldLocation = GetOwner()->GetActorLocation();
	}

	TrackedSpeed = ((GetOwner()->GetActorLocation() - OldLocation) / DeltaTime).Length();
	SmoothedTrackedSpeed += (TrackedSpeed - SmoothedTrackedSpeed) * FMath::Clamp(DeltaTime, 0, 1);
	OldLocation = GetOwner()->GetActorLocation();

	if (const UPOTGameplayAbility* ActiveAttackAbility = GetCurrentAttackAbility())
	{
		TimeInCurrentAttackAbility += DeltaTime;

		if (ActiveAttackAbility->AutoCancelMovementSpeedThreshold > 0.f)
		{
			//UE4 does not properly track velocity when it's being adjusted via GA so need to manually track
			const float SquaredThreshold = FMath::Square(ActiveAttackAbility->AutoCancelMovementSpeedThreshold);
			const float SquaredSpeed = FMath::Square(TrackedSpeed);

			if (SquaredSpeed <= SquaredThreshold)
			{
				TimeBelowThreshold += DeltaTime;
				if (TimeBelowThreshold >= ActiveAttackAbility->AutoCancelMovementSpeedDuration)
				{
					CancelAbility(ActiveAttackAbility->GetClass()->GetDefaultObject<UGameplayAbility>());
					TimeBelowThreshold = 0.f;
				}
			}
			else
			{
				TimeBelowThreshold = 0.f;
			}
		}
		
		/*if (TimeInCurrentAttackAbility > GetCurrentAttackAbility()->GetLastDoDamageNotifyEndTime()
			&& OnDamageFinished.IsBound()
			&& bCanFireDamageFinishedEvent)
		{
			OnDamageFinished.Broadcast(GetCurrentAttackAbility());
			bCanFireDamageFinishedEvent = false;
		}*/
	}
}

void UPOTAbilitySystemComponent::OnActiveGameplayEffectAddedCallback(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	if (!ensureAlways(GetOwner() && GetOwner()->HasAuthority()) ||
		!ActiveHandle.IsValid())
	{
		return;
	}

	const FActiveGameplayEffect* ActiveGameplayEffect = GetActiveGameplayEffect(ActiveHandle);
	if (!ActiveGameplayEffect)
	{
		return;
	}

	FGameplayTag BuffTag = FGameplayTag::RequestGameplayTag(NAME_Buff);
	bool ContainsBuffTag = ActiveGameplayEffect->Spec.Def->GetAssetTags().HasTag(BuffTag);

	float BuffDurationMultiplier = GetNumericAttribute(UCoreAttributeSet::GetBuffDurationMultiplierAttribute());

	//If the GE is a buff, and the buff multiplier makes a difference, calculate the new duration.
	if (ContainsBuffTag && BuffDurationMultiplier != 1.0f)
	{
		FActiveGameplayEffect* AGE = const_cast<FActiveGameplayEffect*>(ActiveGameplayEffect);

		if (BuffDurationMultiplier > 1.0f && AGE && AGE->Spec.GetDuration() > 0.0f && ContainsBuffTag)
		{
			AGE->Spec.Duration = AGE->Spec.GetDuration() * BuffDurationMultiplier;

			AGE->StartServerWorldTime = ActiveGameplayEffects.GetServerWorldTime();
			AGE->CachedStartServerWorldTime = AGE->StartServerWorldTime;
			AGE->StartWorldTime = ActiveGameplayEffects.GetWorldTime();
			ActiveGameplayEffects.MarkItemDirty(*AGE);
			ActiveGameplayEffects.CheckDuration(ActiveHandle);

			AGE->EventSet.OnTimeChanged.Broadcast(AGE->Handle, AGE->StartWorldTime, AGE->GetDuration());
			OnGameplayEffectDurationChange(*AGE);
		}
	}

	if (const FActiveGameplayEffect* ActiveEffect = ActiveGameplayEffects.GetActiveGameplayEffect(ActiveHandle))
	{
		if (const UPOTGameplayEffect* POTEffect = Cast<const UPOTGameplayEffect>(ActiveEffect->Spec.Def))
		{
			for (const TPair<FName, FBoneDamageModifier> KVP : POTEffect->BoneDamageMultipliers)
			{
				if (FBoneDamageModifier* const DinoBoneDamageMultiplier = BoneDamageMultiplier.Find(KVP.Key))
				{
					*DinoBoneDamageMultiplier *= KVP.Value;
				}
				else
				{
					UE_LOG(TitansLog, Warning, TEXT("GameplayEffect \"%s\" attempted to alter damage multiplier for bone name \"%s\" which does not exist on character \"%s\"!"), *POTEffect->GetFName().ToString(), *KVP.Key.ToString(), *ParentPOTCharacter->GetName());
				}
			}
		}
	}
}

int32 UPOTAbilitySystemComponent::GetLevel() const
{
	return FMath::FloorToInt(GetLevelFloat());
}

float UPOTAbilitySystemComponent::GetLevelFloat() const
{
	if (!ParentPOTCharacter)
	{
		return 1.f;
	}

	return FMath::GetMappedRangeValueClamped(TRange<float>(0.f, 1.f), TRange<float>(1.f, 5.f), ParentPOTCharacter->GetGrowthPercent());
}

void UPOTAbilitySystemComponent::GetActiveAbilitiesWithTag(const FGameplayTagContainer& GameplayTagContainer, 
	TArray<UPOTGameplayAbility*>& ActiveAbilities)
{
	TArray<FGameplayAbilitySpec*> AbilitiesToActivate;
	GetActivatableGameplayAbilitySpecsByAllMatchingTags(GameplayTagContainer, AbilitiesToActivate, false);

	// Iterate the list of all ability specs
	for (FGameplayAbilitySpec* Spec : AbilitiesToActivate)
	{
		// Iterate all instances on this ability spec
		TArray<UGameplayAbility*> AbilityInstances = Spec->GetAbilityInstances();

		for (UGameplayAbility* ActiveAbility : AbilityInstances)
		{
			ActiveAbilities.Add(Cast<UPOTGameplayAbility>(ActiveAbility));
		}
	}
}

void UPOTAbilitySystemComponent::RefreshPassiveAbilities()
{
	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		UPOTGameplayAbility* const POTGameplayAbilityCDO = Cast<UPOTGameplayAbility>(Spec.Ability);
		if (!POTGameplayAbilityCDO || !POTGameplayAbilityCDO->IsPassive())
		{
			continue;
		}

		const TArray<UGameplayAbility*> AbilityInstances = Spec.GetAbilityInstances();		
		const bool bIsAbilityActive = [&](){
			for (const UGameplayAbility* AbilityInstance : AbilityInstances)
			{
				if (AbilityInstance && AbilityInstance->IsActive())
				{
					return true;
				}
			}
			return false;
		}();
		const bool bShouldBeActive = CanActivateAbility(Spec.Ability->GetClass());
		
		if (bIsAbilityActive == bShouldBeActive)
		{
			continue;
		}

		if (bIsAbilityActive)
		{
			// Cancel
			CancelAbility(POTGameplayAbilityCDO);
		}
		else
		{
			// Activate
			if (!TryActivateAbility(Spec.Handle))
			{
				UE_LOG(LogTemp, Warning, TEXT("UPOTAbilitySystemComponent::RefreshPassiveAbilities: Passive Ability Failed Activation! %s"), *POTGameplayAbilityCDO->GetName());
			}
		}
	}
}

void UPOTAbilitySystemComponent::OnGameplayEffectAppliedToSelf(UAbilitySystemComponent* Target, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveHandle)
{
	Super::OnGameplayEffectAppliedToSelf(Target, SpecApplied, ActiveHandle);

	RefreshPassiveAbilities();
}

void UPOTAbilitySystemComponent::OnActiveEffectRemoved(const FActiveGameplayEffect& ActiveEffect)
{
	Super::OnActiveEffectRemoved(ActiveEffect);

	RefreshPassiveAbilities();
}

void UPOTAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	//Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
	Super::InitAbilityActorInfo(InAvatarActor, InAvatarActor);
}

FName UPOTAbilitySystemComponent::GetAttributeName() const
{
	return ParentPOTCharacter != nullptr && ParentPOTCharacter->CharacterTag.IsValid()
		? UIGameplayStatics::GetTagLeafName(ParentPOTCharacter->CharacterTag)
		: Super::GetAttributeName();
}

void UPOTAbilitySystemComponent::SetOwnerCollision(UPrimitiveComponent* Component)
{
	OwnerCollision = Component;
}

void UPOTAbilitySystemComponent::SetOwnerSkeletalMesh(USkeletalMeshComponent* Mesh)
{
	OwnerMesh = Mesh;
}

void UPOTAbilitySystemComponent::StopMontage(UGameplayAbility* InAnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* Montage)
{
	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;

	if (!Montage || !AnimInstance) return;
	
	FMontageBlendSettings BlendSettings;
	BlendSettings.Blend = Montage->BlendOut;
	BlendSettings.BlendMode = Montage->BlendModeOut;
	BlendSettings.BlendProfile = Montage->BlendProfileOut;

	AnimInstance->Montage_StopWithBlendSettings(BlendSettings, Montage);

	if (IsOwnerActorAuthoritative())
	{
		bool bNeedsDirtying = false;
		for (int32 Index = GetAbilityMontages().Num() - 1; Index >= 0; Index--)
		{
			const FPOTGameplayAbilityRepAnimMontage& RepMontage = GetAbilityMontages()[Index];
			if (!RepMontage.InnerMontage.AnimMontage || RepMontage.InnerMontage.AnimMontage == Montage)
			{
				AbilityMontages.RemoveAt(Index);
				bNeedsDirtying = true;
			}
		}

		if (bNeedsDirtying)
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(UPOTAbilitySystemComponent, AbilityMontages, this);
		}

		UpdateReplicatedAbilityMontages(true);
	}
}


float UPOTAbilitySystemComponent::PlayMontage(UGameplayAbility* InAnimatingAbility, FGameplayAbilityActivationInfo ActivationInfo, UAnimMontage* NewAnimMontage, float InPlayRate, FName StartSectionName /*= NAME_None*/, float StartTimeSeconds)
{
	float Duration = -1.f;

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (AnimInstance && NewAnimMontage)
	{
		Duration = AnimInstance->Montage_Play(NewAnimMontage, InPlayRate, EMontagePlayReturnType::MontageLength, StartTimeSeconds, false);

		UE_LOG(LogTemp, Warning, TEXT("Play Montage: %s %s"), *NewAnimMontage->GetFName().ToString(), *InAnimatingAbility->GetFName().ToString());

		if (Duration > 0.f)
		{
			// Start at a given Section.
			if (StartSectionName != NAME_None)
			{
				AnimInstance->Montage_JumpToSection(StartSectionName, NewAnimMontage);
			}

			// Custom ability montage replication. Default UAbilitySystemComponent only supports 1 active ability montage.
			if (IsOwnerActorAuthoritative())
			{
				FPOTGameplayAbilityRepAnimMontage NewRepMontage = FPOTGameplayAbilityRepAnimMontage::Generate();
				NewRepMontage.InnerMontage.AnimMontage = NewAnimMontage;
				NewRepMontage.InnerMontage.PlayRate = InPlayRate;
				NewRepMontage.InnerMontage.PlayInstanceId = 0;
				NewRepMontage.InnerMontage.SectionIdToPlay = 0;
				NewRepMontage.InnerMontage.bRepPosition = true;
				if (NewRepMontage.InnerMontage.AnimMontage && StartSectionName != NAME_None)
				{
					// we add one so INDEX_NONE can be used in the on rep
					NewRepMontage.InnerMontage.SectionIdToPlay = NewRepMontage.InnerMontage.AnimMontage->GetSectionIndex(StartSectionName) + 1;
				}
				
				GetAbilityMontages_Mutable().Add(NewRepMontage);
				UpdateReplicatedAbilityMontages(true);		
			}
			else
			{
				// If this prediction key is rejected, we need to end the preview
				/*FPredictionKey PredictionKey = GetPredictionKeyForNewAction();
				if (PredictionKey.IsValidKey())
				{
					PredictionKey.NewRejectedDelegate().BindUObject(this, &UPOTAbilitySystemComponent::OnPredictiveMontageRejected, NewAnimMontage);
				}*/
			}
		}
	}

	return Duration;
}

UAnimMontage* UPOTAbilitySystemComponent::GetCurrentAttackMontage() const
{
	if (CurrentAttackAbility)
	{
		return CurrentAttackAbility->GetCurrentMontage();
	}
	return nullptr;
}

bool UPOTAbilitySystemComponent::CurrentAbilityIsExclusive() const
{
	if (CurrentAttackAbility)
	{
		return CurrentAttackAbility->bIsExclusive;
	}
	return false;
}

bool UPOTAbilitySystemComponent::CurrentAbilityExists() const
{	
	if (CurrentAttackAbility)
	{
		return true;
	}
	return false;
}

TArray<UAnimMontage*> UPOTAbilitySystemComponent::GetActiveMontages() const
{
	TArray<UAnimMontage*> Montages;
	Montages.Reserve(GetAbilityMontages().Num());
	
	for (int32 Index = GetAbilityMontages().Num() - 1; Index >= 0; Index--)
	{
		const FPOTGameplayAbilityRepAnimMontage& RepMontage = GetAbilityMontages()[Index];
		if (RepMontage.InnerMontage.AnimMontage)
		{
			Montages.Add(RepMontage.InnerMontage.AnimMontage);
		}
	}
	
	return Montages;
}

void UPOTAbilitySystemComponent::UpdateReplicatedAbilityMontages(bool bForceNetUpdate, UAnimMontage* MontageToUpdate)
{
	if (!IsOwnerActorAuthoritative()) return;

	UpdateShouldTick();

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (!ensure(AnimInstance)) return;

	bool bNeedNetUpdate = bForceNetUpdate;

	TArray<FPOTGameplayAbilityRepAnimMontage>& MutableAbilityMontages = GetAbilityMontages_Mutable();
	for (int32 Index = MutableAbilityMontages.Num() - 1; Index >= 0; Index --)
	{
		FPOTGameplayAbilityRepAnimMontage& RepMontage = MutableAbilityMontages[Index];
		if (!RepMontage.InnerMontage.AnimMontage)
		{
			MutableAbilityMontages.RemoveAt(Index);
			continue;
		}
		
		if (RepMontage.InnerMontage.AnimMontage == MontageToUpdate)
		{
			// Regenerate this to get a new unique id for replication
			FPOTGameplayAbilityRepAnimMontage NewMontage = FPOTGameplayAbilityRepAnimMontage::Generate();
			NewMontage.InnerMontage = RepMontage.InnerMontage;
			RepMontage = NewMontage;
		}

		const bool bIsStopped = AnimInstance->Montage_GetIsStopped(RepMontage.InnerMontage.AnimMontage);

		if (RepMontage.InnerMontage.IsStopped != bIsStopped)
		{
			RepMontage.InnerMontage.IsStopped = bIsStopped;
			bNeedNetUpdate = true;
		}

		if (!bIsStopped)
		{
			RepMontage.InnerMontage.PlayRate = AnimInstance->Montage_GetPlayRate(RepMontage.InnerMontage.AnimMontage);
			RepMontage.InnerMontage.Position = AnimInstance->Montage_GetPosition(RepMontage.InnerMontage.AnimMontage);
			RepMontage.InnerMontage.BlendTime = AnimInstance->Montage_GetBlendTime(RepMontage.InnerMontage.AnimMontage);
		}
		else
		{
			MutableAbilityMontages.RemoveAt(Index);
		}
	}

	if (bNeedNetUpdate && AbilityActorInfo->AvatarActor != nullptr)
	{
		AbilityActorInfo->AvatarActor->ForceNetUpdate();
	}
}

bool UPOTAbilitySystemComponent::GetShouldTick() const
{
	if (!AbilityActorInfo.Get()) return false;
	return AbilityActorInfo->IsLocallyControlled() || AbilityActorInfo->IsNetAuthority();
}

void UPOTAbilitySystemComponent::OnRep_AbilityMontages()
{
	if (AbilityActorInfo->IsLocallyControlled() || AbilityActorInfo->IsNetAuthority()) return;

	UpdateShouldTick();

	UAnimInstance* AnimInstance = AbilityActorInfo.IsValid() ? AbilityActorInfo->GetAnimInstance() : nullptr;
	if (!ensure(AnimInstance)) return;

	TArray<UAnimMontage*> ActiveMontages{};

	for (const FPOTGameplayAbilityRepAnimMontage& Montage : GetAbilityMontages())
	{
		if (!IsValid(Montage.InnerMontage.AnimMontage)) continue;

		if (AnimInstance->Montage_IsPlaying(Montage.InnerMontage.AnimMontage))
		{
			AnimInstance->Montage_SetPosition(Montage.InnerMontage.AnimMontage, Montage.InnerMontage.Position);
			AnimInstance->Montage_SetPlayRate(Montage.InnerMontage.AnimMontage, Montage.InnerMontage.PlayRate);
		}
		else
		{
			AnimInstance->Montage_Play(Montage.InnerMontage.AnimMontage, Montage.InnerMontage.PlayRate, EMontagePlayReturnType::MontageLength, Montage.InnerMontage.Position, false);
		}

		ActiveMontages.Add(Montage.InnerMontage.AnimMontage);
	}

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponent::OnRep_AbilityMontages(): Recieved new AbilityMontages. %i incoming, %i active."), GetAbilityMontages().Num(), ActiveMontages.Num());
#endif

	for (FAnimMontageInstance* Instance : AnimInstance->MontageInstances)
	{
		if (!Instance) continue;
		UAnimMontage* InstanceMontage = Instance->Montage.Get();
		if (!InstanceMontage) continue;

		// Need to manually stop any animations that are no longer in the replicated array, otherwise stopping a montage early would not happen on a remote client.
		if (!ActiveMontages.Contains(InstanceMontage) && !Instance->IsStopped())
		{
#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
			UE_LOG(TitansLog, Log, TEXT("UPOTAbilitySystemComponent::OnRep_AbilityMontages(): Stopped stale Ability Montage %s."), *InstanceMontage->GetName());
#endif
			AnimInstance->Montage_Stop(.25f, InstanceMontage);
		}
	}
}

void UPOTAbilitySystemComponent::NotifyAbilityEnded(FGameplayAbilitySpecHandle Handle, UGameplayAbility* Ability, bool bWasCancelled)
{
	Super::NotifyAbilityEnded(Handle, Ability, bWasCancelled);
	if (Ability == CurrentAttackAbility)
	{
		OnAttackAbilityEnd.Broadcast(CurrentAttackAbility, bWasCancelled);
		//UE_LOG(LogTemp, Log, TEXT("%s : %s: CurrentAttackAbilityClear - NotifyAbilityEnded"), *GetOwner()->GetName(), *Ability->GetName());
		CurrentAttackAbility = nullptr;

		SetCurrentAttackAbilityUsesCharge(false);
	}
	//else
	//{
	//	UE_LOG(LogTemp, Log, TEXT("%s : %s: Ability != CurrentAttackAbility"), *GetOwner()->GetName(), *Ability->GetName());
	//}
}

void UPOTAbilitySystemComponent::CancelCurrentAttackAbility()
{
	if (UPOTGameplayAbility* CurrentAbility = GetCurrentAttackAbility())
	{
		CurrentAbility->SetCanBeCanceled(true);

		FGameplayAbilitySpecHandle Handle = CurrentAbility->GetCurrentAbilitySpecHandle();
		FGameplayAbilityActorInfo ActorInfo = CurrentAbility->GetActorInfo();
		FGameplayAbilityActivationInfo ActivationInfo = CurrentAbility->GetCurrentActivationInfo();
		CurrentAbility->CancelAbility(
			Handle,
			&ActorInfo,
			ActivationInfo,
			/*bReplicateCancelAbility =*/ GetOwner()->HasAuthority()
		);
	}
}

bool UPOTAbilitySystemComponent::GetDamageMultiplierForBone(const FName& BoneName, FBoneDamageModifier& OutDamageModifier) const
{
	if (BoneName == NAME_None)
	{
		OutDamageModifier = FBoneDamageModifier();
		return true;
	}

	if (const FBoneDamageModifier* Multiplier = BoneDamageMultiplier.Find(BoneName))
	{
		OutDamageModifier = *Multiplier;
		return true;
	}
	
	return false;

}

void UPOTAbilitySystemComponent::UpdateStanceEffect()
{
	if (!IsAlive())
	{
		return;
	}

	if (ParentPOTCharacter != nullptr && StateEffects.IsValid())
	{
		EStanceType CurrentStance = ParentPOTCharacter->GetRestingStance();

		if (StanceStateEffect.IsValid())
		{
			RemoveActiveGameplayEffect(StanceStateEffect);
		}

		if (USkeletalMeshComponent* SkeleMesh = ParentPOTCharacter->GetMesh())
		{
			if (UDinosaurAnimBlueprint* DinoAnimBlueprint = Cast<UDinosaurAnimBlueprint>(SkeleMesh->GetAnimInstance()))
			{
				if (DinoAnimBlueprint->IsTransitioning()) return;
			}
		}
		
		if (CurrentStance != EStanceType::Default)
		{
			UGameplayEffect* GameplayEffect = CurrentStance == EStanceType::Resting ? StateEffects.Resting->GetDefaultObject<UGameplayEffect>() : StateEffects.Sleeping->GetDefaultObject<UGameplayEffect>();

			FGameplayEffectSpec GESpec(GameplayEffect, MakeEffectContext(), GetLevelFloat());
			StanceStateEffect = ApplyGameplayEffectSpecToSelf(GESpec);
		}
	}
}

void UPOTAbilitySystemComponent::UpdateSkinMeshEffect()
{
	if (MeshEffects.Num() == 0)
	{
		return;
	}

	if (AIDinosaurCharacter* DinoChar = Cast<AIDinosaurCharacter>(ParentPOTCharacter))
	{
		if (MeshEffects.IsValidIndex(DinoChar->GetSkinData().MeshIndex))
		{
			if (SkinMeshEffect.IsValid())
			{
				const UGameplayEffect* ExistingEffect = GetGameplayEffectDefForHandle(SkinMeshEffect);
				if (ExistingEffect != nullptr && ExistingEffect->GetClass() != MeshEffects[DinoChar->GetSkinData().MeshIndex])
				{
					RemoveActiveGameplayEffect(SkinMeshEffect);
					SkinMeshEffect.Invalidate();
				}


			}

			if (!SkinMeshEffect.IsValid() && MeshEffects[DinoChar->GetSkinData().MeshIndex] != nullptr)
			{
				FGameplayEffectSpec GESpec(MeshEffects[DinoChar->GetSkinData().MeshIndex]->GetDefaultObject<UGameplayEffect>(), MakeEffectContext(), GetLevelFloat());
				SkinMeshEffect = ApplyGameplayEffectSpecToSelf(GESpec);
			}
			
			
		}
		else if (SkinMeshEffect.IsValid())
		{
			RemoveActiveGameplayEffect(SkinMeshEffect);
		}
	}

	
}

bool UPOTAbilitySystemComponent::GetAggregatedTags(FGameplayTagContainer& OutTags) const
{
	GetOwnedGameplayTags(OutTags);

	return true;
}

void UPOTAbilitySystemComponent::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	Super::GetOwnedGameplayTags(TagContainer);

	if (ParentPOTCharacter != nullptr)
	{
		// Need to add character tags for ability system tag checks (Ability Blocked/Required Tags, etc.) to consider them
		TagContainer.AppendTags(ParentPOTCharacter->CharacterTags);
	}
}

bool UPOTAbilitySystemComponent::HasReplicatedLooseGameplayTag(const FGameplayTag& Tag)
{
	for (TPair<FGameplayTag, int32> Pair : GetReplicatedLooseTags().TagMap)
	{
		if (Pair.Key == Tag)
		{
			return true;
		}
	}
	return false;

}


void UPOTAbilitySystemComponent::ClearConsequentAttacks(const bool bClearTotal)
{
	ConsequentAttacks = 0;
	if (bClearTotal)
	{
		TotalConsequentAttacks = 0;
	}
}


void UPOTAbilitySystemComponent::SetAbilityInput(FGameplayAbilitySpec& Spec, bool bPressed)
{
	if (!bPressed)
	{
		if (Spec.Ability && Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
		{
			ServerSetInputReleased(Spec.Handle);
		}

		AbilitySpecInputReleased(Spec);

		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, Spec.Handle,
			Spec.ActivationInfo.GetActivationPredictionKey());
		// UE_LOG(LogTemp, Log, TEXT("HandleInputChange RELEASE"));
	}
	else
	{
		if (Spec.Ability && Spec.Ability->bReplicateInputDirectly && IsOwnerActorAuthoritative() == false)
		{
			ServerSetInputPressed(Spec.Handle);
		}
		AbilitySpecInputPressed(Spec);

		InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, Spec.Handle,
			Spec.ActivationInfo.GetActivationPredictionKey());
		// UE_LOG(LogTemp, Log, TEXT("HandleInputChange PRESS"));
	}
}

float UPOTAbilitySystemComponent::GetCurrentMoveMontageRateScale() const
{
	return CurrentAttackAbility == nullptr || CurrentAttackAbility->GetCurrentMontage() == nullptr ? 
		1.f : CurrentAttackAbility->GetCurrentMontage()->RateScale * CurrentAttackAbility->GetPlayRate();
}


float UPOTAbilitySystemComponent::GetMontageTimeInCurrentMove() const
{
	//UE_LOG(LogTemp, Log, TEXT("Current move time: %f - %f"), GetTimeInCurrentAttackAbility(), GetCurrentMoveMontageRateScale());
	//return GetTimeInCurrentAttackAbility() * GetCurrentMoveMontageRateScale();


	UAnimInstance* OwnerAnimInstance = GetOwnerSkeletalMesh()->GetAnimInstance();
	return OwnerAnimInstance->Montage_GetPosition(CurrentAttackAbility->GetCurrentMontage());
}

void UPOTAbilitySystemComponent::OnOwnerMovementModeChanged(class UCharacterMovementComponent* CharMovement, EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{

}

void UPOTAbilitySystemComponent::ExecuteEffectClientPredicted(FGameplayEffectSpec& Spec)
{
	check(Spec.GetDuration() == UGameplayEffect::INSTANT_APPLICATION);
	if (Spec.GetDuration() != UGameplayEffect::INSTANT_APPLICATION)
	{
		return;
	}

	ActiveGameplayEffects.ExecuteActiveEffectsFrom(Spec, ScopedPredictionKey);

}

FGameplayEffectContextHandle UPOTAbilitySystemComponent::MakeEffectContext() const
{
	if (CurrentAttackAbility != nullptr)
	{
		// By default use the owner and avatar as the instigator and causer
		check(AbilityActorInfo.IsValid());

		FPOTGameplayEffectContext* WaEffectContext = StaticCast<FPOTGameplayEffectContext*>(UPOTAbilitySystemGlobals::Get().AllocGameplayEffectContext());
		WaEffectContext->AddInstigator(AbilityActorInfo->OwnerActor.Get(), AbilityActorInfo->AvatarActor.Get());
	
		
		//#TODO: Initialize more properties for context if possible
		return FGameplayEffectContextHandle(WaEffectContext);

	}
	return Super::MakeEffectContext();
}

void UPOTAbilitySystemComponent::PostDeath()
{
	Super::PostDeath();

	if (USkeletalMeshComponent* SKM = GetOwnerSkeletalMesh())
	{
		SKM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UPrimitiveComponent*  OwnerCol = GetOwnerCollision())
	{
		OwnerCol->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OwnerCol->SetEnableGravity(false);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	for(const FActiveGameplayEffectHandle& ActiveHandle : ActiveGameplayEffects.GetAllActiveEffectHandles())
	{
		RemoveActiveGameplayEffect(ActiveHandle);
	}

	DestroyActiveState();

	if (GetOwner()->HasAuthority())
	{
		ClearAllAbilities();
	}
	
}

void UPOTAbilitySystemComponent::BeginPlay()
{
	Super::BeginPlay();

	//InitAbilityActorInfo(ParentWaCharacter, ParentWaCharacter);
}

void UPOTAbilitySystemComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UPOTAbilitySystemComponent::InitializeComponent()
{
	Super::InitializeComponent();

	ParentPOTCharacter = Cast<AIBaseCharacter>(GetOwner());
	check(ParentPOTCharacter);
	
	SetOwnerSkeletalMesh(ParentPOTCharacter->GetMesh());
	SetOwnerCollision(ParentPOTCharacter->GetCapsuleComponent());
	
}

void UPOTAbilitySystemComponent::AddGameplayTagToOwner(const FGameplayTag& InTag, const bool bFast /*= false*/)
{
	check(ParentPOTCharacter && InTag.IsValid());

	if (bFast)
	{
		ParentPOTCharacter->CharacterTags.AddTagFast(InTag);
	}
	else
	{
		ParentPOTCharacter->CharacterTags.AddTag(InTag);
	}
	
}

void UPOTAbilitySystemComponent::RemoveGameplayTagFromOwner(const FGameplayTag& InTag)
{
	check(ParentPOTCharacter && InTag.IsValid());
	ParentPOTCharacter->CharacterTags.RemoveTag(InTag);
}

void UPOTAbilitySystemComponent::SetCurrentAttackAbilityUsesCharge(bool bShouldUseChargeDuration)
{	
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(UPOTAbilitySystemComponent, bCurrentAttackAbilityUsesCharge, bShouldUseChargeDuration, this);
}

bool UPOTAbilitySystemComponent::GetCurrentAttackAbilityUsesCharge() const
{
	return bCurrentAttackAbilityUsesCharge;
}

void UPOTAbilitySystemComponent::SetCurrentAttackingAbility(UPOTGameplayAbility* Ability)
{
	//We should _technically_ allow multiple attacks to work in tandem but for now clear any existing attack
	if (CurrentAttackAbility != nullptr)
	{
		CancelAbility(CurrentAttackAbility->GetClass()->GetDefaultObject<UGameplayAbility>());
		//CurrentAttackAbility->CancelAbility()
	}

	TimeInCurrentAttackAbility = 0.f;

	if (GetOwnerActor()->HasAuthority() && GetNetMode() != NM_Standalone)
	{
		if (Ability != nullptr)
		{
			SetCurrentAttackAbilityUsesCharge(Ability->bShouldUseChargeDuration);
		}
		else
		{
			SetCurrentAttackAbilityUsesCharge(false);
		}
	}
	
	CurrentAttackAbility = Ability;
	OnAttackAbilityStart.Broadcast(CurrentAttackAbility, false);

	bCanFireDamageFinishedEvent = true;
}

float UPOTAbilitySystemComponent::K2_GetNextAttributeChangeTime(const FGameplayAttribute Attribute) const
{
	return GetNextAttributeChangeTime(Attribute);
}

float UPOTAbilitySystemComponent::GetNextAttributeChangeTime(const FGameplayAttribute& Attribute) const
{
	float NextPeriod, Duration;
	FGameplayEffectQuery Query;
	Query.ModifyingAttribute = Attribute;

	if (GetActiveEffectsNextPeriodAndDuration(Query, NextPeriod, Duration))
	{
		return NextPeriod;
	}

	return -1.f;
}

float UPOTAbilitySystemComponent::GetActiveEffectLevel(FActiveGameplayEffectHandle ActiveHandle) const
{
	const FActiveGameplayEffect* Effect = ActiveGameplayEffects.GetActiveGameplayEffect(ActiveHandle);
	if (Effect != nullptr)
	{
		return Effect->Spec.GetLevel();
	}
	return -1;
}

FGameplayTagContainer UPOTAbilitySystemComponent::K2_AddEventTagToIgnoreList(FGameplayTag EventTag)
{
	return AddEventTagToIgnoreList(EventTag);
}

FGameplayTagContainer UPOTAbilitySystemComponent::AddEventTagToIgnoreList(const FGameplayTag& EventTag)
{
	if (EventTag.IsValid())
	{
		EventsToIgnore.AddTag(EventTag);
	}
	return EventsToIgnore;
}

void UPOTAbilitySystemComponent::K2_RemoveEventTagFromIgnoreList(FGameplayTag EventTag)
{
	RemoveEventTagFromIgnoreList(EventTag);
}

void UPOTAbilitySystemComponent::RemoveEventTagFromIgnoreList(const FGameplayTag& EventTag)
{
	if (EventTag.IsValid())
	{
		EventsToIgnore.RemoveTag(EventTag);
	}
}

bool UPOTAbilitySystemComponent::GetActiveEffectsNextPeriodAndDuration(const FGameplayEffectQuery& Query, 
	float& NextPeriod, float& Duration) const
{
	const TArray<FActiveGameplayEffectHandle> ActiveEffects = GetActiveEffects(Query);
	
	bool bFoundSomething = false;
	float MinPeriod = TNumericLimits<float>::Max();
	float MaxEndTime = -1.f;

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	FTimerManager& WTM = World->GetTimerManager();

	for (const FActiveGameplayEffectHandle& Handle : ActiveEffects)
	{
		const FActiveGameplayEffect& Effect = *ActiveGameplayEffects.GetActiveGameplayEffect(Handle);
		if (!Query.Matches(Effect))
		{
			continue;
		}

		float ThisEndTime = Effect.GetEndTime();

		float ThisPeriod = WTM.GetTimerRemaining(Effect.PeriodHandle);
		if (ThisPeriod <= UGameplayEffect::INFINITE_DURATION)
		{
			// This effect has no period, check how long it has remaining
			float ThisTimeRemaining = Effect.GetTimeRemaining(World->GetTimeSeconds());
			if (ThisTimeRemaining <= UGameplayEffect::INFINITE_DURATION)
			{
				//It's neither period nor has a duration, not interested.
				continue;
			}

			bFoundSomething = true;
			MinPeriod = FMath::Min(ThisTimeRemaining, MinPeriod);
		}
		else
		{
			bFoundSomething = true;
			MinPeriod = FMath::Min(ThisPeriod, MinPeriod);
		}

		if (ThisEndTime > MaxEndTime)
		{
			MaxEndTime = ThisEndTime;
			Duration = Effect.GetDuration();
		}

	}

	NextPeriod = MinPeriod;

	return bFoundSomething;
}

void UPOTAbilitySystemComponent::OnGrowthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue != ChangeData.OldValue)
	{
		InitAttributes(false);

	}
}

void UPOTAbilitySystemComponent::OnStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue == ChangeData.OldValue)
	{
		return;
	}

	const FGameplayTag ExhaustedTag = FGameplayTag::RequestGameplayTag(TEXT("Debuff.StaminaExhausted"));
	const bool HasTag = HasMatchingGameplayTag(ExhaustedTag);

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		if (HasTag && !GetActiveEffect(StaminaExhaustedEffect).Handle.IsValid())
		{
			// client: Effect has been removed, but the tag is still there (bug in the GAS, issue 4064)
			SetLooseGameplayTagCount(ExhaustedTag, 0);
			const bool HasMatchingAfterRemove = HasMatchingGameplayTag(ExhaustedTag);
			UE_LOG(TitansLog, Display, TEXT("UPOTAbilitySystemComponent::OnStaminaChanged - CLIENT - REMOVE LOOSE - Before / After %d / %d"), HasTag, HasMatchingAfterRemove);
		}
		return;
	}

	//Server
	if (!IsExhausted())
	{
		if (ChangeData.NewValue == 0.f && ChangeData.OldValue > 0.f && !HasTag)
		{
			FGameplayEffectSpec ExhaustionSpec(StaminaExhaustedEffect->GetDefaultObject<UGameplayEffect>(), MakeEffectContext(), GetLevelFloat());
			ExhaustionEffect = ApplyGameplayEffectSpecToSelf(ExhaustionSpec);
			check(ExhaustionEffect.WasSuccessfullyApplied());
		}
	}
	else if (ExhaustionEffect.IsValid())
	{
		const float MaxStamina = GetNumericAttribute(UCoreAttributeSet::GetMaxStaminaAttribute());
		const float RequiredStamina = MaxStamina * 0.05f;

		if (ChangeData.NewValue > RequiredStamina)
		{
			RemoveActiveGameplayEffect(ExhaustionEffect);
			ExhaustionEffect.Invalidate();
		}
	}
}


bool UPOTAbilitySystemComponent::IsExhausted() const
{
	if (!ExhaustionEffect.IsValid()) return false;
	const FActiveGameplayEffect* Effect = ActiveGameplayEffects.GetActiveGameplayEffect(ExhaustionEffect);
	return Effect != nullptr;
}


void UPOTAbilitySystemComponent::UpdateLocomotionEffects()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateMovementEffects);

	if (!IsAlive())
	{
		return;
	}

	if (!ParentPOTCharacter)
	{
		return;
	}

	if (!StateEffects.IsValid())
	{
		return;
	}

	check(ParentPOTCharacter->HasAuthority());

	if (UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(ParentPOTCharacter->GetCharacterMovement()))
	{
		ELocomotionState NewState = ELocomotionState::LS_IDLE;

		const FAttachTarget& AttachTarget = ParentPOTCharacter->GetAttachTarget();

		if (AttachTarget.IsValid())
		{
			const APhysicsVolume* const PhysVolume = CharMove->GetPhysicsVolume();
			const bool bInWaterVolume = PhysVolume && PhysVolume->bWaterVolume;
			const bool bUnderwater = bInWaterVolume && !ParentPOTCharacter->IsAtWaterSurface();

			switch (AttachTarget.AttachType)
			{
				case EAttachType::AT_Latch:
					if (bUnderwater)
					{
						NewState = ELocomotionState::LS_LATCHEDUNDERWATER;
					}
					else
					{
						NewState = ELocomotionState::LS_LATCHED;
					}
					break;
				case EAttachType::AT_Carry:
					if (bUnderwater)
					{
						NewState = ELocomotionState::LS_CARRIEDUNDERWATER;
					}
					else
					{
						NewState = ELocomotionState::LS_CARRIED;
					}
					break;
				default:
					NewState = ELocomotionState::LS_IDLE;
					break;
			}
		}
		else if (ParentPOTCharacter->IsJumpProvidingForce()) // Jump
		{
			NewState = ELocomotionState::LS_JUMPING;
		}
		else
		{
			if (CharMove->IsSwimming())
			{
				bool bIsDiving = !CharMove->IsAtWaterSurface();
				if (CharMove->IsSprinting())
				{
					NewState = CharMove->IsAtWaterSurface() ? ELocomotionState::LS_FASTSWIMMING : ELocomotionState::LS_FASTDIVING;
				}
				else
				{
					if (CharMove->IsTrotting() && ParentPOTCharacter->IsAquatic() && StateEffects.TrotSwimming && StateEffects.TrotDiving)
					{
						NewState = CharMove->IsAtWaterSurface() ? ELocomotionState::LS_TROTSWIMMING : ELocomotionState::LS_TROTDIVING;
					}
					else
					{
						NewState = CharMove->IsAtWaterSurface() ? ELocomotionState::LS_SWIMMING : ELocomotionState::LS_DIVING;
					}
				}
			}
			else if (CharMove->IsFlying())
			{
				if (CharMove->IsSprinting())
				{
					NewState = ELocomotionState::LS_FASTFLYING;
				}
				else
				{
					NewState = ELocomotionState::LS_FLYING;
				}
			}
			else if (CharMove->IsCrouching())
			{
				NewState = CharMove->Velocity.SizeSquared() >= 20.f ? ELocomotionState::LS_CROUCHWALKING : ELocomotionState::LS_CROUCHING;
			}
			else if (CharMove->IsMoving())
			{
				AIDinosaurCharacter* DinoChar = Cast<AIDinosaurCharacter>(ParentPOTCharacter);
				if (CharMove->IsSprinting() && DinoChar && CharMove->Velocity.Length() > DinoChar->GetTrotSpeedThreshold(CharMove))
				{
					NewState = ELocomotionState::LS_SPRINTING;
				}
				else if (CharMove->IsTrotting())
				{
					NewState = ELocomotionState::LS_TROTTING;
				}
				else
				{
					NewState = ELocomotionState::LS_WALKING;
				}
			}
		}

		if (CurrentLocomotionState == NewState)
		{
			if (LocomotionStateEffect.IsValid()) // Only return if the current effect is valid
			{
				return;
			}
		}

		CurrentLocomotionState = NewState;

		UGameplayEffect* GameplayEffect = nullptr;
		UGameplayEffect* GameplayEffect_Carry = nullptr;
		switch (CurrentLocomotionState)
		{
			case ELocomotionState::LS_IDLE:
			{
				GameplayEffect = (StateEffects.Standing) ? StateEffects.Standing->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_WALKING:
			{
				GameplayEffect = (StateEffects.Walking) ? StateEffects.Walking->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_SPRINTING:
			{
				GameplayEffect = (StateEffects.Sprinting) ? StateEffects.Sprinting->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_TROTTING:
			{
				GameplayEffect = (StateEffects.Trotting) ? StateEffects.Trotting->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_SWIMMING:
			{
				GameplayEffect = (StateEffects.Swimming) ? StateEffects.Swimming->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_TROTSWIMMING:
			{
				GameplayEffect = (StateEffects.TrotSwimming) ? StateEffects.TrotSwimming->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_FASTSWIMMING:
			{
				GameplayEffect = (StateEffects.FastSwimming) ? StateEffects.FastSwimming->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_DIVING:
			{
				GameplayEffect = (StateEffects.Diving) ? StateEffects.Diving->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_TROTDIVING:
			{
				GameplayEffect = (StateEffects.TrotDiving) ? StateEffects.TrotDiving->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_FASTDIVING:
			{
				GameplayEffect = (StateEffects.FastDiving) ? StateEffects.FastDiving->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_CROUCHING: 
			{
				GameplayEffect = (StateEffects.Crouching) ? StateEffects.Crouching->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_CROUCHWALKING:
			{
				GameplayEffect = (StateEffects.CrouchWalking) ? StateEffects.CrouchWalking->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_JUMPING:
			{
				GameplayEffect = (StateEffects.Jumping) ? StateEffects.Jumping->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_FLYING:
			{
				GameplayEffect = (StateEffects.Flying) ? StateEffects.Flying->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_FASTFLYING:
			{
				GameplayEffect = (StateEffects.FastFlying) ? StateEffects.FastFlying->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_LATCHED:
			{
				GameplayEffect = (StateEffects.Latched) ? StateEffects.Latched->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_LATCHEDUNDERWATER:
			{
				GameplayEffect = (StateEffects.LatchedUnderwater) ? StateEffects.LatchedUnderwater->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_CARRIED:
			{
				GameplayEffect_Carry = (StateEffects.Carried) ? StateEffects.Carried->GetDefaultObject<UGameplayEffect>() : nullptr;
				GameplayEffect = (StateEffects.Standing) ? StateEffects.Standing->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
			case ELocomotionState::LS_CARRIEDUNDERWATER:
			{
				GameplayEffect_Carry = (StateEffects.CarriedUnderwater) ? StateEffects.CarriedUnderwater->GetDefaultObject<UGameplayEffect>() : nullptr;
				GameplayEffect = (StateEffects.Diving) ? StateEffects.Diving->GetDefaultObject<UGameplayEffect>() : nullptr;
				break;
			}
		}

		ApplyLocomotionEffect(CarryLocomotionEffect, GameplayEffect_Carry);
		ApplyLocomotionEffect(LocomotionStateEffect, GameplayEffect);
	}
}

void UPOTAbilitySystemComponent::ApplyLocomotionEffect(FActiveGameplayEffectHandle& CurrentEffectHandle, const UGameplayEffect* const NewEffect)
{
	if (CurrentEffectHandle.IsValid())
	{
		RemoveActiveGameplayEffect(CurrentEffectHandle);
	}

	if (NewEffect)
	{
		CurrentEffectHandle = ApplyGameplayEffectSpecToSelf(FGameplayEffectSpec(NewEffect, MakeEffectContext(), GetLevelFloat()));
	}
}

UAnimMontage* UPOTAbilitySystemComponent::POTGetCurrentMontage() const
{
	return GetCurrentMontage(); // copied to expose to BP
}

UGameplayAbility* UPOTAbilitySystemComponent::POTGetAnimatingAbility()
{
	return GetAnimatingAbility(); // copied to expose to BP
}

void UPOTAbilitySystemComponent::OnOwnerBoneBreak()
{
	UPOTGameplayAbility* GameplayAbility = Cast<UPOTGameplayAbility>(GetCurrentAttackAbility());
	if (!GameplayAbility) return;

	if (!GameplayAbility->bAllowedWhenLegsBroken)
	{
		FGameplayAbilitySpec* Spec = GameplayAbility->GetCurrentAbilitySpec();
		if (Spec)
		{
			SetAbilityInput(*Spec, false);
		}
	}
}

void UPOTAbilitySystemComponent::AddNonExclusiveAbility(UPOTGameplayAbility* Ability)
{
	if (!ensure(Ability)) return;
	if (Ability->IsPassive()) return;

	if (NonExclusiveAbilities.Contains(Ability)) return;

	NonExclusiveAbilities.Add(Ability);
}

void UPOTAbilitySystemComponent::RemoveNonExclusiveAbility(UPOTGameplayAbility* Ability)
{
	if (!ensure(Ability)) return;
	if (Ability->IsPassive()) return;

	for (int Index = 0; Index < NonExclusiveAbilities.Num(); Index++)
	{
		UPOTGameplayAbility* CurrentAbility = NonExclusiveAbilities[Index];
		if (CurrentAbility == Ability)
		{
			NonExclusiveAbilities.RemoveAt(Index);
			return;
		}
	}

}

float UPOTAbilitySystemComponent::GetAbilityForcedMovementSpeed() const
{
	float MaxSpeed = 0;

	if (CurrentAttackAbility && CurrentAttackAbility->bApplyMovementOverride)
	{
		MaxSpeed = CurrentAttackAbility->GetMaxMovementMultiplierSpeed();
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (!Ability->bApplyMovementOverride) continue;
			float NewSpeed = Ability->GetMaxMovementMultiplierSpeed();
			if (NewSpeed > MaxSpeed) MaxSpeed = NewSpeed;
		}
	}

	return MaxSpeed;
}

bool UPOTAbilitySystemComponent::HasAbilityForcedMovementSpeed() const
{
	if (CurrentAttackAbility && CurrentAttackAbility->bApplyMovementOverride)
	{
		return true;
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->bApplyMovementOverride)
			{
				return true;
			}
		}
	}

	return false;
}

bool UPOTAbilitySystemComponent::HasAbilityVerticalControlInWater() const
{
	if (CurrentAttackAbility && CurrentAttackAbility->bAllowVerticalControlInWater)
	{
		return true;
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->bAllowVerticalControlInWater)
			{
				return true;
			}
		}
	}

	return false;
}

void UPOTAbilitySystemComponent::FinishAbilityWithMontage(UAnimMontage* Montage)
{
	if (!ensure(Montage)) return;

	if (CurrentAttackAbility && CurrentAttackAbility->GetCurrentMontage() == Montage)
	{
		CurrentAttackAbility->EndAbility(CurrentAttackAbility->GetCurrentAbilitySpecHandle(), CurrentAttackAbility->GetCurrentActorInfo(), CurrentAttackAbility->GetCurrentActivationInfo(), true, false);
		return;
	}
	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->GetCurrentMontage() == Montage)
			{
				Ability->EndAbility(Ability->GetCurrentAbilitySpecHandle(), Ability->GetCurrentActorInfo(), Ability->GetCurrentActivationInfo(), true, false);
				return;
			}
		}
	}
}

UPOTGameplayAbility* UPOTAbilitySystemComponent::GetActiveAbilityFromMontage(const UAnimMontage* Montage) const
{
	if (!ensure(Montage)) return nullptr;

	if (CurrentAttackAbility && CurrentAttackAbility->GetCurrentMontage() == Montage && CurrentAttackAbility->IsActive())
	{
		return CurrentAttackAbility;
	}
	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->GetCurrentMontage() == Montage && Ability->IsActive())
			{
				return Ability;
			}
		}
	}
	return nullptr;
}

void UPOTAbilitySystemComponent::GetLifetimeReplicatedProps(TArray< FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UPOTAbilitySystemComponent, AbilityMontages, Params);

	
	FDoRepLifetimeParams ParamsSimulatedOnly{};
	ParamsSimulatedOnly.bIsPushBased = true;
	ParamsSimulatedOnly.Condition = COND_SimulatedOnly;
	DOREPLIFETIME_WITH_PARAMS_FAST(UPOTAbilitySystemComponent, bCurrentAttackAbilityUsesCharge, ParamsSimulatedOnly);
	
}

void UPOTAbilitySystemComponent::UpdateAutoCancelMovementType(ECustomMovementType MovementType)
{
	ParentPOTCharacter = Cast<AIBaseCharacter>(GetOwner());
	check(ParentPOTCharacter);
	for (UPOTGameplayAbility* Ability : GetAllActiveGameplayAbilities())
	{
		if (Ability->AutoCancelMovementModes.Contains(MovementType))
		{
			CancelAbility(Ability->GetClass()->GetDefaultObject<UGameplayAbility>());
		}
	}
}

TArray<UPOTGameplayAbility*> UPOTAbilitySystemComponent::GetAllActiveGameplayAbilities() const
{
	TArray<UPOTGameplayAbility*> Abilities;
	if (GetCurrentAttackAbility() && GetCurrentAttackAbility()->IsActive())
	{
		Abilities.Add(GetCurrentAttackAbility());
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability) && Ability->IsActive())
		{
			Abilities.Add(Ability);
		}
	}

	return Abilities;
}

void UPOTAbilitySystemComponent::CancelAllActiveAbilities()
{
	TArray<UPOTGameplayAbility*> Abilities = GetAllActiveGameplayAbilities();
	for (UPOTGameplayAbility* Ability : Abilities)
	{
		if (ensure(Ability) && !Ability->IsPassive())
		{
			CancelAbility(Ability->GetClass()->GetDefaultObject<UGameplayAbility>());
		}
	}
}

void UPOTAbilitySystemComponent::CancelActiveAbilitiesOnBecomeCarried()
{
	const TArray<UPOTGameplayAbility*> Abilities = GetAllActiveGameplayAbilities();
	for (const UPOTGameplayAbility* Ability : Abilities)
	{
		if (!ensureAlways(Ability) ||
			Ability->IsPassive() ||
			Ability->bAllowWhenCarried)
		{
			continue; 
		}

		CancelAbility(Ability->GetClass()->GetDefaultObject<UGameplayAbility>());
	}
}

UPOTGameplayAbility* UPOTAbilitySystemComponent::GetActiveAbilityFromSpec(const FGameplayAbilitySpec* Spec) const
{
	if (!ensure(Spec)) return nullptr;

	if (UPOTGameplayAbility* Ability = GetCurrentAttackAbility())
	{
		if (Ability->GetCurrentAbilitySpec() == Spec && Ability->IsActive()) return Ability;
	}

	for (UPOTGameplayAbility* Ability : NonExclusiveAbilities)
	{
		if (ensure(Ability) && Ability->IsActive())
		{
			if (Ability->GetCurrentAbilitySpec() == Spec) return Ability;
		}
	}
	return nullptr;
}

void UPOTAbilitySystemComponent::ReevaluateEffectsBasedOnAttribute(FGameplayAttribute Attribute, float NewLevel)
{
	TArray<FActiveGameplayEffectHandle> Handles = ActiveGameplayEffects.GetAllActiveEffectHandles();
	for (const FActiveGameplayEffectHandle ActiveHandle : Handles)
	{
		if (const FActiveGameplayEffect* Effect = ActiveGameplayEffects.GetActiveGameplayEffect(ActiveHandle) )
		{
			if (Effect->Spec.Def->GetAssetTags().HasTagExact(FGameplayTag::RequestGameplayTag(FName("ReEval.Attribute." + Attribute.GetName()))))
			{
				ActiveGameplayEffects.SetActiveGameplayEffectLevel(ActiveHandle, NewLevel);
			}
		}
	}
}

void UPOTAbilitySystemComponent::RemoveBoneDamageMultipliersOnEffectRemoved(const FActiveGameplayEffect& RemovedEffect)
{
	if (!ensureAlways(GetOwner() && GetOwner()->HasAuthority()))
	{
		return;
	}
	
	if (const UPOTGameplayEffect* POTEffect = Cast<const UPOTGameplayEffect>(RemovedEffect.Spec.Def))
	{
		for (const TPair<FName, FBoneDamageModifier> KVP : POTEffect->BoneDamageMultipliers)
		{
			if (FBoneDamageModifier* const DinoBoneDamageMultiplier = BoneDamageMultiplier.Find(KVP.Key))
			{
				*DinoBoneDamageMultiplier /= KVP.Value;
			}
			else
			{
				UE_LOG(TitansLog, Warning, TEXT("GameplayEffect \"%s\" attempted to alter damage multiplier for bone name \"%s\" which does not exist on character \"%s\"!"), *POTEffect->GetFName().ToString(), *KVP.Key.ToString(), *ParentPOTCharacter->GetName());
			}
		}
	}
}

UPOTGameplayAbility* UPOTAbilitySystemComponent::GetActiveAbilityFromSpecHandle(FGameplayAbilitySpecHandle Handle)
{
	if (FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Handle))
	{
		return GetActiveAbilityFromSpec(Spec);
	}

	return nullptr;
}

UPOTGameplayAbility* UPOTAbilitySystemComponent::GetActiveAbilityFromCDO(const UPOTGameplayAbility* CDO) const
{
	if (!ensure(CDO)) return nullptr;
	check(CDO == CDO->GetClass()->GetDefaultObject());

	if (UPOTGameplayAbility* Ability = GetCurrentAttackAbility())
	{
		if (Ability->GetClass()->GetDefaultObject() == CDO && Ability->IsActive()) return Ability;
	}

	for (UPOTGameplayAbility* Ability : NonExclusiveAbilities)
	{
		if (ensure(Ability) && Ability->IsActive())
		{
			if (Ability->GetClass()->GetDefaultObject() == CDO) return Ability;
		}
	}
	return nullptr;
}

void UPOTAbilitySystemComponent::ApplyPostDamageEffect(const TSubclassOf<UGameplayEffect> Effect, float NetDamage)
{
	FGameplayEffectSpec GESpec(Effect->GetDefaultObject<UGameplayEffect>(), MakeEffectContext(), GetLevelFloat());
	GESpec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(NAME_SetByCallerAttackDamage), NetDamage);
	FActiveGameplayEffectHandle AppliedEffect = ApplyGameplayEffectSpecToSelf(GESpec);
	
}

bool UPOTAbilitySystemComponent::HasAnyActiveGameplayAbility() const
{
	if (GetCurrentAttackAbility() && GetCurrentAttackAbility()->IsActive()) return true;
	for (UPOTGameplayAbility* Ability : NonExclusiveAbilities)
	{
		if (ensure(Ability) && Ability->IsActive())
		{
			if (Ability->bPreventUsingAbilityDetection) continue;
			return true;
		}
	}
	return false;
}

void UPOTAbilitySystemComponent::RemoveAllBuffs()
{
	FGameplayTagContainer BuffTags = FGameplayTagContainer();
	BuffTags.AddTag(FGameplayTag::RequestGameplayTag(NAME_Buff));

	FGameplayEffectQuery BuffQuery = FGameplayEffectQuery::MakeQuery_MatchAnyEffectTags(BuffTags);

	TArray<FActiveGameplayEffectHandle> ActiveBuffs = GetActiveEffects(BuffQuery);

	// Remove all effects with "Buff" tag
	for (int Index = ActiveBuffs.Num() - 1; Index >= 0; Index--)
	{
		RemoveActiveGameplayEffect(ActiveBuffs[Index]);
	}

}

void UPOTAbilitySystemComponent::OnCharacterHitWall(const FHitResult& HitResult)
{
	if (!ParentPOTCharacter)
	{
		return;
	}
	if (!ParentPOTCharacter->IsLocallyControlled())
	{
		// Don't run this on the server unless locally controlled. Differences in movement can cause rubber banding.
		// Instead, have the client request cancellation on the server
		return;
	}

	bool bHasSentWallEvent = false;
	
	for (UPOTGameplayAbility* Ability : GetAllActiveGameplayAbilities())
	{
		if (Ability->bDealDamageOnHitWall && !ParentPOTCharacter->HasAuthority())
		{ 
			if (AIBaseCharacter* HitChar = Cast<AIBaseCharacter>(HitResult.GetActor()))
			{
				//FGameplayEventData EventData = ParentPOTCharacter->GenerateEventData(HitResult, Ability->DamageConfigOnHitWall);
				//ParentPOTCharacter->ProcessDamageEvent(EventData, Ability->DamageConfigOnHitWall);
			}			
		}

		if (Ability->bShouldCancelOnCharacterHitWall && !ParentPOTCharacter->HasAuthority())
		{
			Ability->EndAbility(Ability->GetCurrentAbilitySpecHandle(), Ability->GetCurrentActorInfo(), Ability->GetCurrentActivationInfo(), false, false);
		}

		// If we hit something and want to deal damage, we need to do a weapons trace.
		if ((Ability->bDealDamageOnHitWall || Ability->bShouldCancelOnCharacterHitWall) && !bHasSentWallEvent)
		{
			FHitResult WeaponTrace = HitResult;

			if (Ability->bDealDamageOnHitWall)
			{
				// If we are going to do damage, we need to use a FHitResult which has bone info from a weapons trace
				FHitResult ClosestWeaponTrace = WeaponTrace;
				ClosestWeaponTrace.Time = 1.0f;
				TArray<FBodyShapes> Shapes;
				if (ParentPOTCharacter->GetDamageBodiesForMesh(ParentPOTCharacter->GetMesh(), Shapes))
				{
					for (FBodyShapes& Shape : Shapes)
					{
						FTransform BoneTransform = ParentPOTCharacter->GetMesh()->GetSocketTransform(Shape.Name, ERelativeTransformSpace::RTS_World);
						
						if (Ability->DamageConfigOnHitWall.BodyFilter.Contains(Shape.Name))
						{
							for (FCollisionShape& CollisionShape : Shape.UShapes)
							{
								TArray<FHitResult> WeaponResults;
								FVector TraceStart = BoneTransform.GetLocation() - (ParentPOTCharacter->GetActorForwardVector() * 10.0f);
								FVector TraceEnd = BoneTransform.GetLocation() + (ParentPOTCharacter->GetActorForwardVector() * 150.0f);
								GetWorld()->SweepMultiByChannel(WeaponResults,
									TraceStart,
									TraceEnd,
									BoneTransform.GetRotation(),
									Ability->DamageConfigOnHitWall.TraceChannel,
									CollisionShape,
									ParentPOTCharacter->GetWeaponTraceParams());
								
								for (FHitResult& WeaponResult : WeaponResults)
								{
									if (WeaponResult.Time <= ClosestWeaponTrace.Time)
									{
										ClosestWeaponTrace = WeaponResult;
										ClosestWeaponTrace.MyBoneName = Shape.Name;
									}
								}								

								ITraceUtils::DrawDebugSweptSphere(GetWorld(), TraceStart, TraceEnd, CollisionShape.GetSphereRadius(), FColor::Red, false, 2.0f);

							}
						}
					}
				}

				if (ClosestWeaponTrace.BoneName != NAME_None)
				{
					WeaponTrace.BoneName = ClosestWeaponTrace.BoneName;
				}
				if (ClosestWeaponTrace.MyBoneName != NAME_None)
				{
					WeaponTrace.MyBoneName = ClosestWeaponTrace.MyBoneName;
				}
			}

			if (ParentPOTCharacter->HasAuthority())
			{
				Server_HitWallEvent_Implementation(WeaponTrace);
			}
			else
			{
				Server_HitWallEvent(WeaponTrace);
			}

			bHasSentWallEvent = true;
		}
	}
}

void UPOTAbilitySystemComponent::Server_HitWallEvent_Implementation(const FHitResult& HitResult)
{
	if (!ParentPOTCharacter)
	{
		return;
	}

	// Since we are accepting a hit result from the client, we need to validate it.
	bool bHitResultValid = false;
	if (ParentPOTCharacter->IsLocallyControlled())
	{
		bHitResultValid = true;
	}
	else
	{
		// Ensure the client's hit result impact point is within a feasible distance from both the owner & the hit object.
		bool bOwnerPositionValid = FVector::Distance(HitResult.ImpactPoint, ParentPOTCharacter->GetActorLocation()) < 1000.0f || FVector::Distance(HitResult.TraceStart, ParentPOTCharacter->GetActorLocation()) < 1000.0f;
		bool bHitObjectPositionValid = true;
		if (HitResult.GetActor())
		{
			bHitObjectPositionValid = FVector::Distance(HitResult.ImpactPoint, HitResult.GetActor()->GetActorLocation()) < 1000.0f || FVector::Distance(HitResult.TraceStart, HitResult.GetActor()->GetActorLocation()) < 1000.0f;
		}

		// Ensure the client's rotation is such that it could actually trigger a hit wall event.
		float DotProduct = FVector::DotProduct(ParentPOTCharacter->GetActorForwardVector(), HitResult.ImpactNormal);
		bool bHitRotationValid = DotProduct < -0.0f;

		bHitResultValid = bOwnerPositionValid && bHitObjectPositionValid && bHitRotationValid;
	}

	for (UPOTGameplayAbility* Ability : GetAllActiveGameplayAbilities())
	{
		if (Ability->bDealDamageOnHitWall && bHitResultValid)
		{
			if (AIBaseCharacter* HitChar = Cast<AIBaseCharacter>(HitResult.GetActor()))
			{
				FGameplayEventData EventData = ParentPOTCharacter->GenerateEventData(HitResult, Ability->DamageConfigOnHitWall);
				ParentPOTCharacter->ProcessDamageEvent(EventData, Ability->DamageConfigOnHitWall);
			}
		}

		if (Ability->bShouldCancelOnCharacterHitWall)
		{
			Ability->EndAbility(Ability->GetCurrentAbilitySpecHandle(), Ability->GetCurrentActorInfo(), Ability->GetCurrentActivationInfo(), false, false);
		}
	}
}

TArray<FPOTGameplayAbilityRepAnimMontage>& UPOTAbilitySystemComponent::GetAbilityMontages_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(UPOTAbilitySystemComponent, AbilityMontages, this);
	return AbilityMontages;
}

bool UPOTAbilitySystemComponent::HasAbilityForcedRotation() const
{
	if (CurrentAttackAbility)
	{
		if (CurrentAttackAbility->HasForcedMovementRotation())
		{
			return true;
		}
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->HasForcedMovementRotation())
			{
				return true;
			}
		}
	}

	return false;
}

bool UPOTAbilitySystemComponent::OverrideMovementImmersionDepth() const
{
	if (CurrentAttackAbility)
	{
		if (CurrentAttackAbility->bOverrideMovementImmersionDepth)
		{
			return true;
		}
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->bOverrideMovementImmersionDepth)
			{
				return true;
			}
		}
	}

	return false;
}

FRotator UPOTAbilitySystemComponent::GetAbilityForcedRotation() const
{
	if (CurrentAttackAbility)
	{
		if (CurrentAttackAbility->HasForcedMovementRotation())
		{
			return CurrentAttackAbility->GetForcedMovementRotation();
		}
	}

	for (int Index = NonExclusiveAbilities.Num() - 1; Index >= 0; Index--)
	{
		UPOTGameplayAbility* Ability = NonExclusiveAbilities[Index];
		if (ensure(Ability))
		{
			if (Ability->HasForcedMovementRotation())
			{
				return Ability->GetForcedMovementRotation();
			}
		}
	}

	return FRotator();
}

void UPOTAbilitySystemComponent::ApplyExternalEffect(const TSubclassOf<UGameplayEffect>& Effect) {
	// Apply a gameplay effect due to some external trigger.
	if (Effect != nullptr) {
		FGameplayEffectSpec GESpec(Effect->GetDefaultObject<UGameplayEffect>(), MakeEffectContext(), GetLevelFloat());
		ApplyGameplayEffectSpecToSelf(GESpec);
	}
}

bool UPOTAbilitySystemComponent::TryActivateAbilityByDerivedClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	ABILITYLIST_SCOPE_LOCK();
	bool bSuccess = false;

	for (const FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (ensureAlways(Spec.Ability) && Spec.Ability->IsA(AbilityClass))
		{
			bSuccess |= TryActivateAbility(Spec.Handle);
			break;
		}
	}

	return bSuccess;
}

void UPOTAbilitySystemComponent::CancelAbilityByClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : ActivatableAbilities.Items)
	{
		if (ensureAlways(Spec.Ability) && Spec.Ability->IsA(AbilityClass))
		{
			CancelAbilitySpec(Spec, nullptr);
		}
	}
}

FPOTGameplayAbilityRepAnimMontage FPOTGameplayAbilityRepAnimMontage::Generate()
{
	FPOTGameplayAbilityRepAnimMontage Montage = FPOTGameplayAbilityRepAnimMontage();

	static uint32 Unique = 0;
	Montage.UniqueId = Unique++;

	return Montage;
}

bool FPOTGameplayAbilityRepAnimMontage::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar.SerializeIntPacked(UniqueId);
	return InnerMontage.NetSerialize(Ar, Map, bOutSuccess);
}
