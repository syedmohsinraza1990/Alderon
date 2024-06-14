// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "GameFramework/Character.h"
#include "PathOfTitans/PathOfTitans.h"
#include "ITypes.h"
#include "World/IWater.h"
#include "Interfaces/UsableInterface.h"
#include "Components/WidgetComponent.h"
#include "TimerManager.h"
#include "GameFramework/DamageType.h"
#include "Runtime/AIModule/Classes/GenericTeamAgentInterface.h"
#include "Components/TimelineComponent.h"
#include "Quests/IQuestManager.h"
#include "Quests/IQuest.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayEffect.h"
#include "AbilitySystemInterface.h"
#include "InputActionValue.h"
#include "Abilities/CoreAttributeSet.h"
#include "PhysicsEngine/PhysicalAnimationComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Abilities/POTAbilityTypes.h"
#include "Abilities/POTAbilityAsset.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Components/ISpringArmComponent.h"
#include "CaveSystem/IPlayerCaveMain.h"
#include "Interfaces/CarryInterface.h"
#include "IBaseCharacter.generated.h"

class UObject;
class USpringArmComponent;
class UCameraComponent;
class UAudioComponent;
class UCurveFloat;
class UTimelineComponent;
class AIBaseItem;
class UIQuest;
class AIFoodItem;
class UAnimMontage;
class UInputAction;
class AIPlayerCaveExtension;
class UHomeCaveExtensionDataAsset;
class UHomeCaveDecorationDataAsset;
class AICritterPawn;
class AIQuestItem;
class AIMoveToQuest;
class AIPOI;

enum class EFootstepType : uint8;

USTRUCT(BlueprintType)
struct FPOTParticleDefinition
{
	GENERATED_BODY()
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UFXSystemAsset> FXAsset;
		
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayAttribute Attribute;
		
};

struct PATHOFTITANS_API FAsyncRequest
{
	FAsyncRequest()
		: RequestId(0)
	{}

	/** Returns the assigned request id for this request. */
	FORCEINLINE uint64 GetRequestId() const { return RequestId; }

	/** Assigned when the async load request is being made. Internal use only, you shouldn't call this. */
	void SetRequestId(uint64 NewRequestId) { RequestId = NewRequestId; }
	
protected:

	/** Request id that should be assigned by whatever object is making the request, used to find the matching streamable handle. */
	uint64 RequestId;
};

/**
 * Since FAsyncRequest is not a USTRUCT(), UHT does not know about it and whines if we try to make our USTRUCT()s derive from it.
 * The CPP definition allows us to "hide" code from the UHT. Use this definition with caution. UHT doesn't need to know about FAsyncRequest in my case.
 */

USTRUCT(BlueprintType)
struct PATHOFTITANS_API FSpawnAttachedSoundParams
#if CPP
	: public FAsyncRequest
#endif
{
	GENERATED_BODY()

	FSpawnAttachedSoundParams()
		: SoundCue(nullptr)
		, AttachName(NAME_None)
		, GrowthParameterName(NAME_None)
		, bIsAnimNotify(false)
		, bShouldFollow(true)
		, VolumeMultiplier(1.0f)
		, PitchMultiplier(1.0f)
	{}
	
	/** The sound to play on the attached audio component. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	TSoftObjectPtr<USoundBase> SoundCue;

	/** The name of the attachment point for the audio component, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	FName AttachName;

	/** The name of the growth parameter to set on the audio component, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	FName GrowthParameterName;

	/** Whether or not this is being played by an anim notify. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	bool bIsAnimNotify;

	/** Whether or not this should be attached to the character, or just played at the character's current location. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	bool bShouldFollow;

	/** Multiplier to apply to the audio component's volume. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	float VolumeMultiplier;

	/** Multiplier to apply to the audio component's pitch. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Attached Sound Params")
	float PitchMultiplier;
};

USTRUCT(BlueprintType)
struct PATHOFTITANS_API FSpawnFootstepSoundParams
#if CPP
	: public FAsyncRequest
#endif
{
	GENERATED_BODY()

	FSpawnFootstepSoundParams();

	/** The sound to play on the attached audio component. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	TSoftObjectPtr<USoundBase> SoundCue;

	/** The location of the footstep, used if bShouldFollow is false. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	FVector FootstepLocation;

	/** The type of footstep that triggered this sound. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	EFootstepType FootstepType;
	
	/** The name of the attachment point for the audio component, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	FName AttachName;

	/** The name of the growth parameter to set on the audio component, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	FName GrowthParameterName;

	/** The name of the footstep type parameter to set on the audio component, if any. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	FName FootstepTypeParameterName;

	/** Whether or not this should be attached to the character, or just played at the character's current location. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	bool bShouldFollow;

	/** Multiplier to apply to the audio component's volume. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	float VolumeMultiplier;

	/** Multiplier to apply to the audio component's pitch. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Sound Params")
	float PitchMultiplier;
};

USTRUCT(BlueprintType)
struct PATHOFTITANS_API FSpawnFootstepParticleParams
#if CPP
	: public FAsyncRequest
#endif
{
	GENERATED_BODY()

	FSpawnFootstepParticleParams()
		: ParticleSystem(nullptr)
		, Location(FVector::ZeroVector)
		, Rotation(FRotator::ZeroRotator)
		, Size(1.0f)
	{}

	/** The particle system to spawn for the footstep. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Particle Params")
	TSoftObjectPtr<UParticleSystem> ParticleSystem;

	/** The location at which we should spawn the particle. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Particle Params")
	FVector Location;

	/** The rotation at which we should spawn the particle. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Particle Params")
	FRotator Rotation;

	/** The size multiplier of the particle. */
	UPROPERTY(BlueprintReadWrite, Category = "Spawn Footstep Particle Params")
	float Size;
};

#ifndef DEBUG_WEAPONS
#define DEBUG_WEAPONS (WITH_EDITOR || UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
#endif

UENUM(BlueprintType)
enum class EStanceType : uint8
{
	Default				UMETA(DisplayName = "Default"),
	Resting				UMETA(DisplayName = "Resting"),
	Sleeping			UMETA(DisplayName = "Sleeping")
};

UENUM(BlueprintType)
enum class ECheckTraceType : uint8
{
	CTT_PIVOT					UMETA(DisplayName = "Pivot"),
	CTT_BOUNDSCENTER			UMETA(DisplayName = "Bounds center"),
	CTT_NONE					UMETA(DisplayName = "None") // No trace
};

// used for blueprint editing and parsing only
USTRUCT(BlueprintType)
struct FDamageBodies
{
	GENERATED_BODY()
	public:
	FDamageBodies() { GeomNames = { NAME_None }; }
	explicit FDamageBodies(const FName& DefaultName) { GeomNames = { DefaultName }; }
public:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
	TArray<FName> GeomNames;
};

// used for blueprint editing and parsing only
USTRUCT()
struct FQuestDescriptiveTextCooldown
{
	GENERATED_BODY()
public:
	UPROPERTY()
	UIQuestItemTask* QuestItemTask;

	UPROPERTY()
	float Timestamp;

public:
	FQuestDescriptiveTextCooldown()
	{
		QuestItemTask = nullptr;
		Timestamp = -1.0f;
	}

	FQuestDescriptiveTextCooldown(UIQuestItemTask* NewQuestItemTask, float NewTimestamp)
	{
		QuestItemTask = NewQuestItemTask;
		Timestamp = NewTimestamp;
	}

	friend bool operator==(const FQuestDescriptiveTextCooldown& LHS, const FQuestDescriptiveTextCooldown& RHS)
	{
		return LHS.QuestItemTask && LHS.QuestItemTask == RHS.QuestItemTask && (abs(LHS.Timestamp - RHS.Timestamp) <= 300);
	}
};

/**
* Maps shapes to body names. Used to enable / disable a batch at once.
*/
USTRUCT(BlueprintType)
struct FBodyShapes
{
	GENERATED_BODY()

	public:
	FBodyShapes()
		: Name(NAME_None)
		, bEnabled(true)
	{
	}

	FBodyShapes(const FName& InName,
		const TArray<FPhysicsShapeHandle>& InShapes,
		const TArray<FCollisionShape>& InUShapes,
		const TArray<FTransform>& InShapeCompTransforms,
		const TArray<FTransform>& InShapeLocalTransforms,
		const USkeletalMeshComponent* SkelMesh)
		: Name(InName)
		, Shapes(InShapes)
		, UShapes(InUShapes)
		, ShapeComponentTransforms(InShapeCompTransforms)
		, ShapeLocalTransforms(InShapeLocalTransforms)
		, OwningComponent(TWeakObjectPtr<const USkeletalMeshComponent>(SkelMesh))
		, bEnabled(true)
	{
	}

	~FBodyShapes()
	{
		Shapes.Empty();
		UShapes.Empty();
	}

	FName Name;
	TArray<FPhysicsShapeHandle> Shapes;
	TArray<FCollisionShape> UShapes;
	TArray<FTransform> ShapeComponentTransforms;
	TArray<FTransform> ShapeLocalTransforms;
	TWeakObjectPtr<const USkeletalMeshComponent> OwningComponent;
	//FTransform BodyComponentTransform;
	bool bEnabled;

};

USTRUCT()
struct FBodyShapeSet
{
	GENERATED_BODY()

	public:
	UPROPERTY(Transient)
	TArray<FBodyShapes> DamageShapes;
	TMultiMap<FName, int32> BodyNameLookupMap;

public:
	FBodyShapeSet()
	{
	}

	FBodyShapeSet(const TArray<FBodyShapes>& InShapes, const TMultiMap<FName, int32> InBodyNameLookupMap)
		: DamageShapes(InShapes)
		, BodyNameLookupMap(InBodyNameLookupMap)
	{
	}

};

USTRUCT()
struct FBloodMaskPixelData
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	TArray<uint8> Pixels;

	UPROPERTY(VisibleAnywhere)
	FVector2D TextureSize = FVector2D(0,0);
};

USTRUCT(BlueprintType)
struct FUsableObjectReference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	int32 Item;

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	TWeakObjectPtr<UObject> Object;

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	float Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	EUseType ObjectUseType;

	FUsableObjectReference()
	{
		Reset();
	}

	void Reset()
	{
		Item = INDEX_NONE;
		Object = nullptr;
		Timestamp = 0;
		ObjectUseType = EUseType::UT_NONE;
	};
};

USTRUCT()
struct FLocalQuestTrackingItem
{
	GENERATED_USTRUCT_BODY()

public:
	UPROPERTY()
	UIQuest* Quest = nullptr;
	UPROPERTY()
	bool bTrack = false;
};

USTRUCT(BlueprintType)
struct FCarriableObjectReference
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	TWeakObjectPtr<UObject> Object;

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	bool bInstaCarry;

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	bool bDrop;

	UPROPERTY(BlueprintReadOnly, Category = Focus)
	float Timestamp;

	FCarriableObjectReference()
	{
		Reset();
	}

	void Reset()
	{
		Object = nullptr;
		bInstaCarry = false;
		bDrop = false;
		Timestamp = 0.0f;
	};
};

USTRUCT(BlueprintType)
struct FAbilitySlotData
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadWrite, EditAnywhere)
	TArray<float> SlotsGrowthRequirement;
};


USTRUCT(BlueprintType)
struct FSlottedAbilities
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadWrite, EditAnywhere)
	EAbilityCategory Category = EAbilityCategory::AC_NONE;

	UPROPERTY(SaveGame, BlueprintReadWrite, EditAnywhere)
	TArray<FPrimaryAssetId> SlottedAbilities;

	friend bool operator==(const FSlottedAbilities& LHS, const FSlottedAbilities& RHS)
	{
		return LHS.Category == RHS.Category && LHS.SlottedAbilities == RHS.SlottedAbilities;
	}
};

USTRUCT(BlueprintType)
struct FSlottedAbilityEffects
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite)
	FPrimaryAssetId AbilityId;
	UPROPERTY(BlueprintReadWrite)
	FGameplayAbilitySpecHandle ActiveAbility;
	UPROPERTY(BlueprintReadWrite)
	TArray<FActiveGameplayEffectHandle> PassiveEffects;

public:
	FSlottedAbilityEffects()
		: AbilityId(FPrimaryAssetId())
		, ActiveAbility(FGameplayAbilitySpecHandle())
		, PassiveEffects({ FActiveGameplayEffectHandle() })
	{}

	FSlottedAbilityEffects(const FPrimaryAssetId& InId, const FGameplayAbilitySpecHandle& InSpecHandle)
		: AbilityId(InId)
		, ActiveAbility(InSpecHandle)
		, PassiveEffects({ FActiveGameplayEffectHandle() })
	{}
	
	FSlottedAbilityEffects(const FPrimaryAssetId& InId, const FActiveGameplayEffectHandle& InGEHandle)
		: AbilityId(InId)
		, ActiveAbility(FGameplayAbilitySpecHandle())
		, PassiveEffects({ InGEHandle })
	{}
};


USTRUCT(BlueprintType)
struct FActionBarAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(SaveGame, BlueprintReadWrite, EditAnywhere)
	EAbilityCategory Category;

	UPROPERTY(SaveGame, BlueprintReadWrite, EditAnywhere)
	int32 Index;

public:
	FActionBarAbility()
		: Category(EAbilityCategory::AC_NONE)
		, Index(INDEX_NONE)
	{
		
	}

	FActionBarAbility(const EAbilityCategory InCat, const int32 InIndex)
		: Category(InCat)
		, Index(InIndex)
	{
		
	}

	friend bool operator==(const FActionBarAbility& LHS, const FActionBarAbility& RHS)
	{
		return LHS.Category == RHS.Category && LHS.Index == RHS.Index;
	}
};

UENUM()
enum class EActionBarCustomizationVersion : uint8
{
	ABCV_Initial				UMETA(DisplayName="Initial"),
	ABCV_OldCharacterFixup		UMETA(DisplayName="Old Character Fixup"),
	MAX							UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct FSavedAbilityCooldown
{
	GENERATED_BODY()
public:

	UPROPERTY(SaveGame)
	FActionBarAbility SlottedAbility;
	UPROPERTY(SaveGame)
	int64 ExpirationUnixTime = 0; // Unix Timestamp for when this ability will expire
};

UCLASS()
class UBreakLegsDamageType : public UDamageType { GENERATED_BODY() };

UCLASS()
class UBloodLossDamageType : public UDamageType { GENERATED_BODY() };

UCLASS(BlueprintType)
class UBleedingDamageType : public UDamageType { GENERATED_BODY() };

UCLASS(BlueprintType)
class UDrowningDamageType : public UDamageType { GENERATED_BODY() };

UCLASS()
class UHungerThirstDamageType : public UDamageType { GENERATED_BODY() };

class AIBaseCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUIPausedEffectCreated);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnTakeDamage, float, DamageDone, float, DamageDoneRatio, const FHitResult&, HitResult, const FGameplayEffectSpec&, Spec, const FGameplayTagContainer&, SourceTags);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPostDeath);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBrokenBone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWellRested);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAbilitySlotsChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLanded, const FHitResult&, HitResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttached, AIBaseCharacter*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDetached, AIBaseCharacter*, OldTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOtherDetached, AIBaseCharacter*, OldTarget, const EAttachType, AttachType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAbilityUnlockStateChanged, const FPrimaryAssetId&, AssetId, bool, bLocked);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRestingStateChanged, EStanceType, Old, EStanceType, New);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStatusStart);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStatusStartWithType, EDamageEffectType, DamageEffectType);

DECLARE_MULTICAST_DELEGATE(FOnExitInstance);

DECLARE_DYNAMIC_DELEGATE_TwoParams(FOnDinoAbilitiesLoadedDynamic, const TArray<UPOTAbilityAsset*>&, Abilities, AIBaseCharacter*, Character);

UCLASS(ABSTRACT, Config = Game)
class PATHOFTITANS_API AIBaseCharacter : public ACharacter, public IUsableInterface, public ISaveableObjectInterface, public IGenericTeamAgentInterface, public IGameplayTagAssetInterface, public IAbilitySystemInterface, public IInteractionPrompt, public ICarryInterface
{
	GENERATED_BODY()
	
public:
	void _SetReplicatesPrivate(bool bNewReplicates) { bReplicates = bNewReplicates; };

	void DirtySerializedNetworkFields();

