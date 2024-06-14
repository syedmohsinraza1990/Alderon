// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "Abilities/POTGameplayAbility.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Abilities/POTTargetType.h"
#include "Player/IBaseCharacter.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/AnimSequence.h"
#include "Abilities/POTAbilityTypes.h"
#include "Abilities/POTAbilitySystemGlobals.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimMontage.h"
#include <algorithm>
#include "IGameplayStatics.h"
#include "Animation/AnimNotifyState_DoDamage.h"
#include "Animation/AnimNotifyState_DoShapeOverlaps.h"
#include "Animation/AnimationPoseData.h"
#include "Components/ICharacterMovementComponent.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/DamageStatics.h"
#include "Animation/DinosaurAnimBlueprint.h"
#include "Misc/DataValidation.h"
#include "Online/IGameState.h"

#if WITH_EDITOR
struct POTDamageStatics;
	FMontageAbilityGeneration UPOTGameplayAbility::MontageAbilityGeneration = FMontageAbilityGeneration();
#endif

UPOTGameplayAbility::UPOTGameplayAbility() 
	: bStartMontageManually(false)
	, bDontEndAbilityOnMontageEnd(false)
	, bPreventUsingAbilityDetection(false)
	, PlayRate(1.f)
	, KnockbackMode(EKnockbackMode::KM_AttackerForward)
	, OverrideDamageType(EDamageType::DT_GENERIC)
	, KnockbackForceMultiplier(1.f)
	, bDisableMovement(true)
	, bDisableRotation(true)
	, bFriendlyFire(true)
	, bStartCooldownWhenAbilityEnds(false)
	, bAutoCommitAbility(true)
	, bUseGrowthAsLevel(true)
	, bIsExclusive(true)
	, bMustHaveTarget(false)
	, bAllowWhenCarryingObject(false)
	, bCanCatchCarriable(false)
	, bAllowWhenInCombat(true)
	, bAllowWhenResting(false)
	, bAllowWhenSleeping(false)
	, bAllowWhenCarried(true)
	, bAllowWhenCarryingCharacter(false)
	, bAllowedWhenLegsBroken(true)
	, bAllowedWhenHomecaveBuffActive(false)
	, bStopPreciseMovement(true)
	, TraceTransformRecordInterval(0.04f)
	, bAcceptsTargetedEvents(false)
	, CurrentEventMagnitude(1.f)

{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	MovementLockTag = FGameplayTag::RequestGameplayTag(NAME_LockMoveMovementAbility);
	RotationLockTag = FGameplayTag::RequestGameplayTag(NAME_LockMoveRotationAbility);


	AutoCancelMovementSpeedDuration = 0.1f;

#if WITH_EDITOR
	GenerateFromScript();
#endif
}

bool UPOTGameplayAbility::IsBlockAbility() const
{
	return bBlockAbility;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

float UPOTGameplayAbility::GetBlockScalarBaseDamage() const
{
	if (bUseGrowthAsLevel)
	{
		return BlockScalarBaseDamage.Eval(GetAbilityLevelFromGrowth(), "POTGameplayAbility.GetBlockScalarBaseDamage");
	}
	return BlockScalarBaseDamage.Eval(GetAbilityLevel(), "POTGameplayAbility.GetBlockScalarBaseDamage");
}
float UPOTGameplayAbility::GetBlockScalarBleed() const
{
	if (bUseGrowthAsLevel)
	{
		return BlockScalarBleed.Eval(GetAbilityLevelFromGrowth(), "POTGameplayAbility.GetBlockScalarBleed");
	}
	return BlockScalarBleed.Eval(GetAbilityLevel(), "POTGameplayAbility.GetBlockScalarBleed");
}
float UPOTGameplayAbility::GetBlockScalarPoison() const
{
	if (bUseGrowthAsLevel)
	{
		return BlockScalarPoison.Eval(GetAbilityLevelFromGrowth(), "POTGameplayAbility.GetBlockScalarPoison");
	}
	return BlockScalarPoison.Eval(GetAbilityLevel(), "POTGameplayAbility.GetBlockScalarPoison");
}
float UPOTGameplayAbility::GetBlockScalarVenom() const
{
	if (bUseGrowthAsLevel)
	{
		return BlockScalarVenom.Eval(GetAbilityLevelFromGrowth(), "POTGameplayAbility.GetBlockScalarVenom");
	}
	return BlockScalarVenom.Eval(GetAbilityLevel(), "POTGameplayAbility.GetBlockScalarVenom");
}
float UPOTGameplayAbility::GetBlockScalarKnockback() const
{
	if (bUseGrowthAsLevel)
	{
		return BlockScalarKnockback.Eval(GetAbilityLevelFromGrowth(), "POTGameplayAbility.GetBlockScalarKnockback");
	}
	return BlockScalarKnockback.Eval(GetAbilityLevel(), "POTGameplayAbility.GetBlockScalarKnockback");
}
float UPOTGameplayAbility::GetBlockScalarBoneBreak() const
{
	if (bUseGrowthAsLevel)
	{
		return BlockScalarBoneBreak.Eval(GetAbilityLevelFromGrowth(), "POTGameplayAbility.GetBlockScalarBoneBreak");
	}
	return BlockScalarBoneBreak.Eval(GetAbilityLevel(), "POTGameplayAbility.GetBlockScalarBoneBreak");
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

float UPOTGameplayAbility::GetBlockScalarForDamageType(const EDamageEffectType DamageEffectType) const
{
	const FCurveTableRowHandle* const CurveTableRowHandle = BlockScalarPerDamageType.Find(DamageEffectType);
	if (!CurveTableRowHandle || CurveTableRowHandle->IsNull())
	{
		return 1.f;
	}
	
	FString RowName = TEXT("POTGameplayAbility.GetBlockScalar");

	const POTDamageStatics& DamageStatics = POTDamageStatics::DamageStatics();
	const FPOTDamageConfig* DamageConfig = DamageStatics.DamageConfig.Find(DamageEffectType);
	
	if (DamageConfig && !DamageConfig->CurveSuffix.IsEmpty())
	{
		RowName += DamageConfig->CurveSuffix;
		return CurveTableRowHandle->Eval(bUseGrowthAsLevel ? GetAbilityLevelFromGrowth() : GetAbilityLevel(), RowName);
	}
	
	return 1.f;
}

bool UPOTGameplayAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */, 
	const FGameplayTagContainer* TargetTags /* = nullptr */, OUT FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	if (const UPOTAbilitySystemComponent* WASC = Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent))
	{
		if (!WASC->IsAlive())
		{
			return false;
		}
		
		if (const UPOTGameplayAbility* CurrentAbility = WASC->GetCurrentAttackAbility())
		{
			if (CurrentAbility->IsActive())
			{
				if (CurrentAbility->bIsExclusive && bIsExclusive)
				{
					return false;
				}
			}
		}
	}
	
	if (const AIBaseCharacter* BaseChar = Cast<AIBaseCharacter>(ActorInfo->OwnerActor))
	{
		if (BaseChar->IsPreloadingClientArea())
		{
			return false;
		}

		if (bMustBeGrounded)
		{
			if (const UICharacterMovementComponent* MoveComp = Cast<UICharacterMovementComponent>(BaseChar->GetMovementComponent()))
			{
				if (!MoveComp->IsMovingOnGround()) return false;
			}
		}

		if (!bAllowWhenInCombat)
		{
			if (BaseChar->IsInCombat()) return false;
		}

		if ((BaseChar->IsSleeping() && !bAllowWhenSleeping) || (BaseChar->IsResting() && !bAllowWhenResting))
		{
			return false;
		}

		const FAttachTarget& AttachTarget = BaseChar->GetAttachTarget();
		if (AttachTarget.IsValid() &&
			((AttachTarget.AttachType == EAttachType::AT_Carry && !bAllowWhenCarried) ||
			(AttachTarget.AttachType == EAttachType::AT_Latch && !bAllowWhenLatchedToCharacter)))
		{
			return false;
		}

		if (!bAllowWhenCarryingCharacter && BaseChar->GetCarriedCharacter() != nullptr)
		{
			return false;
		}

		if (!bAllowedWhenHomecaveBuffActive && BaseChar->IsHomecaveBuffActive())
		{
			return false;
		}

		if (!bAllowedWhenLegsBroken && BaseChar->AreLegsBroken())
		{
			return false;
		}
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);

}

void UPOTGameplayAbility::PreActivate(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FOnGameplayAbilityEnded::FDelegate * OnGameplayAbilityEndedDelegate, const FGameplayEventData * TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate);

	EventTagsFilter.Reset(EventTagsFilter.Num());

	OwningPOTCharacter = Cast<AIBaseCharacter>(ActorInfo->OwnerActor);

	if (OwningPOTCharacter == nullptr)
	{
		if (AController* Ctrl = Cast<AController>(ActorInfo->OwnerActor))
		{
			OwningPOTCharacter = Cast<AIBaseCharacter>(Ctrl->GetPawn());
		}
	}
}

void UPOTGameplayAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* OwnerInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
	const FGameplayEventData* TriggerEventData)
{
	//SCOPE_CYCLE_COUNTER(STAT_WaActivateAbility);

	RegisterEventsHandlers();

	ActivateAbility_Stage2(Handle, OwnerInfo, ActivationInfo, TriggerEventData);
}

