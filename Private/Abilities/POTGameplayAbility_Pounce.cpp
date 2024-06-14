// Copyright 2019-2023 Alderon Games Pty Ltd, All Rights Reserved.


#include "Abilities/POTGameplayAbility_Pounce.h"
#include "Player/IBaseCharacter.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/ICharacterMovementComponent.h"
#include "Abilities/CoreAttributeSet.h"

UPOTGameplayAbility_Pounce::UPOTGameplayAbility_Pounce() : Super()
{
	bAllowWhenLatchedToCharacter = true;
	AutoCancelMovementModes = { ECustomMovementType::DIVING, ECustomMovementType::SWIMMING };
}

bool UPOTGameplayAbility_Pounce::GetPounceSocket_Implementation(const FHitResult& HitResult, FName& OutSocket)
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return false;
	}

	OutSocket = OwningPOTCharacter->GetNearestPounceAttachSocket(HitResult);
	return OutSocket != NAME_None;
}

void UPOTGameplayAbility_Pounce::BindOwnerEvents_Implementation(bool bUnbind)
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	if (bUnbind)
	{
		OwningPOTCharacter->OnAttached.RemoveDynamic(this, &UPOTGameplayAbility_Pounce::OnAttached);
		OwningPOTCharacter->OnDetached.RemoveDynamic(this, &UPOTGameplayAbility_Pounce::OnDetached);
		OwningPOTCharacter->OnCharacterLanded.RemoveDynamic(this, &UPOTGameplayAbility_Pounce::OnLanded);
		return;
	}

	OwningPOTCharacter->OnAttached.AddUniqueDynamic(this, &UPOTGameplayAbility_Pounce::OnAttached);
	OwningPOTCharacter->OnDetached.AddUniqueDynamic(this, &UPOTGameplayAbility_Pounce::OnDetached);
	OwningPOTCharacter->OnCharacterLanded.AddUniqueDynamic(this, &UPOTGameplayAbility_Pounce::OnLanded);
}

void UPOTGameplayAbility_Pounce::OnAttached_Implementation(AIBaseCharacter* NewTarget)
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	OwningPOTCharacter->CharacterTags.AddTag(LatchedGameplayTag);

	K2_EndAbility();
}

void UPOTGameplayAbility_Pounce::OnDetached_Implementation(AIBaseCharacter* OldTarget)
{
	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	OwningPOTCharacter->CharacterTags.RemoveTag(LatchedGameplayTag);

	BindOwnerEvents(true);

	if (OwningPOTCharacter->IsLatched())
	{
		return;
	}

	K2_EndAbility();
}

void UPOTGameplayAbility_Pounce::OnLanded_Implementation(const FHitResult& Hit)
{
	K2_EndAbility();

	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	UICharacterMovementComponent* const CharMovementComponent = Cast<UICharacterMovementComponent>(OwningPOTCharacter->GetMovementComponent());
	if (!ensureAlways(CharMovementComponent))
	{
		return;
	}

	CharMovementComponent->SetMovementMode(MOVE_Walking);
}

void UPOTGameplayAbility_Pounce::DoAttach_Implementation(USkeletalMeshComponent* TargetMesh, const FName& SocketName)
{
	if (!ensureAlways(OwningPOTCharacter) || 
		!ensureAlways(OwningPOTCharacter->HasAuthority()) ||
		!ensureAlways(TargetMesh))
	{
		return;
	}

	OwningPOTCharacter->SetAttachTarget(TargetMesh, SocketName, EAttachType::AT_Latch);
}

void UPOTGameplayAbility_Pounce::K2_EndAbility()
{
	Super::K2_EndAbility();

	if (!ensureAlways(OwningPOTCharacter))
	{
		return;
	}

	if (!OwningPOTCharacter->IsLatched())
	{
		BindOwnerEvents(true);
	}

	StopCurrentAbilityMontage();

	if (AbilityTaskRootMotion)
	{
		AbilityTaskRootMotion->EndTask();
		AbilityTaskRootMotion = nullptr;
	}
}