	// Tag used for player's base type, i.e. aquatic, land, flyer
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "IBaseCharacter")
	FName PlayerStartTag;

	// Combination of PlayerStartTag+Growth
	UFUNCTION(BlueprintCallable, Category = "IBaseCharacter")
	FName BP_GetCombinedPlayerStartTag(bool bUseOverrideGrowth, float OverrideGrowthPercent, const FName& OverridePlayerStartTag) const;

	FName GetCombinedPlayerStartTag(const float* OverrideGrowthPercent = nullptr, const FName& OverridePlayerStartTag = NAME_None) const;

	//UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	//class UVOIPTalker* VoipTalker;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Voice|Effects")
	class USoundAttenuation* VoipAttenuationSettings;

	// Optional audio effects to apply to this player's voice.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio|Voice|Effects")
	class USoundEffectSourcePresetChain* VoipSourceEffectChain;

	bool bVoipSetup = false;

	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UPOTAbilitySystemComponent* AbilitySystem;

	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCapsuleComponent* PushCapsuleComponent = nullptr;

	UPROPERTY(Category = Character, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UMapIconComponent* MapIconComponent;

	TSubclassOf<UMapIconComponent> MapIconClass;

	virtual void OnRep_Controller() override;

	UFUNCTION(BlueprintCallable)
	void UpdateCameraLag();

public:
	UPROPERTY(BlueprintAssignable)
	FOnTakeDamage OnTakeDamage;

	UPROPERTY(BlueprintAssignable)
	FOnPostDeath OnPostDeath;

	UPROPERTY(BlueprintAssignable)
	FOnAbilitySlotsChanged OnAbilitySlotsChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAbilityUnlockStateChanged OnAbilityUnlockStateChanged;

	UPROPERTY(BlueprintAssignable)
	FOnRestingStateChanged OnRestingStateChanged;

	UPROPERTY(BlueprintAssignable)
	FOnStatusStartWithType OnStatusStart;
	
	UPROPERTY(BlueprintAssignable)
	FOnBrokenBone OnBrokenBone;

	UPROPERTY(BlueprintAssignable)
	FOnWellRested OnWellRested;
	
	FOnExitInstance OnExitInstance;

	void CacheBloodMaskPixels();
	// Blood Damage System
	UFUNCTION(BlueprintCallable)
	void UpdateBloodMask(bool bForceUpdate = false);

	UPROPERTY(Config)
	bool bWoundsEnabled;

	// Source Blood Mask
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	UTexture2D* BloodMaskSource;

	// Generated Blood Mask
	UPROPERTY()
	UTexture2DDynamic* BloodMask;

	float LastWoundUpdateAmount = 0.f;

	FORCEINLINE const FPackedWoundInfo& GetDamageWounds() const { return DamageWounds; }

	FPackedWoundInfo& GetDamageWounds_Mutable();

	void SetDamageWounds(const FPackedWoundInfo& NewDamageWounds);
	
protected:

	UPROPERTY(ReplicatedUsing=OnRep_DamageWounds, SaveGame)
	FPackedWoundInfo DamageWounds;

	TArray<FCachedWoundDamage> CachedWoundValues = {
		{FColor(255, 0, 0, 0), EDamageWoundCategory::HeadLeft, 0},
		{FColor(255, 0, 255, 0), EDamageWoundCategory::HeadRight, 0},
		{FColor(255, 255, 0, 0), EDamageWoundCategory::NeckLeft, 0},
		{FColor(0, 255, 0, 0), EDamageWoundCategory::NeckRight, 0},
		{FColor(188, 255, 255, 0), EDamageWoundCategory::ShoulderLeft, 0},
		{FColor(188, 0, 0, 0), EDamageWoundCategory::ShoulderRight, 0},
		{FColor(0, 188, 0, 0), EDamageWoundCategory::BodyLeft, 0},
		{FColor(0, 0, 188, 0), EDamageWoundCategory::BodyRight, 0},
		{FColor(0, 255, 255, 0), EDamageWoundCategory::TailBaseLeft, 0},
		{FColor(0, 0, 255, 0), EDamageWoundCategory::TailBaseRight, 0},
		{FColor(188, 0, 255, 0), EDamageWoundCategory::TailTip, 0},
		{FColor(188, 255, 0, 0), EDamageWoundCategory::LeftArm, 0},
		{FColor(255, 188, 255, 0), EDamageWoundCategory::RightArm, 0},
		{FColor(255, 255, 188, 0), EDamageWoundCategory::LeftHand, 0},
		{FColor(0, 188, 255, 0), EDamageWoundCategory::RightHand, 0},
		{FColor(188, 188, 255, 0), EDamageWoundCategory::LeftLeg, 0},
		{FColor(188, 255, 188, 0), EDamageWoundCategory::RightLeg, 0},
		{FColor(255, 188, 188, 0), EDamageWoundCategory::LeftFoot, 0},
		{FColor(255, 0, 188, 0), EDamageWoundCategory::RightFoot, 0}
	};

public:

	UFUNCTION()
	void OnRep_DamageWounds();
	bool bDamageWoundsDirty = false;

	UFUNCTION()
	void OnUpdateDamageWoundsTimer();

	FTimerHandle UpdateDamageWoundsTimer;

	UFUNCTION(BlueprintCallable)
	void TestWoundDamage(EDamageWoundCategory Category, float InNormalizedDamage);

	UFUNCTION(BlueprintPure)
	float GetWoundDamage(EDamageWoundCategory Category, bool bPermanent) const;

	void ApplyShortImpactInvinsibility();
	bool IsImpactInvinsible();

	void OnPhysicsVolumeChanged(APhysicsVolume* NewVolume);
	void PossiblyUpdateWaterVision(AIWater* Water);

	bool bImpactInvinsible = false;
	float elapsedImpactDamageTime = 0.0f;

	bool bPrecisedMovementKeyPressed = false;

	//The amount of time you can't take another impact hit
	UPROPERTY(EditAnywhere, Category = "Damage")
	float ImpactInvinsibleTime = 0.25f;

	//The amount of damage you do to others. E.G If you take 100 damage from a nosedive, you'll deal 50 if the multiplier is at 0.5f
	UPROPERTY(EditAnywhere, Category = "Damage")
	float NosediveDamageMultiplier = 0.5f;
	
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	TMap<EDamageWoundCategory, FWoundBoneInfo> BoneNameToWoundCategory;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	FVector2D NormalizedWoundsDamageRange;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	FVector2D NormalizedPermaWoundsDamageRange;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	FVector2D NormalizedPermaWoundsOpacityRange;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	float TotalDamageMaxOpacity;
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	float TotalDamageOpacityHealthThreshold;
	UPROPERTY(VisibleDefaultsOnly)
	FBloodMaskPixelData BloodMaskPixelData;

	// Interactive Foliage
protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	UMaterialParameterCollection* MPC_InteractiveFoliage;

public:

	// Character Names
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, SaveGame, Category = IBaseCharacter)
	FPrimaryAssetId CharacterDataAssetId;

	FORCEINLINE const FString& GetCharacterName() const { return CharacterName; }

	void SetCharacterName(const FString& NewCharacterName);

protected:

	UPROPERTY(BlueprintReadOnly, Replicated, SaveGame, Category = IBaseCharacter)
	FString CharacterName;

public:

	UPROPERTY(BlueprintReadOnly, EditAnywhere, SaveGame, Category = IBaseCharacter)
	FGameplayTag CharacterTag;

	// List of Mod Skus used in a character which can be used to determine if we can load this character from the database
	UPROPERTY(SaveGame)
	TArray<FString> ModSkus;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FString MapName;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FVector SaveCharacterPosition;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FRotator SaveCharacterRotation;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FVector FallbackCaveReturnPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FRotator FallbackCaveReturnRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FPrimaryAssetId InstanceId;

	FORCEINLINE const TArray<int>& GetUnlockedSubspeciesIndexes() const { return UnlockedSubspeciesIndexes; }

	TArray<int>& GetUnlockedSubspeciesIndexes_Mutable();
	
protected:
	
	// Mesh Indexes on FSelectedSkin, if empty they're currently equiped skin should be considered unlocked
	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated, Category = IBaseCharacter)
	TArray<int> UnlockedSubspeciesIndexes; 

public:

	float HomeCaveRefundMultiplier = .5f;

	UFUNCTION(Server, Reliable)
	void ServerKickPlayerFromOwnedInstance(AIPlayerState* IPlayerState);
	void ServerKickPlayerFromOwnedInstance_Implementation(AIPlayerState* IPlayerState);

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UTexture2D> HomeCaveToastIconObjectPtr;

	void SendHomeCaveToastToParty(const AIPlayerState* IPlayerState, bool bEntered, bool bOwned);

	UFUNCTION(Client, Reliable)
	void ClientPlayerHomecaveToast(const AIPlayerState* IPlayerState, bool bEntered, bool bOwned);
	void ClientPlayerHomecaveToast_Implementation(const AIPlayerState* IPlayerState, bool bEntered, bool bOwned);

	UFUNCTION(BlueprintCallable, Category = HomeCave)
	void ExitInstance(const FTransform& OverrideReturnTransform);
	UFUNCTION(BlueprintCallable, Category = HomeCave)
	void ExitInstanceFromDeath(const FTransform& OverrideReturnTransform);

	UFUNCTION(BlueprintCallable, Category = HomeCave, Server, Reliable)
	void ServerExitInstance(const FTransform& OverrideReturnTransform);
	void ServerExitInstance_Implementation(const FTransform& OverrideReturnTransform);

	UFUNCTION(Client, Reliable)
	void ClientExitInstance();
	void ClientExitInstance_Implementation();

	UFUNCTION(Client, Reliable)
	void ClientEnterInstance();
	void ClientEnterInstance_Implementation();

	UFUNCTION(Client, Reliable, BlueprintCallable)
	void ClientInCombatWarning();
	void ClientInCombatWarning_Implementation();
	FTimerHandle TimerHandle_InCombatWarning;

	//UFUNCTION(Client, Reliable)
	//void ClientPreloadArea();
	//void ClientPreloadArea_Implementation();

	//UFUNCTION(Server, Reliable, WithValidation)
	//void ServerFinishPreloadArea();
	//bool ServerFinishPreloadArea_Validate();
	//void ServerFinishPreloadArea_Implementation();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerNotifyFinishedClientLoad();
	void ServerNotifyFinishedClientLoad_Implementation();
	bool ServerNotifyFinishedClientLoad_Validate();

	void ClientLoadedAutoRecord();
	//bool bPendingClientPreload = false;

	/**
	* Teleports the player to the specified Tile
	* Tile: The tile instance to enter
	* OverrideTransform: The location to teleport to, if equal FTransform() it will find a spawn location inside the instance
	* ReturnTransform: The location to return to when leaving the instance. If equal FTransform() it will be the Actor's transform before entering the instance
	*/
	void ServerEnterInstance(const FInstancedTile& Tile, const FTransform& OverrideTransform = FTransform(), const FTransform& ReturnTransform = FTransform());

private:
	UFUNCTION()
	void RetryEnterInstance();
	void RemoveAbilityById(FPrimaryAssetId AbilityId);
	FInstancedTile RetryTile;
	FTransform RetryOverrideTransform;
	FTransform RetryReturnTransform;

public:
	
	UFUNCTION(BlueprintCallable, Category = HomeCave)
	AIPlayerCaveBase* GetCurrentInstance() const;

	/** Another victim of the non-const correct BlueprintCallable function. */
	FORCEINLINE AIPlayerCaveBase* NativeGetCurrentInstance() const { return CurrentInstance; }
	
	void SetCurrentInstance(AIPlayerCaveBase* Instance);

	bool IsValidatingInstance() const;

	FORCEINLINE const FHomeCaveSaveableInfo& GetHomeCaveSaveableInfo() const { return HomeCaveSaveableInfo; }

	FHomeCaveSaveableInfo& GetHomeCaveSaveableInfo_Mutable();

protected:

	UPROPERTY(ReplicatedUsing = OnRep_CurrentInstance)
	AIPlayerCaveBase* CurrentInstance = nullptr;

	UFUNCTION()
	void OnRep_CurrentInstance();
	
	UPROPERTY(BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_HomeCaveSaveableInfo, Category = IBaseCharacter)
	FHomeCaveSaveableInfo HomeCaveSaveableInfo;

public:


	void AddOwnedDecoration(UHomeCaveDecorationDataAsset* DataAsset, int Amount);
	FHomeCaveDecorationPurchaseInfo* GetOwnedDecoration(UHomeCaveDecorationDataAsset* DataAsset);
	void RemoveOwnedDecoration(UHomeCaveDecorationDataAsset* DataAsset, int Amount);
	int GetOwnedDecorationCount(UHomeCaveDecorationDataAsset* DataAsset);
	FHomeCaveDecorationSaveInfo* GetSavedDecoration(FGuid Guid);
	void RemoveSavedDecoration(FGuid Guid);

	bool IsExtensionActive(UHomeCaveExtensionDataAsset* DataAsset);
	bool IsExtensionOwned(UHomeCaveExtensionDataAsset* DataAsset);
	void AddOwnedExtension(UHomeCaveExtensionDataAsset* DataAsset);
	void RemoveOwnedExtension(UHomeCaveExtensionDataAsset* DataAsset);


	UFUNCTION(Server, Reliable)
	void ServerRequestDecorationPurchase(UHomeCaveDecorationDataAsset* DataAsset, int Amount);
	void ServerRequestDecorationPurchase_Implementation(UHomeCaveDecorationDataAsset* DataAsset, int Amount);

	UFUNCTION(Server, Reliable)
	void ServerRequestDecorationSale(UHomeCaveDecorationDataAsset* DataAsset, int Amount);
	void ServerRequestDecorationSale_Implementation(UHomeCaveDecorationDataAsset* DataAsset, int Amount);

	UFUNCTION(Server, Reliable)
	void ServerRequestExtensionPurchase(UHomeCaveExtensionDataAsset* DataAsset);
	void ServerRequestExtensionPurchase_Implementation(UHomeCaveExtensionDataAsset* DataAsset);

	UFUNCTION(Server, Reliable)
	void ServerRequestExtensionSale(UHomeCaveExtensionDataAsset* DataAsset);
	void ServerRequestExtensionSale_Implementation(UHomeCaveExtensionDataAsset* DataAsset);

	UFUNCTION(Client, Reliable)
	void ClientConfirmDecorationPurchase();
	void ClientConfirmDecorationPurchase_Implementation();

	UFUNCTION(Client, Reliable)
	void ClientConfirmExtensionPurchase();
	void ClientConfirmExtensionPurchase_Implementation();

	UFUNCTION(Client, Reliable)
	void ClientExtensionSaleFailed();
	void ClientExtensionSaleFailed_Implementation();
	void ExtensionSaleFailedMessageBoxLoaded(class UIUserWidget* LoadedWidget);

protected:

	TSharedPtr<FStreamableHandle> Handle_ExtensionSaleFailedWidget;
	
public:

	UFUNCTION(Server, Reliable)
	void ServerRequestHomeCaveReset();
	void ServerRequestHomeCaveReset_Implementation();

	const int RoomSocketPrices[3] = { 1000, 5000, 10000 };
	int GetBoughtRoomSocketCount();
	bool IsRoomSocketBought(ECaveUpgrade CaveUpgrade);
	int GetRoomSocketPrice();
	int GetRoomSocketRefundAmount();

	UFUNCTION(Server, Reliable)
	void ServerRequestRoomSocketPurchase(ECaveUpgrade CaveUpgrade);
	void ServerRequestRoomSocketPurchase_Implementation(ECaveUpgrade CaveUpgrade);

	UPROPERTY(EditDefaultsOnly, Category = HomeCave)
	TSoftClassPtr<class UIMessageBox> ExtensionSaleFailedMessageBoxClassPtr;

	UFUNCTION()
	void OnRep_HomeCaveSaveableInfo();

	UPROPERTY(BlueprintReadOnly, Category = HomeCave)
	FTimerHandle HomeCaveEnterCooldown;

	void HomeCaveCooldownExpire();
	bool bHasPendingHomeCaveTeleport = false;
	FTransform PendingHomeCaveReturn = FTransform();

	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TSoftClassPtr<class UIHomeCaveDecoratorComponent> HomeCaveDecoratorComponentClassPtr;

	// The selected Instance Id to load when they enter the homecave
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	FPrimaryAssetId HomecaveInstanceId;	
	
	// The default instance Id for the character's homecave
	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	FPrimaryAssetId DefaultHomecaveInstanceId;		
	
	// The default instance Id for the character's homecave
	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TSoftObjectPtr<UHomeCaveExtensionDataAsset> DefaultHomecaveBaseRoom;

	UFUNCTION(Server, Reliable)
	void ServerStartHomeCaveDecoration(class UHomeCaveDecorationDataAsset* DecorationDataAsset);
	void ServerStartHomeCaveDecoration_Implementation(class UHomeCaveDecorationDataAsset* DecorationDataAsset);
	void ServerStartHomeCaveDecorationStage1(class UHomeCaveDecorationDataAsset* DecorationDataAsset);

	// ClientFinishHomeCaveDecoration will force a player out of decorating/editing their home cave, like when a player joins
	UFUNCTION(Client, Reliable)
	void ClientFinishHomeCaveDecoration();
	void ClientFinishHomeCaveDecoration_Implementation();
	
	UFUNCTION(Client, Reliable)
	void ClientFinishInstanceTeleport();
	void ClientFinishInstanceTeleport_Implementation();

	void AwardHomeCaveDecoration(const struct FQuestHomecaveDecorationReward& HomecaveDecorationReward);
	void HomeCaveDecorationAwardLoaded(struct FQuestHomecaveDecorationReward HomecaveDecorationReward);
	UFUNCTION(Client, Reliable)
	void ClientOnHomeCaveDecorationAward(const TSoftObjectPtr<UTexture2D>& IconTexture, const FText& Name, int Amount);
	void ClientOnHomeCaveDecorationAward_Implementation(const TSoftObjectPtr<UTexture2D>& IconTexture, const FText& Name, int Amount);

	void RefreshInstanceHiddenActors();

	virtual void Restart() override;

	// Boolean Flag to determine if we need to respawn and reset stats
	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	bool bRespawn = false;

	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	bool bAllowAttackInDemo = false;

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	TArray<FInstanceLogoutSaveableInfo> InstanceLogoutInfo;

	FInstanceLogoutSaveableInfo* GetInstanceLogoutInfo(const FString& LevelName);

	bool LoggedOutInInstance(const FString& LevelName);

	bool LoggedOutInOwnedInstance(const FString& LevelName);

	void RemoveInstanceLogoutInfo(const FString& LevelName);
	
	UFUNCTION(BlueprintCallable, Server, Reliable, Category = IBaseCharacter)
	void ServerRequestLeaveCave(bool bHatchling);
	void ServerRequestLeaveCave_Implementation(bool bHatchling);

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = IBaseCharacter)
	void ServerTryLeaveHatchlingCave();
	void ServerTryLeaveHatchlingCave_Implementation();

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = IBaseCharacter)
	void ServerResetPositionHatchlingCave();
	void ServerResetPositionHatchlingCave_Implementation();

	UFUNCTION(BlueprintCallable, Client, Reliable, Category = IBaseCharacter)
	void ClientLeaveCavePrompt(const FString& Prompt, bool bCanLeave, bool bHatchlingCave);
	void ClientLeaveCavePrompt_Implementation(const FString& Prompt, bool bCanLeave, bool bHatchlingCave);

	UFUNCTION()
	void CavePromptLoaded(UIUserWidget* IUserWidget, FString Prompt, bool bCanLeave, bool bHatchlingCave);

	/************************************************************************/
	/* Paused Effects                                                       */
	/************************************************************************/

	UPROPERTY(SaveGame, Replicated, ReplicatedUsing = OnRep_PausedGameplayEffectsList)
	TArray<FSavedGameplayEffectData> PausedGameplayEffectsList;

	UFUNCTION()
	void OnRep_PausedGameplayEffectsList();

	UPROPERTY()
	FOnUIPausedEffectCreated OnUIPausedEffectCreated;

	UFUNCTION(BlueprintCallable, Category = IBaseCharacter)
	void ReassignAllGameplayEffectFromPausedList();

	UFUNCTION()
	void RemoveAllGameplayEffectAndAddToPausedList();

	void AddEffectToPausedList(FActiveGameplayEffectHandle ActiveGEHandle, float RemainingTime);

	void RemoveEffectFromPausedList(const UGameplayEffect* Effect, float RemainingTime);

protected:

	TSharedPtr<FStreamableHandle> Handle_CavePromptWidgetLoaded;
	
public:

	void SetActualGrowth(float Growth);

	FORCEINLINE float GetActualGrowth() const { return ActualGrowth; }

	FORCEINLINE bool HasNoGrowthSave() const { return bNoGrowthSave; }

	FORCEINLINE bool ShouldUseActualGrowth() const { return bNoGrowthSave && !bGrowthUsedLast; } 

	void SetGrowthUsedLast(bool bGrowthUsed);

	FORCEINLINE bool WasGrowthUsedLast() const { return bGrowthUsedLast; }

protected:

	UPROPERTY(SaveGame)
	float ActualGrowth = 0.0f;

	UPROPERTY(SaveGame)
	bool bNoGrowthSave = false;

	UPROPERTY(SaveGame)
	bool bGrowthUsedLast = true;

public:
	UFUNCTION()
	void LeaveHatchlingCavePromptClosed(EMessageBoxResult Result);

	UFUNCTION()
	void LeaveHomeCavePromptClosed(EMessageBoxResult Result);

	bool IsReadyToLeaveHatchlingCave() const;

	FORCEINLINE bool IsReadyToLeaveHatchlingCaveRaw() const { return bReadyToLeaveHatchlingCave; }

	virtual bool HasLeftHatchlingCave() const { return bLeftHatchlingCave; }

protected:

	UPROPERTY(BlueprintReadOnly, SaveGame, Replicated, Category = IBaseCharacter)
	bool bLeftHatchlingCave = false;
	
	UPROPERTY(BlueprintReadWrite, SaveGame, Replicated, Category = IBaseCharacter)
	bool bReadyToLeaveHatchlingCave = false;

public:

	UPROPERTY()
	bool bAwaitingCaveResponse = false;

	void DisableCollisionCapsules(bool bPermanentlyIgnoreAllChannels);

	void EnableCollisionCapsules();

	void SetCollisionResponseProfile(FName NewCollisionProfile);

	void SetCollisionResponseProfilesToDefault();

	UPROPERTY(EditDefaultsOnly, Category = Collision)
	FName AttachedCollisionResponseProfile = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = Collision)
	FName RecentlyDetachedCollisionResponseProfile = NAME_None;


protected:
	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TSoftClassPtr<UIMessageBox> HatchlingExitPromptClassPtr;

	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TSoftClassPtr<UIMessageBox> HatchlingExitPromptYesNoClassPtr;

	FTimerHandle DelayCollisionAfterUnlatchHandle;