void UPOTGameplayAbility::ActivateAbility_Stage2(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* OwnerInfo, const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!ensure(OwnerInfo->GetAnimInstance()))
	{
		UE_LOG(TitansLog, Error, TEXT("UPOTGameplayAbility::ActivateAbility_Stage2: Owner has no anim instance!"));
		return;
	}

	GetOwnerPOTCharacter()->SetAnimRootMotionTranslationScale(AbilityMontageSet.AnimRootMotionTranslationScale);
	UPOTAbilitySystemComponent* WAC =
		Cast<UPOTAbilitySystemComponent>(OwnerInfo->AbilitySystemComponent.Get());

	static int MontageEndKeys = 0;
	MontageEndKey = MontageEndKeys++;

	if (bHasBlueprintActivate)
	{
		// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
		K2_ActivateAbility();
	}
	else if (bHasBlueprintActivateFromEvent)
	{
		if (TriggerEventData)
		{
			// A Blueprinted ActivateAbility function must call CommitAbility somewhere in its execution chain.
			K2_ActivateAbilityFromEvent(*TriggerEventData);
		}
		else
		{
			UE_LOG(LogAbilitySystem, Warning, TEXT("Ability %s expects event data but none is being supplied. Use Activate Ability instead of Activate Ability From Event."), *GetName());
			bool bReplicateEndAbility = false;
			bool bWasCancelled = true;
			EndAbility(Handle, OwnerInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
		}
	}
	else
	{
		if (!IsActive())
		{
			bool bReplicateEndAbility = false;
			bool bWasCancelled = true;
			EndAbility(Handle, OwnerInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
			return;
		}


		if (bAutoCommitAbility)
		{
			if (!CommitAbility(Handle, OwnerInfo, ActivationInfo))
			{
				return;
			}
		}



		if (bDisableMovement)
		{
			//OwningPOTCharacter->AddLock(ELockType::LT_MOVEMENT, MovementLockTag);
			//UE_LOG(LogTemp, Log, TEXT("ADD_MOVEMENT_LOCK UWaGameplayAbility::ActivateAbility"));
		}

		if (bDisableRotation)
		{
			//OwningPOTCharacter->AddLock(ELockType::LT_ROTATION, RotationLockTag);
		}

		K2_PreMontageActivateAbility();

		//#TODO: Check side-effects of disabling cast to wa anim instance
		if (!bStartMontageManually)
		{
			UE_LOG(LogTemp, Warning, TEXT("UPOTGameplayAbility::ActivateAbility_Stage2: %s"), *this->GetFName().ToString());

			PlayMontage(Handle, OwnerInfo, WAC, ActivationInfo, OwnerInfo->GetAnimInstance());
		}

		K2_PostMontageActivateAbility();

		//Blueprint events might have ended the ability
		if (!IsActive())
		{
			return;
		}
	}

	if (IsActive() && !IsPassive())
	{
		if (bIsExclusive)
		{
			WAC->SetCurrentAttackingAbility(this);
		}
		else
		{
			WAC->AddNonExclusiveAbility(this);
		}
	}

	if (!bStartCooldownWhenAbilityEnds)
	{
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}

	if (bStopPreciseMovement)
	{
		if (AIBaseCharacter* BaseChar = Cast<AIBaseCharacter>(OwnerInfo->AvatarActor.Get()))
		{
			BaseChar->bDesiresPreciseMovement = false;
			BaseChar->UpdateMovementState();
			if (UICharacterMovementComponent* MoveComp = Cast<UICharacterMovementComponent>(BaseChar->GetMovementComponent()))
			{
				MoveComp->UpdateRotationMethod();
			}
		}
	}

	if (bApplyEffectContainerOnActivate)
	{
		for (const TPair<FGameplayTag, FPOTGameplayEffectContainer>& Pair : EffectContainerMap)
		{
			if (SpecificActivationEffectTags.Num() > 0)
			{
				if (!SpecificActivationEffectTags.HasTagExact(Pair.Key)) continue;
			}
			ApplyEffectContainer(Pair.Key, TriggerEventData ? *TriggerEventData : FGameplayEventData(), TMap<FGameplayTag, float>(), -1);
		}
	}

	if (PeriodicActivation > 0.f && OwnerInfo)
	{
		if (AActor* Actor = OwnerInfo->OwnerActor.Get())
		{
			FTimerDelegate Del;
			Del.BindUObject(this, &UPOTGameplayAbility::PeriodicActivate);
			Actor->GetWorld()->GetTimerManager().SetTimer(PeriodicActivationTimerHandle, Del, PeriodicActivation, true, PeriodicActivation);
		}
	}
}


void UPOTGameplayAbility::PeriodicActivate()
{
	if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
	{
		if (UAbilitySystemComponent* AbilitySystem = Info->AbilitySystemComponent.Get())
		{
			FScopedPredictionWindow Prediction(AbilitySystem);
			if (CanActivateAbility(CurrentSpecHandle, GetCurrentActorInfo(), nullptr, nullptr, nullptr))
			{
				ActivateAbility_Stage2(CurrentSpecHandle, GetCurrentActorInfo(), GetCurrentActivationInfo(), nullptr);
			}
		}
	}
}
	
void UPOTGameplayAbility::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnRemoveAbility(ActorInfo, Spec);

	if (ActorInfo)
	{
		BPRemoveAbility(*ActorInfo);
	}

	if (ActorInfo)
	{
		if (AActor* Actor = ActorInfo->OwnerActor.Get())
		{
			Actor->GetWorld()->GetTimerManager().ClearTimer(PeriodicActivationTimerHandle);
		}
	}

	UPOTAbilitySystemComponent* WAC = Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (WAC && MonitoredTags.Num() > 0)
	{
		for (FGameplayTag Tag : MonitoredTags)
		{
			if (FDelegateHandle* Handle = MonitoredTagHandles.Find(Tag))
			{
				FOnGameplayEffectTagCountChanged& Del = WAC->RegisterGameplayTagEvent(Tag);
				Del.Remove(*Handle);
			}
		}
	}
}

void UPOTGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	if (ActorInfo)
	{
		BPGiveAbility(*ActorInfo);
	}

	if (Spec.Ability->AbilityTags.HasTag(FGameplayTag::RequestGameplayTag(NAME_AbilityPassive)) &&
		ActorInfo->OwnerActor.Get() && 
		ActorInfo->OwnerActor->HasAuthority())
	{
		if (!ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false))
		{
			UE_LOG(LogTemp, Warning, TEXT("UPOTGameplayAbility::OnGiveAbility: Passive Ability failed activation! %s"), *Spec.Ability->GetName());
		}
	}

	UPOTAbilitySystemComponent* WAC = Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (WAC && MonitoredTags.Num() > 0)
	{
		for (FGameplayTag Tag : MonitoredTags)
		{
			FOnGameplayEffectTagCountChanged& Del = WAC->RegisterGameplayTagEvent(Tag);
			MonitoredTagHandles.FindOrAdd(Tag) = Del.AddUObject(this, &UPOTGameplayAbility::OnTagChanged);
		}
	}
}

void UPOTGameplayAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	if (Spec.Ability->AbilityTags.HasTag(FGameplayTag::RequestGameplayTag(NAME_AbilityPassive)) &&
		ActorInfo->OwnerActor.Get() &&
		ActorInfo->OwnerActor->HasAuthority())
	{
		if (!ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle, false))
		{
			UE_LOG(LogTemp, Warning, TEXT("UPOTGameplayAbility::OnAvatarSet: Passive Ability failed activation! %s"), *Spec.Ability->GetName());
		}
	}
}


int32 UPOTGameplayAbility::GetAbilityLevelFromGrowth() const
{
	if (IsInstantiated() == false || CurrentActorInfo == nullptr)
	{
		return 1;
	}

	if (CurrentActorInfo->AbilitySystemComponent.IsValid())
	{
		float Growth = 1.f;
		if (AIBaseCharacter* BaseChar = Cast<AIBaseCharacter>(CurrentActorInfo->OwnerActor))
		{
			Growth = BaseChar->GetGrowthPercent();
		}

		//float Growth = CurrentActorInfo->AbilitySystemComponent->GetNumericAttribute(UCoreAttributeSet::GetGrowthAttribute());
		return FMath::RoundToInt(Growth * 5.f);
	}

	return 1;
}

void UPOTGameplayAbility::RegisterEventsHandlers()
{
	UPOTAbilitySystemComponent* WAC =
		Cast<UPOTAbilitySystemComponent>(GetCurrentActorInfo()->AbilitySystemComponent.Get());

	for (const TPair<FGameplayTag, FPOTGameplayEffectContainer>& EffectKVP : EffectContainerMap)
	{
		EventTagsFilter.AddTag(EffectKVP.Key);
	}
	/*for (const TPair<FGameplayTag, FWaGameplayEffectContainer>& EffectKVP : AdditionalEffectContainerMap)
	{
		EventTagsFilter.AddTagFast(EffectKVP.Key);
	}*/

	if (WAC != nullptr)
	{
		EventHandle = WAC->AddGameplayEventTagContainerDelegate(EventTagsFilter,
			FGameplayEventTagMulticastDelegate::FDelegate::CreateUObject(
				this, &UPOTGameplayAbility::OnGameplayEvent));
	}

	if (UAnimInstance * AnimInst = GetCurrentActorInfo()->GetAnimInstance())
	{
		AnimInst->OnMontageEnded.AddUniqueDynamic(this, &UPOTGameplayAbility::OnAnyMontageEnded);
	}
}

UGameplayEffect* UPOTGameplayAbility::GetCooldownGameplayEffect() const
{
	if (bIsInterrupted && InterruptCooldownEffectClass)
	{
		return InterruptCooldownEffectClass->GetDefaultObject<UGameplayEffect>();
	}
	else
	{
		return Super::GetCooldownGameplayEffect();
	}
}

bool UPOTGameplayAbility::CheckCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
	check(ActorInfo);
	if (!ActorInfo)
	{
		UE_LOG(TitansLog, Error, TEXT("UPOTGameplayAbility::CheckCooldown: ActorInfo nullptr"));
		return true;
	}

	UPOTAbilitySystemComponentBase* ASC = Cast<UPOTAbilitySystemComponentBase>(ActorInfo->AbilitySystemComponent.Get());

	check(ASC);
	if (!ASC)
	{
		UE_LOG(TitansLog, Error, TEXT("UPOTGameplayAbility::CheckCooldown: ASC nullptr"));
		return true;
	}

	float RemainingCooldown = ASC->GetCooldownTimeRemaining(GetClass());

	return RemainingCooldown <= 0;
}

bool UPOTGameplayAbility::CommitAbilityCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo * ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const bool ForceCooldown, OUT FGameplayTagContainer* OptionalRelevantTags)
{
	if (ActorInfo->AbilitySystemComponent != nullptr)
	{
		return Super::CommitAbilityCooldown(Handle, ActorInfo, ActivationInfo, ForceCooldown, OptionalRelevantTags);
	}
	return false;
}

void UPOTGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!ActorInfo) return;
	UPOTAbilitySystemComponentBase* ASC = Cast<UPOTAbilitySystemComponentBase>(ActorInfo->AbilitySystemComponent.Get());
	check(ASC);
	if (!ASC)
	{
		return;
	}

	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE)
	{
		return;
	}

	if (ActorInfo->IsLocallyControlled())
	{
		if (HasAuthorityOrPredictionKey(ActorInfo, &ActivationInfo))
		{
			FActiveGameplayEffectHandle CooldownEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
			// Track the local cooldown effects manually to improve prediction

			ASC->AddNewCooldownEffect(CooldownEffectHandle);				
		}
		else // we cant predict this effect properly so UE wont allow adding it. We can add a cooldown delay instead. The server will apply the effect properly and replicate it
		{
			FGameplayEffectSpecHandle NewHandle = ASC->MakeOutgoingSpec(CooldownGE->GetClass(), GetAbilityLevel(Handle, ActorInfo), MakeEffectContext(Handle, ActorInfo));
			if (NewHandle.IsValid())
			{
				ASC->AddCooldownDelay(CooldownGE, NewHandle.Data->GetDuration());
			}
		}
	}
	else
	{
		FActiveGameplayEffectHandle CooldownEffectHandle = ApplyGameplayEffectToOwner(Handle, ActorInfo, ActivationInfo, CooldownGE, GetAbilityLevel(Handle, ActorInfo));
		ASC->AddNewCooldownEffect(CooldownEffectHandle);

	}
}