void UPOTGameplayAbility_Pounce::K2_PostMontageActivateAbility_Implementation()
{
	Super::K2_PostMontageActivateAbility_Implementation();

	if (!ensureAlways(OwningPOTCharacter) || 
		!ensureAlways(StrengthOverTime))
	{
		return;
	}

	BindOwnerEvents(false);

	if (OwningPOTCharacter->IsLatched())
	{
		// Only let the client initiate unlatching this way. -Poncho
		if (OwningPOTCharacter->IsLocallyControlled())
		{
			OwningPOTCharacter->ServerRequestClearAttachTarget();
		}

		return;
	}

	const FVector WorldDirection = UKismetMathLibrary::RotateAngleAxis(OwningPOTCharacter->GetActorForwardVector(), PounceAngle, OwningPOTCharacter->GetActorRightVector());
	const float GrowthLevel = OwningPOTCharacter->GetGrowthLevel();
	const float Strength = PounceBaseStrength.GetValueAtLevel(GrowthLevel);
	const float Duration = PounceBaseDuration.GetValueAtLevel(GrowthLevel);

	AbilityTaskRootMotion = UAbilityTask_ApplyRootMotionConstantForce::ApplyRootMotionConstantForce(
		this, 
		RootMotionInstanceName,
		WorldDirection, 
		Strength, 
		Duration, 
		true, 
		StrengthOverTime, 
		ERootMotionFinishVelocityMode::MaintainLastRootMotionVelocity,
		FVector::ZeroVector, 
		0.0f, 
		false
	);

	if (!ensureAlways(AbilityTaskRootMotion))
	{
		return;
	}

	AbilityTaskRootMotion->OnFinish.AddDynamic(this, &UPOTGameplayAbility_Pounce::RootMotionFinished);

	if (!OwningPOTCharacter->HasAuthority())
	{	
		// Return now because we only want the latching initiated on the server. -Poncho
		return;
	}

	AbilityTaskWaitGameplayEvent = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this,
		WeaponHitGameplayTag,
		nullptr,
		true,
		true
	);

	if (!ensureAlways(AbilityTaskWaitGameplayEvent))
	{
		return;
	}

	AbilityTaskWaitGameplayEvent->EventReceived.AddDynamic(this, &UPOTGameplayAbility_Pounce::WeaponHitGameplayEvent);
	AbilityTaskWaitGameplayEvent->Activate();
}

void UPOTGameplayAbility_Pounce::RootMotionFinished_Implementation()
{
	if (!ensureAlways(OwningPOTCharacter) ||
		OwningPOTCharacter->IsLatched())
	{
		return;
	}

	K2_EndAbility();
}

void UPOTGameplayAbility_Pounce::WeaponHitGameplayEvent_Implementation(FGameplayEventData Payload)
{
	if (!ensureAlways(OwningPOTCharacter) || 
		!OwningPOTCharacter->HasAuthority())
	{
		return;
	}
	
	const FHitResult HitResult = UAbilitySystemBlueprintLibrary::GetHitResultFromTargetData(Payload.TargetData, 0);

	FName PounceSocket = NAME_None;
	if (!GetPounceSocket(HitResult, PounceSocket))
	{
		K2_EndAbility();
		return;
	}

	const AIBaseCharacter* const HitCharacter = Cast<AIBaseCharacter>(HitResult.GetActor());
	if (!CanLatchTo(HitCharacter))
	{
		K2_EndAbility();
		return;
	}

	DoAttach(Cast<USkeletalMeshComponent>(HitResult.GetComponent()), PounceSocket);
}

bool UPOTGameplayAbility_Pounce::CanLatchTo_Implementation(const AIBaseCharacter* HitCharacter) const
{
	if (!ensureAlways(HitCharacter) ||
		!ensureAlways(OwningPOTCharacter))
	{
		return false;
	}

	const UAbilitySystemComponent* const HitAbilitySystemComponent = HitCharacter->GetAbilitySystemComponent();
	if (HitCharacter->IsHomecaveBuffActive())
	{
		return false;
	}

	const UAbilitySystemComponent* const OwnerAbilitySystemComponent = OwningPOTCharacter->GetAbilitySystemComponent();
	if (!ensureAlways(OwnerAbilitySystemComponent))
	{
		return false;
	}

	if (bCheckWeight)
	{
		const float VictimWeight = HitAbilitySystemComponent->GetNumericAttribute(UCoreAttributeSet::GetCombatWeightAttribute());
		const float AttackerWeight = OwnerAbilitySystemComponent->GetNumericAttribute(UCoreAttributeSet::GetCombatWeightAttribute());

		if (VictimWeight < AttackerWeight * AttackerWeightMultiplier)
		{	
			return false;
		}
	}

	if (bCheckSize)
	{
		const float VictimSize = HitCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
		const float AttackerSize = OwningPOTCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius();
		if (VictimSize < AttackerSize * AttackerSizeMultiplier)
		{
			return false;
		}
	}

	return true;
}

bool UPOTGameplayAbility_Pounce::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */, const FGameplayTagContainer* TargetTags /* = nullptr */, OUT FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AIBaseCharacter* const ContextCharacter = Cast<AIBaseCharacter>(ActorInfo->OwnerActor);
	if (!ContextCharacter)
	{
		return false;
	}

	if (ContextCharacter->IsLatched())
	{
		// Always allow while latched.
		return true;
	}

	const UICharacterMovementComponent* const CharMovementComponent = Cast<UICharacterMovementComponent>(ActorInfo->MovementComponent);
	if (!CharMovementComponent)
	{
		return false;
	}

	return ((CharMovementComponent->IsFalling() || CharMovementComponent->IsFlying()) && bCanUseOnLand) || 
		(CharMovementComponent->IsSwimming() && bCanUseUnderwater);
}