public:
	
	UFUNCTION(BlueprintCallable)
	void QueueTutorialPrompt(const FTutorialPrompt& NewTutorialPrompt);

	FORCEINLINE const FTutorialPrompt& GetTutorialPrompt() const { return TutorialPrompt; }

protected:
	
	UPROPERTY(ReplicatedUsing = "OnRep_TutorialPrompt");
	FTutorialPrompt TutorialPrompt;

	UFUNCTION()
	void OnRep_TutorialPrompt();

public:
	/*UFUNCTION(Client, Reliable, BlueprintCallable, Category = IBaseCharacter)
	void ClientShowTutorialPrompt(const FText& Text, const FText& LeftTooltipText, const FText& RightTooltipText, const TArray<class UInputAction*>& LeftTooltipActions, const TArray<class UInputAction*>& RightTooltipActions);
	void ClientShowTutorialPrompt_Implementation(const FText& Text, const FText& LeftTooltipText, const FText& RightTooltipText, const TArray<class UInputAction*>& LeftTooltipActions, const TArray<class UInputAction*>& RightTooltipActions);*/

	UPROPERTY(BlueprintReadOnly, SaveGame, Category = IBaseCharacter)
	int HatchlingTutorialIndex = 0;

private:
	// Largest type of carriable this dinosaur can pickup
	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	ECarriableSize MaxCarriableSize;

public:

	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	bool OverrideDefaultCarriableSizeLogic = false;

	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TMap<float, ECarriableSize> MediumCarriableSizeMap = {
		{0.5f, ECarriableSize::SMALL},
		{1.f, ECarriableSize::MEDIUM}
	};

	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TMap<float, ECarriableSize> LargeCarriableSizeMap = {
		{0.33f, ECarriableSize::SMALL},
		{0.66f, ECarriableSize::MEDIUM},
		{1.f, ECarriableSize::LARGE}
	};

	// Modders can change this map in their custom dinosaurs
	UPROPERTY(EditDefaultsOnly, Category = IBaseCharacter)
	TMap<float, ECarriableSize> OverrideCarriableSizeMap = {
		{1.f, ECarriableSize::LARGE}
	};

	UFUNCTION(BlueprintCallable)
	ECarriableSize GetMaxCarriableSize();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carry Data")
	FCarriableData CarryData;

	UPROPERTY(BlueprintReadOnly)
	float CurrentMaxJawOpenAmount;

	UFUNCTION(BlueprintCallable)
	float GetMaxJawOpenAmountByGrowth(float GrowthPercentage);

	UFUNCTION(BlueprintCallable)
	float GetJawOpenRequirementScaled(AActor* CarriedObject, float DeltaSeconds);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = IBaseCharacter, AdvancedDisplay)
	float BasePushForce;

	UPROPERTY(Config)
	bool bPushEnabled = false;


	// Anim Notify Sounds, Particle Effects, Calls etc
public:
	

	// Quest System
public:

	// How many in game seconds the player has spent waiting for the group cooldown timer
	UPROPERTY(SaveGame)
	float GroupMeetupTimeSpent = 0.0f;

	UFUNCTION(BlueprintCallable, Client, Reliable, Category = IBaseCharacter)
	void ClientRequestChatEnabledForQuest();
	void ClientRequestChatEnabledForQuest_Implementation();

	virtual bool CanHaveQuests() const;

protected:
	bool HaveMetMaxUnclaimedRewards() const;

public:

	UFUNCTION(BlueprintCallable, Server, Reliable, Category = IBaseCharacter)
	void ServerSubmitGenericTask(FName TaskKey);
	void ServerSubmitGenericTask_Implementation(FName TaskKey);

	/** Method that allows an actor to replicate sub objects on its actor channel */
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	UFUNCTION()
	void OnRep_ActiveQuests();

	UFUNCTION()
	void OnRep_UncollectedRewardQuests();

	UFUNCTION()
	void OnRep_LocationsProgress();

	void CheckIfUserShouldBeNotifiedToClaimRewards();

	UFUNCTION(Client, Reliable)
	void Client_NotifyUserToClaimRewards(const FString& Key, const FString& User);
	void Client_NotifyUserToClaimRewards_Implementation(const FString& Key, const FString& User);

	FTimerHandle TimerHandle_UnclaimedQuestRewards;
	UFUNCTION(BlueprintCallable)
	void UpdateUnclaimedQuestRewardsTimer();
	void OnUnclaimedQuestRewardsTimer();

	void QuestTasksReplicated(UIQuest* Quest);

	UFUNCTION()
	void OnActiveQuestDataLoaded(UQuestData* QuestData, UIQuest* Quest);

	UFUNCTION()
	void OnUncollectedRewardQuestDataLoaded(UQuestData* QuestData, UIQuest* Quest);

	void UpdateQuestMarker();

	UFUNCTION(Server, Reliable)
	void ServerForceLocationCheck();

	TArray<FQuestCooldown> LocalQuestFailures;
	
	UPROPERTY()
	TArray<FLocalQuestTrackingItem> TrackedGroupQuests;
	
	FTimerHandle TimerHandle_LocalQuestFailDelay;
	bool AddLocalQuestFailure(FPrimaryAssetId QuestId);
	bool RemoveLocalQuestFailure(FPrimaryAssetId QuestId);

	FTimerHandle TimerHandle_ActiveQuestInitializationCheck;
	void StartActiveQuestInitializationCheck();
	void StopActiveQuestInitializationCheck();

	void UpdateCompletedQuests(FPrimaryAssetId QuestId, EQuestType QuestType, int32 Max);
	void UpdateFailedQuests(FPrimaryAssetId QuestId, EQuestType QuestType, int32 Max);

	TConstArrayView<FRecentQuest> GetRecentCompletedQuests() const;
	TConstArrayView<FRecentQuest> GetRecentFailedQuests() const;

	FORCEINLINE const TArray<UIQuest*>& GetActiveQuests() const { return ActiveQuests; }

	TArray<UIQuest*>& GetActiveQuests_Mutable();

	FORCEINLINE const TArray<UIQuest*>& GetUncollectedRewardQuests() const { return UncollectedRewardQuests; }

	TArray<UIQuest*>& GetUncollectedRewardQuests_Mutable();

protected:

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_ActiveQuests, Category = IBaseCharacter)
	TArray<UIQuest*> ActiveQuests;

	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_UncollectedRewardQuests, Category = IBaseCharacter)
	TArray<UIQuest*> UncollectedRewardQuests;

public:

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TArray<FRecentQuest> RecentCompletedQuests;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	FDateTime LastCompletedExplorationQuestTime;

	UPROPERTY(SaveGame, BlueprintReadOnly)
	TArray<FRecentQuest> RecentFailedQuests;

	FORCEINLINE const TArray<FLocationProgress>& GetLocationsProgress() const { return LocationsProgress; }

	TArray<FLocationProgress>& GetLocationsProgress_Mutable();

protected:

	UPROPERTY(SaveGame, BlueprintReadOnly, ReplicatedUsing = OnRep_LocationsProgress)
	TArray<FLocationProgress> LocationsProgress;

public:

	UPROPERTY(SaveGame)
	TArray<FQuestSave> QuestSaves;

	UPROPERTY(SaveGame)
	TArray<FQuestSave> UncollectedRewardQuestSaves;

	UPROPERTY(SaveGame)
	TArray<FQuestCooldown> QuestCooldowns;

	// Tags to allow for retrieving a quest
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuestSelection")
	TArray<FName> QuestTags;

	UPROPERTY(BlueprintReadOnly)
	bool bIsCharacterEditorPreviewCharacter;
	bool bNewCharacter;
	bool bForceIKDisabled;
	bool bIsCharacterEditorPreviewCharacterCanFall = true;
	
public:

	/************************************************************************/
	/* Actions                                                              */
	/************************************************************************/
	
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Move;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* LookAround;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* AutoRun;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Sprint;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Trot;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions", DisplayName = "Crouch")
	UInputAction* CrouchAction;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input Actions", DisplayName = "Jump")
	UInputAction* JumpAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* PreciseMovement;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Use;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Collect;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Carry;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* CameraZoom;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Resting;
	//UPROPERTY(EditDefaultsOnly, Category = "Actions")
	//UInputAction* Sleeping;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Bugsnap;
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* FeignLimping;

	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* Map;

	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* QuestsAction;

	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	UInputAction* ToggleAdminNameTags;
	
	UPROPERTY(EditDefaultsOnly, Category = "Input Actions")
	TArray<UInputAction*> AbilityActions;

	FORCEINLINE const FAttachTarget& GetAttachTarget() const { return AttachTargetInternal; }
	
	FAttachTarget& GetAttachTarget_Mutable();

	void SetAttachTarget(const FAttachTarget& NewAttachTarget);

private:

	UFUNCTION(BlueprintCallable, BlueprintPure, meta = (AllowPrivateAccess = "true"))
	const FAttachTarget& GetAttachTargetBP() const { return GetAttachTarget(); }

	UPROPERTY(ReplicatedUsing = OnRep_AttachTarget)
	FAttachTarget AttachTargetInternal;
	
	float LastAttachedCameraUpdateTime = 0.0f;

	float LastRepositionIfObstructedTime = 0.0f;

	UPROPERTY(EditDefaultsOnly)
	float MaxObstructedGroundTraceHeight = 400.0f;

	UPROPERTY(EditDefaultsOnly)
	float ObstructedDownwardSweepHeightOffset = 400.0f;

	// Used for fall damage check when bEnableDetachFallDamageCheck is true.
	UPROPERTY(EditDefaultsOnly)
	float DetachFallDamageCheckPadding = 500.0f;

	UPROPERTY(Transient, Replicated)
	bool bEnableDetachGroundTraces = true;

	UPROPERTY(Transient, Replicated)
	bool bEnableDetachFallDamageCheck = true;

	bool bHighTeleportOnDetach = false;

	// Used for fall damage check when bEnableDetachFallDamageCheck is true.
	float LastDetachHeight = 0.0f;
	float LastDetachTeleportHeightDelta = 0.0f;

	void PerformRepositionIfObstructed(const UWorld* const World, const FVector& NewPosition);

	void EnableCollisionAfterDelay();

public:
	void SetEnableDetachGroundTraces(const bool bEnable);
	void SetEnableDetachFallDamageCheck(const bool bEnable);

	void RepositionIfObstructed(AIBaseCharacter* const OldAttachCharacter, const FQuat& BaseSweepRotation, const float ZoneRadius, const float CapsuleInflationMultiplier, const bool bSkipInitialSweep);

	UPROPERTY()
	AIBaseCharacter* AttachTargetOwner = nullptr;

protected:

	// Server side only
	UPROPERTY()
	TMap<FName, AIBaseCharacter*> AttachedCharacters;

	UFUNCTION()
	void OnRep_AttachTarget(const FAttachTarget& OldAttachTarget);
	
	bool bDelayOnRep_AttachTarget = false;
	bool bDelayOnRep_IsStuckInOtherCharacter = false;

	UPROPERTY(Transient)
	AIBaseCharacter* LastAttachCharacter = nullptr;

	//ICarryInterface
public:
	virtual bool Notify_TryCarry_Implementation(class AIBaseCharacter* User) override;
	virtual void PickedUp_Implementation(AIBaseCharacter* User) override;
	virtual void Dropped_Implementation(AIBaseCharacter* User, FTransform DropPosition) override;
	virtual FVector GetCarryLocation_Implementation(class AIBaseCharacter* User) override;
	virtual FVector GetDropLocation_Implementation(class AIBaseCharacter* User) override;
	virtual FCarriableData GetCarryData_Implementation() override;
	virtual bool IsCarried_Implementation() const override;
	virtual bool IsCarriedBy_Implementation(AIBaseCharacter* PotentiallyCarrying) override;
	virtual bool CanCarry_Implementation(AIBaseCharacter* PotentiallyCarrying) override;

public:

	bool HasDinosLatched() const;

	void UnlatchAllDinosAndSelf();

	UFUNCTION(BlueprintCallable)
	bool SetAttachTarget(USceneComponent* InAttachComp, FName AttachSocket, EAttachType AttachType = EAttachType::AT_Default);

	UFUNCTION(BlueprintCallable)
	void ClearAttachTarget();

	UFUNCTION(BlueprintCallable, Server, Reliable)
	void ServerRequestClearAttachTarget();
	void ServerRequestClearAttachTarget_Implementation();

	UFUNCTION(BlueprintCallable)
	void ClearAttachedCharacters();

	// returns the character that is actively carried by this character.
	UFUNCTION(BlueprintPure)
	AIBaseCharacter* GetCarriedCharacter() const;

private:
	void ClearCarriedCharacter();

public:
	UPROPERTY(BlueprintAssignable)
	FOnAttached OnAttached;

	// When this character detaches from another character
	UPROPERTY(BlueprintAssignable)
	FOnDetached OnDetached;

	// When a character attached to this character detaches
	UPROPERTY(BlueprintAssignable)
	FOnOtherDetached OnOtherDetached;

	UFUNCTION(BlueprintPure)
	bool IsLatched() const;

	UFUNCTION(BlueprintPure)
	bool IsAttached() const;

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	// Blinking & Breathing
public:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	UCurveFloat* CurveBreathe;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	UCurveFloat* CurveBlink;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = IBaseCharacter)
	UCurveFloat* BlinkGrowthMultiplier;

	UPROPERTY()
	UTimelineComponent* BreatheTimeline;

	UPROPERTY()
	UTimelineComponent* BlinkTimeline;

	UPROPERTY()
	float BreatheTimelineValue;

	UPROPERTY()
	float BlinkTimelineValue;

	UPROPERTY()
	TEnumAsByte<ETimelineDirection::Type> TimelineDirection;

	UFUNCTION()
	void TimelineCallback_Breathe(float val);

	UFUNCTION()
	void StopBreathing();

	UFUNCTION()
	void TimelineCallback_Blink(float val);

	UFUNCTION()
	void StopBlinking(bool bCloseEyes);
public:
	// Database Callbacks
	virtual void DatabasePostLoad_Implementation();
	virtual void DatabasePreSave_Implementation();
	virtual void DatabasePostSave_Implementation();
	virtual bool IsPostLoadThreadSafe() const override;

	void SetIsPreviewCharacter(bool bRequiresLevelLoad = true);

	FORCEINLINE int32 GetMarks() const { return Marks; }

	UFUNCTION(BlueprintCallable)
	void SetMarks(int32 NewMarks);

protected:
	
	UPROPERTY(SaveGame, BlueprintReadOnly, ReplicatedUsing = OnRep_Marks)
	int32 Marks;

	const int32 MaximumMarks = 999999999;

	UFUNCTION()
	void OnRep_Marks(int OldMarks);

	UFUNCTION()
	void OnLatchedStaminaTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION(BlueprintCallable)
	void BuckLatched();

	UFUNCTION(BlueprintCallable)
	void BuckCarrying();

	friend class UPOTGameplayAbility_Buck;

public:
	FText GetCurrentLocationText();

	UPROPERTY(BlueprintReadOnly)
	FName LastLocationTag = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FName LastBiomeTag = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	FText LocationDisplayName;

	UPROPERTY(BlueprintReadOnly)
	bool LastLocationEntered = false;

	UPROPERTY(BlueprintReadOnly)
	TArray<AIMoveToQuest*> ActiveMoveToQuestPOIs;

	UFUNCTION(BlueprintCallable)
	void AddMarks(int32 MarksToAdd);

	// Sets default values for this character's properties
	AIBaseCharacter(const class FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	virtual void BeginDestroy() override;

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;

	void Serialize(FArchive& Ar) override;
	void PostLoad() override;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	UFUNCTION()
	void OnMontageStarted(UAnimMontage* Montage);

	UFUNCTION()
	void OnMontageEnded(UAnimMontage* Montage, bool bInterupted);

	virtual void PostInitializeComponents() override;

	//  Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void DetachFromControllerPendingDestroy() override;

	// Stop playing all montages
	void StopAllAnimMontages();

	// Remove Unused Components on Death / Unneeded Server Ones for Performance
	virtual void DestroyCosmeticComponents();

	void UpdateMovementState(bool bSkipAbilityCancel = false);

	void UpdateWoundsTextures();

	// Called several times a frame
	virtual void Tick(float DeltaSeconds) override;

	/** We check all movement state modifying inputs here, makes sense in terms of the call chain. */
	virtual void CheckJumpInput(float DeltaTime) override;

	UPROPERTY()
	AController* LastPossessedController = nullptr;
	virtual void PossessedBy(AController* NewController) override;

	virtual void UnPossessed() override;
	virtual void PawnClientRestart() override;

	virtual void HandleDamage(float DamageDone, const FHitResult& HitResult, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags, class AController* EventInstigator, class AActor* DamageCauser);

	void DelayedServerNotifyPOIOverlap(AIPOI* POI);

private:
	void ServerNotifyPOIOverlap(AIPOI* POI);
	FTimerHandle TimerHandle_DelayedServerNotifyPOIOverlap{};
	static constexpr float POIOverlapDelaySeconds = 1.0f;

	void ApplyCombatTimerToPlayersGroup(const AIBaseCharacter* Player) const;

public:
	UFUNCTION(Client, Reliable)
	void Client_BroadcastStatus(EDamageEffectType DamageEffectType);
	
	virtual void HandleHealthChange(float Delta, const FHitResult& HitResult, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags, class AController* EventInstigator, class AActor* DamageCauser);

	void HandleStatusRateChange(const FGameplayAttribute& AttributeToCheck,
		const FPOTStatusHandling& StatusHandlingInformation,
		const float Delta,
		const FGameplayEffectSpec& Spec,
		const FGameplayTagContainer& SourceTags,
		const bool bReplicateParticle = true,
		const FHitResult& HitResult = FHitResult());

	
	virtual void HandleLegDamageChange(float Delta, const FGameplayTagContainer& SourceTags);
	virtual void HandleBrokenBone(const FGameplayTagContainer& SourceTags);
	virtual void HandleWellRested(const FGameplayTagContainer& SourceTags);
	virtual void HandleBodyFoodChange(float Delta, const FGameplayTagContainer& SourceTags);
	virtual void HandleKnockbackForce(const FVector& Direction, float Force);

	UFUNCTION(Client, Reliable)
	void ClientGetKnockback(const FVector& Direction, float Force);
	void ClientGetKnockback_Implementation(const FVector& Direction, float Force);

	//UPROPERTY(EditDefaultsOnly)
	//UCurveFloat* ReceivedKnockbackCurve;

	virtual void LaunchCharacter(FVector NewLaunchVelocity, bool bXYOverride, bool bZOverride) override;
	UFUNCTION(Client, Reliable)
	void AddLaunchVelocity(FVector NewLaunchVel);
	void AddLaunchVelocity_Implementation(FVector NewLaunchVel);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float KnockbackMaxForce = 2000.f;

	/************************************************************************/
	/* Knockback Prediction					                                */
	/************************************************************************/

	FORCEINLINE bool IsStunned() const { return bStunned; }

	void SetIsStunned(bool bNewIsStunned);

	FORCEINLINE const FVector& GetLaunchVelocity() const { return LaunchVelocity; }
	
	void SetLaunchVelocity(const FVector& NewLaunchVelocity);
	
protected:

	UPROPERTY(ReplicatedUsing = OnRep_LaunchVelocity)
	FVector LaunchVelocity = FVector::ZeroVector;

	UFUNCTION()
	void OnRep_LaunchVelocity(FVector OldLaunchVelocity);
	
	UPROPERTY(BlueprintReadOnly, Replicated)
	bool bStunned = false;

public:

	bool GetDamageEventFromEffectSpec(const FGameplayEffectSpec& Spec, FDamageEvent& OutEvent) const;
	bool GetDamageEventFromDamageType(const EDamageType Type, FDamageEvent& OutEvent) const;
	EDamageType GetDamageTypeFromDamageEvent(const FDamageEvent& Event) const;
	bool IsHitResultProperDirection(EWoundDirectionFilter Filter, const FHitResult& Result) const;
	
	UFUNCTION(Client, Reliable)
	void Client_BroadcastBoneBreakDelegate();

	UFUNCTION(Client, Reliable)
	void Client_BroadcastWellRestedDelegate();

	UFUNCTION(Client, Reliable)
	void Client_BroadcastOnTakeDamageDelegate(float DamageDone, float DamageRatio, const FHitResult& HitResult, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags);

	FORCEINLINE bool IsAIControlled() const { return bIsAIControlled; }

	void SetIsAIControlled(bool bNewIsAIControlled);

	FORCEINLINE const FAlderonUID& GetCharacterID() const { return CharacterID; }

	void SetCharacterID(const FAlderonUID& NewCharacterID);

protected:
	
	// AI Tracking Flag
	UPROPERTY(BlueprintReadOnly, Category = AI, ReplicatedUsing = OnRep_IsAIControlled)
	uint32 bIsAIControlled : 1;

	// Character ID
	UPROPERTY(Replicated, BlueprintReadOnly)
	FAlderonUID CharacterID;
	
	// TODO: Note (Erlite) THIS IS NOT USED AT ALL. Left in here in case mods use it. Do NOT use it, use the IDinosaurCharacter re-declaration which is apparently being used.
	// True = Female / False = Male
	UPROPERTY(BlueprintReadWrite, Category = Character, Replicated, SaveGame)
	uint32 bGender : 1;

public:

	// Disabling Hunger / Thirst
	void DisableHungerThirst();

protected:
	bool bCanGetHungry = true;
	bool bCanGetThirsty = true;

	// Char Select Random Emotes
public:
	UFUNCTION(BlueprintCallable)
	void CharSelectRandomEmotes();

	virtual void PlayRandomEmote();

protected:
	FTimerHandle TimerHandle_RandomEmote;
	float MinEmoteRange = 15.0f;
	float MaxEmoteRange = 60.0f;

protected:
	UFUNCTION()
	void OnRep_IsAIControlled();

public:
	bool ShouldBlockOnPreload() const;

	void PotentiallySetPreloadingClientArea();
	void SetPreloadPlayerLocks(const bool bLockPlayer);

	void TryFinishPreloadClientArea();

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsPreloadingClientArea() const { return bPreloadingClientArea; }

	void SetPreloadingClientArea(const bool bPreloading);

protected:
	
	UPROPERTY(ReplicatedUsing = OnRep_PreloadingClientArea)
	bool bPreloadingClientArea = false;

	UFUNCTION()
	void OnRep_PreloadingClientArea();

	UPROPERTY(EditDefaultsOnly, Category = AI)
	float AICapsuleScale;

	/************************************************************************/
	/* Group System / Name Tags				                                */
	/************************************************************************/

protected:

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = MapIcon)
	TSoftObjectPtr<UTexture2D> PlayerMapIcon;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = MapIcon)
	TSoftObjectPtr<UTexture2D> LeaderPlayerMapIcon;

	TSoftObjectPtr<UTexture2D> RequestedLocalMapIcon;