void UPOTGameplayAbility::K2_PlayMontage(UPOTAbilitySystemComponent* WAC, const FGameplayAbilityActivationInfo ActivationInfo, UAnimInstance* WaAnimInstance)
{
	UE_LOG(LogTemp, Warning, TEXT("UPOTGameplayAbility::K2_PlayMontage: %s"), *this->GetFName().ToString());

	PlayMontage(GetCurrentAbilitySpecHandle(), GetCurrentActorInfo(), WAC, ActivationInfo, WaAnimInstance);
}

void UPOTGameplayAbility::StopCurrentAbilityMontage()
{
	UPOTAbilitySystemComponent* WAC = Cast<UPOTAbilitySystemComponent>(OwningPOTCharacter != nullptr ? OwningPOTCharacter->GetAbilitySystemComponent() : nullptr);
	if (!ensure(WAC)) return;
	WAC->StopMontage(this, GetCurrentActivationInfo(), CurrentMontage);
	SetCurrentMontage(nullptr);
}

bool UPOTGameplayAbility::IsUsingTransitionMontage() const
{
	return CurrentMontage != AbilityMontageSet.CharacterMontage;
}

void UPOTGameplayAbility::TransitionMontage(UAnimMontage* const NewMontage, UAnimInstance* const AnimInstance, const FName Section)
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	UPOTAbilitySystemComponent* const POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(OwningPOTCharacter->GetAbilitySystemComponent());
	
	if (!ensureAlways(NewMontage) ||
		!ensureAlways(POTAbilitySystemComponent) ||
		!ensureAlways(AnimInstance))
	{
		return;
	}

	FName SectionName = Section;
	if (SectionName == NAME_None && !MontageSections.IsEmpty())
	{
		SectionName = MontageSections[FMath::RandRange(0, MontageSections.Num() - 1)];
	}

	UAnimMontage* const PreviousMontage = CurrentMontage;

	SetCurrentMontage(NewMontage);

	if (PreviousMontage)
	{
		POTAbilitySystemComponent->StopMontage(this, CurrentActivationInfo, PreviousMontage);
	}

	UE_LOG(LogTemp, Warning, TEXT("UPOTGameplayAbility::TransitionMontage: %s %s"), *NewMontage->GetFName().ToString(), *this->GetFName().ToString());

	POTAbilitySystemComponent->PlayMontage(this, CurrentActivationInfo, NewMontage, GetPlayRate(), SectionName);

	FOnMontageEnded EndDelegate = FOnMontageEnded::CreateUObject(this, &UPOTGameplayAbility::OnMontageEnded, POTAbilitySystemComponent, MontageEndKey);
	AnimInstance->Montage_SetEndDelegate(EndDelegate, CurrentMontage);
}

void UPOTGameplayAbility::PlayMontage(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* OwnerInfo,
	UPOTAbilitySystemComponent* WAC, const FGameplayAbilityActivationInfo ActivationInfo, UAnimInstance* WaAnimInstance)
{
	
	if (!ensure(WaAnimInstance))
	{
		return;
	}

	FName SectionName = NAME_None;
	int32 SectionIndex = 0;
	if (MontageSections.Num() > 0)
	{
		SectionIndex = MontageSections.Num() == 1 ? 0 : FMath::RandRange(0, MontageSections.Num() - 1);
		SectionName = MontageSections[SectionIndex];
	}

	GetMontageSet(Handle, *OwnerInfo, ActivationInfo, CurrentMontageSet);
	if (CurrentMontageSet.CharacterMontage != nullptr || CurrentMontageSet.WeaponMontage != nullptr)
	{
		SetCurrentMontage(CurrentMontageSet.CharacterMontage); // Questionable?

		if (AIBaseCharacter* BaseChar = GetOwnerPOTCharacter())
		{
			BaseChar->SetAnimRootMotionTranslationScale(AbilityMontageSet.AnimRootMotionTranslationScale);
			BaseChar->SetForcePoseUpdateWhenNotRendered(true);
		}
		
		UE_LOG(LogTemp, Warning, TEXT("UPOTGameplayAbility::PlayMontage: %s %s"), *CurrentMontage->GetFName().ToString(), *this->GetFName().ToString());

		WAC->PlayMontage(this, ActivationInfo, CurrentMontage, GetPlayRate(), SectionName);

		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UPOTGameplayAbility::OnMontageEnded, WAC, MontageEndKey);
		WaAnimInstance->Montage_SetEndDelegate(EndDelegate, CurrentMontage);
	}
}

void UPOTGameplayAbility::CancelAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
	bool bReplicateCancelAbility)
{
	if (CanBeCanceled() && bInterruptable)
	{
		bIsInterrupted = true;
	}

	CancelAbility_Stage2(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);

	bIsPressed = false;
	bIsInterrupted = false;
}

void UPOTGameplayAbility::CancelAbility_Stage2(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
	bool bReplicateCancelAbility)
{
	BPCancelAbility();

	if (!ActorInfo) return;

	if (OwningPOTCharacter == nullptr)
	{
		OwningPOTCharacter = Cast<AIBaseCharacter>(ActorInfo->OwnerActor);

		if (OwningPOTCharacter == nullptr)
		{
			if (AController* Ctrl = Cast<AController>(ActorInfo->OwnerActor))
			{
				OwningPOTCharacter = Cast<AIBaseCharacter>(Ctrl->GetPawn());
			}
		}
	}
	//SCOPE_CYCLE_COUNTER(STAT_WaCancelAbility);
	if (OwningPOTCharacter && OwningPOTCharacter->AbilitySystem->CurrentAttackAbility == this)
	{
		OwningPOTCharacter->SetAnimRootMotionTranslationScale(1.0f);
	}

	UAnimInstance* WaAnimInstance = Cast<UAnimInstance>(ActorInfo->GetAnimInstance());
	if (WaAnimInstance != nullptr && CurrentMontage != nullptr
		&& WaAnimInstance->Montage_IsPlaying(CurrentMontage))
	{
		FAnimMontageInstance* AMI = WaAnimInstance->GetActiveInstanceForMontage(CurrentMontageSet.CharacterMontage);
		if (AMI != nullptr)
		{
			AMI->OnMontageEnded = FOnMontageEnded();

			const float BlendOutTime = CurrentMontageSet.CharacterMontage->BlendOut.GetBlendTime();
			AMI->Stop(FAlphaBlend(CurrentMontageSet.CharacterMontage->BlendOut, BlendOutTime));
		}
	}

	if (AIDinosaurCharacter* IDinoOwner = Cast<AIDinosaurCharacter>(OwningPOTCharacter))
	{
		IDinoOwner->FadeOutVocalSounds();
	}

	if (bStopPreciseMovement)
	{
		if (ActorInfo->AvatarActor.IsValid())
		{
			if (AIBaseCharacter* BaseChar = Cast<AIBaseCharacter>(ActorInfo->AvatarActor.Get()))
			{
				if (BaseChar->bPrecisedMovementKeyPressed)
				{
					BaseChar->bDesiresPreciseMovement = true;

					// Skip Ability Cancel is true to prevent a stack overflow
					bool bSkipAbilityCancel = true;
					BaseChar->UpdateMovementState(bSkipAbilityCancel);

					if (UICharacterMovementComponent* MoveComp = Cast<UICharacterMovementComponent>(BaseChar->GetMovementComponent()))
					{
						MoveComp->UpdateRotationMethod();
					}
				}
			}
		}
	}
}

bool UPOTGameplayAbility::CanBeCanceled() const
{
	bool bSuperCancel = Super::CanBeCanceled();
	return OwningPOTCharacter == nullptr ? bSuperCancel : (!OwningPOTCharacter->AbilitySystem->IsAlive() || bSuperCancel);
}

void UPOTGameplayAbility::GetMontageSet_Implementation(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo& ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, FAbilityMontageSet& OutMontageSet) const
{
	OutMontageSet = AbilityMontageSet;
}

void UPOTGameplayAbility::FinishAbility()
{
	FScopedPredictionWindow Prediction(OwningPOTCharacter != nullptr ? OwningPOTCharacter->AbilitySystem : nullptr);
	K2_EndAbility();
}

void UPOTGameplayAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, 
	bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!IsActive() && !bIsInterrupted) return;
	//SCOPE_CYCLE_COUNTER(STAT_EndWaAbility);

	if (OwningPOTCharacter->AbilitySystem->CurrentAttackAbility == this)
	{
		GetOwnerPOTCharacter()->SetAnimRootMotionTranslationScale(1.0f);
	}

	if (!bWasCancelled || !bDontEndAbilityOnCancel)
	{
		Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	}

	if (bDisableMovement)
	{
		//OwningPOTCharacter->RemoveLock(ELockType::LT_MOVEMENT, MovementLockTag);
		//UE_LOG(LogTemp, Log, TEXT("REMOVE_MOVEMENT_LOCK UWaGameplayAbility::EndAbility"));
	}

	if (bDisableRotation)
	{
		//OwningPOTCharacter->RemoveLock(ELockType::LT_ROTATION, RotationLockTag);
	}

	UAnimInstance* WaAnimInstance = Cast<UAnimInstance>(ActorInfo->GetAnimInstance());
	if (WaAnimInstance != nullptr && CurrentMontageSet.CharacterMontage != nullptr  
		&& WaAnimInstance->Montage_IsPlaying(CurrentMontageSet.CharacterMontage))
	{
		FAnimMontageInstance* AMI = WaAnimInstance->GetActiveInstanceForMontage(CurrentMontageSet.CharacterMontage);
		if (AMI != nullptr)
		{
			AMI->OnMontageEnded = FOnMontageEnded();
			//AMI->Stop(FAlphaBlend(CurrentMontageSet.CharacterMontage->BlendOut, 0.1f));
		}

	}

	UPOTAbilitySystemComponent* WAC =
		Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	if (WAC != nullptr)
	{
		if (!IsPassive() && !bIsExclusive)
		{
			WAC->RemoveNonExclusiveAbility(this);
		}

		WAC->RemoveGameplayEventTagContainerDelegate(EventTagsFilter, EventHandle);

		for (FGameplayTag& Tag : InputTags)
		{
			if (WAC->HasReplicatedLooseGameplayTag(Tag))
			{
				WAC->RemoveReplicatedLooseGameplayTag(Tag);
			}
		}


		if (bRemoveEffectContainerOnEnd)
		{
			TArray<TSubclassOf<UGameplayEffect>> EffectsToRemove;

			for (const TPair<FGameplayTag, FPOTGameplayEffectContainer>& Pair : EffectContainerMap)
			{
				if (SpecificActivationEffectTags.Num() > 0)
				{
					if (!SpecificActivationEffectTags.HasTagExact(Pair.Key)) continue;
				}
				for (const FPOTGameplayEffectContainerEntry& Entry : Pair.Value.ContainerEntries)
				{
					for (const FPOTGameplayEffectEntry& Effect : Entry.TargetGameplayEffects)
					{
						EffectsToRemove.Append(Effect.TargetGameplayEffectClasses);
					}
				}
			}

			for (const TSubclassOf<UGameplayEffect>& Effect : EffectsToRemove)
			{
				WAC->RemoveActiveGameplayEffectBySourceEffect(Effect, nullptr, -1);
			}
		}

		if (CurrentMontage)
		{
			WAC->StopMontage(this, ActivationInfo, CurrentMontage);
		}
	}

	if (bStartCooldownWhenAbilityEnds)
	{
		CommitAbilityCooldown(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
	}

	if (bStopPreciseMovement)
	{
		if (AIBaseCharacter* BaseChar = Cast<AIBaseCharacter>(ActorInfo->AvatarActor.Get()))
		{
			if (BaseChar->bPrecisedMovementKeyPressed)
			{
				BaseChar->bDesiresPreciseMovement = true;
				BaseChar->UpdateMovementState();
				if (UICharacterMovementComponent* MoveComp = Cast<UICharacterMovementComponent>(BaseChar->GetMovementComponent()))
				{
					MoveComp->UpdateRotationMethod();
				}
			}
		}
	}

	ResetTracingData();
	ResetOverlapTiming();
}

