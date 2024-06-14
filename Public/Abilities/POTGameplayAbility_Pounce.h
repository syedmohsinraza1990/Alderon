// Copyright 2019-2023 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/POTGameplayAbility.h"
#include "ScalableFloat.h"
#include "AttributeSet.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "POTGameplayAbility_Pounce.generated.h"

class AIBaseCharacter;
class UCurveFloat;
class UAbilityTask_ApplyRootMotionConstantForce;
class UAbilityTask_WaitGameplayEvent;

UCLASS(Blueprintable)
class PATHOFTITANS_API UPOTGameplayAbility_Pounce : public UPOTGameplayAbility
{
	GENERATED_BODY()
	
public:

	UPOTGameplayAbility_Pounce();

	FORCEINLINE UAbilityTask_ApplyRootMotionConstantForce* GetAbilityTaskRootMotion() const { return AbilityTaskRootMotion; }

protected:

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool GetPounceSocket(const FHitResult& HitResult, FName& OutSocket);
	virtual bool GetPounceSocket_Implementation(const FHitResult& HitResult, FName& OutSocket);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void BindOwnerEvents(bool bUnbind);
	virtual void BindOwnerEvents_Implementation(bool bUnbind);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnAttached(AIBaseCharacter* NewTarget);
	virtual void OnAttached_Implementation(AIBaseCharacter* NewTarget);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnDetached(AIBaseCharacter* OldTarget);
	virtual void OnDetached_Implementation(AIBaseCharacter* OldTarget);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void OnLanded(const FHitResult& Hit);
	virtual void OnLanded_Implementation(const FHitResult& Hit);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void DoAttach(USkeletalMeshComponent* TargetMesh, const FName& SocketName);
	virtual void DoAttach_Implementation(USkeletalMeshComponent* TargetMesh, const FName& SocketName);

	virtual void K2_EndAbility() override;

	virtual void K2_PostMontageActivateAbility_Implementation() override;

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags /* = nullptr */, const FGameplayTagContainer* TargetTags /* = nullptr */, OUT FGameplayTagContainer* OptionalRelevantTags /* = nullptr */) const override;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void RootMotionFinished();
	virtual void RootMotionFinished_Implementation();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void WeaponHitGameplayEvent(FGameplayEventData Payload);
	virtual void WeaponHitGameplayEvent_Implementation(FGameplayEventData Payload);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool CanLatchTo(const AIBaseCharacter* HitCharacter) const;
	bool CanLatchTo_Implementation(const AIBaseCharacter* HitCharacter) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FScalableFloat PounceBaseStrength = FScalableFloat(50.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FScalableFloat PounceBaseDuration = FScalableFloat(1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float PounceAngle = -12.0f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCanUseUnderwater = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bCanUseOnLand = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName RootMotionInstanceName = TEXT("Pounce");

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag LatchedGameplayTag = FGameplayTag::RequestGameplayTag(TEXT("Debuff.Latched"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag WeaponHitGameplayTag = FGameplayTag::RequestGameplayTag(TEXT("Event.Montage.Shared.WeaponHit"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UCurveFloat* StrengthOverTime = nullptr;

	// If true, the attacker's weight must be less than the victim's weight.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCheckWeight = true;

	// Multiply the attacker's weight by AttackerWeightMultiplier when checking weight requirements.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackerWeightMultiplier = 1.0f;

	// If true, the attacker's size must be less than the victim's size.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bCheckSize = true;

	// Multiply the attacker's size by AttackerSizeMultiplier when checking weight requirements.
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackerSizeMultiplier = 1.0f;

	UPROPERTY()
	UAbilityTask_ApplyRootMotionConstantForce* AbilityTaskRootMotion = nullptr;

	UPROPERTY()
	UAbilityTask_WaitGameplayEvent* AbilityTaskWaitGameplayEvent = nullptr;
};