public:
	// Initialize Map Icon for locally controlled player
	UFUNCTION(Client, Reliable, BlueprintCallable, Category = NameTag)
	void LoadAndUpdateMapIcon(bool bGroupLeader = false);

	void SetMapIconActive(bool bActive);

	void RefreshMapIcon();
protected:
	// Update Map Icon for locally controlled player
	UFUNCTION(BlueprintCallable, Category = NameTag)
	void OnMapIconLoaded();


private:
	FGenericTeamId TeamId;

public:
	virtual FGenericTeamId GetGenericTeamId() const override;

public:

	/************************************************************************/
	/* AI				                                                    */
	/************************************************************************/

	/*Gets Actor's View point from Actor's Head POV*/
	virtual void GetActorEyesViewPoint(FVector &Location, FRotator & Rotation) const override;
	
	/*Creates Function Get Head Location and Rotation in BP and C++ however can be overwritten from BP*/
	UFUNCTION(BlueprintNativeEvent, Category = "AI Functions | Character")
	void GetHeadLocRot(FVector &OutLocation, FRotator &OutRotation) const;
	void GetHeadLocRot_Implementation(FVector &OutLocation, FRotator &OutRotation) const;

	// Id to be considered Friendly must be the same, to be Enemy must be different, to be Neutral must be 255
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	int AITeamId;

	// Behavior Level of AI
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	EAIBehavior AIBehavior;

	// Chance of AI running solo
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	EAIGroupingScale AIGroupSoloChance;

	// Chance of AI having a mate
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	EAIGroupingScale AIGroupMateChance;

	// Chance of AI joining a pack
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	EAIGroupingScale AIGroupPackChance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	TArray<FVector> AIHerbsFound;

	// Health Threshold which AI will consider itself injured.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AILowHealthThreshold;

	// Distance of which the AI can be interacted with.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAcceptableInteractDistance;

	// How far down to search for source to interact.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	FVector AIInteractEndLoc;

	// Radius of which the AI can interact with food.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIInteractRadius;

	// Radius of which the AI can start attacking.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAttackRadius;

	// Distance of which the AI can be Followed.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAcceptableDistanceToBeFollowed;

	// Rate of which the AI may Attack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAttackRate;

	// Amount of Stamina which is acceptable to Sprint again.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAbleToSprintAgain;

	// Distance of which the AI can attack.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAcceptableAttackDistance;

	// An acceptable distance for the AI to chase their Enemy.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAcceptableChaseDistance;

	// An acceptable distance for the AI to follow their Pack Leader.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAcceptableFollowDistance;

	// An acceptable distance for the AI to eat their prey.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIAcceptableEatingDistance;

	// Distance of which the AI will forget target and return to spawn point.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIResetDistance;

	// Threat Level AI has
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	int AIThreatLevel;

	// AI will avoid from this threat level or higher
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	int AIAvoidanceThreatLevel;

	// AI will flee from this threat level or higher
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	int AIFleeThreatLevel;

	// Amount of AI currently in the Pack
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	int AICurrentPackSize;

	// Max Pack Size this AI will group into
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	int AIMaxPackSize;

	// What Pack ID does this AI belong to
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	int AIPackID;

	// Is AI in a Pack already?
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	bool AIIsInPack;

	// Is AI Friend or Foe?
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	bool AIIsFriendly;
	
	// Distance things outside of won't be relevant for this AI
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIRelevantRange;

	// Max distance at which a makenoise(1.0) loudness sound can be heard, regardless of occlusion 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	float AIHearingThreshold;

	// Max distance at which a makenoise(1.0) loudness sound can be heard if unoccluded (LOSHearingThreshold should be > HearingThreshold)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	float AILOSHearingThreshold;

	// Maximum sight distance
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	float AISightRadius;

	// Amount of time between pawn sensing updates. Use SetSensingInterval() to adjust this at runtime. A value <= 0 prevents any updates
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AI)
	float AISensingInterval;

	// Max age of sounds we can hear. Should be greater than SensingInterval, or you might miss hearing some sounds!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = AI)
	float AIHearingMaxSoundAge;

	// How far to the side AI can see, in degrees. Use SetPeripheralVisionAngle to change the value at runtime. 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AI)
	float AIPeripheralVisionAngle;

	// Radius an animal should detect around it and adequately try to keep other things it wants out of it, "out of it"
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AI)
	float AIAvoidanceRadius;
	
	// much smaller avoidance radius that decides on whether or not it's going to attack something for being too close, or outright running away.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = AI)
	float AIFleeRadius;

	// AI Specific base turning radius.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float AIBaseTurningRadius;
	
	/************************************************************************/
	/* Death			                                                    */
	/************************************************************************/

	UFUNCTION(BlueprintImplementableEvent, Category = Death, Meta = (DisplayName = "On Death (Global)"))
	void OnDeathEvent();

public:
	/************************************************************************/
	/* Gameplay Tag Asset Interface                                        */
	/************************************************************************/

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = GameplayTags)
	FGameplayTagContainer CharacterTags;

	UPROPERTY(EditAnywhere, Category = "Abilities")
	FAbilitySlotData AbilitySlotConfiguration[(uint8)EAbilityCategory::MAX];

	UPROPERTY()
	TMap<EAbilityCategory, FSlottedAbilities> SlottedAbilityAssets;

	FORCEINLINE const TArray<FSlottedAbilities>& GetSlottedAbilityAssetsArray() const { return SlottedAbilityAssetsArray; }

	TArray<FSlottedAbilities>& GetSlottedAbilityAssetsArray_Mutable();

protected:

	UPROPERTY(SaveGame, ReplicatedUsing = OnRep_SlottedAbilityAssets, EditAnywhere)
	TArray<FSlottedAbilities> SlottedAbilityAssetsArray;

public:
	
	void PrepareCooldownsForSave();
	void AddCooldownsAfterLoad();

	void PrepareGameplayEffectsForSave();
	void AddGameplayEffectsAfterLoad();

protected:
	UPROPERTY(SaveGame)
	TArray<FSavedAbilityCooldown> SlottedAbilityCooldowns;

	bool bHasRestoredSavedCooldowns = false;

	UPROPERTY(SaveGame)
	TArray<FSavedGameplayEffectData> ActiveGameplayEffects;

	bool bHasRestoredSavedEffects = false;

public:

	// Disabled save game because of data migration for new attacks
	// re-enable this when we have attack equipping / swapping
	//UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_SlottedAbilityIds) // SaveGame
	//TArray<FPrimaryAssetId> SlottedAbilityIds;

	FORCEINLINE const TArray<FActionBarAbility>& GetSlottedAbilityCategories() const { return SlottedAbilityCategories; }

	TArray<FActionBarAbility>& GetSlottedAbilityCategories_Mutable();
	
	UFUNCTION(BlueprintCallable)
	void BP_AsyncGetAllDinosaurAbilities(FOnDinoAbilitiesLoadedDynamic OnLoad);

protected:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_SlottedAbilityCategories)
	TArray<FActionBarAbility> SlottedAbilityCategories;

public:

	UPROPERTY(SaveGame)
	EActionBarCustomizationVersion ActionBarCustomizationVersion;

	FORCEINLINE const TArray<FSlottedAbilityEffects>& GetSlottedActiveAndPassiveEffects() const { return SlottedActiveAndPassiveEffects; }

	TArray<FSlottedAbilityEffects>& GetSlottedActiveAndPassiveEffects_Mutable();

	bool IsSlottedPassiveEffectActive(const FPrimaryAssetId AbilityId) const;

	UFUNCTION(BlueprintPure)
	const TArray<FPrimaryAssetId>& GetUnlockedAbilities() const { return UnlockedAbilities; }

	TArray<FPrimaryAssetId>& GetUnlockedAbilities_Mutable();

	UFUNCTION(BlueprintCallable)
	void SetUnlockedAbilities(const TArray<FPrimaryAssetId>& InAbilities);

protected:

	UPROPERTY(BlueprintReadOnly, Replicated, ReplicatedUsing = OnRep_SlottedActiveAndPassiveEffects)
	TArray<FSlottedAbilityEffects> SlottedActiveAndPassiveEffects;
	
	UPROPERTY(BlueprintReadOnly, Replicated, SaveGame)
	TArray<FPrimaryAssetId> UnlockedAbilities;

public:


	UFUNCTION(BlueprintCallable)
	bool GetAbilityForSlot(const FActionBarAbility& ActionBarAbility, FPrimaryAssetId& OutId) const;

	bool GetAbilityInCategoryArray(EAbilityCategory Category, FSlottedAbilities& OutAbilities) const;

	DECLARE_DELEGATE_OneParam(FDinosaurAbilitiesLoaded, TArray<UPOTAbilityAsset*>);

	void AsyncGetAllDinosaurAbilities(FDinosaurAbilitiesLoaded OnLoad);

	void HandlePassiveEffectsOnGrowthChanged();

	UFUNCTION(BlueprintCallable)
	void NotifyAbilitiesNotMeetingGrowthRequirement();

private:
	void AsyncGetAllDinosaurAbilities_PostLoad(TSharedPtr<FStreamableHandle> Handle, TArray<FPrimaryAssetId> CurrentDinosaurAttacks, FDinosaurAbilitiesLoaded OnLoad);
	void RemoveActiveAbility(const FPrimaryAssetId& Id);
	void RemovePassiveEffect(const FPrimaryAssetId& Id);
	void AddActiveAbility(const FPrimaryAssetId& Id, const FGameplayAbilitySpecHandle& Handle);
	void AddPassiveEffect(const FPrimaryAssetId& Id, const FActiveGameplayEffectHandle& Handle);

public:
	const struct FGameplayAbilitySpecHandle* GetAbilitySpecHandle(const FPrimaryAssetId& Id) const;
	TArray<FActiveGameplayEffectHandle> GetActiveGEHandles(const FPrimaryAssetId& Id) const;
	
	UFUNCTION(BlueprintPure)
	int32 GetNumberOfSlotsForCategory(const EAbilityCategory Category) const;

	UFUNCTION(BlueprintPure)
	float GetGrowthRequirementForSlot(const FActionBarAbility& ActionBarAbility) const;

	float GetGrowthRequirementForAbility(const EAbilityCategory AbilityCategory, const int32 SlotIndex) const;

	UFUNCTION(BlueprintPure)
	bool IsGrowthRequirementMet(const FActionBarAbility& ActionBarAbility) const;

	bool IsGrowthRequirementMetForAbility(const UPOTAbilityAsset* AbilityAsset) const;

	bool IsGrowthRequirementMetForAbility(const EAbilityCategory AbilityCategory, const int32 SlotIndex) const;

	UFUNCTION(BlueprintPure)
	bool IsValidSlot(const EAbilityCategory Category, const int32 Slot) const;

	UFUNCTION()
	void OnRep_SlottedAbilityIds();

	UFUNCTION()
	void OnRep_SlottedAbilityAssets();
	
	UFUNCTION()
	void OnRep_SlottedAbilityCategories();

	UFUNCTION()
	void OnRep_SlottedActiveAndPassiveEffects();

	UFUNCTION(Server, Reliable)
	void Server_SetAbilityInCategorySlot(int32 InSlot, const FPrimaryAssetId& AbilityId, EAbilityCategory SlotCategory);
	void Server_SetAbilityInCategorySlot_Implementation(int32 InSlot, const FPrimaryAssetId& AbilityId, EAbilityCategory SlotCategory);

	UFUNCTION(Server, Reliable)
	void Server_SetAbilityInActionBar(int32 InSlot, const FPrimaryAssetId& AbilityId, const int32 OldIndex = INDEX_NONE);
	void Server_SetAbilityInActionBar_Implementation(int32 InSlot, const FPrimaryAssetId& AbilityId, const int32 OldIndex = INDEX_NONE);

	UFUNCTION(BlueprintPure)
	bool IsAbilityUnlocked(const FPrimaryAssetId& AbilityId) const;

	bool IsAbilityAssetUnlocked(const UPOTAbilityAsset* AbilityAsset) const;

	UFUNCTION(BlueprintCallable, Reliable, Server)
	void Server_UnlockAbility(const FPrimaryAssetId& AbilityId, bool bSkipCostCheck = false);
	void Server_UnlockAbility_Implementation(const FPrimaryAssetId& AbilityId, bool bSkipCostCheck = false);

	UFUNCTION(BlueprintCallable, Reliable, Server)
	void Server_LockAbility(const FPrimaryAssetId& AbilityId);
	void Server_LockAbility_Implementation(const FPrimaryAssetId& AbilityId);

	UFUNCTION(Reliable, Client)
	void ServerUnlockAbility_ReplySuccess(FPrimaryAssetId AbilityId, int32 NewMarks);
	void ServerUnlockAbility_ReplySuccess_Implementation(FPrimaryAssetId AbilityId, int32 NewMarks);

	UFUNCTION(Reliable, Client)
	void ServerUnlockAbility_ReplyFail(FPrimaryAssetId AbilityId, const FString& DebugText);
	void ServerUnlockAbility_ReplyFail_Implementation(FPrimaryAssetId AbilityId, const FString& DebugText);

	UFUNCTION(Reliable, Client)
	void ServerLockAbility_ReplySuccess(FPrimaryAssetId AbilityId, int32 NewMarks);
	void ServerLockAbility_ReplySuccess_Implementation(FPrimaryAssetId AbilityId, int32 NewMarks);

	UFUNCTION(Reliable, Client)
	void ServerLockAbility_ReplyFail(FPrimaryAssetId AbilityId, const FString& DebugText);
	void ServerLockAbility_ReplyFail_Implementation(FPrimaryAssetId AbilityId, const FString& DebugText);

	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override;

	/************************************************************************/
	/* Ability System Interface                                             */
	/************************************************************************/
	virtual class UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	/************************************************************************/
	/* Eating / Drinking                                                    */
	/************************************************************************/

	// Uses drinking interaction
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Diet")
	bool bCanDrinkWaterSource = true;

	//Diet Requirements
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Diet")
	EDietaryRequirements DietRequirements;

	// Tags to allow for eating food
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "QuestSelection")
	TArray<FName> FoodTags;

	UFUNCTION()
	bool ContainsFoodTag(FName FoodTag);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Diet", meta = (Bitmask, BitmaskEnum = "EFoliageFoodType"))
	uint8 FoliageFoodTypes;

	FORCEINLINE float GetBodyFoodPercentage() const { return BodyFoodPercentage; }

	void SetBodyFoodPercentage(float NewBodyFoodPercentage);

	FORCEINLINE FActiveGameplayEffectHandle GetGE_WellRestedBonus() const { return GE_WellRestedBonus; }

	FActiveGameplayEffectHandle& GetGE_WellRestedBonus_Mutable();

	void SetGE_WellRestedBonus(FActiveGameplayEffectHandle NewGE_WellRestedBonus);
	
protected:
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BodyFoodPercentage)
	float BodyFoodPercentage = 1.0f;
	
	UPROPERTY(BlueprintReadWrite, Replicated)
	FActiveGameplayEffectHandle GE_WellRestedBonus;
	
public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AI)
	float MinAliveBodyFoodPercentage = 0.5f;

	void ApplyWellRestedBonusMultiplier();

	UFUNCTION()
	void OnRep_BodyFoodPercentage();

	UFUNCTION()
	void UpdateDeathEffects();

	UFUNCTION()
	void SoundLoaded_DeathParticle(USoundCue* LoadedSound);

	UFUNCTION()
	void SoundLoaded_WindLoop(USoundCue* LoadedSound);

	UFUNCTION()
	void ParticleLoaded_DeathParticle(UParticleSystem* LoadedParticle);

	float GetSplashEffectScale(float FallVelocity);

	void PlaySplashEffectAtLocation();

	UFUNCTION()
	void UpdateDeathParams();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition|Body")
	TArray<FMeatChunkData> MeatChunkClassesSoft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Condition|Body")
	TSoftClassPtr<AIQuestItem> QuestItemClass;

	FORCEINLINE bool HasSpawnedQuestItem() const { return bHasSpawnedQuestItem; }

	void SetHasSpawnedQuestItem(bool bNewHasSpawnedQuestItem);
	
protected:

	UPROPERTY(Replicated)
	bool bHasSpawnedQuestItem = false;

	float SplashThresholdVelocity = -400.0f;
	float SplashVolumeMultiplier = 2.0f;
	float SplashPitchMultiplier = 5.0f;