void UPOTGameplayAbility::RemoveEffectContainerByTagActorInfo(FGameplayTag Tag, const FGameplayAbilityActorInfo& ActorInfo)
{
	
	UPOTAbilitySystemComponent* WAC = Cast<UPOTAbilitySystemComponent>(ActorInfo.AbilitySystemComponent.Get());
	if (!WAC) return;

	TArray<TSubclassOf<UGameplayEffect>> EffectsToRemove;

	for (const TPair<FGameplayTag, FPOTGameplayEffectContainer>& Pair : EffectContainerMap)
	{
		if (Pair.Key != Tag) continue;

		for (const FPOTGameplayEffectContainerEntry& Entry : Pair.Value.ContainerEntries)
		{
			for (const FPOTGameplayEffectEntry& Effect : Entry.TargetGameplayEffects)
			{
				EffectsToRemove.Append(Effect.TargetGameplayEffectClasses);
			}
		}
	}

	for (const TSubclassOf<UGameplayEffect>& Effect : EffectsToRemove)
	{
		WAC->RemoveActiveGameplayEffectBySourceEffect(Effect, nullptr, -1);
	}
}

void UPOTGameplayAbility::RemoveEffectContainerByTag(FGameplayTag Tag)
{
	if (GetCurrentActorInfo())
	{
		RemoveEffectContainerByTagActorInfo(Tag, *GetCurrentActorInfo());
	}
}

void UPOTGameplayAbility::OnMontageEnded(UAnimMontage* Montage, bool bInterrupted, 
	UPOTAbilitySystemComponent* AbilitySystemComponent, int Key)
{
	//UE_LOG(TitansLog, Log, TEXT("UPOTGameplayAbility::OnMontageEnded"));
	if (IsActive())
	{
		if (AIBaseCharacter* BaseChar = GetOwnerPOTCharacter())
		{
			BaseChar->SetAnimRootMotionTranslationScale(1.f);
		}

		if (!bDontEndAbilityOnMontageEnd && Montage == CurrentMontage)
		{
			if (Key == MontageEndKey)
			{
				FScopedPredictionWindow Prediction(AbilitySystemComponent, true);
				EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);

				if (AIBaseCharacter* BaseChar = GetOwnerPOTCharacter())
				{
					if (!BaseChar->GetCurrentlyCarriedObject().Object.IsValid())
					{
						//UE_LOG(TitansLog, Log, TEXT("UPOTGameplayAbility::OnMontageEnded - SetForcePoseUpdateWhenNotRendered"));
						BaseChar->SetForcePoseUpdateWhenNotRendered(false);
					}
				}
			}
		}

		ResetTracingData();

		K2_OnMontageEnded(Montage, bInterrupted, AbilitySystemComponent);
	}
	
	
}

void UPOTGameplayAbility::OnAnyMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
	if (!IsActive())
	{
		return;
	}

	if (AIBaseCharacter* BaseChar = GetOwnerPOTCharacter())
	{
		BaseChar->SetAnimRootMotionTranslationScale(1.f);
	}

	/*if (!bDontEndAbilityOnMontageEnd && Montage == CurrentMontage && IsActive())
	{
		UPOTAbilitySystemComponent* AbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent);
		FScopedPredictionWindow Prediction(AbilitySystemComponent, true);
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bInterrupted);
	}*/

	ResetTracingData();

	K2_OnMontageEnded(Montage, bInterrupted, Cast<UPOTAbilitySystemComponent>(CurrentActorInfo->AbilitySystemComponent));
}

TArray<FPOTGameplayEffectContainerSpec> UPOTGameplayAbility::MakeEffectContainerSpecFromContainer(
	const FPOTGameplayEffectContainer& Container, const FGameplayEventData& EventData, const TMap<FGameplayTag, float>& SetByCallerMagnitudes,
	int32 OverrideGameplayLevel /*= -1*/)
{
	NonAbilityTargets.Reset(NonAbilityTargets.Num());

	if (OwningPOTCharacter == nullptr && EventData.Instigator != nullptr)
	{
		OwningPOTCharacter = Cast<AIBaseCharacter>(const_cast<AActor*>(EventData.Instigator.Get()));
	}

	// Precaution; Bug found when UWaAbilitySystemGlobals::MakeEffectContainerSpec
	// CurrentActorInfo doesn't get initialized in the call stack so we do it manually here
	if (CurrentActorInfo == nullptr)
	{
		if (ensure(OwningPOTCharacter))
		{
			UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(OwningPOTCharacter);
			if (ensure(AbilitySystemComponent))
			{
				CurrentActorInfo = AbilitySystemComponent->AbilityActorInfo.Get();
			}
		}
	}

	if (!OwningPOTCharacter && CurrentActorInfo)
	{
		OwningPOTCharacter = Cast<AIBaseCharacter>(CurrentActorInfo->OwnerActor.Get());
	}

	// First figure out our actor info
	TArray<FPOTGameplayEffectContainerSpec> ReturnSpecs;
	UPOTAbilitySystemComponent* OwningASC = OwningPOTCharacter->AbilitySystem;

	FGameplayTagContainer CueTags;
	Container.GetGameplayCueTags(CueTags);

	if (OwningASC != nullptr && Container.ContainerEntries.Num() > 0)
	{
		for (const FPOTGameplayEffectContainerEntry& Entry : Container.ContainerEntries)
		{
			FPOTGameplayEffectContainerSpec Spec;

			// If we have a target type, run the targeting logic. This is optional, targets can be added later
			if (Entry.TargetType.Get() != nullptr)
			{
				Spec.TargetType = Entry.TargetType;
				TArray<FHitResult> HitResults;
				TArray<AActor*> TargetActors;
				const UPOTTargetType* TargetTypeCDO = Entry.TargetType.GetDefaultObject();
				TargetTypeCDO->GetTargets(OwningPOTCharacter, EventData, HitResults, TargetActors);
				Spec.AddTargets(HitResults, TargetActors);

				for (const FHitResult& Result : HitResults)
				{
					if (UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Result.GetActor()) == nullptr)
					{
						NonAbilityTargets.Emplace(FNonAbilityTarget(CueTags, Result));
					}
				}

				for (AActor* Act : TargetActors)
				{
					if (UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Act) == nullptr)
					{
						NonAbilityTargets.Emplace(FNonAbilityTarget(CueTags, Act));
					}
				}
			}


			// If we don't have an override level, use the default on the ability system component
			if (OverrideGameplayLevel == INDEX_NONE)
			{
				OverrideGameplayLevel = GetAbilityLevel();

				if (bUseGrowthAsLevel)
				{
					OverrideGameplayLevel = GetAbilityLevelFromGrowth();
				}

			}

			// Build GameplayEffectSpecs for each applied effect
			for (const FPOTGameplayEffectEntry& TGEEntry : Entry.TargetGameplayEffects)
			{
				if (!TGEEntry.SourceTags.IsEmpty())
				{
					FGameplayTagContainer OwnerTags;
					OwningPOTCharacter->GetOwnedGameplayTags(OwnerTags);
					if (!TGEEntry.SourceTags.RequirementsMet(OwnerTags))
					{
						continue;
					}
				}

				for (const TSubclassOf<UGameplayEffect>& EffectClass : TGEEntry.TargetGameplayEffectClasses)
				{
					// This part is copied from UGameplayAbility::MakeOutgoingGameplayEffectSpec.
					//   Reason was that it was necessary to override effect causer in effect context.
					FGameplayEffectContextHandle GECHandle = MakeEffectContext(CurrentSpecHandle, CurrentActorInfo);
					// Override instigator/causer

					FPOTGameplayEffectContext* ExistingPOTEffectContext = StaticCast<FPOTGameplayEffectContext*>(GECHandle.Get());
					if (ExistingPOTEffectContext && ExistingPOTEffectContext->GetDamageType() == EDamageType::DT_SPIKES)
					{
						// fix instigator for spikes
						GECHandle.AddInstigator(OwningPOTCharacter, OwningPOTCharacter);						

						if (EventData.ContextHandle.GetHitResult())
						{ // Flip hit bones for spikes
							const FHitResult* OldHitResult = EventData.ContextHandle.GetHitResult();
							FHitResult HitResult = *EventData.ContextHandle.GetHitResult();
							HitResult.BoneName = OldHitResult->MyBoneName;
							HitResult.MyBoneName = OldHitResult->BoneName;
							GECHandle.AddHitResult(HitResult);
						}
					}
					else
					{
						AActor* Instigator = EventData.ContextHandle.GetInstigator() == nullptr ? OwningPOTCharacter : EventData.ContextHandle.GetInstigator();
						GECHandle.AddInstigator(Instigator, EventData.ContextHandle.GetEffectCauser());

						if (EventData.ContextHandle.GetHitResult())
						{
							GECHandle.AddHitResult(*EventData.ContextHandle.GetHitResult());
						}

					}

					UAbilitySystemComponent* const AbilitySystemComponent = CurrentActorInfo->AbilitySystemComponent.Get();
					FGameplayEffectSpecHandle NewHandle = AbilitySystemComponent->MakeOutgoingSpec(EffectClass, OverrideGameplayLevel, GECHandle);
					if (NewHandle.IsValid())
					{
						FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle);
						ApplyAbilityTagsToGameplayEffectSpec(*NewHandle.Data.Get(), AbilitySpec);

						if (EventData.ContextHandle.GetHitResult()) // Add target's gameplay tags to effect spec
						{
							if (UPOTAbilitySystemComponent* TargetASC = Cast<UPOTAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(EventData.ContextHandle.GetHitResult()->GetActor())))
							{
								FGameplayTagContainer TargetTags;
								TargetASC->GetOwnedGameplayTags(TargetTags);
								NewHandle.Data.Get()->CapturedTargetTags.GetSpecTags().AppendTags(TargetTags);
							}
						}

						// Copy over set by caller magnitudes
						if (AbilitySpec)
						{
							NewHandle.Data->SetByCallerTagMagnitudes = AbilitySpec->SetByCallerTagMagnitudes;
							NewHandle.Data->SetByCallerTagMagnitudes.Append(SetByCallerMagnitudes);
							//NewHandle.Data->SetContext(EventData.ContextHandle);
						}

					}

					// Add Gameplay effect spec handle to container
					Spec.TargetGameplayEffectSpecs.Add(NewHandle);
					//ReturnSpec.TargetGameplayEffectSpecs.Last().Data.Get()->SetContext(EventData.ContextHandle);
				}
			}

			ReturnSpecs.Emplace(Spec);
		}
		
		
	}

	NonAbilityTargets.Shrink();
	return ReturnSpecs;
}