public:
	
	UPROPERTY(SaveGame)
	float CalculatedGrowthForRespawn = 0.0f;

	bool bDelayDestructionTillQuestItemSpawned = false;

	UFUNCTION()
	bool CanTakeQuestItem(AIBaseCharacter* User);

	UPROPERTY()
	TArray<FMeatChunkData> MeatChunksToLoad;

	UPROPERTY()
	TArray<FMeatChunkData> AllowedMeatChunks;

	UPROPERTY()
	class AIBaseCharacter* MeatChunkEater;
	bool bMeatChunkSkipAnimOnPickup = false;
	bool bMeatChunkAllowPickupWhileFalling = false;

	TSharedPtr<FStreamableHandle> MeatChunkLoadHandle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Food")
	float FoodConsumedTickRate;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Food")
	float WaterConsumedTickRate;

	FTimerHandle TimerHandle_BodyDestroy;
	void DestroyBody(bool bFromCmd = false);

	UFUNCTION(BlueprintCallable)
	float GetFoodConsumePerTick() const;

	UFUNCTION(BlueprintCallable)
	float GetWaterConsumePerTick() const;

	UFUNCTION(Unreliable, Server)
	void Server_UpdatePelvisPosition();
	
	UFUNCTION(BlueprintCallable)
	void UpdateLocalAreaLoading();

	UFUNCTION(Server, Reliable)
	void Server_SetLocalAreaLoading(const bool bAreaLoading);

	FTimerHandle TimerHandle_StopPelvisUpdate;
	void StopPelvisUpdate();

	FORCEINLINE const FVector_NetQuantize& GetPelvisLocationRaw() const { return PelvisLocation; }

	void SetPelvisLocation(const FVector& NewPelvisLocation);

	FORCEINLINE bool IsLocalAreaLoading() const { return bLocalAreaLoading; }

	void SetIsLocalAreaLoading(bool bNewIsLocalAreaLoading);

protected:

	UPROPERTY(Replicated, Transient, BlueprintReadOnly, Category = "Condition|Physics")
	FVector_NetQuantize PelvisLocation;
	
	UPROPERTY(Replicated, BlueprintReadOnly, Category = IBaseCharacter)
	bool bLocalAreaLoading;
	
public:

	FTimerHandle TimerHandle_LocalAreaLoadingCheck;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic|CameraShakes")
	TSubclassOf<class UCameraShakeBase> CS_Damaged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cosmetic|BleedingDecals")
	TArray< TSubclassOf<class ADecalActor> > BleedingDecals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Cosmetic|Footprints")
	FStepData Footprints;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Condition|Health")
	float GetBleedingRate() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Condition|Health")
	float GetPoisonRate() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Condition|Health")
	float GetVenomRate() const;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	float BleedingRatio;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	float BleedingRatioResting;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	float BleedingRatioSleeping;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	float BleedingRatioTrotting;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	float BleedingRatioSprinting;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	int BleedingHealAmount;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	int BleedingHealAmountResting;

	UPROPERTY(EditAnywhere, Transient, BlueprintReadWrite, Category = "Condition|Health")
	int BleedingHealAmountSleeping;

public:


	/************************************************************************/
	/* Drinking / Eating                                                    */
	/************************************************************************/
	
	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void EatBody(AIBaseCharacter* Body);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void EatPlant(UIFocusTargetFoliageMeshComponent* TargetPlant);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void EatFood(AIFoodItem* Food);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void EatCritter(AICritterPawn* Critter);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void DrinkWater(AIWater* Water);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	bool IsEating() const;

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	bool IsDrinking() const;

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	bool IsEatingOrDrinking() const;

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void SkipMontageTime(UAnimMontage* Montage, float TimeSkipped);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Actions")
	FVector GetNormalizedForwardVector();

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Actions")
	FVector GetNormalizedRightVector();

	bool bMoveForwardWhileAutoRunning = false;
	bool bMoveForwardPressed = false;
	bool bMoveBackwardPressed = false;

	// Note: InputMoveY is only called by Autorun
	void InputMoveY(float InAxis);

	void OnMoveStarted(const FInputActionValue& Value);
	void OnMoveCompleted(const FInputActionValue& Value);
	void OnMoveTriggered(const FInputActionValue& Value);
	void OnLookAround(const FInputActionValue& Value);

	FRotator GetReplicatedControlRotation() const;

	FORCEINLINE const FVector_NetQuantizeNormal& GetReplicatedAccelerationNormal() const { return ReplicatedAccelerationNormal; }

	void SetReplicatedAccelerationNormal(const FVector& NewAccelerationNormal);

protected:

	// ReplicatedAccelerationNormal is used primarily for getting an input normal on simulated proxies when calculating animations. it does not need much precision
	UPROPERTY(Replicated)
	FVector_NetQuantizeNormal ReplicatedAccelerationNormal;
	
	FVector PreviousMovementInput = FVector::ZeroVector;

	UPROPERTY(Config)
	float AnalogMovementInputSmoothingSpeed = 5.0f;

public:

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void InputTurn(float InAxis);
	virtual void InputTurn_Implementation(float InAxis);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void InputLookUp(float InAxis);
	virtual void InputLookUp_Implementation(float InAxis);

	// Mapped to input
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	bool InputStartUse();
	virtual bool InputStartUse_Implementation();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void InputStopUse();
	virtual void InputStopUse_Implementation();

	UFUNCTION(BlueprintCallable, Category = "Actions")
	virtual void StartUsePrimary();

	// Mapped to input
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	bool InputStartCollect();
	virtual bool InputStartCollect_Implementation();

	UPROPERTY()
	bool bCollectHeld = false;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void InputStopCollect();
	virtual void InputStopCollect_Implementation();

	// Mapped to input
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	bool InputStartCarry();
	virtual bool InputStartCarry_Implementation();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void InputStopCarry();
	virtual void InputStopCarry_Implementation();

	// Called when use is held
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void UseHeld();
	virtual void UseHeld_Implementation();

	// Called when use is tapped
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	void UseTapped();
	virtual void UseTapped_Implementation();

	// Called on the Locally Controlled Client Only
	UFUNCTION(BlueprintCallable, Category = "Actions")
	virtual void StopUsingIfPossible();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	bool InputStartBugSnap();
	virtual bool InputStartBugSnap_Implementation();

	/************************************************************************/
	/* Attacking                                                            */
	/************************************************************************/

	UFUNCTION()
	virtual bool OnAbilityInputPressed(int32 SlotIndex);


	UFUNCTION()
	virtual void OnAbilityInputReleased(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "Abilities")
	bool IsUsingAbility() const;

protected:
	virtual void Internal_OnAbilityInputReleased(FGameplayAbilitySpecHandle Handle, bool bForced);

public:

	UFUNCTION(BlueprintCallable)
	void ForceAbilityActivate(int32 SlotIndex);
	
	UFUNCTION()
	virtual void OnAbilityInputPressed_Enhanced(int32 SlotIndex);
    
    
	UFUNCTION()
	virtual void OnAbilityInputReleased_Enhanced(int32 SlotIndex);

	UFUNCTION(BlueprintCallable)
	void ActivateAbility(FPrimaryAssetId AbilityId);

	UFUNCTION(BlueprintCallable)
	void ManuallyPressAbility(int32 SlotIndex);

	UFUNCTION(BlueprintCallable)
	void ManuallyReleaseAbility(int32 SlotIndex);

	UFUNCTION(BlueprintImplementableEvent)
	void OnAbilityFailedLackingGrowth(FPrimaryAssetId AbilityId);
	
	UFUNCTION(BlueprintImplementableEvent)
	void OnAbilityNotMeetingGrowthRequirement(EAbilityCategory Category);

	// Check if pawn is allowed to fire
	UFUNCTION(BlueprintCallable, Category = "Actions")
	virtual bool CanFire() const;

	// Check if pawn is allowed to altfire
	UFUNCTION(BlueprintCallable, Category = "Actions")
	virtual bool CanAltFire() const;

	/************************************************************************/
	/* Material                                                             */
	/************************************************************************/

	void SetTextureParameterValueOnMaterials(const FName ParameterName, UTexture* Texture);

	/************************************************************************/
	/* Movement                                                             */
	/************************************************************************/

	virtual void FaceRotation(FRotator NewControlRotation, float DeltaTime /* = 0.f */) override;

	bool OnStartPreciseMovement();
	void OnStopPreciseMovement();

	UFUNCTION(BlueprintCallable, Category = "Actions")
	bool DesiresTurnInPlace() const;

	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;
	void DamageImpact(const FHitResult& Hit);

	UPROPERTY(BlueprintReadOnly, BlueprintAssignable)
	FOnLanded OnCharacterLanded;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FallDamageDeathThreshold = 60000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	float FallDamageLimpingThreshold = 50000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	bool bCanSprintDuringPreciseMovementSwimming = false;

	bool IsInputBlocked(bool bFromMove = false, bool bFromCall = false);

	virtual bool CanUse();

	// Client mapped to Input
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Actions")
	bool OnStartJump();
	virtual bool OnStartJump_Implementation();

	// Client mapped to Input
	UFUNCTION()
	void OnStopJump();

	void BeginBuck();

	// Mobile - Client mapped to Input
	UFUNCTION(BlueprintCallable, Category = IBaseCharacter)
	void ToggleSprint();

	// Mobile - Client mapped to Input
	UFUNCTION(BlueprintCallable, Category = IBaseCharacter)
	bool ToggleAutoRun();

	bool InputAutoRunStarted();

	bool bAutoRunActive = false;

	// Client mapped to Input
	UFUNCTION(BlueprintCallable, Category = IBaseCharacter)
	virtual bool OnStartSprinting();

	// Client mapped to Input
	UFUNCTION(BlueprintCallable, Category = IBaseCharacter)
	void OnStopSprinting();

	// Client mapped to Input
	bool OnStartTroting();

	// Client mapped to Input
	void OnStopTroting();

	// Client mapped to Input
	bool InputCrouchPressed();

	// Client mapped to Input
	void InputCrouchReleased();

	void ForceUncrouch();

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	void SetMovementEnabled(const bool bEnabled);

	UFUNCTION(Reliable, Server)
	void ServerSetBasicInputDisabled(bool bInIsBasicInputDisabled);
	void ServerSetBasicInputDisabled_Implementation(bool bInIsBasicInputDisabled);

	/************************************************************************/
	/* Godmode                                                              */
	/************************************************************************/

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	bool GetGodmode() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	void SetGodmode(bool bEnabled);

	FORCEINLINE bool IsCombatLogAI() const { return bCombatLogAI; }

	void SetCombatLogAI(bool bEnabled);

protected:
	
	/************************************************************************/
	/* Combat Log AI														*/
	/************************************************************************/
	UPROPERTY(BlueprintReadWrite, ReplicatedUsing = OnRep_CombatLogAi, Category = CombatLog)
	uint32 bCombatLogAI : 1;
	
	// server only flag to indicate that this AI is a user that is respawning
	uint32 bCombatLogAIRespawningUser : 1;

public:
	FORCEINLINE void SetCombatLogRespawningUser() { bCombatLogAIRespawningUser = true; }
	FORCEINLINE void ClearCombatLogRespawningUser() { bCombatLogAIRespawningUser = false; }
	FORCEINLINE bool IsCombatLogRespawningUser() const { return bCombatLogAIRespawningUser; }

	UFUNCTION()
	void OnRep_CombatLogAi();

	FORCEINLINE FAlderonPlayerID GetCombatLogAlderonId() const { return CombatLogAlderonId; }

	void SetCombatLogAlderonId(FAlderonPlayerID NewCombatLogAlderonId);

	FORCEINLINE const FAlderonUID& GetKillerCharacterId() const { return KillerCharacterId; }

	void SetKillerCharacterId(const FAlderonUID& NewKillerCharacterId);

	FORCEINLINE FAlderonPlayerID GetKillerAlderonId() const { return KillerAlderonId; }

	void SetKillerAlderonId(FAlderonPlayerID NewKillerAlderonId);
	
protected:

	UPROPERTY(BlueprintReadWrite, Replicated)
	FAlderonPlayerID CombatLogAlderonId;
	
	// The Character ID of the character who killed this character
	UPROPERTY(Transient, Replicated)
	FAlderonUID KillerCharacterId;

	// The Player ID of the player who killed this character
	UPROPERTY(Transient, Replicated)
	FAlderonPlayerID KillerAlderonId;
	
public:
	
	void SetTempBodyCleanup();

	float GetCombatLogDuration();

	float GetRevengeKillDuration();

	void ExpiredCombatLogAI();

	bool bAbilitySystemSetup = false;

protected:
	FTimerHandle TimerHandle_CombatLogAI;

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Movement")
	void SetIsLimping(bool NewLimping);

protected:
	void UpdateLimpingState();

	/************************************************************************/
	/* Feign Limping														*/
	/************************************************************************/
public:
	bool CanFeignLimp();

	bool InputFeignLimp();

	UFUNCTION(BlueprintPure, Category = "Condition|Movement")
	FORCEINLINE bool IsFeignLimping() const { return bIsFeignLimping; }

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	void SetIsFeignLimping(bool NewLimping);

	UFUNCTION(Reliable, Server)
	void ServerSetFeignLimping(bool NewLimping);
	void ServerSetFeignLimping_Implementation(bool NewLimping);

protected:
	/************************************************************************/
	/* Broken Legs  														*/
	/************************************************************************/

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Movement")
	void BreakLegsFromFalling();

	UFUNCTION(BlueprintImplementableEvent, Category = "Condition|Movement")
	void OnBreakLegsFromFalling();

public:
	/************************************************************************/
	/* Resting																*/
	/************************************************************************/
	UFUNCTION(BlueprintNativeEvent, Category = "Condition|Movement")
	bool CanRest();
	virtual bool CanRest_Implementation();

public:
	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool InputRestPressed();

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual void InputRestReleased();

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	void SetIsResting(EStanceType NewStance);

	UFUNCTION(BlueprintCallable)
	float GetAnimationSequenceLengthScaled(UAnimSequenceBase* AnimSequence);

	UFUNCTION(BlueprintPure, Category = "Condition|Movement")
	FORCEINLINE EStanceType GetRestingStance() const { return Stance; }

	UFUNCTION(Reliable, Server)
	void ServerSetIsResting(EStanceType NewStance);
	void ServerSetIsResting_Implementation(EStanceType NewStance);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Condition|Movement")
	bool IsSleeping() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Condition|Movement")
	bool IsRestingOrSleeping() const;

	TSoftObjectPtr<UAnimMontage> GetEatAnimMontageFromName(FName FilterName);

	/************************************************************************/
	/* Jumping																*/
	/************************************************************************/

public:
	void SetIsJumping(bool NewJumping);

public:

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TArray<FAnimItem> EatAnimMontages;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> EatAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> PickupAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> PickupSwimmingAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> CollectAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> SwallowAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> UnderwaterEatAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> UnderwaterSwallowAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> HurtAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> LandAnimMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gameplay")
	bool bAllowEatWhileSwimming = false;

	// These need to be public so all objects can access them
	FTimerHandle ServerUseTimerHandle;
	FTimerDelegate ServerUseTimerDelegate;

public:
	void ResetServerUseTimer();
	
	/************************************************************************/
	/* Drinking																*/
	/************************************************************************/
public:
	//UFUNCTION(BlueprintPure, Category = "Eating")
	//UAnimMontage* GetDrinkAnimMontage();

	UFUNCTION(BlueprintPure, Category = "Eating")
	TSoftObjectPtr<UAnimMontage> GetDrinkAnimMontageSoft();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Eating")
	bool CanDrinkWater();
	virtual bool CanDrinkWater_Implementation();

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartDrinkingWater(AIWater* Water);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StopDrinkingWater();

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartDrinkingWaterFromLandscape();

	/************************************************************************/
	/* Eating																*/
	/************************************************************************/

	UFUNCTION(BlueprintPure, Category = "Eating")
	virtual TSoftObjectPtr<UAnimMontage> GetEatAnimMontageSoft();

	UFUNCTION(BlueprintPure, Category = "Eating")
	virtual TSoftObjectPtr<UAnimMontage> GetPickupAnimMontageSoft();

	UPROPERTY(BlueprintReadOnly)
	TArray<AIBaseCharacter*> ActivelyEatingBody;

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartEatingBody(AIBaseCharacter* Body);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartEatingPlant(UIFocusTargetFoliageMeshComponent* Plant);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartEatingFood(AIFoodItem* Food);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartEatingCritter(AICritterPawn* Critter);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StartDisturbingCritter(AIAlderonCritterBurrowSpawner* CritterBurrow);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StopDisturbingCritter(AIAlderonCritterBurrowSpawner* CritterBurrow);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool IsDisturbingCritter() const;

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	void DisturbCritter(AIAlderonCritterBurrowSpawner* CritterBurrow);

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void StopEating();

	UFUNCTION(BlueprintCallable, Category = "Eating")
	void PlayEatingSound();

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> SoundHerbivoreEat;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> SoundCarnivoreEat;

	FTimerHandle TimerHandle_UpdateFoodAnimation;
	float TotalFoodAnimationLength;
	float FoodAnimationDuration;

	UPROPERTY(EditDefaultsOnly, Category = Eating)
	FUsableFoodData UsableFoodData;

	FUsableFoodData RequestedFoodData;

	UFUNCTION(BlueprintCallable, Category = Eating)
	FUsableFoodData GetFoodMeshData();

	// Spawn a particle from the food currently being eaten. Called from anim notify BP.
	UFUNCTION(BlueprintCallable, Category = Eating)
	void SpawnFoodParticle(FName SocketName, const FVector& LocOffset, const FRotator& RotOffset, const FVector& ScaleOffset);

private:
	void SpawnFoodParticle_Complete(UParticleSystem* LoadedParticle, FInteractParticleData ParticleData, FName SocketName, FVector LocOffset, FRotator RotOffset, FVector ScaleOffset);

	void InitFoodComponents();
public:
	UFUNCTION(BlueprintCallable, Category = Eating)
	void OnStartFoodAnimation(float TimelineLength, bool bSwallowing);

	UFUNCTION(BlueprintCallable, Category = Eating)
	void AsyncLoadFoodMesh();

	UFUNCTION(BlueprintCallable, Category = Eating)
	void OnFoodMeshLoaded();

	UFUNCTION(BlueprintCallable, Category = Eating)
	void OnUpdateFoodAnimation();

	UFUNCTION(BlueprintCallable, Category = Eating)
	void OnStopFoodAnimation(bool bSwallowing);

protected:
	//UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	//UAnimMontage* DrinkAnimMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TSoftObjectPtr<UAnimMontage> DrinkAnimMontage;

public:
	virtual bool OnStartVocalWheel();
	virtual void OnStopVocalWheel();

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Movement")
	bool CanMove(bool bFromMove = false);
	virtual bool CanMove_Implementation(bool bFromMove = false);

	UFUNCTION(BlueprintNativeEvent, BlueprintPure, Category = "Movement")
	bool CanPreciseMove();
	virtual bool CanPreciseMove_Implementation();

	bool DesiresPreciseMove();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	FORCEINLINE bool IsInitiatedJump() const { return bIsJumping; }

	/************************************************************************/
	/* Object Use Interaction                                               */
	/************************************************************************/
public:
	bool IsObjectInFocusDistance(UObject* ObjectToUse, int32 Item);
	bool IsFishInKillDistance(AIFish* Fish);

	UFUNCTION(BlueprintNativeEvent, Category = "ObjectInteraction")
	bool UseBlocked(UObject* ObjectToUse, EUseType UseType);
	virtual bool UseBlocked_Implementation(UObject* ObjectToUse, EUseType UseType);

	UFUNCTION(BlueprintNativeEvent, Category = "ObjectInteraction")
	bool CanUseWhileSwimming(UObject* ObjectToUse, EUseType UseType);
	virtual bool CanUseWhileSwimming_Implementation(UObject* ObjectToUse, EUseType UseType);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	virtual bool TryUseObject(UObject* ObjectToUse, int32 Item, EUseType UseType);

	UFUNCTION(Server, Reliable)
	void ServerUseObject(UObject* ObjectToUse, int32 Item, float ClientTimeStamp, EUseType UseType);
	virtual void ServerUseObject_Implementation(UObject* ObjectToUse, int32 Item, float ClientTimeStamp, EUseType UseType);

	UFUNCTION(Server, Reliable)
	void ServerCancelUseObject(float ClientTimeStamp);
	virtual void ServerCancelUseObject_Implementation(float ClientTimeStamp);

	UFUNCTION(Client, Reliable)
	void ClientCancelUseObject(float ClientTimeStamp, bool bForce = false);
	virtual void ClientCancelUseObject_Implementation(float ClientTimeStamp, bool bForce = false);

	UFUNCTION(Server, Reliable)
	void ServerStartDrinkingWaterFromLandscape();
	virtual void ServerStartDrinkingWaterFromLandscape_Implementation();

	UFUNCTION(Server, Reliable)
	void ServerStopDrinkingWaterFromLandscape();
	virtual void ServerStopDrinkingWaterFromLandscape_Implementation();



	UFUNCTION(BlueprintPure, Category = "ObjectInteraction")
	bool IsUsingObject();

	UStaticMeshComponent* GetInstancedUsableCustomDepthStaticMeshComponent();

	UFUNCTION(BlueprintPure, Category = "ObjectInteraction")
	bool HasUsableFocus();

	UFUNCTION(BlueprintPure, Category = "ObjectInteraction")
	bool IsAnyMontagePlaying() const;

	UFUNCTION(BlueprintPure, Category = "ObjectInteraction")
	bool IsMontagePlaying(UAnimMontage* TargetMontage);

	UFUNCTION(BlueprintPure, Category = "ObjectInteraction")
	bool IsMontageNotPlaying(UAnimMontage* TargetMontage);

	float LastUseActorClientTimeStamp;

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	const FUsableObjectReference& GetCurrentlyUsedObject();

	void ForceStopEating();

protected:
	TSharedPtr<FStreamableHandle> InteractAnimLoadHandle;

	UPROPERTY(BlueprintReadOnly)
	bool bCarryInProgress = false;

	void LoadUseAnim(float UseDuration, TSoftObjectPtr<UAnimMontage> UseAnimSoft, bool bLandscapeWater = false);
	void OnUseAnimLoaded(float UseDuration, TSoftObjectPtr<UAnimMontage> UseAnimSoft, bool bLandscapeWater);

	void LoadInteractAnim(bool bDrop, UObject* CarriableObject);
	void OnInteractAnimLoaded(TSoftObjectPtr<UAnimMontage> InteractAnimSoft, bool bDrop, UObject* CarriableObject);

	void LoadHurtAnim();
	void OnHurtAnimLoaded(TSoftObjectPtr<UAnimMontage> HurtAnimSoft);

	void LoadLandedMontage();
	void OnLandedMontageLoaded(TSoftObjectPtr<UAnimMontage> LandAnimSoft);

	UFUNCTION()
	virtual void StartUsing(UObject* UsedObject, int32 Item, EUseType UseType);

	virtual void StopAllInteractions(bool bSkipEvents = false);

	void StopEatingAndDrinking();
	void ForceStopDrinking();

public:
	
	void ClearCurrentlyUsedObjects();

	/** FORCEINLINE version of the unfortunately const-incorrect GetCurrentlyUsedObject(). Why :c */
	FORCEINLINE const FUsableObjectReference& NativeGetCurrentlyUsedObject() const { return CurrentlyUsedObject; }

	FUsableObjectReference& GetCurrentlyUsedObject_Mutable();

protected:

	FTimerHandle UseTimerHandle;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_CurrentlyUsedObject)
	FUsableObjectReference CurrentlyUsedObject;

	UFUNCTION()
	virtual void OnRep_CurrentlyUsedObject(FUsableObjectReference OldUsedObject);

	UPROPERTY(Transient, BlueprintReadOnly)
	UAnimMontage* CurrentUseAnimMontage;

private:
	//validate that an object exists and is usable
	bool ObjectExistsAndImplementsUsable(UObject* ObjectToTest);

public:

	//validate that an object exists and is usable
	bool ObjectExistsAndImplementsCarriable(UObject* ObjectToTest) const;

	UFUNCTION(BlueprintCallable)
	bool FocusExistsAndIsUsable();

	UFUNCTION(BlueprintCallable)
	bool FocusExistsAndIsCarriable();

	UFUNCTION(BlueprintCallable)
	bool FocusExistsAndIsCollectable();

	UFUNCTION(BlueprintCallable)
	void SetForcePoseUpdateWhenNotRendered(const bool bNewForcingPoseUpdate);

	UFUNCTION(BlueprintCallable)
	void SetForceBoneUpdateWhenNotRendered(const bool bNewForcingBoneUpdate);

	bool bForcingPoseUpdate = false;
	bool bForcingBoneUpdate = false;

protected:

	/************************************************************************/
	/* Object Carry Interaction                                             */
	/************************************************************************/

	UPROPERTY(BlueprintReadOnly, Transient, ReplicatedUsing = OnRep_RemoteCarriableObject)
	FCarriableObjectReference RemoteCarriableObject;

	UFUNCTION()
	virtual void OnRep_RemoteCarriableObject();

	void LoadRemotePickupAnim();

	void OnRemotePickupAnimLoaded();

	// Used to Pickup
	UFUNCTION()
	virtual void StartCarrying(UObject* CarriableObject, bool bSkipAnimation = false);

	UFUNCTION()
	virtual void StartPickupAnimation(UObject* CarriableObject, bool bInstaCarry);

public:

	UFUNCTION(Server, Reliable)
	virtual void ServerEndCarrying(UObject* CarriableObject, bool bSkipEvents = false, bool bSetDropFromAbility = false, FTransform NewDropTransform = FTransform());
	UFUNCTION(Server, Reliable)
	virtual void ServerEndCarryingAny();
	// Used to Drop
	virtual void EndCarrying(UObject* CarriableObject, bool bSkipEvents = false);

	bool bCanAttackOnServer = false;
	bool bAttacking = false;

protected:

	FTransform DropTransform = FTransform();
	bool bDropFromAbility = false;

	UFUNCTION()
	void OnAttackAbilityStart(UPOTGameplayAbility* Ability, bool bSkip);

	UFUNCTION()
	void OnAttackAbilityEnd(UPOTGameplayAbility* Ability, bool bSkip);

public:

	// Used to detach/attach depending on animation called
	UFUNCTION(BlueprintCallable)
	void OnInteractReady(bool bDrop, UObject* CarriableObject);

	UFUNCTION(BlueprintCallable)
	FTransform GetDropTransform(FVector Start, FVector End, AActor* CarriableActor, bool bForce);

protected:
	FTimerHandle TimerHandle_Interact;
	FTimerHandle TimerHandle_OnStartInteract;

	UFUNCTION()
	void OnInteractAnimFinished();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool ShouldSkipInteractAnimation(UObject* CarriableObject);
	virtual bool ShouldSkipInteractAnimation_Implementation(UObject* CarriableObject);

public:

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	const FCarriableObjectReference& GetCurrentlyCarriedObject() const;

	FCarriableObjectReference& GetCurrentlyCarriedObject_Mutable();

	UFUNCTION(BlueprintPure, Category = "ObjectInteraction")
	bool IsCarryingObject() const;

	UFUNCTION(BlueprintNativeEvent, Category = "ObjectInteraction")
	bool CarryBlocked(UObject* CarriableObject);
	virtual bool CarryBlocked_Implementation(UObject* CarriableObject);

	UFUNCTION(BlueprintCallable, Category = "ObjectInteraction")
	virtual bool TryCarriableObject(UObject* CarriableObject, bool bSkipAnimation = false);

	UFUNCTION(Server, Reliable)
	void ServerCarryObject(UObject* CarriableObject, bool bSkipAnimation = false);
	virtual void ServerCarryObject_Implementation(UObject* CarriableObject, bool bSkipAnimation = false);

	UFUNCTION(Server, Reliable)
	void ServerKillFish(AIFish* TargetFish);
	virtual void ServerKillFish_Implementation(AIFish* TargetFish);

	// Only used when biting a fish.
	UFUNCTION(Client, Reliable)
	void ClientCarryObject(UObject* CarriableObject, bool bSkipAnimation = false);
	virtual void ClientCarryObject_Implementation(UObject* CarriableObject, bool bSkipAnimation = false);

	UFUNCTION(Client, Reliable)
	void ClientCancelCarryObject(bool bForce = false);
	virtual void ClientCancelCarryObject_Implementation(bool bForce = false);

	UFUNCTION()
	virtual void OnClientCancelCarryCorrection();

	UFUNCTION(Server, Reliable)
	void ServerTryDamageCritter(const FGameplayEventData EventData, AICritterPawn* CritterActor);

	/************************************************************************/
	/* Focus / Aiming	                                                    */
	/************************************************************************/
public:
	UFUNCTION(BlueprintPure, Category = "Aiming")
	FRotator GetLocalAimRotation();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aiming")
	float MaxAimBehindYaw;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus : ")
	float MaxFocusTargetDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus : ")
	float MinFocusTargetDistance;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus : ")
	float FocusTargetRadius;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus : ")
	float FreeAimFocalRange;

	const FUsableObjectReference& GetFocusedObject();

	UFUNCTION(BlueprintCallable, Category = Test)
	FUsableObjectReference K2_GetFocusedObject();

	float GetGrowthFocusTargetDistance() const;

protected:
	bool LocationWithinViewAngleAndFocusRange(const FVector& Location) const;

	UPROPERTY(BlueprintReadOnly, Category = "Focus")
	class UFocalPointComponent* DesiredTargetFocalPointComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus : Target Components")
	float ClearTargetFocalPointComponentDelay;

	void SetTargetFocalPointComponent(class UFocalPointComponent* NewTarget);

	void SetRenderOutlinesForFocusObject(UObject* FocusedObject, const bool bRender);
	
	void UpdateFocusAndAimRotation(const float DeltaSeconds);

	virtual void ProcessFocusTargets(FVector TraceStart, FVector TraceEnd, FCollisionShape Shape, FVector &TargetFocalPointLocation, const float DeltaSeconds, bool bForceReset = false);

	virtual void EvaluateFocusFromLocation(FVector& OutFocusFromLocation) const;

	UPROPERTY(BlueprintReadOnly, Category = "Focus")
	class UFocalPointComponent* TargetFocalPointComponent;

public:

	AIBaseCharacter* GetFocusedCharacter();

	FTimerHandle ClearTargetFocalPointComponentTimer;

	UFUNCTION()
	void ClearTargetFocalPointComponent();

public:

	virtual void TeleportSucceeded(bool bIsATest) override;

	UPROPERTY()
	class AIPointOfInterest* SinglePlayerTeleportPOI = nullptr;
	FVector SinglePlayerTeleportLocation = FVector::ZeroVector;

public:
	UFUNCTION(Client, Reliable)
	void Client_TeleportSucceeded();

	UFUNCTION(BlueprintCallable, Category = Gameplay)
	FVector GetSafeTeleportLocation(FVector InLocation, bool& FoundSafeLocation);

	UFUNCTION()
	virtual void OnLevelsLoaded();

	FTimerHandle ClearPreloadAreaHandle;
	void DelayedClearPreloadArea();

	virtual void ServerBetterTeleport(const FVector& DestLocation, const FRotator& DestRotation, bool bAddHalfHeight, bool bForce = true);

private:
	UFUNCTION(Client, Reliable)
	virtual void ClientBetterTeleport(const FVector& DestLocation, const FRotator& DestRotation, bool bForce);
	virtual void ClientBetterTeleport_Implementation(const FVector& DestLocation, const FRotator& DestRotation, bool bForce);

	FTransform BetterTeleportTransform;

	UFUNCTION()
	void EnsureTeleportedRotation(FRotator DestRotation);

	bool bIsTeleporting = false;

public:
	bool IsTeleporting() const { return bIsTeleporting; }
	virtual void UpdateQuestsFromTeleport();
	virtual bool TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest = false, bool bNoCheck = false) override;
	virtual bool TeleportToFromCommand(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest = false, bool bNoCheck = false);

	void SetDesiredAimRotation(FRotator NewAimRotation);

protected:
	float LastTeleportTime = 0.0f;

	// How many seconds after teleporting, fall damage will be ignored
	UPROPERTY(Config)
	float TeleportFallDamageImmunitySeconds = 0.5f;

	void TryTeleportTo(FVector Location, FRotator Rotation);
	FTimerHandle ListenServerTeleportHandle = FTimerHandle();

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	FRotator AimRotationRate;
	
	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	FRotator DefaultAimOffset;

	FRotator AimRotation;
	FRotator DesiredAimRotation;

public:
	
	FORCEINLINE uint8 GetRemoteDesiredAimPitch() const { return RemoteDesiredAimPitch; }

	void SetRemoteDesiredAimPitch(uint8 NewRemoteDesiredAimPitch);

	FORCEINLINE uint8 GetRemoteDesiredAimYaw() const { return RemoteDesiredAimYaw; }

	void SetRemoteDesiredAimYaw(uint8 NewRemoteDesiredAimYaw);

protected:

	UPROPERTY(Transient, Replicated)
	uint8 RemoteDesiredAimPitch;

	UPROPERTY(Transient, Replicated)
	uint8 RemoteDesiredAimYaw;

public:

	FORCEINLINE uint8 GetRemoteDesiredAimPitchReplay() const { return RemoteDesiredAimPitchReplay; }

	void SetRemoteDesiredAimPitchReplay(uint8 NewRemoteDesiredAimPitchReplay);

	FORCEINLINE uint8 GetRemoteDesiredAimYawReplay() const { return RemoteDesiredAimYawReplay; }

	void SetRemoteDesiredAimYawReplay(uint8 NewRemoteDesiredAimYawReplay);
	
protected:

	UPROPERTY(Transient, Replicated)
	uint8 RemoteDesiredAimPitchReplay;

	UPROPERTY(Transient, Replicated)
	uint8 RemoteDesiredAimYawReplay;

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bDesiresPreciseMovement : 1;

	// Local Client Only
	float TimeSinceLastServerUpdateAim = 0.0f;

	void UpdateAimRotation(float DeltaSeconds);

	//called on the local client to set the aim
	UFUNCTION(Unreliable, Server)
	virtual void ServerUpdateDesiredAim(uint8 ClientPitch, uint8 ClientYaw);
	virtual void ServerUpdateDesiredAim_Implementation(uint8 ClientPitch, uint8 ClientYaw);

public:
	void UnHighlightFocusedObject();
	void HighlightFocusedObject();
	void UnHighlightObject(TWeakObjectPtr<UObject> Object);
	void HighlightObject(TWeakObjectPtr<UObject> Object);

protected:
	virtual void SetFocusedObjectAndItem(UObject* NewFocusedObject, int32 FocusedItem, bool bUnpossessed = false);
	virtual void ToggleInteractionPrompt(bool bValidFocusable);
	virtual void ShowQuestDescriptiveText();

	bool IsQuestDescriptiveCooldownActive(FQuestDescriptiveTextCooldown QuestDescriptiveCooldown);

	UFUNCTION(BlueprintCallable, BlueprintPure)
	bool ShouldShowInteractionPrompt();

	UPROPERTY(BlueprintReadOnly, Category = "Focus")
	FUsableObjectReference FocusedObject;

	UPROPERTY(BlueprintReadOnly)
	bool bContinousUse = false;

	UPROPERTY(BlueprintReadOnly)
	bool bContinousUseAnimLoop = false;

	UPROPERTY()
	TArray<FQuestDescriptiveTextCooldown> QuestDescriptiveCooldowns;

public:

	/************************************************************************/
	/* Ragdoll                                                             	*/
	/************************************************************************/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Condition|Physics")
	UPhysicalAnimationComponent* PhysicalAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Physics")
	FPhysicalAnimationData PhysicalAnimationLandData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Physics")
	FPhysicalAnimationData PhysicalAnimationSwimmingData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Physics")
	FName TailRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Physics")
	bool bIncludeRootInTailPhysics = false;

	FPhysicalAnimationData PhysicalAnimationCurrentData;
	float TailPhysicsBlendRatio = 0.0f;
	bool bShouldAllowTailPhysics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition|Physics")
	float TailPhysicsBlendWeight = 1.0;
	float LastTailPhysicsBlendWeight = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition|Physics")
	float UnderwaterRagdollLinearDamping = 25.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Condition|Physics")
	float UnderwaterRagdollAngularDamping = 720.f;

	/************************************************************************/
	/* Sound                                                             	*/
	/************************************************************************/

	UAudioComponent* PlayCharacterSound(USoundCue* CueToPlay);

	UPROPERTY(VisibleAnywhere, Category = "Sound")
	UAudioComponent* AudioLoopDeathComp;

	UPROPERTY(VisibleAnywhere, Category = "Sound")
	UAudioComponent* AudioLoopWindComp;

	UPROPERTY(VisibleAnywhere, Category = "Sound")
	UAudioComponent* AudioSplashComp;

	UPROPERTY(VisibleAnywhere, Category = "Death|Particle")
	UParticleSystemComponent* DeathParticleComponent;

	UPROPERTY(VisibleAnywhere, Category = "Death|Particle")
	UParticleSystemComponent* SplashParticleComponent;

	// Dead body flies sound
	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> SoundDeathLoop;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<UParticleSystem> ParticleDeath;

	// Percentage eg 0.90 (90%) of food remaining before flies start
	float FliesStartPercentage = 0.90;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> SoundTakeHit;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> SoundBreakLegs;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> SoundDeath;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TSoftObjectPtr<USoundCue> HighSpeedWind;
	bool windSoundLoading = false;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	float WindVolumeMultiplier = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "Sound")
	TArray<UAudioComponent*> AnimNotifySounds;

	UPROPERTY(EditDefaultsOnly, Category = "Splash|Particle")
	UParticleSystem* ParticleSplash;

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	USoundCue* SoundSplash;

	/**
	 * Spawns a new audio component that will play the given sound which may or may not be attached to this character.
	 * @param Params The parameters to use for this request. 
	 */
	void SpawnSoundAttachedAsync(FSpawnAttachedSoundParams& Params);

	/**
	 * Spawns a new audio component that will play the given footstep sound which may or may not be attached to this character.
	 * @param Params The parameters to use for this request. 
	 */
	void SpawnSoundFootstepAsync(FSpawnFootstepSoundParams& Params);

	/**
	 * Spawns a new particle system component that will play the given particle system at the provided location and rotation.
	 * @param Params The parameters to use for this request. 
	 */
	void SpawnFootstepParticleAsync(FSpawnFootstepParticleParams& Params);

private:

	/** Note: made private, should not be called manually to avoid GC issues. Always use the async calls, even if your soft pointer is """"valid"""" */
	
	void SpawnSoundAttachedCallback(const FSpawnAttachedSoundParams Params);

	void SpawnSoundFootstepCallback(const FSpawnFootstepSoundParams Params);

	void SpawnFootstepParticleCallback(const FSpawnFootstepParticleParams Params);

protected:

	/**
	 * Async loading handles.
	 * Kept to reference async loaded assets until their callbacks are executed.
	 */

	TMap<uint64, TSharedPtr<FStreamableHandle>> AsyncLoadingHandles = {};

	uint64 NextAsyncLoadingRequestId = 0;
	