TArray<FPOTGameplayEffectContainerSpec> UPOTGameplayAbility::MakeEffectContainerSpec(FGameplayTag ContainerTag, const FGameplayEventData& EventData, const TMap<FGameplayTag, float>& SetByCallerMagnitudes, int32 OverrideGameplayLevel /*= -1*/)
{
	return MakeSpecificEffectContainerSpec(ContainerTag, EventData, EffectContainerMap, SetByCallerMagnitudes, 
		OverrideGameplayLevel);
}


TArray<FPOTGameplayEffectContainerSpec> UPOTGameplayAbility::MakeSpecificEffectContainerSpec(FGameplayTag ContainerTag, 
	const FGameplayEventData& EventData, TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap, const TMap<FGameplayTag, float>& SetByCallerMagnitudes, int32 OverrideGameplayLevel /*= -1*/)
{
	FPOTGameplayEffectContainer* FoundContainer = InEffectContainerMap.Find(ContainerTag);

	if (FoundContainer != nullptr)
	{
		return MakeEffectContainerSpecFromContainer(*FoundContainer, EventData, SetByCallerMagnitudes, OverrideGameplayLevel);
	}

	return TArray<FPOTGameplayEffectContainerSpec>();
}

TArray<FActiveGameplayEffectHandle> UPOTGameplayAbility::ApplyEffectContainerSpec(
	const FPOTGameplayEffectContainerSpec& ContainerSpec)
{
	TArray<FActiveGameplayEffectHandle> AllEffects;

	check(CurrentActorInfo);

	bool bClientPredict = CurrentActorInfo->AbilitySystemComponent->ScopedPredictionKey.IsValidForMorePrediction() && CurrentActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Type::Predicting;
	bool bIsAuth = CurrentActorInfo->IsNetAuthority();

	if (!(CurrentActorInfo->IsNetAuthority() || bClientPredict))
	{
		//UE_LOG(TitansLog, Warning, TEXT("Tried to apply effect container spec on client when not predicting. Ability = %s"), *GetName());
		//return AllEffects;
	}

	// Iterate list of effect specs and apply them to their target data
	for (const FGameplayEffectSpecHandle& SpecHandle : ContainerSpec.TargetGameplayEffectSpecs)
	{
		if (bIsAuth)
		{
			AllEffects.Append(K2_ApplyGameplayEffectSpecToTarget(SpecHandle, ContainerSpec.TargetData));
		}
		else
		{
			for (TSharedPtr<FGameplayAbilityTargetData> Data : ContainerSpec.TargetData.Data)
			{
				if (!Data.Get()) continue;

				for (TWeakObjectPtr<AActor> Actor : Data->GetActors())
				{
					UPOTAbilitySystemComponent* PotAsc = Cast<UPOTAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Actor.Get()));
					if (PotAsc && SpecHandle.Data.Get())
					{
						FGameplayEffectSpec SpecCopy = *SpecHandle.Data.Get();
						if (SpecCopy.GetDuration() == UGameplayEffect::INSTANT_APPLICATION)
						{
							PotAsc->ExecuteEffectClientPredicted(SpecCopy);
						}
					}
				}
			}
		}
	}

	for (const FNonAbilityTarget& NAT : NonAbilityTargets)
	{
		if ((NAT.bHasHitResult && !NAT.TargetHitResult.GetActor()) || (!NAT.TargetActor.IsValid()))
		{
			continue;
		}

		FGameplayCueParameters GCParams;
		UPOTAbilitySystemGlobals& WASG = UPOTAbilitySystemGlobals::Get();

		//If MAYBE non-ASC actors want to react to abilities hitting them (like destructibles)
		//we will just do a generic damage event.

		if (NAT.bHasHitResult && NAT.TargetHitResult.GetActor())
		{
			WASG.InitGameplayCueParameters_HitResult(GCParams, this, NAT.TargetHitResult);
			UGameplayStatics::ApplyPointDamage(NAT.TargetHitResult.GetActor(), CurrentEventMagnitude, -NAT.TargetHitResult.ImpactNormal, NAT.TargetHitResult,
				OwningPOTCharacter->GetController(), OwningPOTCharacter, nullptr);
		}
		else if (NAT.TargetActor.IsValid())
		{
			WASG.InitGameplayCueParameters_Actor(GCParams, this, NAT.TargetActor.Get());
			UGameplayStatics::ApplyDamage(NAT.TargetActor.Get(), CurrentEventMagnitude, 
				OwningPOTCharacter->GetController(), OwningPOTCharacter, nullptr);
		}


		for (auto It = NAT.CueContainer.CreateConstIterator(); It; ++It)
		{
			const FGameplayTag& Tag = *It;
			GCParams.OriginalTag = Tag;
			CurrentActorInfo->AbilitySystemComponent->ExecuteGameplayCue(Tag, GCParams);
		}
		
	}

	return AllEffects;
}

TArray<FActiveGameplayEffectHandle> UPOTGameplayAbility::ApplyEffectContainer(FGameplayTag ContainerTag, 
	const FGameplayEventData& EventData, const TMap<FGameplayTag, float>& SetByCallerMagnitudes, int32 OverrideGameplayLevel /*= -1*/)
{
	
	return ApplySpecifiedEffectContainer(ContainerTag, EventData, EffectContainerMap, SetByCallerMagnitudes, OverrideGameplayLevel);
}

TArray<FActiveGameplayEffectHandle> UPOTGameplayAbility::ApplySpecifiedEffectContainer(FGameplayTag ContainerTag, const FGameplayEventData& EventData, TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap, const TMap<FGameplayTag, float>& SetByCallerMagnitudes, int32 OverrideGameplayLevel /*= -1*/)
{
	const TArray<FPOTGameplayEffectContainerSpec>& Specs = MakeSpecificEffectContainerSpec(ContainerTag, EventData, 
		InEffectContainerMap, SetByCallerMagnitudes, OverrideGameplayLevel);


	TArray<FActiveGameplayEffectHandle> RetHandles;
	for (const FPOTGameplayEffectContainerSpec& Spec : Specs)
	{
		RetHandles.Append(ApplyEffectContainerSpec(Spec));
	}

	return RetHandles;
}

bool UPOTGameplayAbility::ExecuteEffectContainerCuesAtTransform(const FGameplayTag& ContainerTag, 
	const FTransform& DestinationTransform, TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap)
{
	check(CurrentActorInfo);
	FPOTGameplayEffectContainer* FoundContainer = InEffectContainerMap.Find(ContainerTag);

	if (FoundContainer != nullptr)
	{
		FGameplayTagContainer ContainerCueTags;
		if (FoundContainer->GetGameplayCueTags(ContainerCueTags))
		{
			FGameplayCueParameters GCParams;
			UPOTAbilitySystemGlobals& WASG = UPOTAbilitySystemGlobals::Get();

			WASG.InitGameplayCueParameters_Transform(GCParams, this, DestinationTransform);

			for (auto It = ContainerCueTags.CreateConstIterator(); It; ++It)
			{
				const FGameplayTag& Tag = *It;
				GCParams.OriginalTag = Tag;
				CurrentActorInfo->AbilitySystemComponent->ExecuteGameplayCue(Tag, GCParams);
			}

			return true;
		}
	}

	return false;
}

bool UPOTGameplayAbility::ExecuteEffectContainerCuesAtHit(const FGameplayTag& ContainerTag, 
	const FHitResult& HitResult, TMap<FGameplayTag, FPOTGameplayEffectContainer>& InEffectContainerMap)
{
	check(CurrentActorInfo);
	FPOTGameplayEffectContainer* FoundContainer = InEffectContainerMap.Find(ContainerTag);

	if (FoundContainer != nullptr)
	{
		FGameplayTagContainer ContainerCueTags;
		if (FoundContainer->GetGameplayCueTags(ContainerCueTags))
		{
			FGameplayCueParameters GCParams;
			UPOTAbilitySystemGlobals& WASG = UPOTAbilitySystemGlobals::Get();

			WASG.InitGameplayCueParameters_HitResult(GCParams, this, HitResult);

			for (auto It = ContainerCueTags.CreateConstIterator(); It; ++It)
			{
				const FGameplayTag& Tag = *It;
				GCParams.OriginalTag = Tag;
				CurrentActorInfo->AbilitySystemComponent->ExecuteGameplayCue(Tag, GCParams);
			}

			return true;
		}
	}

	return false;
}


float UPOTGameplayAbility::GetPlayRate_Implementation() const
{
	return PlayRate;
}

FGameplayEffectContextHandle UPOTGameplayAbility::GetContextFromOwner(FGameplayAbilityTargetDataHandle OptionalTargetData) const
{
	FGameplayEffectContextHandle ParentHandle = Super::GetContextFromOwner(OptionalTargetData);

	return ParentHandle;
}

FGameplayEffectContextHandle UPOTGameplayAbility::MakeEffectContext(const FGameplayAbilitySpecHandle Handle, 
	const FGameplayAbilityActorInfo *ActorInfo) const
{
	FGameplayEffectContextHandle Context = Super::MakeEffectContext(Handle, ActorInfo);
	FPOTGameplayEffectContext* WaEffectContext = StaticCast<FPOTGameplayEffectContext*>(Context.Get());

	WaEffectContext->SetKnockbackMode(KnockbackMode);
	WaEffectContext->SetKnockbackForce(CurrentKnockbackForce * KnockbackForceMultiplier);
	WaEffectContext->SetEventMagnitude(CurrentEventMagnitude);

	if (OverrideDamageType != EDamageType::DT_GENERIC)
	{
		WaEffectContext->SetDamageType(OverrideDamageType);
	}
	

	//#TODO Probably wrong, instigator is not necesarily owner
	WaEffectContext->AddInstigator(ActorInfo->OwnerActor.Get(), ActorInfo->OwnerActor.Get());

	return Context;
}

bool UPOTGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	OUT FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	check(ActorInfo);
	if (!ActorInfo) return true;
	if (AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(ActorInfo->OwnerActor.Get()))
	{
		if (IBaseCharacter->GetGodmode())
		{
			return true; // Always pass check for a player with godmode
		}
	}

	if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
	{
		FGameplayTagContainer ActiveTags;
		AbilitySystem->GetAggregatedTags(ActiveTags);
		if (!bAllowedWhenHomecaveBuffActive && ActiveTags.HasTag(FGameplayTag::RequestGameplayTag(NAME_BuffInHomecave)))
		{
			return false;
		}
	}

	return Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

void UPOTGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
	// Useful for abilities that use cost for checking if activation is allowed, but then apply the cost in a different way.
	if (bDontApplyCost) return;
	check(ActorInfo);
	if (!ActorInfo) return;
	if (AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(ActorInfo->OwnerActor.Get()))
	{
		if (IBaseCharacter->GetGodmode())
		{
			return; // Don't apply cost for a player with godmode
		}
	}
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
}

void UPOTGameplayAbility::RemoveActiveCooldown()
{
	UAbilitySystemComponent* const ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC)
	{
		const FGameplayTagContainer* CooldownTags = GetCooldownTags();
		if (CooldownTags && CooldownTags->Num() > 0)
		{
// 			FGameplayEffectQuery const Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(*CooldownTags);
// 			TArray< FActiveGameplayEffectHandle > ActiveCooldowns = ASC->GetActiveEffects(Query);
// 			for (const FActiveGameplayEffectHandle& AGEH : ActiveCooldowns)
// 			{
				ASC->RemoveActiveEffectsWithGrantedTags(*CooldownTags);
// 			}
		}
	}
}



void UPOTGameplayAbility::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	bool bCookingOrCommandlet = (Ar.IsCooking() || IsRunningCommandlet());
	if (Ar.IsSaving() && !bCookingOrCommandlet)
	{
		GatherWeaponTraceData(nullptr);
		GatherOverlapNotifyData();
	}
#endif //WITH_EDITOR

	Super::Serialize(Ar);
}

void UPOTGameplayAbility::PostLoad()
{
	Super::PostLoad();

	if (TimedTraceSections.Num() == 0)
	{
		if (!IsRunningCommandlet())
		{
			//UE_LOG(LogBlueprint, Warning, TEXT("'%s' has 0 TimedTraceSections, this can cause deterministic cooking issues please resave package and investigate."), *GetPathName());
			GatherWeaponTraceData(nullptr);
			GatherOverlapNotifyData();
		}
	}
}

void UPOTGameplayAbility::GetTimedTraceTransforms(TArray<FTimedTraceBoneGroup>& OutBoneGroups) const
{
	if (TimedTraceSections.IsValidIndex(TraceGroup))
	{
		OutBoneGroups = TimedTraceSections[TraceGroup].TimedTraceBoneGroups;
	}
}

FORCEINLINE class UAnimNotifyState_DoDamage* UPOTGameplayAbility::GetDoDamageNotifyForCurrentTraceGroup() const
{
	return Cast<UAnimNotifyState_DoDamage>(DoDamageNotifies[TraceGroup].NotifyStateClass);
}

void UPOTGameplayAbility::OnGameplayEvent(FGameplayTag EventTag, const FGameplayEventData* Payload)
{
	if (Payload == nullptr) return;
	if (!OwningPOTCharacter) return;

	const bool bClientPredict = CurrentActorInfo->AbilitySystemComponent->ScopedPredictionKey.IsValidForMorePrediction() && CurrentActivationInfo.ActivationMode == EGameplayAbilityActivationMode::Type::Predicting;
	const bool bAuth = OwningPOTCharacter->HasAuthority();
	const bool ServerAndClientAllowed = bAuth || bClientPredict;

	//Check if ability can be predicted (if on client) or is server
	if (!ServerAndClientAllowed)
	{
		UE_LOG(TitansLog, Log, TEXT("UPOTGameplayAbility::OnGameplayEvent - Can't be predicted, skipping event from client"));
		return;
	}

	CurrentEventMagnitude = Payload->EventMagnitude;
	if (!Payload->ContextHandle.IsValid())
	{
		const_cast<FGameplayEventData*>(Payload)->ContextHandle = OwningPOTCharacter->AbilitySystem->MakeEffectContext();
	}

	const FPOTGameplayEffectContext* WaEffectContext =
		StaticCast<const FPOTGameplayEffectContext*>(Payload->ContextHandle.Get());

	CurrentKnockbackForce = WaEffectContext ? WaEffectContext->GetKnockbackForce() : 1.0f;

	TMap<FGameplayTag, float> SetByCallerMagnitudes;
	if (bShouldUseChargeDuration) 
	{
		// Calculate ChargedDuration
		float NewTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		ChargedDuration = NewTime - PressedTime;

		// Calculate ChargePercentage to be 0-1 with 0 being MinChargeSeconds and 1 being MaxChargeSeconds
		float ChargePercentage = FMath::Clamp((GetChargedDuration() - MinChargeDuration) / (MaxChargeDuration - MinChargeDuration), 0.f, 1.f);

		// Calculate ChargedMultiplier to be between MinChargeMultiplier and MaxChargeMultiplier
		float ChargeMultiplier = FMath::Lerp(MinChargeMultiplier, MaxChargeMultiplier, ChargePercentage);

		SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(NAME_SetByCallerChargedMultiplier), ChargeMultiplier);
	}
	else
	{
		SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(NAME_SetByCallerChargedMultiplier), 1.0f);
	}


	float CharacterMovementSpeed = WaEffectContext->GetSmoothMovementSpeed();
	float MaxMovementSpeedMultiplier = GetMaxMovementMultiplierSpeed(); 

	if (MaxMovementSpeedMultiplier > 0.f)
	{
		float SpeedMultiplier = CharacterMovementSpeed > 0.f ? FMath::Clamp(CharacterMovementSpeed / MaxMovementSpeedMultiplier, 0.f, 1.f) : 0.f;

		if (WaEffectContext != nullptr)
		{
			CurrentKnockbackForce = WaEffectContext->GetKnockbackForce() * SpeedMultiplier; 
		}
		else
		{
			CurrentKnockbackForce = 1.0f * SpeedMultiplier;
		}

		SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(NAME_SetByCallerMovementMultiplier), SpeedMultiplier);

		// Temporarily setting MovementMultiplier to always be 1.0 - Poncho
		// SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(NAME_SetByCallerMovementMultiplier), 1.0f);
	}
	else
	{
		SetByCallerMagnitudes.Add(FGameplayTag::RequestGameplayTag(NAME_SetByCallerMovementMultiplier), 1.0f);
	}

	ApplySpecifiedEffectContainer(EventTag, *Payload, EffectContainerMap, SetByCallerMagnitudes);
	//ApplySpecifiedEffectContainer(EventTag, *Payload, AdditionalEffectContainerMap);


}


#if WITH_EDITOR
void UPOTGameplayAbility::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FProperty* PropertyThatChanged = PropertyChangedEvent.Property;

	if (PropertyThatChanged != NULL)
	{
		if (OnMoveAnyPropertyChanged.IsBound())
		{
			OnMoveAnyPropertyChanged.Broadcast();
		}

		if (PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(UPOTGameplayAbility, AbilityMontageSet))
		{
			GatherWeaponTraceData(nullptr);
			GatherOverlapNotifyData();
		}
	}
}


FDelegateHandle UPOTGameplayAbility::RegisterOnMoveAnyPropertyChanged(const FOnMoveAnyPropertyChanged& Delegate)
{
	return OnMoveAnyPropertyChanged.Add(Delegate);
}


void UPOTGameplayAbility::UnregisterOnMoveAnyPropertyChanged(FDelegateHandle Handle)
{
	OnMoveAnyPropertyChanged.Remove(Handle);
}

EDataValidationResult UPOTGameplayAbility::IsDataValid(class FDataValidationContext& Context) const
{
	Super::IsDataValid(Context);
	EDataValidationResult DataValidationResult = EDataValidationResult::Valid;
	FString ErrorText;
	/*if (CooldownGameplayEffectClass == nullptr)
	{
		ErrorText = GetName() + ": Cooldown gameplay effect class is not set ";
		ValidationErrors.Add(FText::FromString(ErrorText));
		DataValidationResult = EDataValidationResult::Invalid;
	}*/
	for (auto EElement : EffectContainerMap)
	{
		if (!EElement.Key.IsValid())
		{
			ErrorText = GetName() + ": gameplay effect key is not set ";
			Context.AddError(FText::FromString(ErrorText));
			DataValidationResult = EDataValidationResult::Invalid;
		}
		for (auto Entries : EElement.Value.ContainerEntries)
		{
			if (Entries.TargetType == nullptr)
			{
				ErrorText = GetName() + ": target type is not set!";
				Context.AddError(FText::FromString(ErrorText));
				DataValidationResult = EDataValidationResult::Invalid;
			}
			if (Entries.TargetGameplayEffects.Num() == 0)
			{
				ErrorText = GetName() + ": target gameplay effects are not set!";
				Context.AddError(FText::FromString(ErrorText));
				DataValidationResult = EDataValidationResult::Invalid;
			}
		}
	}
	return DataValidationResult;
}


#endif

void UPOTGameplayAbility::ResetTracingData()
{
	TraceGroup = 0;
}

void UPOTGameplayAbility::SetNextTraceGroup()
{

	//UE_LOG(LogTemp, Log, TEXT("WTRACE:  SetNextTraceGroup"));
	if (OwningPOTCharacter != nullptr)
	{
		if (UAnimInstance* WAI = OwningPOTCharacter->GetMesh()->GetAnimInstance())
		{
			if (CurrentMontage == nullptr)
			{
				CurrentMontage = AbilityMontageSet.CharacterMontage;
			}
			//UE_LOG(LogTemp, Log, TEXT("WTRACE:  SetNextTraceGroup ==>  CurrentMontage:  %s"), *CurrentMontage->GetName());

			FName CurrentSectionName = WAI->Montage_GetCurrentSection(CurrentMontage);

			int32 CurrentSectionID = CurrentMontage->GetSectionIndex(CurrentSectionName);

			if (CurrentSectionID == WAI->Montage_GetNextSectionID(CurrentMontage, CurrentSectionID))
			{
				float CurrentSectionStartTime;
				float CurrentSectionEndTime;

				CurrentMontage->GetSectionStartAndEndTime(CurrentSectionID, CurrentSectionStartTime, CurrentSectionEndTime);

				bool bHasMoreDoDamages = false;
				// Check if there is another DoDamageNotify after the current one in the same section
				for (int i = TraceGroup + 1; i < TimedTraceSections.Num(); i++)
				{
					if (TimedTraceSections[i].TimedTraceBoneGroups[0].MontageTime > CurrentSectionEndTime)
					{
						break;
					}
					else
					{
						bHasMoreDoDamages = true;
						break;
					}
				}

				//UE_LOG(LogTemp, Log, TEXT("WTRACE:  SetNextTraceGroup ==>  bHasMoreDoDamages:  %s"), (bHasMoreDoDamages ? TEXT("TRUE") : TEXT("FALSE")));

				if (bHasMoreDoDamages)
				{
					IncrementTraceGroup();
				}
				else
				{
					TraceGroup = 0;
					for (int i = 0; i < CurrentSectionID; i++)
					{
						TraceGroup += SectionDamageNotifyCount[i];
					}
				}
			}
			else
			{
				IncrementTraceGroup();
			}

		}
	}

	//UE_LOG(LogTemp, Log, TEXT("WTRACE:  SetNextTraceGroup ==>  TraceGroup:  %d"), TraceGroup);
}