public:

	/************************************************************************/
	/* Damage & Health                                                      */
	/************************************************************************/

	UFUNCTION(BlueprintCallable)
	virtual void Suicide();

	virtual void KilledBy(class APawn* EventInstigator);

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	void ResetHungerThirst();

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	bool IsHungry() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	bool IsThirsty() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual float GetGrowthPercent() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual float GetGrowthLevel() const;

	UFUNCTION(BlueprintCallable, Category = "Growth")
	virtual void SetGrowthPercent(float Percentage);

	/**
	 * @param GrowthPenaltyPercent  Penalty percentage as a number from 0 to 100
	 * @param bSubtract  If true, directly subtract the percentage from the current growth.
	                     If false, remove a percentage from the current growth.
	 * @param bOverrideLoseGrowthPastGrowthStages  If true, Ignore the bLoseGrowthPastGrowthStages config setting and allow the growth to go past growth stages.
	 * @param bUpdateWellRestedGrowth  If true, subtract the growth penalty from the start and end the well rested growth effect if it's currently active
	 */
	UFUNCTION(BlueprintCallable, Category = "Growth")
	virtual void ApplyGrowthPenalty(float GrowthPenaltyPercent, bool bSubtract = false, bool bOverrideLoseGrowthPastGrowthStages = false, bool bUpdateWellRestedGrowth = false);

	UFUNCTION(BlueprintCallable, Category = "Growth")
	virtual void ApplyGrowthPenaltyNextSpawn(float GrowthPenaltyPercent, bool bSubtract = false, bool bOverrideLoseGrowthPastGrowthStages = false, bool bUpdateWellRestedGrowth = false);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual bool IsGrowing() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual bool IsWellRested(bool bSkipGameplayEffect = false) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual float GetWellRestedBonusMultiplier() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual float GetWellRestedBonusStartedGrowth() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Growth")
	virtual float GetWellRestedBonusEndGrowth() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	virtual float GetMaxHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	virtual float GetHealth() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	virtual float GetCombatWeight() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	void SetHealth(float Health);

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	bool GetIsDying() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Health")
	bool IsAlive() const;

	UFUNCTION(BlueprintPure)
	bool IsInCombat() const;

	UFUNCTION(BlueprintPure)
	bool IsHomecaveBuffActive() const;

	UFUNCTION(BlueprintPure)
	bool IsHomecaveCampingDebuffActive() const;

	//UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	//void LoadStats(int32 NewHealth, int32 NewHunger, int32 NewThirst, int32 NewStamina, float NewLegDamage, EMovementMode MovementMode, uint8 CustomMovement);
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void ResetStats();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void RestoreHealth(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void RestoreHunger(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void RestoreThirst(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void RestoreStamina(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void RestoreOxygen(int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Condition|Diet")
	void RestoreBodyFood(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsLimping() const;

	UFUNCTION(BlueprintPure, Category = "Condition|Movement")
	virtual float GetLimpThreshold() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool HasBrokenLegs() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual float GetLegDamage() const;

	UFUNCTION(BlueprintPure, Category = "Condition|Movement")
	bool AreLegsBroken() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsResting() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	bool IsAttacking() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsSprinting() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsSwimming() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsFlying() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsTrotting() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsWalking() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsNotMoving() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsAtWaterSurface() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool IsInWater(float PercentageNeededToTrigger = 50.0f, FVector StartLocation = FVector(ForceInit)) const;

	virtual bool IsInOrAboveWater(FHitResult* OutHit = nullptr, const FVector* CastStart = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual bool CanSleepOrRestInWater(float PercentToTrigger = 50.0f) const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual void AdjustCapsuleWhenSwimming(ECustomMovementType NewMovementType);

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual ECustomMovementType GetCustomMovementType();

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual ECustomMovementMode GetCustomMovementMode();

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual float GetSprintingSpeedModifier() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual float GetTrottingSpeedModifier() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	bool IsExhausted() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	float GetStamina() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	void SetStamina(float Value);

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual float GetMaxStamina() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	virtual float GetStaminaRecoveryRate() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	float GetStaminaCarryCostMultiplier() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	float GetStaminaBadWeatherCostMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "Condition|Movement")
	bool IsBucking() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	virtual float GetHunger() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	void SetHunger(float Value);

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	virtual float GetMaxHunger() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	virtual float GetThirst() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	void SetThirst(float Value);

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	virtual float GetMaxThirst() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	float GetOxygen() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	void SetOxygen(float Value);

	UFUNCTION(BlueprintCallable, Category = "Condition|Diet")
	virtual float GetMaxOxygen() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Body")
	virtual float GetMaxBodyFood() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Body")
	virtual float GetCurrentBodyFood() const;

	virtual bool CanJumpInternal_Implementation() const override;

	virtual void Crouch(bool bClientSimulation = false) override;
	virtual bool CanCrouch() const override;
	virtual bool CanUncrouch() const;

	UFUNCTION(BlueprintCallable, Category = "Condition|Movement")
	bool CanBuck() const;

	virtual void OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation) override;
	virtual void PostNetReceiveLocationAndRotation() override;

	virtual float GetMovementSpeedMultiplier() const;

	FORCEINLINE float GetStaminaJumpModifier() const
	{
		return StaminaJumpModifier;
	}

	

public:

	/************************************************************************/
	/* Sprinting                                                            */
	/************************************************************************/

	// Space needed below the capsule for dinosaur to dive
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Swimming")
	float SpaceNeededToDive = 20.0f;

	// Amount to offset the top of the capsule for floating calculations
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Swimming")
	float FloatOffset = 0.0f;

	// Space needed from top of capsule to detect being at water surface
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Swimming")
	float SpaceNeededToFloat = 100.0f;

	//The amount of time in seconds that the dino cannot crouch after landing from flying
	UPROPERTY(EditDefaultsOnly, Category = "Movement|Flying")
	float TimeCantCrouchAfterLanding = 0.25f;
	float TimeCantCrouchElapsed = 0.0f;

	//This is used for tracking stamina decay.
	int32 ConsecutiveJumps;

protected:

	virtual bool CanSprint();
	bool ShouldCancelToggleSprint() const;
	virtual bool CanTrot() const;
	virtual bool CanTurnInPlace();


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Movement")
	int32 StaminaJumpDecay;


	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Movement")
	float StaminaJumpModifier;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Movement")
	float StaminaJumpModifierCooldown;
	float TimeSinceLastJump;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Movement")
	float StaminaCarryCostMultiplier = 1.1f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Condition|Movement")
	float StaminaBadWeatherCostMultiplier = 1.2f;

	void ResetJumpModifier();

	FTimerHandle TimerHandle_ResetJumpModifier;

public:
	UPROPERTY()
	TWeakObjectPtr<AController> LastDamageInstigator;

	/* Take damage & handle death */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser) override;

	virtual bool CanDie(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser) const;

	virtual bool Die(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser);

	virtual void OnDeath(float KillingDamage, const FPOTDamageInfo& DamageInfo, APawn* PawnInstigator, AActor* DamageCauser, FVector HitLoc, FRotator HitRot);


	virtual void OnDeathCosmetic();

	virtual void FellOutOfWorld(const class UDamageType& DmgType) override;

	void SetRagdollPhysics();

	void StopRagdollPhysicsDelayed(float Delay = 10.0f);

	FTimerHandle TimerHandle_StopRagdollDelay;
	void StopRagdollPhysics();

	UFUNCTION(BlueprintCallable)
	void BlendTailPhysics(bool bSwimming, float DeltaSeconds);

	UFUNCTION(BlueprintCallable)
	bool ShouldUpdateTailPhysics();

	virtual void PlayHit(float DamageTaken, const FPOTDamageInfo& DamageInfo, APawn* PawnInstigator, AActor* DamageCauser, bool bKilled, FVector HitLoc, FRotator HitRot, FName BoneName);
	void ReplicateHit(float DamageTaken, const FPOTDamageInfo& DamageInfo, APawn* PawnInstigator, AActor* DamageCauser, bool bKilled, FVector HitLoc, FRotator HitRot, FName BoneName);

	FORCEINLINE bool HasDiedInWater() const { return bDiedInWater; }

	void SetHasDiedInWater(bool bNewHasDiedInWater);
	
protected:
	
	UPROPERTY(Transient, Replicated)
	bool bDiedInWater;

private:
	// This flag is true if the player died from hitting a killZ volume, i.e. they fell through the world. It is reset to false almost immediately.
	bool bDiedByKillZ;

public:

	FORCEINLINE const FTakeHitInfo& GetLastTakeHitInfo() const { return LastTakeHitInfo; }

	FTakeHitInfo& GetLastTakeHitInfo_Mutable();

	void SetLastTakeHitInfo(const FTakeHitInfo& NewLastTakeHitInfo);
	
protected:

	/* Holds hit data to replicate hits and death to clients */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_LastTakeHitInfo)
	struct FTakeHitInfo LastTakeHitInfo;

public:

	UFUNCTION()
	void OnRep_LastTakeHitInfo();

	/************************************************************************/
	/* Particles													        */
	/************************************************************************/
	void ReplicateParticle(FDamageParticleInfo DamageInfo);
	void ReplicateParticle(FCosmeticParticleInfo EffectInfo);
	void GetParticleLocation(const FVector& WorldPosition, FClosestPointOnPhysicsAsset& ClosestPointOnPhysicsAsset, bool bApproximate) const;

	FTimerHandle TimerHandle_CheckLocalParticles;
	void StartCheckingLocalParticles();
	void StopCheckingLocalParticles();
	void CheckLocalParticles();

	/************************************************************************/
	/* Damage Particles		                                                */
	/************************************************************************/

	FORCEINLINE const TArray<FDamageParticleInfo>& GetReplicatedDamageParticles() const { return ReplicatedDamageParticles; }

	TArray<FDamageParticleInfo>& GetReplicatedDamageParticles_Mutable();

protected:
	
	/* Holds hit data to replicate particles to clients */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedDamageParticles)
	TArray<FDamageParticleInfo> ReplicatedDamageParticles;
	
public:
	
	UPROPERTY(BlueprintReadOnly, SaveGame)
	FDeathInfo DeathInfo;

	UPROPERTY(BlueprintReadOnly, SaveGame)
	FDateTime LastPlayedDate;

	float LastSaveTime;

	

	EDamageEffectType DamageEffectLoadingType;
	TSharedPtr<FStreamableHandle> PreviewDamageParticleLoadHandle;

	UFUNCTION()
	void OnRep_ReplicatedDamageParticles();

	void RemoveReplicatedDamageParticle(EDamageEffectType DamageEffectTypeToRemove);
	TSoftObjectPtr<UFXSystemAsset> GetDamageParticle(const FDamageParticleInfo& DamageParticleInfo);
	void RegisterDamageParticle(FDamageParticleInfo DamageParticleInfo);
	void LoadDamageParticle(FDamageParticleInfo DamageParticleInfo, bool bRegister = true);
	void OnDamageParticleLoaded(FDamageParticleInfo DamageParticleInfo, bool bRegister = true);
	void DeregisterDamageParticle(FDamageParticleInfo DamageParticleInfo, bool bDeregisterAll = false);
	void DeregisterAllDamageParticles();

	UPROPERTY(BlueprintReadOnly, Category = "VFX|Damage|Death")
	TArray<FDamageParticleInfo> LocalDamageEffectParticles;
	
	// Used when dinosaur has a damage status outside the others
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Damage", meta=(ForceInlineRow))
	TMap<EDamageEffectType, FPOTParticleDefinition> DamageEffectParticles;

	/************************************************************************/
	/* Cosmetic particles                                                   */
	/************************************************************************/

	FORCEINLINE const TArray<FCosmeticParticleInfo>& GetReplicatedCosmeticParticles() const { return ReplicatedCosmeticParticles; }

	TArray<FCosmeticParticleInfo>& GetReplicatedCosmeticParticles_Mutable();	

protected:
	
	/* Holds effect data to replicate particles to clients */
	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedCosmeticParticles)
	TArray<FCosmeticParticleInfo> ReplicatedCosmeticParticles;

public:

	UPROPERTY(BlueprintReadOnly, Category = "VFX|Cosmetic")
	TArray<FCosmeticParticleInfo> LocalCosmeticEffectParticles;

	ECosmeticEffectType CosmeticEffectLoadingType;
	TSharedPtr<FStreamableHandle> PreviewCosmeticParticleLoadHandle;

	UFUNCTION()
	void OnRep_ReplicatedCosmeticParticles();

	void RemoveReplicatedCosmeticParticle(ECosmeticEffectType CosmeticEffectTypeToRemove);
	virtual TSoftObjectPtr<UFXSystemAsset> GetCosmeticParticle(const FCosmeticParticleInfo CosmeticParticleInfo);
	void RegisterCosmeticParticle(FCosmeticParticleInfo CosmeticParticleInfo);
	void LoadCosmeticParticle(FCosmeticParticleInfo CosmeticParticleInfo, bool bRegister = true);
	void OnCosmeticParticleLoaded(FCosmeticParticleInfo CosmeticParticleInfo, bool bRegister = true);
	void DeregisterCosmeticParticle(FCosmeticParticleInfo CosmeticParticleInfo, bool bDeregisterAll = false);
	void DeregisterAllCosmeticParticles();

	// Used when dinosaur has a damage status outside the others
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Cosmetic", meta=(ForceInlineRow))
	TMap<ECosmeticEffectType, TSoftObjectPtr<UFXSystemAsset>> CosmeticEffectParticles;


	/************************************************************************/
	/* Being eaten		                                                    */
	/************************************************************************/

	/*IUsableInterface implementation*/
public:

	// Universal
	virtual uint8 GetFocusPriority_Implementation() override;
	virtual FVector GetUseLocation_Implementation(class AIBaseCharacter* User, int32 Item) override;
	virtual bool IsUseMeshStatic_Implementation(class AIBaseCharacter* User, int32 Item) override;
	virtual TSoftObjectPtr<USkeletalMesh> GetUseSkeletalMesh_Implementation(class AIBaseCharacter* User, int32 Item) override;
	virtual TSoftObjectPtr<UStaticMesh> GetUseStaticMesh_Implementation(class AIBaseCharacter* User, int32 Item) override;
	virtual FVector GetUseOffset_Implementation(class AIBaseCharacter* User, int32 Item) override;
	virtual FRotator GetUseRotation_Implementation(class AIBaseCharacter* User, int32 Item) override;
	virtual bool CanBeFocusedOnBy_Implementation(AIBaseCharacter* User) override;
	virtual void OnBecomeFocusTarget_Implementation(class AIBaseCharacter* Focuser) override;
	virtual void OnStopBeingFocusTarget_Implementation(class AIBaseCharacter* Focuser) override;
	virtual void GetFocusTargetOutlineMeshComponents_Implementation(TArray<class UMeshComponent*>& OutOutlineMeshComponents) override;

	// Primary
	virtual float GetPrimaryUseDuration_Implementation(class AIBaseCharacter* User) override;
	virtual TSoftObjectPtr<UAnimMontage> GetPrimaryUserAnimMontageSoft_Implementation(class AIBaseCharacter* User) override;
	virtual bool CanPrimaryBeUsedBy_Implementation(AIBaseCharacter* User) override;
	virtual bool UsePrimary_Implementation(AIBaseCharacter* User, int32 Item) override;
	virtual void EndUsedPrimaryBy_Implementation(AIBaseCharacter* User) override;
	virtual void OnClientCancelUsePrimaryCorrection_Implementation(AIBaseCharacter* User) override;

	// Carry
	virtual float GetCarryUseDuration_Implementation(class AIBaseCharacter* User) override;
	virtual bool CanCarryBeUsedBy_Implementation(AIBaseCharacter* User) override;
	virtual bool UseCarry_Implementation(AIBaseCharacter* User, int32 Item) override;
	virtual void EndUsedCarryBy_Implementation(AIBaseCharacter* User) override;
	virtual void OnClientCancelUseCarryCorrection_Implementation(AIBaseCharacter* User) override;

	UPROPERTY(EditDefaultsOnly)
	FInteractParticleData InteractParticle;

	virtual bool GetInteractParticle_Implementation(FInteractParticleData& OutParticleData) override;

	UFUNCTION(BlueprintCallable)
	void SpawnMeatChunk(class AIBaseCharacter* Eater, bool bSkipAnimOnPickup = false, bool bAffectFoodValue = false, bool bAllowPickupWhileFalling = false);

	// Start Loading Data Asset
	void StartLoadingMeatChunk();

	// Start Spawn Process when Fish is Loaded
	void OnMeatChunkLoaded(FMeatChunkData MeatChunkHard);

	void SpawnQuestItem(class AIBaseCharacter* Eater);

	// Start Loading Data Asset
	void StartLoadingQuestItem();

	// Start Spawn Process when Fish is Loaded
	void OnQuestItemLoaded();

	/*End IUsableInterface implementation*/

	/************************************************************************/
	/* View              													*/
	/************************************************************************/
public:
	virtual FRotator GetBaseAimRotation() const override;

	/* Mapped to input */
	UFUNCTION(BlueprintNativeEvent, Category = "Actions")
	void CameraIn();
	virtual void CameraIn_Implementation();

	/* Mapped to input */
	UFUNCTION(BlueprintNativeEvent, Category = "Actions")
	void CameraOut();
	virtual void CameraOut_Implementation();

	UFUNCTION()
	void OnCameraZoom(const FInputActionValue& Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector2D CameraDistanceRange;

	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	FVector2D CameraDistanceRangeWhileCarried;

	UPROPERTY(EditAnywhere, BlueprintReadonly, Category = "Camera")
	UISpringArmComponent* ThirdPersonSpringArmComponent;

	UPROPERTY(EditAnywhere, BlueprintReadonly, Category = "Camera")
	UCameraComponent* ThirdPersonCameraComponent;

	UPROPERTY(EditAnywhere, BlueprintReadonly, Category = "Mesh")
	USkeletalMeshComponent* FoodSkeletalMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadonly, Category = "Mesh")
	UStaticMeshComponent* FoodStaticMeshComponent;

	UFUNCTION(BlueprintCallable)
	FVector2D GetCurrentCameraDistanceRange() const;

	virtual FVector GetHeadLocation() const;
	virtual FVector GetPelvisLocation() const;

	UPROPERTY(BlueprintReadWrite, Category = "Camera")
	APhysicsVolume* CurrentCameraPhysicsVolume;

protected:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	float SpringArmMoveSpeed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	float SpringArmRotateRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	bool bEnableNewCameraSystem;

	UFUNCTION(BlueprintImplementableEvent)
	void UpdateCameraOffsetBP(float DeltaTime);

	void UpdateCameraOffset(float DeltaTime);

	UFUNCTION()
	void OnPushComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
	UFUNCTION()
	void OnPushComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void PushCharacter(float DeltaTime);

	virtual void ModifyCameraLocation(FVector& WorldCameraLocation) {};

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	float CameraDollyTargetAdjustSize;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	float CameraDollyRate; 

	UPROPERTY(BlueprintReadOnly, Category = "Camera - Old")
	float DesireSpringArmLength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName HeadBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FName PelvisBoneName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	float DesiredRagdollStrength;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	UCurveFloat* CameraDistanceToHeadMovementContributionCurve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	FVector2D CameraRotRelativeFOVRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	FVector2D CameraRotRelativeOffsetRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	FVector2D CameraRotRelativeSpeedRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera - Old")
	float SpeedBasedCameraEffectLerpRate;

	float CurrentSpeedBasedCameraEffectAlpha;

private:
	FTransform CachedThirdPersonSpringArmTransform;

	void SetDesireSpringArmLength(const float NewDesireSpringArmLength);

	/************************************************************************/
	/* Dismemberment              										    */
	/************************************************************************/
public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Dismeberment")
	void SliceThroughBone(const FName BoneName);

	// Packed Variables
public:
	// Key to disable basic movement input when doing actions like pouncing
	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	uint32 bIsBasicInputDisabled : 1;	
		
	UPROPERTY(BlueprintReadWrite, Category = "Movement")
	uint32 bIsAbilityInputDisabled : 1;

	UPROPERTY(BlueprintReadWrite, Category = "Condition|Movement")
	uint32 bDesiresToSprint : 1;

	UPROPERTY(BlueprintReadWrite, Category = "Condition|Movement")
	uint32 bDesiresToTrot : 1;

	UPROPERTY(BlueprintReadWrite, Category = "Condition|Movement")
	uint32 bDesiredUncrouch : 1;

	FORCEINLINE ECustomMovementType GetCurrentMovementType() const { return CurrentMovementType; }

	void SetCurrentMovementType(ECustomMovementType NewCurrentMovementType);

	FORCEINLINE ECustomMovementMode GetCurrentMovementMode() const { return CurrentMovementMode; }

	void SetCurrentMovementMode(ECustomMovementMode NewCurrentMovementMode);
	
protected:

	// Local / Simulated States
	UPROPERTY(Transient, Replicated, BlueprintReadOnly, Category = "Movement")
	ECustomMovementType CurrentMovementType;
	
	UPROPERTY(Transient, Replicated, BlueprintReadOnly, Category = "Movement")
	ECustomMovementMode CurrentMovementMode;

public:
	
	bool IsNosediving();
	bool IsGliding();

	UFUNCTION(BlueprintPure, Category = "Gameplay")
	virtual bool IsAquatic() const;

	UPROPERTY(BlueprintReadOnly)
	bool bTransitioningToStance = false;

	/** Raw getter for bIsNosediving, you probably wanna use IsNosediving() in most cases. */
	FORCEINLINE bool IsNosedivingRaw() const { return bIsNosediving; }

	void SetIsNosediving(bool bNewIsNosediving);

	/** Raw getter for bIsGliding, you probably wanna use IsGliding() in most cases. */
	FORCEINLINE bool IsGlidingRaw() const { return bIsGliding; }

	void SetIsGliding(bool bNewIsGliding);
	
protected:
	
	// Is character currently performing a jump action. Resets on landed.
	UPROPERTY(Transient, Replicated)
	uint32 bIsJumping : 1;

	UPROPERTY(Transient, Replicated)
	uint32 bIsNosediving : 1;

	UPROPERTY(Transient, Replicated)
	uint32 bIsGliding : 1;

	UPROPERTY(EditDefaultsOnly, Category = "Condition|Movement")
	uint32 bEnableJumpModifier : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Movement")
	uint32 bCanEverLimp : 1;

public:

	/** Gets the raw bIsLimping value. Use IsLimping() in most cases. */
	FORCEINLINE bool IsLimpingRaw() const { return bIsLimping; }


protected:

	UPROPERTY(Transient, Replicated)
	uint32 bIsLimping : 1;

	UPROPERTY(Transient, Replicated)
	uint32 bIsFeignLimping : 1;

	UPROPERTY(BlueprintReadOnly, Transient, Replicated)
	EStanceType Stance;

	FTimerHandle TimerHandle_TransitionCheck;

	void TransitionCheck();

	bool bRestButtonHeld = false;

	float RestHoldTime = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Movement")
	float RestMovementTolerance = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Movement")
	float RestUnderwaterMovementTolerance = 25.f;

	float RestHoldThreshold = 0.2f;
	float WaystoneHoldThreshold = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Movement")
	uint32 bCanEverRest : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition|Movement")
	uint32 bCanEverSleep : 1;

	uint32 bIsDying : 1;

	// Used to detect if key is held down
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bUseButtonHeld = false;

	// Used to detect if Use Held has been called
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	bool bUseHeldCalled = false;

	// Used to detect how long Use key is held down
	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float UseHoldTime = 0.0f;

	// Used to detect how long a key should be held before hold use action is called
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	float UseHoldThreshold = 0.2f;

	UPROPERTY(Transient)
	uint32 bIsEating : 1;

	UPROPERTY(Transient)
	uint32 bIsDrinking : 1;

	UPROPERTY(Transient)
	uint32 bIsDisturbing : 1;

	UPROPERTY(EditDefaultsOnly, Category = "Aiming")
	uint32 bUseSeparateAimRotation : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	uint32 bAquatic : 1;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	uint32 bUpdateFocusBasedOnPlayerView : 1; //includes AI

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Focus")
	uint32 bRotateToNewViewTargetAroundPawnViewLocation : 1; //includes AI

	uint32 bDesiresTurnInPlace : 1;
protected:
	uint32 bLockDesiredFocalPoint : 1;

public:
	UPROPERTY(Category = "Combat", EditAnywhere, BlueprintReadOnly)
	TMap<FName, FDamageBodies> DamageBodies;

	UPROPERTY(Category = "Combat", EditAnywhere, BlueprintReadOnly)
	TArray<FName> PounceAttachSockets;

	UPROPERTY(Category = "Combat", EditAnywhere, AdvancedDisplay, BlueprintReadOnly)
	bool bSweepComplex;

	UPROPERTY(Category = "Combat", BlueprintReadOnly, Transient)
	bool bDoingDamage;

	// NOTE: This is used to check visibility between the trace that hit something that has core attributes and not used for anything else!!!!!!
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Combat", AdvancedDisplay, meta = (Bitmask, BitmaskEnum = "ECollisionChannel"))
	int32 ObjectTypesToQuery;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	ECheckTraceType CheckTraceType;

	//This needs to be per GA.
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Combat")
	FAINoiseEvent AINoiseSpec;

	FWeaponDamageConfiguration CurrentDamageConfiguration;

public:
	void ProcessCompletedTraceSet(const FQueuedTraceSet& TraceSet);
	bool ProcessDamageResultsArray(const FQueuedTraceItem& Item);

	FGameplayEventData GenerateEventData(const FHitResult& Result, const FWeaponDamageConfiguration& DamageConfig);
	void ProcessDamageEvent(const FGameplayEventData& EventData, const FWeaponDamageConfiguration& DamageConfig);

	bool ProcessDamageHitResult(FHitResult Result, const FQueuedTraceItem& Item);

	UFUNCTION(Client, Reliable)
	void Client_ProcessEvent(const FGameplayEventData& EventData);

	void FilterDamageBodies(const TArray<FName>& EnabledBodies);
	//Probably some include funkiness going on but ECC_WeaponTrace doesn't work here
	UFUNCTION(BlueprintCallable, Category = "Combat")
	virtual void StartDamage(const FWeaponDamageConfiguration& DamageConfig);

	UFUNCTION(BlueprintCallable, Category = "Wa Weapons")
	void SetAllDamageBodiesEnabled(const bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void StopDamageAndClearDamagedActors();

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ClearDamagedActors();

	virtual void EnableDamage(const FWeaponDamageConfiguration& DamageConfiguration);
	virtual void DisableDamage(const bool bDoFinalTrace = true, const bool bResetBodyFilter = true);
	virtual void ReportAINoise(const FVector& Location);

	UFUNCTION(BlueprintNativeEvent, Category = "Combat")
	bool GetDamageBodiesForMesh(const USkeletalMeshComponent* InMesh, TArray<FBodyShapes>& OutDamageBodies);

	UFUNCTION(BlueprintCallable, Category = "Combat")
	FName GetNearestPounceAttachSocket(const FHitResult& HitResult);

protected:

	/**
	* The growth which currently represents morph target, size scaling, skin material, etc.
	* Used for interpolation
	* Negative indicates uninitialized
	*/
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Meta = (ClampMin = 0.f, ClampMax = 1.f))
	float CurrentAppliedGrowth = -1;
	
	void InitializeCombatData();

	int32 GetBodies(const TMap<FName, FDamageBodies>& Bodies, const USkeletalMeshComponent* DamageComp, TArray<FBodyShapes>& OutShapes, TMultiMap<FName, int32>& OutBodyNameLookupMap, const float CharOwnerScale);

	void DoDamageSweeps(const float DeltaTime);

	bool QueueShapeTraces(UWorld* World, const TArray<FTimedTraceBoneGroup> SectionBones, const FCollisionQueryParams& TraceParams, const float CurrentMontageTime, const float DeltaTime, bool bStopOnBlock = true);

	void QueueTraces(const FQueuedTraceSet& QueueSet);

	bool WaPShape2UCollision(FCollisionShape& OutShape, const FPhysicsShapeHandle& ShapeHandle) const;

	FTransform GetShapeLocalTransform(FPhysicsShapeHandle& PXS, const bool bConvertCapsuleRotation = true, const float OwnerCharComponentScale = 1.f);

#if DEBUG_WEAPONS
	void DebugTraceItem(const FQueuedTraceItem& Item);
	void DebugShapes();
	void DebugSingleShape(UWorld* World, physx::PxShape* Shape);
#endif

private:
	TArray<TWeakObjectPtr<AActor>, TInlineAllocator<16>> DamagedActors;
	TMap<TWeakObjectPtr<AActor>, float, TInlineSetAllocator<16>> LastFramesDamagedActors;

	UPROPERTY(Transient)
	TMap<USkeletalMeshComponent*, FBodyShapeSet> DamageShapeMap;

	UPROPERTY()
	int32 TracePointIndex;

	UPROPERTY()
	int32 CurrentTraceGroup;

	FCollisionQueryParams WeaponTraceParams;

	UPROPERTY(Transient)
	TArray<FTimedTraceBoneGroup> CurrentSectionBones;

	UPROPERTY(Transient)
	TArray<FAlderonBoneData> PreviousBoneData;
	
	float LastHitTime;
	int DamageSweepsCounter;
	float CachedBodyOwnerScale;

	int32 TotalSlotCount;
	TArray<FName> SlotKeys;

	UPROPERTY(Transient)
	TArray<AIBaseCharacter*> PushingCharacters;

	friend class UPOTGameplayAbility;

	FActiveGameplayEffectHandle InCombatEffect;

	// IInteractionPromptInterface
public:

	FCollisionQueryParams GetWeaponTraceParams() const
	{
		return WeaponTraceParams;
	}

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Interaction)
	FInteractionPromptData InteractionPromptData;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Interaction)
	FText TrophyInteractionName;

	virtual FText GetInteractionName_Implementation() override;
	virtual TSoftObjectPtr<UTexture2D> GetInteractionImage_Implementation() override;
	virtual FText GetPrimaryInteraction_Implementation(class AIBaseCharacter* User) override;
	virtual FText GetCarryInteraction_Implementation(class AIBaseCharacter* User) override;
	virtual FText GetNoInteractionReason_Implementation(class AIBaseCharacter* User) override;
	virtual int32 GetInteractionPercentage_Implementation() override;
	// ~IInteractionPromptInterface

	UFUNCTION(BlueprintCallable)
	virtual void OnTabStatusUpdate(bool bTabActive) {};

	UFUNCTION(BlueprintCallable, Category = NameTag)
	bool InputToggleNameTags();

	FORCEINLINE bool IsBeingDragged() const { return bIsBeingDragged; }
	
	void SetIsBeingDragged(bool bNewBeingDragged);

protected:

	UPROPERTY(ReplicatedUsing = OnRep_IsBeingDragged)
	bool bIsBeingDragged = false;

public:
	
	UFUNCTION()
	void OnRep_IsBeingDragged();

	UPROPERTY(EditDefaultsOnly)
	TSoftObjectPtr<UTexture2D> DraggedToastIcon;

	UFUNCTION(BlueprintCallable)
	bool ToggleMap();

	UFUNCTION(BlueprintCallable)
	bool InputQuests();

	UFUNCTION(BlueprintCallable)
	void ToggleQuestWindow();

protected:
	void ReInitializePhysicsConstraints();

	UFUNCTION()
	void OnConstraintBrokenWrapperCopy(int32 ConstraintIndex);

	bool IsValidatedCheck();

	virtual void OutsideWorldBounds() override;

	FMapMarkerInfo SurvivalMapMarker = FMapMarkerInfo();
	TArray<FMapMarkerInfo> CritterBurrowMapMarkers;
	TArray<FMapMarkerInfo> QuestMapMarkers;

	FTimerHandle TimerHandle_NearestItem;
	FTimerHandle TimerHandle_NearestBurrow;
public:

	UFUNCTION()
	void StartUpdateNearestItem();

	UFUNCTION()
	void StopUpdateNearestItem();

	bool bHasSurvivalQuest = false;
	UPROPERTY()
	AActor* NearestItem;

	void UpdateNearestItem();
	bool ShouldUpdateNearestItem();

	AActor* FindNearestBurrow(const TArray<FPrimaryAssetId>& CritterIds);

	UFUNCTION()
	void StartUpdateNearestBurrow();

	UFUNCTION()
	void StopUpdateNearestBurrow();

	bool bHasCritterQuest = false;

	void UpdateNearestBurrow();
	bool ShouldUpdateNearestBurrow();

	bool IsNearbyHomecave() { return bHomecaveNearby; }
	void UpdateHomeCaveDistanceTracking(bool bHomecaveInRange);

	void UpdateCampingHomecaveData(bool bWasCampingHomecave);
	bool IsCampingHomecave();
	
	UPROPERTY(SaveGame)
	TArray<FCampingHomecaveData> CampingHomecaveData;

	void OnEnteringHomecave();

protected:

	void ToggleInHomecaveEffect(bool bEnable);
	
	void ToggleHomecaveCampingDebuff(bool bEnable);
	
	void ToggleHomecaveExitbuff(bool bEnable);

	UPROPERTY(BlueprintReadOnly)
	FActiveGameplayEffectHandle GE_InHomecaveEffect;
	
	UPROPERTY(BlueprintReadOnly)
	FActiveGameplayEffectHandle GE_HomecaveCampingDebuff;

	UPROPERTY(BlueprintReadOnly)
	FActiveGameplayEffectHandle GE_HomecaveExitBuff;

	bool bFirstHomecaveCheck = true;
	bool bHomecaveNearby = false;
	
	float HomecaveNearbyInitialTimestamp = -1.0f;

	UPROPERTY(EditDefaultsOnly, Category = Tutorial)
	FPrimaryAssetId HomeCaveTrackingQuest;

	UPROPERTY(EditDefaultsOnly, Category = Tutorial)
	TSoftObjectPtr<UTexture2D> HomeCaveTrackingMapIconSoft;

	FMapMarkerInfo HomeCaveTrackingMarker;

	FTimerHandle TimerHandle_HomeCaveTracking;

	void UpdateHomeCaveTracking();
	
	UPROPERTY()
	UIQuest* CurrentHomeCaveTrackingQuest = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = Tutorial)
	FPrimaryAssetId WaystoneTrackingQuest;

	UPROPERTY(EditDefaultsOnly, Category = Tutorial)
	TSoftObjectPtr<UTexture2D> WaystoneTrackingMapIconSoft;

	FMapMarkerInfo WaystoneTrackingMarker;

	FTimerHandle TimerHandle_WaystoneTracking;

	void UpdateWaystoneTracking();

	UPROPERTY()
	UIQuest* CurrentWaystoneTrackingQuest = nullptr;

public:
	void StartTrackingHomeCave(UIQuest* IQuest);

	UFUNCTION(Client, Reliable, BlueprintCallable, Category = IBaseCharacter)
	void ClientStopTrackingHomeCave();
	void ClientStopTrackingHomeCave_Implementation();

	void StartTrackingWaystone(UIQuest* IQuest);

	UFUNCTION(Client, Reliable, BlueprintCallable, Category = IBaseCharacter)
	void ClientStopTrackingWaystone();
	void ClientStopTrackingWaystone_Implementation();

protected:
	FTimerHandle TimerHandle_ShowSkinAvailableCooldown;

	UPROPERTY()
	TArray<USkinDataAsset*> ShownAvailableSkinAssets;

	bool IsReadyToCheckPurchasableSkin();
	void CheckForPurchasableSkin();
	void CheckForPurchasableSkin_PostCharLoad(bool bSuccess, FPrimaryAssetId CharacterAssetId, UCharacterDataAsset* CharacterDataAsset);
	void CheckForPurchasableSkin_PostSkinsLoad(bool bSuccess, FPrimaryAssetId CharacterAssetId, UCharacterDataAsset* CharacterDataAsset, TArray<FPrimaryAssetId> SkinAssetIds, TArray<USkinDataAsset*> SkinDataAssets, TSharedPtr<FStreamableHandle> Handle);
	void ShowSkinAvailableForPurchaseToast(USkinDataAsset* Skin);

	FTimerHandle TimerHandle_ShowAbilityAvailableCooldown;

	UPROPERTY()
	TArray<UPOTAbilityAsset*> ShownAvailableAbilityAssets;

	TSharedPtr<FStreamableHandle> LoadCharacterDataHandle;

	bool IsReadyToCheckPurchasableAbility();
	void CheckForPurchasableAbility();
	void CheckForPurchasableAbility_PostAbilityLoad(TArray<UPOTAbilityAsset*> Abilities);
	void ShowAbilityAvailableForPurchaseToast(UPOTAbilityAsset* Ability);

//overlap checks
public:
	void StartOverlapChecks();
	void StopOverlapChecks();

protected:
	bool bDoingOverlapChecks;

	//Holds onto actors that have been targetted with the current running aoe ability
	UPROPERTY()
	TArray<AActor*> PreviouslyOverlappedTargetActors;

	void UpdateOverlaps(float DeltaTime);
	void CheckOverlaps(float DeltaTime);

public:
	void SnapToGround();
	
	void CancelWaystoneInviteCharging();

protected:
	UPROPERTY(config)
	float PeriodicUnderWorldCheckSeconds = 0.5f;

	FTimerHandle TimerHandle_UnderWorldCheck;
	TOptional<float> GetSafeHeightIfUnderTheWorld(bool bForce = false) const;
	void PeriodicGroundCheck();

	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFellThroughWorldAdjustment(float RequestedHeight);
	bool ServerFellThroughWorldAdjustment_Validate(float RequestedHeight);
	void ServerFellThroughWorldAdjustment_Implementation(float RequestedHeight);

	UFUNCTION(Client, Reliable)
	void ClientAckFellThroughWorldAdjustment();
	void ClientAckFellThroughWorldAdjustment_Implementation();

	void StartAutoRecording();

	UFUNCTION(Client, Reliable)
	void ClientShowWaystonePrompt();
	void ClientShowWaystonePrompt_Implementation();
	UFUNCTION()
	void WaystonePromptClosed(EMessageBoxResult Result);

	UFUNCTION(Server, Reliable)
	void ServerAcceptPendingWaystoneInvite();
	void ServerAcceptPendingWaystoneInvite_Implementation();

	void WaystoneInviteFullyCharged();
	FTimerHandle TimerHandle_WaystoneCharging;

	UPROPERTY()
	UIMessageBox* WaystonePromptMessageBox;

public:

	FORCEINLINE bool IsWaystoneInProgress() const { return bWaystoneInProgress;	}

	void SetIsWaystoneInProgress(bool bNewIsWaystoneInProgress);
	
protected:

	UPROPERTY(ReplicatedUsing = OnRep_WaystoneInProgress)
	bool bWaystoneInProgress = false;

	UFUNCTION()
	void OnRep_WaystoneInProgress();

	UPROPERTY()
	AActor* WaystoneInProgressFX;

	void WaystoneInProgressFXLoaded();

public:
	virtual bool CanEnterGroup() const
	{
		return true;
	}

	void SetHasLeftHatchlingCave(bool bNewLeftHatchlingCave);
	void SetReadyToLeaveHatchlingCave(bool bReady);
};