#if WITH_EDITOR
void UPOTGameplayAbility::GenerateFromScript()
{
	if (MontageAbilityGeneration.IsReady())
	{
		AbilityMontageSet.CharacterMontage = MontageAbilityGeneration.CharacterMontage;

		// Assign Damage GameplayEffect
		if (MontageAbilityGeneration.DamageGameplayEffectClass.Get() != nullptr)
		{
			FPOTGameplayEffectEntry WGEE{ { MontageAbilityGeneration.DamageGameplayEffectClass } };

			FPOTGameplayEffectContainerEntry WGECE;
			WGECE.TargetType = UPOTTargetType_UseEventData::StaticClass();
			WGECE.TargetGameplayEffects.Emplace(WGEE);

			FPOTGameplayEffectContainer WGEC;
			WGEC.ContainerEntries.Emplace(WGECE);
			EffectContainerMap.Emplace(FGameplayTag::RequestGameplayTag(NAME_EventMontageSharedWeaponHit), WGEC);
		}

		// Assign Cooldown GameplayEffect
		if (MontageAbilityGeneration.CooldownGameplayEffectClass.Get() != nullptr)
		{
			CooldownGameplayEffectClass = MontageAbilityGeneration.CooldownGameplayEffectClass;
		}

		// Assign Cost GameplayEffect
		if (MontageAbilityGeneration.CostGameplayEffectClass.Get() != nullptr)
		{
			CostGameplayEffectClass = MontageAbilityGeneration.CostGameplayEffectClass;
		}

	

		SetTemporaryFlags({ 
			"AbilityMontageSet", 
			"EffectContainerMap", 
			"CooldownGameplayEffectClass", 
			"CostGameplayEffectClass",
			"TimedTraceSections",
			"PredictionTimedTraceSections",
			"SectionDamageNotifyCount"
		});

	}
}

void UPOTGameplayAbility::SetTemporaryFlags(const TArray<FString>& Properties)
{
	for (const FString& PropertyName : Properties)
	{
		// Workaround so that property doesn't get overwritten with null by UE4 system
		FProperty* Property = GetClass()->FindPropertyByName(FName(*PropertyName));
		Property->SetPropertyFlags(CPF_DuplicateTransient);
		Property->SetPropertyFlags(CPF_ContainsInstancedReference);
	}
}



#endif

float ConvertTimeToAnimTime(FAnimSegment* AnimSegment, float Time)
{
	ensureAlways(AnimSegment);
	const TObjectPtr<UAnimSequenceBase> Anime = AnimSegment->GetAnimReference();
	const float SeqPlayRate = Anime ? Anime->RateScale : 1.f;
	const float FinalPlayRate = SeqPlayRate * AnimSegment->AnimPlayRate;
	const float PlayRate = (FMath::IsNearlyZero(FinalPlayRate) ? 1.f : FinalPlayRate);

	const float AnimLength = (AnimSegment->AnimEndTime - AnimSegment->AnimStartTime);
	const float AnimPositionUnWrapped = (Time - AnimSegment->StartPos) * PlayRate;

	// Figure out how many times animation is allowed to be looped.
	const float LoopCount = FMath::Min(FMath::FloorToInt(FMath::Abs(AnimPositionUnWrapped) / AnimLength),
		FMath::Max(AnimSegment->LoopingCount - 1, 0));
	// Position within AnimSequence
	const float AnimPoint = (PlayRate >= 0.f) ? AnimSegment->AnimStartTime : AnimSegment->AnimEndTime;

	const float AnimPosition = AnimPoint + (AnimPositionUnWrapped - float(LoopCount) * AnimLength);

	return AnimPosition;
}

void UPOTGameplayAbility::GatherWeaponTraceData(USkeletalMeshComponent* OwnerMesh, UAnimMontage* OverrideMontage)
{
	TraceGroup = 0;
	if (OverrideMontage == nullptr)
	{
		OverrideMontage = AbilityMontageSet.CharacterMontage;
	}

	if (TraceTransformRecordInterval <= 0.f)
	{
		return;
	}

	if (OverrideMontage == nullptr)
	{
		return;
	}

	USkeleton* Skel = OverrideMontage->GetSkeleton();
	if (Skel == nullptr)
	{
		return;
	}

	TimedTraceSections.Empty(TimedTraceSections.Num());

	const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();

	OriginalRootRotation = RefSkel.GetRefBonePose()[0].GetRotation();

	TArray<float> StartTimes;
	TArray<float> EndTimes;
	TArray<FSocketsToRecord> SocketsToRecord;

	SectionDamageNotifyCount.Reset(OverrideMontage->CompositeSections.Num());
	SectionDamageNotifyCount.AddDefaulted(OverrideMontage->CompositeSections.Num());

	int8 TraceCounter = 0;

	DoDamageNotifyIDs.Reset();

	for (const FAnimNotifyEvent& Event : OverrideMontage->Notifies)
	{
		if (Event.NotifyStateClass != nullptr && Event.NotifyStateClass->IsA<UAnimNotifyState_DoDamage>())
		{
			StartTimes.Add(Event.GetTriggerTime());
			EndTimes.Add(Event.GetEndTriggerTime());
			int32 SectionIndex = OverrideMontage->GetSectionIndexFromPosition(Event.GetTriggerTime());
			if (SectionDamageNotifyCount.IsValidIndex(SectionIndex))
			{
				SectionDamageNotifyCount[SectionIndex]++;
			}

			UAnimNotifyState_DoDamage* DoDmgNotifyState = Cast<UAnimNotifyState_DoDamage>(Event.NotifyStateClass);
			if (DoDmgNotifyState != nullptr)
			{
				DoDamageNotifyIDs.Emplace(DoDmgNotifyState->GetDamageUniqueID());

				FSocketsToRecord NewSockets;

				for (const FWeaponSlotConfiguration& SlotConfig : DoDmgNotifyState->SlotsConfiguration)
				{
					NewSockets.Emplace(SlotConfig.DmgBodyFilter);
				}

				SocketsToRecord.Emplace(NewSockets);
				TraceCounter++;
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Notify: %s"),
					Event.Notify != nullptr ? *Event.Notify->GetName() : TEXT("Null"));
			}
		}
	}

	check(StartTimes.Num() == EndTimes.Num());
	for (int32 i = 0; i < StartTimes.Num(); i++)
	{
		GetTimedTraceTransformGroup(StartTimes[i], EndTimes[i], SocketsToRecord[i], Skel, TimedTraceSections, OverrideMontage);
	}
}

void UPOTGameplayAbility::GatherOverlapNotifyData()
{
	TimedOverlapConfigurations.Empty();

	const UAnimMontage* const OverrideMontage = AbilityMontageSet.CharacterMontage;
	if (!OverrideMontage)
		return;

	for (const FAnimNotifyEvent& Event : OverrideMontage->Notifies)
	{
		if (!Event.NotifyStateClass || !Event.NotifyStateClass->IsA(UAnimNotifyState_DoShapeOverlaps::StaticClass()))
			continue;

		const UAnimNotifyState_DoShapeOverlaps* const OverlapsNotifyState = Cast<UAnimNotifyState_DoShapeOverlaps>(Event.NotifyStateClass);
		if (!OverlapsNotifyState)
			continue;

		OverlapNotifyIDs.Emplace(OverlapsNotifyState->GetOverlapUniqueID());

		for (const FAnimOverlapConfiguration& Config : OverlapsNotifyState->AnimOverlapConfigurations)
		{
			if (Config.OverlapRecordInterval <= 0)
			{
				UE_LOG(LogTemp, Error, TEXT("UPOTGameplayAbility::GatherOverlapNotifyData - Overlap timestamp interval is not greater than zero.  Could not create timestamps."));
				break;
			}

			FTimedOverlapConfiguration TimedOverlapConfig{};
			TimedOverlapConfig.AnimOverlapConfiguration = Config;

			//Create each overlap trigger timestamp
			float CurrentTime = Event.GetTriggerTime();
			while (CurrentTime < Event.GetEndTriggerTime())
			{
				TimedOverlapConfig.OverlapTriggerTimestamps.Add(CurrentTime);
				CurrentTime += Config.OverlapRecordInterval;
			}

			//Add ending trigger if needed.
			if (Config.bCheckOverlapOnNotifyEnd)
			{
				TimedOverlapConfig.OverlapTriggerTimestamps.Add(Event.GetEndTriggerTime());
			}

			TimedOverlapConfigurations.Add(TimedOverlapConfig);
		}
	}
}

void UPOTGameplayAbility::ResetOverlapTiming()
{
	for(FTimedOverlapConfiguration& OverlapConfiguration : TimedOverlapConfigurations)
	{
		OverlapConfiguration.ResetTriggerIndex();
	}
}

void UPOTGameplayAbility::GetTimedTraceTransformGroup(float Start, float End, const FSocketsToRecord& CurrentSocketGroup, USkeleton* Skel, TArray<FTimedTraceBoneSection>& TraceSections, UAnimMontage* Montage)
{
	FTimedTraceBoneSection NewTTTSection;
	const FReferenceSkeleton& RefSkel = Skel->GetReferenceSkeleton();

	float Time = Start;
	while (Time < End)
	{
		FAnimSegment* Result = Montage->SlotAnimTracks[0].AnimTrack.GetSegmentAtTime(Time);

		if (Result != nullptr)
		{
			TArray<FName> BoneNames;
			float AnimTime = ConvertTimeToAnimTime(Result, Time);//   Result->ConvertTrackPosToAnimPos(Time);

			UAnimSequence* AnimSeq = Cast<UAnimSequence>(Result->GetAnimReference());
			if (AnimSeq == nullptr)
			{
				UE_LOG(LogTemp, Error, TEXT("Montage %s has no anim sequence."), *Montage->GetName());
				return;
			}

			for (auto It = CurrentSocketGroup.SocketNamesSet.CreateConstIterator(); It; ++It)
			{
				const FName SocketOrBoneName = *It;
				USkeletalMeshSocket* Socket = Skel->FindSocket(SocketOrBoneName);
				FName BoneName = Socket != nullptr ? Socket->BoneName : SocketOrBoneName;
				FTransform GlobalTransform = FTransform::Identity;

				if (RefSkel.FindBoneIndex(BoneName) != INDEX_NONE)
				{
					if (!BoneNames.Contains(SocketOrBoneName))
					{
						BoneNames.Add(SocketOrBoneName);
					}
				}
			}

			NewTTTSection.TimedTraceBoneGroups.Add(FTimedTraceBoneGroup(Time, BoneNames));
		}

		Time += TraceTransformRecordInterval;
	}

	TraceSections.Emplace(NewTTTSection);
}

void UPOTGameplayAbility::ExtractBoneTransform(USkeleton* Skel, const FReferenceSkeleton& RefSkel, FName BoneName, UAnimSequence* AnimSeq, float AnimTime, bool bUseRawDataOnly, const bool bExtractRootMotion, FTransform& GlobalTransform)
{
	TArray<FName> BoneChain;
	do
	{
		BoneChain.Emplace(BoneName);

		int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);
		int32 ParentIndex = RefSkel.GetParentIndex(BoneIndex);

		if (RefSkel.IsValidIndex(ParentIndex))
		{
			BoneName = RefSkel.GetBoneName(ParentIndex);
		}
		else
		{
			BoneName = NAME_None;
		}

	} while (BoneName != NAME_None);

	FVector RootOffset = FVector::ZeroVector;
	FQuat RootRotationOffset = FQuat::Identity;
	for (int32 i = BoneChain.Num() - 1; i >= 0; i--)
	{
		BoneName = BoneChain[i];
		int32 BoneIndex = RefSkel.FindBoneIndex(BoneName);

		if (BoneIndex == INDEX_NONE)
		{
			break;
		}

		FTransform BoneTransform;
		GetBonePoseForTime(AnimSeq, BoneName, AnimTime, bExtractRootMotion, BoneTransform);

		if (i == BoneChain.Num() - 1)
		{
			RootOffset = BoneTransform.GetLocation();
			RootRotationOffset = BoneTransform.GetRotation();
			BoneTransform.SetRotation(OriginalRootRotation);

		}

		GlobalTransform = BoneTransform * GlobalTransform;



		// 		UE_LOG(LogTemp, Log, TEXT("%f - %s - %s (Cumulative: %s)"),
		// 			AnimTime, *BoneName.ToString(), *BoneTransform.GetLocation().ToString(),
		// 			*GlobalTransform.GetLocation().ToString());
	}


	GlobalTransform.AddToTranslation(-RootOffset);
	//GlobalTransform.ConcatenateRotation(RootRotationOffset.Inverse());
	GlobalTransform.SetScale3D(FVector(1.f, 1.f, 1.f));
}

void UPOTGameplayAbility::GetBonePoseForTime(const UAnimSequence* AnimationSequence, FName BoneName, float Time, bool bExtractRootMotion, FTransform& Pose)
{
	Pose.SetIdentity();
	if (AnimationSequence)
	{
		TArray<FName> BoneNameArray;
		TArray<FTransform> PoseArray;
		BoneNameArray.Add(BoneName);
		GetBonePosesForTime(AnimationSequence, BoneNameArray, Time, bExtractRootMotion, PoseArray);
		Pose = PoseArray[0];
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Animation Sequence supplied for GetBonePoseForTime"));
	}
}

void UPOTGameplayAbility::GetBonePosesForTime(const UAnimSequence* AnimationSequence, TArray<FName> BoneNames, float Time, bool bExtractRootMotion, TArray<FTransform>& Poses)
{
	Poses.Empty(BoneNames.Num());
	if (AnimationSequence)
	{
		Poses.AddDefaulted(BoneNames.Num());

		// Need this for FCompactPose
		FMemMark Mark(FMemStack::Get());

		if (IsValidTimeInternal(AnimationSequence, Time))
		{
			TArray<FBoneIndexType> RequiredBones;
			TArray<int32> FoundBoneIndices;

			FoundBoneIndices.AddZeroed(BoneNames.Num());

			for (int32 BoneNameIndex = 0; BoneNameIndex < BoneNames.Num(); ++BoneNameIndex)
			{
				const FName& BoneName = BoneNames[BoneNameIndex];
				const int32 BoneIndex = AnimationSequence->GetSkeleton()->GetReferenceSkeleton().FindRawBoneIndex(BoneName);

				FoundBoneIndices[BoneNameIndex] = INDEX_NONE;
				if (BoneIndex != INDEX_NONE)
				{
					FoundBoneIndices[BoneNameIndex] = RequiredBones.Add(BoneIndex);
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("Invalid bone name %s for Animation Sequence %s in GetBonePosesForTime"), *BoneName.ToString(), *AnimationSequence->GetName());
				}
			}

			if (RequiredBones.Num())
			{
				FBoneContainer BoneContainer(RequiredBones, UE::Anim::FCurveFilterSettings(), *AnimationSequence->GetSkeleton());
				BoneContainer.SetUseSourceData(true);
				BoneContainer.SetDisableRetargeting(true);
				FCompactPose Pose;
				Pose.SetBoneContainer(&BoneContainer);

				FBlendedCurve Curve;
				FAnimExtractContext Context;
				Context.bExtractRootMotion = bExtractRootMotion;
				Context.CurrentTime = Time;
				const bool bForceUseRawData = true;
				Curve.InitFrom(BoneContainer);

				UE::Anim::FStackAttributeContainer TempAttributes;
				FAnimationPoseData OutAnimationPoseData(Pose, Curve, TempAttributes);
				AnimationSequence->GetAnimationPose(OutAnimationPoseData, Context);

				for (int32 BoneNameIndex = 0; BoneNameIndex < BoneNames.Num(); ++BoneNameIndex)
				{
					const int32 BoneContainerIndex = FoundBoneIndices[BoneNameIndex];
					Poses[BoneNameIndex] = BoneContainerIndex != INDEX_NONE ? Pose.GetBones()[BoneContainerIndex] : FTransform::Identity;
				}
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("Invalid or no bone names specified to retrieve poses given  Animation Sequence %s in GetBonePosesForTime"), *AnimationSequence->GetName());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid time value %f for Animation Sequence %s supplied for GetBonePosesForTime"), Time, *AnimationSequence->GetName());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid Animation Sequence supplied for GetBonePosesForTime"));
	}
}

bool UPOTGameplayAbility::IsValidTimeInternal(const UAnimSequence* AnimationSequence, const float Time)
{
	return FMath::IsWithinInclusive(Time, 0.0f, AnimationSequence->GetPlayLength());
}

void UPOTGameplayAbility::InputPressed(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputPressed(Handle, ActorInfo, ActivationInfo);
	if(bIsPressed == true) return;	// prevent people using abilities twice with two hotkeys
	bIsPressed = true;
	ChargedDuration = 0;
	PressedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get()))
	{
		if (bShouldAutoRelease)
		{
			float TotalReleaseDuration = AutoReleaseDuration + MaxChargeDuration;
			if (TotalReleaseDuration > 0.f)
			{
				FTimerDelegate TimerDel;
				TimerDel.BindUObject(this, &UPOTGameplayAbility::ReleaseAbility, AbilitySystem);
				GetWorld()->GetTimerManager().SetTimer(AutoReleaseTimerHandle, TimerDel, TotalReleaseDuration, false);
			}
			else
			{
				ReleaseAbility(AbilitySystem);
			}
		}

		for (FGameplayTag& Tag : InputTags)
		{
			AbilitySystem->AddReplicatedLooseGameplayTag(Tag);
		}
	}
}

void UPOTGameplayAbility::InputReleased(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	Super::InputReleased(Handle, ActorInfo, ActivationInfo);

	UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(ActorInfo->AbilitySystemComponent.Get());
	ReleaseAbility(AbilitySystem);

	if ((bShouldCancelBeforeDuration && bShouldUseChargeDuration && ChargedDuration <= ChargeCancelThreshold) || bEndAbilityOnInputRelease)
	{
		FScopedPredictionWindow Prediction(AbilitySystem, true);
		CancelAbility(Handle, ActorInfo, ActivationInfo, true);
		return;
	}
	else if (bCancelSoundsOnRelease)
	{
		if (AIDinosaurCharacter* IDinoOwner = Cast<AIDinosaurCharacter>(AbilitySystem->GetOwnerActor()))
		{
			IDinoOwner->FadeOutVocalSounds();
		}
	}

}

void UPOTGameplayAbility::ReleaseAbility(UPOTAbilitySystemComponent* AbilitySystem)
{
	const float NewTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	ChargedDuration = NewTime - PressedTime;

	bIsPressed = false;

	GetWorld()->GetTimerManager().ClearTimer(AutoReleaseTimerHandle);
	if (!ensure(AbilitySystem)) return;
	
	for (FGameplayTag& Tag : InputTags)
	{
		if (AbilitySystem->HasReplicatedLooseGameplayTag(Tag))
		{
			AbilitySystem->RemoveReplicatedLooseGameplayTag(Tag);
		}
	}	
}

#if WITH_EDITOR

void UPOTGameplayAbility::PostCDOCompiled(const FPostCDOCompiledContext& Context)
{
	Super::PostCDOCompiled(Context);
	CheckBlockScalarMigration();
}

#endif

void UPOTGameplayAbility::CheckBlockScalarMigration()
{
	if (bBlockAbility && BlockScalarPerDamageType.IsEmpty())
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		BlockScalarPerDamageType.Empty();
		if (!BlockScalarBaseDamage.IsNull())
		{
			BlockScalarPerDamageType.Add(EDamageEffectType::DAMAGED, BlockScalarBaseDamage);
		}
		if (!BlockScalarBleed.IsNull())
		{
			BlockScalarPerDamageType.Add(EDamageEffectType::BLEED, BlockScalarBleed);
		}
		if (!BlockScalarVenom.IsNull())
		{
			BlockScalarPerDamageType.Add(EDamageEffectType::VENOM, BlockScalarVenom);
		}
		if (!BlockScalarPoison.IsNull())
		{
			BlockScalarPerDamageType.Add(EDamageEffectType::POISONED, BlockScalarPoison);
		}
		if (!BlockScalarKnockback.IsNull())
		{
			BlockScalarPerDamageType.Add(EDamageEffectType::KNOCKBACK, BlockScalarKnockback);
		}
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
}

void UPOTGameplayAbility::PostInitProperties()
{
	Super::PostInitProperties();
	CheckBlockScalarMigration();
}

bool UPOTGameplayAbility::IsPressed() const
{
	return bIsPressed;
}

void UPOTGameplayAbility::CommitExecute(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo)
{
	// Not calling Super function because we dont want it to apply cooldown.

	ApplyCost(Handle, ActorInfo, ActivationInfo);
}

float UPOTGameplayAbility::GetChargedDuration() const
{
	return ChargedDuration;
}

FGameplayTagContainer UPOTGameplayAbility::GetAbilityTriggerTags() const
{
	FGameplayTagContainer Tags;
	for (const FAbilityTriggerData& AbilityTrigger : AbilityTriggers)
	{
		Tags.AddTag(AbilityTrigger.TriggerTag);
	}
	return Tags;
}

TArray<FAbilityTriggerData>& UPOTGameplayAbility::GetAbilityTriggers()
{
	return AbilityTriggers;
}

bool UPOTGameplayAbility::IsPassive() const
{
	return AbilityTags.HasTagExact(FGameplayTag::RequestGameplayTag(NAME_AbilityPassive));
}

void UPOTGameplayAbility::K2_EndAbility()
{
	// Ending the ability this way can be caused by the server telling the client to stop after rejecting an activation.
	check(CurrentActorInfo);

	bool bReplicateEndAbility = true;
	bool bWasCancelled = false;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, bReplicateEndAbility, bWasCancelled);	
}
