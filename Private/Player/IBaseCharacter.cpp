// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Player/IBaseCharacter.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "GameMode/IGameMode.h"
#include "Components/ICharacterMovementComponent.h"
#include "Weapons/IDamageType.h"
#include "Components/FocalPointComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "IGameUserSettings.h"
#include "Components/IFocusTargetFoliageMeshComponent.h"
#include "IGameInstance.h"
#include "Online/IGameSession.h"
#include "Online/IPlayerState.h"
#include "Online/IGameState.h"
#include "World/ICorpse.h"
#include "AI/Fish/IFish.h"
#include "Runtime/AIModule/Classes/GenericTeamAgentInterface.h"
#include "Quests/IQuest.h"
#include "Quests/IPOI.h"
#include "Engine/ActorChannel.h"
#include "IGameplayStatics.h"
#include "Quests/IQuestManager.h"
#include "IWorldSettings.h"
#include "UI/IChatWindow.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Abilities/CoreAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Stats/Stats.h"
#include "Stats/IStats.h"
#include "Perception/AISense_Hearing.h"
#include "Abilities/POTGameplayAbility.h"
#include "LandscapeStreamingProxy.h"
#include "UI/ICreatorModeReflectionEditor.h"
#include "UI/IMainGameHUDWidget.h"
#include "UI/ITabMenu.h"
#include "Player/AlderonEnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/GameNetworkManager.h"
#include "Net/VoiceConfig.h"
#include "UI/IFoodWaterStatus.h"
#include "Chaos/Sphere.h" 
#include "Chaos/Capsule.h"
#include "Chaos/Box.h"
#include "AlderonReplay.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Abilities/POTGameplayAbility_Pounce.h"
#include "Abilities/POTGameplayAbility_Clamp.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotionConstantForce.h"
#include "Animation/AnimNotify_Footstep.h"

const FQuat Chaos_POTU2PSphylBasis(FVector(1.0 / FMath::Sqrt(2.0), 0.0, 1.0 / FMath::Sqrt(2.0)), PI);

#include "Abilities/WeaponTraceWorker.h"
#include "Engine/StreamableManager.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "UI/IGameHUD.h"
#include "Abilities/POTAbilitySystemGlobals.h"
#include "GameplayEffectTypes.h"
#include "Items/IMeatChunk.h"
#include "Player/IAdminCharacter.h"
#include "UI/IAdminPanelMain.h"
#include "MapRevealerComponent.h"
#include "UI/Map/IFullMapWidget.h"
#include "MapIconComponent.h"
#include "GameplayAbilitySpec.h"
#include "Abilities/POTAbilityAsset.h"
#include "GameFramework/Controller.h"
#include "Components/ISpringArmComponent.h"
#include "Items/IFoodItem.h"
#include "World/IDeliveryPoint.h"
#include "World/IWaterManager.h"
#include "UI/IInteractionMenuWidget.h"
#include "UI/IHomeCaveUnlockWidget.h"
#include "CaveSystem/IHomeCaveDecoratorComponent.h"
#include "CaveSystem/HomeCaveExtensionDataAsset.h"
#include "UI/IHomeCaveCustomiserWidget.h"
#include "UI/IHomeCaveDecorPlacementWidget.h"
#include "UI/IHomeCaveDecorationWidget.h"
#include "UI/ICommonMarks.h"
#include "Online/IPlayerGroupActor.h"
#include "UI/IHomeCaveDecorPurchaseWidget.h"
#include "UI/IHomeCaveRoomPurchaseWidget.h"
#include "UI/IHomeCaveCatalogueWidget.h"
#include "UI/IHomeCaveRoomCatalogueWidget.h"
#include "World/ILevelStreamingLoader.h"
#include "Critters/ICritterPawn.h"
#include "CaveSystem/IHatchlingCave.h"
#include "AlderonRemoteConfig.h"
#include "ITypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Quests/ITutorialManager.h"
#include "UI/ICharacterMenu.h"
#include "UI/IUserWidget.h"
#include "CaveSystem/IHomeCaveManager.h"
#include "UI/IAbilitySlotsEditor.h"
#include "CaveSystem/IHomeRock.h"
#include "Critters/IAlderonCritterBurrowSpawner.h"
#include "Animation/DinosaurAnimBlueprint.h"
#include "Engine/DamageEvents.h"
#include "Items/IQuestItem.h"
#include "UI/IQuestTracking.h"
#include "Online/IReplicationGraph.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/UnrealType.h"
#include "NiagaraComponent.h"
#include "Abilities/POTGameplayAbility_Buck.h"

DEFINE_LOG_CATEGORY_STATIC(LogIBaseCharacter, Log, All);

#if DEBUG_WEAPONS
FAutoConsoleVariable CVarDebugSweeps(
	TEXT("pot.DebugWeaponSwings"),
	0,
	TEXT("Whether or not to show weapon swings.\n")
	TEXT(" 0: off, 1: all sweeps"),
	ECVF_Cheat);

FAutoConsoleVariable CVarDebugWeaponColliders(
	TEXT("pot.DebugWeaponColliders"),
	0,
	TEXT("Whether or not to show weapon colliders during the game.\n")
	TEXT(" 0: off, otherwise on"),
	ECVF_Cheat);

#endif

#if OLD_STATS_AND_ABILITIES
int32 GUseGAS = 0;
FAutoConsoleVariableRef CVarUseGAS(
	TEXT("pot.UseGAS"),
	GUseGAS,
	TEXT("Whether or not to use the new GAS stats for dinosaurs instead of the legacy system.\n")
	TEXT(" 0: off, otherwise on"),
	ECVF_Cheat);
#endif

#define BIND_ABILITY_SLOT_ACTION(ActionName, SlotIndex) BIND_ABILITY_SLOT_ACTION_STR(ActionName, #ActionName, SlotIndex)
#define BIND_ABILITY_SLOT_ACTION_STR(ActionName, ActionNameStr, SlotIndex)													\
FInputActionBinding ActionName##PressedBinding(ActionNameStr, IE_Pressed);													\
FInputActionHandlerSignature ActionName##PressedHandler;																	\
ActionName##PressedHandler.BindUObject(this, &AIBaseCharacter::OnAbilityInputPressed, SlotIndex);							\
ActionName##PressedBinding.ActionDelegate = ActionName##PressedHandler;														\
PlayerInputComponent->AddActionBinding(ActionName##PressedBinding);															\
FInputActionBinding ActionName##ReleasedBinding(ActionNameStr, IE_Released);												\
FInputActionHandlerSignature ActionName##ReleasedHandler;																	\
ActionName##ReleasedHandler.BindUObject(this, &AIBaseCharacter::OnAbilityInputReleased, SlotIndex);							\
ActionName##ReleasedBinding.ActionDelegate = ActionName##ReleasedHandler;													\
PlayerInputComponent->AddActionBinding(ActionName##ReleasedBinding);														\

static const float HatchlingGrowthPercent = 0.25f;
static const float UnclaimedQuestRewardsNoticeTime = 60 * 7;

static const float RepositionIfObstructedDelay = 1.0f;
static const float DelayCollisionAfterUnlatchTime = 0.5f;

static const FQuat RelativeClampedSpringArmRotation = FQuat::MakeFromRotator(FRotator(-45, 90, 0));

AIBaseCharacter::AIBaseCharacter(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UICharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Enable Ticking
	PrimaryActorTick.bCanEverTick = true;

	AbilitySystem = CreateDefaultSubobject<UPOTAbilitySystemComponent>(TEXT("AbilitySystem"));
	AbilitySystem->OnAttackAbilityStart.AddDynamic(this, &AIBaseCharacter::OnAttackAbilityStart);
	AbilitySystem->OnAttackAbilityEnd.AddDynamic(this, &AIBaseCharacter::OnAttackAbilityEnd);

	// Voip
	//VoipAudio = CreateDefaultSubobject<UVoipAudioComponent>(TEXT("VoipAudio"));
	//VoipAudio->SetupAttachment(GetMesh());
	//VoipAudio->bAutoActivate = true;

	// Spring Arm
	ThirdPersonSpringArmComponent = CreateDefaultSubobject<UISpringArmComponent>(TEXT("ThirdPersonSpringArmComp"));
	ThirdPersonSpringArmComponent->SetupAttachment(GetMesh());
	ThirdPersonSpringArmComponent->SetCanEverAffectNavigation(false);

	ThirdPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCameraComp"));
	ThirdPersonCameraComponent->SetupAttachment(ThirdPersonSpringArmComponent);
	ThirdPersonCameraComponent->SetShouldUpdatePhysicsVolume(true);

	// Don't collide with camera checks to keep 3rd person camera at position when dinosaurs or other players are standing behind us
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	// Always Tick Pose / Refresh Bones
	if (USkeletalMeshComponent* SkMesh = Cast<USkeletalMeshComponent>(GetMesh()))
	{
#if !UE_SERVER
		SkMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		SkMesh->bEnableUpdateRateOptimizations = true;
#else
		SkMesh->bEnableUpdateRateOptimizations = false;
		SkMesh->bNoSkeletonUpdate = true;
		SkMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
#endif
	}

	PhysicalAnimation = CreateDefaultSubobject<UPhysicalAnimationComponent>(FName("PhysicalAnimation"));

	PushCapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>("PushCapsuleComponent");
	if (PushCapsuleComponent != nullptr)
	{
		PushCapsuleComponent->SetupAttachment(GetCapsuleComponent());
		PushCapsuleComponent->ComponentTags.Add("IgnoreForMovement");
	}

	BasePushForce = 450.f;

	StaminaJumpDecay = 10;
	bEnableJumpModifier = false;
	StaminaJumpModifier = 1.5; // consecutive jumps multiply jump decay by this amount
	StaminaJumpModifierCooldown = 5.0f; // time it takes to reset the modifier and jump decay

	// New Food Consume System
	FoodConsumedTickRate = 4.0f;

	WaterConsumedTickRate = 3.0f;

	bIsBasicInputDisabled = false;
	bIsAbilityInputDisabled = false;

	// Limping
	bCanEverLimp = true;
	bIsLimping = false;

	// Feign Limping
	bIsFeignLimping = false;

	// Resting
	bCanEverRest = true;

	Stance = EStanceType::Default;

	// Focus
	MaxFocusTargetDistance = 4096.f;
	MinFocusTargetDistance = 200.f;
	FocusTargetRadius = 32.f;
	ClearTargetFocalPointComponentDelay = 0.75f;

	// Object interaction
	bContinousUse = false;

	// Advanced Movement Modes
	bDesiresToSprint = false;
	bDesiresToTrot = true;

	// Camera 
	CameraDistanceRange = FVector2D(512.f, 4096.f);
	CameraDistanceRangeWhileCarried = FVector2D(512.f, 4096.f);
	DesireSpringArmLength = 512.f;
	CameraDollyRate = 512.f;
	CameraDollyTargetAdjustSize = 128.f;
	CameraDistanceToHeadMovementContributionCurve = nullptr;
	CameraRotRelativeFOVRange.X = 85.f;
	CameraRotRelativeFOVRange.Y = 65.f;
	CameraRotRelativeOffsetRange.X = -512;
	CameraRotRelativeOffsetRange.Y = 512.f;
	CameraRotRelativeSpeedRange.X = -12024.f;
	CameraRotRelativeSpeedRange.Y = 12024.f;
	SpeedBasedCameraEffectLerpRate = 0.3f;
	SpringArmRotateRate = 800.f;

	// AI
	bIsAIControlled = false;

	TeamId = FGenericTeamId(AITeamId);

	// Auto Run
	bAutoRunActive = false;

	// Combat Log AI
	bCombatLogAI = false;

	// Adjust jump to make it less floaty
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->GravityScale = 1.5f;
	MoveComp->JumpZVelocity = 620;
	MoveComp->bCanWalkOffLedgesWhenCrouching = true;
	MoveComp->MaxWalkSpeedCrouched = 200;

	MaxAimBehindYaw = 170.f;

	AICapsuleScale = 1.0f;

	CurrentlyUsedObject.Object = nullptr;
	CurrentlyUsedObject.Item = INDEX_NONE;

	FocusedObject.Object = nullptr;
	FocusedObject.Item = INDEX_NONE;

	// AI Behavior / Threat Levels
	AIBehavior = EAIBehavior::NONE;
	AIGroupSoloChance = EAIGroupingScale::NEVER;
	AIGroupMateChance = EAIGroupingScale::NEVER;
	AIGroupPackChance = EAIGroupingScale::NEVER;
	AIThreatLevel = 0;
	AIAvoidanceThreatLevel = 0;
	AIFleeThreatLevel = 0;
	AIMaxPackSize = 0;
	AIRelevantRange = 9000.0;

	// AI Sight / Hearing / Sense
	AIHearingThreshold = 2800.0;		// 1400.0
	AILOSHearingThreshold = 5600.0;		// 2800.0
	AISightRadius = 9000.0;				// 5000.0
	AISensingInterval = 0.5;			// 0.5
	AIHearingMaxSoundAge = 1.0;			// 1.0
	AIPeripheralVisionAngle = 120.0;	// 90.0

	CarryData.AttachLocationOffset = FVector(0, 0, 0);
	CarryData.AttachRotationOffset = FRotator(0, 90, 0);

	bDiedByKillZ = false;
}

#if !UE_SERVER
static void WriteRawToTexture_RenderThread(FTexture2DDynamicResource* TextureResource, TArray64<uint8>* RawData, bool bUseSRGB = true)
{
	check(IsInRenderingThread());

	if (TextureResource)
	{
		FRHITexture2D* TextureRHI = TextureResource->GetTexture2DRHI();

		int32 Width = TextureRHI->GetSizeX();
		int32 Height = TextureRHI->GetSizeY();

		uint32 DestStride = 0;
		uint8* DestData = reinterpret_cast<uint8*>(RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false, false));

		for (int32 y = 0; y < Height; y++)
		{
			uint8* DestPtr = &DestData[((int64)Height - 1 - y) * DestStride];

			const FColor* SrcPtr = &((FColor*)(RawData->GetData()))[((int64)Height - 1 - y) * Width];
			for (int32 x = 0; x < Width; x++)
			{
				*DestPtr++ = SrcPtr->B;
				*DestPtr++ = SrcPtr->G;
				*DestPtr++ = SrcPtr->R;
				*DestPtr++ = SrcPtr->A;
				SrcPtr++;
			}
		}

		RHIUnlockTexture2D(TextureRHI, 0, false, false);
	}

	delete RawData;
}

#endif

void AIBaseCharacter::CacheBloodMaskPixels()
{
	if (!BloodMaskSource)
	{
		//UE_LOG(LogTemp, Warning, TEXT("AIBaseCharacter::CacheBloodMaskPixels: BloodMaskSource is nullptr"));
		return;
	}

	if (BloodMaskSource->GetPixelFormat() != EPixelFormat::PF_B8G8R8A8)
	{
		//UE_LOG(LogTemp, Warning, TEXT("AIBaseCharacter::CacheBloodMaskPixels: BloodMaskSource is invalid pixel format: Expected PF_B8G8R8A8"));
		return;
	}

	// Read source data
	FTexture2DMipMap* SourceMipMap = &BloodMaskSource->GetPlatformData()->Mips[0];
	FByteBulkData* RawImageData = &SourceMipMap->BulkData;
	uint8* SourcePixels = (uint8*)RawImageData->Lock(LOCK_READ_WRITE);

	if (!SourcePixels)
	{
		UE_LOG(LogTemp, Warning, TEXT("AIBaseCharacter::CacheBloodMaskPixels: SourcePixels is nullptr"));
		return;
	}

	BloodMaskPixelData.TextureSize.X = SourceMipMap->SizeX;
	BloodMaskPixelData.TextureSize.Y = SourceMipMap->SizeY;

	//uint8* Pixels = GeneratePixels(TextureHeight, TextureWidth);
	//Each element of the array contains a single color so in order to represent information in
	//RGBA we need to have an array of length TextureWidth * TextureHeight * 4


	int TotalImageLength = BloodMaskPixelData.TextureSize.X * BloodMaskPixelData.TextureSize.Y * 4;
	BloodMaskPixelData.Pixels.Reset(TotalImageLength);
	BloodMaskPixelData.Pixels.AddZeroed(TotalImageLength);

	//uint8* PixelArrayStart = BloodMaskPixelData.Pixels.GetData();

	for (int32 y = 0; y < BloodMaskPixelData.TextureSize.Y; y++)
	{
		for (int32 x = 0; x < BloodMaskPixelData.TextureSize.X; x++)
		{
			// Get the current pixel
			int32 CurrentPixelIndex = ((y * BloodMaskPixelData.TextureSize.X) + x);

			
			BloodMaskPixelData.Pixels[4 * CurrentPixelIndex] = SourcePixels[4 * CurrentPixelIndex]; // B
			BloodMaskPixelData.Pixels[4 * CurrentPixelIndex + 1] = SourcePixels[4 * CurrentPixelIndex + 1]; // G
			BloodMaskPixelData.Pixels[4 * CurrentPixelIndex + 2] = SourcePixels[4 * CurrentPixelIndex + 2]; // R
			BloodMaskPixelData.Pixels[4 * CurrentPixelIndex + 3] = SourcePixels[4 * CurrentPixelIndex + 3]; // A
		}
	}

	

	BloodMaskSource->GetPlatformData()->Mips[0].BulkData.Unlock();
}

void AIBaseCharacter::UpdateBloodMask(bool bForceUpdate)
{
#if !UE_SERVER
	if (IsRunningDedicatedServer() || !bWoundsEnabled)
	{
		return;
	}

	/*
		https://isaratech.com/ue4-reading-the-pixels-from-a-utexture2d/
		CompressionSettings set to VectorDisplacementmap
		MipGenSettings to NoMipmaps
		SRGB to false
	*/



	if (BloodMaskPixelData.TextureSize.X == 0 || BloodMaskPixelData.TextureSize.Y == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("AIBaseCharacter::UpdateBloodMask: Invalid TextureSize"));
		return;
	}

	const bool bIsAlive = IsAlive();
	const float HealthRatio = GetHealth() / GetMaxHealth();

	bool bHasAnyChange = false;

	uint8 HealthBasedWoundOpacity = 0;
	if (TotalDamageMaxOpacity > 0.f && TotalDamageOpacityHealthThreshold > 0.f)
	{
		const float Alpha = 1.f - FMath::Clamp(HealthRatio / TotalDamageOpacityHealthThreshold, 0.f, 1.f);
		const uint8 TotalDamageOpacity = (uint8)(255.0f * Alpha * TotalDamageMaxOpacity);
		HealthBasedWoundOpacity = FMath::Clamp<uint8>(TotalDamageOpacity, 0, 255);
	}

	for (FCachedWoundDamage& CachedWoundValue : CachedWoundValues)
	{
		const float CategoryDamage = GetDamageWounds().GetDamageForCategory(CachedWoundValue.Category);
		const float NormalizedDamage = UKismetMathLibrary::MapRangeClamped(CategoryDamage, NormalizedWoundsDamageRange.X, NormalizedWoundsDamageRange.Y, 0.0f, 1.0f);
		const uint8 DamageOpacity = bIsAlive ? (uint8)(255 * NormalizedDamage) : 255;
		const uint8 PermaDamageOpacity = (uint8)(255 * FMath::Lerp(NormalizedPermaWoundsOpacityRange.X, NormalizedPermaWoundsOpacityRange.Y, GetDamageWounds().GetPermaDamageForCategory(CachedWoundValue.Category)));
		const uint8 NewOpacity = FMath::Max<uint8>({DamageOpacity, PermaDamageOpacity, HealthBasedWoundOpacity});

		if (CachedWoundValue.CachedOpacity == NewOpacity)
		{
			continue;
		}

		CachedWoundValue.CachedOpacity = NewOpacity;
		bHasAnyChange = true;
	}

	if (!bHasAnyChange && !bForceUpdate)
	{
		// We won't change any pixels, so exit early.
		return;
	}

	auto GetWoundOpacity = [&](uint8 B, uint8 G, uint8 R) -> uint8
	{
		for (const FCachedWoundDamage& CachedWoundValue : CachedWoundValues)
		{
			const FColor& SourceColor = CachedWoundValue.Color;
			if (SourceColor.B != B || SourceColor.G != G || SourceColor.R != R)
			{
				continue;
			}

			return CachedWoundValue.CachedOpacity;		
		}

		return 0;
	};	

	FTexture2DDynamicCreateInfo CreateInfo{};
	CreateInfo.bSRGB = false;
	CreateInfo.Format = PF_B8G8R8A8;

	UTexture2DDynamic* const NewTexture = UTexture2DDynamic::Create(BloodMaskPixelData.TextureSize.X, BloodMaskPixelData.TextureSize.Y, CreateInfo);
	if (!NewTexture)
	{
		UE_LOG(LogTemp, Error, TEXT("AIBaseCharacter::UpdateBloodMask: Failed to create texture."));
		return;
	}

	TArray64<uint8>* Pixels = new TArray64<uint8>();

	Pixels->Reserve(BloodMaskPixelData.Pixels.Num());
	uint8* const PixelArrayStart = Pixels->GetData();

	for (int32 y = 0; y < BloodMaskPixelData.TextureSize.Y; y++)
	{
		for (int32 x = 0; x < BloodMaskPixelData.TextureSize.X; x++)
		{
			// Get the current pixel
			const int32 CurrentPixelIndex = ((y * BloodMaskPixelData.TextureSize.X) + x) * 4;

			const uint8 SourceB = BloodMaskPixelData.Pixels[CurrentPixelIndex];
			const uint8 SourceG = BloodMaskPixelData.Pixels[CurrentPixelIndex + 1];
			const uint8 SourceR = BloodMaskPixelData.Pixels[CurrentPixelIndex + 2];

			const uint8 FinalOpacity = GetWoundOpacity(SourceB, SourceG, SourceR);
					
			PixelArrayStart[CurrentPixelIndex] = FinalOpacity; // B
			PixelArrayStart[CurrentPixelIndex + 1] = FinalOpacity; // G
			PixelArrayStart[CurrentPixelIndex + 2] = FinalOpacity; // R
			PixelArrayStart[CurrentPixelIndex + 3] = 255; // A
		}
	}

	

	NewTexture->CompressionSettings = TC_VectorDisplacementmap;
	NewTexture->UpdateResource();

	ENQUEUE_RENDER_COMMAND(FWriteRawDataToTexture)(
	[NewTexture, Pixels](FRHICommandListImmediate& RHICmdList)
	{
		if (NewTexture)
		{
			FTexture2DDynamicResource* TextureResource = static_cast<FTexture2DDynamicResource*>(NewTexture->GetResource());
			if (TextureResource)
			{
				WriteRawToTexture_RenderThread(TextureResource, Pixels);
			}
			else
			{
				delete Pixels;
			}
		}
		else
		{
			delete Pixels;
		}
	});


	// Swap textures out
	UTexture2DDynamic* OldTexture = BloodMask;
	BloodMask = NewTexture;

	UpdateWoundsTextures();

	if (OldTexture)
	{
		OldTexture->ConditionalBeginDestroy();
		OldTexture = nullptr;
	}

#endif
}

FPackedWoundInfo& AIBaseCharacter::GetDamageWounds_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, DamageWounds, this);
	return DamageWounds;
}

void AIBaseCharacter::SetDamageWounds(const FPackedWoundInfo& NewDamageWounds)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, DamageWounds, NewDamageWounds, this);
}

void AIBaseCharacter::OnRep_DamageWounds()
{
	if (IsRunningDedicatedServer() || !bWoundsEnabled)
	{
		return;
	}

	GetDamageWounds_Mutable().CalculateTotal();

	const float InstantUpdateMaskThreshold = 0.05f;

	if (FMath::Abs(GetDamageWounds().TotalWoundDamage - LastWoundUpdateAmount) >= InstantUpdateMaskThreshold)
	{
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::UpdateBloodMask, false);
		GetWorldTimerManager().SetTimerForNextTick(Del);
		bDamageWoundsDirty = false;
	}
	else
	{
		bDamageWoundsDirty = true;
	}

	LastWoundUpdateAmount = GetDamageWounds().TotalWoundDamage;
}

void AIBaseCharacter::OnUpdateDamageWoundsTimer()
{
	if (!bDamageWoundsDirty)
	{
		return;
	}

	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::UpdateBloodMask, false);
	GetWorldTimerManager().SetTimerForNextTick(Del);
	bDamageWoundsDirty = false;
}

void AIBaseCharacter::TestWoundDamage(EDamageWoundCategory Category, float InNormalizedDamage)
{
	float TotalNormalizedDamage = GetDamageWounds_Mutable().AddDamageToCategory(Category, InNormalizedDamage);

	float NormalizedPermaWoundDamage = FMath::Clamp((TotalNormalizedDamage - NormalizedPermaWoundsDamageRange.X) /
		(NormalizedPermaWoundsDamageRange.Y - NormalizedPermaWoundsDamageRange.X), 0.f, 1.f);

	if (NormalizedPermaWoundDamage > GetDamageWounds().GetPermaDamageForCategory(Category))
	{
		GetDamageWounds_Mutable().SetPermaDamageForCategory(Category, NormalizedPermaWoundDamage);
	}
}

float AIBaseCharacter::GetWoundDamage(EDamageWoundCategory Category, bool bPermanent) const
{
	return bPermanent ? GetDamageWounds().GetPermaDamageForCategory(Category) : GetDamageWounds().GetDamageForCategory(Category);
}

void AIBaseCharacter::OnRep_CombatLogAi()
{
}

void AIBaseCharacter::SetCombatLogAlderonId(FAlderonPlayerID NewCombatLogAlderonId)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, CombatLogAlderonId, NewCombatLogAlderonId, this);
}

void AIBaseCharacter::SetKillerCharacterId(const FAlderonUID& NewKillerCharacterId)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, KillerCharacterId, NewKillerCharacterId, this);
}

void AIBaseCharacter::SetKillerAlderonId(FAlderonPlayerID NewKillerAlderonId)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, KillerAlderonId, NewKillerAlderonId, this);
}

// Called by the server when enabling the Combat Log AI
void AIBaseCharacter::SetCombatLogAI(bool bEnabled)
{
	// Authority Only
	check(HasAuthority());
	check(IsCombatLogAI() != bEnabled);

	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bCombatLogAI, bEnabled, this);

	if (bEnabled)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_CombatLogAI, this, &AIBaseCharacter::ExpiredCombatLogAI, GetCombatLogDuration(), false);
	}
	else
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_CombatLogAI);
	}
}

float AIBaseCharacter::GetCombatLogDuration()
{
	const float CombatLogDuration = 120.0f; // 2 mins
	return CombatLogDuration;
}

float AIBaseCharacter::GetRevengeKillDuration()
{
	const float RevengeKillDuration = 600.0f; // 10 mins
	return RevengeKillDuration;
}

void AIBaseCharacter::ExpiredCombatLogAI()
{
	AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
	
	// Timer should only be called on Combat Log AIs
	check(IsCombatLogAI());

	if (IGameMode && IsCombatLogAI())
	{
		bool bDestroy = IsAlive();
		bool bRemoveTimestamp = false;

		AIPlayerController* IPlayerController = Cast<AIPlayerController>(GetController());
		check(!IPlayerController);
		if (IPlayerController)
		{
			bDestroy = false;
			UE_LOG(LogTemp, Error, TEXT("AIBaseCharacter::ExpiredCombatLogAI: Combat Log AI has a player controller on it for characterId: %s"), *GetCharacterID().ToString());
		}

		const bool bSaveCombatLogAI = true;
		IGameMode->RemoveCombatLogAI(GetCharacterID(), bDestroy, bRemoveTimestamp, bSaveCombatLogAI);
	}
}

void AIBaseCharacter::SetTempBodyCleanup()
{
	check(HasAuthority());
	const float MaxMinutes = 30.0f;
	const float MaxSeconds = MaxMinutes * 60.0f;
	SetLifeSpan(MaxSeconds);
}

bool AIBaseCharacter::IsPostLoadThreadSafe() const { return true; }

void AIBaseCharacter::SetIsPreviewCharacter(bool bRequiresLevelLoad)
{
	bIsCharacterEditorPreviewCharacter = true;
	if (bRequiresLevelLoad)
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);
		IGameInstance->RequestWaitForLevelLoading();

		SetPreloadingClientArea(true);
	}
}

void AIBaseCharacter::DatabasePreSave_Implementation() { }

void AIBaseCharacter::DatabasePostLoad_Implementation()
{
	RestoreHealth(0);
	RestoreHunger(0);
	RestoreThirst(0);
	RestoreStamina(0);
	RestoreOxygen(0);
	UpdateLimpingState();
}

void AIBaseCharacter::DatabasePostSave_Implementation()
{
}

FText AIBaseCharacter::GetCurrentLocationText()
{
	return FText::GetEmpty();
}

FGenericTeamId AIBaseCharacter::GetGenericTeamId() const
{
	return TeamId;
}

// Creates output variables for Get Head Location and Rotation Function in BP
void AIBaseCharacter::GetActorEyesViewPoint(FVector &Location, FRotator &Rotation) const
{
	GetHeadLocRot(Location, Rotation);
}

// Creates and Sets default values for Location and Rotation output for character head
void AIBaseCharacter::GetHeadLocRot_Implementation(FVector &OutLocation, FRotator &OutRotation) const
{
	OutLocation = GetActorLocation() + FVector(0, 0, 50);
	OutRotation = GetActorRotation();
}

void AIBaseCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (USkeletalMeshComponent* Mesh3P = GetMesh())
		{
			if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
			{
				Mesh3P->bEnableUpdateRateOptimizations = !IGameUserSettings->bHighQualityAnimationsAtDistance;
			}
			
			if (ThirdPersonSpringArmComponent)
			{
				if (bEnableNewCameraSystem)
				{
					ThirdPersonSpringArmComponent->SetAbsolute(false, false, true);
				}

				// ensure camera distance range makes sense
				if (CameraDistanceRange.X > CameraDistanceRange.Y)
				{
					FVector2D(512.f, 4096.f);
				}

				if (ThirdPersonSpringArmComponent->TargetArmLength < CameraDistanceRange.X || ThirdPersonSpringArmComponent->TargetArmLength > CameraDistanceRange.Y)
				{
					ThirdPersonSpringArmComponent->TargetArmLength = CameraDistanceRange.X;
				}

				DesireSpringArmLength = ThirdPersonSpringArmComponent->TargetArmLength;
			}
		}
	}
#else
	if (IsRunningDedicatedServer())
	{
		UIGameInstance* IGameInstance = Cast<UIGameInstance>(GetGameInstance());
		check(IGameInstance);

		AIGameSession* Session = Cast<AIGameSession>(IGameInstance->GetGameSession());

		if (Session != nullptr && Session->bServerExperimentalOptimizations)
		{
			if (GetCharacterMovement()) GetCharacterMovement()->SetComponentTickInterval(0.0333f);
			if (GetMesh()) GetMesh()->SetComponentTickInterval(0.0333f);
			if (AbilitySystem) AbilitySystem->SetComponentTickInterval(0.0333f);
			SetActorTickInterval(0.0333f);

			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::PostInitializeComponents() - ExperimentalOptimizations Enabled!"));
		}
			
		if (PhysicalAnimation) PhysicalAnimation->DestroyComponent();
		if (ThirdPersonCameraComponent) ThirdPersonCameraComponent->DestroyComponent();
		if (ThirdPersonSpringArmComponent) ThirdPersonSpringArmComponent->DestroyComponent();
		if (FoodSkeletalMeshComponent) FoodSkeletalMeshComponent->DestroyComponent();
		if (FoodStaticMeshComponent) FoodStaticMeshComponent->DestroyComponent();
	}
#endif
}

void AIBaseCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	
	if (AbilitySystem)
	{
		// Mutex Lock to avoid ability system being applied several times over as combat log ai characters are reused
		if (!bAbilitySystemSetup)
		{
			bAbilitySystemSetup = true;

			const int32 CurrentCustomizationEnumValue = static_cast<int32>(ActionBarCustomizationVersion);
			const int32 ReferenceCustomizationEnumValue = static_cast<int32>(EActionBarCustomizationVersion::MAX) - 1;
			if (CurrentCustomizationEnumValue != ReferenceCustomizationEnumValue)
			{
				//SlottedAbilityIds = GetClass()->GetDefaultObject<AIBaseCharacter>()->SlottedAbilityIds;
				GetSlottedAbilityCategories_Mutable() = GetClass()->GetDefaultObject<AIBaseCharacter>()->GetSlottedAbilityCategories();
				GetSlottedAbilityAssetsArray_Mutable() = GetClass()->GetDefaultObject<AIBaseCharacter>()->GetSlottedAbilityAssetsArray();
			}

			AbilitySystem->InitializeAbilitySystem(NewController, this);
			InitializeCombatData();
			AbilitySystem->RefreshAbilityActorInfo();
		}
	}

	if (IsValid(NewController))
	{
		SetIsAIControlled(!NewController->IsA<APlayerController>());
		if (IsRunningDedicatedServer())
		{
			if (IsCampingHomecave() && !GetCurrentInstance())
			{
				bHomecaveNearby = true;
				ToggleHomecaveCampingDebuff(true);
			}

			if (IsHomecaveCampingDebuffActive() && !UIGameplayStatics::AreHomeCavesEnabled(this))
			{
				ToggleHomecaveCampingDebuff(false);
			}
		}
	}

	OnRep_IsAIControlled();


	if (HasAuthority())
	{
		if (!IsRunningDedicatedServer())
		{
			OnRep_Controller();
		}

		if (AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>())
		{
			IPlayerState->SetMarksTemp(GetMarks());
		}

		LastPossessedController = NewController;

		SetGodmode(false);

		AIPlayerController* IPlayerController = Cast<AIPlayerController>(NewController);
		if (IPlayerController)
		{
			IPlayerController->RemoveWaystoneInviteEffect(false);
			IPlayerController->RemoveWaystoneInviteEffect(true);
		}
	}

	//Updates the physics volume.
	OnPhysicsVolumeChanged(GetPhysicsVolume());
}

void AIBaseCharacter::UnPossessed()
{
	UPOTAbilitySystemComponent* POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(AbilitySystem);
	if (POTAbilitySystemComponent)
	{
		POTAbilitySystemComponent->RemoveAllBuffs();
	}

	AIPlayerState* IPlayerState = Cast<AIPlayerState>(GetPlayerState());
	//check(IPlayerState);
	if (IPlayerState)
	{
		// We need to set the iplayerstate currentinstance to nullptr when unpossessing or else it will effect other characters
		// when re-joining as a new character
		IPlayerState->SetCachedCurrentInstance(nullptr);
	}

	// Player Character has been unpossessed, stop all movement to fix issues with dinosaurs
	// still moving when someone quits the game.
	if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		//ICharMove->StopMovementImmediately();
	}

	if (HasAuthority())
	{
		if (IsLatched())
		{
			// If we are latched to a dino, unlatch.
			ClearAttachTarget();
		}
		if (AIBaseCharacter* CarryTarget = GetCarriedCharacter())
		{
			// If we are carrying a dino, release.
			CarryTarget->ClearAttachTarget();
		}
	}

#if !UE_SERVER
	if (IsLocallyControlled() && HasUsableFocus())
	{
		SetFocusedObjectAndItem(nullptr, INDEX_NONE, true);
	}
#endif

	Super::UnPossessed();
}

void AIBaseCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();

	APlayerController* PC = Cast<APlayerController>(Controller);
	if (PC && PC->IsLocalController())
	{
		HighlightFocusedObject();
		NotifyAbilitiesNotMeetingGrowthRequirement();
	}

#if !UE_SERVER
 	if (!IsRunningDedicatedServer())
	{
		if (IsLocallyControlled())
		{
			AIPlayerController* IPlayerController = Cast<AIPlayerController>(PC);
			if (IPlayerController)
			{
				IPlayerController->ClientClearPreviousQuestMarkers();
			}

			UpdateQuestMarker();

			RefreshMapIcon();

			if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
			{
				if (UIMainGameHUDWidget* IMainGameHUDWidget = Cast<UIMainGameHUDWidget>(IGameHUD->ActiveHUDWidget))
				{
					IMainGameHUDWidget->RefreshMiniMapVisibility();
					IMainGameHUDWidget->FoodWaterStatus->UpdateGrowth();
					IMainGameHUDWidget->UpdateOnPawnPossessed(this);
				}

				if (IsWellRested())
				{
					IGameHUD->ShowHomecaveWellRestedNotice();
				}
			}

			RefreshInstanceHiddenActors();
			
			if (UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this))
			{
				IGameInstance->SetLevelStreamingEnabled(true);
			}

			OnRep_ActiveQuests();

			if (PeriodicUnderWorldCheckSeconds > 0)
			{
				// Periodically check if we are under the world
				FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::PeriodicGroundCheck);
				GetWorldTimerManager().SetTimer(TimerHandle_UnderWorldCheck, Del, PeriodicUnderWorldCheckSeconds, false);
			}

			bAwaitingCaveResponse = false;
		}
	}
#endif
}


void AIBaseCharacter::HandleDamage(float DamageDone, const FHitResult& HitResult, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags, class AController* EventInstigator, class AActor* DamageCauser)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::HandleDamage - DamageDone: %f, %s"), DamageDone, *HitResult.BoneName.ToString()));
	bool bForceAbilityCancel = false;

	FPOTDamageInfo DamageInfo;
	const FGameplayEffectContextHandle& ContextHandle = Spec.GetEffectContext();
	if (ContextHandle.IsValid())
	{
		const FPOTGameplayEffectContext* POTEffectContext = StaticCast<const FPOTGameplayEffectContext*>(ContextHandle.Get());
		DamageInfo = POTEffectContext->GetDamageInfo();
	}

	if (AbilitySystem && !bIsDying)
	{
		if (DamageDone > 0)
		{
			switch (DamageInfo.DamageType)
			{
			case EDamageType::DT_ATTACK:
			case EDamageType::DT_BREAKLEGS:
			case EDamageType::DT_GENERIC:
			case EDamageType::DT_TRAMPLE:
			case EDamageType::DT_SPIKES:
				APawn* Pawn = EventInstigator ? EventInstigator->GetPawn() : nullptr;
				PlayHit(DamageDone, DamageInfo, Pawn, DamageCauser, !AbilitySystem->IsAlive(), HitResult.Location, GetActorRotation(), HitResult.BoneName);
				break;
			}
		}
		//This is damage from another player
		if (EventInstigator != nullptr && EventInstigator->GetPawn() != nullptr && EventInstigator->GetPawn() != this)
		{
			UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
			if (PAGS.InCombatEffect != nullptr)
			{
				UGameplayEffect* InCombatGameplayEffect = PAGS.InCombatEffect->GetDefaultObject<UGameplayEffect>();
				
				// Apply combat effect to you and your gruop
				InCombatEffect = AbilitySystem->ApplyGameplayEffectToSelf(InCombatGameplayEffect, 1.f, AbilitySystem->MakeEffectContext());
				ApplyCombatTimerToPlayersGroup(this);
				// Apply Combat Status Effect to person who damaged you and their group
				AIBaseCharacter* IDamageCauser = Cast<AIBaseCharacter>(DamageCauser);
				if (IDamageCauser)
				{
					UPOTAbilitySystemComponent* DamageCauserAbilityComponent = Cast<UPOTAbilitySystemComponent>(IDamageCauser->GetAbilitySystemComponent());
					if (DamageCauserAbilityComponent && DamageCauserAbilityComponent->IsAlive())
					{
						IDamageCauser->InCombatEffect = DamageCauserAbilityComponent->ApplyGameplayEffectToSelf(InCombatGameplayEffect, 1.f, DamageCauserAbilityComponent->MakeEffectContext());
						if (UPOTGameplayAbility* CurrentAbility = DamageCauserAbilityComponent->GetCurrentAttackAbility())
						{
							if (CurrentAbility->bCanForceAbilityInterrupt)
							{
								bForceAbilityCancel = true;
							}
						}
					}

					ApplyCombatTimerToPlayersGroup(IDamageCauser);
				}
			}
		}

		float MaxHP = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxHealthAttribute());
		
		if (OnTakeDamage.IsBound())
		{
			OnTakeDamage.Broadcast(DamageDone, DamageDone / MaxHP, HitResult, Spec, SourceTags);
		}

		if (HasAuthority())
		{
			Client_BroadcastOnTakeDamageDelegate(DamageDone, DamageDone / MaxHP, HitResult, Spec, SourceTags);
			if (IsWaystoneInProgress())
			{
				CancelWaystoneInviteCharging();
			}
		}

		static FGameplayTag AmbientDamageGameplayCue = FGameplayTag::RequestGameplayTag(NAME_GameplayCueAmbientSoundCombatEvent);
		FGameplayCueParameters AmbientDamageParameters = FGameplayCueParameters(Spec.GetContext());
		AbilitySystem->ExecuteGameplayCue(AmbientDamageGameplayCue,AmbientDamageParameters);
	}
	//UE_LOG(LogTemp, Log, TEXT("Damage: %f New Health: %f"), DamageDone, AbilitySystem->GetNumericAttributeBase(UCoreAttributeSet::GetHealthAttribute()));

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	IGameInstance->AddReplayEvent(EReplayHighlight::EnteringCombat);

	if (bForceAbilityCancel)
	{
		AbilitySystem->CancelCurrentAttackAbility();
	}
}

void AIBaseCharacter::ApplyCombatTimerToPlayersGroup(const AIBaseCharacter* Player) const
{
	//check group combat timer server setting
	const AIGameState* const IGameState = UIGameplayStatics::GetIGameState(this);
	if (IGameState == nullptr || !(IGameState->GetGameStateFlags().bCombatTimerAppliesToGroup))
	{
		return;
	}

	const AIPlayerState* IPlayerState = Player->GetPlayerState<AIPlayerState>();
	if (IPlayerState == nullptr || IPlayerState->GetPlayerGroupActor() == nullptr)
	{
		return;
	}

	UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
	if (PAGS.InCombatEffect == nullptr)
	{
		return;
	}

	UGameplayEffect* InCombatGameplayEffect = PAGS.InCombatEffect->GetDefaultObject<UGameplayEffect>();

	for (const AIPlayerState* const OtherPlayerState : IPlayerState->GetPlayerGroupActor()->GetGroupMembers())
	{
		if (!OtherPlayerState)
		{
			continue;
		}


		AIBaseCharacter* OtherPlayerBase = Cast<AIBaseCharacter>(OtherPlayerState->GetPawn());
		if (OtherPlayerBase == nullptr || OtherPlayerBase == Player)
		{
			continue;
		}

		UPOTAbilitySystemComponent* DamageCauserAbilityComponent = Cast<UPOTAbilitySystemComponent>(OtherPlayerBase->GetAbilitySystemComponent());
		if (DamageCauserAbilityComponent && DamageCauserAbilityComponent->IsAlive())
		{
			OtherPlayerBase->InCombatEffect = DamageCauserAbilityComponent->ApplyGameplayEffectToSelf(InCombatGameplayEffect, 1.f, DamageCauserAbilityComponent->MakeEffectContext());
		}
	}
}

void AIBaseCharacter::Client_BroadcastStatus_Implementation(EDamageEffectType DamageEffectType)
{
	OnStatusStart.Broadcast(DamageEffectType);
}

void AIBaseCharacter::DelayedServerNotifyPOIOverlap(AIPOI* POI)
{
	if (!POI || !GetWorld())
	{
		return;
	}

	FTimerManager& WorldTimerManager = GetWorldTimerManager();
	if (WorldTimerManager.TimerExists(TimerHandle_DelayedServerNotifyPOIOverlap))
	{
		WorldTimerManager.ClearTimer(TimerHandle_DelayedServerNotifyPOIOverlap);
	}

	const FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &ThisClass::ServerNotifyPOIOverlap, POI);
	constexpr bool bLoopingTimer = false;
	WorldTimerManager.SetTimer(TimerHandle_DelayedServerNotifyPOIOverlap, Del, POIOverlapDelaySeconds, bLoopingTimer);
}

void AIBaseCharacter::ServerNotifyPOIOverlap(AIPOI* POI)
{
	if (!POI || !IsAlive())
	{
		return;
	}

	if (AIPlayerController* const IPlayerController = Cast<AIPlayerController>(GetController()))
	{
		IPlayerController->ServerNotifyPOIOverlap(true, POI);
	}
}

void AIBaseCharacter::HandleHealthChange(float Delta, const FHitResult& HitResult, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags,
	class AController* EventInstigator, class AActor* DamageCauser)
{
	if (HasAuthority() && EventInstigator)
	{
		if (EventInstigator != GetController())
		{
			LastDamageInstigator = EventInstigator;
		}
		const AIPlayerState* const InstigatorPlayerState = EventInstigator->GetPlayerState<AIPlayerState>();
		AIPlayerController* const IPlayerController = Cast<AIPlayerController>(GetController());
		
		if (InstigatorPlayerState && InstigatorPlayerState != GetPlayerState() && IPlayerController)
		{
			FReportPlayerInfo& ReportInfo = IPlayerController->GetReportInfo_Mutable();
			ReportInfo.ReportedPlayerName = FName(*InstigatorPlayerState->GetPlayerName());
			ReportInfo.ReportedPlayerPlatform = InstigatorPlayerState->GetPlatform();
			ReportInfo.LastDamageCauserID = InstigatorPlayerState->GetUniquePlayerId();
			ReportInfo.RecentDamageCauserIDs.AddUnique(InstigatorPlayerState->GetAlderonID());
		}
	}

	if (GetHealth() <= 0.f && Delta > 0.f)
	{
		return;
	}

	EDamageType DType = EDamageType::DT_GENERIC;
	FPOTDamageInfo DamageInfo{};
	const FGameplayEffectContextHandle& ContextHandle = Spec.GetEffectContext();
	if (ContextHandle.IsValid())
	{
		const FPOTGameplayEffectContext* POTEffectContext = StaticCast<const FPOTGameplayEffectContext*>(ContextHandle.Get());
		DType = POTEffectContext->GetDamageType();
		DamageInfo = POTEffectContext->GetDamageInfo();
	}

	//This ensures that the proper combat penalty is applied below even if the player opted to jump off a cliff to avoid the penalty.
	if (DType != EDamageType::DT_ATTACK && DType != EDamageType::DT_BLEED && DType != EDamageType::DT_TRAMPLE && DType != EDamageType::DT_SPIKES  && IsInCombat())
	{
		DType = EDamageType::DT_ATTACK;
	}

	if (bWoundsEnabled)
	{
		const bool bAttacked = DType == EDamageType::DT_ATTACK && Delta <= 0.f;
		EDamageWoundCategory Category = EDamageWoundCategory::MAX;

		if (bAttacked)
		{
			for (auto& Elem : BoneNameToWoundCategory)
			{
				if (Elem.Value.BoneNames.Contains(HitResult.BoneName) && IsHitResultProperDirection(Elem.Value.DirectionFilter, HitResult))
				{
					Category = Elem.Key;
					break;
				}
			}

			if (Category != EDamageWoundCategory::MAX)
			{
				FPackedWoundInfo NewWoundInfo = GetDamageWounds();

				const float MaxHP = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxHealthAttribute());
				const float NormalizedDamage = FMath::Abs(Delta) / MaxHP;
				const float TotalNormalizedDamage = NewWoundInfo.AddDamageToCategory(Category, NormalizedDamage);

				float NormalizedPermaWoundDamage = UKismetMathLibrary::MapRangeClamped(TotalNormalizedDamage, NormalizedPermaWoundsDamageRange.X, NormalizedPermaWoundsDamageRange.Y, 0.f, 1.f);
				if (const FBoneDamageModifier* Mod = AbilitySystem->BoneDamageMultiplier.Find(HitResult.BoneName))
				{
					NormalizedPermaWoundDamage = FMath::Clamp(NormalizedPermaWoundDamage / Mod->DamageMultiplier, 0.0f, 1.0f);
				}

				if (NormalizedPermaWoundDamage > GetDamageWounds().GetPermaDamageForCategory(Category))
				{
					NewWoundInfo.SetPermaDamageForCategory(Category, NormalizedPermaWoundDamage);
				}

				SetDamageWounds(NewWoundInfo);
			}
		}

		if (Delta > 0.f && HasAuthority())
		{
			const float MaxHP = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxHealthAttribute());

			bool bChangedAnyWound = false;

			for (const EDamageWoundCategory WoundCategory : TEnumRange< EDamageWoundCategory>())
			{
				if (WoundCategory == Category)
				{
					continue;
				}

				FPackedWoundInfo NewWoundInfo = GetDamageWounds();
				float WoundDelta = Delta;
				if (const FBoneDamageModifier* Mod = AbilitySystem->BoneDamageMultiplier.Find(HitResult.BoneName))
				{
					WoundDelta /= Mod->DamageMultiplier;
				}
				WoundDelta /= MaxHP;

				const float CurrentWoundDamage = NewWoundInfo.GetDamageForCategory(WoundCategory);
				const float NewWoundDamage = FMath::Clamp(CurrentWoundDamage - WoundDelta, 0.0f, 1.0f);

				if (CurrentWoundDamage != NewWoundDamage)
				{
					NewWoundInfo.SetDamageForCategory(WoundCategory, NewWoundDamage);

					SetDamageWounds(NewWoundInfo);
					bChangedAnyWound = true;
				}
			}

			if (bChangedAnyWound)
			{
				OnRep_DamageWounds();
			}
		}
	}

	if (GetHealth() <= 0.f && !bIsDying)
	{
		LastPlayedDate = FDateTime();

		const UIGameInstance* const IGameInstance = Cast<UIGameInstance>(GetGameInstance());
		check(IGameInstance);

		// Game Mode Specific Calls
		AIGameMode* IGameMode = Cast<AIGameMode>(GetWorld()->GetAuthGameMode());
		const AIGameSession* const Session = Cast<AIGameSession>(IGameInstance->GetGameSession());

		if (Session && Session->bPermaDeath)
		{
			DeathInfo.bPermaDead = true;
		}

		DeathInfo.KillTimeUTC = FDateTime::UtcNow().ToUnixTimestamp();
		if (AIPlayerController* IPlayerController = Cast<AIPlayerController>(GetController()))
		{
			DeathInfo.DeathLocation = IPlayerController->GetLocationDisplayName();
			if (DeathInfo.DeathLocation.IsEmptyOrWhitespace())
			{
				DeathInfo.DeathLocation = FText::FromString(UGameplayStatics::GetCurrentLevelName(this));
			}

			IPlayerController->SetDeathLocation(GetActorLocation());
		}

		if (EventInstigator && EventInstigator != GetController())
		{
			if (AIPlayerState* InstigatorPState = EventInstigator->GetPlayerState<AIPlayerState>())
			{
				DeathInfo.KillerSpecies = InstigatorPState->GetGenderAndSpeciesDisplayName();
				DeathInfo.KillerName = InstigatorPState->GetPlayerName();
			}

			if (const AIBaseCharacter* const OtherBaseChar = Cast<AIBaseCharacter>(EventInstigator->GetPawn()))
			{
				DeathInfo.KillerDinosaurName = OtherBaseChar->GetCharacterName();
			}
		}
		else
		{
			DeathInfo.KillerSpecies = FText::GetEmpty();
			DeathInfo.KillerName.Empty();
			DeathInfo.KillerDinosaurName.Empty();
		}

		if (IGameMode)
		{
			if (Session)
			{
				int32 GrowthPenaltyPercent = 0;
				int32 MarksPenaltyPercent = 0;
				
				switch (DType)
				{
					case EDamageType::DT_ATTACK:
					case EDamageType::DT_BLEED:
					case EDamageType::DT_TRAMPLE:
					case EDamageType::DT_SPIKES:
					{
						MarksPenaltyPercent = Session->GetCombatDeathMarksPenaltyPercent();
						GrowthPenaltyPercent = Session->GetCombatDeathGrowthPenaltyPercent();
						break;
					}
					case EDamageType::DT_OXYGEN:
					case EDamageType::DT_THIRST:
					case EDamageType::DT_HUNGER:
					{
						MarksPenaltyPercent = Session->GetSurvivalDeathMarksPenaltyPercent();
						GrowthPenaltyPercent = Session->GetSurvivalDeathGrowthPenaltyPercent();
						break;
					}
					case EDamageType::DT_BREAKLEGS:
					{
						MarksPenaltyPercent = Session->GetFallDeathMarksPenaltyPercent();
						GrowthPenaltyPercent = Session->GetFallDeathGrowthPenaltyPercent();
						break;
					}
					case EDamageType::DT_GENERIC:
					{
						// We consider generic death a combat death
						MarksPenaltyPercent = Session->GetCombatDeathMarksPenaltyPercent();
						GrowthPenaltyPercent = Session->GetCombatDeathGrowthPenaltyPercent();
						break;
					}
					default:
					{
						break;
					}
				}

				if (MarksPenaltyPercent > 0 && GetMarks() > 0)
				{
					const int32 MarksToRemove = FMath::CeilToInt(GetMarks() * (static_cast<float>(MarksPenaltyPercent) / 100.f));
					AddMarks(-MarksToRemove);
				}

				float BypassTutorialCaveRequiredGrowth = FMath::Max(HatchlingGrowthPercent, Session->HatchlingCaveExitGrowth);
				BypassTutorialCaveRequiredGrowth = FMath::Max(BypassTutorialCaveRequiredGrowth, Session->GetMinGrowthAfterDeath());

				// If the player died by falling through the world, we skip growth adjustment and reset the flag
				if (bDiedByKillZ)
				{
					bDiedByKillZ = false;
				}
				else if (Session->bServerHatchlingCaves && GetGrowthPercent() < BypassTutorialCaveRequiredGrowth)
				{
					// If we die as a hatchling, set growth to 0.25 to become a juvenile.
					// Players shouldn't die as a hatchling, unless they die immediately after they exit the tutorial cave before they become juvenile.
					SetGrowthPercent(BypassTutorialCaveRequiredGrowth);
				}
				else
				{
					ApplyGrowthPenaltyNextSpawn(GrowthPenaltyPercent);
				}
			}

			// Notify the game mode we got killed for scoring and game over state
			AController* KilledPlayer = nullptr;
			if (Controller)
			{
				KilledPlayer = Controller;
			} else {
				KilledPlayer = Cast<AController>(GetOwner());
			}

			// Correct Quests for Death by Bleed and other sources
			if (!EventInstigator || EventInstigator == KilledPlayer)
			{
				EventInstigator = LastDamageInstigator.Get();
			}

			// Flag character as dead and save so we can respawn properly next spawn
			bRespawn = true;
			GetDamageWounds_Mutable().Reset();

			// Character Killed
			IGameMode->Killed(EventInstigator, KilledPlayer, this, DType);
		}

		// Perform Death
		OnDeath(Delta, DamageInfo, EventInstigator ? EventInstigator->GetPawn() : nullptr, DamageCauser, HitResult.Location, GetActorRotation());
	}

	// Ensure we clear the damage causer if we've fully healed
	AIPlayerController* const IPlayerController = Cast<AIPlayerController>(GetController());
	if (GetHealth() == GetMaxHealth() && IPlayerController)
	{
		IPlayerController->GetReportInfo_Mutable().Reset();
		LastDamageInstigator = nullptr;
	}
}

void AIBaseCharacter::HandleStatusRateChange(const FGameplayAttribute& AttributeToCheck, const FPOTStatusHandling& StatusHandlingInformation, const float Delta, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags, const bool bReplicateParticle, const FHitResult& HitResult)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentValue = AbilitySystem->GetNumericAttribute(AttributeToCheck);

	if (FMath::IsNearlyZero(CurrentValue) && !GetReplicatedDamageParticles().IsEmpty() && !bIsDying && IsAlive())
	{
		RemoveReplicatedDamageParticle(StatusHandlingInformation.DamageEffectType);
	}

	if (StatusHandlingInformation.bShouldApplyCombatGE && CurrentValue > 0.f)
	{
		bool bApplyCombatEffect = true;
		if (!StatusHandlingInformation.bRefreshCombatGEAtEveryTick)
		{
			if (const FActiveGameplayEffect* ActiveEffect = AbilitySystem->GetActiveGameplayEffect(InCombatEffect))
			{
				if (ActiveEffect->GetTimeRemaining(World->GetTimeSeconds()) > 0 && !ActiveEffect->IsPendingRemove)
				{
					bApplyCombatEffect = false;
				}
			}
		}

		if (bApplyCombatEffect)
		{
			const UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
			if (PAGS.InCombatEffect)
			{
				const UGameplayEffect* InCombatGameplayEffect = PAGS.InCombatEffect->GetDefaultObject<UGameplayEffect>();
				InCombatEffect = AbilitySystem->ApplyGameplayEffectToSelf(InCombatGameplayEffect, 1.f, AbilitySystem->MakeEffectContext());
			}
		}
	}

	// This usually means status just started
	if (CurrentValue == Delta && CurrentValue > 0.f)
	{
		if (bReplicateParticle)
		{
			ReplicateParticle(FDamageParticleInfo(StatusHandlingInformation.DamageEffectType, GetActorLocation(), GetActorRotation()));
		}
		
		OnStatusStart.Broadcast(StatusHandlingInformation.DamageEffectType);
		if (GetNetMode() != ENetMode::NM_Client)
		{
			Client_BroadcastStatus(StatusHandlingInformation.DamageEffectType);
		}
	}
}

void AIBaseCharacter::HandleLegDamageChange(float Delta, const FGameplayTagContainer& SourceTags)
{
	if (FMath::IsNearlyZero(GetLegDamage()) && !GetReplicatedDamageParticles().IsEmpty() && !bIsDying && IsAlive())
	{
		RemoveReplicatedDamageParticle(EDamageEffectType::BROKENBONE);
		RemoveReplicatedDamageParticle(EDamageEffectType::BROKENBONEONGOING);
	}
}

bool AIBaseCharacter::GetDamageEventFromEffectSpec(const FGameplayEffectSpec& Spec, FDamageEvent& OutEvent) const
{
	const FGameplayEffectContextHandle& ContextHandle = Spec.GetEffectContext();
	if (ContextHandle.IsValid())
	{
		const FPOTGameplayEffectContext* POTEffectContext = StaticCast<const FPOTGameplayEffectContext*>(ContextHandle.Get());
		return GetDamageEventFromDamageType(POTEffectContext->GetDamageType(), OutEvent);
	}

	return false;
}

bool AIBaseCharacter::GetDamageEventFromDamageType(const EDamageType Type, FDamageEvent& OutEvent) const
{
	switch (Type)
	{

	case EDamageType::DT_OXYGEN:
		OutEvent = FDamageEvent(UDrowningDamageType::StaticClass());
		break;
	case EDamageType::DT_BLEED:
		OutEvent = FDamageEvent(UBloodLossDamageType::StaticClass());
		break;
	case EDamageType::DT_THIRST:
	case EDamageType::DT_HUNGER:
		OutEvent = FDamageEvent(UHungerThirstDamageType::StaticClass());
		break;
	case EDamageType::DT_BREAKLEGS:
		OutEvent = FDamageEvent(UBreakLegsDamageType::StaticClass());
		break;
	case EDamageType::DT_ATTACK:
	case EDamageType::DT_SPIKES:
	case EDamageType::DT_GENERIC:
	default:
		OutEvent = FDamageEvent(UDamageType::StaticClass());
		break;
	}

	return true;
}

EDamageType AIBaseCharacter::GetDamageTypeFromDamageEvent(const FDamageEvent& Event) const
{
	//Since multiple enums maps to a damage type class this is not a losless conversion.

	if(Event.DamageTypeClass == UDrowningDamageType::StaticClass())
	{
		return EDamageType::DT_OXYGEN;
	}

	if (Event.DamageTypeClass == UBloodLossDamageType::StaticClass())
	{
		return EDamageType::DT_BLEED;
	}

	if (Event.DamageTypeClass == UBreakLegsDamageType::StaticClass())
	{
		return EDamageType::DT_BREAKLEGS;
	}

	if (Event.DamageTypeClass == UHungerThirstDamageType::StaticClass())
	{
		return EDamageType::DT_HUNGER;
	}

	return EDamageType::DT_GENERIC;
}

bool AIBaseCharacter::IsHitResultProperDirection(EWoundDirectionFilter Filter, const FHitResult& Result) const
{
	if (Filter == EWoundDirectionFilter::None) return true;

	FVector InverseLocalHitDirection = GetActorTransform().InverseTransformVectorNoScale(Result.ImpactNormal);
	switch (Filter)
	{
	case EWoundDirectionFilter::Left:
		return InverseLocalHitDirection.X < 0.f;
	case EWoundDirectionFilter::Right:
		return InverseLocalHitDirection.X > 0.f;
	case EWoundDirectionFilter::Front:
		return InverseLocalHitDirection.Y > 0.f;
	case EWoundDirectionFilter::Back:
		return InverseLocalHitDirection.Y < 0.f;
	}

	return false;
}

void AIBaseCharacter::Client_BroadcastBoneBreakDelegate_Implementation()
{
	OnBrokenBone.Broadcast();
}

void AIBaseCharacter::Client_BroadcastWellRestedDelegate_Implementation()
{
	OnWellRested.Broadcast();
}

void AIBaseCharacter::Client_BroadcastOnTakeDamageDelegate_Implementation(float DamageDone, float DamageRatio, const FHitResult& HitResult, const FGameplayEffectSpec& Spec, const FGameplayTagContainer& SourceTags)
{
	if (OnTakeDamage.IsBound())
	{
		OnTakeDamage.Broadcast(DamageDone, DamageRatio, HitResult, Spec, SourceTags);
	}
}

void AIBaseCharacter::HandleBrokenBone(const FGameplayTagContainer& SourceTags)
{
	// BreakLegsFromFalling();
	OnBrokenBone.Broadcast();
	if (HasAuthority() && GetNetMode() != ENetMode::NM_Standalone)
	{
		Client_BroadcastBoneBreakDelegate();
	}
}

void AIBaseCharacter::HandleWellRested(const FGameplayTagContainer& SourceTags)
{
	OnWellRested.Broadcast();

	if (HasAuthority() && GetNetMode() != ENetMode::NM_Standalone)
	{
		Client_BroadcastWellRestedDelegate();
	}
}

void AIBaseCharacter::HandleBodyFoodChange(float Delta, const FGameplayTagContainer& SourceTags)
{
#if WITH_EDITOR
	//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::HandleBodyFoodChange - GetMaxBodyFood: %s"), *FString::FromInt(GetMaxBodyFood())));
	//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::HandleBodyFoodChange - GetCurrentBodyFood: %s"), *FString::FromInt(GetCurrentBodyFood())));
#endif

	if (GetCurrentBodyFood() <= 0.0f)
	{
		DestroyBody();
	}
	else
	{
		SetBodyFoodPercentage(GetCurrentBodyFood() / GetMaxBodyFood());
	}
}

void AIBaseCharacter::HandleKnockbackForce(const FVector& Direction, float Force)
{
	if (IsRestingOrSleeping() && (IsLocallyControlled() || HasAuthority()))
	{
		SetIsResting(EStanceType::Default);
	}

	UICharacterMovementComponent* const CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	if (!CharMove)
	{
		return;
	}
	//Modify knockback amount if blocking
	const bool bAbilityBlocking = AbilitySystem != nullptr && AbilitySystem->GetCurrentAttackAbility() != nullptr && AbilitySystem->GetCurrentAttackAbility()->bStopPreciseMovement;
	if (bAbilityBlocking)
	{
		Force *= AbilitySystem->GetCurrentAttackAbility()->GetBlockScalarForDamageType(EDamageEffectType::KNOCKBACK);
	}
	if (Force > KnockbackMaxForce)
	{
		Force = KnockbackMaxForce;
	}

	//Clear momentum and velocity when we want to knockback
	CharMove->StopActiveMovement();
	CharMove->Velocity = FVector::ZeroVector;
	CharMove->bWantsToKnockback = true;
	
	if (Direction == FVector::ZeroVector || FMath::IsNearlyZero(Force))
	{
		return;
	}
	FVector ModifiedForceDirection = Direction;
	if (CharMove->IsMovingOnGround())
	{
		ModifiedForceDirection.Z = FMath::Max(Direction.Z, 0.f);
	}

	FVector Impulse = CharMove->ConstrainDirectionToPlane(ModifiedForceDirection) * Force;

	if (IsLocallyControlled())
	{
		LaunchCharacter(Impulse, true, true);
	}
	else if (HasAuthority())
	{
		ClientGetKnockback(Direction, Force);
	}

	if (Force > AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetKnockbackToCancelAttackThresholdAttribute()) && AbilitySystem->GetCurrentAttackAbility() && AbilitySystem->GetCurrentAttackAbility()->bInterruptable)
	{
		AbilitySystem->CancelCurrentAttackAbility();
	}

	if (HasAuthority())
	{
		const float ForceToDeCarry = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetKnockbackToDecarryThresholdAttribute());
		const float ForceToDeLatch = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetKnockbackToDelatchThresholdAttribute());
		TArray<FName> AttachedCharacterKeys{};
		AttachedCharacters.GetKeys(AttachedCharacterKeys);
		for (const FName AttachedCharKey : AttachedCharacterKeys) // separate attached dinosaurs if knockback is sufficient
		{
			if (!AttachedCharacters.Contains(AttachedCharKey))
			{
				UE_LOG(LogIBaseCharacter, Error, TEXT("AttachedChararcters no longer contains key \"%s\"!"))
				continue;
			}

			if ((Force >= ForceToDeCarry && AttachedCharacters[AttachedCharKey]->GetAttachTarget().AttachType == EAttachType::AT_Carry) ||
				(Force >= ForceToDeLatch && AttachedCharacters[AttachedCharKey]->GetAttachTarget().AttachType == EAttachType::AT_Latch))
			{
				AttachedCharacters[AttachedCharKey]->ClearAttachTarget();
			}
		}

		if (GetAttachTarget().IsValid() && AttachTargetOwner) // separate from parent dinosaur if knockback is sufficient (uses parent dino's attributes for consistency)
		{
			const float ParentForceToDeCarry = AttachTargetOwner->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetKnockbackToDecarryThresholdAttribute());
			const float ParentForceToDeLatch = AttachTargetOwner->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetKnockbackToDelatchThresholdAttribute());
			if ((Force >= ParentForceToDeCarry && GetAttachTarget().AttachType == EAttachType::AT_Carry) ||
				(Force >= ParentForceToDeLatch && GetAttachTarget().AttachType == EAttachType::AT_Latch))
			{
				ClearAttachTarget();
			}
		}
	}

	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		const FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::LaunchCharacter, Impulse, true, true);
		const AIPlayerState* const IPlayerState = GetPlayerState<AIPlayerState>();
		check(IPlayerState);
		if (IPlayerState)
		{
			FTimerHandle DelayKnock;
			GetWorldTimerManager().SetTimer(DelayKnock, Del, IPlayerState->GetRoundTripTimeSeconds(), false);
		}
	}
}

void AIBaseCharacter::ClientGetKnockback_Implementation(const FVector& Direction, float Force)
{
	if (IsLocallyControlled())
	{
		HandleKnockbackForce(Direction, Force);
	}
}

void AIBaseCharacter::LaunchCharacter(FVector NewLaunchVelocity, bool bXYOverride, bool bZOverride)
{
	UE_LOG(TitansLog, Verbose, TEXT("AIBaseCharacter::LaunchCharacter '%s' (%f,%f,%f)"), *GetName(), NewLaunchVelocity.X, NewLaunchVelocity.Y, NewLaunchVelocity.Z);

	if (UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		FVector FinalVel = NewLaunchVelocity;
		const FVector Velocity = GetVelocity();

		if (!bXYOverride)
		{
			FinalVel.X += Velocity.X;
			FinalVel.Y += Velocity.Y;
		}
		if (!bZOverride)
		{
			FinalVel.Z += Velocity.Z;
		}

		SetLaunchVelocity(GetLaunchVelocity() + NewLaunchVelocity);
		CharMove->Launch(FinalVel);

		OnLaunched(NewLaunchVelocity, bXYOverride, bZOverride);
		if (HasAuthority() && !IsLocallyControlled())
		{
			CharMove->GetPredictionData_Server_ICharacter()->bForceNextClientAdjustment = true;
			//AddLaunchVelocity(LaunchVelocity);
		}
	}
}

void AIBaseCharacter::AddLaunchVelocity_Implementation(FVector NewLaunchVel)
{
	SetLaunchVelocity(NewLaunchVel);
}

void AIBaseCharacter::SetLaunchVelocity(const FVector& NewLaunchVelocity)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, LaunchVelocity, NewLaunchVelocity, this);
}

bool AIBaseCharacter::ShouldCancelToggleSprint() const
{
	if (IsInitiatedJump() || GetCharacterMovement()->IsFalling())
	{
		return false;
	}

	if (GetStamina() <= 0)
	{
		return true;
	}

	if (HasBrokenLegs())
	{
		return true;
	}

	if (IsExhausted())
	{
		return true;
	}

	if (IsLimpingRaw() || IsFeignLimping())
	{
		return true;
	}

	return false;
}

void AIBaseCharacter::Tick(float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::Tick"))

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		// Blood Texture Overlay
		UpdateWoundsTextures();
	}
#endif

	// Don't do custom Tick Logic If Dead (this gets disabled later after ragdoll process if finished)
	if (!IsAlive())
	{
		USkeletalMeshComponent* Mesh3P = GetMesh();

		//Stops the wind sound when dead.
		if (IsLocallyControlled() && AudioLoopWindComp)
		{
			AudioLoopWindComp->DestroyComponent();
		}

		if (GetLocalRole() > ROLE_SimulatedProxy)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("Server Updating Pelvis")));
			Server_UpdatePelvisPosition();
		}
		else if (Mesh3P && GetNetMode() == NM_Client)
		{
			FVector CurrentPelvisLocation = GetMesh()->GetSocketLocation(PelvisBoneName);
			int32 LocationDifferenceScale = (CurrentPelvisLocation - GetPelvisLocationRaw()).Size();

			//GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Red, FString::Printf(TEXT("LocationDifferenceScale: %i"), LocationDifferenceScale));

			if (LocationDifferenceScale > 365)
			{
				const FVector Force = ((GetPelvisLocationRaw() - CurrentPelvisLocation).GetSafeNormal() * FMath::Clamp(LocationDifferenceScale * 10, 0, 2000));
				//GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, FString::Printf(TEXT("Force: X: %f Y: %f Z: %f"), Force.X, Force.Y, Force.Z));
				Mesh3P->AddForceToAllBodiesBelow(Force, PelvisBoneName, true, true);
			}
		}

		APhysicsVolume* PhysicsVolume = Mesh3P->GetPhysicsVolume();
		if (PhysicsVolume->bWaterVolume)
		{
			FVector PhysicsVolumeOrigin;
			FVector PhysicsVolumeBoxExtent;
			PhysicsVolume->GetActorBounds(false, PhysicsVolumeOrigin, PhysicsVolumeBoxExtent);

			float DistanceFromSurface = PhysicsVolumeOrigin.Z + PhysicsVolumeBoxExtent.Z - Mesh3P->GetComponentLocation().Z;
			float UnderwaterAmount = DistanceFromSurface / Mesh3P->Bounds.SphereRadius;

			if (UnderwaterAmount < 1.0f) 
			{
				Mesh3P->SetLinearDamping(UnderwaterRagdollLinearDamping * UnderwaterAmount);
				Mesh3P->SetAngularDamping(UnderwaterRagdollAngularDamping * UnderwaterAmount);
				Mesh3P->SetEnableGravity(true);
			}
			else
			{
				Mesh3P->SetLinearDamping(UnderwaterRagdollLinearDamping);
				Mesh3P->SetAngularDamping(UnderwaterRagdollAngularDamping);
				Mesh3P->SetEnableGravity(false);
			}
		}
		else
		{
			Mesh3P->SetLinearDamping(0.f);
			Mesh3P->SetAngularDamping(0.f);
			Mesh3P->SetEnableGravity(true);
		}

		Super::Tick(DeltaTime);
		return;
	}
	else if (GetLocalRole() > ROLE_SimulatedProxy)
	{
		PushCharacter(DeltaTime);
	}
	else if (GetLocalRole() == ROLE_SimulatedProxy && !GetCharacterMovement()->IsComponentTickEnabled())
	{
		// If we are running as SimulatedProxy, and the movement component is disabled, we need to lerp the Character Mesh back to the base offset & rotation. 
		// This is because the movement component performs replicated movement smoothing on the mesh itself.
		if (USkeletalMeshComponent* SkelMesh = GetMesh())
		{
			const float LerpSpeed = 10.0f;
			SkelMesh->SetRelativeLocation(FMath::Lerp(SkelMesh->GetRelativeLocation(), GetBaseTranslationOffset(), DeltaTime * LerpSpeed));
			SkelMesh->SetRelativeRotation(FMath::RInterpTo(SkelMesh->GetRelativeRotation(), GetBaseRotationOffset().Rotator(), DeltaTime, LerpSpeed));
		}		
	}
	
	if (HasAuthority())
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		if (IsLocalAreaLoading() && !IsPreloadingClientArea() && !bIsCharacterEditorPreviewCharacter && !IGameInstance->CheckIfThereAreAnyLoadsInProgress())
		{
			SetPreloadingClientArea(IsLocalAreaLoading());
			UICharacterMovementComponent* MoveComp = Cast<UICharacterMovementComponent>(GetMovementComponent());
			MoveComp->Velocity = FVector(0.f);
		}
	}

	//GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, FString::Printf(TEXT("GrowthPerSecond: %f"), AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetGrowthPerSecondAttribute())));

	Super::Tick(DeltaTime);

	// Don't do this functionality on AI
	if (IsAIControlled()) return;

	/*
	if (GrabbedCharacter)
	{
		const FVector ConstraintLocation = GrabPhysConstraint->ConstraintInstance.GetRefFrame(EConstraintFrame::Frame2).GetLocation();
		if (!ConstraintLocation.IsZero())
		{
			const float NewOffsetSize = FMath::Max(ConstraintLocation.Size() - (MoveGrabbedBodyToHoldPositionRate * DeltaTime), 0.f);
			GrabPhysConstraint->SetConstraintReferencePosition(EConstraintFrame::Frame2, ConstraintLocation.GetSafeNormal() * NewOffsetSize);
		}
		//GrabbedBodyOffsetDistance =
		//FVector TargetLocation = GrabbedCharacter->GetMesh()->GetBoneTransform(GrabbedCharacter->GetMesh()->GetBoneIndex(TEXT("DinoHead"))).InverseTransformPosition(GetMesh()->GetBoneLocation(TEXT("DinoHead")));
		//
	}
	*/

	// ODS VOIP
	//if (UIGameInstance::IsVoipSupported())
	//{
	//	if (!bVoipSetup && GetMesh() && IsLocallyControlled())
	//	{
	//		AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
	//		if (IPlayerState)
	//		{
	//			bVoipSetup = true;
	//
	//			//LocalVoiceComponent->RadioObject->SubscribeToRadio();
	//
	//			//VoipTalker = UVOIPTalker::CreateTalkerForPlayer(IPlayerState);
	//			//if (VoipTalker)
	//			//{
	//			//	VoipTalker->Settings.AttenuationSettings = VoipAttenuationSettings;
	//			//	VoipTalker->Settings.SourceEffectChain = VoipSourceEffectChain;
	//			//	VoipTalker->Settings.ComponentToAttachTo = GetMesh();
	//			//}
	//		}
	//	}
	//}

	// Locally Controlled Tick
#if !UE_SERVER
	if (IsLocallyControlled() && !IsRunningDedicatedServer())
	{
		if (UICharacterMovementComponent* MoveComp = Cast<UICharacterMovementComponent>(GetMovementComponent()))
		{
			if (HighSpeedWind.IsValid() && AudioLoopWindComp != nullptr)
			{
				USoundCue* HighSpeedWindCue = Cast<USoundCue>(HighSpeedWind.Get());

				//If current velocity is at MaxFlySpeed or less, wind will be silent (0.0f). If at MaxNosedivingSpeed the wind will be loud (1.0f)
				float CurrentVelocity = MoveComp->Velocity.Size();
				float targetVolume = 0.0f;

				if (CurrentVelocity > MoveComp->MaxFlySpeed)
				{
					float t = (CurrentVelocity - MoveComp->MaxFlySpeed) / (MoveComp->MaxNosedivingSpeed - MoveComp->MaxFlySpeed);
					targetVolume = t * WindVolumeMultiplier;
				}
				
				HighSpeedWindCue->VolumeMultiplier = FMath::FInterpConstantTo(HighSpeedWindCue->VolumeMultiplier, targetVolume, DeltaTime, 1.0f);
			}
			else if (AudioLoopWindComp == nullptr && !windSoundLoading)
			{
				UIGameplayStatics::LoadSoundAsync(HighSpeedWind, FOnSoundLoaded::CreateUObject(this, &AIBaseCharacter::SoundLoaded_WindLoop), this);
				windSoundLoading = true;
			}	

			if (!MoveComp->IsComponentTickEnabled())
			{
				// We need to manually send client camera updates if the movement component tick is disabled. Otherwise the server will never receive an update. -Poncho

				static const float TimeBetweenLatchedCameraUpdates = 1.0f;
				const float CurrentTime = GetWorld()->GetTimeSeconds();
				if (CurrentTime - TimeBetweenLatchedCameraUpdates >= LastAttachedCameraUpdateTime)
				{
					LastAttachedCameraUpdateTime = CurrentTime;
					MoveComp->ManualSendClientCameraUpdate();
				}
			}
		}

		if (IsPreloadingClientArea())
		{
			TryFinishPreloadClientArea();
		}

		if (bCollectHeld && CanUse())
		{
			InputStartCollect();
		}

		if (bAutoRunActive)
		{
			InputMoveY(1.0f);
		}

		// Aim Rotation
		FRotator PreviousDesiredAim = DesiredAimRotation;
		UpdateFocusAndAimRotation(DeltaTime); // find our focal point location (whether it be locked to a component or from free looking)

		const float TimeDilation = FMath::Max(GetActorTimeDilation(), KINDA_SMALL_NUMBER);
		TimeSinceLastServerUpdateAim += (DeltaTime / TimeDilation);

		const AGameNetworkManager* const GameNetworkManager = GetDefault<AGameNetworkManager>();
		const float ClientNetAimUpdateDeltaTime = 0.0166f;
		const float ClientNetAimUpdatePositionLimit = 1000.0f;

		//const bool bPositionThreshold = ((DesiredAimRotation - PreviousDesiredAim).Normalize() > (ClientNetAimUpdatePositionLimit * ClientNetAimUpdatePositionLimit));

		if (TimeSinceLastServerUpdateAim > ClientNetAimUpdateDeltaTime)
		{
			if (!DesiredAimRotation.Equals(PreviousDesiredAim) && GetLocalRole() == ROLE_AutonomousProxy && GetNetMode() == NM_Client)
			{
				const FRotator ClampedDesiredAimRot = DesiredAimRotation.Clamp();
				ServerUpdateDesiredAim(static_cast<uint8>((ClampedDesiredAimRot.Pitch / 360) * 255), static_cast<uint8>((ClampedDesiredAimRot.Yaw / 360) * 255)); // send compressed pitch and yaw
				TimeSinceLastServerUpdateAim = 0.0f;
			}
		}

		// Camera Offset
		UpdateCameraOffset(DeltaTime);

		// Stop Crouching if trying to swim - ANIXON TEMP FIX
		if (IsSwimming() && GetCharacterMovement() && GetCharacterMovement()->bWantsToCrouch)
		{
			UnCrouch();
			bDesiredUncrouch = false;
		}

		// Use Hold Time
		if (bUseButtonHeld)
		{
			UseHoldTime += DeltaTime;

			if (UseHoldTime > UseHoldThreshold)
			{
				UseHoldTime = 0.0f;
				bUseButtonHeld = false;
				bUseHeldCalled = true;
				UseHeld();
			}
		}

		// Rest Hold Time
		if (bCanEverSleep && bRestButtonHeld && GetRestingStance() == EStanceType::Resting && CanSleepOrRestInWater(90.0f))
		{
			RestHoldTime += DeltaTime;

			if (RestHoldTime > RestHoldThreshold && !HasDinosLatched())
			{
				if (!UIGameplayStatics::IsInGameWaystoneAllowed(this))
				{
					bRestButtonHeld = false;
				}

				SetIsResting(EStanceType::Sleeping);
			}
		}

		//Stops the player from immediately crouching when landing while holding ctrl
		if (TimeCantCrouchElapsed < TimeCantCrouchAfterLanding)
		{
			TimeCantCrouchElapsed += DeltaTime;
		}

		if (GetRestingStance() == EStanceType::Sleeping && bRestButtonHeld)
		{
			RestHoldTime += DeltaTime;

			if (RestHoldTime > WaystoneHoldThreshold)
			{
				bRestButtonHeld = false;
			}
		}

		// Foliage
		if (MPC_InteractiveFoliage)
		{
			UMaterialParameterCollectionInstance* Instance = GetWorld()->GetParameterCollectionInstance(MPC_InteractiveFoliage);
			if (Instance)
			{
				FVector InteractiveFoliagePlayerLoc = GetActorLocation();
				Instance->SetVectorParameterValue(NAME_PlayerLocation, FLinearColor(InteractiveFoliagePlayerLoc));
				Instance->SetScalarParameterValue(NAME_CapsuleRadius, GetCapsuleComponent()->GetScaledCapsuleRadius());
			}
		}

		// Faster Breathing when Low on Stamina
		if (BreatheTimeline && BreatheTimeline->IsPlaying())
		{
			float NewPlayRate = UKismetMathLibrary::MapRangeClamped(GetStamina(), 0, GetMaxStamina(), 2.5f, 1.0f);
			BreatheTimeline->SetPlayRate(NewPlayRate); 
		}

		if (bDesiredUncrouch)
		{
			if (CanUncrouch())
			{
				UnCrouch();
				bDesiredUncrouch = false;
			}
		}
	}
#endif

	UpdateAimRotation(DeltaTime);

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (ShouldUpdateTailPhysics())
		{
			BlendTailPhysics(IsSwimming(), DeltaTime);
		}
	}
#endif

	// Update Movement States on Authority and Locally Controlled Players.
	if (GetLocalRole() > ROLE_SimulatedProxy)
	{
		UpdateMovementState();

		if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			if (IGameUserSettings->bToggleSprint && bDesiresToSprint && ShouldCancelToggleSprint())
			{
				bDesiresToSprint = false;
			}
		}
	}

	// The dinosaur is falling and is just about to hit the water surface
	if (GetCustomMovementType() == ECustomMovementType::FALLING && IsInWater(105) && GetVelocity().Z <= SplashThresholdVelocity)
	{
		PlaySplashEffectAtLocation();
	}

	//Bandaid fix for collision remaining after death
	if (AbilitySystem != nullptr && GetCapsuleComponent() != nullptr &&
		GetCapsuleComponent()->IsCollisionEnabled() && !AbilitySystem->IsAlive())
	{
		DisableCollisionCapsules(true);
	}

	if (bDoingDamage && !IsStunned() && !IsHomecaveBuffActive())
	{
#if UE_SERVER
		if (IsRunningDedicatedServer())
		{
			if (bCanAttackOnServer)
			{
				DoDamageSweeps(DeltaTime);
			}
			else
			{
				//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::Tick() - DoDamageSweeps not processed due to characters distance from nearest player!"));
			}
		}
#else
		DoDamageSweeps(DeltaTime);
#endif
	}

	UpdateOverlaps(DeltaTime);

	if (IsStunned())
	{
		if (GetVelocity().SizeSquared() <= 100.f)
		{
			SetIsStunned(false);
		}
	}

	if (HasAuthority() && bImpactInvinsible)
	{
		elapsedImpactDamageTime += DeltaTime;
		if (elapsedImpactDamageTime > ImpactInvinsibleTime)
		{
			elapsedImpactDamageTime = 0.0f;
			bImpactInvinsible = false;
		}
	}
}

void AIBaseCharacter::OnPhysicsVolumeChanged(APhysicsVolume* NewVolume)
{
	if (NewVolume && NewVolume->bWaterVolume && IsLocallyControlled())
	{
		if (AbilitySystem)
		{

			FHitResult WaterHit;
			FVector Start = GetActorLocation();

			if (IsInOrAboveWater(&WaterHit, &Start))
			{
				if (AIWater* IWater = Cast<AIWater>(WaterHit.GetActor()))
				{
					PossiblyUpdateWaterVision(IWater);
				}
			}

		}
	}
}

void AIBaseCharacter::PossiblyUpdateWaterVision(AIWater* Water)
{
	if (!ensure(Water))
	{
		return;
	}
	if (!ensure(GetCapsuleComponent()))
	{
		return;
	}
	FVector WaterOrigin, WaterExtent;
	Water->GetActorBounds(true, WaterOrigin, WaterExtent);
	if (!UKismetMathLibrary::IsPointInBox(GetActorLocation(), WaterOrigin, WaterExtent) || !UKismetMathLibrary::IsPointInBox(GetActorLocation() - (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 5.0f), WaterOrigin, WaterExtent))
	{
		// Actor isn't within water bounds, also point 5 units under the bottom of the actor isn't within water bounds.
		return;
	}
	float WaterVision = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetWaterVisionAttribute());
	Water->SetFogDistanceMultiplier(WaterVision);
}

bool AIBaseCharacter::CanSleepOrRestInWater(float PercentToTrigger /*= 50.0f*/) const
{
	// Overridden in IDinosaurCharacter which is the version that most likely is actually being used
	// The code base checks for AIBaseCharacter so it was easier to put the function here and override it
	return !IsInWater(PercentToTrigger);
}

void AIBaseCharacter::UpdateWoundsTextures()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;

	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh) return;

	float Value = 1 - (GetHealth() / GetMaxHealth());

	bool bShouldUseWounds = bWoundsEnabled 
		&& BloodMaskSource != nullptr
		&& BloodMaskPixelData.TextureSize.X > 0
		&& BloodMaskPixelData.TextureSize.Y > 0
		&& BloodMaskPixelData.Pixels.Num() > 0;

	int PrimarySkinSlot = CharacterMesh->GetMaterialIndex(NAME_Skin);
	if (PrimarySkinSlot != INDEX_NONE)
	{
		UMaterialInstanceDynamic* PrimarySkinDynamic = Cast<UMaterialInstanceDynamic>(CharacterMesh->GetMaterial(PrimarySkinSlot));
		if (PrimarySkinDynamic)
		{
			
			PrimarySkinDynamic->SetScalarParameterValue(TEXT("bWoundsEnabled"), bShouldUseWounds ? 1.0f : 0.0f);
			PrimarySkinDynamic->SetScalarParameterValue(TEXT("DamageAmount"), Value);
			if (BloodMask)
			{
				PrimarySkinDynamic->SetTextureParameterValue(TEXT("WoundLocations"), BloodMask);
			}
		}
	}

	// Secondary Skin Dynamic Material Instance
	int SecondarySkinSlot = CharacterMesh->GetMaterialIndex(NAME_Skin2);
	if (SecondarySkinSlot != INDEX_NONE)
	{
		UMaterialInstanceDynamic* SecondarySkinDynamic = Cast<UMaterialInstanceDynamic>(CharacterMesh->GetMaterial(SecondarySkinSlot));
		if (SecondarySkinDynamic)
		{
			SecondarySkinDynamic->SetScalarParameterValue(TEXT("bWoundsEnabled"), bShouldUseWounds ? 1.0f : 0.0f);
			SecondarySkinDynamic->SetScalarParameterValue(TEXT("DamageAmount"), Value);
			if (BloodMask)
			{
				SecondarySkinDynamic->SetTextureParameterValue(TEXT("WoundLocations"), BloodMask);
			}
		}
	}

	//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::UpdateWoundsTextures() Completed"));
#endif
}

void AIBaseCharacter::UpdateMovementState(bool bSkipAbilityCancel)
{
	// Don't update the movement state on simulated proxies since its determined from replicated state
	// authority and locally controlled players need to calculate this however
	if (GetLocalRole() <= ROLE_SimulatedProxy) return;

	// Fetch Character Movement Component and Ensure It's Valid
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetMovementComponent());
	check(ICharMove);

	// Update Movement Type
	ECustomMovementType NewMovementType = GetCustomMovementType();

	if (GetCurrentMovementType() != NewMovementType)
	{
		if (AbilitySystem && !bSkipAbilityCancel)
		{
			AbilitySystem->UpdateAutoCancelMovementType(NewMovementType);
		}

		if (NewMovementType == ECustomMovementType::SWIMMING && IsRestingOrSleeping())
		{
			SetIsResting(EStanceType::Default);
		}
		
		SetCurrentMovementType(NewMovementType);
	}

	// Update Movement Mode
	ECustomMovementMode NewMovementMode = GetCustomMovementMode();
	if (GetCurrentMovementMode() != NewMovementMode)
	{
		SetCurrentMovementMode(NewMovementMode);
	}

	// Locally Controlled Updates
	if (IsLocallyControlled())
	{
		// Stopped
		bool bStopped = !CanMove(false);
		ICharMove->SetStopped(bStopped);

		// Precise Movement
		ICharMove->SetPreciseMovement(CanPreciseMove());

		// Limping
		ICharMove->SetLimping(IsLimpingRaw() || IsFeignLimping() || GetLegDamage() > 0);

		// Trying to sprint when crouched should take you out of crouch
		if (bDesiresToSprint && bIsCrouched && CanSprint())
		{
			UnCrouch();
		}

		// Sprinting
		ICharMove->SetSprinting(bDesiresToSprint && CanSprint() && !GetVelocity().IsNearlyZero());

		// Trotting
		ICharMove->SetTrotting(bDesiresToTrot && CanTrot());
	}
}

void AIBaseCharacter::CheckJumpInput(float DeltaTime)
{
	UpdateLimpingState();

	Super::CheckJumpInput(DeltaTime);

	UpdateMovementState();
}

void AIBaseCharacter::OnMontageStarted(UAnimMontage* Montage)
{
	if (!Montage) return;

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	//GEngine->AddOnScreenDebugMessage(-1, 60.f, FColor::Green, FString::Printf(TEXT("OnMontageStarted() %s"), *Montage->GetName()));
#endif
}

void AIBaseCharacter::OnMontageEnded(UAnimMontage* Montage, bool bInterupted)
{
	if (!Montage) return;

#if WITH_EDITOR || UE_BUILD_DEVELOPMENT
	//GEngine->AddOnScreenDebugMessage(-1, 60.f, FColor::Red, FString::Printf(TEXT("OnMontageEnded() %s Interupted: %i"), *Montage->GetName(), bInterupted));
#endif
}

void AIBaseCharacter::BeginPlay()
{
	Super::BeginPlay();

	UPOTAbilitySystemComponent* const POTAbilitySystemBase = Cast<UPOTAbilitySystemComponent>(AbilitySystem);
	if (ensureAlways(POTAbilitySystemBase))
	{
		POTAbilitySystemBase->InitAttributes(true);
		POTAbilitySystemBase->SetupAttributeDelegates();
	}
	
	CREATOR_FLOATSLIDER_GETSET(GetHealth, SetHealth, 0, GetMaxHealth, TEXT("Health"));
	CREATOR_FLOATSLIDER_GETSET(GetStamina, SetStamina, 0, GetMaxStamina, TEXT("Stamina"));
	CREATOR_FLOATSLIDER_GETSET(GetOxygen, SetOxygen, 0, GetMaxOxygen, TEXT("Oxygen"));
	CREATOR_FLOATSLIDER_GETSET(GetThirst, SetThirst, 0, GetMaxThirst, TEXT("Thirst"));
	CREATOR_FLOATSLIDER_GETSET(GetHunger, SetHunger, 0, GetMaxHunger, TEXT("Hunger"));
	CREATOR_BOOLTOGGLE_GETSET(GetGodmode, SetGodmode, TEXT("Godmode"));

	if (bDelayOnRep_AttachTarget)
	{		
		OnRep_AttachTarget(FAttachTarget());
		bDelayOnRep_AttachTarget = false;
	}

	/*AbilitySystem->InitializeAbilitySystem(this, true);
	ResetStats();*/

#if !UE_SERVER

	// Users re-entering replication distance will need to update their mesh textures
	UpdateBloodMask(true);
	UpdateWoundsTextures();

	GetWorldTimerManager().SetTimer(UpdateDamageWoundsTimer, this, &AIBaseCharacter::OnUpdateDamageWoundsTimer, 1.f, true);

	if (!IsAlive())
	{
		if (!IsRunningDedicatedServer())
		{
			OnDeathCosmetic();
		}

		return;
	}
#endif

	if (HasAuthority())
	{
		SetGodmode(false);
	}
	if (IsLocallyControlled())
	{
		GetWorldTimerManager().SetTimer(TimerHandle_LocalAreaLoadingCheck, this, &AIBaseCharacter::UpdateLocalAreaLoading, 0.2f, true);
	}

	if (bPushEnabled)
	{
		PushCapsuleComponent->OnComponentBeginOverlap.AddDynamic(this, &AIBaseCharacter::OnPushComponentBeginOverlap);
		PushCapsuleComponent->OnComponentEndOverlap.AddDynamic(this, &AIBaseCharacter::OnPushComponentEndOverlap);

		PushCapsuleComponent->IgnoreActorWhenMoving(this, true);
	}
	else
	{
		if (PushCapsuleComponent) PushCapsuleComponent->DestroyComponent();
	}

	USkeletalMeshComponent* SkeleMesh = GetMesh();
	if (SkeleMesh)
	{
		UAnimInstance* AnimInstance = SkeleMesh->GetAnimInstance();
		if (AnimInstance)
		{
			AnimInstance->OnMontageStarted.AddDynamic(this, &AIBaseCharacter::OnMontageStarted);
			AnimInstance->OnMontageEnded.AddDynamic(this, &AIBaseCharacter::OnMontageEnded);
		}
	}

	const FVector2D CurrentCameraDistanceRange = GetCurrentCameraDistanceRange();

	// Set the default third person spring arm component arm length to be the upper bounds of the camera distance range
	if (ThirdPersonSpringArmComponent)
	{
		ThirdPersonSpringArmComponent->TargetArmLength = CurrentCameraDistanceRange.Y; //TODO: Scale to growth percent
	}

	DesireSpringArmLength = CurrentCameraDistanceRange.Y;

	// Breathe Timeline
#if !UE_SERVER
	if (CurveBreathe && !IsRunningDedicatedServer())
	{
		FOnTimelineFloat onTimelineCallback;
		//FOnTimelineEventStatic onTimelineFinishedCallback;

		BreatheTimeline = NewObject<UTimelineComponent>(this, FName("TimelineBreathe"));
		BreatheTimeline->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		BlueprintCreatedComponents.Add(BreatheTimeline);
		BreatheTimeline->SetPropertySetObject(this);
		BreatheTimeline->SetDirectionPropertyName(FName("TimelineDirection"));
		BreatheTimeline->SetLooping(true);
		BreatheTimeline->SetTimelineLength(5.0f);
		BreatheTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_LastKeyFrame);
		//BreatheTimeline->SetPlaybackPosition(0.0f, false);

		onTimelineCallback.BindUFunction(this, FName{ TEXT("TimelineCallback_Breathe") });
		//onTimelineFinishedCallback.BindUFunction(this, FName{ TEXT("TimelineCallback_Breathe") });
		BreatheTimeline->AddInterpFloat(CurveBreathe, onTimelineCallback, FName{ TEXT("BreatheTimelineValue") }, FName{ TEXT("BreatheTimelineValue") });
		//BreatheTimeline->SetTimelineFinishedFunc(onTimelineFinishedCallback);
		BreatheTimeline->RegisterComponent();
		BreatheTimeline->PlayFromStart();
	}
#endif

#if !UE_SERVER
	// Blink Timeline
	if (CurveBlink && !IsRunningDedicatedServer())
	{
		FOnTimelineFloat onTimelineCallback;
		//FOnTimelineEventStatic onTimelineFinishedCallback;

		BlinkTimeline = NewObject<UTimelineComponent>(this, FName("TimelineBlink"));
		BlinkTimeline->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		BlueprintCreatedComponents.Add(BlinkTimeline);
		BlinkTimeline->SetPropertySetObject(this);
		BlinkTimeline->SetDirectionPropertyName(FName("TimelineDirection"));
		BlinkTimeline->SetLooping(true);
		BlinkTimeline->SetTimelineLength(30.0f);
		BlinkTimeline->SetTimelineLengthMode(ETimelineLengthMode::TL_LastKeyFrame);
		//BlinkTimeline->SetPlaybackPosition(0.0f, false);

		onTimelineCallback.BindUFunction(this, FName{ TEXT("TimelineCallback_Blink") });
		//onTimelineFinishedCallback.BindUFunction(this, FName{ TEXT("TimelineCallback_Blink") });
		BlinkTimeline->AddInterpFloat(CurveBlink, onTimelineCallback, FName{ TEXT("BlinkTimelineValue") }, FName{ TEXT("BlinkTimelineValue") });
		//BreatheTimeline->SetTimelineFinishedFunc(onTimelineFinishedCallback);
		BlinkTimeline->RegisterComponent();
		BlinkTimeline->PlayFromStart();
	}
#endif

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		bShouldAllowTailPhysics = true;

		//For character preview physics init
		//BlendTailPhysics(false, 0.f);

		if (AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>())
		{
			if (IPlayerState->GetPlayerGroupActor() && IPlayerState->ProxyGroupActor)
			{
				IPlayerState->ProxyGroupActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);
			}
			else if (IPlayerState->ProxyAdminNametagActor)
			{
				IPlayerState->ProxyAdminNametagActor->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);
			}
		}
	}
#endif
}

void AIBaseCharacter::BeginDestroy()
{
	if (HasAuthority())
	{
		// Possible server crash workaround
		UWorld* CurrentWorld = GetWorld();
		if (CurrentWorld)
		{
			CurrentWorld->GetTimerManager().ClearTimer(ServerUseTimerHandle);
		}

		if (AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>())
		{
			if (IPlayerState->GetPlayerGroupActor() && IPlayerState->ProxyGroupActor)
			{
				IPlayerState->ProxyGroupActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}
			else if (IPlayerState->ProxyAdminNametagActor)
			{
				IPlayerState->ProxyAdminNametagActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
			}
		}

		if (GetCurrentInstance())
		{
			// Need to remove the player directly
			// as calling ExitInstance() will 
			// reset the logout save information
			GetCurrentInstance()->RemovePlayer(this, false);
		}
	}

	Super::BeginDestroy();
}

void AIBaseCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (BloodMaskPixelData.Pixels.Num() == 0)
	{
		CacheBloodMaskPixels();
	}
}

void AIBaseCharacter::PreSave(FObjectPreSaveContext ObjectSaveContext)
{
	/*if (SlottedAbilityAssets.Num() > 0)
	{
		for (const auto& KVP : SlottedAbilityAssets)
		{
			FSlottedAbilities SAbilities;
			SAbilities.Category = KVP.Key;
			SAbilities.SlottedAbilities = KVP.Value.SlottedAbilities;
			SlottedAbilityAssetsArray.Add(SAbilities);
		}

		SlottedAbilityAssets.Empty();
	}*/

	Super::PreSave(ObjectSaveContext);
}

void AIBaseCharacter::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsSaving())
	{
		CacheBloodMaskPixels();
	}
#endif //WITH_EDITOR
}

void AIBaseCharacter::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (BloodMaskPixelData.Pixels.Num() == 0)
	{
		CacheBloodMaskPixels();
	}
#endif
}

void AIBaseCharacter::AddMarks(int32 MarksToAdd)
{
	check(HasAuthority());
	int32 NewMarks = GetMarks();
	if (MarksToAdd != 0)
	{
		//make sure it is not already capped before adding
		NewMarks = FMath::Clamp(NewMarks, 0, MaximumMarks);
		NewMarks = FMath::Clamp(NewMarks + MarksToAdd, 0, MaximumMarks);
		SetMarks(NewMarks);	
	}

}

void AIBaseCharacter::SetIsAIControlled(bool bNewIsAIControlled)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bIsAIControlled, bNewIsAIControlled, this);
}

void AIBaseCharacter::SetCharacterID(const FAlderonUID& NewCharacterID)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, CharacterID, NewCharacterID, this);
}

void AIBaseCharacter::DisableHungerThirst()
{
	bCanGetThirsty = false;
	bCanGetHungry = false;


	if (AbilitySystem != nullptr)
	{
		UPOTAbilitySystemGlobals& PASG = UPOTAbilitySystemGlobals::Get();
		AbilitySystem->AddLooseGameplayTag(PASG.SurvivalDisabledTag);
	}
}

void AIBaseCharacter::CharSelectRandomEmotes()
{
	float RandomTimerRange = FMath::RandRange(MinEmoteRange, MaxEmoteRange);
	GetWorldTimerManager().SetTimer(TimerHandle_RandomEmote, this, &AIBaseCharacter::PlayRandomEmote, RandomTimerRange, true);
}

void AIBaseCharacter::PlayRandomEmote()
{
	// Reset Timer to another random amount
	CharSelectRandomEmotes();

	// STUB - Overridden in child classes
}

void AIBaseCharacter::OnRep_AttachTarget(const FAttachTarget& OldAttachTarget)
{
	if (!HasActorBegunPlay())
	{
		// Sometimes AttachTarget can be replicated before BeginPlay. We need to delay this execution until after BeginPlay otherwise some state is broken.
		bDelayOnRep_AttachTarget = true;
		return;
	}

	UCharacterMovementComponent* const CharMove = GetCharacterMovement();
	USkeletalMeshComponent* const SkeleMesh = GetMesh();
	if (CharMove == nullptr || SkeleMesh == nullptr)
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("IBaseCharacter::OnRep_AttachTarget() - Character Movement or Skeletal Mesh is NULL!"));
		return;
	}

	UDinosaurAnimBlueprint* const DinoAnimBlueprint = Cast<UDinosaurAnimBlueprint>(SkeleMesh->GetAnimInstance());
	if (DinoAnimBlueprint == nullptr)
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("IBaseCharacter::OnRep_AttachTarget() - Dinosaur Animation Blueprint is NULL!"));
		return;
	}

	const UWorld* const World = GetWorld();
	if (!ensureAlwaysMsgf(World, TEXT("Could not find world in AttachTarget")))
	{
		return;
	}

	const FAttachTarget& AttachTarget = GetAttachTarget();

	AIBaseCharacter* OldAttachCharacter = nullptr;
	AIBaseCharacter* NewAttachCharacter = nullptr;

	if (OldAttachTarget.IsValid())
	{
		OldAttachCharacter = Cast<AIBaseCharacter>(OldAttachTarget.AttachComponent->GetOwner());
	}
	if (AttachTarget.IsValid())
	{
		NewAttachCharacter = Cast<AIBaseCharacter>(AttachTarget.AttachComponent->GetOwner());
	}
	AttachTargetOwner = NewAttachCharacter;

	LastAttachCharacter = OldAttachCharacter;

	DinoAnimBlueprint->SetIsPounceAttached(false);
	DinoAnimBlueprint->SetIsCarryAttached(false);

	const FGameplayTag StaminaExhaustionTag = UPOTAbilitySystemGlobals::Get().StaminaExhaustionTag;
	
	if (HasAuthority())
	{
		AbilitySystem->RegisterGameplayTagEvent(StaminaExhaustionTag).RemoveAll(this);
		if (OldAttachCharacter)
		{
			OldAttachCharacter->AttachedCharacters.Remove(OldAttachTarget.AttachSocket);
			
			if (OldAttachTarget.AttachType == EAttachType::AT_Carry)
			{
				OldAttachCharacter->GetCurrentlyCarriedObject_Mutable().Reset();
			}
		}
	}

	World->GetTimerManager().ClearTimer(DelayCollisionAfterUnlatchHandle);

	if (NewAttachCharacter)
	{
		SetCollisionResponseProfile(AttachedCollisionResponseProfile);
		BaseTranslationOffset = FVector::ZeroVector; // #3583 - removes skeletal mesh offset on simulated proxies while attached - restored when Uncrouch() 
		SkeleMesh->SetRelativeLocation(FVector::ZeroVector);

		// HACK: Normally, skeletal mesh ticks during MovementComponent::TickComponent, but we need to disable ticking MovementComponent 
		// to ensure the attachment replication doesn't jitter so tick the skeletal mesh component outside of the movement component.
		SkeleMesh->bOnlyAllowAutonomousTickPose = false;
		DinoAnimBlueprint->bUseIK = false;
		switch (AttachTarget.AttachType)
		{
			case (EAttachType::AT_Latch):
			{
				DinoAnimBlueprint->SetIsPounceAttached(true);

				// Only run the following code on the server and the owning client.
				if (!HasAuthority() && !IsLocallyControlled())
				{
					break;
				}

				// If we latched, we want to grab the active pounce ability and make sure the VelocityOnFinishMode is set to ERootMotionFinishVelocityMode::SetVelocity.
				// We don't do it by default, else a missed pounce will lead to the player just stopping mid-air due to their velocity being reset to zero.
				const FGameplayAbilitySpec* const FoundAbility = AbilitySystem->GetActivatableAbilities().FindByPredicate([](const FGameplayAbilitySpec& Spec) {
					return IsValid(Spec.Ability) && Spec.Ability->IsA<UPOTGameplayAbility_Pounce>();
				});

				if (!FoundAbility)
				{
					break;
				}

				// This will propagate to the root motion source on the CMC so we shouldn't get any prediction error.
				const UPOTGameplayAbility_Pounce* const PounceAbility = Cast<UPOTGameplayAbility_Pounce>(FoundAbility->GetPrimaryInstance());
				if (!PounceAbility)
				{
					break;
				}

				if (UAbilityTask_ApplyRootMotionConstantForce* AbilityRootMotion = PounceAbility->GetAbilityTaskRootMotion())
				{
					AbilityRootMotion->SetFinishVelocityMode(ERootMotionFinishVelocityMode::SetVelocity);
				}

				break;
			}
			case (EAttachType::AT_Carry):
			{
				if (World->IsNetMode(NM_Client) && ThirdPersonSpringArmComponent && ThirdPersonCameraComponent && NewAttachCharacter->ThirdPersonSpringArmComponent)
				{
					CachedThirdPersonSpringArmTransform = ThirdPersonSpringArmComponent->GetRelativeTransform();

					ThirdPersonSpringArmComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);

					ThirdPersonSpringArmComponent->AttachToComponent(NewAttachCharacter->GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);

					ThirdPersonSpringArmComponent->SetRelativeLocation(NewAttachCharacter->ThirdPersonSpringArmComponent->GetRelativeLocation());
					ThirdPersonSpringArmComponent->SetRelativeRotation(RelativeClampedSpringArmRotation);

					ThirdPersonSpringArmComponent->bUsePawnControlRotation = true;
					ThirdPersonCameraComponent->bUsePawnControlRotation = true;

					// Set the camera distance range to be whatever the attach target's is, and set the desired camera length to the higher of the range.
					CameraDistanceRangeWhileCarried = NewAttachCharacter->GetCurrentCameraDistanceRange();
					SetDesireSpringArmLength(CameraDistanceRangeWhileCarried.Y);
				}

				DinoAnimBlueprint->SetIsCarryAttached(true);
				StopAllInteractions(false);

				AbilitySystem->CancelActiveAbilitiesOnBecomeCarried();

				break;
			}
		}
		
		CharMove->SetComponentTickEnabled(false);
		CharMove->StopMovementImmediately();
		CharMove->SetMovementMode(MOVE_Falling); // stops swimming stuff
		SetIsResting(EStanceType::Default);

		if (HasAuthority())
		{
			switch (AttachTarget.AttachType)
			{
				case (EAttachType::AT_Latch):
				{
					AbilitySystem->RegisterGameplayTagEvent(StaminaExhaustionTag).AddUObject(this, &AIBaseCharacter::OnLatchedStaminaTagChanged);
					NewAttachCharacter->SetIsResting(EStanceType::Default);
					break;
				}
				case (EAttachType::AT_Carry):
				{
					NewAttachCharacter->GetCurrentlyCarriedObject_Mutable().Object = this;
					break;
				}
			}
			
			NewAttachCharacter->AttachedCharacters.FindOrAdd(AttachTarget.AttachSocket) = this;
			AttachToComponent(AttachTarget.AttachComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachTarget.AttachSocket);
		}

		if (NewAttachCharacter != OldAttachCharacter && OldAttachCharacter != nullptr)
		{
			// Make sure the detach of the old character is broadcast before broadcasting the new attach.
			OldAttachCharacter->OnOtherDetached.Broadcast(this, OldAttachTarget.AttachType);
			OnDetached.Broadcast(OldAttachCharacter);
		}

		OnAttached.Broadcast(NewAttachCharacter);
	}
	else
	{
		DinoAnimBlueprint->bUseIK = true;

		// Reset pitch & roll when detaching
		FRotator CurrentRot = GetActorRotation();
		CurrentRot.Pitch = 0.f;
		CurrentRot.Roll = 0.f;
		SetActorRotation(CurrentRot);

		if (!CharMove->IsComponentTickEnabled())
		{
			CharMove->SetComponentTickEnabled(true);
		}

		static const float MinZoneRadius = 700.0f;
		static const float OverlapCapsuleInflationMultiplier = 1.0f;

		if (OldAttachTarget.IsValid())
		{
			const FBoxSphereBounds AttachBounds = OldAttachTarget.AttachComponent->CalcBounds(OldAttachTarget.AttachComponent->GetComponentTransform());
			const float ZoneRadius = FMath::Max(AttachBounds.SphereRadius, MinZoneRadius);

			const FVector OldAttachLocation = OldAttachTarget.AttachComponent->GetSocketLocation(OldAttachTarget.AttachSocket);

			const FVector BaseSweepForward = (OldAttachLocation - OldAttachCharacter->GetActorLocation()).GetSafeNormal();
			const FQuat BaseSweepRotation = BaseSweepForward.ToOrientationQuat();

			const bool bSkipInitialSweep = (OldAttachTarget.AttachType != EAttachType::AT_Carry);
			RepositionIfObstructed(OldAttachCharacter, BaseSweepRotation, ZoneRadius, OverlapCapsuleInflationMultiplier, bSkipInitialSweep);

			LastDetachHeight = GetActorLocation().Z;
			LastDetachTeleportHeightDelta = (OldAttachCharacter->GetActorLocation().Z - LastDetachHeight);
		}

		if (GetAttachParentActor() != nullptr)
		{
			DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		CharMove->UnCrouch(); // resets charmove mesh offsets to appropriate standing height, which is overridden while carried
		if (CharMove->IsInWater())
		{
			CharMove->SetMovementMode(MOVE_Swimming);
		}
		else
		{
			CharMove->SetMovementMode(MOVE_Falling);
		}
		
		const FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::EnableCollisionAfterDelay);
		World->GetTimerManager().SetTimer(DelayCollisionAfterUnlatchHandle, Del, DelayCollisionAfterUnlatchTime, false);
		SetCollisionResponseProfile(RecentlyDetachedCollisionResponseProfile);

		if (OldAttachCharacter)
		{
			// Attach the camera to the other player since we don't want to let the camera clip through the world 
			// while clamped.
			if (OldAttachTarget.AttachType == EAttachType::AT_Carry && World->IsNetMode(NM_Client) && ThirdPersonSpringArmComponent && ThirdPersonCameraComponent)
			{
				ThirdPersonSpringArmComponent->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
				ThirdPersonSpringArmComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::KeepRelativeTransform);

				ThirdPersonSpringArmComponent->bUsePawnControlRotation = true;
				ThirdPersonCameraComponent->bUsePawnControlRotation = true;

				ThirdPersonSpringArmComponent->SetRelativeTransform(CachedThirdPersonSpringArmTransform);

				// Calling this to clamp DesireSpringArmLength between its new min and max values.
				SetDesireSpringArmLength(DesireSpringArmLength);
			}

			OldAttachCharacter->OnOtherDetached.Broadcast(this, OldAttachTarget.AttachType);
		}
		OnDetached.Broadcast(OldAttachCharacter);
	}
}

void AIBaseCharacter::EnableCollisionAfterDelay()
{
	if (!IsAlive())
	{
		return;
	}
	
	SetCollisionResponseProfilesToDefault();
}

bool AIBaseCharacter::SetAttachTarget(USceneComponent* InAttachComp, FName AttachSocket, EAttachType AttachType /*= EAttachType::AT_Default*/)
{
	if (!ensureAlways(HasAuthority()))
	{
		return false;
	}
	
	if (ICarryInterface::Execute_IsCarried(this) && AttachType == EAttachType::AT_Carry)
	{
		UE_LOG(LogIBaseCharacter, Log, TEXT("AIBaseCharacter::SetAttachTarget: Attach Failed. Already carried."));
		return false;
	}

	if (!IsValid(InAttachComp))
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Invalid attach component."));
		ClearAttachTarget();
		return false;
	}

	if (HasDinosLatched())
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Something is attached to the attaching dino."));
		ClearAttachTarget();
		return false;
	}

	AIBaseCharacter* const TargetOwner = Cast<AIBaseCharacter>(InAttachComp->GetOwner());

	if (!IsValid(TargetOwner))
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Invalid target owner."));
		ClearAttachTarget();
		return false;
	}

	if (TargetOwner->GetAttachParentActor())
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Target has an attach parent."));
		ClearAttachTarget();
		return false;
	}

	if (!InAttachComp->DoesSocketExist(AttachSocket))
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Target socket does not exist."));
		ClearAttachTarget();
		return false;
	}

	if (TargetOwner->AttachedCharacters.Contains(AttachSocket))
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Target socket is occupied."));
		ClearAttachTarget();
		return false;
	}

	switch (AttachType)
	{
	case EAttachType::AT_Latch:
	{
		// This switch case is empty because at one point if a dino latched a dino carrying another it would force them to drop it.
		// Design changed the logic to keep the dino carrying but if we remove this case the latching dino would perform
		// the default behaviour and not latch onto it's target.
		break;
	}
	case EAttachType::AT_Carry:
	{
		ClearAttachedCharacters();
		break;
	}
	default:
	{
		UE_LOG(LogIBaseCharacter, Error, TEXT("Attach Failed: Invalid attach type (%d)."), static_cast<int32>(AttachType));
		ClearAttachTarget();
		return false;
	}
	}

	FAttachTarget& MutableAttachTarget = GetAttachTarget_Mutable();
	const FAttachTarget OldAttachTarget = MutableAttachTarget;

	MutableAttachTarget = FAttachTarget(InAttachComp, AttachSocket, AttachType);
	OnRep_AttachTarget(OldAttachTarget);
	return true;
}

void AIBaseCharacter::ClearAttachTarget()
{
	if (!ensureAlways(HasAuthority()))
	{
		return;
	}

	FAttachTarget& MutableAttachTarget = GetAttachTarget_Mutable();
	const FAttachTarget OldAttachTarget = MutableAttachTarget;

	MutableAttachTarget.Invalidate();
	OnRep_AttachTarget(OldAttachTarget);
}

void AIBaseCharacter::ServerRequestClearAttachTarget_Implementation()
{
	if (IsLatched())
	{
		ClearAttachTarget();
	}
}

void AIBaseCharacter::PerformRepositionIfObstructed(const UWorld* const World, const FVector& NewPosition)
{
	if (World->IsNetMode(NM_Client))
	{
		SetActorLocation(NewPosition, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	const UICharacterMovementComponent* const CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());

	if (!ensureAlwaysMsgf(CharMove, TEXT("AIBaseCharacter::RepositionIfObstructed: GetCharacterMovement() is not of type UICharacterMovementComponent, or is null.")))
	{
		return;
	}

	const AGameStateBase* const GameStateBase = World->GetGameState();

	if (!ensureAlwaysMsgf(GameStateBase, TEXT("AIBaseCharacter::RepositionIfObstructed: World->GetGameState is null.")))
	{
		return;
	}

	CharMove->GetPredictionData_Server_ICharacter()->bForceNextClientAdjustment = true;

	LastRepositionIfObstructedTime = GameStateBase->GetServerWorldTimeSeconds();

	SetActorLocation(NewPosition, false, nullptr, ETeleportType::TeleportPhysics);
}

void AIBaseCharacter::SetEnableDetachGroundTraces(const bool bEnable)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bEnableDetachGroundTraces, bEnable, this);
}

void AIBaseCharacter::SetEnableDetachFallDamageCheck(const bool bEnable)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bEnableDetachFallDamageCheck, bEnable, this);
}

FAttachTarget& AIBaseCharacter::GetAttachTarget_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, AttachTargetInternal, this);
	return AttachTargetInternal;
}

void AIBaseCharacter::SetAttachTarget(const FAttachTarget& NewAttachTarget)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, AttachTargetInternal, NewAttachTarget, this);
}

void AIBaseCharacter::ClearAttachedCharacters()
{
	TArray<FName> Keys{};
	AttachedCharacters.GetKeys(Keys);
	for (const FName& Key : Keys)
	{
		AIBaseCharacter* const AttachedCharacter = AttachedCharacters[Key];
		
		if (!IsValid(AttachedCharacter))
		{
			AttachedCharacters.Remove(Key);
			continue;
		}

		if (AttachedCharacter->GetAttachTarget().IsValid())
		{
			AttachedCharacter->ClearAttachTarget();
		}
	}
}

AIBaseCharacter* AIBaseCharacter::GetCarriedCharacter() const
{
	const TWeakObjectPtr<UObject> CarriedObject = GetCurrentlyCarriedObject().Object;

	if (!CarriedObject.IsValid())
	{
		return nullptr;
	}

	return Cast<AIBaseCharacter>(CarriedObject);
}

void AIBaseCharacter::ClearCarriedCharacter()
{
	TArray<FName> Keys{};
	AttachedCharacters.GetKeys(Keys);

	for (const FName& Key : Keys)
	{
		AIBaseCharacter* const AttachedCharacter = AttachedCharacters[Key];

		if (!IsValid(AttachedCharacter))
		{
			AttachedCharacters.Remove(Key);
			continue;
		}

		if (AttachedCharacter->GetAttachTarget().IsValid() && AttachedCharacter->GetAttachTarget().AttachType == EAttachType::AT_Carry)
		{
			AttachedCharacter->ClearAttachTarget();
			return;
		}
	}
}

bool AIBaseCharacter::HasDinosLatched() const
{
	bool bOutHasLatched = false;
	ForEachAttachedActors([&bOutHasLatched](AActor* AttachedActor)
		{
			const AIBaseCharacter* const AttachedCharacter = Cast<AIBaseCharacter>(AttachedActor);
			if (!AttachedCharacter)
			{
				return true;
			}

			const FAttachTarget& AttachTarget = AttachedCharacter->GetAttachTarget();
			if (AttachTarget.IsValid() && AttachTarget.AttachType == EAttachType::AT_Latch)
			{
				bOutHasLatched = true;
				return false;
			}

			return true;
		});
	return bOutHasLatched;
}

void AIBaseCharacter::UnlatchAllDinosAndSelf()
{
	if (!HasAuthority())
	{
		return;
	}

	// For death of a dino or teleport of any kind (home cave, admin teleport, etc), we should both remove all dinosaurs attached to the teleported one and also detach the teleported one 
	if (!AttachedCharacters.IsEmpty())
	{
		ClearAttachedCharacters();
	}
	if (GetAttachTarget().IsValid())
	{
		ClearAttachTarget();
	}
}

bool AIBaseCharacter::IsLatched() const
{
	const FAttachTarget& AttachTarget = GetAttachTarget();
	return AttachTarget.IsValid() && AttachTarget.AttachType == EAttachType::AT_Latch;
}

bool AIBaseCharacter::IsAttached() const
{
	const FAttachTarget& AttachTarget = GetAttachTarget();
	return AttachTarget.IsValid();
}

void AIBaseCharacter::OnLatchedStaminaTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	if (NewCount < 1 || !GetAttachTarget().IsValid())
	{
		return;
	}

	ClearAttachTarget();
}

bool AIBaseCharacter::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	bool bParentRelevant = Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
	const FAttachTarget& AttachTarget = GetAttachTarget();
	if (!bParentRelevant && AttachTarget.IsValid())
	{
		const AIBaseCharacter* ConnectionCharacter = nullptr;

		//Double check for attach targets
		if (const AController* Ctrl = Cast<AController>(RealViewer))
		{
			ConnectionCharacter = Ctrl->GetPawn<AIBaseCharacter>();
		}

		if (ConnectionCharacter == nullptr)
		{
			ConnectionCharacter = Cast<AIBaseCharacter>(ViewTarget);
		}

		if (ConnectionCharacter != nullptr)
		{
			bParentRelevant = ConnectionCharacter == AttachTarget.AttachComponent->GetOwner();
		}
	}

	return bParentRelevant;
}

void AIBaseCharacter::RepositionIfObstructed(AIBaseCharacter* const OldAttachCharacter, const FQuat& BaseSweepRotation, const float ZoneRadius, const float CapsuleInflationMultiplier, const bool bSkipInitialSweep)
{
	if (!OldAttachCharacter)
	{
		return;
	}

	const UCapsuleComponent* const Capsule = GetCapsuleComponent();

	if (!ensureAlwaysMsgf(Capsule, TEXT("AIBaseCharacter::RepositionIfObstructed: this->GetCapsuleComponent() is null.")))
	{
		return;
	}

	const UCharacterMovementComponent* const CharMove = GetCharacterMovement();

	if (!ensureAlwaysMsgf(CharMove, TEXT("AIBaseCharacter::RepositionIfObstructed: this->GetCharacterMovement() is null.")))
	{
		return;
	}

	const FCollisionShape CapsuleShape = Capsule->GetCollisionShape();

	const float CapsuleInflation = Capsule->GetScaledCapsuleRadius() * CapsuleInflationMultiplier;
	const FCollisionShape InflatedCapsuleShape = Capsule->GetCollisionShape(CapsuleInflation);

	const FVector OldAttachLocation = GetActorLocation();

	FCollisionQueryParams IgnoreAttachCharacterQueryParams{};
	IgnoreAttachCharacterQueryParams.bTraceComplex = false;
	IgnoreAttachCharacterQueryParams.AddIgnoredActor(this);
	IgnoreAttachCharacterQueryParams.AddIgnoredActor(OldAttachCharacter);

	static const ECollisionChannel SweepChannel = COLLISION_DINOCAPSULE;
	static const ECollisionChannel OverlapChannel = COLLISION_DINOCAPSULE;

	FCollisionQueryParams DetectAttachCharacterQueryParams{};
	DetectAttachCharacterQueryParams.bTraceComplex = false;
	DetectAttachCharacterQueryParams.AddIgnoredActor(this);

	const FVector AttachCharacterLocation = OldAttachCharacter->GetActorLocation();

	const FQuat AttachCharacterQuat = OldAttachCharacter->GetActorQuat();

	const FBoxSphereBounds SelfBounds = GetMesh()->CalcBounds(GetMesh()->GetComponentTransform());

	static const TArray<FVector> GroundTraceOffsetDirections
	{
		FVector::ZeroVector, // First ground trace starts at OldAttachLocation, so no reason to have a direction here.
		FVector::RightVector,
		FVector::LeftVector,
		FVector::BackwardVector,
	};

	const UWorld* const World = GetWorld();

	static const ECollisionChannel GroundTraceChannel = COLLISION_DINOCAPSULE;

	const float AttachCharacterDistanceFromGroundTrace = FMath::Max(ZoneRadius - InflatedCapsuleShape.GetCapsuleRadius(), InflatedCapsuleShape.GetCapsuleRadius());

	TArray<FOverlapResult> Overlaps{};
	TArray<FHitResult> GroundHits{};

	FVector InitialOverlapLocation = OldAttachLocation;
	FCollisionShape InitialOverlapCapsuleShape = InflatedCapsuleShape;
	FCollisionQueryParams InitialOverlapQueryParams = IgnoreAttachCharacterQueryParams;

	if (bEnableDetachGroundTraces)
	{
		if (!bSkipInitialSweep)
		{
			const FVector SweepStart = AttachCharacterLocation;
			const FVector SweepEnd = OldAttachLocation;

			FHitResult InitialTraceHit{};
			World->SweepSingleByChannel(InitialTraceHit, SweepStart, SweepEnd, FQuat::Identity, SweepChannel, CapsuleShape, IgnoreAttachCharacterQueryParams);

			InitialOverlapLocation = InitialTraceHit.bBlockingHit ? InitialTraceHit.Location : OldAttachLocation;
			InitialOverlapCapsuleShape = CapsuleShape;
			InitialOverlapQueryParams = DetectAttachCharacterQueryParams;
		}

		Overlaps.Reset();
		World->OverlapMultiByChannel(Overlaps, InitialOverlapLocation, FQuat::Identity, OverlapChannel, InitialOverlapCapsuleShape, InitialOverlapQueryParams);

		const FOverlapResult* const InitialBlockingOverlap = Overlaps.FindByPredicate([](const FOverlapResult& Overlap)
			{
				return Overlap.bBlockingHit;
			});

		if (!InitialBlockingOverlap)
		{
			return;
		}

		const float InflatedCapsuleRadius = InflatedCapsuleShape.GetCapsuleRadius();

		const FVector HalfBaseGroundTraceVector(0, 0, MaxObstructedGroundTraceHeight);

		static const double BaseOverlapHeightPadding = 10;

		const FVector BaseOverlapOffset(0, 0, InflatedCapsuleShape.GetCapsuleHalfHeight() + BaseOverlapHeightPadding);

		for (int32 DirectionIndex = 0; DirectionIndex < GroundTraceOffsetDirections.Num(); ++DirectionIndex)
		{
			const FVector& RelativeDirection = GroundTraceOffsetDirections[DirectionIndex];

			const FVector DesiredTeleportLocation = (DirectionIndex == 0)
				? OldAttachLocation
				: AttachCharacterLocation + BaseSweepRotation.RotateVector(RelativeDirection * AttachCharacterDistanceFromGroundTrace);

			FVector GroundTraceStart = DesiredTeleportLocation - HalfBaseGroundTraceVector;
			FVector GroundTraceEnd = DesiredTeleportLocation + HalfBaseGroundTraceVector;

			// Using a loop here felt unnecessary. However when I tried removing the loop 
			// and using LineTraceSingleByChannel, but the character was getting stuck 
			// in a wall again when I tested it, so I've left this as-is for now.
			for (int32 GroundTraceIndex = 0; GroundTraceIndex < 2; ++GroundTraceIndex)
			{
				GroundHits.Reset();
				World->LineTraceMultiByChannel(GroundHits, GroundTraceStart, GroundTraceEnd, GroundTraceChannel, IgnoreAttachCharacterQueryParams);

				FVector OverlapLocation = DesiredTeleportLocation;

				// Sometimes we can still get non-blocking results.
				GroundHits.RemoveAll([](const FHitResult& GroundHit) -> bool
					{
						return !GroundHit.bBlockingHit;
					});

				if (!GroundHits.IsEmpty())
				{
					const FHitResult& ClosestGroundHit = (GroundTraceIndex == 0) ? GroundHits.Last() : GroundHits[0];

					// On linux, doing the following generates a compiler error:
					// (GroundTraceIndex == 0) ? (-ClosestGroundHit.ImpactNormal) : ClosestGroundHit.ImpactNormal;
					// (-ClosestGroundHit.ImpactNormal) returns FVector, but ClosestGroundHit.ImpactNormal is of type 
					// FVector_NetQuantizeNormal. The ternary operator will fail to compile since both expressions are 
					// technically different types.
					// For this reason I separated ImpactNormal into its own FVector variable.
					const FVector ImpactNormal = ClosestGroundHit.ImpactNormal;
					const FVector Normal = (GroundTraceIndex == 0) ? (-ImpactNormal) : ImpactNormal;

					const double SlopeDot = FVector::DotProduct(FVector::UpVector, Normal);
					const double SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(SlopeDot));

					if (SlopeAngle > CharMove->GetWalkableFloorAngle())
					{
						GroundTraceStart = DesiredTeleportLocation + HalfBaseGroundTraceVector;
						GroundTraceEnd = DesiredTeleportLocation - HalfBaseGroundTraceVector;
						continue;
					}

					// Use the capsule radius and the impact normal to determine how far away to push the overlap capsule 
					// before doing the overlap test. This is to account for situations where the overlap test fails 
					// due to clipping through a slope.

					const double SlopeOffsetMultiplier = 1.0 - SlopeDot;
					const FVector SlopeOffset = Normal * SlopeOffsetMultiplier * InflatedCapsuleRadius;

					OverlapLocation = ClosestGroundHit.ImpactPoint + BaseOverlapOffset + SlopeOffset;
				}

				Overlaps.Reset();
				World->OverlapMultiByChannel(Overlaps, OverlapLocation, FQuat::Identity, OverlapChannel, InflatedCapsuleShape, IgnoreAttachCharacterQueryParams);

				const FOverlapResult* const BlockingOverlap = Overlaps.FindByPredicate([](const FOverlapResult& Overlap)
					{
						return Overlap.bBlockingHit;
					});

				if (!BlockingOverlap)
				{
					static const ECollisionChannel InBetweenTraceChannel = COLLISION_DINOCAPSULE;

					FHitResult InBetweenTraceHit{};
					World->LineTraceSingleByChannel(InBetweenTraceHit, AttachCharacterLocation, OverlapLocation, InBetweenTraceChannel, IgnoreAttachCharacterQueryParams);

					if (!InBetweenTraceHit.bBlockingHit)
					{
						// If DirectionIndex is not 0, we always need to reposition the character since the DesiredTeleportLocation 
						// won't be equal to OldAttachLocation.
						// If the ground trace hit something, it's possible for the dino to get stuck inside of an object unless 
						// we reposition them at InitialOverlapLocation, since it can be different from DesiredTeleportLocation.
						if (DirectionIndex != 0 || !GroundHits.IsEmpty())
						{
							PerformRepositionIfObstructed(World, OverlapLocation);
						}

						return;
					}
				}

				GroundTraceStart = DesiredTeleportLocation + HalfBaseGroundTraceVector;
				GroundTraceEnd = DesiredTeleportLocation - HalfBaseGroundTraceVector;
			}
		}
	}

	const UCapsuleComponent* const AttachCharacterCapsuleComp = OldAttachCharacter->GetCapsuleComponent();
	if (!ensureAlwaysMsgf(AttachCharacterCapsuleComp, TEXT("OldAttachCharacter->GetCapsuleComponent() is null.")))
	{
		return;
	}

	const double DownwardSweepHeight = ObstructedDownwardSweepHeightOffset + CapsuleShape.GetCapsuleHalfHeight();

	const FVector DownwardSweepEnd = AttachCharacterLocation;
	const FVector DownwardSweepStart = DownwardSweepEnd + FVector(0, 0, DownwardSweepHeight);

	TArray<FHitResult> DownwardSweepHits{};
	World->SweepMultiByChannel(DownwardSweepHits, DownwardSweepStart, DownwardSweepEnd, FQuat::Identity, SweepChannel, InflatedCapsuleShape, DetectAttachCharacterQueryParams);

	const double InflatedCapsuleHalfHeight = InflatedCapsuleShape.GetCapsuleHalfHeight();
	const double InflatedCapsuleHeightSq = FMath::Square(InflatedCapsuleHalfHeight * 2);

	// Backwards for loop to iterate over hits closer to the player first, since we want to make the location 
	// change only as big as it needs to be.
	for (int32 Index = DownwardSweepHits.Num() - 1; Index >= 0; Index--)
	{
		const FHitResult& CurrentHit = DownwardSweepHits[Index];

		const bool bNotEnoughSpace = (Index > 0 && (CurrentHit.Location - DownwardSweepHits[Index - 1].Location).SizeSquared() < InflatedCapsuleHeightSq);

		if (bNotEnoughSpace || Cast<ABlockingVolume>(CurrentHit.GetActor()))
		{
			continue;
		}

		static const double HeightOffsetPadding = 10;

		const FVector HeightOffset(0, 0, -InflatedCapsuleHalfHeight + HeightOffsetPadding);

		PerformRepositionIfObstructed(World, CurrentHit.Location + HeightOffset);

		bHighTeleportOnDetach = true;

		return;
	}

	// If all else fails, just place the player under the attach character.
	// This means that the dropped character could get stuck inside of the attach character, but this should be rare.
	// We've tried switching the characters' collision responses to overlapping but had too many issues with that and 
	// there were major concerns regarding unkown edge cases and exploits.

	PerformRepositionIfObstructed(World, AttachCharacterLocation);
}

void AIBaseCharacter::TimelineCallback_Breathe(float val)
{
#if !UE_SERVER

	// Don't let dinosaurs breath when dead
	if (!IsAlive())
	{
		StopBreathing();
		return;
	}

	GetMesh()->SetMorphTarget(NAME_Breathe, val);
#endif
}

void AIBaseCharacter::StopBreathing()
{
	if (BreatheTimeline)
	{
		BreatheTimeline->Stop();
		BreatheTimeline->UnregisterComponent();
		BreatheTimeline->DestroyComponent();
		//BreatheTimeline->ConditionalBeginDestroy();
		BreatheTimeline = nullptr;
	}
}

void AIBaseCharacter::TimelineCallback_Blink(float val)
{
#if !UE_SERVER

	// Don't let dinosaurs blink when dead
	if (!IsAlive())
	{
		StopBlinking(true);
		return;
	}

	float BlinkMultiplier = 1.f;
	if (BlinkGrowthMultiplier != nullptr && UIGameplayStatics::IsGrowthEnabled(this) && !bIsCharacterEditorPreviewCharacter)
	{
		BlinkMultiplier = BlinkGrowthMultiplier->GetFloatValue(GetGrowthPercent());
	}

	if (IsSleeping())
	{
		GetMesh()->SetMorphTarget(NAME_Blink, BlinkMultiplier);
	} else {
		GetMesh()->SetMorphTarget(NAME_Blink, val * BlinkMultiplier);
	}
#endif
}

void AIBaseCharacter::StopBlinking(bool bCloseEyes)
{
	//UE_LOG(TitansCharacter, Warning, TEXT("StopBlinking"));
	if (BlinkTimeline)
	{
		BlinkTimeline->Stop();
		BlinkTimeline->UnregisterComponent();
		BlinkTimeline->DestroyComponent();
		//BlinkTimeline->ConditionalBeginDestroy();
		BlinkTimeline = nullptr;
	}

	if (bCloseEyes)
	{
		GetMesh()->SetMorphTarget(NAME_Blink, 1);
	}
}

void AIBaseCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UAlderonEnhancedInputComponent* const PlayerEnhancedInputComponent = Cast<UAlderonEnhancedInputComponent>(PlayerInputComponent);
	if (!PlayerEnhancedInputComponent)
	{
		return;
	}

	for(int32 Index = 0; Index < AbilityActions.Num(); Index++)
	{
		const UInputAction* InputAction = AbilityActions[Index];
		PlayerEnhancedInputComponent->BindConsumingAction(InputAction, ETriggerEvent::Started, this, &AIBaseCharacter::OnAbilityInputPressed, Index);
		PlayerEnhancedInputComponent->BindAction(InputAction, ETriggerEvent::Completed, this, &AIBaseCharacter::OnAbilityInputReleased, Index);
	}
	
	// Movement
	PlayerEnhancedInputComponent->BindAction(Move, ETriggerEvent::Started, this, &AIBaseCharacter::OnMoveStarted);
	PlayerEnhancedInputComponent->BindAction(Move, ETriggerEvent::Triggered, this, &AIBaseCharacter::OnMoveTriggered);
	PlayerEnhancedInputComponent->BindAction(Move, ETriggerEvent::Completed, this, &AIBaseCharacter::OnMoveCompleted);

	PlayerEnhancedInputComponent->BindAction(LookAround, ETriggerEvent::Triggered, this, &AIBaseCharacter::OnLookAround);

	PlayerEnhancedInputComponent->BindConsumingAction(AutoRun, ETriggerEvent::Started, this, &AIBaseCharacter::InputAutoRunStarted);

	PlayerEnhancedInputComponent->BindConsumingAction(Sprint, ETriggerEvent::Started, this, &AIBaseCharacter::OnStartSprinting);
	PlayerEnhancedInputComponent->BindAction(Sprint, ETriggerEvent::Completed, this, &AIBaseCharacter::OnStopSprinting);

	PlayerEnhancedInputComponent->BindConsumingAction(Trot, ETriggerEvent::Started, this, &AIBaseCharacter::OnStartTroting);
	PlayerEnhancedInputComponent->BindAction(Trot, ETriggerEvent::Completed, this, &AIBaseCharacter::OnStopTroting);

	PlayerEnhancedInputComponent->BindConsumingAction(CrouchAction, ETriggerEvent::Started, this, &AIBaseCharacter::InputCrouchPressed);
	PlayerEnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed, this, &AIBaseCharacter::InputCrouchReleased);

	PlayerEnhancedInputComponent->BindConsumingAction(JumpAction, ETriggerEvent::Started, this, &AIBaseCharacter::OnStartJump);
	PlayerEnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AIBaseCharacter::OnStopJump);

	PlayerEnhancedInputComponent->BindConsumingAction(PreciseMovement, ETriggerEvent::Started, this, &AIBaseCharacter::OnStartPreciseMovement);
	PlayerEnhancedInputComponent->BindAction(PreciseMovement, ETriggerEvent::Completed, this, &AIBaseCharacter::OnStopPreciseMovement);

	// Interaction
	PlayerEnhancedInputComponent->BindConsumingAction(Use, ETriggerEvent::Started, this, &AIBaseCharacter::InputStartUse);
	PlayerEnhancedInputComponent->BindAction(Use, ETriggerEvent::Completed, this, &AIBaseCharacter::InputStopUse);
	PlayerEnhancedInputComponent->BindConsumingAction(Collect, ETriggerEvent::Started, this, &AIBaseCharacter::InputStartCollect);
	PlayerEnhancedInputComponent->BindAction(Collect, ETriggerEvent::Completed, this, &AIBaseCharacter::InputStopCollect);
	PlayerEnhancedInputComponent->BindConsumingAction(Carry, ETriggerEvent::Started, this, &AIBaseCharacter::InputStartCarry);
	PlayerEnhancedInputComponent->BindAction(Carry, ETriggerEvent::Completed, this, &AIBaseCharacter::InputStopCarry);

	// Camera
	PlayerEnhancedInputComponent->BindAction(CameraZoom, ETriggerEvent::Triggered, this, &AIBaseCharacter::OnCameraZoom);

	// Resting
	PlayerEnhancedInputComponent->BindConsumingAction(Resting, ETriggerEvent::Started, this, &AIBaseCharacter::InputRestPressed);
	PlayerEnhancedInputComponent->BindAction(Resting, ETriggerEvent::Completed, this, &AIBaseCharacter::InputRestReleased);

	// Bug Snap
	PlayerEnhancedInputComponent->BindConsumingAction(Bugsnap, ETriggerEvent::Started, this, &AIBaseCharacter::InputStartBugSnap);

	// Feign Limping
	PlayerEnhancedInputComponent->BindConsumingAction(FeignLimping, ETriggerEvent::Started, this, &AIBaseCharacter::InputFeignLimp);

	// Map
	PlayerEnhancedInputComponent->BindConsumingAction(Map, ETriggerEvent::Started, this, &AIBaseCharacter::ToggleMap);

	// Quests
	PlayerEnhancedInputComponent->BindConsumingAction(QuestsAction, ETriggerEvent::Started, this, &AIBaseCharacter::InputQuests);

	// Feign Limping
	PlayerEnhancedInputComponent->BindConsumingAction(ToggleAdminNameTags, ETriggerEvent::Started, this, &AIBaseCharacter::InputToggleNameTags);	
	
}

void AIBaseCharacter::DetachFromControllerPendingDestroy()
{
	// Detach any carried actors before destroying pawn
	if (IsCarryingObject())
	{
		OnInteractReady(true, GetCurrentlyCarriedObject().Object.Get());
	}

	if (HasAuthority())
	{	
		if (AIPlayerState* IPS = Cast<AIPlayerState>(GetPlayerState()))
		{
			SetCombatLogAlderonId(IPS->GetAlderonID());
		}

		if (GetCurrentInstance())
		{
			// Need to remove the player directly
			// as calling ExitInstance() will 
			// reset the logout save information
			GetCurrentInstance()->RemovePlayer(this, false);
		}

		UnlatchAllDinosAndSelf();
	}

	Super::DetachFromControllerPendingDestroy();
}

bool AIBaseCharacter::InputStartBugSnap_Implementation()
{
	if (IsSprinting())
	{
		OnStopSprinting();
	}
	else if (DesiresPreciseMove())
	{
		OnStopPreciseMovement();
	}

	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		IGameHUD->StartBugReport();
		return true;
	}

	return false;
}

void AIBaseCharacter::ActivateAbility(FPrimaryAssetId AbilityId)
{
	if (const FGameplayAbilitySpecHandle* FoundHandle = GetAbilitySpecHandle(AbilityId))
	{
		AbilitySystem->TryActivateAbility(*FoundHandle, true);

		if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(*FoundHandle))
		{
			AbilitySystem->SetAbilityInput(*Spec, true);
		}
	}
}

bool AIBaseCharacter::OnAbilityInputPressed(int32 SlotIndex)
{
	FPrimaryAssetId AbilityId{};
	if (IsInputBlocked() ||
		bIsBasicInputDisabled || 
		bIsAbilityInputDisabled ||
		AbilitySystem == nullptr ||
		!GetSlottedAbilityCategories().IsValidIndex(SlotIndex) ||
		!GetAbilityForSlot(GetSlottedAbilityCategories()[SlotIndex], AbilityId)) 
	{
		return false;
	}

	if (!IsGrowthRequirementMet(GetSlottedAbilityCategories()[SlotIndex]))
	{
		OnAbilityFailedLackingGrowth(AbilityId);
		return false;
	}

	UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
	if (!ensure(IGameUserSettings))
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::OnAbilityInputPressed(): IGameUserSettings nullptr"));
		return false;
	}

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	if (!ensure(IGameInstance))
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::OnAbilityInputPressed(): IGameInstance nullptr"));
		return false;
	}

	if (const FGameplayAbilitySpecHandle* FoundHandle = GetAbilitySpecHandle(AbilityId))
	{
		if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(*FoundHandle))
		{
			if (IGameUserSettings->GetAbilityChargingMode() == EAbilityChargingMode::DoubleTap)
			{
				bool bChargedAbilityIsHeld = false;
				UPOTGameplayAbility* CurrentAbility = AbilitySystem->GetActiveAbilityFromSpec(Spec);
				if (CurrentAbility)
				{
					bChargedAbilityIsHeld = CurrentAbility->IsActive() && CurrentAbility->bShouldUseChargeDuration;
				}

				if (bChargedAbilityIsHeld) // charged abilities are released upon second button press
				{
					Internal_OnAbilityInputReleased(CurrentAbility->GetCurrentAbilitySpecHandle(), true);
				}
				else
				{
					AbilitySystem->TryActivateAbility(*FoundHandle, true);
					AbilitySystem->SetAbilityInput(*Spec, true);
				}
			}
			else
			{
				AbilitySystem->TryActivateAbility(*FoundHandle, true);
				AbilitySystem->SetAbilityInput(*Spec, true);
			}
		}
	}
	
	ServerSubmitGenericTask(NAME_EquipAbility);
	return true;
}

void AIBaseCharacter::Internal_OnAbilityInputReleased(FGameplayAbilitySpecHandle Handle, bool bForced)
{
	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	if (!ensure(IGameInstance))
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::Internal_OnAbilityInputReleased(): IGameInstance nullptr"));
		return;
	}

	UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
	if (!ensure(IGameUserSettings))
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::Internal_OnAbilityInputReleased(): IGameUserSettings nullptr"));
		return;
	}

	if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle))
	{
		if (IGameUserSettings->GetAbilityChargingMode() == EAbilityChargingMode::DoubleTap)
		{
			bool bBlockRelease = false;
			UPOTGameplayAbility* CurrentAbility = AbilitySystem->GetActiveAbilityFromSpec(Spec);
			if (CurrentAbility && !bForced)
			{
				bBlockRelease = CurrentAbility->bShouldUseChargeDuration && CurrentAbility->IsActive();
			}
			if (!bBlockRelease)
			{
				AbilitySystem->SetAbilityInput(*Spec, false);
			}
		}
		else
		{
			AbilitySystem->SetAbilityInput(*Spec, false);
		}
	}
	
}

void AIBaseCharacter::OnAbilityInputReleased(int32 SlotIndex)
{
	if (IsInputBlocked()) return;

	if (AbilitySystem == nullptr)
	{
		return;
	}

	if (!GetSlottedAbilityCategories().IsValidIndex(SlotIndex))
	{
		return;
	}

	FPrimaryAssetId AbilityId;
	if (!GetAbilityForSlot(GetSlottedAbilityCategories()[SlotIndex], AbilityId))
	{
		return;
	}

	if (const FGameplayAbilitySpecHandle* FoundHandle = GetAbilitySpecHandle(AbilityId))
	{
		Internal_OnAbilityInputReleased(*FoundHandle, false);
	}
}

void AIBaseCharacter::ForceAbilityActivate(int32 SlotIndex)
{
	if (IsInputBlocked() || bIsBasicInputDisabled || bIsAbilityInputDisabled) return;

	if (AbilitySystem == nullptr)
	{
		return;
	}

	if (!GetSlottedAbilityCategories().IsValidIndex(SlotIndex))
	{
		return;
	}

	FPrimaryAssetId AbilityId;
	if (!GetAbilityForSlot(GetSlottedAbilityCategories()[SlotIndex], AbilityId))
	{
		return;
	}

	if (!IsGrowthRequirementMet(GetSlottedAbilityCategories()[SlotIndex]))
	{
		OnAbilityFailedLackingGrowth(AbilityId);
		return;
	}

	if (const FGameplayAbilitySpecHandle* FoundHandle = GetAbilitySpecHandle(AbilityId))
	{
		AbilitySystem->TryActivateAbility(*FoundHandle, false);

		if (FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(*FoundHandle))
		{
			AbilitySystem->SetAbilityInput(*Spec, true);
		}
	}
}

void AIBaseCharacter::OnAbilityInputPressed_Enhanced(int32 SlotIndex)
{
	OnAbilityInputPressed(SlotIndex);
}

void AIBaseCharacter::OnAbilityInputReleased_Enhanced(int32 SlotIndex)
{
	OnAbilityInputReleased(SlotIndex);
}

void AIBaseCharacter::ManuallyPressAbility(int32 SlotIndex)
{
	OnAbilityInputPressed(SlotIndex);
}

void AIBaseCharacter::ManuallyReleaseAbility(int32 SlotIndex)
{
	OnAbilityInputReleased(SlotIndex);
}



bool AIBaseCharacter::CanFire() const
{
	return IsAlive();
}

bool AIBaseCharacter::CanAltFire() const
{
	return IsAlive();
}

void AIBaseCharacter::SetTextureParameterValueOnMaterials(const FName ParameterName, UTexture* Texture)
{
	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp == nullptr) return;

	const TArray<UMaterialInterface*> MaterialInterfaces = MeshComp->GetMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialInterfaces.Num(); ++MaterialIndex)
	{
		UMaterialInterface* MaterialInterface = MaterialInterfaces[MaterialIndex];
		if (MaterialInterface)
		{
			UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(MaterialInterface);
			if (!DynamicMaterial)
			{
				DynamicMaterial = MeshComp->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
			}

			DynamicMaterial->SetTextureParameterValue(ParameterName, Texture);
		}
	}
}

void AIBaseCharacter::FaceRotation(FRotator NewControlRotation, float DeltaTime /* = 0.f */)
{
	//Super shouldn't be called here when using MCM.
	if (UICharacterMovementComponent* UCMC = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (UCMC->IsUsingMultiCapsuleCollision())
		{
			return;
		}
	}



	Super::FaceRotation(NewControlRotation, DeltaTime);
}

void AIBaseCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (HasAuthority() && !bIsCharacterEditorPreviewCharacter)
	{
		UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);

		AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
		check(Session);

		//impact damage is already done when flying on impact, so don't apply it twice.
		if (GetCharacterMovement() && Session && Session->bServerFallDamage && !IsFlying())
		{
			bool bIgnoreFallDamageFromHighTeleport = bHighTeleportOnDetach;

			if (bEnableDetachFallDamageCheck && bHighTeleportOnDetach && FMath::Abs(GetActorLocation().Z - LastDetachHeight) > (LastDetachTeleportHeightDelta + DetachFallDamageCheckPadding))
			{
				bIgnoreFallDamageFromHighTeleport = false;
			}

			if (!bIgnoreFallDamageFromHighTeleport)
			{
				BreakLegsFromFalling();
			}
		}
	}

	OnCharacterLanded.Broadcast(Hit);

	bHighTeleportOnDetach = false;
	
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;

	if (!bIsCharacterEditorPreviewCharacter)
	{
		LoadLandedMontage();
	}
#endif
}

void AIBaseCharacter::DamageImpact(const FHitResult& Hit)
{
	FGameplayTagContainer ActiveTags;
	AbilitySystem->GetAggregatedTags(ActiveTags);
	if (ActiveTags.HasTag(FGameplayTag::RequestGameplayTag(NAME_BuffInHomecave)))
	{
		// Don't take impact damage if we are in a HomeCave
		return;
	}

	if (HasAuthority())
	{
		if (UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
		{
			float ImpactSpeed = CharMove->Velocity.Size();

			if (IsSwimming())
			{
				//Makes it so that normal aquatic dinos swimming don't take damage when they hit stuff at highspeeds.
				//Only flying dinos that dive into the water and hit something at high speeds will take damage.
				if (ImpactSpeed <= CharMove->GetMaxSpeed() + CharMove->MaxSpeedThresholdImpact)
				{
					return;
				}
			}

			AIBaseCharacter* const OtherChar = Cast<AIBaseCharacter>(Hit.GetActor());
			UICharacterMovementComponent* const OtherCharMove = IsValid(OtherChar) ? Cast<UICharacterMovementComponent>(OtherChar->GetCharacterMovement()) : nullptr;

			if (IsValid(OtherCharMove))
			{
				if (OtherChar->LastAttachCharacter == this)
				{
					//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DamageImpact: OtherChar->LastAttachChar == this, no damage dealt"));
					return;
				}

				// Colliding with another player when moving towards each other increases damage.
				// Colliding with another player when moving parallel reduces damage.
				ImpactSpeed = (-OtherCharMove->Velocity + CharMove->Velocity).Size();
				//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DamageImpact ImpactSpeed %f"), ImpactSpeed);
			}

			float DamageRatio = FMath::Clamp(ImpactSpeed / CharMove->MaxImpactSpeed, 0.f, 1.f);
			float MaxHealth = GetMaxHealth();
			float ActualDamage = MaxHealth * DamageRatio;

			if (CharMove->bCanTakeImpactDamage)
			{
				if (!IsValid(OtherChar))
				{
					// Head on collisions with the environment scale in damage
					const float Dot = FMath::Clamp(GetActorForwardVector().Dot(-Hit.Normal), 0.f, 1.f);
					ActualDamage *= Dot;
				}

				float HealthRatio = ActualDamage / MaxHealth;

				if (HealthRatio > 0.1f && !bImpactInvinsible)
				{
					UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DamageImpact ActualDamage %f"), ActualDamage);
					UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this,
						UCoreAttributeSet::GetIncomingDamageAttribute(),
						ActualDamage, EGameplayModOp::Additive, HealthRatio >= 0.75f ? EDamageType::DT_BREAKLEGS : EDamageType::DT_ATTACK);

					ApplyShortImpactInvinsibility();
				}

				if (HealthRatio < 1.0f)
				{
					// Don't turn in place with a broken leg
					bDesiresTurnInPlace = false;

					if (AbilitySystem->LegDamageEffect && AbilitySystem->LegDamageEffect->IsValidLowLevel() && (HasAuthority() || GetLocalRole() == ROLE_None))
					{
						//float BoneBreakChance = (FMath::Clamp(DamageRatio - 0.5f, 0.f, 0.5f)) / 0.5f;
						if (HealthRatio >= 0.75f /*FMath::FRand() <= BoneBreakChance*/)
						{
							FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
							AbilitySystem->ApplyGameplayEffectToSelf(AbilitySystem->LegDamageEffect->GetDefaultObject<UGameplayEffect>(), 1, ContextHandle);
						}
					}
				}
				//If the ground dot is above 0.5f (ground is less than 45 degree steepness), set to falling if flying.
				//float GroundDot = Hit.Normal.Dot(FVector(0.0f, 0.0f, 1.0f));

				//if (IsFlying() && (HealthRatio > 0.1f || (GroundDot > 0.5f && !OtherChar))) {
				//	CharMove->SetMovementMode(MOVE_Falling);
				//}
			}

			// If another character was hit
			if (IsValid(OtherChar) && IsValid(OtherCharMove) && OtherCharMove->bCanTakeImpactDamage && !OtherChar->IsImpactInvinsible() && !ActiveTags.HasTag(FGameplayTag::RequestGameplayTag(NAME_BuffHomecaveExitSafety)))
			{
				UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DamageImpact (OtherChar) ActualDamage %f"), ActualDamage * NosediveDamageMultiplier);

				UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(OtherChar, UCoreAttributeSet::GetIncomingDamageAttribute(), ActualDamage * NosediveDamageMultiplier, EGameplayModOp::Additive, EDamageType::DT_ATTACK);

				OtherChar->ApplyShortImpactInvinsibility();
			}
		}
	}
}

void AIBaseCharacter::ApplyShortImpactInvinsibility()
{
	bImpactInvinsible = true;
}

bool AIBaseCharacter::IsImpactInvinsible()
{
	return bImpactInvinsible;
}

void AIBaseCharacter::UpdateLimpingState()
{
	// Don't Process Logic If You Can't Limp At All
	if (!bCanEverLimp)
	{
		SetIsLimping(false);
		return;
	}

	bool NewLimpingState = false;

	float HealthPercent = (GetHealth() / GetMaxHealth());
	if (HealthPercent <= GetLimpThreshold())
	{
		NewLimpingState = true;
	}
	else if (IsFeignLimping())
	{
		NewLimpingState = true;
	}
	else if (AreLegsBroken())
	{
		NewLimpingState = true;
	}

	SetIsLimping(NewLimpingState);
}

void AIBaseCharacter::SetIsLimping(bool NewLimping)
{
	if (IsLimpingRaw() != NewLimping)
	{
		bIsLimping = NewLimping;
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, bIsLimping, this);
		UpdateMovementState();
	}
}

bool AIBaseCharacter::CanFeignLimp()
{
	return bCanEverLimp && !AreLegsBroken() && GetCharacterMovement() && !GetCharacterMovement()->IsSwimming();
}

bool AIBaseCharacter::InputFeignLimp()
{
	if (IsInputBlocked() ||
		UIChatWindow::StaticUserTypingInChat(this) ||
		UIAdminPanelMain::StaticAdminPanelActive(this)) 
	{
		return false;
	}

	const bool bToggleState = !IsFeignLimping();
	if (bToggleState)
	{
		// Ensure Player can actually feign limp if they are trying to toggle it on
		if (!CanFeignLimp()) 
		{
			return false;
		}
	}
	SetIsFeignLimping(bToggleState);
	return true;
}

void AIBaseCharacter::SetIsFeignLimping(bool NewLimping)
{
	if (IsFeignLimping() != NewLimping)
	{
		bIsFeignLimping = NewLimping;
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, bIsFeignLimping, this);
		UpdateMovementState();
	}
}

void AIBaseCharacter::ServerSetFeignLimping_Implementation(bool NewLimping)
{
	SetIsFeignLimping(NewLimping);
}

void AIBaseCharacter::BreakLegsFromFalling()
{
	if (AbilitySystem == nullptr) return;

	if (TeleportFallDamageImmunitySeconds > 0.0f && LastTeleportTime > GetWorld()->GetTimeSeconds() - TeleportFallDamageImmunitySeconds)
	{
		return;
	}

	if (GetCurrentInstance())
	{
		return;
	}

	if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
	{
		float ImpactSpeed = FMath::Abs(CharMove->Velocity.Z);
		float DeathThresholdSpeed = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetFallDeathSpeedAttribute());

		float DamageRatio = (ImpactSpeed - (DeathThresholdSpeed * 0.5f)) / (DeathThresholdSpeed * 0.5f);

		if (DamageRatio > 0.f)
		{

			float ActualDamage = GetMaxHealth() * DamageRatio;
			UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetIncomingDamageAttribute(),
				ActualDamage, EGameplayModOp::Additive, EDamageType::DT_BREAKLEGS);
			
			if (DamageRatio < 1.f)
			{
				// Don't turn in place with a broken leg
				bDesiresTurnInPlace = false;

				

				if (AbilitySystem->LegDamageEffect && AbilitySystem->LegDamageEffect->IsValidLowLevel() && (HasAuthority() || GetLocalRole() == ROLE_None))
				{
					//float BoneBreakChance = (FMath::Clamp(DamageRatio - 0.5f, 0.f, 0.5f)) / 0.5f;
					if (DamageRatio >= 0.75f /*FMath::FRand() <= BoneBreakChance*/)
					{
						FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
						AbilitySystem->ApplyGameplayEffectToSelf(AbilitySystem->LegDamageEffect->GetDefaultObject<UGameplayEffect>(), 1, ContextHandle);
					}
				}
			}
		}
	}

	
}

float AIBaseCharacter::GetBleedingRate() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetBleedingRateAttribute());
}

float AIBaseCharacter::GetPoisonRate() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetPoisonRateAttribute());
}

float AIBaseCharacter::GetVenomRate() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetVenomRateAttribute());
}

bool AIBaseCharacter::CanRest_Implementation()
{
	UCharacterMovementComponent* CharMove = GetCharacterMovement();

	//Dino can't rest unless no movement is being held
	FVector movement = CharMove ? CharMove->GetLastUpdateVelocity() : FVector::ZeroVector;

	float movementThreshold = IsSwimming() ? RestUnderwaterMovementTolerance : RestMovementTolerance;

	bool isFalling = CharMove && CharMove->IsFalling();

	return bCanEverRest && !IsEatingOrDrinking() && CanSleepOrRestInWater(50.0f) && movement.IsNearlyZero(movementThreshold) && !IsFlying() && !isFalling && !IsAttached();
}

bool AIBaseCharacter::InputRestPressed()
{
	if (IsInputBlocked(false, true) || UIChatWindow::StaticUserTypingInChat(this) || bCarryInProgress) return false;

	bRestButtonHeld = true;
	RestHoldTime = 0.0f;

	return true;
}

void AIBaseCharacter::InputRestReleased()
{
	if (IsInputBlocked(false, true) || UIChatWindow::StaticUserTypingInChat(this) || bCarryInProgress) return;

	bRestButtonHeld = false;

	// Check if we held the key for a short enough time for a single press
	if (AIPlayerController * AController = Cast<AIPlayerController>(GetController()))
	{
		if (RestHoldTime < RestHoldThreshold)
		{
			if (GetRestingStance() == EStanceType::Default && CanRest())
			{
				SetIsResting(EStanceType::Resting);
			}
			else if (GetRestingStance() == EStanceType::Resting)
			{
				SetIsResting(EStanceType::Default);
			}
			else if (GetRestingStance() == EStanceType::Sleeping)
			{
				SetIsResting(EStanceType::Resting);
			}

			OnStopVocalWheel();
		}
	}

	if (IsSleeping())
	{
		AIPlayerController* IPlayerController = GetController<AIPlayerController>();
		check(IPlayerController);
		
		if (UIGameplayStatics::IsInGameWaystoneAllowed(this) && IPlayerController && IPlayerController->HasWaystoneInviteEffect(false) && !IsInCombat())
		{
			ClientShowWaystonePrompt_Implementation();
		}
	}

	UpdateMovementState();
}

void AIBaseCharacter::SetIsResting(EStanceType NewStance)
{
	if (GetRestingStance() == NewStance) return;

	if (AbilitySystem != nullptr && AbilitySystem->GetCurrentAttackAbility() != nullptr) return;

	if (UICharacterMovementComponent* UICC = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (NewStance == EStanceType::Resting && !UICC->CanTransitionToState(ECapsuleState::CS_RESTING)) return;
		if (NewStance == EStanceType::Sleeping && !UICC->CanTransitionToState(ECapsuleState::CS_SLEEPING)) return;
	}

	const EStanceType OldStance = GetRestingStance();
	Stance = NewStance;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, Stance, this);
	
	bShouldAllowTailPhysics = (GetRestingStance() == EStanceType::Default);
	ToggleInteractionPrompt(GetRestingStance() == EStanceType::Default && ShouldShowInteractionPrompt());


	if (IsLocallyControlled())
	{
		UnCrouch(true);
		StopUsingIfPossible();
	}

	if (!HasAuthority())
	{
		ServerSetIsResting(NewStance);
	}
	else 
	{
		if (NewStance == EStanceType::Sleeping)
		{
			const FGameplayAbilitySpec* const FoundAbility = AbilitySystem->GetActivatableAbilities().FindByPredicate([](const FGameplayAbilitySpec& Spec) {
				return IsValid(Spec.Ability) && Spec.Ability->IsA<UPOTGameplayAbility_Clamp>();
			});

			if (FoundAbility)
			{
				UPOTGameplayAbility_Clamp* const ClampAbility = Cast<UPOTGameplayAbility_Clamp>(FoundAbility->GetPrimaryInstance());
				if (ClampAbility)
				{
					ClampAbility->CancelClamp();
				}
			}
		}

		if (!HasLeftHatchlingCave())
		{
			if (NewStance == EStanceType::Resting)
			{
				ServerSubmitGenericTask(NAME_StartRest);
			}
		}

		bTransitioningToStance = true;
		if (!GetWorldTimerManager().IsTimerActive(TimerHandle_TransitionCheck))
		{
			GetWorldTimerManager().SetTimer(TimerHandle_TransitionCheck, this, &AIBaseCharacter::TransitionCheck, 0.25, true, 1.0f);
		}

		if (GetRestingStance() != EStanceType::Sleeping)
		{
			if (IsWaystoneInProgress())
			{
				CancelWaystoneInviteCharging();
			}
		}

	}

	OnRestingStateChanged.Broadcast(OldStance, GetRestingStance());
}

float AIBaseCharacter::GetAnimationSequenceLengthScaled(UAnimSequenceBase* AnimSequence)
{
	float RateScaleRatio = 1.0f / AnimSequence->RateScale;
	
	return (AnimSequence->GetPlayLength() * RateScaleRatio);
}

void AIBaseCharacter::ServerSetIsResting_Implementation(EStanceType NewStance)
{
	SetIsResting(NewStance);
}

bool AIBaseCharacter::IsSleeping() const
{
	return (GetRestingStance() == EStanceType::Sleeping);
}

bool AIBaseCharacter::IsRestingOrSleeping() const
{
	return (GetRestingStance() == EStanceType::Resting || GetRestingStance() == EStanceType::Sleeping);
}

TSoftObjectPtr<UAnimMontage> AIBaseCharacter::GetEatAnimMontageFromName(FName FilterName)
{
	if (EatAnimMontages.Num() > 0)
	{
		for (int i = 0; i < EatAnimMontages.Num(); i++)
		{
			if (EatAnimMontages[i].AnimName == FilterName)
			{
				if (IsCarryingObject())
				{
					return IsSwimming() ? EatAnimMontages[i].AnimMontageSwallowSwimSoft : EatAnimMontages[i].AnimMontageSwallowSoft;
				}
				else
				{
					return IsSwimming() ? EatAnimMontages[i].AnimMontageSwimSoft : EatAnimMontages[i].AnimMontageSoft;
				}
			}
		}

		if (IsCarryingObject())
		{
			return IsSwimming() ? EatAnimMontages[0].AnimMontageSwallowSwimSoft : EatAnimMontages[0].AnimMontageSwallowSoft;
		}
		else
		{
			return IsSwimming() ? EatAnimMontages[0].AnimMontageSwimSoft : EatAnimMontages[0].AnimMontageSoft;
		}
	}

	return nullptr;
}

void AIBaseCharacter::OnRep_SlottedActiveAndPassiveEffects()
{
	OnAbilitySlotsChanged.Broadcast();
}

bool AIBaseCharacter::CanTakeQuestItem(AIBaseCharacter* User)
{ 
	if (User)
	{
		AIPlayerState* IPS = Cast<AIPlayerState>(User->GetPlayerState());
		if (IPS)
		{
			if (IPS->GetAlderonID() == GetCombatLogAlderonId()) return false;
			if (IPS->GetAlderonID() == GetKillerAlderonId())
			{
				if (User->GetCharacterID() != GetKillerCharacterId())
				{
					return false;
				}
			}
		}

		AIQuestManager* QuestMgr = AIWorldSettings::GetWorldSettings(this)->QuestManager;

		if (!QuestMgr || QuestMgr->HasTrophyQuestCooldown(User) || !QuestMgr->ShouldEnableTrophyQuests()) return false;
	}

	return !HasSpawnedQuestItem() && GetGrowthPercent() >= 0.9f && QuestItemClass.ToSoftObjectPath().IsValid();
}

bool AIBaseCharacter::UseBlocked_Implementation(UObject* ObjectToUse, EUseType UseType)
{
	if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (ICharMove->IsFalling() || IsEatingOrDrinking() || IsResting() || IsAttacking())
		{
			return true;
		}

		ECustomMovementType MovementType = GetCustomMovementType();
		if (MovementType == ECustomMovementType::NONE) return true;

		if (ICharMove->IsSwimming()) {
			return !CanUseWhileSwimming(ObjectToUse, UseType);
		}
	}

	return false;
}

bool AIBaseCharacter::CanUseWhileSwimming_Implementation(UObject* ObjectToUse, EUseType UseType)
{
	if (UseType == EUseType::UT_CARRY || UseType == EUseType::UT_SECONDARY) return true;
	if (UseType == EUseType::UT_PRIMARY && bAllowEatWhileSwimming) return true;

	return false;
}

void AIBaseCharacter::InitFoodComponents()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;

	if (!FoodSkeletalMeshComponent)
	{
		FoodSkeletalMeshComponent = NewObject<USkeletalMeshComponent>(this, USkeletalMeshComponent::StaticClass(), TEXT("FoodSkeletalMeshComponent"));
		FoodSkeletalMeshComponent->SetupAttachment(GetMesh(), NAME_JawSocket);
		FoodSkeletalMeshComponent->RegisterComponent();
		FoodSkeletalMeshComponent->SetGenerateOverlapEvents(false);
		FoodSkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (!FoodStaticMeshComponent)
	{
		FoodStaticMeshComponent = NewObject<UStaticMeshComponent>(this, UStaticMeshComponent::StaticClass(), TEXT("FoodStaticMeshComponent"));
		FoodStaticMeshComponent->SetupAttachment(GetMesh(), NAME_JawSocket);
		FoodStaticMeshComponent->RegisterComponent();
		FoodStaticMeshComponent->SetGenerateOverlapEvents(false);
		FoodStaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
#endif
}

bool AIBaseCharacter::IsObjectInFocusDistance(UObject* ObjectToUse, int32 Item)
{
	if (!ObjectExistsAndImplementsUsable(ObjectToUse)) 
	{
		return false;
	}
	
	FVector UseLocation = IUsableInterface::Execute_GetUseLocation(ObjectToUse, this, Item);
	float DistanceToUseLocation = (UseLocation - GetMesh()->GetSocketLocation(HeadBoneName)).Size();
	float MaxDistance = GetGrowthFocusTargetDistance() * 2.0f;
	float BoxDiagonalLength = 0.0f;

	AActor* UsableActor = Cast<AActor>(ObjectToUse);
	if (!UsableActor)
	{
		if (UActorComponent* ActorComponent = Cast<UActorComponent>(ObjectToUse))
		{
			UsableActor = ActorComponent->GetOwner();
		}
	}
	if (UsableActor)
	{
		FVector Center, Extent;
		UsableActor->GetActorBounds(false, Center, Extent);		
		if (Extent == FVector::ZeroVector)
		{			
			if (AIDeliveryPoint* IDeliveryPoint = Cast<AIDeliveryPoint>(UsableActor))
			{
				Extent = IDeliveryPoint->CachedExtent;
			}
			else if (AIFish* IFish = Cast<AIFish>(UsableActor))
			{
				Extent = IFish->CachedExtent;
			}
			else if (AIMeatChunk* IMeat = Cast<AIMeatChunk>(UsableActor))
			{
				Extent = IMeat->CachedExtent;
			}
			else if (AIBaseItem* IBaseItem = Cast<AIBaseItem>(UsableActor))
			{
				Extent = IBaseItem->CachedExtent;
			}
			else
			{
				// Fallback 
				Extent = FVector(2500, 2500, 2500);
			}
		}
		// Multiply Extent by 2 to get the full width,length,height of the bounding box
		BoxDiagonalLength = UIGameplayStatics::GetBoxDiagonalLength(Extent * 2);
	}
	bool bWithinUsableDistance = DistanceToUseLocation < (MaxDistance + (BoxDiagonalLength / 2.0f));
	return bWithinUsableDistance;
}

bool AIBaseCharacter::IsFishInKillDistance(AIFish* Fish)
{
	if (!Fish) 
	{
		return false;
	}

	FVector UseLocation = IUsableInterface::Execute_GetUseLocation(Fish, this, 0);
	float DistanceToUseLocation = (UseLocation - GetActorLocation()).Size();

	FVector ActorCenter, ActorExtent;
	GetActorBounds(true, ActorCenter, ActorExtent);
	
	float MaxDistance = UIGameplayStatics::GetBoxDiagonalLength(ActorExtent * 2);
	float BoxDiagonalLength = 0.0f;

	FVector Extent = Fish->CachedExtent;
	
	// Multiply Extent by 2 to get the full width,length,height of the bounding box
	BoxDiagonalLength = UIGameplayStatics::GetBoxDiagonalLength(Extent * 2);
	
	bool bWithinUsableDistance = DistanceToUseLocation < (MaxDistance + (BoxDiagonalLength / 2.0f));
	return bWithinUsableDistance;
}

bool AIBaseCharacter::TryUseObject(UObject* ObjectToUse, int32 Item, EUseType UseType)
{
	// Validate that object exists and is usable
	if (!ObjectExistsAndImplementsUsable(ObjectToUse)) return false;

	// Simulated proxies can start using another actor as they only do cosmetic work
	if (GetLocalRole() != ROLE_SimulatedProxy && (IsUsingObject() || UseBlocked(ObjectToUse, UseType))) return false;

	const bool bRemoteCharacter = GetLocalRole() == ROLE_SimulatedProxy;
	bool bWithinUsableDistance = IsObjectInFocusDistance(ObjectToUse, Item);

	const AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	if (IGameHUD && IGameHUD->bVocalWindowActive) return false;

	if (bRemoteCharacter || bWithinUsableDistance)
	{
		if (IsUsingObject())
		{
			StopAllInteractions();
		}

		if (UseType == EUseType::UT_PRIMARY)
		{
			if (IUsableInterface::Execute_CanPrimaryBeUsedBy(ObjectToUse, this) || bRemoteCharacter)
			{
				if (IUsableInterface::Execute_UsePrimary(ObjectToUse, this, Item) || bRemoteCharacter)
				{
					StartUsing(ObjectToUse, Item, UseType);
					return true;
				}
			}
		}
		else if (UseType == EUseType::UT_SECONDARY)
		{
			if (IUsableInterface::Execute_CanSecondaryBeUsedBy(ObjectToUse, this) || bRemoteCharacter)
			{
				if (IUsableInterface::Execute_UseSecondary(ObjectToUse, this, Item) || bRemoteCharacter)
				{
					StartUsing(ObjectToUse, Item, UseType);
					return true;
				}
			}
		}
		else if (UseType == EUseType::UT_CARRY)
		{
			if (IUsableInterface::Execute_CanCarryBeUsedBy(ObjectToUse, this) || bRemoteCharacter)
			{
				if (IUsableInterface::Execute_UseCarry(ObjectToUse, this, Item) || bRemoteCharacter)
				{
					StartUsing(ObjectToUse, Item, UseType);
					return true;
				}
			}
		}
	}

	return false;
}

void AIBaseCharacter::ServerUseObject_Implementation(UObject* ObjectToUse, int32 Item, float ClientTimeStamp, EUseType UseType)
{
	if (!TryUseObject(ObjectToUse, Item, UseType))
	{
		ClientCancelUseObject(ClientTimeStamp);
	}
}

void AIBaseCharacter::ServerCancelUseObject_Implementation(float ClientTimeStamp)
{
	StopAllInteractions();
}

void AIBaseCharacter::ServerStartDrinkingWaterFromLandscape_Implementation()
{
	StartDrinkingWaterFromLandscape();
}

void AIBaseCharacter::ServerStopDrinkingWaterFromLandscape_Implementation()
{
	StopDrinkingWater();
}

void AIBaseCharacter::ClientCancelUseObject_Implementation(float ClientTimeStamp, bool bForce)
{
	if (LastUseActorClientTimeStamp == ClientTimeStamp || bForce)
	{
		if (NativeGetCurrentlyUsedObject().Object.IsValid())
		{
			IUsableInterface::Execute_OnClientCancelUsePrimaryCorrection(NativeGetCurrentlyUsedObject().Object.Get(), this);
		}

		StopAllInteractions(true);
	}

	bCarryInProgress = false;
}

float AIBaseCharacter::GetGrowthFocusTargetDistance() const
{
	return FMath::Clamp(MaxFocusTargetDistance * GetGrowthPercent(), MinFocusTargetDistance, MaxFocusTargetDistance);
}

bool AIBaseCharacter::IsUsingObject()
{
	return NativeGetCurrentlyUsedObject().Object != nullptr;
}

bool AIBaseCharacter::IsAnyMontagePlaying() const
{
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		return GetMesh()->GetAnimInstance()->IsAnyMontagePlaying();
	}
	return false;
}

bool AIBaseCharacter::IsMontagePlaying(UAnimMontage* TargetMontage)
{
	if (TargetMontage && GetMesh() && GetMesh()->GetAnimInstance())
	{
		return GetMesh()->GetAnimInstance()->Montage_IsPlaying(TargetMontage);
	}

	return false;
}

bool AIBaseCharacter::IsMontageNotPlaying(UAnimMontage* TargetMontage)
{
	if (GetMesh() && GetMesh()->GetAnimInstance())
	{
		return GetMesh()->GetAnimInstance()->GetCurrentActiveMontage() != TargetMontage;
	}

	return false;
}

void AIBaseCharacter::StopAllInteractions(bool bSkipEvents)
{
	//#if WITH_EDITOR
	//	GEngine->AddOnScreenDebugMessage(-1, 60.f, FColor::Green, FString::Printf(TEXT("EndUse Server: %i Local: %i Skip Events: %i"), HasAuthority(), IsLocallyControlled(), bSkipEvents));
	//#endif

	if (InteractAnimLoadHandle.IsValid())
	{
		bCarryInProgress = false;
		InteractAnimLoadHandle->CancelHandle();
	}

	OnStopFoodAnimation(IsCarryingObject());

	// Stop our use animation
	if (CurrentUseAnimMontage && CurrentUseAnimMontage != GetPickupAnimMontageSoft())
	{
		//if (bContinousUse)

		//if (IsMontagePlaying(CurrentUseAnimMontage) && !HasAuthority())
		if (false)
		{
			//#if WITH_EDITOR
			//	GEngine->AddOnScreenDebugMessage(-1, 60.f, FColor::Green, TEXT("Montage Loop to End"));
			//#endif
			UAnimInstance* AI = GetMesh()->GetAnimInstance();
			AI->Montage_SetNextSection(NAME_Loop, NAME_End, CurrentUseAnimMontage);
		}
		else
		{
			//#if WITH_EDITOR
			//	GEngine->AddOnScreenDebugMessage(-1, 60.f, FColor::Green, TEXT("Montage Cancel"));
			//#endif

			// Use a temp var so if things ended properly, animmontage = nullptr so we can fix up use handling
			// if required in the montage ended callback
			UAnimMontage* TempUseMontage = CurrentUseAnimMontage;
			CurrentUseAnimMontage = nullptr;
			StopAnimMontage(TempUseMontage);
		}
	}
	else
	{
		//#if WITH_EDITOR
		//	GEngine->AddOnScreenDebugMessage(-1, 60.f, FColor::Red, TEXT("CurrentUseAnimMontage Invalid"));
		//#endif
	}

	// if the used object still exists then fire the use event.
	if (NativeGetCurrentlyUsedObject().Object.IsValid())
	{
		// trigger end use events
		if (!bSkipEvents)
		{
			IUsableInterface::Execute_EndUsedPrimaryBy(NativeGetCurrentlyUsedObject().Object.Get(), this);
		}
	}

	// Hack Fix: Getting Stuck while eating / drinking sometimes
	StopEatingAndDrinking();
	ClearCurrentlyUsedObjects();
	bIsDisturbing = false;

	ToggleInteractionPrompt(ShouldShowInteractionPrompt());
	HighlightFocusedObject();
}

void AIBaseCharacter::StopEatingAndDrinking()
{
	if (bIsDrinking)
	{
		StopDrinkingWater();
	}
	else if(bIsEating)
	{
		StopEating();
	}
}

void AIBaseCharacter::ForceStopEating()
{
	if (HasAuthority())
	{
		if (AIBaseCharacter* IBC = Cast<AIBaseCharacter>(GetCurrentlyUsedObject().Object.Get()))
		{
			IBC->ActivelyEatingBody.Remove(this);
		}
		else if (AICritterPawn* ICP = Cast<AICritterPawn>(GetCurrentlyUsedObject().Object.Get()))
		{
			ICP->ActivelyEatingBody.Remove(this);
		}
		else if (AIFish* IFish = Cast<AIFish>(GetCurrentlyUsedObject().Object.Get()))
		{
			IFish->ActivelyEatingBody.Remove(this);
		}
		else if (AIMeatChunk* IMeatChunk = Cast<AIMeatChunk>(GetCurrentlyUsedObject().Object.Get()))
		{
			IMeatChunk->ActivelyEatingChunk.Remove(this);
		}
	}

	bIsEating = false;
	bIsDisturbing = false;

	if (!IsValid(GetWorld()) || !IsValid(this)) return;
	
	// Tell Local client to stop eating
	ClientCancelUseObject(GetWorld()->TimeSeconds, true);

	// Call stop eating event on the server
	StopAllInteractions(true);

	ResetServerUseTimer();
}

void AIBaseCharacter::ForceStopDrinking()
{
	bIsDrinking = false;

	if (!GetWorld()) return;

	// Tell Local client to stop eating
	ClientCancelUseObject(GetWorld()->TimeSeconds, true);

	// Call stop eating event on the server
	StopAllInteractions(true);

	ResetServerUseTimer();
}

void AIBaseCharacter::ClearCurrentlyUsedObjects()
{
	// clear all set used actors, unlock focal point
	FUsableObjectReference& MutableCurrentlyUsedObject = GetCurrentlyUsedObject_Mutable();
	MutableCurrentlyUsedObject.Object = nullptr;
	MutableCurrentlyUsedObject.Item = INDEX_NONE;
	MutableCurrentlyUsedObject.Timestamp = 0;
	bLockDesiredFocalPoint = false;
	bContinousUse = false;
}

FUsableObjectReference& AIBaseCharacter::GetCurrentlyUsedObject_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, CurrentlyUsedObject, this);
	return CurrentlyUsedObject;
}

const FUsableObjectReference& AIBaseCharacter::GetCurrentlyUsedObject()
{
	return NativeGetCurrentlyUsedObject();
}

void AIBaseCharacter::LoadUseAnim(float UseDuration, TSoftObjectPtr<UAnimMontage> UseAnimSoft, bool bLandscapeWater /*= false*/)
{
	//UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::LoadDamageParticle()"));
	// Async Load the particle
	if (IsAlive())
	{
		if (UseAnimSoft.ToSoftObjectPath().IsValid())
		{
			// Anim already loaded
			if (UseAnimSoft.Get() != nullptr)
			{
				return OnUseAnimLoaded(UseDuration, UseAnimSoft, bLandscapeWater);
			}

			// Start Async Loading
			FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
			InteractAnimLoadHandle = Streamable.RequestAsyncLoad(
				UseAnimSoft.ToSoftObjectPath(),
				FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnUseAnimLoaded, UseDuration, UseAnimSoft, bLandscapeWater),
				FStreamableManager::AsyncLoadHighPriority, false
			);
		}
		else
		{
			// Use Until Canceled
			if (bContinousUse) return;

			// Instant Use Item
			StopAllInteractions();
		}
	}
}

void AIBaseCharacter::OnUseAnimLoaded(float UseDuration, TSoftObjectPtr<UAnimMontage> UseAnimSoft, bool bLandscapeWater)
{
	float MontageDuration = 0.0f;

	if (bLandscapeWater)
	{
		CurrentUseAnimMontage = UseAnimSoft.Get();
		bIsDrinking = true;
		ForceUncrouch();

		if (CurrentUseAnimMontage)
		{
			MontageDuration = PlayAnimMontage(CurrentUseAnimMontage);

			// If we are the server, we skip the first part of the use animation to remain in sync with network lagg.
			if (HasAuthority())
			{
				AIWater* EmptyWater = nullptr;
				// Bind New Delgate
				ServerUseTimerDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::DrinkWater, EmptyWater);

				// Setup Timer to Restore Water Per Tick while Being Used
				GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, WaterConsumedTickRate, true);

				if (const AIPlayerState* PS = GetPlayerState<AIPlayerState>())
				{
					if (MontageDuration > 2.0f)
					{
						const float PingSeconds = FMath::Min(PS->GetPingSeconds(), 0.5f);
						SkipMontageTime(CurrentUseAnimMontage, PingSeconds);
						MontageDuration -= PingSeconds;	
					}
				}
			}
		}

		return;
	}
	if (UAnimMontage* TempRequestedUseAnimMontage = UseAnimSoft.Get())
	{
		CurrentUseAnimMontage = TempRequestedUseAnimMontage;
		MontageDuration = PlayAnimMontage(CurrentUseAnimMontage);

		// If we are the server, we skip the first part of the use animation to remain in sync with network lagg.
		if (HasAuthority())
		{
			if (const AIPlayerState* PS = GetPlayerState<AIPlayerState>())
			{
				if (MontageDuration > 2.0f)
				{
					const float PingSeconds = FMath::Min(PS->GetPingSeconds(), 0.5f);
					SkipMontageTime(CurrentUseAnimMontage, PingSeconds);
					MontageDuration -= PingSeconds;	
				}
			}
		}
	}

	bContinousUse = (static_cast<int32>(UseDuration) == INDEX_NONE);

	// Use Until Canceled
	if (bContinousUse)
	{
		return;
	}

	if (UseDuration > 0.f)
	{
		// Use Item with a Duration
		const FTimerDelegate StopAllInteractionsDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::StopAllInteractions, false);
		GetWorldTimerManager().SetTimer(UseTimerHandle, StopAllInteractionsDelegate, MontageDuration, false);
	}
	else {
		// Instant Use Item
		StopAllInteractions();
	}
}

void AIBaseCharacter::LoadInteractAnim(bool bDrop, UObject* CarriableObject)
{
	//UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::LoadDamageParticle()"));
		// Async Load the particle
	if (IsAlive() && GetPickupAnimMontageSoft().ToSoftObjectPath().IsValid())
	{
		// Anim already loaded
		if (GetPickupAnimMontageSoft().Get() != nullptr)
		{
			return OnInteractAnimLoaded(GetPickupAnimMontageSoft(), bDrop, CarriableObject);
		}

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		InteractAnimLoadHandle = Streamable.RequestAsyncLoad(
			GetPickupAnimMontageSoft().ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnInteractAnimLoaded, GetPickupAnimMontageSoft(), bDrop, CarriableObject),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
}

void AIBaseCharacter::OnInteractAnimLoaded(TSoftObjectPtr<UAnimMontage> InteractAnimSoft, bool bDrop, UObject* CarriableObject)
{
	SetForcePoseUpdateWhenNotRendered(true);
	SetForceBoneUpdateWhenNotRendered(bDrop);

	UAnimInstance* AnimInstance = (GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;
	if (GetPickupAnimMontageSoft().Get() && AnimInstance)
	{
		CurrentUseAnimMontage = GetPickupAnimMontageSoft().Get();
		// Play Montage
		float AnimDuration = AnimInstance->Montage_Play(GetPickupAnimMontageSoft().Get(), bDrop ? -1.0f : 1.0f);

		// Set Starting Position
		if (AnimDuration > 0.f)
		{
			AnimInstance->Montage_SetPosition(CurrentUseAnimMontage, bDrop ? GetPickupAnimMontageSoft().Get()->GetPlayLength() : 0.0f);
		}

		// If we are the server, we skip the first part of the use animation to remain in sync with network lagg.
		if (HasAuthority())
		{
			if (GetCurrentlyCarriedObject().bDrop != bDrop)
			{
				GetCurrentlyCarriedObject_Mutable().bDrop = bDrop;
			}

			if (AIPlayerState* PS = GetPlayerState<AIPlayerState>())
			{
				if (AnimDuration > 2.0f)
				{
					const float PingSeconds = FMath::Min(PS->GetPingSeconds(), 0.5f);
					SkipMontageTime(CurrentUseAnimMontage, PingSeconds);
					AnimDuration -= PingSeconds;	
				}
			}
		}

		if (AnimDuration > 0.f)
		{
			GetWorldTimerManager().SetTimer(TimerHandle_Interact, this, &AIDinosaurCharacter::OnInteractAnimFinished, AnimDuration, false);

			// Temporary Timer is used to call the OnInteractReady function rather than using the AnimNotify_Interact for pick up and drops on items as the notify was not consistent.
			const FTimerDelegate InteractDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::OnInteractReady, bDrop, CarriableObject);
			GetWorldTimerManager().SetTimer(TimerHandle_OnStartInteract, InteractDelegate, AnimDuration * 0.5f, false);
		}
		else
		{
			OnInteractReady(false, CarriableObject);
			OnInteractAnimFinished();
		}
	}
}


void AIBaseCharacter::LoadHurtAnim()
{
	if (IsAlive() && HurtAnimMontage.ToSoftObjectPath().IsValid())
	{
		if (UAnimInstance* AInst = GetMesh()->GetAnimInstance())
		{
			if(AInst->IsAnyMontagePlaying())
			{
				return;
			}
		}
		

		// Anim already loaded
		if (HurtAnimMontage.Get() != nullptr)
		{
			OnHurtAnimLoaded(HurtAnimMontage);
			return;
		}

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		InteractAnimLoadHandle = Streamable.RequestAsyncLoad(
			HurtAnimMontage.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnHurtAnimLoaded, HurtAnimMontage),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
}

void AIBaseCharacter::OnHurtAnimLoaded(TSoftObjectPtr<UAnimMontage> HurtAnimSoft)
{
	UAnimInstance* AnimInstance = (GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;
	if (HurtAnimSoft.Get() && AnimInstance)
	{
		// Play Montage
		float AnimDuration = AnimInstance->Montage_Play(HurtAnimSoft.Get());
	}
}

void AIBaseCharacter::LoadLandedMontage()
{
	if (IsAlive() && LandAnimMontage.ToSoftObjectPath().IsValid())
	{
		if (UAnimInstance* AInst = GetMesh()->GetAnimInstance())
		{
			if (AInst->IsAnyMontagePlaying())
			{
				return;
			}
		}


		// Anim already loaded
		if (LandAnimMontage.Get() != nullptr)
		{
			OnLandedMontageLoaded(LandAnimMontage);
			return;
		}

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		InteractAnimLoadHandle = Streamable.RequestAsyncLoad(
			LandAnimMontage.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnLandedMontageLoaded, LandAnimMontage),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
}

void AIBaseCharacter::OnLandedMontageLoaded(TSoftObjectPtr<UAnimMontage> LandAnimSoft)
{
	UAnimInstance* AnimInstance = (GetMesh()) ? GetMesh()->GetAnimInstance() : nullptr;
	if (LandAnimSoft.Get() && AnimInstance)
	{
		// Play Montage
		float AnimDuration = AnimInstance->Montage_Play(LandAnimSoft.Get());
	}
}

void AIBaseCharacter::StartUsing(UObject* UsedObject, int32 Item, EUseType UseType)
{
	if (UseType == EUseType::UT_CARRY)
	{
		if (IsLocallyControlled())
		{
			bCarryInProgress = true;
		}

		return;
	}

	if (IsValid(UsedObject) && UsedObject->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
	{
		FUsableObjectReference& MutableCurrentlyUsedObject = GetCurrentlyUsedObject_Mutable();
		MutableCurrentlyUsedObject.Object = UsedObject;
		MutableCurrentlyUsedObject.Item = Item;
		MutableCurrentlyUsedObject.ObjectUseType = UseType;

		bLockDesiredFocalPoint = bContinousUse;

		ToggleInteractionPrompt(false);
		UnHighlightFocusedObject();

		float MontageDuration = 0.0f;
		float UseDuration = 0.0f;
		TSoftObjectPtr<UAnimMontage> TempRequestedUseAnimMontage;

		switch (UseType)
		{
		case EUseType::UT_PRIMARY: 
		{
			UseDuration = IUsableInterface::Execute_GetPrimaryUseDuration(UsedObject, this);
			TempRequestedUseAnimMontage = IUsableInterface::Execute_GetPrimaryUserAnimMontageSoft(UsedObject, this);
			break;
		}
		case EUseType::UT_SECONDARY:
		{
			UseDuration = IUsableInterface::Execute_GetSecondaryUseDuration(UsedObject, this);
			TempRequestedUseAnimMontage = IUsableInterface::Execute_GetSecondaryUserAnimMontageSoft(UsedObject, this);
			break;
		}
		}

		bContinousUse = (UseDuration == 0.0f);

		LoadUseAnim(UseDuration, TempRequestedUseAnimMontage);
	}
}

void AIBaseCharacter::SkipMontageTime(UAnimMontage* Montage, float TimeSkipped)
{
	check(Montage);
	if (!Montage) return;

	USkeletalMeshComponent* SMesh = GetMesh();
	if (!SMesh) return;

	UAnimInstance* IAnimInstance = SMesh->GetAnimInstance();
	if (!IAnimInstance) return;

	FAnimMontageInstance* Inst = IAnimInstance->GetActiveInstanceForMontage(Montage);
	if (!Inst) return;

	float CurrentPlayPosition = Inst->GetPosition();
	float CurrentPlayRate = Inst->GetPlayRate();

	// If play rate is in reverse then skip backward
	if (CurrentPlayRate < 0.0f)
	{
		Inst->SetPosition(CurrentPlayPosition - TimeSkipped);
	}
	// If play rate is forward then skip forward
	else
	{
		Inst->SetPosition(CurrentPlayPosition + TimeSkipped);
	}
}

FVector AIBaseCharacter::GetNormalizedForwardVector()
{
	FVector ForwardNormalized;
	ForwardNormalized = UKismetMathLibrary::GetForwardVector(GetControlRotation());

	bool bAllowPitchControl = false;

	if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		// Only allow pitch control when it's enabled while precise swimming
		bAllowPitchControl = ICharMove->bCanUsePreciseSwimmingPitchControl && IsSwimming() && ICharMove->ShouldUsePreciseMovement() && !ICharMove->IsAtWaterSurface();
	}

	// When pitch control is enabled control rotation Z will almost never be exactly 0.
	// This makes it very difficult to swim straight forward/backward without also moving slightly up or down.
	// To compensate for this clamp Z to 0 when the value falls within a deadzone.
	const float PitchControlDeadZone = 0.1f;

	if (!bAllowPitchControl || FMath::Abs(ForwardNormalized.Z) < PitchControlDeadZone)
	{
		ForwardNormalized.Z = 0;
	}

	ForwardNormalized.Normalize(0.0001f);

	if (ForwardNormalized.Equals(FVector(0.0f, 0.0f, 0.0f), 0.01f))
	{
		ForwardNormalized = UKismetMathLibrary::GetForwardVector(FRotator(0.0f, GetControlRotation().Yaw, 0.0f));
		ForwardNormalized.Normalize(0.0001f);
	}

	return ForwardNormalized;
}

FVector AIBaseCharacter::GetNormalizedRightVector()
{
	FVector RightNormalized = UKismetMathLibrary::GetRightVector(GetControlRotation());
	RightNormalized.Z = 0;

	RightNormalized.Normalize(0.0001f);

	return RightNormalized;
}

void AIBaseCharacter::InputMoveY(float InAxis)
{
	if (fabsf(InAxis) == 0.0f) return;

	if (!CanMove(true))
	{
		return;
	}

	if (fabsf(InAxis) >= 0.1)
	{
		StopUsingIfPossible();
	}

	if (IsRestingOrSleeping())
	{
		InputRestPressed();
		InputRestReleased();
	}
	else
	{
		if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
		{
			FVector MovementDirection = FVector();
			AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);

			if (IGameState && IGameState->IsNewCharacterMovement())
			{
				MovementDirection = GetNormalizedForwardVector();
			}
			else
			{
				MovementDirection = (ICharMove->ShouldUsePreciseMovement() && !ICharMove->bOrientRotationToMovement) ? GetActorForwardVector() : GetNormalizedForwardVector();
			}
			AddMovementInput(MovementDirection, InAxis);
		}
	}
}

void AIBaseCharacter::OnMoveStarted(const FInputActionValue& Value)
{
	bMoveForwardWhileAutoRunning = false;
}

void AIBaseCharacter::OnMoveCompleted(const FInputActionValue& Value)
{
	// Cancel Autorunning if you didn't activate auto run while moving
	if (bAutoRunActive && bMoveForwardPressed && !bMoveForwardWhileAutoRunning)
	{
		ToggleAutoRun();
	}

	bMoveForwardPressed = false;
	bMoveBackwardPressed = false;
}

void AIBaseCharacter::OnMoveTriggered(const FInputActionValue& Value)
{
	const float MagSq = Value.GetMagnitudeSq();

	float ValueY = Value.Get<FVector2D>().Y;

	if (FMath::IsNearlyZero(MagSq)  ||
		Value.GetValueType() != EInputActionValueType::Axis2D)
	{
		return;
	}
	
	if (IsRestingOrSleeping())
	{
		InputRestPressed();
		InputRestReleased();
		return;
	}

	if (!CanMove(true))
	{
		return;
	}

	if (MagSq >= 0.01f)
	{
		StopUsingIfPossible();
		
		if (ValueY > 0.01f)// Check if moving forward
		{
			bMoveForwardPressed = true;
		}
		else if (ValueY < -0.01f)
		{
			bMoveBackwardPressed = true;
		}
	}
		
	UICharacterMovementComponent* const ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	if (!ICharMove)
	{
		return;
	}

	const float X = Value[0];
	const float Y = Value[1];
		
	FVector ForwardMovementDirection = FVector();
	FVector RightMovementDirection = FVector();
	const AIGameState* const IGameState = UIGameplayStatics::GetIGameState(this);

	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		ForwardMovementDirection = GetNormalizedForwardVector();
		RightMovementDirection = GetNormalizedRightVector();
	}
	else
	{
		ForwardMovementDirection = (ICharMove->ShouldUsePreciseMovement() && !ICharMove->bOrientRotationToMovement) ? GetActorForwardVector() : GetNormalizedForwardVector();
		RightMovementDirection = (ICharMove->ShouldUsePreciseMovement() && !ICharMove->bOrientRotationToMovement) ? GetActorRightVector() : GetNormalizedRightVector();
	}
	//UE_LOG(LogTemp, Log, TEXT("Forward: %s (%f) --- Right: %s (%f)"), *ForwardMovementDirection.ToCompactString(), Y, *RightMovementDirection.ToCompactString(), X);
	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	if (IGameInstance->GetInputHardware() == EInputHardware::KeyboardMouse)
	{
		PreviousMovementInput = { X, Y, 0.0f };
		AddMovementInput(ForwardMovementDirection, Y);
		AddMovementInput(RightMovementDirection, X);
	}
	else
	{
		FVector CurrentValues = { X, Y, 0.0f };
		FVector Diff = CurrentValues - PreviousMovementInput;
		FVector SmoothedValues;

		if (Diff.Size() > 0.05) // Blend from previous and current movement values
		{
			SmoothedValues = PreviousMovementInput + Diff * FMath::Clamp(GetWorld()->DeltaRealTimeSeconds * AnalogMovementInputSmoothingSpeed, 0.f, 1.f);
			PreviousMovementInput = SmoothedValues;
		}
		else
		{
			SmoothedValues = PreviousMovementInput; // If change in movement is so small, ignore it
		}

		AddMovementInput(ForwardMovementDirection, SmoothedValues.Y);
		AddMovementInput(RightMovementDirection, SmoothedValues.X);
	}
	
}

void AIBaseCharacter::OnLookAround(const FInputActionValue& Value)
{
	if (UIFullMapWidget::StaticMapActive(this)) return;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	AIPlayerController* IPlayerController = UIGameplayStatics::GetIPlayerController(this);
	check(IPlayerController);

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(IPlayerController->GetHUD());
		
	if (IGameInstance->IsInputHardwareGamepad() && IGameHUD && (IGameHUD->bTabSystemActive || IGameHUD->bVocalWindowActive))
	{
		return;
	}

	const float MagSq = Value.GetMagnitudeSq();

	if (FMath::IsNearlyZero(MagSq)) return;
	
	if (Value.GetValueType() != EInputActionValueType::Axis2D) return;

	if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		const float X = Value[0];
		const float Y = Value[1];

		//UE_LOG(LogTemp, Log, TEXT("MouseX: %f"), X);

		const float SensitivityX = IGameInstance->GetInputHardware() != EInputHardware::KeyboardMouse ? ((IGameUserSettings->DinosaurGamepadSensitivityX * UGameplayStatics::GetWorldDeltaSeconds(this)) * 50.0f) : IGameUserSettings->DinosaurMouseSensitivity;
		const float SensitivityY = IGameInstance->GetInputHardware() != EInputHardware::KeyboardMouse ? ((IGameUserSettings->DinosaurGamepadSensitivityY * UGameplayStatics::GetWorldDeltaSeconds(this)) * 50.0f) : IGameUserSettings->DinosaurMouseSensitivity;

		AddControllerYawInput(IGameUserSettings->GetXAxisOrientation() * (X * SensitivityX));
		AddControllerPitchInput(IGameUserSettings->GetYAxisOrientation() * (Y * SensitivityY));
	}
}

void AIBaseCharacter::InputTurn_Implementation(float InAxis)
{
	if (fabsf(InAxis) == 0.0f) return;

	//UE_LOG(LogTemp, Log, TEXT("MouseX: %f"), InAxis);

	if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);

		float Sensitivity = IGameInstance->GetInputHardware() != EInputHardware::KeyboardMouse ? ((IGameUserSettings->DinosaurGamepadSensitivityX * UGameplayStatics::GetWorldDeltaSeconds(this)) * 50.0f) : IGameUserSettings->DinosaurMouseSensitivity;
		AddControllerYawInput(IGameUserSettings->GetXAxisOrientation() * (InAxis * Sensitivity));
	}
}

void AIBaseCharacter::InputLookUp_Implementation(float InAxis)
{
	if (fabsf(InAxis) == 0.0f) return;

	if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);

		float Sensitivity = IGameInstance->GetInputHardware() != EInputHardware::KeyboardMouse ? ((IGameUserSettings->DinosaurGamepadSensitivityY * UGameplayStatics::GetWorldDeltaSeconds(this)) * 50.0f) : IGameUserSettings->DinosaurMouseSensitivity;
		AddControllerPitchInput(IGameUserSettings->GetYAxisOrientation() * (InAxis * Sensitivity));
	}
}

bool AIBaseCharacter::IsEating() const
{
	return bIsEating;
}

bool AIBaseCharacter::IsDrinking() const
{
	return bIsDrinking;
}

bool AIBaseCharacter::IsEatingOrDrinking() const
{
	return (bIsEating || bIsDrinking);
}

void AIBaseCharacter::EatBody(AIBaseCharacter* Body)
{
	if (!IsValid(this)) return;

	UWorld* World = GetWorld();
	if (!World) return;

	bool bCancelEating = false;

	if (GetController<AIPlayerController>() != nullptr)
	{
		if (Body->IsValidLowLevel() && IsValid(Body->AbilitySystem) && !Body->IsHidden())
		{
			// Play Eating Animation
			bIsEating = true;
			ForceUncrouch();

			// Restore Hunger
			int FoodToConsume = GetFoodConsumePerTick();
			if (FoodToConsume > Body->GetCurrentBodyFood()) { FoodToConsume = Body->GetCurrentBodyFood(); }

			RestoreHunger(FoodToConsume);

			// Decay Body
			Body->RestoreBodyFood(FoodToConsume * -1.0f);
			if (Body->GetCurrentBodyFood() <= 0)
			{
				bCancelEating = true;
			}
			else
			{
				// Two Dinosaurs Eating each other, handle corpse functionality
				AIDinosaurCharacter* DinoSelf = Cast<AIDinosaurCharacter>(this);
				AIDinosaurCharacter* DinoBody = Cast <AIDinosaurCharacter>(Body);
				if (IsValid(DinoSelf) && IsValid(DinoBody))
				{
					bool bSpawnCorpse = DinoBody->BeingEaten(DinoSelf);
					if (bSpawnCorpse)
					{
						bCancelEating = true;
					}
				}
			}
		}
		else
		{
			bCancelEating = true;
		}
	}
	else
	{
		bCancelEating = true;
	}

	if (GetHunger() >= GetMaxHunger())
	{
		bCancelEating = true;
	}

	if (bCancelEating)
	{
		ForceStopEating();
	}
}

void AIBaseCharacter::EatPlant(UIFocusTargetFoliageMeshComponent* TargetPlant)
{
	bool bCancelEating = false;

	if (GetController<AIPlayerController>() != nullptr)
	{
		if (IsValid(TargetPlant) && GetHunger() < GetMaxHunger())
		{
			// Play Drinking Animation
			bIsEating = true;
			ForceUncrouch();

			// Restore Thirst
			RestoreHunger(GetFoodConsumePerTick());
		}
		else
		{
			bCancelEating = true;
		}
	}
	else
	{
		bCancelEating = true;
	}

	if (GetHunger() >= GetMaxHunger())
	{
		bCancelEating = true;
	}

	if (bCancelEating)
	{
		ForceStopEating();
	}
}

void AIBaseCharacter::EatFood(AIFoodItem* Food)
{
	UWorld* World = GetWorld();
	if (!World) return;

	bool bCancelEating = false;

	if (GetController<AIPlayerController>() != nullptr)
	{
		if (IsValid(Food) && !Food->IsHidden())
		{
			// Play Eating Animation
			bIsEating = true;
			ForceUncrouch();

			// Restore Hunger
			int FoodToConsume = GetFoodConsumePerTick();
			if ((FoodToConsume > Food->GetFoodValue()) || Food->IsBeingSwallowed())
			{
				FoodToConsume = Food->GetFoodValue();
			}

			RestoreHunger(FoodToConsume);
			Food->Consumed(this);

			//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::EatFood() - B Food->FoodValue: %f"), Food->FoodValue));

			// Decay Food
			if (!Food->IsInfinite())
			{
				Food->SetFoodValue(FMath::Clamp<int32>(Food->GetFoodValue() - FoodToConsume, 0, Food->GetMaxFoodValue()));
			}

			//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::EatFood() - A Food->FoodValue: %f"), Food->FoodValue));
			//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::EatFood() - FoodToConsume: %s"), *FString::FromInt(FoodToConsume)));
			//GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::EatFood() - (Food->FoodValue - FoodToConsume): %f"), (Food->FoodValue - FoodToConsume)));

			if (Food->GetFoodValue() <= 0 || Food->IsBeingSwallowed())
			{
				bCancelEating = true;
			}
		}
		else
		{
			bCancelEating = true;
		}
	}
	else
	{
		bCancelEating = true;
	}

	if (GetHunger() >= GetMaxHunger())
	{
		bCancelEating = true;
	}

	if (bCancelEating)
	{
		ForceStopEating();
	}
}

void AIBaseCharacter::EatCritter(AICritterPawn* Critter)
{
	UWorld* World = GetWorld();
	if (!World) return;

	bool bCancelEating = false;

	if (GetController<AIPlayerController>() != nullptr)
	{
		if (IsValid(Critter) && !Critter->IsHidden())
		{
			// Play Eating Animation
			bIsEating = true;
			ForceUncrouch();

			// Restore Hunger
			int FoodToConsume = GetFoodConsumePerTick();
			if (FoodToConsume > Critter->GetFoodValue())
			{
				FoodToConsume = Critter->GetFoodValue();
			}

			RestoreHunger(FoodToConsume);
			Critter->Consumed(this);

			// Decay Food
			Critter->SetFoodValue(FMath::Clamp<int32>(Critter->GetFoodValue() - FoodToConsume, 0, Critter->GetMaxFoodValue()));
			Critter->OnRep_FoodValue();

			if (Critter->GetFoodValue() <= 0)
			{
				bCancelEating = true;
			}
		}
		else
		{
			bCancelEating = true;
		}
	}
	else
	{
		bCancelEating = true;
	}

	if (GetHunger() >= GetMaxHunger())
	{
		bCancelEating = true;
	}

	if (bCancelEating)
	{
		ForceStopEating();
	}
}

void AIBaseCharacter::DrinkWater(AIWater* Water)
{
 	bool bCancelDrinking = false;
	float WaterQuality = 1.0f;

	if (Water)
	{
		if (const AIWorldSettings* const WorldSettings = AIWorldSettings::GetWorldSettings(this))
		{
			if (AIWaterManager* const WaterManager = WorldSettings->WaterManager)
			{
				WaterQuality = WaterManager->GetWaterVolume(Water->GetIndentifier());
			}
		}
	}

	//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DrinkWater | Tag: %s - Quality: %f"), *Water->WaterTag.ToString(), WaterQuality);

	if (GetController<AIPlayerController>() != nullptr)
	{
		// Play Drinking Animation
		bIsDrinking = true;
		ForceUncrouch();

		if (Water)
		{
			// Restore Thirst
			float WaterToConsume = GetWaterConsumePerTick() * FMath::Clamp(WaterQuality, 0.5f, 1.0f);

			RestoreThirst(WaterToConsume);

			Water->ModifyWaterQuality(WaterToConsume * -1.0f);
		}
		else
		{
			// Restore Thirst
			RestoreThirst(GetWaterConsumePerTick());
		}

		if (Water)
		{
			Water->Consumed(this);
		}
	}
	else
	{
		bCancelDrinking = true;

		//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DrinkWater | Controller == nullptr"));
	}

	if (GetThirst() >= GetMaxThirst() || bCancelDrinking)
	{
		ForceStopDrinking();
	}
	else if (Water)
	{
		if (const AIWorldSettings* const WorldSettings = AIWorldSettings::GetWorldSettings(this))
		{
			if (AIWaterManager* const WaterManager = WorldSettings->WaterManager)
			{
				WaterQuality = WaterManager->GetWaterQuality(Water->GetIndentifier());
			}
		}

		if (WaterQuality <= 0.0f)
		{
			UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::DrinkWater | WaterQuality <= 0.0f"));
			ForceStopDrinking();
		}
	}
}

bool AIBaseCharacter::InputStartUse_Implementation()
{
	if (!CanUse()) return false;

	if (AIPlayerController* IPlayerController = Cast<AIPlayerController>(GetController()))
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);

		if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
		{
			if (IPlayerController->ShouldShowTouchInterface())
			{
				const bool bFocusedIsCarriable = FocusExistsAndIsCarriable();
				// Can be used
				if (FocusExistsAndIsUsable() || bFocusedIsCarriable)
				{
					UObject* ObjectToUse = FocusedObject.Object.Get();
					const bool bCanUsePrimary = IUsableInterface::Execute_CanPrimaryBeUsedBy(ObjectToUse, this);
					const bool bCanUseSecondary = IUsableInterface::Execute_CanSecondaryBeUsedBy(ObjectToUse, this);
					const bool bCanUseCarry = IUsableInterface::Execute_CanCarryBeUsedBy(ObjectToUse, this);
					const bool bCanCarry = bFocusedIsCarriable && ICarryInterface::Execute_CanCarry(ObjectToUse, this);

					if (bCanUsePrimary && !bCanUseSecondary && !bCanUseCarry && !bCanCarry)
					{
						StartUsePrimary();
					}
					else if (bCanUseSecondary && !bCanUsePrimary && !bCanUseCarry && !bCanCarry)
					{
						InputStartCollect();
					}
					else if ((bCanUseCarry || bCanCarry) && !bCanUsePrimary && !bCanUseSecondary)
					{
						InputStartCarry();
					}
					else
					{
						bDesiresPreciseMovement = false;
						if (bAutoRunActive)
						{
							ToggleAutoRun();
						}
						IGameHUD->ShowInteractionMenu();
					}

					return true;
				}
			}
		}
	}

	StartUsePrimary();
	return true;
}

void AIBaseCharacter::InputStopUse_Implementation()
{
	if (!CanUse()) return;
}

void AIBaseCharacter::StartUsePrimary()
{
	UObject* ObjectToInteract = FocusedObject.Object.Get();

	// Try to use object in mouth first
	if (ObjectToInteract && TryUseObject(ObjectToInteract, INDEX_NONE, EUseType::UT_PRIMARY))
	{
		if (bAutoRunActive)
		{
			ToggleAutoRun();
		}

		// replicate if successful
		if (GetLocalRole() != ROLE_Authority)
		{
			// store timestamp so we can ensure we don't cancel a use when attempting to cancel a previous use
			LastUseActorClientTimeStamp = GetWorld()->TimeSeconds;
			ServerUseObject(ObjectToInteract, INDEX_NONE, LastUseActorClientTimeStamp, EUseType::UT_PRIMARY);
		}
	}
}

bool AIBaseCharacter::InputStartCollect_Implementation()
{
	if (!CanUse()) return false;

	UObject* const ObjectToInteract = IsCarryingObject() ? GetCurrentlyCarriedObject().Object.Get() : FocusedObject.Object.Get();

	// Try to use object in mouth first
	if (!ObjectToInteract || !TryUseObject(ObjectToInteract, INDEX_NONE, EUseType::UT_SECONDARY))
	{
		return false;
	}

	bCollectHeld = true;

	if (bAutoRunActive)
	{
		ToggleAutoRun();
	}

	// replicate if successful
	if (GetLocalRole() != ROLE_Authority)
	{
		// store timestamp so we can ensure we don't cancel a use when attempting to cancel a previous use
		LastUseActorClientTimeStamp = GetWorld()->TimeSeconds;
		ServerUseObject(ObjectToInteract, INDEX_NONE, LastUseActorClientTimeStamp, EUseType::UT_SECONDARY);
	}

	return true;

}

void AIBaseCharacter::InputStopCollect_Implementation()
{
	bCollectHeld = false;
}

bool AIBaseCharacter::InputStartCarry_Implementation()
{
	if (!CanUse() || bAttacking) 
	{
		return false;
	}

	UObject* ObjectToInteract = FocusedObject.Object.Get();

	if (!ObjectToInteract)
	{
		return false;
	}

	// Try to pickup/drop currently carried carriable
	if (ObjectToInteract && TryCarriableObject(ObjectToInteract)) // do locally
	{
		if (bAutoRunActive)
		{
			ToggleAutoRun();
		}

		// replicate if successful
		if (GetLocalRole() != ROLE_Authority)
		{
			ServerCarryObject(FocusedObject.Object.Get());
		}
		return true;
	}

	// Else try to use focused object
	if (ObjectToInteract && TryUseObject(ObjectToInteract, FocusedObject.Item, EUseType::UT_CARRY)) // do locally
	{
		// replicate if successful
		if (GetLocalRole() != ROLE_Authority)
		{
			if (bAutoRunActive)
			{
				ToggleAutoRun();
			}
			// store timestamp so we can ensure we don't cancel a use when attempting to cancel a previous use
			LastUseActorClientTimeStamp = GetWorld()->TimeSeconds;
			ServerUseObject(ObjectToInteract, FocusedObject.Item, LastUseActorClientTimeStamp, EUseType::UT_CARRY);
		}
		return true;
	}

	return false;
}

void AIBaseCharacter::InputStopCarry_Implementation()
{
	if (!CanUse()) return;


}

void AIBaseCharacter::UseHeld_Implementation()
{
	
}

void AIBaseCharacter::UseTapped_Implementation()
{
	
}

void AIBaseCharacter::StopUsingIfPossible()
{
	check(IsLocallyControlled());

	// Check the timestamp too because the object could become invalid
	if ((NativeGetCurrentlyUsedObject().Object.IsValid() || NativeGetCurrentlyUsedObject().Timestamp > 0) && (NativeGetCurrentlyUsedObject().ObjectUseType == EUseType::UT_PRIMARY && bContinousUse))
	{
		// Stop Using Locally
		StopAllInteractions();

		// Replicate to the server if we don't have authority
		if (GetLocalRole() != ROLE_Authority)
		{
			ServerCancelUseObject(GetWorld()->TimeSeconds);
		}
	}

	if (IsDrinking())
	{
		// Stop Drinking Locally
		StopDrinkingWater();

		// Replicate if successful
		if (GetLocalRole() != ROLE_Authority)
		{
			ServerStopDrinkingWaterFromLandscape();
		}
	}
}

void AIBaseCharacter::OnRep_CurrentlyUsedObject(FUsableObjectReference OldUsedObject)
{
	if (IsLocallyControlled()) return;

	bool bHandledUseEnd = false;

	if (OldUsedObject.Object.IsValid() || OldUsedObject.Timestamp > 0)
	{
		StopAllInteractions();
		bHandledUseEnd = true;
	}

	if (NativeGetCurrentlyUsedObject().Object.IsValid())
	{
		TryUseObject(NativeGetCurrentlyUsedObject().Object.Get(), NativeGetCurrentlyUsedObject().Item, NativeGetCurrentlyUsedObject().ObjectUseType);
	}
	else
	{
		if (!bHandledUseEnd)
		{
			StopAllInteractions();
		}
	}
}

bool AIBaseCharacter::ObjectExistsAndImplementsUsable(UObject* ObjectToTest)
{
	return IsValid(ObjectToTest) && ObjectToTest->GetClass()->ImplementsInterface(UUsableInterface::StaticClass());
}

bool AIBaseCharacter::HasUsableFocus()
{
	return FocusedObject.Object != nullptr;
}

void AIBaseCharacter::SetForcePoseUpdateWhenNotRendered(const bool bNewForcingPoseUpdate)
{
	//if we are already forcing updates do not touch this
	if (IsLocallyControlled()) return;

	bForcingPoseUpdate = bNewForcingPoseUpdate;
}

void AIBaseCharacter::SetForceBoneUpdateWhenNotRendered(const bool bNewForcingBoneUpdate)
{
	//if we are already forcing updates do not touch this
	if (IsLocallyControlled()) return;

	bForcingBoneUpdate = bNewForcingBoneUpdate;
}

void AIBaseCharacter::OnRep_RemoteCarriableObject()
{
	if (IsRunningDedicatedServer()) return;

	// Pickup
	if (GetCurrentlyCarriedObject().Object.IsValid())
	{
		if (!IsLocallyControlled())
		{
			if (GetCurrentlyCarriedObject().bDrop)
			{
				EndCarrying(GetCurrentlyCarriedObject().Object.Get(), false);
			}
			else
			{
				LoadRemotePickupAnim();
			}
		}

		OnInteractReady(GetCurrentlyCarriedObject().bDrop, GetCurrentlyCarriedObject().Object.Get());
	}
}

void AIBaseCharacter::LoadRemotePickupAnim()
{
	if (GetPickupAnimMontageSoft().ToSoftObjectPath().IsValid())
	{
		// Anim already loaded
		if (GetPickupAnimMontageSoft().Get() != nullptr)
		{
			return OnRemotePickupAnimLoaded();
		}

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		Streamable.RequestAsyncLoad(
			GetPickupAnimMontageSoft().ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnRemotePickupAnimLoaded),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
	else
	{
		OnRemotePickupAnimLoaded();
	}
}

void AIBaseCharacter::OnRemotePickupAnimLoaded()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Debugging crash where gamestate is nullptr
	// https://cdn.discordapp.com/attachments/417093037504593920/701273431869489152/unknown.png
	AGameStateBase* GameStateBase = World->GetGameState();
	if (!GameStateBase) return;

	float AnimationLength = GetPickupAnimMontageSoft().Get() ? GetPickupAnimMontageSoft().Get()->GetPlayLength() : 0.0f;
	bool bRecent = GetCurrentlyCarriedObject().Timestamp + AnimationLength > GameStateBase->GetServerWorldTimeSeconds();

	StartCarrying(GetCurrentlyCarriedObject().Object.Get(), (GetCurrentlyCarriedObject().bInstaCarry || !bRecent));
}

void AIBaseCharacter::StartCarrying(UObject* CarriableObject, bool bSkipAnimation /*= false*/)
{
	if (IsValid(CarriableObject) && CarriableObject->GetClass()->ImplementsInterface(UCarryInterface::StaticClass()))
	{
		if (HasAuthority())
		{
			if (GetCurrentlyCarriedObject().Object.Get()) return;
			
			//We need to delatch the dino's that are attached to the Carriable Character we are trying to carry otherwise they will be teleported into the shadowrealm 
			if (AIBaseCharacter* const CarriableCharacter = Cast<AIBaseCharacter>(CarriableObject))
			{
                if (CarriableCharacter->HasDinosLatched())
                {
				    CarriableCharacter->UnlatchAllDinosAndSelf();
				}
			}

			FCarriableObjectReference& MutableCarriableObject = GetCurrentlyCarriedObject_Mutable();
			MutableCarriableObject.Object = CarriableObject;
			MutableCarriableObject.bInstaCarry = ShouldSkipInteractAnimation(CarriableObject) || bSkipAnimation;
			MutableCarriableObject.bDrop = false;

			if (UWorld* World = GetWorld())
			{
				if (AGameStateBase* GameStatebase = World->GetGameState())
				{
					MutableCarriableObject.Timestamp = GameStatebase->GetServerWorldTimeSeconds();
				}
			}

			AActor* const CarriableActor = Cast<AActor>(CarriableObject);

			if (CarriableActor)
			{
				UIReplicationGraph* IReplicationGraph = nullptr;
				if (GetNetDriver())
				{
					IReplicationGraph = Cast<UIReplicationGraph>(GetNetDriver()->GetReplicationDriver());
				}
				if (IReplicationGraph)
				{
					IReplicationGraph->AddTemporaryDynamicSpatialActor(CarriableActor);
				}
			}
		}

		StartPickupAnimation(CarriableObject, ShouldSkipInteractAnimation(CarriableObject) || bSkipAnimation);
	}
}

void AIBaseCharacter::StartPickupAnimation(UObject* CarriableObject, bool bInstaCarry)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::StartPickupAnimation")));

	if (bInstaCarry)
	{
		if (HasAuthority())
		{
			OnInteractReady(false, CarriableObject);
		}

		if (IsLocallyControlled())
		{
			bCarryInProgress = false;
		}
	}
	else
	{
		if (IsLocallyControlled())
		{
			bCarryInProgress = true;
		}

		LoadInteractAnim(false, CarriableObject);
	}
}

void AIBaseCharacter::ServerEndCarrying_Implementation(UObject* CarriableObject, bool bSkipEvents /*= false*/, bool bSetDropFromAbility /*= false*/, FTransform NewDropTransform /*= FTransform()*/)
{
	if (AActor* CarriableActor = Cast<AActor>(CarriableObject))
	{
		// If the player can't attack on the server, then their pose isn't being ticked. So we need to pull the clients bone transform for drops to be accurate.
		if ((NewDropTransform.GetLocation() - CarriableActor->GetActorLocation()).Size() < 5000)
		{
			DropTransform = NewDropTransform;
		}
		else
		{
			DropTransform = FTransform();
		}

		if (AIMeatChunk* IMeatChunk = Cast<AIMeatChunk>(CarriableObject))
		{
			if (AICritterPawn* ICritterPawn = Cast<AICritterPawn>(IMeatChunk->GetSource()))
			{
				ICritterPawn->SetIsInteractionInProgress(false);
			}
		}

		UE_LOG(TitansLog, Log, TEXT("AIBaseCharacter::ServerEndCarrying SizeDiff: %f"), (DropTransform.GetLocation() - CarriableActor->GetActorLocation()).Size());
	}

	bDropFromAbility = bSetDropFromAbility;
	EndCarrying(CarriableObject, bSkipEvents);
}

void AIBaseCharacter::ServerEndCarryingAny_Implementation()
{
	if (GetCurrentlyCarriedObject().Object.Get())
	{
		ServerEndCarrying_Implementation(GetCurrentlyCarriedObject().Object.Get(), true, false, FTransform());
	}
}

void AIBaseCharacter::EndCarrying(UObject* CarriableObject, bool bSkipEvents /*= false*/)
{
	AActor* const CarriableActor = Cast<AActor>(CarriableObject);

	if (CarriableActor)
	{
		UIReplicationGraph* IReplicationGraph = nullptr;
		if (GetNetDriver())
		{
			IReplicationGraph = Cast<UIReplicationGraph>(GetNetDriver()->GetReplicationDriver());
		}
		if (IReplicationGraph)
		{
			IReplicationGraph->RemoveTemporaryDynamicSpatialActor(CarriableActor);
		}
	}

	if (bSkipEvents)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_Interact);
		GetWorldTimerManager().ClearTimer(TimerHandle_OnStartInteract);
		OnInteractAnimFinished();
		OnInteractReady(true, CarriableObject);

		if (IsLocallyControlled())
		{
			bCarryInProgress = false;
		}
		
		return;
	}
	else
	{
		if (ShouldSkipInteractAnimation(CarriableObject))
		{
			bDropFromAbility = false;

			OnInteractReady(true, CarriableObject);

			if (IsLocallyControlled())
			{
				bCarryInProgress = false;
			}
		}
		else
		{
			LoadInteractAnim(true, CarriableObject);
		}
	}	
}

void AIBaseCharacter::OnAttackAbilityStart(UPOTGameplayAbility* Ability, bool bCancelled)
{
	PreviousBoneData.Empty();
	bAttacking = true;
	//UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::OnAttackAbilityStart - bAttacking = true"));
#if UE_SERVER
	if (IsRunningDedicatedServer())
	{
		if (!bCanAttackOnServer) return;
	}
#endif

	if (IsLocallyControlled())
	{
		StopUsingIfPossible();
	}

#if !UE_SERVER
	if (IsLocallyControlled())
	{
		if (AActor* CarriableActor = Cast<AActor>(GetCurrentlyCarriedObject().Object.Get()))
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::OnAttackAbilityStart() - CarriableActor")));
			AIBaseItem* BaseItem = Cast<AIBaseItem>(CarriableActor);
			AIMeatChunk* MeatChunk = Cast<AIMeatChunk>(CarriableActor);
			AIFish* Fish = Cast<AIFish>(CarriableActor);
			AICritterPawn* ICritterPawn = Cast<AICritterPawn>(CarriableActor);

			bool bInteractionInProgress = ((BaseItem && BaseItem->IsInteractionInProgress()) || (MeatChunk && MeatChunk->IsInteractionInProgress()) || (Fish && Fish->IsInteractionInProgress()) || (ICritterPawn && ICritterPawn->IsInteractionInProgress()));
			if (!Ability->bAllowWhenCarryingObject || bInteractionInProgress)
			{
				//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::OnAttackAbilityStart() - !Ability->bAllowWhenCarryingObject || bInteractionInProgress")));
				ServerEndCarrying(GetCurrentlyCarriedObject().Object.Get(), false, true, CarriableActor->GetActorTransform());
			}
			else
			{
				//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::OnAttackAbilityStart() - No Interaction in Progress")));
			}
		}
		else
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::OnAttackAbilityStart() - !CarriableActor")));
		}
	}
#endif
}

void AIBaseCharacter::OnAttackAbilityEnd(UPOTGameplayAbility* Ability, bool bSkip)
{
	bAttacking = false;
	StopOverlapChecks();
	//UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::OnAttackAbilityEnd - bAttacking = false"));
}

void AIBaseCharacter::OnInteractReady(bool bDrop, UObject* CarriableObject)
{
	// If the requested CarriableObject is not the same as inside RemoteCarriableObject, then drop the object inside RemoteCarriableObject instead.
	if (bDrop && GetCurrentlyCarriedObject().Object != CarriableObject && GetCurrentlyCarriedObject().Object.Get())
	{
		CarriableObject = GetCurrentlyCarriedObject().Object.Get();
	}

	if (!ObjectExistsAndImplementsCarriable(CarriableObject) || !HasAuthority()) return;
	
	if (AActor* CarriableActor = Cast<AActor>(CarriableObject))
	{
		if (bDrop)
		{
			USkeletalMeshComponent* SkMesh = GetMesh();
			check(SkMesh);

			if (IsCarryingObject())
			{
				FVector MainTraceStart = SkMesh->GetSocketLocation(NAME_FoodSocket);

				if (DropTransform.GetLocation() != FVector(EForceInit::ForceInitToZero))
				{
					MainTraceStart = DropTransform.GetLocation();
				}

				if (GetCapsuleComponent())
				{
					MainTraceStart.Z += (GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2);
				}
				FVector MainTraceEnd = FVector(MainTraceStart.X, MainTraceStart.Y, MainTraceStart.Z - (IsFlying() ? 250000 : 5000));

				FTransform PotentialDropLocation = GetDropTransform(MainTraceStart, MainTraceEnd, CarriableActor, false);

				if (PotentialDropLocation.GetLocation() == FVector(EForceInit::ForceInitToZero))
				{
					FVector BoxOrigin;
					FVector BoxExtent;

					if (AIMeatChunk* MeatChunk = Cast<AIMeatChunk>(CarriableActor))
					{
						BoxExtent = MeatChunk->BoxExtent;
					}
					else
					{
						CarriableActor->GetActorBounds(true, BoxOrigin, BoxExtent, false);
					}

					FVector OffsetTraceStart = FVector(ForceInitToZero);

					const int NumberOfLoops = 9;
					float RotationDirection = FMath::RandRange(0.f, 360.f);
					FVector SecondaryTraceStart;
					FVector SecondaryTraceEnd;
					for (int i = 0; i < NumberOfLoops; i++)
					{
						if (i > 3)
						{
							BoxExtent += BoxExtent;
						}

						OffsetTraceStart = BoxExtent.RotateAngleAxis(RotationDirection, FVector(0.0f, 0.0f, 1.0f));
						RotationDirection += 45.0f;

						SecondaryTraceStart = MainTraceStart;
						SecondaryTraceEnd = SecondaryTraceStart;

						SecondaryTraceStart.X += OffsetTraceStart.X;
						SecondaryTraceStart.Y += OffsetTraceStart.Y;

						SecondaryTraceEnd.Z -= 5000.f;

						PotentialDropLocation = GetDropTransform(SecondaryTraceStart, SecondaryTraceEnd, CarriableActor, false);

						if (PotentialDropLocation.GetLocation() != FVector(0.0f, 0.0f, 0.0f))
						{
							break;
						}
					}

					if (PotentialDropLocation.GetLocation() == FVector(0.0f, 0.0f, 0.0f))
					{
						PotentialDropLocation = GetDropTransform(SecondaryTraceStart, SecondaryTraceEnd, CarriableActor, true);
					}
				}

				if (AIBaseItem* IBaseItem = Cast<AIBaseItem>(CarriableActor))
				{
					IBaseItem->SetActorScale3D(FVector(1.0f, 1.0f, 1.0f));
					IBaseItem->InitializeCarryData();
				}

				DropTransform = FTransform();
				ICarryInterface::Execute_Dropped(CarriableActor, this, FTransform(PotentialDropLocation.GetRotation(), PotentialDropLocation.GetLocation(), CarriableActor->GetActorScale3D()));
			}
			else
			{
				ICarryInterface::Execute_Dropped(CarriableActor, this, CarriableActor->GetActorTransform());

				if (AIMeatChunk* IMeatChunk = Cast<AIMeatChunk>(CarriableActor))
				{
					if (!IMeatChunk->IsVisible())
					{
						IMeatChunk->DestroyBody();
					}
				}
			}

			GetCurrentlyCarriedObject_Mutable().Reset();
		}
		else if (!bDrop)
		{
			ICarryInterface::Execute_PickedUp(CarriableActor, this);

			const FCarriableData LatestCarryData = ICarryInterface::Execute_GetCarryData(CarriableActor);
			CarriableActor->SetActorRelativeLocation(LatestCarryData.AttachLocationOffset);
			CarriableActor->SetActorRelativeRotation(LatestCarryData.AttachRotationOffset);
		}
	}
}

void AIBaseCharacter::ServerTryDamageCritter_Implementation(const FGameplayEventData EventData, AICritterPawn* CritterActor)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::ServerTryDamageCritter")));

	if (!CritterActor || CritterActor->IsDead()) return;
	if (abs((GetActorLocation() - CritterActor->GetActorLocation()).Size()) > 5000) return;

	CritterActor->CritterTakeDamage(EventData, this);
}

FTransform AIBaseCharacter::GetDropTransform(FVector Start, FVector End, AActor* CarriableActor, bool bForce)
{
	check(CarriableActor);
	if (!CarriableActor)
	{
		return FTransform::Identity;
	}

	FVector SpawnLocation = CarriableActor->GetActorLocation();
	FRotator SpawnRotation = CarriableActor->GetActorRotation();

	if (DropTransform.GetLocation() != FVector(EForceInit::ForceInitToZero))
	{
		//UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::GetDropTransform - SpawnLocation = DropTransform.GetLocation();"));
		SpawnLocation = DropTransform.GetLocation();
		SpawnRotation = DropTransform.GetRotation().Rotator();
		SpawnRotation.Pitch = 0.0f;
		SpawnRotation.Roll = 0.0f;
	}

	APhysicsVolume* PhysicsVolume = GetPhysicsVolume();
	APhysicsVolume* WaterVolume = (PhysicsVolume && PhysicsVolume->bWaterVolume) ? PhysicsVolume : nullptr;
	bool bInWater = false;
	FHitResult WaterHit;

	if (WaterVolume)
	{
		SpawnLocation = CarriableActor->GetActorLocation();
		SpawnLocation.Z = WaterVolume->GetActorLocation().Z + WaterVolume->GetBounds().BoxExtent.Z;	

		bInWater = true;
	}
	else if (IsInOrAboveWater(&WaterHit, &Start))
	{
		if (Cast<AIWater>(WaterHit.GetActor()))
		{
			SpawnLocation = CarriableActor->GetActorLocation();
			SpawnLocation.Z = WaterHit.ImpactPoint.Z;
			bInWater = true;
		}
	}

	FHitResult Hit(ForceInit);
	bool bHitItem = false;
	TArray<FHitResult> Hits;

	FCollisionQueryParams TraceParams(NAME_TraceUsableActor, true, this);
	TraceParams.TraceTag = FName("IDinosaur_Trace_Tag");
	TraceParams.bReturnPhysicalMaterial = false;
	TraceParams.bTraceComplex = false;
	TraceParams.AddIgnoredActor(this);
	TraceParams.AddIgnoredActor(CarriableActor);

	if (!bInWater)
	{
		GetWorld()->LineTraceMultiByChannel(Hits, Start, End, ECC_Visibility, TraceParams);

		for (int i = 0; i < Hits.Num(); i++)
		{
			//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("Hit: %s"), *Hits[i].Actor->GetName()));
			if (!bForce)
			{
				// If hit meat or fish first then skip this trace
				if (Cast<AIMeatChunk>(Hits[i].GetActor()) || Cast<AIFish>(Hits[i].GetActor()) || Cast<AIBaseItem>(Hits[i].GetActor())) 
				{
					bHitItem = true;
					break;
				}
			}

			// Save Trace Info
			if (Hits.IsValidIndex(i) && !Cast<AIWater>(Hits[i].GetActor()))
			{
				Hit = Hits[i];
				break;
			}
		}
	}

	// Calculate Transform from hit point/normal
	if (!bHitItem)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::GetDropTransform Calculating Custom Rot/Loc")));

		float ZOffset = 0.0f;
		if ((!bInWater || (SpawnLocation.X == 0 && SpawnLocation.Y == 0)) && Hit.ImpactPoint != FVector(EForceInit::ForceInitToZero))
		{
			SpawnLocation = Hit.ImpactPoint;
		}
	
		// Spawn it slightly above the ground if is a fish to fix ragdoll issues
		if (AIFish* FishCarriable = Cast<AIFish>(CarriableActor))
		{
			if (FishCarriable->bRollWhenDropped)
			{
				ZOffset = (FishCarriable->GetCollisionBounds().Y);
			}
			else
			{
				ZOffset = (FishCarriable->GetCollisionBounds().Z);
			}
		}
		else if (AIMeatChunk* MeatCarriable = Cast<AIMeatChunk>(CarriableActor))
		{
			if (MeatCarriable->GetBoxComponent())
			{
				ZOffset = (MeatCarriable->GetBoxComponent()->GetScaledBoxExtent().Z / 2);
			}
		}
		else if (AICritterPawn* CritterCarriable = Cast<AICritterPawn>(CarriableActor))
		{
			ZOffset = (ICarryInterface::Execute_GetCarryData(CritterCarriable).AttachLocationOffset.Z);
		}
		
		if (bInWater)
		{
			if (AIBaseItem* IBaseItem = Cast<AIBaseItem>(CarriableActor))
			{
				ZOffset = IBaseItem->BoxExtent.Z;
			}
			ZOffset *= -1.0f;
		}

		SpawnLocation.Z += ZOffset;

		// Calculate Pitch and Yaw of rotation
		FQuat RootQuat = SpawnRotation.Quaternion();
		FVector UpVector = RootQuat.GetUpVector();
		FVector RotationAxis = FVector::CrossProduct(UpVector, Hit.Normal);
		RotationAxis.Normalize();

		float DotProduct = FVector::DotProduct(UpVector, Hit.Normal);
		float RotationAngle = acosf(DotProduct);

		FQuat Quat = FQuat(RotationAxis, RotationAngle);
		FQuat NewQuat = Quat * RootQuat;

		SpawnRotation = NewQuat.Rotator();

		// Make fish lay on its side and rotate to face same direction as its held in mouth
		if (AIFish* FishCarriable = Cast<AIFish>(CarriableActor))
		{
			if (FishCarriable->bRollWhenDropped)
			{
				SpawnRotation.Roll = 90.0f;	
			}
		}

		// Ensure we aren't blocked one last time
		Hit = FHitResult(ForceInit);
		Hits.Empty();
		GetWorld()->LineTraceMultiByChannel(Hits, Start, SpawnLocation, ECC_Visibility, TraceParams);

		for (int i = 0; i < Hits.Num(); i++)
		{
			if (Hits.IsValidIndex(i) && !Cast<AIWater>(Hits[i].GetActor()))
			{
				Hit = Hits[i];
				break;
			}
		}
	}

	FTransform SpawnTransform;
	// If Spawn location is blocked then force it to spawn at dinosaurs location
	if (!bForce && bHitItem)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::GetDropTransform 0.0f")));
		SpawnTransform.SetLocation(FVector(0.0f, 0.0f, 0.0f));
		SpawnTransform.SetRotation(FQuat(ForceInitToZero));
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::GetDropTransform SpawnLocation")));
		SpawnTransform.SetLocation(SpawnLocation);
		SpawnTransform.SetRotation(SpawnRotation.Quaternion());
	}

	return SpawnTransform;
}

void AIBaseCharacter::OnInteractAnimFinished()
{
	if (IsLocallyControlled())
	{
		bCarryInProgress = false;
	}

	//Eating can start before drop animation finishes so check to ensure we're not already eating
	if (CurrentUseAnimMontage && !IsEating())
	{
		UAnimMontage* TempUseMontage = CurrentUseAnimMontage;
		CurrentUseAnimMontage = nullptr;
		StopAnimMontage(TempUseMontage);
	}

	if (!bAttacking)
	{
		//UE_LOG(TitansLog, Log, TEXT("AIBaseCharacter::OnInteractAnimFinished() - SetForcePoseUpdateWhenNotRendered"));
		SetForcePoseUpdateWhenNotRendered(false);
		SetForceBoneUpdateWhenNotRendered(false);
	}

	DropTransform = FTransform::Identity;
}

bool AIBaseCharacter::ShouldSkipInteractAnimation_Implementation(UObject* CarriableObject)
{
	if (bDropFromAbility) return true;
	if (const AIFish* const CarriableFish = Cast<AIFish>(CarriableObject))
	{
		return CarriableFish->IsAlive();
	}

	if (const AIBaseCharacter* const CarriableCharacter = Cast<AIBaseCharacter>(CarriableObject))
	{
		return CarriableCharacter->IsAlive();
	}

	return false;
}

const FCarriableObjectReference& AIBaseCharacter::GetCurrentlyCarriedObject() const
{
	return RemoteCarriableObject;
}

// ICarryInterface
bool AIBaseCharacter::Notify_TryCarry_Implementation(class AIBaseCharacter* User) 
{
	return true;
}

void AIBaseCharacter::PickedUp_Implementation(AIBaseCharacter* User)
{
	SetAttachTarget(User->GetMesh(), NAME_FoodSocket, EAttachType::AT_Carry);
}

void AIBaseCharacter::Dropped_Implementation(AIBaseCharacter* User, FTransform DropPosition)
{
	ClearAttachTarget();
}

FVector AIBaseCharacter::GetCarryLocation_Implementation(class AIBaseCharacter* User)
{
	return FVector::ZeroVector;
}

FVector AIBaseCharacter::GetDropLocation_Implementation(class AIBaseCharacter* User)
{
	return FVector::ZeroVector;
}

FCarriableData AIBaseCharacter::GetCarryData_Implementation()
{
	if (FMath::IsNearlyZero(CarryData.JawOpenRequirement))
	{
		CarryData.JawOpenRequirement = CurrentMaxJawOpenAmount;
	}

	return CarryData;
}

bool AIBaseCharacter::IsCarried_Implementation() const
{
	const FAttachTarget& AttachTarget = GetAttachTarget();
	return AttachTarget.IsValid() && AttachTarget.AttachType == EAttachType::AT_Carry;
}

bool AIBaseCharacter::IsCarriedBy_Implementation(AIBaseCharacter* PotentiallyCarrying)
{
	return IsCarried_Implementation() && PotentiallyCarrying && AttachTargetOwner == PotentiallyCarrying;
}

bool AIBaseCharacter::CanCarry_Implementation(AIBaseCharacter* PotentiallyCarrying)
{
	check(PotentiallyCarrying);
	if (!PotentiallyCarrying || !PotentiallyCarrying->IsAlive() || !IsAlive() || Cast<AIPlayerController>(Controller) == nullptr)
	{
		return false;
	}

	return true;
}

FCarriableObjectReference& AIBaseCharacter::GetCurrentlyCarriedObject_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, RemoteCarriableObject, this);
	return RemoteCarriableObject;
}

bool AIBaseCharacter::IsCarryingObject() const
{
	return GetCurrentlyCarriedObject().Object.IsValid();
}

bool AIBaseCharacter::CarryBlocked_Implementation(UObject* CarriableObject)
{
	UICharacterMovementComponent* const ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	if (!ICharMove)
	{
		return false;
	}

	bool bAllowPickupWhileFalling = false;
	if (const AIMeatChunk* const MeatChunk = Cast<AIMeatChunk>(CarriableObject))
	{
		bAllowPickupWhileFalling = MeatChunk->ShouldAllowPickupWhileFalling();
	}

	return (!bAllowPickupWhileFalling && ICharMove->IsFalling()) || IsEatingOrDrinking() || IsResting();
}

bool AIBaseCharacter::TryCarriableObject(UObject* CarriableObject, bool bSkipAnimation /*= false*/)
{
	if (IsCarryingObject())
	{
		if (IsLocallyControlled())
		{
			if (GetCurrentlyCarriedObject().Object.Get() != CarriableObject)
			{
				return false;
			}
		}

		EndCarrying(CarriableObject);
		return true;
	}
	else
	{
		// Validate that object exists and is carriable
		if (!ObjectExistsAndImplementsCarriable(CarriableObject))
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::TryCarriableObject - Object doesn't exist or isn't carriable!"));
			return false;
		}

		if (!bSkipAnimation)
		{
			//if (bAttacking)
			//{
			//	UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::TryCarriableObject - bAttacking!"));
				//return false;
			//}
		}

		const AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
		if (IGameHUD && IGameHUD->bVocalWindowActive) return false;

		// Simulated proxies can start carrying another actor as they only do cosmetic work
		if (GetLocalRole() != ROLE_SimulatedProxy && CarryBlocked(CarriableObject))
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::TryCarriableObject - Carry Blocked and not simulated proxy!"));
			return false;
		}

		const AIBaseItem* const Item = Cast<AIBaseItem>(CarriableObject);

		const bool bIsObjectSpawner = (Item && Item->IsSpawnPoint()) || CarriableObject->IsA<AICorpse>();

		if (!ICarryInterface::Execute_CanCarry(CarriableObject, this))
		{
			if (!bIsObjectSpawner)
			{
				UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::TryCarriableObject - Cannot Carry Object!"));
			}
			return false;
		}

		if (IsObjectInFocusDistance(CarriableObject, INDEX_NONE))
		{
			if (ICarryInterface::Execute_Notify_TryCarry(CarriableObject, this))
			{
				StartCarrying(CarriableObject, bSkipAnimation);
				return true;
			}
		}
	}

	UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::TryCarriableObject - false!"));

	return false;
}

void AIBaseCharacter::ServerCarryObject_Implementation(UObject* CarriableObject, bool bSkipAnimation /*= false*/)
{
	if (!TryCarriableObject(CarriableObject, bSkipAnimation))
	{
		ClientCancelCarryObject();
	}
}

void AIBaseCharacter::ClientCarryObject_Implementation(UObject* CarriableObject, bool bSkipAnimation /*= false*/)
{
	if (TryCarriableObject(CarriableObject, bSkipAnimation))
	{
		// replicate if successful
		if (GetLocalRole() != ROLE_Authority)
		{
			ServerCarryObject(CarriableObject, bSkipAnimation);
		}
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::ClientCarryObject() - !TryCarriableObject")));
		bCarryInProgress = false;
	}
}

void AIBaseCharacter::ClientCancelCarryObject_Implementation(bool bForce /*= false*/)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::ClientCancelCarryObject Called!")));
	UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ClientCancelCarryObject Called!"));

	if (GetCurrentlyCarriedObject().Object.IsValid())
	{
		OnClientCancelCarryCorrection();
	}
	else
	{
		bCarryInProgress = false;
	}
}

void AIBaseCharacter::OnClientCancelCarryCorrection()
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::OnClientCancelCarryCorrection Called!")));
	UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::OnClientCancelCarryCorrection Called!"));

	if (GetCurrentlyCarriedObject().Object.IsValid())
	{
		EndCarrying(GetCurrentlyCarriedObject().Object.Get(), true);
	}
}

FRotator AIBaseCharacter::GetLocalAimRotation()
{
	return AimRotation + DefaultAimOffset;
}

const FUsableObjectReference& AIBaseCharacter::GetFocusedObject()
{
	return FocusedObject;
}

FUsableObjectReference AIBaseCharacter::K2_GetFocusedObject()
{
	return FocusedObject;
}

bool AIBaseCharacter::LocationWithinViewAngleAndFocusRange(const FVector& Location) const
{
	const FVector Delta = Location - GetActorLocation();
	return Delta.Size() <= GetGrowthFocusTargetDistance() && FVector::DotProduct(Delta.GetSafeNormal(), GetActorForwardVector()) > 0.f;
}

void AIBaseCharacter::SetTargetFocalPointComponent(UFocalPointComponent* NewTarget)
{
	// Don't change the target focal point if its the same
	if (TargetFocalPointComponent == NewTarget) return;

	if (IsValid(TargetFocalPointComponent))
	{
		// Stop focusing on old target
		AActor* OldTargetActor = TargetFocalPointComponent->GetOwner();
		if (IsValid(OldTargetActor) && OldTargetActor->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
		{
			// Inform the interface actor
			IUsableInterface::Execute_OnStopBeingFocusTarget(OldTargetActor, this);
			SetRenderOutlinesForFocusObject(OldTargetActor, false);
		}
	}

	TargetFocalPointComponent = NewTarget;

	if (TargetFocalPointComponent)
	{
		// Start focusing on new target
		AActor* NewTargetActor = TargetFocalPointComponent->GetOwner();
		if (IsValid(NewTargetActor) && NewTargetActor->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
		{
			// Inform the interface actor
			IUsableInterface::Execute_OnBecomeFocusTarget(NewTargetActor, this);
			SetRenderOutlinesForFocusObject(NewTargetActor, true);
		}
	}
}

void AIBaseCharacter::SetRenderOutlinesForFocusObject(UObject* FocusObject, const bool bRender)
{
	/*
	if (FocusObject && !FocusObject->IsPendingKill() && FocusObject->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()) && GetController() && GetController()->IsLocalPlayerController())
	{
		TArray<UMeshComponent*> OutlineMeshComponents;
		IUsableInterface::Execute_GetFocusTargetOutlineMeshComponents(FocusObject, OutlineMeshComponents);

		for (UMeshComponent* OutlineMeshComponent : OutlineMeshComponents)
		{
			if (OutlineMeshComponent && !OutlineMeshComponent->IsPendingKill())
			{
				OutlineMeshComponent->SetRenderCustomDepth(bRender);
			}
		}
	}
	*/
}

void AIBaseCharacter::UpdateFocusAndAimRotation(const float DeltaSeconds)
{
#if !UE_SERVER
	// NEW WIP VERSION - ANIXON
	if (!GetWorld()) return;
	
	FRotator ViewPointRot;
	FVector TraceStart;
	FVector TraceEnd;
	
	// need these a few times
	if (GetMesh())
	{
		//GetController()->GetPlayerViewPoint(TraceStart, ViewPointRot);
		// Need straight control rot. Cam will rotate to face dino head
		TraceStart = GetMesh()->GetSocketLocation(HeadBoneName);
		ViewPointRot = GetControlRotation();
	}
	else
	{
		GetActorEyesViewPoint(TraceStart, ViewPointRot);
	}
	
	FVector FocusFromLocation;
	EvaluateFocusFromLocation(FocusFromLocation);
	
	FVector TargetFocalPointLocation = FVector::ZeroVector;
	
	if (IsSleeping() || (IsEatingOrDrinking() && !IsCarryingObject()))
	{
		DesiredAimRotation = FRotator(ForceInitToZero);
		return;
	}
	
	// Update the target focus location, simulated proxy will received this via replication
	if (!bLockDesiredFocalPoint)
	{
		TraceEnd = TraceStart + (ViewPointRot.Vector() * GetGrowthFocusTargetDistance());
	
		ProcessFocusTargets(TraceStart, TraceEnd, FCollisionShape::MakeSphere(FocusTargetRadius), TargetFocalPointLocation, DeltaSeconds, false);
	}
	
	if (!IsValid(TargetFocalPointComponent) && TargetFocalPointComponent)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString("Target Focal Component Is Pending Kill, Clearing Target"));
		TargetFocalPointComponent = nullptr;
	}
	
	// sticky focal point components
	if (DesiredTargetFocalPointComponent)
	{
		if (GetWorld()->GetTimerManager().TimerExists(ClearTargetFocalPointComponentTimer))
		{
			GetWorld()->GetTimerManager().ClearTimer(ClearTargetFocalPointComponentTimer);
		}
	
		SetTargetFocalPointComponent(DesiredTargetFocalPointComponent);
	}
	else if (TargetFocalPointComponent)
	{
		// if the target goes out of our range
		if (GetGrowthFocusTargetDistance() < (TargetFocalPointComponent->GetComponentLocation() - TraceStart).Size() || FVector::DotProduct((TargetFocalPointComponent->GetComponentLocation() - TraceStart).GetSafeNormal(), GetActorForwardVector()) <= 0.f)
		{
			if (GetWorld()->GetTimerManager().TimerExists(ClearTargetFocalPointComponentTimer))
			{
				GetWorld()->GetTimerManager().ClearTimer(ClearTargetFocalPointComponentTimer);
			}
	
			ClearTargetFocalPointComponent();
		}
		else
		{
			if (!GetWorld()->GetTimerManager().TimerExists(ClearTargetFocalPointComponentTimer))
			{
				GetWorld()->GetTimerManager().SetTimer(ClearTargetFocalPointComponentTimer, this, &AIBaseCharacter::ClearTargetFocalPointComponent, ClearTargetFocalPointComponentDelay);
			}
		}
	}
	
	// use the getter so we retrieve the component location when appropriate
	if (TargetFocalPointComponent)
	{
		TargetFocalPointLocation = TargetFocalPointComponent->GetComponentLocation();
	}
	
	// test
	if (!bLockDesiredFocalPoint)
	{
		TargetFocalPointLocation = TraceEnd;
		// update the desired aim rotation to face the focal point location
		DesiredAimRotation = GetActorRotation().UnrotateVector(TargetFocalPointLocation - TraceStart).Rotation().GetNormalized();
	}
#endif
}

void AIBaseCharacter::ProcessFocusTargets(FVector TraceStart, FVector TraceEnd, FCollisionShape Shape, FVector &TargetFocalPointLocation, const float DeltaSeconds, bool bForceReset /* = false*/)
{
	if (bIsAbilityInputDisabled) 
	{
		SetFocusedObjectAndItem(nullptr, INDEX_NONE);
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	// Debug Trace Tags
	//const FName TraceTag("IsAtWaterSurface");
	//QueryParams.TraceTag = TraceTag;
	//FlushPersistentDebugLines(GetWorld());
	//GetWorld()->DebugDrawTraceTag = TraceTag;

	TArray<FHitResult> FocusHitResults;
	GetWorld()->SweepMultiByChannel(FocusHitResults, TraceStart, TraceEnd, FQuat::Identity, TRACE_FOCUSTARGET, Shape, QueryParams);

	//need to know if there's a blocking hit
	float DistanceToBlockingHit = -1.f;
	AActor* BlockingHitActor = nullptr;
	for (FHitResult& HitResult : FocusHitResults)
	{
		if (HitResult.bBlockingHit)
		{
			FRotator FocusPointRot = FRotationMatrix::MakeFromX(TraceStart - HitResult.GetActor()->GetActorLocation()).Rotator();
			FVector FocusPointVec = FocusPointRot.Vector() - GetControlRotation().Vector();

			BlockingHitActor = HitResult.GetActor();
			DistanceToBlockingHit = HitResult.Distance;
			break;
		}
	}

	// calculate the nearest 
	float NearestToFocusFromLoc = BIG_NUMBER;
	uint8 HighestPriorityToFocus = 0;
	DesiredTargetFocalPointComponent = nullptr;
	bool bSelectedIsUsable = false;
	bool bFoundUsableFocus = false;

	TargetFocalPointLocation = TraceEnd;

	if (FocusHitResults.Num() == 0 && !IsCarryingObject())
	{
#if WITH_EDITOR
		//GEngine->AddOnScreenDebugMessage(-1, DeltaSeconds, FColor::Red, FString("Focus Hit Results = 0, Clearing Target"));
#endif
		SetTargetFocalPointComponent(nullptr);
	}
	else
	{
		if (IsCarryingObject())
		{
			FUsableObjectReference DesiredUsable;

			for (FHitResult& HitResult : FocusHitResults)
			{
				if (HitResult.bBlockingHit || Cast<AIDeliveryPoint>(HitResult.GetActor()))
				{
					// test that the hit result location is within our characters focal field
					if (LocationWithinViewAngleAndFocusRange(HitResult.Location))
					{
						UObject* PotentialUsableObject = nullptr;

						// find any potential usable objects
						if (HitResult.Component.IsValid() && HitResult.Component.Get()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass())) // handle components
						{
							PotentialUsableObject = HitResult.Component.Get();
						}
						else if (HitResult.GetActor() && (HitResult.GetActor()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()) || ObjectExistsAndImplementsCarriable(HitResult.GetActor()))) //handle actors
						{
							PotentialUsableObject = HitResult.GetActor();
						}

						// test if this usable object can be focused on by this user
						if (IsValid(PotentialUsableObject) && IUsableInterface::Execute_CanBeFocusedOnBy(PotentialUsableObject, this))
						{
							TargetFocalPointLocation = HitResult.Location;
							DesiredTargetFocalPointComponent = nullptr;
							bFoundUsableFocus = true;
							DesiredUsable.Object = PotentialUsableObject;
							DesiredUsable.Item = HitResult.Item;
							break;
						}
					}
				}
			}

			if (DesiredUsable.Object.IsValid())
			{
				if (AIDeliveryPoint* DeliveryPoint = Cast<AIDeliveryPoint>(DesiredUsable.Object))
				{
					if (DesiredUsable.Object.IsValid() && IUsableInterface::Execute_CanPrimaryBeUsedBy(DeliveryPoint, this))
					{
						SetFocusedObjectAndItem(DesiredUsable.Object.Get(), DesiredUsable.Item);
						return;
					}
				}
				else if (AIHomeRock* HomeRock = Cast<AIHomeRock>(DesiredUsable.Object))
				{
					if (DesiredUsable.Object.IsValid() && IUsableInterface::Execute_CanPrimaryBeUsedBy(HomeRock, this))
					{
						SetFocusedObjectAndItem(DesiredUsable.Object.Get(), DesiredUsable.Item);
						return;
					}
				}
			}

			SetFocusedObjectAndItem(GetCurrentlyCarriedObject().Object.Get(), INDEX_NONE);
			bFoundUsableFocus = true;
		}
		else
		{
			FUsableObjectReference DesiredUsable;

			for (FHitResult& HitResult : FocusHitResults)
			{
				//if (HitResult.Actor.Get()) GEngine->AddOnScreenDebugMessage(-1, DeltaSeconds, FColor::Green, FString::Printf(TEXT("HitResult.Actor: %s"), *HitResult.Actor.Get()->GetName()));

				if (HitResult.bBlockingHit)
				{
					// test that the hit result location is within our characters focal field
					if (LocationWithinViewAngleAndFocusRange(HitResult.Location))
					{
						UObject* PotentialUsableObject = nullptr;

						// find any potential usable objects
						if (HitResult.Component.IsValid() && HitResult.Component.Get()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass())) // handle components
						{
							PotentialUsableObject = HitResult.Component.Get();
						}
						else if (HitResult.GetActor() && (HitResult.GetActor()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()) || ObjectExistsAndImplementsCarriable(HitResult.GetActor()))) //handle actors
						{
							PotentialUsableObject = HitResult.GetActor();
						}

						// test if this usable object can be focused on by this user
						if (IsValid(PotentialUsableObject) && IUsableInterface::Execute_CanBeFocusedOnBy(PotentialUsableObject, this))
						{
							TargetFocalPointLocation = HitResult.Location;
							DesiredTargetFocalPointComponent = nullptr;
							bFoundUsableFocus = true;
							DesiredUsable.Object = PotentialUsableObject;
							DesiredUsable.Item = HitResult.Item;
							break;
						}
					}
				}
				else
				{
					uint8 CurrentUsablePriority = static_cast<uint8>(EFocusPriority::FP_LOWEST);
					if (AActor* HitActor = HitResult.GetActor())
					{
						if (HitActor->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
						{
							CurrentUsablePriority = IUsableInterface::Execute_GetFocusPriority(HitActor);
						}
					}

					FVector ForwardVectorToTarget = FRotationMatrix::MakeFromX(TraceEnd - HitResult.GetActor()->GetActorLocation()).Rotator().Vector();
					FVector TraceForwardVector = FRotationMatrix::MakeFromX(TraceEnd - TraceStart).Rotator().Vector();

					const float DistanceToViewPointLocation = FVector::Distance(ForwardVectorToTarget, TraceForwardVector);
					const float DistanceToTraceHitLocation = HitResult.Distance;

					// Lower priority then skip
					if (CurrentUsablePriority < HighestPriorityToFocus)
					{
						continue;
					}
					// Same priority then favor the closest to focus
					else if (CurrentUsablePriority == HighestPriorityToFocus)
					{
						if (NearestToFocusFromLoc < DistanceToViewPointLocation)
						{
							continue;
						}
					}

					//test if view occluder is nearer than target, skip if it is
					if (BlockingHitActor && BlockingHitActor != HitResult.GetActor() && DistanceToBlockingHit < DistanceToTraceHitLocation) continue;

					//test that the hit result location is within our characters focal field
					if (!LocationWithinViewAngleAndFocusRange(HitResult.Location)) continue;

					//focal point components
					UFocalPointComponent* HitFocalPoint = Cast<UFocalPointComponent>(HitResult.Component.Get());
					if (IsValid(HitFocalPoint))
					{
						if (HitFocalPoint->GetOwner() && HitFocalPoint->GetOwner()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
						{
							TargetFocalPointLocation = HitResult.Location;
							NearestToFocusFromLoc = DistanceToViewPointLocation;
							HighestPriorityToFocus = CurrentUsablePriority;
							DesiredTargetFocalPointComponent = HitFocalPoint;
							bFoundUsableFocus = true;
							DesiredUsable.Object = HitFocalPoint->GetOwner();
							DesiredUsable.Item = HitResult.Item;
						}
					}
					else
					{
						UObject* PotentialUsableObject = nullptr;

						//find any potential usable objects
						if (HitResult.Component.IsValid() && HitResult.Component.Get()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass())) // handle components
						{
							PotentialUsableObject = HitResult.Component.Get();
						}
						else if (HitResult.GetActor() && HitResult.GetActor()->GetClass()->ImplementsInterface(UUsableInterface::StaticClass())) //handle actors
						{
							PotentialUsableObject = HitResult.GetActor();
						}

						if (PotentialUsableObject && IUsableInterface::Execute_CanBeFocusedOnBy(PotentialUsableObject, this))
						{
							TargetFocalPointLocation = HitResult.Location;
							NearestToFocusFromLoc = DistanceToViewPointLocation;
							HighestPriorityToFocus = CurrentUsablePriority;
							DesiredTargetFocalPointComponent = nullptr;
							bFoundUsableFocus = true;
							DesiredUsable.Object = PotentialUsableObject;
							DesiredUsable.Item = HitResult.Item;
						}
					}
				}
			}

			if (DesiredUsable.Object.IsValid())
			{
				bool bUsableIsWater = false;
				
				if (AIWater* WaterTarget = Cast<AIWater>(DesiredUsable.Object))
				{
					if (bForceReset)
					{
						bFoundUsableFocus = false;
						DesiredUsable.Object = nullptr;
						DesiredUsable.Item = INDEX_NONE;
						DesiredTargetFocalPointComponent = nullptr;
					}
				
					if (!bForceReset)
					{
						TraceStart.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2;
						FVector TraceWaterEnd = GetMesh()->GetSocketLocation(HeadBoneName);
						TraceWaterEnd.Z -= GetGrowthFocusTargetDistance();
				
						FCollisionQueryParams TraceParams;
						TraceParams.AddIgnoredActor(this);
						//TraceParams.TraceTag = TraceTag;
						FCollisionObjectQueryParams ObjectTraceParams;
						ObjectTraceParams.AddObjectTypesToQuery(ECC_WorldStatic);
						ObjectTraceParams.AddObjectTypesToQuery(ECC_GameTraceChannel8);
				
						FHitResult Hit(ForceInit);
						GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceWaterEnd, ObjectTraceParams, TraceParams);

						if (Hit.GetActor() != WaterTarget && Cast<AIDeliveryPoint>(Hit.GetActor()) == nullptr)
						{
							bFoundUsableFocus = false;
							DesiredUsable.Object = nullptr;
							DesiredUsable.Item = INDEX_NONE;
							DesiredTargetFocalPointComponent = nullptr;
						}
					}
				}
				
				if (DesiredUsable.Object.IsValid())
				{
					SetFocusedObjectAndItem(DesiredUsable.Object.Get(), DesiredUsable.Item);
				}
			}
		}
	}

	if (!bFoundUsableFocus)
	{
		if (!bForceReset)
		{
			FVector SecondTraceEnd = GetActorLocation();
			SecondTraceEnd.Z -= GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
			ProcessFocusTargets(TraceStart, SecondTraceEnd, FCollisionShape::MakeSphere(GetGrowthFocusTargetDistance() / 4), TargetFocalPointLocation, DeltaSeconds, true);
			return;
		}
		else
		{
			SetFocusedObjectAndItem(nullptr, INDEX_NONE);
		}
	}
}

void AIBaseCharacter::EvaluateFocusFromLocation(FVector & OutFocusFromLocation) const
{
	if (GetMesh())
	{
		OutFocusFromLocation = GetMesh()->GetSocketLocation(HeadBoneName);
	} else {
		OutFocusFromLocation = GetActorLocation();
	}
}

AIBaseCharacter* AIBaseCharacter::GetFocusedCharacter()
{
	if (AIBaseCharacter* DeadCharacter = Cast<AIBaseCharacter>(GetFocusedObject().Object.Get()))
	{
		return DeadCharacter;
	}
	else
	{
		if (!GetWorld()) return nullptr;

		FRotator ViewPointRot;
		FVector TraceStart;
		FVector TraceEnd;

		// need these a few times
		if (GetMesh())
		{
			// Need straight control rot. Cam will rotate to face dino head
			TraceStart = GetMesh()->GetSocketLocation(HeadBoneName);
			ViewPointRot = GetControlRotation();
		}
		else
		{
			GetActorEyesViewPoint(TraceStart, ViewPointRot);
		}

		// Update the target focus location, simulated proxy will received this via replication
		if (!bLockDesiredFocalPoint)
		{
			TraceEnd = TraceStart + (ViewPointRot.Vector() * GetGrowthFocusTargetDistance());
		}

		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		// Debug Trace Tags
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		const FName TraceTag("IsAtWaterSurface");
		QueryParams.TraceTag = TraceTag;
		FlushPersistentDebugLines(GetWorld());
		//GetWorld()->DebugDrawTraceTag = TraceTag;
#endif

		FHitResult FocusHitResults;
		GetWorld()->SweepSingleByChannel(FocusHitResults, TraceStart, TraceEnd, FQuat::Identity, ECC_GameTraceChannel4, FCollisionShape::MakeSphere(64.0f), QueryParams);

#if WITH_EDITOR
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("RemoteCharacter: %s"), *FocusHitResults.GetActor()->GetName()));
#endif

		if (AIBaseCharacter* RemoteCharacter = Cast<AIBaseCharacter>(FocusHitResults.GetActor()))
		{
			return RemoteCharacter;
		}
	}

	return nullptr;
}

void AIBaseCharacter::ClearTargetFocalPointComponent()
{
	SetTargetFocalPointComponent(nullptr);
}

bool AIBaseCharacter::ObjectExistsAndImplementsCarriable(UObject* ObjectToTest) const
{
	if (IsValid(ObjectToTest))
	{
		if (UClass* ObjectClass = ObjectToTest->GetClass())
		{
			return ObjectClass->ImplementsInterface(UCarryInterface::StaticClass());
		}
	}
	return false;
}

bool AIBaseCharacter::FocusExistsAndIsUsable()
{
	UObject* CurrentFocusedObject = FocusedObject.Object.Get();

	if (CurrentFocusedObject && ObjectExistsAndImplementsUsable(CurrentFocusedObject))
	{
		const bool bCanPrimaryBeUsed = IUsableInterface::Execute_CanPrimaryBeUsedBy(CurrentFocusedObject, this);
		const bool bCanSecondaryBeUsed = IUsableInterface::Execute_CanSecondaryBeUsedBy(CurrentFocusedObject, this);
		const bool bCanCarryBeUsed = IUsableInterface::Execute_CanCarryBeUsedBy(CurrentFocusedObject, this);

		return bCanPrimaryBeUsed || bCanSecondaryBeUsed || bCanCarryBeUsed;
	}

	return false;
}

bool AIBaseCharacter::FocusExistsAndIsCarriable()
{
	UObject* CurrentFocusedObject = FocusedObject.Object.Get();
	return CurrentFocusedObject && ObjectExistsAndImplementsCarriable(CurrentFocusedObject) && ICarryInterface::Execute_CanCarry(CurrentFocusedObject, this);
}

bool AIBaseCharacter::FocusExistsAndIsCollectable()
{
	UObject* const CurrentFocusedObject = FocusedObject.Object.Get();

	if (CurrentFocusedObject && ObjectExistsAndImplementsUsable(CurrentFocusedObject))
	{
		if (AIBaseItem* const BaseItem = Cast<AIBaseItem>(FocusedObject.Object))
		{
			return BaseItem->IsPartOfActiveQuestAndCanCollect(this);
		}
	}

	return false;
}

void AIBaseCharacter::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	if (!ensureAlways(AbilitySystem))
	{
		return;
	}

	UCoreAttributeSet* CoreAttrSet = AbilitySystem->GetAttributeSet_Mutable<UCoreAttributeSet>();
	if (!ensureAlways(CoreAttrSet))
	{
		return;
	}

	CoreAttrSet->PreReplicate();
}

void AIBaseCharacter::SetRemoteDesiredAimPitch(uint8 NewRemoteDesiredAimPitch)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, RemoteDesiredAimPitch, NewRemoteDesiredAimPitch, this);
}

void AIBaseCharacter::SetRemoteDesiredAimYaw(uint8 NewRemoteDesiredAimYaw)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, RemoteDesiredAimYaw, NewRemoteDesiredAimYaw, this);
}

void AIBaseCharacter::SetRemoteDesiredAimPitchReplay(uint8 NewRemoteDesiredAimPitchReplay)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, RemoteDesiredAimPitchReplay, NewRemoteDesiredAimPitchReplay, this);
}

void AIBaseCharacter::SetRemoteDesiredAimYawReplay(uint8 NewRemoteDesiredAimYawReplay)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, RemoteDesiredAimYawReplay, NewRemoteDesiredAimYawReplay, this);
}

void AIBaseCharacter::UpdateAimRotation(float DeltaSeconds)
{
	FRotator DesiredAim = FRotator::ZeroRotator;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	const bool bUseReplayData = (IGameInstance && IGameInstance->IsWatchingReplay());

	if (bUseReplayData)
	{
		DesiredAim.Pitch = FRotator::NormalizeAxis((GetRemoteDesiredAimPitchReplay() * 360) / 255.f);
		DesiredAim.Yaw = FRotator::NormalizeAxis((GetRemoteDesiredAimYawReplay() * 360) / 255.f);
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("Using Replay Data: %i %i"), RemoteDesiredAimPitchReplay, RemoteDesiredAimYawReplay));
	}
	else
	{
		// If we are simulated then we need to use the remote version of these properties
		if (GetLocalRole() == ROLE_SimulatedProxy)// || FMath::IsNearlyZero(DesiredAimRotation.Pitch))
		{
			DesiredAim.Pitch = FRotator::NormalizeAxis((GetRemoteDesiredAimPitch() * 360) / 255.f);
			DesiredAim.Yaw = FRotator::NormalizeAxis((GetRemoteDesiredAimYaw() * 360) / 255.f);
		} else {
			// rotate towards the desired using locally controlled pitch
			DesiredAim = DesiredAimRotation;
		}
	}

	//if (IsLocallyControlled())
	//{
	//	// rotate towards the desired
	//	DesiredAim = DesiredAimRotation;
	//} else {
	//	DesiredAim.Pitch = FRotator::NormalizeAxis((RemoteDesiredAimPitch * 360) / 255.f);
	//	DesiredAim.Yaw = FRotator::NormalizeAxis((RemoteDesiredAimYaw * 360) / 255.f);
	//	//AimRotation.Pitch = FRotator::NormalizeAxis((RemoteAimPitch * 360) / 255.f); //should be replicating desired remote aim and rotating?
	//	//AimRotation.Yaw = FRotator::NormalizeAxis((RemoteAimYaw * 360) / 255.f);
	//	//AimRotation.Normalize();
	//}

	DesiredAim.Normalize();

	const float AbsDesiredYaw = FMath::Abs(FRotator::NormalizeAxis(DesiredAim.Yaw));
	if (AbsDesiredYaw > MaxAimBehindYaw) DesiredAim.Yaw = 0.f;

	// If Authority, replicate pitch out to everyone else
	if (GetLocalRole() == ROLE_Authority)
	{
		DesiredAimRotation = DesiredAim;

		// Server Sets these Replicated Variables so they go out to all the clients
		SetRemoteDesiredAimPitch(static_cast<uint8>((DesiredAimRotation.Pitch / 360.f) * 255));
		SetRemoteDesiredAimYaw(static_cast<uint8>((DesiredAimRotation.Yaw / 360.f) * 255));

		// Duplicate the results for replay only so they get recorded
		SetRemoteDesiredAimPitchReplay(GetRemoteDesiredAimPitch());
		SetRemoteDesiredAimYawReplay(GetRemoteDesiredAimYaw());
	}

	const float AngleTolerance = 5e-2f;
	if (!AimRotation.Equals(DesiredAim, 0.f) && DeltaSeconds > 0.f)
	{
		const FRotator DeltaRot = AimRotationRate * DeltaSeconds;

		// PITCH
		if (!FMath::IsNearlyEqual(AimRotation.Pitch, DesiredAim.Pitch, AngleTolerance))
		{
			AimRotation.Pitch = FMath::FixedTurn(AimRotation.Pitch, DesiredAim.Pitch, DeltaRot.Pitch);
		}

		// YAW
		if (!FMath::IsNearlyEqual(AimRotation.Yaw, DesiredAim.Yaw, AngleTolerance))
		{
			AimRotation.Yaw = FMath::FixedTurn(AimRotation.Yaw, DesiredAim.Yaw, DeltaRot.Yaw);
		}

		// ROLL
		if (!FMath::IsNearlyEqual(AimRotation.Roll, DesiredAim.Roll, AngleTolerance))
		{
			AimRotation.Roll = FMath::FixedTurn(AimRotation.Roll, DesiredAim.Roll, DeltaRot.Roll);
		}

		AimRotation.Normalize();
	}
}

void AIBaseCharacter::ServerUpdateDesiredAim_Implementation(uint8 ClientPitch, uint8 ClientYaw)
{
	DesiredAimRotation.Pitch = (ClientPitch * 360) / 255.f;
	DesiredAimRotation.Yaw = (ClientYaw * 360) / 255.f;
	DesiredAimRotation.Normalize();
	// decompress, set aim rotation
}

void AIBaseCharacter::UnHighlightObject(TWeakObjectPtr<UObject> Object)
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;
	// Un-focus old
	if (Object.IsValid() && Object->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
	{
		IUsableInterface::Execute_OnStopBeingFocusTarget(Object.Get(), this);
		SetRenderOutlinesForFocusObject(Object.Get(), false);
	}
#endif
}

void AIBaseCharacter::HighlightObject(TWeakObjectPtr<UObject> Object)
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;
	// focus new
	if (Object.IsValid() && Object->IsValidLowLevel() && Object->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
	{
		IUsableInterface::Execute_OnBecomeFocusTarget(Object.Get(), this);
		SetRenderOutlinesForFocusObject(Object.Get(), true);
	}
#endif
}

void AIBaseCharacter::UnHighlightFocusedObject()
{
	UnHighlightObject(FocusedObject.Object);
}

void AIBaseCharacter::HighlightFocusedObject()
{
	HighlightObject(FocusedObject.Object);
}

void AIBaseCharacter::SetFocusedObjectAndItem(UObject* NewFocusedObject, int32 FocusedItem, bool bUnpossessed /*= false*/)
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;

	if (!IsLocallyControlled() || bUnpossessed)
	{
		UnHighlightFocusedObject();

		FocusedObject.Item = INDEX_NONE;
		FocusedObject.Object = nullptr;

		ToggleInteractionPrompt(false);
		return;
	}

	// Nothing to focus
	if (!FocusedObject.Object.Get() && !NewFocusedObject)
	{
		ToggleInteractionPrompt(false);

		// Save new
		FocusedObject.Item = FocusedItem;
		FocusedObject.Object = NewFocusedObject;
	}
	// Swap Focus
	else if ((NewFocusedObject != FocusedObject.Object.Get()))
	{
		//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("(NewFocusedObject != FocusedObject.Object.Get())")));
	
		// Un-focus old
		UnHighlightFocusedObject();

		ToggleInteractionPrompt(false);
	
		// Save new
		FocusedObject.Item = FocusedItem;
		FocusedObject.Object = NewFocusedObject;
	
		// focus new
		HighlightFocusedObject();

		ToggleInteractionPrompt(ShouldShowInteractionPrompt());
		ShowQuestDescriptiveText();
	}
	// Remove carriable from highlighting
	else if ((IsCarryingObject() || IsUsingObject()) && FocusedObject.Object.Get())
	{
		//GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, FString::Printf(TEXT("(IsCarryingObject() || IsUsingObject()) && FocusedObject.Object.Get()")));

		// Un-focus carriable
		UnHighlightFocusedObject();
		ToggleInteractionPrompt(ShouldShowInteractionPrompt());
	}

	AIPlayerController* const IPlayerController = GetController<AIPlayerController>();
	if (!IPlayerController)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* const InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(IPlayerController->GetLocalPlayer());
	if (!InputSubsystem)
	{
		return;
	}

	const UInputMappingContext* const InteractionContext = IPlayerController->FindOrAddRuntimeMappingContext(IPlayerController->InteractionContext);
	if (!InteractionContext)
	{
		return;
	}

	const bool bShouldHaveMappingContext = ShouldShowInteractionPrompt();
	const bool bHasMappingContext = InputSubsystem->HasMappingContext(InteractionContext);

	// During some input states we shouldn't change the context. For now, it's just bCollectHeld. -Poncho
	const bool bCanEverChangeContext = !bCollectHeld;

	if (bShouldHaveMappingContext == bHasMappingContext || !bCanEverChangeContext)
	{
		return;
	}

	if (bShouldHaveMappingContext)
	{
		FModifyContextOptions ModifyContextOptions{};
		ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease = false;
		// Raise the interaction priority to 2 so it overrides default bindings (priority 1) that may be conflicting.
		InputSubsystem->AddMappingContext(InteractionContext, 2, ModifyContextOptions);
	}
	else
	{
		FModifyContextOptions ModifyContextOptions{};
		ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease = false;
		InputSubsystem->RemoveMappingContext(InteractionContext, ModifyContextOptions);
	}

#endif
}

void AIBaseCharacter::ToggleInteractionPrompt(bool bValidFocusable)
{
	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
		{
			if (IGameUserSettings->bShowInteractionPrompts && !IGameHUD->bHideActiveWidget)
			{
				if (bValidFocusable && !IsUsingObject())
				{
					if (!IGameHUD->bInteractPromptActive)
					{
						IGameHUD->ShowInteractionPrompt();
					}
				}
				else
				{
					if (IGameHUD->bInteractPromptActive)
					{
						IGameHUD->HideAndCancelInteractionPrompt();
					}
				}
			}
			else if (IGameHUD->bInteractPromptActive)
			{
				IGameHUD->HideAndCancelInteractionPrompt();
			}
		}
	}
}

bool AIBaseCharacter::ShouldShowInteractionPrompt()
{
	if (bIsBasicInputDisabled) return false;
	const UObject* const FocusedObj = FocusedObject.Object.Get();
	if (!FocusedObj) return false;
	if (IsUsingObject() || IsEatingOrDrinking()) return false;
	if (FocusedObj->IsA<AIWater>())
	{
		if (IsSwimming() && !IsCarryingObject()) return false;
		if (!IsSwimming() && !bCanDrinkWaterSource && !IsCarryingObject()) return false;
	}

	if (const AIBaseCharacter* const IBaseCharacter = Cast<AIBaseCharacter>(FocusedObject.Object.Get()))
	{
		if (IBaseCharacter->IsAlive())
		{
			return false;
		}
	}

	if (UICharacterMovementComponent* ICMC = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (!IsSwimming() && IsCarryingObject() && GetVelocity().Size() > (ICMC->MaxTrotSpeed * 1.1f)) return false;
	}

	return true;
}

bool AIBaseCharacter::CanJumpInternal_Implementation() const
{
	// Ensure the character isn't currently crouched.
	bool bCanJump = !bIsCrouched;

	// Ensure that the CharacterMovement state is valid
	bCanJump &= GetCharacterMovement()->CanAttemptJump();

	if (bCanJump)
	{
		if (bIsBasicInputDisabled) return false;
		if (GetStamina() < StaminaJumpDecay) return false;
		if (IsLimping()) return false;
		if (IsRestingOrSleeping()) return false;
		if (IsEatingOrDrinking()) return false;
		if (GetCharacterMovement()->IsSwimming()) return false;
		if (GetCharacterMovement()->IsFlying()) return false;
		if (IsExhausted()) return false;
		//if () return false;

		//bCanJump = (!GetCharacterMovement()->IsFalling() || bWasJumping);

		// Ensure JumpHoldTime and JumpCount are valid.
		if (!bWasJumping || GetJumpMaxHoldTime() <= 0.0f)
		{
			if (JumpCurrentCount == 0 && GetCharacterMovement()->IsFalling())
			{
				bCanJump = JumpCurrentCount + 1 < JumpMaxCount;
			}
			else
			{
				bCanJump = JumpCurrentCount < JumpMaxCount;
			}
		}
		else
		{
			// Only consider JumpKeyHoldTime as long as:
			// A) The jump limit hasn't been met OR
			// B) The jump limit has been met AND we were already jumping
			const bool bJumpKeyHeld = (bPressedJump && JumpKeyHoldTime < GetJumpMaxHoldTime());
			bCanJump = bJumpKeyHeld && ((JumpCurrentCount < JumpMaxCount) || (bWasJumping && JumpCurrentCount == JumpMaxCount));
		}
	}

	return bCanJump;
}

bool AIBaseCharacter::IsInputBlocked(bool bFromMove /*= false*/, bool bFromCall /*= false*/)
{
	if (IsStunned()) return true;

	if (bFromMove && UIChatWindow::StaticUserTypingInChat(this) && !bAutoRunActive) return true;
	if (UIAdminPanelMain::StaticAdminPanelActive(this)) return true;
	if (UIInteractionMenuWidget::StaticInteractionMenuActive(this)) return true;
	
	// Ensure we aren't in a menu that should block input
	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		if (IGameHUD->bEscapeWindowActive) return true;
		if (bFromCall && IGameHUD->bMapWindowActive) return true;
		if (Cast<UICharacterMenu>(IGameHUD->GetTopWindow())) return true;
		
		if (UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this))
		{
			const UAlderonGameInstanceSubsystem* UAInstanceSubsystem = IGameInstance->GetSubsystem<UAlderonGameInstanceSubsystem>();
			bool bIsLoading = UAInstanceSubsystem && UAInstanceSubsystem->IsLoadingInProgress();

			return bIsLoading || (IGameInstance->IsInputHardwareGamepad() && IGameHUD->bTabSystemActive);
		}
	}

	return false;
}

bool AIBaseCharacter::CanUse()
{
	// Don't allow use if input or turn in place is disabled
	if (bIsBasicInputDisabled || bDesiresTurnInPlace) return false;

	// If in chat window don't process input
	if (UIChatWindow::StaticUserTypingInChat(this)) return false;
	if (UIAdminPanelMain::StaticAdminPanelActive(this)) return false;

	// Don't allow use if use anim montage is currently playing
	if (GetCurrentMontage() != nullptr) return false;

	if (IsLocallyControlled() && bCarryInProgress) return false;

	return true;
}

bool AIBaseCharacter::OnStartJump_Implementation()
{
	if (IsInputBlocked() || 
		IsMontagePlaying(CurrentUseAnimMontage)) 
	{
		return false;
	}

	if (IsLatched())
	{
		ServerRequestClearAttachTarget();
		return true;
	}
	else if (CanBuck())
	{
			BeginBuck();
		return true;
	}

	if (CanJump())
	{
		Jump();
		return true;
	}

	return false;
}

void AIBaseCharacter::OnStopJump()
{	
	if (IsInputBlocked()) return;
	//UE_LOG(LogTemp, Log, TEXT("StopJump"));
	StopJumping();
}

bool AIBaseCharacter::CanBuck() const
{
	return HasDinosLatched() || ICarryInterface::Execute_IsCarried(this);
}

void AIBaseCharacter::BeginBuck()
{
	if (!ensureAlways(AbilitySystem) || !CanBuck())
	{
		return;
	}

	AbilitySystem->TryActivateAbilityByDerivedClass(UPOTGameplayAbility_Buck::StaticClass());
}

void AIBaseCharacter::BuckLatched()
{
	if(!HasDinosLatched() || !AbilitySystem || !AbilitySystem->BuckingTargetEffect)
	{
		return;
	}

	// Bucking affects all dinos latched onto you (like via pounce)
	TArray<AActor*> LatchedActors{};
	GetAttachedActors(LatchedActors);

	for (AActor* LatchedActor : LatchedActors)
	{
		const AIBaseCharacter* const LatchedCharacter = Cast<AIBaseCharacter>(LatchedActor);
		if (!LatchedCharacter)
		{
			continue;
		}

		const FAttachTarget& AttachTarget = LatchedCharacter->GetAttachTarget();
		if (!AttachTarget.IsValid() || AttachTarget.AttachType != EAttachType::AT_Latch)
		{
			continue;
		}

		UPOTAbilitySystemComponent* const AttachedTargetAbilitySystem = Cast<UPOTAbilitySystemComponent>(LatchedCharacter->GetAbilitySystemComponent());
		if (!AttachedTargetAbilitySystem)
		{
			continue;
		}

		FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
		EffectContext.AddInstigator(this, this);
		const FGameplayEffectSpecHandle GESpecHandle = AttachedTargetAbilitySystem->MakeOutgoingSpec(
			AbilitySystem->BuckingTargetEffect,
			AbilitySystem->GetLevel(),
			EffectContext);

		AttachedTargetAbilitySystem->ApplyGameplayEffectSpecToSelf(*GESpecHandle.Data, AttachedTargetAbilitySystem->GetPredictionKeyForNewAction());
	}
}

void AIBaseCharacter::BuckCarrying()
{
	// Bucking also affects the dino that is carrying you (like sarco clamp)
	if (!ICarryInterface::Execute_IsCarried(this) || !GetAttachTarget().AttachComponent || !AbilitySystem || !AbilitySystem->BuckingTargetEffect)
	{
		return;
	}

	const AIBaseCharacter* const CarryingCharacter = Cast<AIBaseCharacter>(GetAttachTarget().AttachComponent->GetOwner());
	if (!CarryingCharacter)
	{
		return;
	}

	UPOTAbilitySystemComponent* const CarryingCharAbilitySystem = Cast<UPOTAbilitySystemComponent>(CarryingCharacter->GetAbilitySystemComponent());
	if (!CarryingCharAbilitySystem)
	{
		return;
	}

	FGameplayEffectContextHandle EffectContext = AbilitySystem->MakeEffectContext();
	EffectContext.AddInstigator(this, this);
	const FGameplayEffectSpecHandle GESpecHandle = CarryingCharAbilitySystem->MakeOutgoingSpec(
		AbilitySystem->BuckingTargetEffect,
		AbilitySystem->GetLevel(),
		EffectContext);

	CarryingCharAbilitySystem->ApplyGameplayEffectSpecToSelf(*GESpecHandle.Data, CarryingCharAbilitySystem->GetPredictionKeyForNewAction());
}

bool AIBaseCharacter::IsBucking() const
{
	if (!ensureAlways(AbilitySystem))
	{
		return false;
	}

	UPOTAbilitySystemGlobals& PASG = UPOTAbilitySystemGlobals::Get();
	return AbilitySystem->HasMatchingGameplayTag(PASG.BuckingTag);
}

bool AIBaseCharacter::OnStartVocalWheel() 
{ 
	return false; 
}

void AIBaseCharacter::OnStopVocalWheel() 
{
}

bool AIBaseCharacter::CanMove_Implementation(bool bFromMove /*= false*/)
{
	if (bIsBasicInputDisabled) return false;
	if (IsInputBlocked(bFromMove)) return false;

	if (IsRestingOrSleeping()) return false;

	// Don't allow Movement if we are doing something
	//if (IsMontagePlaying(CurrentUseAnimMontage) && !IsFlying()) return false;

	if (UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this))
	{
		if (IGameInstance->IsLagging()) return false;
	}

	return true;
}

bool AIBaseCharacter::DesiresPreciseMove()
{
	return bDesiresPreciseMovement;
}

bool AIBaseCharacter::CanPreciseMove_Implementation()
{
	// Fetch Character Movement Component and Ensure It's Valid
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetMovementComponent());
	check(ICharMove);

	const bool bAbilityBlocking = AbilitySystem != nullptr && AbilitySystem->GetCurrentAttackAbility() != nullptr && AbilitySystem->GetCurrentAttackAbility()->bStopPreciseMovement;
	
	if (bAbilityBlocking) return false;
	if (bIsBasicInputDisabled) return false;

	if (IsSwimming())
	{
		if (!CanMove(ICharMove->GetPendingInputVector().Size() > 0.001f)) return false;
		if (!ICharMove->bCanUsePreciseSwimming) return false;
		if (bDesiresPreciseMovement == IsAquatic()) return false;

		return true;
	}
	else
	{
		if (!CanMove(true)) return false;
		if (!ICharMove->bCanUsePreciseNavigation) return false;
		return bDesiresPreciseMovement;
	}
}

bool AIBaseCharacter::IsAquatic() const
{
	return bAquatic;
}

void AIBaseCharacter::TransitionCheck()
{
	if (USkeletalMeshComponent* SkeleMesh = GetMesh())
	{
		if (UDinosaurAnimBlueprint* DinoAnimBlueprint = Cast<UDinosaurAnimBlueprint>(SkeleMesh->GetAnimInstance()))
		{
			if (!DinoAnimBlueprint->IsTransitioning())
			{
				AbilitySystem->UpdateStanceEffect();
				bTransitioningToStance = false;

				GetWorldTimerManager().ClearTimer(TimerHandle_TransitionCheck);
			}
		}
	}
}

void AIBaseCharacter::SetIsJumping(bool NewJumping)
{
	// Go to standing pose if trying to jump while crouched
	if (bIsCrouched && NewJumping)
	{
		UnCrouch();

		return;
	}

	if (NewJumping == IsInitiatedJump())
	{
		return;
	}
	
	bIsJumping = NewJumping;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, bIsJumping, this);

	if (IsInitiatedJump())
	{
		if (bEnableJumpModifier)
		{
			const bool bTimerActive = GetWorldTimerManager().IsTimerActive(TimerHandle_ResetJumpModifier);
			if (!bTimerActive)
			{
				GetWorldTimerManager().SetTimer(TimerHandle_ResetJumpModifier, this, &AIBaseCharacter::ResetJumpModifier, StaminaJumpModifierCooldown, true);
			}
			else
			{
				if (!GetGodmode())
				{
					StaminaJumpDecay = StaminaJumpDecay * StaminaJumpModifier; // calc new decay on clients as well to prevent jumping on low stamina!
				}
				TimeSinceLastJump = GetWorld()->GetTimeSeconds();
				ConsecutiveJumps++;
			}
		}


		// decrease stamina, todo: this could be autonomous too, would need to correct with jumps that get corrected
		if (GetLocalRole() >= ROLE_AutonomousProxy)
		{
			//UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetStaminaAttribute(), -StaminaJumpDecay);
			//Stamina = FMath::Clamp<int32>(Stamina - StaminaJumpDecay, 0, GetMaxStamina());

			//Add stamina jump decay
			if (AbilitySystem != nullptr && AbilitySystem->JumpCostEffect != nullptr)
			{
				FGameplayEffectSpecHandle GESpecHandle = AbilitySystem->MakeOutgoingSpec(
					AbilitySystem->JumpCostEffect,
					AbilitySystem->GetLevel(),
					AbilitySystem->MakeEffectContext());

				//GESpecHandle.Data->SetSetByCallerMagnitude();
				AbilitySystem->ApplyGameplayEffectSpecToSelf(*GESpecHandle.Data, AbilitySystem->GetPredictionKeyForNewAction());
			}
		}



	}
}

void AIBaseCharacter::ResetJumpModifier()
{
	if (TimeSinceLastJump + StaminaJumpModifierCooldown > GetWorld()->GetTimeSeconds()) return;

	TimeSinceLastJump = 0.0f;
	StaminaJumpDecay = GetClass()->GetDefaultObject<AIBaseCharacter>()->StaminaJumpDecay;
	ConsecutiveJumps = 0;

	GetWorldTimerManager().ClearTimer(TimerHandle_ResetJumpModifier);
}

void AIBaseCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (UICharacterMovementComponent* IMovementComp = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		// Check if we are no longer falling/jumping
		if ((PrevMovementMode == EMovementMode::MOVE_Falling && GetCharacterMovement()->MovementMode != EMovementMode::MOVE_Falling))
		{
			LastAttachCharacter = nullptr;
			
			SetIsJumping(false);
		}

		if (IMovementComp->MovementMode == EMovementMode::MOVE_Swimming)
		{
			LastAttachCharacter = nullptr;
		}

		if (AbilitySystem != nullptr)
		{
			AbilitySystem->SetLooseGameplayTagCount(UPOTAbilitySystemGlobals::Get().GroundedTag, IMovementComp->IsMovingOnGround() ? 1 : 0);
		}
		
		if (HasAuthority())
		{
			UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
			check(GI);

			AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());

			if (GetCharacterMovement() && Session && Session->bServerFallDamage)
			{
				if ((PrevMovementMode == MOVE_Falling && IMovementComp->MovementMode == MOVE_Swimming) && !IsAquatic())
				{
					if (IMovementComp->Velocity.Z < (FallDamageDeathThreshold * 1.4f))
					{
						Suicide();
					}
					else if (IMovementComp->Velocity.Z < (FallDamageLimpingThreshold * 1.4f))
					{
						BreakLegsFromFalling();
					}
				}
			}
		}
	}

	if (AIPlayerController* PC = Cast<AIPlayerController>(GetController()))
	{
		// Get the Enhanced Input Local Player Subsystem from the Local Player related to our Player Controller.
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			const uint8 OldIndex = static_cast<uint8>(PrevMovementMode);
			const uint8 NewIndex = static_cast<uint8>(GetCharacterMovement()->MovementMode);
			
			if (PC->LocomotionContexts[OldIndex] != PC->LocomotionContexts[NewIndex])
			{
				if (PC->LocomotionContexts[OldIndex] != nullptr)
				{
					FModifyContextOptions ModifyContextOptions;
					ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease = false;
					Subsystem->RemoveMappingContext(PC->FindOrAddRuntimeMappingContext(PC->LocomotionContexts[OldIndex]), ModifyContextOptions);
				}

				if (PC->LocomotionContexts[NewIndex] != nullptr)
				{
					FModifyContextOptions ModifyContextOptions;
					ModifyContextOptions.bIgnoreAllPressedKeysUntilRelease = false;
					Subsystem->AddMappingContext(PC->FindOrAddRuntimeMappingContext(PC->LocomotionContexts[NewIndex]), 0, ModifyContextOptions);
				}
			}
		}
	}
}

void AIBaseCharacter::ServerSetBasicInputDisabled_Implementation(bool bInIsBasicInputDisabled)
{
	bIsBasicInputDisabled = bInIsBasicInputDisabled;
}

TSoftObjectPtr<UAnimMontage> AIBaseCharacter::GetEatAnimMontageSoft()
{
	if (IsCarryingObject())
	{
		return IsSwimming() ? UnderwaterSwallowAnimMontage : SwallowAnimMontage;
	}
	else
	{
		return IsSwimming() ? UnderwaterEatAnimMontage : EatAnimMontage;
	}
}

TSoftObjectPtr<UAnimMontage> AIBaseCharacter::GetPickupAnimMontageSoft()
{
	return IsSwimming() ? PickupSwimmingAnimMontage : PickupAnimMontage;
}

void AIBaseCharacter::PlayEatingSound()
{
	if (!bIsEating)
	{
		return;
	}

	bool bEatingMeat = false;

	if (AIBaseCharacter* IBC = Cast<AIBaseCharacter>(GetCurrentlyUsedObject().Object.Get()))
	{
		bEatingMeat = true;
	}
	else if (AICritterPawn* ICP = Cast<AICritterPawn>(GetCurrentlyUsedObject().Object.Get()))
	{
		bEatingMeat = true;
	}
	else if (AIFish* IFish = Cast<AIFish>(GetCurrentlyUsedObject().Object.Get()))
	{
		bEatingMeat = true;
	}
	else if (AIMeatChunk* IMeatChunk = Cast<AIMeatChunk>(GetCurrentlyUsedObject().Object.Get()))
	{
		bEatingMeat = true;
	}
	else if (AICorpse* ICorpse = Cast<AICorpse>(GetCurrentlyUsedObject().Object.Get()))
	{
		bEatingMeat = true;
	}

	FSpawnAttachedSoundParams Params{};
	Params.SoundCue = bEatingMeat ? SoundCarnivoreEat : SoundHerbivoreEat;

	SpawnSoundAttachedAsync(Params);
}

void AIBaseCharacter::StopEating()
{
	bIsEating = false;
	bIsDisturbing = false;
	if (HasAuthority())
	{
		if (AIBaseCharacter* IBC = Cast<AIBaseCharacter>(GetCurrentlyUsedObject().Object.Get()))
		{
			IBC->ActivelyEatingBody.Remove(this);
		}
		else if (AICritterPawn* ICP = Cast<AICritterPawn>(GetCurrentlyUsedObject().Object.Get()))
		{
			ICP->ActivelyEatingBody.Remove(this);
		}
		else if (AIFish* IFish = Cast<AIFish>(GetCurrentlyUsedObject().Object.Get()))
		{
			IFish->ActivelyEatingBody.Remove(this);
		}
		else if (AIMeatChunk* IMeatChunk = Cast<AIMeatChunk>(GetCurrentlyUsedObject().Object.Get()))
		{
			IMeatChunk->ActivelyEatingChunk.Remove(this);
		}

		ResetServerUseTimer();
	}
}

FUsableFoodData AIBaseCharacter::GetFoodMeshData()
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UObject* UseObject = GetCurrentlyCarriedObject().Object.Get();

		if (!UseObject) UseObject = GetCurrentlyUsedObject().Object.Get();
		if (!UseObject) return FUsableFoodData();
		
		if (UseObject->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
		{
			return FUsableFoodData(IUsableInterface::Execute_IsUseMeshStatic(UseObject, this, INDEX_NONE), IUsableInterface::Execute_GetUseStaticMesh(UseObject, this, INDEX_NONE), IUsableInterface::Execute_GetUseSkeletalMesh(UseObject, this, INDEX_NONE), IUsableInterface::Execute_GetUseOffset(UseObject, this, INDEX_NONE), IUsableInterface::Execute_GetUseRotation(UseObject, this, INDEX_NONE));
		}
	}
#endif

	return FUsableFoodData();
}

void AIBaseCharacter::SpawnFoodParticle(FName SocketName, const FVector& LocOffset, const FRotator& RotOffset, const FVector& ScaleOffset)
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UObject* UseObject = GetCurrentlyUsedObject().Object.Get();
		if (!UseObject)
		{
			return;
		}

		USkeletalMeshComponent* MeshComp = GetMesh();
		check(MeshComp);
		if (!MeshComp)
		{
			return;
		}

		if (!UseObject->GetClass()->ImplementsInterface(UUsableInterface::StaticClass()))
		{
			return;
		}

		FInteractParticleData ParticleData;
		if (IUsableInterface::Execute_GetInteractParticle(UseObject, ParticleData))
		{
			if (!ParticleData.PSTemplate.IsNull())
			{
				FOnParticleLoaded Del = FOnParticleLoaded::CreateUObject(this, &AIBaseCharacter::SpawnFoodParticle_Complete, ParticleData, SocketName, LocOffset, RotOffset, ScaleOffset);
				UIGameplayStatics::LoadParticleAsync(ParticleData.PSTemplate, Del, this);
			}
		}
	}
#endif
}

void AIBaseCharacter::SpawnFoodParticle_Complete(UParticleSystem* LoadedParticle, FInteractParticleData ParticleData, FName SocketName, FVector LocOffset, FRotator RotOffset, FVector ScaleOffset)
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (LoadedParticle)
		{
			USkeletalMeshComponent* MeshComp = GetMesh();
			check(MeshComp);
			if (!MeshComp)
			{
				return;
			}

			if (ParticleData.bAttached)
			{
				UGameplayStatics::SpawnEmitterAttached(LoadedParticle, MeshComp, SocketName, ParticleData.LocationOffset + LocOffset, ParticleData.RotationOffset + RotOffset, ParticleData.Scale * ScaleOffset);
			}
			else
			{
				const FTransform MeshTransform = MeshComp->GetSocketTransform(SocketName);
				FTransform SpawnTransform;
				SpawnTransform.SetLocation(MeshTransform.TransformPosition(ParticleData.LocationOffset + LocOffset));
				SpawnTransform.SetRotation(MeshTransform.GetRotation() * (ParticleData.RotationOffset + RotOffset).Quaternion());
				SpawnTransform.SetScale3D(ParticleData.Scale * ScaleOffset);
				UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), LoadedParticle, SpawnTransform);
			}
		}
	}
#endif
}

void AIBaseCharacter::OnStartFoodAnimation(float TimelineLength, bool bSwallowing)
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (!FoodSkeletalMeshComponent || !FoodStaticMeshComponent)
		{
			InitFoodComponents();
		}

		// Need something a little different for swallowing - ANixon
		if (bSwallowing || !FoodSkeletalMeshComponent || !FoodStaticMeshComponent) return;

		AsyncLoadFoodMesh();
		TotalFoodAnimationLength = TimelineLength;
		FoodAnimationDuration = 0.0f;
		FoodSkeletalMeshComponent->SetVisibility(!bSwallowing);
		FoodStaticMeshComponent->SetVisibility(!bSwallowing);
		GetWorldTimerManager().SetTimer(TimerHandle_UpdateFoodAnimation, this, &AIBaseCharacter::OnUpdateFoodAnimation, 0.03f, true);
	}
#endif
}

void AIBaseCharacter::AsyncLoadFoodMesh()
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (!IsAlive()) return;

		RequestedFoodData = GetFoodMeshData();

		// Widget already loaded, create and show it
		if ((RequestedFoodData.bStaticFoodMesh && RequestedFoodData.StaticFoodMesh.Get() != nullptr) || (RequestedFoodData.SkeletalFoodMesh.Get() != nullptr))
		{
			return OnFoodMeshLoaded();
		}

		FSoftObjectPath FoodSoftObjectPath = RequestedFoodData.bStaticFoodMesh ? RequestedFoodData.StaticFoodMesh.ToSoftObjectPath() : RequestedFoodData.SkeletalFoodMesh.ToSoftObjectPath();

		if (!FoodSoftObjectPath.IsValid()) return;

		// Widget isn't loaded, async load and show it
		FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
		AssetLoader.RequestAsyncLoad
		(
			FoodSoftObjectPath,
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnFoodMeshLoaded),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
#endif
}

void AIBaseCharacter::OnFoodMeshLoaded()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (!FoodSkeletalMeshComponent || !FoodStaticMeshComponent) return;

	if ((RequestedFoodData.bStaticFoodMesh && RequestedFoodData.StaticFoodMesh.Get() == nullptr) || (RequestedFoodData.SkeletalFoodMesh.Get() == nullptr)) return;

	if (RequestedFoodData.bStaticFoodMesh)
	{
		FoodStaticMeshComponent->SetStaticMesh(RequestedFoodData.StaticFoodMesh.Get());
	}
	else
	{
		FoodSkeletalMeshComponent->SetSkeletalMesh(RequestedFoodData.SkeletalFoodMesh.Get());
	}
#endif
}

void AIBaseCharacter::OnUpdateFoodAnimation()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}

	if (!FoodSkeletalMeshComponent || !FoodStaticMeshComponent) return;

	FoodAnimationDuration += 0.03f;

	if (FoodAnimationDuration > TotalFoodAnimationLength)
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_UpdateFoodAnimation);
		return;
	}

	const float BlendAlpha = FMath::Clamp((FoodAnimationDuration / TotalFoodAnimationLength), 0.0f, 1.0f);
	const float BlendAlphaScale = FMath::Lerp(1.0f, 0.0f, ((BlendAlpha - 0.75f) / .25f));

	FTransform NewFoodMeshPosition;

	const FVector FoodSocketRelativeLocation = GetMesh()->GetSocketTransform(NAME_FoodSocket, ERelativeTransformSpace::RTS_ParentBoneSpace).GetLocation();
	const FVector FoodSocketEndRelativeLocation = GetMesh()->GetSocketTransform(NAME_FoodSocketEnd, ERelativeTransformSpace::RTS_ParentBoneSpace).GetLocation();

	NewFoodMeshPosition.SetLocation(FMath::Lerp(FoodSocketRelativeLocation, FoodSocketEndRelativeLocation, BlendAlpha));
	NewFoodMeshPosition.SetLocation(NewFoodMeshPosition.GetLocation() + RequestedFoodData.FoodMeshOffset);

	const FRotator FoodSocketRelativeRotation = GetMesh()->GetSocketTransform(NAME_FoodSocket, ERelativeTransformSpace::RTS_ParentBoneSpace).GetRotation().Rotator();
	const FRotator FoodSocketRelativeRotationWithOffset = UKismetMathLibrary::ComposeRotators(RequestedFoodData.FoodMeshRotation, FoodSocketRelativeRotation);
	NewFoodMeshPosition.SetRotation(FoodSocketRelativeRotationWithOffset.Quaternion());

	NewFoodMeshPosition.SetScale3D(BlendAlpha > 0.75f ? FVector(BlendAlphaScale) : FVector(1.0f, 1.0f, 1.0f));

	if (RequestedFoodData.bStaticFoodMesh)
	{
		FoodStaticMeshComponent->SetRelativeTransform(NewFoodMeshPosition);
	}
	else
	{
		FoodSkeletalMeshComponent->SetRelativeTransform(NewFoodMeshPosition);
	}
#endif
}

void AIBaseCharacter::OnStopFoodAnimation(bool bSwallowing)
{
#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}
	
	GetWorldTimerManager().ClearTimer(TimerHandle_UpdateFoodAnimation);

	if (FoodSkeletalMeshComponent)
	{
		FoodSkeletalMeshComponent->SetSkeletalMesh(nullptr);
		FoodSkeletalMeshComponent->SetVisibility(false);
	}

	if (FoodStaticMeshComponent)
	{
		FoodStaticMeshComponent->SetStaticMesh(nullptr);
		FoodStaticMeshComponent->SetVisibility(false);
	}
	
	RequestedFoodData = FUsableFoodData();

	if (!IsCarryingObject())
	{
		return;
	}

	const FCarriableData CarriableData = ICarryInterface::Execute_GetCarryData(GetCurrentlyCarriedObject().Object.Get());

	if (AActor* const CarriableActor = Cast<AActor>(GetCurrentlyCarriedObject().Object.Get()))
	{
		CarriableActor->SetActorRelativeLocation(CarriableData.AttachLocationOffset * GetActorScale3D().X);
		CarriableActor->SetActorRelativeRotation(CarriableData.AttachRotationOffset);
	}

#endif
}

void AIBaseCharacter::ToggleSprint()
{
	bDesiresToSprint = !bDesiresToSprint;
	UpdateMovementState();
}


bool AIBaseCharacter::InputAutoRunStarted()
{
	ToggleAutoRun();

	// Check if you are moving forward and auto run is false
	if (bAutoRunActive && bMoveForwardPressed)
	{
		bMoveForwardWhileAutoRunning = true;
	}

	return true;
}

bool AIBaseCharacter::ToggleAutoRun()
{
	if (bAutoRunActive)
	{
		bAutoRunActive = false;
		bMoveForwardWhileAutoRunning = false;
	}
	else
	{
		bAutoRunActive = true;
	}

	bDesiresToSprint = bAutoRunActive;
	
	UpdateMovementState();

	return true;
}

bool AIBaseCharacter::OnStartSprinting()
{
	// You need at least 2% Max Stamina to start running again
	float PercentageStamina = GetMaxStamina() * 0.02f;

	UICharacterMovementComponent* const ICharMove = Cast<UICharacterMovementComponent>(GetMovementComponent());
	if (!ensureAlways(ICharMove))
	{
		return false;
	}

	const UIGameUserSettings* const IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
	if (!ensureAlways(IGameUserSettings))
	{
		return false;
	}

	if (ICharMove->IsTransitioningOutOfWaterBackwards())
	{
		ICharMove->CancelBackwardsTransition();
		OnStopPreciseMovement();
		UpdateMovementState();
	}	

	if (GetStamina() >= PercentageStamina)
	{
		if (IGameUserSettings->bToggleSprint)
		{
			// ToggleSprint
			bDesiresToSprint = !bDesiresToSprint;
		}
		else
		{
			//while auto running and not using sprint toggle, we should bring player to a trot
			if (bAutoRunActive)
			{
				bDesiresToSprint = false;
			}
			else
			{
				// Start Sprinting
				bDesiresToSprint = true;
			}
		}

		UpdateMovementState();
		return true;
	}

	if (bDesiresToSprint && IGameUserSettings->bToggleSprint) //This is here to ensure that the toggle is input processed even if there is no stamina
	{
		// ToggleSprint
		bDesiresToSprint = false;

		return true;
	}

	return false;
}

void AIBaseCharacter::OnStopSprinting()
{
	const UIGameUserSettings* const IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
	if (!IGameUserSettings || IGameUserSettings->bToggleSprint)
	{
		return;
	}
	
	//If we are already auto-running and not using toggle sprint, then bring player back to a sprint.
	if (bAutoRunActive)
	{
		bDesiresToSprint = true;
	}
	else
	{
		bDesiresToSprint = false;
	}

	UpdateMovementState();
}

bool AIBaseCharacter::OnStartTroting()
{
	bDesiresToTrot = !bDesiresToTrot;
	UpdateMovementState();

	return true;
}

void AIBaseCharacter::OnStopTroting()
{
}

bool AIBaseCharacter::InputCrouchPressed()
{
	if (IsInputBlocked()) return false;

	// If we are crouching then CanCrouch will return false. If we cannot crouch then calling Crouch() wont do anything
	if (CanCrouch())
	{
		bDesiredUncrouch = false;
		Crouch();
	}
	else if (CanUncrouch())
	{
		UnCrouch();
		bDesiredUncrouch = false;
	}

	return true;
}

void AIBaseCharacter::InputCrouchReleased()
{
	if (IsInputBlocked()) return;

	if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		if (!IGameUserSettings->bToggleCrouch)
		{
			if (CanUncrouch())
			{
				UnCrouch();
				bDesiredUncrouch = false;
			}
			else
			{
				bDesiredUncrouch = true;
			}

		}
	}
}

void AIBaseCharacter::ForceUncrouch()
{
	if (CanUncrouch())
	{
		UnCrouch();
		bDesiredUncrouch = false;
	}
}

void AIBaseCharacter::Crouch(bool bClientSimulation /*= false*/)
{
	UpdateMovementState();

	Super::Crouch(bClientSimulation);
}

bool AIBaseCharacter::CanCrouch() const
{
	if (UICharacterMovementComponent* UICMC = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (!UICMC->CanTransitionToState(ECapsuleState::CS_CROUCHING))
		{
			return false;
		}
	}

	//Stops the player from immediately crouching when landing while holding ctrl
	bool hasFlyingLandingLag = TimeCantCrouchElapsed < TimeCantCrouchAfterLanding;

	return Super::CanCrouch() && !IsJumpProvidingForce() && !hasFlyingLandingLag && 
		!IsInitiatedJump() && !IsRestingOrSleeping() &&
		/*!bIsLimping && !bIsFeignLimping && !AreLegsBroken() &&*/
		!IsSprinting() && !GetCharacterMovement()->IsInWater() && !IsInWater(75) &&
		!GetCharacterMovement()->IsSwimming() && !bIsBasicInputDisabled && !IsEatingOrDrinking();
}

bool AIBaseCharacter::CanUncrouch() const
{
	if (UICharacterMovementComponent* UICMC = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (!UICMC->CanTransitionToState(ECapsuleState::CS_DEFAULT))
		{
			return false;
		}
	}

	return !bIsBasicInputDisabled;
}

float AIBaseCharacter::GetMovementSpeedMultiplier() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMovementSpeedMultiplierAttribute());
}

bool AIBaseCharacter::OnStartPreciseMovement()
{
	if (IsInputBlocked())
	{
		return false;
	}

	// Precise Movement
	bDesiresPreciseMovement = true;
	bPrecisedMovementKeyPressed = true;

	if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetMovementComponent()))
	{
		if (ICharMove->IsTransitioningOutOfWaterBackwards())
		{
			ICharMove->CancelBackwardsTransition();
		}
	}

	return true;
}

void AIBaseCharacter::OnStopPreciseMovement()
{
	if (IsInputBlocked()) return;

	// Precise Movement
	bDesiresPreciseMovement = false;
	bPrecisedMovementKeyPressed = false;

	// Prevent from completing precise movement quest while rested since it does not 
	// give a visible indication to the player what this ability does while resting. 
	if (!HasLeftHatchlingCave() && GetRestingStance() == EStanceType::Default)
	{
		ServerSubmitGenericTask(NAME_PreciseMovement);
	}
}

bool AIBaseCharacter::DesiresTurnInPlace() const
{
	return (GetCurrentMovementMode() == ECustomMovementMode::TURNINPLACE);
}

void AIBaseCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	SetIsJumping(true);
}

float AIBaseCharacter::GetCombatWeight() const
{
	if (AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetCombatWeightAttribute());
	}

	return 1000.f;
}

float AIBaseCharacter::GetHealth() const
{
	if (AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetHealthAttribute());
	}

	return 0;
}

void AIBaseCharacter::SetHealth(float Health)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetHealthAttribute(), Health, EGameplayModOp::Override);
}

bool AIBaseCharacter::GetIsDying() const
{
	return bIsDying;
}

float AIBaseCharacter::GetStamina() const
{
	if (AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetStaminaAttribute());
	}

	return 0;
}

void AIBaseCharacter::SetStamina(float Stamina)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetStaminaAttribute(), Stamina, EGameplayModOp::Override);
}

float AIBaseCharacter::GetHunger() const
{
	if (AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetHungerAttribute());
	}

	return 0;
}

void AIBaseCharacter::SetHunger(float Hunger)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetHungerAttribute(), Hunger, EGameplayModOp::Override);
}

float AIBaseCharacter::GetThirst() const
{
	if (AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetThirstAttribute());
	}

	return 0;
}

void AIBaseCharacter::SetThirst(float Thirst)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetThirstAttribute(), Thirst, EGameplayModOp::Override);
}

float AIBaseCharacter::GetOxygen() const
{
	if (AbilitySystem)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetOxygenAttribute());
	}
	
	return 0;
}

void AIBaseCharacter::SetOxygen(float Oxygen)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetOxygenAttribute(), Oxygen, EGameplayModOp::Override);
}

void AIBaseCharacter::ResetHungerThirst()
{
	RestoreHunger(0);
	RestoreThirst(0);
}

bool AIBaseCharacter::IsHungry() const
{
	return (GetMaxHunger() - GetHunger()) >= GetFoodConsumePerTick();
}

bool AIBaseCharacter::IsThirsty() const
{
	return (GetMaxThirst() - GetThirst()) >= GetWaterConsumePerTick();
}

void AIBaseCharacter::SetGrowthPercent(float Percentage)
{
	if (!UIGameplayStatics::IsGrowthEnabled(this))
	{
		return;
	}

	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetGrowthAttribute(), Percentage, EGameplayModOp::Override);
}

void AIBaseCharacter::ApplyGrowthPenalty(float GrowthPenaltyPercent, bool bSubtract, bool bOverrideLoseGrowthPastGrowthStages, bool bUpdateWellRestedGrowth)
{
	if (!AbilitySystem || GrowthPenaltyPercent <= 0.0f || !UIGameplayStatics::IsGrowthEnabled(this))
	{
		return;
	}

	const UIGameInstance* const IGameInstance = Cast<UIGameInstance>(GetGameInstance());
	if (!IGameInstance) return;

	const AIGameSession* const Session = Cast<AIGameSession>(IGameInstance->GetGameSession());
	if (!Session) return;

	float CurrentGrowth = GetGrowthPercent();
	if (CalculatedGrowthForRespawn != 0.0f)
	{
		CurrentGrowth = CalculatedGrowthForRespawn;
	}

	// If the player is a hatchling, don't apply the penalty
	// bLoseGrowthPastGrowthStages is handled below
	if (!Session->bLoseGrowthPastGrowthStages && CurrentGrowth < HatchlingGrowthPercent)
	{
		return;
	}

	float NewGrowth = CurrentGrowth;
	if (Session->bLoseGrowthPastGrowthStages || bOverrideLoseGrowthPastGrowthStages)
	{
		if (bSubtract)
		{
			NewGrowth = CurrentGrowth - ((float)GrowthPenaltyPercent / 100.f);
		}
		else
		{
			float GrowthMultiplier = FMath::Clamp(1.f - ((float)GrowthPenaltyPercent / 100.f), 0.f, 1.f);
			NewGrowth = CurrentGrowth * GrowthMultiplier;
		}
	}
	else
	{
		float StageMinGrowth = FMath::Min(FMath::Floor(CurrentGrowth * 4.f) * 0.25f, 0.75f);

		if (bSubtract)
		{
			NewGrowth = FMath::Max(StageMinGrowth, CurrentGrowth - ((float)GrowthPenaltyPercent / 100.f));
		}
		else
		{
			float GrowthMultiplier = FMath::Clamp((float)GrowthPenaltyPercent / 100.f, 0.f, 1.f);
			NewGrowth = FMath::Max(StageMinGrowth, CurrentGrowth - (GrowthMultiplier * 0.25f));
		}
	}

	// Get min growth based on bLoseGrowthPastGrowthStages
	float MinGrowthAfterDeath = Session->bLoseGrowthPastGrowthStages ? Session->GetMinGrowthAfterDeath() : FMath::Max(Session->GetMinGrowthAfterDeath(), HatchlingGrowthPercent);

	// Clamp min growth if the tutorial cave is turned on.
	MinGrowthAfterDeath = Session->bServerHatchlingCaves ? FMath::Max(MinGrowthAfterDeath, Session->HatchlingCaveExitGrowth) : MinGrowthAfterDeath;

	NewGrowth = FMath::Clamp(NewGrowth, FMath::Max(MIN_GROWTH, MinGrowthAfterDeath), 1.0f);

	if (CalculatedGrowthForRespawn != 0.0f)
	{
		CalculatedGrowthForRespawn = NewGrowth;
	}

	if (bUpdateWellRestedGrowth && GetGE_WellRestedBonus().IsValid())
	{
		float GrowthLoss = CurrentGrowth - NewGrowth;
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusStartedGrowthAttribute(), GetWellRestedBonusStartedGrowth() - GrowthLoss, EGameplayModOp::Override);
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusEndGrowthAttribute(), GetWellRestedBonusEndGrowth() - GrowthLoss, EGameplayModOp::Override);
	}


	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetGrowthAttribute(), NewGrowth, EGameplayModOp::Override);
}

float AIBaseCharacter::GetGrowthPercent() const
{
	// IsRegistered prevents warning spam from trying to access ActiveGameplayEffects too early
	if (AbilitySystem && AbilitySystem->IsRegistered() && !bIsCharacterEditorPreviewCharacter)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetGrowthAttribute());
	}
	
	return CurrentAppliedGrowth;
}

float AIBaseCharacter::GetGrowthLevel() const
{
	return GetGrowthPercent() * 5.f;
}

bool AIBaseCharacter::IsGrowing() const
{
	return !bIsCharacterEditorPreviewCharacter && GetGrowthPercent() < 1.f && AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetGrowthPerSecondAttribute()) > 0.f;
}

bool AIBaseCharacter::IsWellRested(bool bSkipGameplayEffect /*= false*/) const
{
	return GetWellRestedBonusMultiplier() > 0.0f && GetGrowthPercent() < GetWellRestedBonusEndGrowth() && (bSkipGameplayEffect || GetGE_WellRestedBonus().IsValid());
}

float AIBaseCharacter::GetWellRestedBonusMultiplier() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetWellRestedBonusMultiplierAttribute());
}

float AIBaseCharacter::GetWellRestedBonusStartedGrowth() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetWellRestedBonusStartedGrowthAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetWellRestedBonusEndGrowth() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetWellRestedBonusEndGrowthAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetMaxHealth() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxHealthAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetMaxStamina() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxStaminaAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetStaminaRecoveryRate() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetStaminaRecoveryRateAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetMaxHunger() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxHungerAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetMaxThirst() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxThirstAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetMaxOxygen() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetMaxOxygenAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetMaxBodyFood() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetBodyFoodAmountAttribute());
	}

	return 0.f;
}

float AIBaseCharacter::GetCurrentBodyFood() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetCurrentBodyFoodAmountAttribute());
	}

	return 0.f;
}

bool AIBaseCharacter::IsAlive() const
{
	return GetHealth() > 0;
}

void AIBaseCharacter::RestoreHealth(int32 Amount)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetHealthAttribute(), Amount);
}

void AIBaseCharacter::RestoreHunger(int32 Amount)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetHungerAttribute(), Amount);
}

void AIBaseCharacter::RestoreThirst(int32 Amount)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetThirstAttribute(), Amount);
}

void AIBaseCharacter::RestoreStamina(int32 Amount)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetStaminaAttribute(), Amount);
}

void AIBaseCharacter::RestoreOxygen(int32 Amount)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetOxygenAttribute(), Amount);
}


void AIBaseCharacter::RestoreBodyFood(float Amount)
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetCurrentBodyFoodAmountAttribute(), Amount);
}

void AIBaseCharacter::ResetStats()
{
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetHealthAttribute(), GetMaxHealth());
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetThirstAttribute(), GetMaxThirst());
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetHungerAttribute(), GetMaxHunger());
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetStaminaAttribute(), GetMaxStamina());
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetOxygenAttribute(), GetMaxOxygen());
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetCurrentBodyFoodAmountAttribute(), GetMaxBodyFood());

	const UCoreAttributeSet* const AttributeSet = Cast<UCoreAttributeSet>(AbilitySystem->GetAttributeSet(UCoreAttributeSet::StaticClass()));
	for (const auto& StatusHandlingInformation : AttributeSet->CallForStatusHandling)
	{
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, StatusHandlingInformation.Key, 0, EGameplayModOp::Override);	
	}

	// Broken Bone / Leg damage is not yet incorporated fully in the Status refactor (#4113), so has to be called directly
	UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetLegDamageAttribute(), 0, EGameplayModOp::Override);
}

void AIBaseCharacter::OnRep_IsAIControlled()
{
	//if (bIsAIControlled && !bCombatLogAI)
	//{
	//	// disable all mesh collisions and mesh physics for ai
	//	//if (UMeshComponent* CharacterMesh = GetMesh())
	//	//{
	//	//	CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	//	//	CharacterMesh->SetTickGroup(ETickingGroup::TG_DuringPhysics);
	//	//}
	//
	//	//DestroyCosmeticComponents();
	//}
}

bool AIBaseCharacter::ShouldBlockOnPreload() const
{
	if (!GetWorld() || (!GetWorld()->WorldComposition && !GetWorld()->GetWorldPartition()))
	{
		return false;
	}

	if (AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(this))
	{
		if (!WS->bUseClientSideLevelStreamingVolumes)
		{
			return false;
		}
	}

	if (IsCombatLogAI() && !IsCombatLogRespawningUser())
	{
		return false;
	}

	return true;
}

void AIBaseCharacter::PotentiallySetPreloadingClientArea()
{
	return;
}

void AIBaseCharacter::OnRep_PreloadingClientArea()
{
	UE_LOG(TitansLog, Log, TEXT("AIBaseCharacter::SetPreloadingClientArea: %s"), (IsPreloadingClientArea() ? TEXT("true") : TEXT("false")));
	if (IsPreloadingClientArea())
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);
		TWeakObjectPtr<AIBaseCharacter> WeakThis = this;

		// Create a load screen task that expires when this character is destroyed or bPreloadingClientArea is set to false
		IGameInstance->QueueLoadScreenTask(
			{ FLoadScreenTask::CreateLambda([WeakThis]() 
				{
					if (!WeakThis.Get()) return true;
					return !WeakThis->IsPreloadingClientArea();
				}), 
				FText::FromString(TEXT("Preloading Client Area"))
			}, 60.0f);
	}
}

void AIBaseCharacter::SetPreloadingClientArea(const bool bPreloading)
{
	//Preloading client area basically just turns off visibility and gravity for the character.
	//This is set when the controller is replicated and is cleared when the controller finished initialization
	//(see AIPlayerController::OnRep_Pawn) and when there are no more levels to stream in.

	if (IsPreloadingClientArea() == bPreloading || !ShouldBlockOnPreload()) return;

	bPreloadingClientArea = bPreloading;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, bPreloadingClientArea, this);

	if (HasAuthority() && IsLocallyControlled())
	{
		OnRep_PreloadingClientArea();
	}

	if (!IsPreloadingClientArea())
	{
		SetPreloadPlayerLocks(false);
	}
	else
	{
		SetPreloadPlayerLocks(true);
	}

}

void AIBaseCharacter::SetPreloadPlayerLocks(const bool bLockPlayer)
{
	if(!ShouldBlockOnPreload()) return;
	check(HasAuthority() || bIsCharacterEditorPreviewCharacter);
	//This is where the actual locks are set and where the loading screen is shown and hidden (if locally controlled)
	//This should _only_ be called from SetPreloadingClientArea, no manual calls are needed.

	//Note that this also binds to OnLevelsLoaded which will initialize the unlocking.

	//a client should be hidden and disable collision until finished loading the area they will spawn in, unless they are attached to/by another player
	// or loading in from combat log ai
	if (HasAuthority() && !GetAttachTarget().IsValid() && AttachedCharacters.IsEmpty() && !IsCombatLogAI())
	{
		SetActorHiddenInGame(bLockPlayer);
		SetActorEnableCollision(!bLockPlayer);
	}
	
	if (IsLocallyControlled() || bIsCharacterEditorPreviewCharacter)
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);
		if (bLockPlayer)
		{
			IGameInstance->OnLevelsFinishedLoading.AddUniqueDynamic(this, &AIBaseCharacter::OnLevelsLoaded);
		}
	}
}

void AIBaseCharacter::TryFinishPreloadClientArea()
{
	if(GetController() == nullptr) return;
	if (!IsPreloadingClientArea()) return;
	//Checks to see if we can finish preloading (this is called from the client).
	//The client initializing isn't enough though as we have to check if there's any loading in progres too
	//It then passes on the OnLevelsLoaded call just like the delegate in AIBaseCharacter::SetPreloadPlayerLocks to keep it all going
	//through one code path.

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	if (!IGameInstance->CheckIfThereAreAnyLoadsInProgress())
	{
		OnLevelsLoaded();
	}
}

void AIBaseCharacter::LoadAndUpdateMapIcon_Implementation(bool bGroupLeader /*= false*/)
{
	if (!MapIconComponent) return;
	//if (!GetPlayerState()) return;
	if (!IsAlive()) return;

	RequestedLocalMapIcon = bGroupLeader ? LeaderPlayerMapIcon : PlayerMapIcon;

	// Widget already loaded, create and show it
	if (RequestedLocalMapIcon.Get() != nullptr)
	{
		return OnMapIconLoaded();
	}

	// Widget isn't loaded, async load and show it
	FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
	AssetLoader.RequestAsyncLoad
	(
		RequestedLocalMapIcon.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnMapIconLoaded),
		FStreamableManager::AsyncLoadHighPriority, false
	);
}

void AIBaseCharacter::SetMapIconActive(bool bActive)
{
	if (MapIconComponent)
	{
		MapIconComponent->SetIconVisible(bActive);
	}
	else
	{
		if (bActive && IsLocallyControlled())
		{
			// Sometimes the map icon doesn't appear 
			// so if it isn't spawned and it should be,
			// spawn it
			MapIconComponent = NewObject<UMapIconComponent>(this, TEXT("MapIconComp"));
			if (MapIconComponent)
			{
				MapIconComponent->RegisterComponent();
				MapIconComponent->SetIconRotates(true);

				MapIconComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);

				LoadAndUpdateMapIcon();
			}
		}
	}

	if (AIPlayerController* PlayerController = Cast<AIPlayerController>(GetController()))
	{
		if (PlayerController->MapRevealerComponent)
		{
			PlayerController->MapRevealerComponent->SetRevealMode(bActive ? EMapFogRevealMode::Permanent : EMapFogRevealMode::Off);
		}
	}
}

void AIBaseCharacter::RefreshMapIcon()
{
	if (!IsLocallyControlled()) 
	{
		SetMapIconActive(false);
		return;
	}

	if (GetCurrentInstance())
	{
		SetMapIconActive(false);
	}
	else
	{
		SetMapIconActive(true);
	}
}

void AIBaseCharacter::OnMapIconLoaded()
{
	// Ensure Name Tag is valid and hasn't been destroyed (character dying etc)
	if (!MapIconComponent) return;

	//if (!GetPlayerState()) return;

	if (!IsAlive())
	{
		if (MapIconComponent)
		{
			MapIconComponent->UnregisterComponent();
			MapIconComponent->DestroyComponent();
			MapIconComponent = nullptr;
		}
		return;
	}

	if (RequestedLocalMapIcon.Get())
	{
		MapIconComponent->SetIconTexture(RequestedLocalMapIcon.Get());
		MapIconComponent->SetIconVisible(true);
	}
	else
	{
		MapIconComponent->SetIconVisible(false);
	}

	RefreshMapIcon();
}

float AIBaseCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, class AController* EventInstigator, class AActor* DamageCauser)
{
	UE_LOG(LogIBaseCharacter, Error, TEXT("Non-GAS damage should no longer be dealt, please report this error."));

	FDebug::DumpStackTraceToLog(ELogVerbosity::Error);

	return 0.f;
}

bool AIBaseCharacter::CanDie(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser) const
{
	// Check if character is already dying, destroyed or if we have authority
	if (bIsDying || !IsValid(this) || !HasAuthority() || !GetWorld()->GetAuthGameMode()) return false;

	return true;
}

void AIBaseCharacter::FellOutOfWorld(const class UDamageType& DmgType)
{
	bDiedByKillZ = true;
	Die(GetHealth(), FDamageEvent(DmgType.GetClass()), nullptr, nullptr);
}

bool AIBaseCharacter::Die(float KillingDamage, FDamageEvent const& DamageEvent, AController* Killer, AActor* DamageCauser)
{
	ensureMsgf(AbilitySystem->InstaKillEffect, TEXT("Please set 'InstaKillEffect' in %s."), *GetDebugName(this));
	// Only authority or non-networked actors should be destroyed, otherwise misprediction can destroy something the server is intending to keep alive.
	if (AbilitySystem->InstaKillEffect->IsValidLowLevel() && (HasAuthority() || GetLocalRole() == ROLE_None))
	{
		FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
		FPOTGameplayEffectContext* POTEffectContext = StaticCast<FPOTGameplayEffectContext*>(ContextHandle.Get());
		POTEffectContext->SetDamageType(GetDamageTypeFromDamageEvent(DamageEvent));
		AbilitySystem->ApplyGameplayEffectToSelf(AbilitySystem->InstaKillEffect->GetDefaultObject<UGameplayEffect>(), 1, ContextHandle);

		return true;
	}

	return false;
}

void AIBaseCharacter::OnDeath(float KillingDamage, const FPOTDamageInfo& DamageInfo, APawn* PawnInstigator, AActor* DamageCauser, FVector HitLoc, FRotator HitRot)
{
	//UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::OnDeath()"));
	// Avoid Multiple Death Events Calling
	if (bIsDying) return;

	// Mutex Lock to avoid Multi-Death
	bIsDying = true;

	UnlatchAllDinosAndSelf();

	if (UICreatorModeObjectComponent* ICMOComp = Cast< UICreatorModeObjectComponent>(GetComponentByClass(UICreatorModeObjectComponent::StaticClass())))
	{
		ICMOComp->RemoveEditable(TEXT("Health"));
		ICMOComp->RemoveEditable(TEXT("Stamina"));
		ICMOComp->RemoveEditable(TEXT("Oxygen"));
		ICMOComp->RemoveEditable(TEXT("Thirst"));
		ICMOComp->RemoveEditable(TEXT("Hunger"));
		ICMOComp->RemoveEditable(TEXT("Godmode"));
		ICMOComp->Update();
	}

	AIPlayerController* DyingPlayerController = Cast<AIPlayerController>(GetController());
	if (DyingPlayerController)
	{
		DyingPlayerController->DisableInput(DyingPlayerController);
	}

	if (HasAuthority())
	{
		AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
		if (IGameMode)
		{
			IGameMode->OnCharacterDie(this, Cast<AIBaseCharacter>(DamageCauser), KillingDamage);
		}

		if (DyingPlayerController)
		{
			DyingPlayerController->DeathLocal(DyingPlayerController->GetReportInfo().LastDamageCauserID.IsValid());
			DyingPlayerController->DeathServer();

			if (PawnInstigator)
			{
				if (AIPlayerController* KillerPlayerController = PawnInstigator->GetController<AIPlayerController>())
				{
					if (KillerPlayerController != DyingPlayerController)
					{
						if (AIBaseCharacter* KillerCharacter = Cast<AIBaseCharacter>(PawnInstigator))
						{
							SetKillerCharacterId(KillerCharacter->GetCharacterID());
						}
						if (AIPlayerState* KillerPlayerState = Cast<AIPlayerState>(PawnInstigator->GetPlayerState()))
						{
							SetKillerAlderonId(KillerPlayerState->GetAlderonID());
						}
					}
				}
			}
		}

		ExitInstanceFromDeath(FTransform());
	}

	OnDeathCosmetic();

	// Add Dead Body Timer for Cleanup On Server
	if (HasAuthority())
	{

		// Change Dead Body Time if override on a server option
		UIGameInstance* IGameInstance = Cast<UIGameInstance>(GetGameInstance());
		check(IGameInstance);

		AIGameSession* Session = Cast<AIGameSession>(IGameInstance->GetGameSession());
		if (Session)
		{
			int32 ServerDeadBodyTime = 0;
			if (AIGameState* GameState = UIGameplayStatics::GetIGameState(this))
			{
				ServerDeadBodyTime = GameState->GetGameStateFlags().DeadBodyTime;
			}
			if (ServerDeadBodyTime > 0)
			{
				GetWorldTimerManager().SetTimer(TimerHandle_BodyDestroy, FTimerDelegate::CreateUObject(this, &AIBaseCharacter::DestroyBody, false), ServerDeadBodyTime, false);
			}
		}

		if (AbilitySystem != nullptr && !AbilitySystem->IsAlive())
		{
			//This activates Dead Body Food Decay
			if (AbilitySystem->DeadBodyFoodDecayEffect != nullptr)
			{
				UGameplayEffect* DeadBodyFoodDecayGameplayEffect = AbilitySystem->DeadBodyFoodDecayEffect->GetDefaultObject<UGameplayEffect>();
				AbilitySystem->ApplyGameplayEffectToSelf(DeadBodyFoodDecayGameplayEffect, GetGrowthLevel(), AbilitySystem->MakeEffectContext());
			}

			// Remove WellRestedBonus
			if (GetGE_WellRestedBonus().IsValid())
			{
				UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusStartedGrowthAttribute(), 0.0f, EGameplayModOp::Override);
				UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusEndGrowthAttribute(), 0.0f, EGameplayModOp::Override);
				AbilitySystem->RemoveActiveGameplayEffect(GetGE_WellRestedBonus(), -1);
				GetGE_WellRestedBonus_Mutable().Invalidate();
			}
		}

		// Flag Quest as Failed
		AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
		if (IWorldSettings)
		{
			if (AIQuestManager* QuestMgr = IWorldSettings->QuestManager)
			{
				TArray<UIQuest*> QuestsToCheck = GetActiveQuests();
				if (Session && Session->bLoseUnclaimedQuestsOnDeath)
				{
					QuestsToCheck.Append(GetUncollectedRewardQuests());
				}

				if (QuestsToCheck.Num() > 0)
				{
					for (UIQuest* Quest : QuestsToCheck)
					{
						if (Quest && Quest->QuestData && Quest->QuestData->QuestType != EQuestType::Tutorial && Quest->QuestData->QuestType != EQuestType::WorldTutorial)
						{
							QuestMgr->OnQuestFail(this, Quest, false, false, true);
						}
					}
				}
			}
		}

		// Set Temp Despawn
		SetTempBodyCleanup();
	}

	PlayHit(KillingDamage, DamageInfo, PawnInstigator, DamageCauser, true, HitLoc, HitRot, NAME_None);

	if (HasAuthority())
	{
		ToggleHomecaveCampingDebuff(false);
		//UE_LOG(LogTemp, Log, TEXT("Saving Character On Death"));
		AIGameMode* IGameMode = UIGameplayStatics::GetIGameMode(this);
		if (IGameMode)
		{
			IGameMode->SaveCharacter(this, ESavePriority::High);
		}
	}

	// Force Network Update
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	if (IGameInstance)
	{
		IGameInstance->AddReplayEvent(EReplayHighlight::Death);	
	}
}

void AIBaseCharacter::OnDeathCosmetic()
{
	//Mark character as dead
	if (AbilitySystem != nullptr)
	{
		AbilitySystem->PostDeath();

		for (const FPrimaryAssetId& Id : GetUnlockedAbilities())
		{
			UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());
			if (UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(Id))
			{
				if (LoadedAbility->bLoseOnDeath)
				{
					if (HasAuthority())
					{
						Server_LockAbility_Implementation(Id);
					}
					else
					{
						Server_LockAbility(Id);
					}
				}
			}
		}
	}

	// Drop Fish if dying
	if (IsCarryingObject())
	{
		OnInteractReady(true, GetCurrentlyCarriedObject().Object.Get());
	}

	// Clear focus
	if (HasUsableFocus())
	{
		SetFocusedObjectAndItem(nullptr, INDEX_NONE, true);
	}

	// Stop Current Anim Montages
	StopAllAnimMontages();

	StopBreathing();
	StopBlinking(true);

	// Stop Controller Movement / Actions
	AController* DyingController = GetController();
	if (IsValid(DyingController))
	{
		DyingController->StopMovement();
	}

	// Reset location display name on death
	AIPlayerController* DyingPlayerController = Cast<AIPlayerController>(GetController());
	if (DyingPlayerController)
	{
		DyingPlayerController->ResetLocationDisplayName();
	}

	// Reset Currently Used / Focused Object
	FUsableObjectReference& MutableCurrentlyUsedObject = GetCurrentlyUsedObject_Mutable();
	MutableCurrentlyUsedObject.Object = nullptr;
	MutableCurrentlyUsedObject.Item = INDEX_NONE;

	bLockDesiredFocalPoint = false;
	FocusedObject.Object = nullptr;
	FocusedObject.Item = INDEX_NONE;

	// Clear Unused Memory / Variables to Save Memory
	BleedingDecals.Empty();
	CurrentUseAnimMontage = nullptr;
	SetRemoteDesiredAimPitch(0);
	SetRemoteDesiredAimYaw(0);
	DrinkAnimMontage = nullptr;
	EatAnimMontage = nullptr;

	// Remove All Timers
	GetWorldTimerManager().ClearAllTimersForObject(this);

	if (HasAuthority())
	{
		if (IsCombatLogAI())
		{
			ExpiredCombatLogAI();
		}
	}

	if (IsLocallyControlled())
	{
		if (ThirdPersonCameraComponent)
		{
			ThirdPersonCameraComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}

	OnPostDeath.Broadcast();

	// Unposses and Remove From Controller
	//DetachFromControllerPendingDestroy();
	DestroyCosmeticComponents();

#if !UE_SERVER
	if (IsLocallyControlled() && GetLocalRole() == ROLE_AutonomousProxy)
	{
		SetRole(ROLE_SimulatedProxy); // Switch to simulated proxy as we no longer have control, this will allow physics replication on the ragdoll.
	}
#endif

	// Disable all collision on all capsules
	DisableCollisionCapsules(true);

	SetActorEnableCollision(true);

	UCharacterMovementComponent* CharacterComp = Cast<UCharacterMovementComponent>(GetMovementComponent());
	if (CharacterComp)
	{
		CharacterComp->StopMovementImmediately();
		CharacterComp->DisableMovement();
		CharacterComp->SetComponentTickEnabled(false);
	}

#if !UE_SERVER
	// Temp Workaround for stiff dead bodies
	if (!IsRunningDedicatedServer())
	{
		if (USkeletalMeshComponent* SkeletalMesh = GetMesh())
		{
			const int32 MaterialIndex = SkeletalMesh->GetMaterialIndex(NAME_Eyes);
			UMaterialInstanceDynamic* MID_EyeBlink = SkeletalMesh->CreateDynamicMaterialInstance(MaterialIndex, SkeletalMesh->GetMaterial(MaterialIndex), NAME_None);
			if (MID_EyeBlink)
			{
				MID_EyeBlink->SetScalarParameterValue(NAME_Blink, 0.0f);
			}
		}
	}
#endif

	// Wait a little then set Radgoll Physics, this is so that any ability related animation that were stopped
	// above have a tick to send NotifyEnd events (like for turning off particle emitters)
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &AIBaseCharacter::SetRagdollPhysics, 0.1f);

	// Death Sound Effects
	UpdateDeathEffects();
}

bool AIBaseCharacter::ContainsFoodTag(FName FoodTag)
{
	return FoodTags.Contains(FoodTag);
}

void AIBaseCharacter::SetBodyFoodPercentage(float NewBodyFoodPercentage)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, BodyFoodPercentage, NewBodyFoodPercentage, this);
}

FActiveGameplayEffectHandle& AIBaseCharacter::GetGE_WellRestedBonus_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, GE_WellRestedBonus, this);
	return GE_WellRestedBonus;
}

void AIBaseCharacter::SetGE_WellRestedBonus(FActiveGameplayEffectHandle NewGE_WellRestedBonus)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, GE_WellRestedBonus, NewGE_WellRestedBonus, this);
}

void AIBaseCharacter::ApplyWellRestedBonusMultiplier()
{
	if (GetWellRestedBonusStartedGrowth() < GetWellRestedBonusEndGrowth())
	{
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusMultiplierAttribute(), 2.0f, EGameplayModOp::Override);
	}
	else
	{
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusMultiplierAttribute(), 0.0f, EGameplayModOp::Override);
	}
	
	if (IsWellRested(true))
	{
		FGameplayEffectContextHandle ContextHandle = AbilitySystem->MakeEffectContext();
		SetGE_WellRestedBonus(AbilitySystem->ApplyGameplayEffectToSelf(AbilitySystem->WellRestedEffect->GetDefaultObject<UGameplayEffect>(), 1, ContextHandle));
	}
	else
	{
		GetGE_WellRestedBonus_Mutable().Invalidate();
	}
}

void AIBaseCharacter::OnRep_BodyFoodPercentage()
{
#if !UE_SERVER
	if (!IsAlive())
	{
		UpdateDeathEffects();
	}
#endif
}

void AIBaseCharacter::UpdateDeathEffects()
{
#if !UE_SERVER
	if (GetNetMode() != ENetMode::NM_DedicatedServer)
	{
		if (GetCurrentBodyFood() <= 0) return;
		if (HasDiedInWater())
		{
			if (AudioLoopDeathComp)
			{
				AudioLoopDeathComp->DestroyComponent();
			}

			if (DeathParticleComponent)
			{
				DeathParticleComponent->DestroyComponent();
			}

			return;
		}

		if (GetBodyFoodPercentage() <= FliesStartPercentage)
		{
			// Death Particle Effect Flies Sound
			if (AudioLoopDeathComp == nullptr)
			{
				UIGameplayStatics::LoadSoundAsync(SoundDeathLoop, FOnSoundLoaded::CreateUObject(this, &AIBaseCharacter::SoundLoaded_DeathParticle), this);
			}

			// Death Particle Effect Flies Particle
			if (DeathParticleComponent == nullptr)
			{
				UIGameplayStatics::LoadParticleAsync(ParticleDeath, FOnParticleLoaded::CreateUObject(this, &AIBaseCharacter::ParticleLoaded_DeathParticle), this);
			}

			UpdateDeathParams();
		}
	}
#endif
}

void AIBaseCharacter::SoundLoaded_DeathParticle(USoundCue* LoadedSound)
{
	if (LoadedSound && AudioLoopDeathComp == nullptr)
	{
		AudioLoopDeathComp = UGameplayStatics::SpawnSoundAttached(LoadedSound, GetMesh(), NAME_None, FVector(ForceInitToZero), EAttachLocation::KeepRelativeOffset, true);
		if (AudioLoopDeathComp)
		{
			AudioLoopDeathComp->bIsMusic = false;
			AudioLoopDeathComp->bIsUISound = false;
			if (!AudioLoopDeathComp->IsPlaying())
			{
				AudioLoopDeathComp->Play();
			}
			UpdateDeathParams();
		}
	}
}

void AIBaseCharacter::SoundLoaded_WindLoop(USoundCue* LoadedSound)
{
	if (LoadedSound && AudioLoopWindComp == nullptr)
	{
		AudioLoopWindComp = UGameplayStatics::SpawnSoundAttached(LoadedSound, GetMesh(), NAME_None, FVector(ForceInitToZero), EAttachLocation::KeepRelativeOffset, true);
		if (AudioLoopWindComp)
		{
			AudioLoopWindComp->bIsMusic = false;
			AudioLoopWindComp->bIsUISound = false;
			if (!AudioLoopWindComp->IsPlaying())
			{
				AudioLoopWindComp->Play();
			}
		}
	}
}

void AIBaseCharacter::ParticleLoaded_DeathParticle(UParticleSystem* LoadedParticle)
{
	if (!LoadedParticle) return;

	if (DeathParticleComponent == nullptr)
	{
		DeathParticleComponent = UGameplayStatics::SpawnEmitterAttached(LoadedParticle, GetMesh());
		UpdateDeathParams();
	}
}

float AIBaseCharacter::GetSplashEffectScale(float FallVelocity)
{
	const float MaximmSplashVelocity = -800.0f;
	const float MinScale = 1.0f;
	const float MaxScale = 4.0f;
	const float MinGrowthFactor = 0.5f;
	const float MaxGrowthFactor = 1.0f;

	// Calculate the scale of the splash based on the fall velocity
	FallVelocity = FMath::Clamp(FallVelocity, MaximmSplashVelocity, SplashThresholdVelocity);
	float InterpolationFactor = FMath::GetMappedRangeValueClamped(FVector2D(SplashThresholdVelocity, MaximmSplashVelocity), FVector2D(0.0f, 1.0f), FallVelocity);
	float ParticleScale = FMath::Lerp(MinScale, MaxScale, InterpolationFactor);

	// Factor in the growth of the character
	ParticleScale *= FMath::Clamp(GetGrowthLevel(), MinGrowthFactor, MaxGrowthFactor);

	return ParticleScale;
}

void AIBaseCharacter::PlaySplashEffectAtLocation()
{
	const bool bIsUsingParticleSplash = !ParticleSplash || (SplashParticleComponent && !SplashParticleComponent->HasCompleted());
	const bool bIsUsingSoundSplash = !SoundSplash || (AudioSplashComp && AudioSplashComp->IsPlaying());

	if (bIsUsingParticleSplash || bIsUsingSoundSplash)
	{
		return;
	}

	FVector SplashLocation = GetActorLocation() + FVector(0, 0, -GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	float SplashScale = GetSplashEffectScale(GetVelocity().Z);

	if (!SplashParticleComponent)
	{
		SplashParticleComponent = UGameplayStatics::SpawnEmitterAtLocation(this, ParticleSplash, SplashLocation, FRotator(ForceInit), false);
	}
	if (SplashParticleComponent)
	{
		SplashParticleComponent->SetRelativeScale3D(FVector::OneVector * SplashScale);
		SplashParticleComponent->SetWorldLocation(SplashLocation);
		SplashParticleComponent->Activate();
	}

	if (!AudioSplashComp)
	{
		AudioSplashComp = UGameplayStatics::SpawnSoundAtLocation(this, SoundSplash, SplashLocation);
		if (AudioSplashComp)
		{
			AudioSplashComp->bAutoDestroy = false;
		}
	}
	if (AudioSplashComp)
	{
		AudioSplashComp->SetVolumeMultiplier(SplashScale * SplashVolumeMultiplier);
		AudioSplashComp->SetPitchMultiplier(SplashPitchMultiplier);
		AudioSplashComp->SetWorldLocation(SplashLocation);
		AudioSplashComp->Activate();
	}
}

void AIBaseCharacter::UpdateDeathParams()
{
	const float PercentageOfFlies = UKismetMathLibrary::MapRangeClamped(GetBodyFoodPercentage(), 1.0f, 0.0f, 0.0f, 1.0f);

	if (AudioLoopDeathComp)
	{
		int SpawnAmount = 0; // small default
		if (PercentageOfFlies >= 0.7)
		{
			SpawnAmount = 2; // large
		}
		else if (PercentageOfFlies >= 0.4)
		{
			SpawnAmount = 1; // medium
		}

		AudioLoopDeathComp->SetFloatParameter(TEXT("Spawn"), SpawnAmount);
	}

	if (DeathParticleComponent)
	{
		DeathParticleComponent->SetFloatParameter(TEXT("Spawn"), PercentageOfFlies);
	}
}

void AIBaseCharacter::SetHasSpawnedQuestItem(bool bNewHasSpawnedQuestItem)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bHasSpawnedQuestItem, bNewHasSpawnedQuestItem, this);
}

void AIBaseCharacter::TeleportSucceeded(bool bIsATest)
{
	Super::TeleportSucceeded(bIsATest);

	if (!bIsATest)
	{
		if (HasAuthority() && GetNetMode() != ENetMode::NM_Standalone)
		{
			Client_TeleportSucceeded();
		}
		if (IsLocallyControlled())
		{
			if (AIPlayerController* IPlayerController = GetController<AIPlayerController>())
			{
				IPlayerController->bPendingInstanceTeleport = false;
			}
		}
	}
}

void AIBaseCharacter::Client_TeleportSucceeded_Implementation()
{

	if (IsLocallyControlled())
	{
		if (AIPlayerController* IPlayerController = GetController<AIPlayerController>())
		{
			IPlayerController->bPendingInstanceTeleport = false;
		}
	}
}

void AIBaseCharacter::OnLevelsLoaded()
{
	//Once everything is done for and we want to release the player, we delay it a bit to make sure that we're not in some weird
	//edge case transition (happens when you try to log in before the log-in area finished loading at times)
	if (!bIsCharacterEditorPreviewCharacter)
	{
		if (!GetWorldTimerManager().IsTimerActive(ClearPreloadAreaHandle))
		{
			GetWorldTimerManager().SetTimer(ClearPreloadAreaHandle, this, &AIBaseCharacter::DelayedClearPreloadArea, 0.5f, true);
		}
	}
	else
	{
		DelayedClearPreloadArea();
	}

	bDesiresPreciseMovement = false;
}


void AIBaseCharacter::DelayedClearPreloadArea()
{
	if (!IsPreloadingClientArea()) return;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	bool bShouldRelease = true;

	if (!GetCurrentInstance())
	{
		if (IGameInstance->CheckIfThereAreAnyLoadsInProgress())
		{
			bShouldRelease = false;
		}
		else if(ULevelStreaming * LevelStreaming = UIGameplayStatics::GetLevelForLocation(this, GetActorLocation()))
		{		
			if (!LevelStreaming->IsLevelVisible())
			{
				bShouldRelease = false;
			}			
		}
	}

	if (bShouldRelease)
	{
		if (bIsCharacterEditorPreviewCharacter)
		{
			SetPreloadingClientArea(false);
		}
		else
		{
			const TOptional<float> PotentiallySafeHeight = GetSafeHeightIfUnderTheWorld(true);
			if (PotentiallySafeHeight.IsSet() && (PotentiallySafeHeight.GetValue() > GetActorLocation().Z))
			{
				ServerFellThroughWorldAdjustment(PotentiallySafeHeight.GetValue());
				return;
			}

			ServerNotifyFinishedClientLoad();
			ClientLoadedAutoRecord();
		}
		GetWorldTimerManager().ClearTimer(ClearPreloadAreaHandle);
	}
	else
	{
		IGameInstance->RequestWaitForLevelLoading();
		IGameInstance->OnLevelsFinishedLoading.AddUniqueDynamic(this, &AIBaseCharacter::OnLevelsLoaded);
	}
}

void AIBaseCharacter::Server_SetAbilityInCategorySlot_Implementation(int32 InSlot, const FPrimaryAssetId& AbilityId, EAbilityCategory SlotCategory)
{
	if (AbilitySystem == nullptr)
	{
		return;
	}

	if (!AbilityId.IsValid())
	{
		// Clear the existing slot:

		FPrimaryAssetId ExistingIndexAbility;
		if (GetAbilityForSlot(FActionBarAbility(SlotCategory, InSlot), ExistingIndexAbility))
		{
			//If this ability was a passive effect, remove it.
			RemoveAbilityById(ExistingIndexAbility);
		}

		int32 SlottedCatIndex = INDEX_NONE;
		for (int32 i = 0; i < GetSlottedAbilityAssetsArray().Num(); i++)
		{
			if (GetSlottedAbilityAssetsArray()[i].Category == SlotCategory)
			{
				SlottedCatIndex = i;
				break;
			}
		}

		if (SlottedCatIndex != INDEX_NONE)
		{
			const FSlottedAbilities& SlottedCatAbilities = GetSlottedAbilityAssetsArray()[SlottedCatIndex];

			if (InSlot < SlottedCatAbilities.SlottedAbilities.Num())
			{
				GetSlottedAbilityAssetsArray_Mutable()[SlottedCatIndex].SlottedAbilities[InSlot] = FPrimaryAssetId();
			}
		}

		if (HasAuthority())
		{
			OnRep_SlottedAbilityCategories();
		}

		return;
	}

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(AbilityId);

	if (LoadedAbility == nullptr)
	{
		return;
	}

	ActionBarCustomizationVersion = static_cast<EActionBarCustomizationVersion>(static_cast<int32>(EActionBarCustomizationVersion::MAX) - 1);

	const EAbilityCategory AbilityCategory = LoadedAbility->AbilityCategory;
	int32 AbilityCategoryIndex = INDEX_NONE;

	FSlottedAbilities SlottedAbility;
	if (GetAbilityInCategoryArray(AbilityCategory, SlottedAbility))
	{
		AbilityCategoryIndex = SlottedAbility.SlottedAbilities.Find(AbilityId);
	}

	if (!IsValidSlot(AbilityCategory, InSlot))
	{
		return;
	}

	TArray<int32> ActionBarIndices;

	//Is there an ability in this category index?
	FPrimaryAssetId ExistingIndexAbility;
	if (GetAbilityForSlot(FActionBarAbility(AbilityCategory, InSlot), ExistingIndexAbility))
	{
		if (ExistingIndexAbility == AbilityId)
		{
			//Putting X into X, no changes needed.
			return;
		}

		//If this ability was a passive effect, remove it.
		RemoveAbilityById(ExistingIndexAbility);
	}

	//Was the ability we're trying to slot already in a different category index?
	int32 SlottedCatIndex = INDEX_NONE;
	for(int32 i = 0; i < GetSlottedAbilityAssetsArray().Num(); i++)
	{
		if(GetSlottedAbilityAssetsArray()[i].Category == AbilityCategory)
		{
			SlottedCatIndex = i;
			break;
		}
	}

	if(SlottedCatIndex == INDEX_NONE)
	{
		FSlottedAbilities NewAbilitiesCategory;
		NewAbilitiesCategory.Category = AbilityCategory;
		SlottedCatIndex = GetSlottedAbilityAssetsArray_Mutable().Add(NewAbilitiesCategory);
	}
	
	FSlottedAbilities& SlottedCatAbilities = GetSlottedAbilityAssetsArray_Mutable()[SlottedCatIndex];
	//FSlottedAbilities& SlottedCatAbilities = SlottedAbilityAssets.FindOrAdd(AbilityCategory);

	for (int32 i = 0; i < SlottedCatAbilities.SlottedAbilities.Num(); i++)
	{
		FPrimaryAssetId& CategoryAbility = SlottedCatAbilities.SlottedAbilities[i];
		if (CategoryAbility == AbilityId && i != InSlot)
		{
			//This ability already exists in some other slot, clear that slot.
			CategoryAbility = FPrimaryAssetId();
			break;
		}
	}

	//Finally place the new ability in its new index.

	if (InSlot >= SlottedCatAbilities.SlottedAbilities.Num())
	{
		const int32 Delta = InSlot - SlottedCatAbilities.SlottedAbilities.Num() + 1;
		for (int i = 0; i < Delta; i++)
		{
			SlottedCatAbilities.SlottedAbilities.Add(FPrimaryAssetId());
		}
	}
	
	SlottedCatAbilities.SlottedAbilities[InSlot] = AbilityId;

	if (LoadedAbility->GrantedAbility != nullptr &&
		GetAbilitySpecHandle(AbilityId) == nullptr)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(LoadedAbility->GrantedAbility, AbilitySystem->GetLevel(), INDEX_NONE, LoadedAbility);
		AddActiveAbility(AbilityId, AbilitySystem->GiveAbility(AbilitySpec));
	}
	

	if (LoadedAbility->GrantedPassiveEffects.Num() > 0 &&
		GetActiveGEHandles(AbilityId).IsEmpty())
	{
		for (TSubclassOf<UGameplayEffect> PassiveEffect : LoadedAbility->GrantedPassiveEffects)
		{
			if(!IsValid(PassiveEffect)) continue;
			const UGameplayEffect* Def = PassiveEffect->GetDefaultObject<UGameplayEffect>();
			FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();

			AddPassiveEffect(AbilityId, AbilitySystem->ApplyGameplayEffectToSelf(Def, GetGrowthLevel(), Context));
		}
	}

	//If this is a passive ability only, and there was an ability on the action bar referencing this slot, remove it.
	if (LoadedAbility->GrantedPassiveEffects.Num() > 0 && LoadedAbility->GrantedAbility == nullptr)
	{
		for (FActionBarAbility& ABarAbility : GetSlottedAbilityCategories_Mutable())
		{
			if (ABarAbility.Category == AbilityCategory && ABarAbility.Index == InSlot)
			{
				ABarAbility.Category = EAbilityCategory::MAX;
				ABarAbility.Index = INDEX_NONE;
			}
		}
	}

	if (HasAuthority())
	{
		OnRep_SlottedAbilityCategories();
	}
}

void AIBaseCharacter::Server_SetAbilityInActionBar_Implementation(int32 InSlot, const FPrimaryAssetId& AbilityId, const int32 OldIndex /*= INDEX_NONE*/)
{
	if (!AbilityId.IsValid())
	{
		//We just want to clear the current slot.
		if (!GetSlottedAbilityCategories().IsValidIndex(InSlot))
		{
			//This slot doesn't actually exist.
			return;
		}
		
		
		GetSlottedAbilityCategories_Mutable()[InSlot] = FActionBarAbility();
		
		if (HasAuthority())
		{
			OnRep_SlottedAbilityCategories();
		}

		return;
	}


	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(AbilityId);

	if (LoadedAbility == nullptr)
	{
		return;
	}

	if (!UIAbilitySlotsEditor::CheckAbilityCompatibilityWithCharacter(this, LoadedAbility))
	{
		return;
	}

	const EAbilityCategory AbilityCategory = LoadedAbility->AbilityCategory;
	int32 AbilityCategoryIndex = INDEX_NONE;

	FSlottedAbilities SlottedAbility;
	if (GetAbilityInCategoryArray(AbilityCategory, SlottedAbility))
	{
		AbilityCategoryIndex = SlottedAbility.SlottedAbilities.Find(AbilityId);
	}

	if (AbilityCategoryIndex == INDEX_NONE || !IsValidSlot(AbilityCategory, AbilityCategoryIndex))
	{
		return;
	}
	
	if (InSlot >= GetSlottedAbilityCategories().Num())
	{
		TArray<FActionBarAbility>& MutableSlottedAbilityCategories = GetSlottedAbilityCategories_Mutable();
		const int32 Delta = InSlot - MutableSlottedAbilityCategories.Num() + 1;
		for (int32 Index = 0; Index < Delta; Index++)
		{
			MutableSlottedAbilityCategories.Add(FActionBarAbility());
		}
	}

	if (GetSlottedAbilityCategories()[InSlot].Category == AbilityCategory && GetSlottedAbilityCategories()[InSlot].Index == AbilityCategoryIndex)
	{
		return;
	}


	GetSlottedAbilityCategories_Mutable()[InSlot] = FActionBarAbility(AbilityCategory, AbilityCategoryIndex);

	ActionBarCustomizationVersion = static_cast<EActionBarCustomizationVersion>(static_cast<int32>(EActionBarCustomizationVersion::MAX) - 1);

	if (OldIndex != INDEX_NONE)
	{
		GetSlottedAbilityCategories_Mutable()[OldIndex] = FActionBarAbility();
	}

	if (HasAuthority())
	{
		OnRep_SlottedAbilityCategories();
	}
}

TArray<FSlottedAbilityEffects>& AIBaseCharacter::GetSlottedActiveAndPassiveEffects_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, SlottedActiveAndPassiveEffects, this);
	return SlottedActiveAndPassiveEffects;
}

bool AIBaseCharacter::IsSlottedPassiveEffectActive(const FPrimaryAssetId AbilityId) const
{
	for (const FSlottedAbilityEffects& AbilityEffect : SlottedActiveAndPassiveEffects)
	{
		if (AbilityEffect.AbilityId == AbilityId && !AbilityEffect.PassiveEffects.IsEmpty())
		{
			return true;
		}
	}
	return false;
}

TArray<FPrimaryAssetId>& AIBaseCharacter::GetUnlockedAbilities_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, UnlockedAbilities, this);
	return UnlockedAbilities;
}

bool AIBaseCharacter::GetAbilityForSlot(const FActionBarAbility& ActionBarAbility, FPrimaryAssetId& OutId) const
{
	FSlottedAbilities Abilities;
	if (GetAbilityInCategoryArray(ActionBarAbility.Category, Abilities))
	{
		if (Abilities.SlottedAbilities.IsValidIndex(ActionBarAbility.Index) &&
			IsValidSlot(ActionBarAbility.Category, ActionBarAbility.Index))
		{
			OutId = Abilities.SlottedAbilities[ActionBarAbility.Index];
			return true;
		}
	}

	OutId = FPrimaryAssetId();
	return false;
}

bool AIBaseCharacter::GetAbilityInCategoryArray(EAbilityCategory Category, FSlottedAbilities& OutAbilities) const
{
	for (const FSlottedAbilities& SAbilities : GetSlottedAbilityAssetsArray())
	{
		if(SAbilities.Category == Category)
		{
			OutAbilities = SAbilities;
			return true;
		}
	}

	return false;
}

const FGameplayAbilitySpecHandle* AIBaseCharacter::GetAbilitySpecHandle(const FPrimaryAssetId& Id) const
{
	for (const FSlottedAbilityEffects& AEffect : GetSlottedActiveAndPassiveEffects())
	{
		if(AEffect.AbilityId == Id && Id.IsValid() && AEffect.ActiveAbility.IsValid())
		{
			return &AEffect.ActiveAbility;
		}
	}
	
	return nullptr;
}

TArray<FActiveGameplayEffectHandle> AIBaseCharacter::GetActiveGEHandles(const FPrimaryAssetId& Id) const
{
	TArray<FActiveGameplayEffectHandle> ValidEffects;
	for (const FSlottedAbilityEffects& AEffect : GetSlottedActiveAndPassiveEffects())
	{
		for (FActiveGameplayEffectHandle Handle : AEffect.PassiveEffects)
		{
			if (AEffect.AbilityId == Id && Id.IsValid() && Handle.IsValid())
			{
				ValidEffects.Add(Handle);
			}
		}
	}
	
	return ValidEffects;
}

void AIBaseCharacter::RemoveActiveAbility(const FPrimaryAssetId& Id)
{
	for (int32 Index = 0; Index < GetSlottedActiveAndPassiveEffects().Num(); ++Index)
	{
		if(Id.IsValid() && GetSlottedActiveAndPassiveEffects()[Index].AbilityId == Id)
		{
			GetSlottedActiveAndPassiveEffects_Mutable()[Index].ActiveAbility = FGameplayAbilitySpecHandle();
		}
	}
}

void AIBaseCharacter::HandlePassiveEffectsOnGrowthChanged()
{
	const TArray<FSlottedAbilities>& SlottedAbilitiesArray = GetSlottedAbilityAssetsArray();
	for (const FSlottedAbilities& SlottedAbilityAssetsItem : SlottedAbilitiesArray)
	{
		for (int32 i = 0; i < SlottedAbilityAssetsItem.SlottedAbilities.Num(); i++)
		{
			const FPrimaryAssetId& AbilityAssetId = SlottedAbilityAssetsItem.SlottedAbilities[i];
			if (!AbilityAssetId.IsValid())
			{
				continue;
			}

			// Remove passive effects belonging to abilities that do not meet growth requirements anymore.
			// We don't remove the effect for metabolism abilities since these effects let players eat, and lower growth dinosaurs should still be able to eat.
			if (SlottedAbilityAssetsItem.Category != EAbilityCategory::AC_METABOLISM && !IsGrowthRequirementMetForAbility(SlottedAbilityAssetsItem.Category, i))
			{
				RemovePassiveEffect(AbilityAssetId);
				continue;
			}

			// Activate passive effects of abilities that were slotted but didn't meet growth requirements up until now.
			if (IsSlottedPassiveEffectActive(AbilityAssetId))
			{
				continue;
			}

			UTitanAssetManager* const AssetManager = Cast<UTitanAssetManager>(UAssetManager::GetIfInitialized());
			if (!AssetManager)
			{
				return;
			}

			const UPOTAbilityAsset* const LoadedAbility = AssetManager->ForceLoadAbility(AbilityAssetId);
			if (!LoadedAbility)
			{
				continue;
			}

			if (LoadedAbility->GrantedPassiveEffects.IsEmpty() || !GetActiveGEHandles(AbilityAssetId).IsEmpty())
			{
				continue;
			}

			for (const TSubclassOf<UGameplayEffect>& PassiveEffect : LoadedAbility->GrantedPassiveEffects)
			{
				if (!IsValid(PassiveEffect))
				{
					continue;
				}
				const UGameplayEffect* const GameplayEffect = PassiveEffect->GetDefaultObject<UGameplayEffect>();
				const FGameplayEffectContextHandle Context = AbilitySystem->MakeEffectContext();
				AddPassiveEffect(AbilityAssetId, AbilitySystem->ApplyGameplayEffectToSelf(GameplayEffect, GetGrowthLevel(), Context));
			}
		}
	}
}

void AIBaseCharacter::NotifyAbilitiesNotMeetingGrowthRequirement()
{
	float Growth = GetGrowthPercent();
	const TArray<FSlottedAbilities>& SlottedAbilitiesArray = GetSlottedAbilityAssetsArray();
	for (const FSlottedAbilities& SlottedAbilityAssetsItem : SlottedAbilitiesArray)
	{
		for (int32 i = 0; i < SlottedAbilityAssetsItem.SlottedAbilities.Num(); i++)
		{
			if (Growth < GetGrowthRequirementForAbility(SlottedAbilityAssetsItem.Category, i))
			{
				OnAbilityNotMeetingGrowthRequirement(SlottedAbilityAssetsItem.Category);
				break;
			}
		}
	}
}

void AIBaseCharacter::RemovePassiveEffect(const FPrimaryAssetId& Id)
{
	bool bNeedsDirtying = false;
	
	for (FSlottedAbilityEffects& AEffect : SlottedActiveAndPassiveEffects)
    {
    	if(Id.IsValid() && AEffect.AbilityId == Id)
    	{
			for (int32 Index = AEffect.PassiveEffects.Num() - 1; Index >= 0; --Index)
			{
				const FActiveGameplayEffectHandle& Effect = AEffect.PassiveEffects[Index];
				if (Effect.IsValid())
				{
					AbilitySystem->RemoveActiveGameplayEffect(Effect);
					AEffect.PassiveEffects.RemoveAt(Index);
					bNeedsDirtying = true;
				}
			}
    	}
    }

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, SlottedActiveAndPassiveEffects, this);
	}
}

void AIBaseCharacter::AddActiveAbility(const FPrimaryAssetId& Id, const FGameplayAbilitySpecHandle& Handle)
{
	bool bFound = false;
	TArray<FSlottedAbilityEffects>& MutableSlottedActiveAndPassiveEffects = GetSlottedActiveAndPassiveEffects_Mutable();
	
	for (FSlottedAbilityEffects& AEffect : MutableSlottedActiveAndPassiveEffects)
	{
		if(AEffect.AbilityId == Id && Id.IsValid())
		{
			AEffect.ActiveAbility = Handle;
			bFound = true;
		}
	}

	if (!bFound)
	{
		MutableSlottedActiveAndPassiveEffects.Add(FSlottedAbilityEffects(Id, Handle));
	}
}

void AIBaseCharacter::AddPassiveEffect(const FPrimaryAssetId& Id, const FActiveGameplayEffectHandle& Handle)
{
	bool bFound = false;
	bool bNeedsDirtying = false;
	
	for (FSlottedAbilityEffects& AEffect : SlottedActiveAndPassiveEffects)
	{
		if(AEffect.AbilityId == Id && Id.IsValid())
		{
			if (!AEffect.PassiveEffects.Contains(Handle))
			{
				AEffect.PassiveEffects.Add(Handle);
				bNeedsDirtying = true;
			}
			bFound = true;
		}
	}

	if (!bFound)
	{
		SlottedActiveAndPassiveEffects.Add(FSlottedAbilityEffects(Id, Handle));
		bNeedsDirtying = true;
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, SlottedActiveAndPassiveEffects, this);
	}
}

int32 AIBaseCharacter::GetNumberOfSlotsForCategory(const EAbilityCategory Category) const
{
	return AbilitySlotConfiguration[static_cast<int32>(Category)].SlotsGrowthRequirement.Num();
}

float AIBaseCharacter::GetGrowthRequirementForSlot(const FActionBarAbility& ActionBarAbility) const
{
	if (!IsValidSlot(ActionBarAbility.Category, ActionBarAbility.Index))
	{
		return -1.f;
	}
	
	return AbilitySlotConfiguration[static_cast<int32>(ActionBarAbility.Category)].SlotsGrowthRequirement[ActionBarAbility.Index];
}

float AIBaseCharacter::GetGrowthRequirementForAbility(const EAbilityCategory AbilityCategory, const int32 SlotIndex) const
{
	if (!IsValidSlot(AbilityCategory, SlotIndex))
	{
		return -1.0f;
	}
	// We can blindly trust the array indices to be valid as both are checked inside of `IsValidSlot(Category, Index)`
	return AbilitySlotConfiguration[static_cast<uint8>(AbilityCategory)].SlotsGrowthRequirement[SlotIndex];
}

bool AIBaseCharacter::IsGrowthRequirementMet(const FActionBarAbility& ActionBarAbility) const
{
	if (!IsValidSlot(ActionBarAbility.Category, ActionBarAbility.Index))
	{
		return false;
	}
	
	const float RequiredGrowth = AbilitySlotConfiguration[static_cast<int32>(ActionBarAbility.Category)].SlotsGrowthRequirement[ActionBarAbility.Index];
	return GetGrowthPercent() >= RequiredGrowth;
}

bool AIBaseCharacter::IsGrowthRequirementMetForAbility(const UPOTAbilityAsset* AbilityAsset) const
{
	if (!AbilityAsset) return false;

	const FAbilitySlotData& AbilitySlotData = AbilitySlotConfiguration[(uint8)AbilityAsset->AbilityCategory];

	if (AbilitySlotData.SlotsGrowthRequirement.Num() == 0) return false;

	return (AbilitySlotData.SlotsGrowthRequirement[0] <= GetGrowthPercent());
}

bool AIBaseCharacter::IsGrowthRequirementMetForAbility(const EAbilityCategory AbilityCategory, const int32 SlotIndex) const
{
	return (GetGrowthRequirementForAbility(AbilityCategory, SlotIndex) <= GetGrowthPercent());
}

bool AIBaseCharacter::IsValidSlot(const EAbilityCategory Category, const int32 Slot) const
{
	return AbilitySlotConfiguration[static_cast<int32>(Category)].SlotsGrowthRequirement.IsValidIndex(Slot);
}

void AIBaseCharacter::OnRep_SlottedAbilityIds()
{
	OnAbilitySlotsChanged.Broadcast();
}

void AIBaseCharacter::OnRep_SlottedAbilityAssets()
{
	OnAbilitySlotsChanged.Broadcast();
}

void AIBaseCharacter::OnRep_SlottedAbilityCategories()
{
	OnAbilitySlotsChanged.Broadcast();
}

bool AIBaseCharacter::IsAbilityUnlocked(const FPrimaryAssetId& AbilityId) const
{
	if (!AbilityId.IsValid()) return false;

	if (GetUnlockedAbilities().Contains(AbilityId)) return true;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());
	UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(AbilityId);

	bool bUnlocked = LoadedAbility == nullptr ? false : LoadedAbility->bInitiallyUnlocked;
	if (!bUnlocked)
	{
		if (AIBaseCharacter* DefaultCharacter = GetClass()->GetDefaultObject<AIBaseCharacter>())
		{
			for (const FActionBarAbility& SlottedCatAbility : DefaultCharacter->GetSlottedAbilityCategories())
			{
				FPrimaryAssetId SlottedAbility;
				
				if (GetAbilityForSlot(SlottedCatAbility, SlottedAbility) && SlottedAbility == AbilityId)
				{
					bUnlocked = true;
					break;
				}
			}
		}
	}

	return bUnlocked;
}

void AIBaseCharacter::Server_UnlockAbility_Implementation(const FPrimaryAssetId& AbilityId, bool bSkipCostCheck /*= false*/)
{
	if (!AbilityId.IsValid() || GetUnlockedAbilities().Contains(AbilityId))
	{
		ServerUnlockAbility_ReplyFail(AbilityId, "DEBUG ERROR: Invalid ability or ability already unlocked.");
		return;
	}

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());
	UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(AbilityId);
	if (!LoadedAbility)
	{
		ServerUnlockAbility_ReplyFail(AbilityId, "Server failed to load Ability");
		return;
	}

	if (LoadedAbility->UnlockCost > GetMarks() && !bSkipCostCheck)
	{
		// UE-Log: Return Failure - Not enough marks
		ServerUnlockAbility_ReplyFail(AbilityId, "DEBUG ERROR: Not Enough Marks");
		return;
	}

	GetUnlockedAbilities_Mutable().Emplace(AbilityId);
	ServerUnlockAbility_ReplySuccess(AbilityId, GetMarks());

	if (LoadedAbility->UnlockCost > 0 && !bSkipCostCheck)
	{
		AddMarks(-LoadedAbility->UnlockCost);
	}

	AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
	if (IWorldSettings)
	{
		if (AIQuestManager* QuestMgr = IWorldSettings->QuestManager)
		{
			QuestMgr->OnPlayerSubmitGenericTask(this, TEXT("UnlockAbility"));
		}
	}
}

void AIBaseCharacter::SetUnlockedAbilities(const TArray<FPrimaryAssetId>& InAbilities)
{
	GetUnlockedAbilities_Mutable() = InAbilities;
}

void AIBaseCharacter::Server_LockAbility_Implementation(const FPrimaryAssetId& AbilityId)
{
	if (!AbilityId.IsValid() || !GetUnlockedAbilities().Contains(AbilityId))
	{
		ServerLockAbility_ReplyFail(AbilityId, "DEBUG ERROR: Invalid ability or ability already locked.");
		return;
	}

	GetUnlockedAbilities_Mutable().RemoveSingleSwap(AbilityId);
	ServerLockAbility_ReplySuccess(AbilityId, GetMarks());
}

void AIBaseCharacter::ServerUnlockAbility_ReplySuccess_Implementation(FPrimaryAssetId AbilityId, int32 NewMarks)
{
	GetUnlockedAbilities_Mutable().Emplace(AbilityId); //This will also get replicated
	OnAbilityUnlockStateChanged.Broadcast(AbilityId, true);
}

void AIBaseCharacter::ServerUnlockAbility_ReplyFail_Implementation(FPrimaryAssetId AbilityId, const FString& DebugText)
{
	UE_LOG(LogTemp, Log, TEXT("Unlock ability %s failed: %s"), *AbilityId.ToString(), *DebugText);
}

void AIBaseCharacter::ServerLockAbility_ReplySuccess_Implementation(FPrimaryAssetId AbilityId, int32 NewMarks)
{
	GetUnlockedAbilities_Mutable().RemoveSingleSwap(AbilityId);
	for (int32 i = 0; i < GetSlottedAbilityCategories().Num(); i++)
	{
		FPrimaryAssetId SlottedAbility;
		if (GetAbilityForSlot(GetSlottedAbilityCategories()[i], SlottedAbility) && SlottedAbility == AbilityId)
		{
			GetSlottedAbilityCategories_Mutable()[i] = FActionBarAbility();
			
			RemoveAbilityById(AbilityId);
		}
	}

	if (HasAuthority())
	{
		OnRep_SlottedAbilityIds();
	}

	OnAbilityUnlockStateChanged.Broadcast(AbilityId, false);
}

void AIBaseCharacter::RemoveAbilityById(const FPrimaryAssetId AbilityId)
{
	const TArray<FActiveGameplayEffectHandle> FoundHandles = GetActiveGEHandles(AbilityId);
	if (!FoundHandles.IsEmpty())
	{
		for (FActiveGameplayEffectHandle Handle : FoundHandles)
		{
			AbilitySystem->RemoveActiveGameplayEffect(Handle);
		}
		RemovePassiveEffect(AbilityId);
	}


	if (const FGameplayAbilitySpecHandle* FoundHandle = GetAbilitySpecHandle(AbilityId))
	{
		AbilitySystem->ClearAbility(*FoundHandle);
		RemoveActiveAbility(AbilityId);
	}
}

void AIBaseCharacter::ServerLockAbility_ReplyFail_Implementation(FPrimaryAssetId AbilityId, const FString& DebugText)
{
	UE_LOG(LogTemp, Log, TEXT("Lock ability %s failed: %s"), *AbilityId.ToString(), *DebugText);
}

void AIBaseCharacter::GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const
{
	if (AbilitySystem)
	{
		AbilitySystem->GetOwnedGameplayTags(TagContainer);
	}
	TagContainer.AppendTags(CharacterTags);
}

class UAbilitySystemComponent* AIBaseCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystem;
}

void AIBaseCharacter::DestroyBody(bool bFromCmd /*= false*/)
{
	if (ActivelyEatingBody.Num() > 0)
	{
		const TArray<AIBaseCharacter*> TempActivelyEatingBody = ActivelyEatingBody;
		for (AIBaseCharacter* IBC : TempActivelyEatingBody)
		{
			if (IsValid(IBC))
			{
				IBC->ForceStopEating();
			}
		}
	}

	if (!bFromCmd && CanTakeQuestItem(nullptr))
	{
		bDelayDestructionTillQuestItemSpawned = true;
		SpawnQuestItem(nullptr);
	}

	if (bDelayDestructionTillQuestItemSpawned) return;

	Destroy();
}

float AIBaseCharacter::GetFoodConsumePerTick() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetFoodConsumptionRateAttribute()) / FoodConsumedTickRate;
	}

	return 0.f;
}

float AIBaseCharacter::GetWaterConsumePerTick() const
{
	if (AbilitySystem != nullptr)
	{
		return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetWaterConsumptionRateAttribute()) / WaterConsumedTickRate;
	}

	return 0.f;
}

void AIBaseCharacter::StopPelvisUpdate()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_StopPelvisUpdate);

	// Disable any left over tick events and stop simulation
	StopRagdollPhysics();

	if (HasAuthority())
	{
		SetHasDiedInWater(IsInWater(50.0f, GetPelvisLocationRaw()));

		// Lower Network Update Frequency
		NetPriority = 1;
		NetUpdateFrequency = 0.25;
	}
}

void AIBaseCharacter::SetPelvisLocation(const FVector& NewPelvisLocation)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, PelvisLocation, NewPelvisLocation, this);
}

void AIBaseCharacter::SetIsLocalAreaLoading(bool bNewIsLocalAreaLoading)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bLocalAreaLoading, bNewIsLocalAreaLoading, this);
}

void AIBaseCharacter::Server_SetLocalAreaLoading_Implementation(const bool bAreaLoading)
{
	SetIsLocalAreaLoading(bAreaLoading);
}

void AIBaseCharacter::UpdateLocalAreaLoading()
{
	if (const UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this))
	{
		if (const UAlderonGameInstanceSubsystem* UAInstanceSubsystem = IGameInstance->GetSubsystem<UAlderonGameInstanceSubsystem>())
		{
			const bool bClientAreaLoading = UAInstanceSubsystem->CheckForCurrentLevelLoading();
			if (IsLocalAreaLoading() != bClientAreaLoading)
			{
				Server_SetLocalAreaLoading(bClientAreaLoading);
			}
		}
	}
}

void AIBaseCharacter::Server_UpdatePelvisPosition_Implementation()
{
	const FVector& LatestServerPelvisLocation = GetMesh()->GetSocketLocation(PelvisBoneName);

	// UpdatePelvisLocation if it is greater than 1
	if ((GetPelvisLocationRaw() - LatestServerPelvisLocation).Size() > 1)
	{
		SetPelvisLocation(LatestServerPelvisLocation);
	}
}

void AIBaseCharacter::Suicide()
{
	KilledBy(this);
}

void AIBaseCharacter::KilledBy(class APawn* EventInstigator)
{
	if (GetLocalRole() == ROLE_Authority && !bIsDying)
	{
		AController* Killer = nullptr;
		if (EventInstigator)
		{
			Killer = EventInstigator->Controller;
			LastHitBy = nullptr;
		}

		Die(GetHealth(), FDamageEvent(UDamageType::StaticClass()), Killer, nullptr);
	}
}

void AIBaseCharacter::SetRagdollPhysics()
{
	bool bRagdoll = false;
	USkeletalMeshComponent* Mesh3P = GetMesh();

	if (!Mesh3P || !Mesh3P->GetPhysicsAsset())
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::SetRagdollPhysics() - Mesh or Physics Asset not valid!"));
	}
	else
	{
		bRagdoll = true;

		Mesh3P->bPauseAnims = true;
		Mesh3P->bNoSkeletonUpdate = false;
		Mesh3P->bEnableUpdateRateOptimizations = false;
		Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh3P->bEnablePhysicsOnDedicatedServer = true;
		Mesh3P->SetComponentTickEnabled(true);
#if UE_SERVER
		Mesh3P->SetComponentTickInterval(0.0333f);
#endif

		bShouldAllowTailPhysics = false;
		Mesh3P->SetCollisionProfileName(TEXT("Ragdoll"));
		Mesh3P->SetAllBodiesSimulatePhysics(true);
		Mesh3P->SetSimulatePhysics(true);

		Mesh3P->SetCollisionObjectType(ECollisionChannel::ECC_Pawn); // Needed for physics volume to update
		Mesh3P->SetShouldUpdatePhysicsVolume(true); // Needed to check if in a water physics volume

		if (GetCharacterMovement())
		{
			Mesh3P->SetAllPhysicsLinearVelocity(GetCharacterMovement()->GetLastUpdateVelocity());	
		}

		if (IsRunningDedicatedServer())
		{
			GetWorldTimerManager().SetTimer(TimerHandle_StopPelvisUpdate, this, &AIBaseCharacter::StopPelvisUpdate, 30.0f, false);
		}
	}

	// Delete Characters Who Don't Have Ragdoll
	if (!bRagdoll)
	{
		Destroy();
	}

}

void AIBaseCharacter::StopRagdollPhysicsDelayed(float Delay /*= 10.0f*/)
{
	if (!GetWorldTimerManager().IsTimerActive(TimerHandle_StopRagdollDelay))
	{
		GetWorldTimerManager().SetTimer(TimerHandle_StopRagdollDelay, this, &AIBaseCharacter::StopRagdollPhysics, Delay, false);
	}
}

void AIBaseCharacter::StopRagdollPhysics()
{
	GEngine->AddOnScreenDebugMessage(-1, 15.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::StopRagdollPhysics()")));
	//UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::StopRagdollPhysics()"));
	
	if (IsRunningDedicatedServer())
	{
		if (USkeletalMeshComponent* Mesh3P = GetMesh())
		{
			Mesh3P->SetSimulatePhysics(false);
			Mesh3P->SetComponentTickEnabled(false);
			Mesh3P->bNoSkeletonUpdate = true;
			Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
		}

		SetActorTickEnabled(false);
	}
}

void AIBaseCharacter::BlendTailPhysics(bool bSwimming, float DeltaSeconds)
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (TailRoot == NAME_None) return;

		if (TailPhysicsBlendRatio == 1.0f)
		{
			TailPhysicsBlendRatio = 0.0f;
		}

		TailPhysicsBlendRatio = FMath::Clamp(TailPhysicsBlendRatio += DeltaSeconds, 0.0f, 1.0f);

		FPhysicalAnimationData TargetAnimationData = bShouldAllowTailPhysics ? (bSwimming ? PhysicalAnimationSwimmingData : PhysicalAnimationLandData) : FPhysicalAnimationData();

		PhysicalAnimationCurrentData.bIsLocalSimulation = TargetAnimationData.bIsLocalSimulation;
		PhysicalAnimationCurrentData.MaxAngularForce = FMath::Lerp(PhysicalAnimationCurrentData.MaxAngularForce, TargetAnimationData.MaxAngularForce, TailPhysicsBlendRatio);
		PhysicalAnimationCurrentData.MaxLinearForce = FMath::Lerp(PhysicalAnimationCurrentData.MaxLinearForce, TargetAnimationData.MaxLinearForce, TailPhysicsBlendRatio);
		PhysicalAnimationCurrentData.OrientationStrength = FMath::Lerp(PhysicalAnimationCurrentData.OrientationStrength, TargetAnimationData.OrientationStrength, TailPhysicsBlendRatio);
		PhysicalAnimationCurrentData.PositionStrength = FMath::Lerp(PhysicalAnimationCurrentData.PositionStrength, TargetAnimationData.PositionStrength, TailPhysicsBlendRatio);
		PhysicalAnimationCurrentData.VelocityStrength = FMath::Lerp(PhysicalAnimationCurrentData.VelocityStrength, TargetAnimationData.VelocityStrength, TailPhysicsBlendRatio);
		PhysicalAnimationCurrentData.AngularVelocityStrength = FMath::Lerp(PhysicalAnimationCurrentData.AngularVelocityStrength, TargetAnimationData.AngularVelocityStrength, TailPhysicsBlendRatio);

		if (USkeletalMeshComponent* Mesh3P = GetMesh())
		{
			if (PhysicalAnimation)
			{
				PhysicalAnimation->SetSkeletalMeshComponent(Mesh3P);
				PhysicalAnimation->ApplyPhysicalAnimationSettingsBelow(TailRoot, PhysicalAnimationCurrentData, bIncludeRootInTailPhysics);
				Mesh3P->SetAllBodiesBelowSimulatePhysics(TailRoot, true, bIncludeRootInTailPhysics);
				Mesh3P->SetAllBodiesBelowPhysicsBlendWeight(TailRoot, TailPhysicsBlendWeight);
				LastTailPhysicsBlendWeight = TailPhysicsBlendWeight;
			}
		}
	}
#endif
}

bool AIBaseCharacter::ShouldUpdateTailPhysics()
{
	if (!IsAlive()) return false;
	
	if (TailRoot != NAME_None)
	{
		if (TailPhysicsBlendWeight != LastTailPhysicsBlendWeight)
		{
			return true;
		}

		if (IsSwimming())
		{
			return PhysicalAnimationCurrentData.OrientationStrength != PhysicalAnimationSwimmingData.OrientationStrength;
		}
		else
		{
			return PhysicalAnimationCurrentData.OrientationStrength != PhysicalAnimationLandData.OrientationStrength;
		}
	}

	return false;
}

void AIBaseCharacter::PlayHit(float DamageTaken, const FPOTDamageInfo& DamageInfo, APawn* PawnInstigator, AActor* DamageCauser, bool bKilled, FVector HitLoc, FRotator HitRot, FName BoneName)
{
	if (GetLocalRole() == ROLE_Authority)
	{
		ReplicateHit(DamageTaken, DamageInfo, PawnInstigator, DamageCauser, bKilled, HitLoc, HitRot, BoneName);
	}

#if !UE_SERVER
	if (GetNetMode() != NM_DedicatedServer)
	{
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GameStateBase = World->GetGameState())
			{
				if (GameStateBase->GetServerWorldTimeSeconds() <= GetLastTakeHitInfo().Timestamp)
				{
					if (bKilled)
					{
						FSpawnAttachedSoundParams Params{};
						Params.SoundCue = SoundDeath;
						
						SpawnSoundAttachedAsync(Params);
					}
					else if (DamageInfo.DamageType == EDamageType::DT_BLEED)
					{
					}
					else
					{
						FSpawnAttachedSoundParams Params{};
						Params.SoundCue = SoundTakeHit;
							
						LoadHurtAnim();
						SpawnSoundAttachedAsync(Params);
					}

					FVector RelativeLocation;
					if (BoneName == NAME_None)
					{
						RelativeLocation = HitLoc - GetActorLocation();
					}
					else
					{
						USkeletalMeshComponent* SkelMesh = GetMesh();
						check(SkelMesh);
						if (!SkelMesh)
						{
							RelativeLocation = HitLoc - GetActorLocation();
						}
						else
						{
							RelativeLocation = HitLoc - SkelMesh->GetBoneLocation(BoneName);
						}
					}
			
					if (DamageInfo.bBoneBreak)
					{
						FSpawnAttachedSoundParams Params{};
						Params.SoundCue = SoundBreakLegs;
							
						SpawnSoundAttachedAsync(Params);

						if (PawnInstigator)
						{
							LoadDamageParticle(FDamageParticleInfo(EDamageEffectType::BROKENBONE, RelativeLocation, HitRot, BoneName), false);
						}
					} 
					else if (DamageInfo.DamageType == EDamageType::DT_BREAKLEGS)
					{
						if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
						{
							float DamageRatio = DamageTaken / GetMaxHealth();

							if (DamageRatio > 0.75f)
							{
								FSpawnAttachedSoundParams Params{};
								Params.SoundCue = SoundBreakLegs;
								
								SpawnSoundAttachedAsync(Params);

								if (PawnInstigator)
								{
									LoadDamageParticle(FDamageParticleInfo(EDamageEffectType::BROKENBONE, RelativeLocation, HitRot, BoneName), false);
								}
							}
						}
					}

					if (PawnInstigator)
					{
						LoadDamageParticle(FDamageParticleInfo(EDamageEffectType::DAMAGED, RelativeLocation, HitRot, BoneName), false);
					}

					// Shake the camera when damaged by something
					if (IsLocallyControlled())
					{
						if (AIPlayerController* IPC = UIGameplayStatics::GetIPlayerController(this))
						{
							UIGameplayStatics::GetIPlayerController(this)->ClientStartCameraShake(CS_Damaged, 1.0f);
						}
					}
				}
			}
		}
	}
#endif
}

void AIBaseCharacter::SpawnSoundAttachedAsync(FSpawnAttachedSoundParams& Params)
{
	// Don't spawn sounds on dedicated server
	if (IsRunningDedicatedServer()) return;

	checkf(IsInGameThread(), TEXT("Cannot call SpawnSoundAttachedAsync() outside the game thread."));
	
	if (Params.SoundCue.IsNull())
	{
		UE_LOG(TitansCharacter, Error, TEXT("UIGameplayStatics:SpawnSoundAttachedAsync: Sound %s not found when attempting to async load."), *Params.SoundCue.ToSoftObjectPath().ToString());
		return;
	};

	Params.SetRequestId(NextAsyncLoadingRequestId++);
	
	FStreamableManager& StreamableManager = UIGameplayStatics::GetStreamableManager(this);
	const FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::SpawnSoundAttachedCallback, Params);
	
	if (const TSharedPtr<FStreamableHandle> SoundHandle = StreamableManager.RequestAsyncLoad(Params.SoundCue.ToSoftObjectPath(), Delegate, FStreamableManager::AsyncLoadHighPriority))
	{
		AsyncLoadingHandles.Add(Params.GetRequestId(), SoundHandle);
	}
	else
	{
		UE_LOG(TitansCharacter, Error, TEXT("UIGameplayStatics:SpawnSoundAttachedAsync: Failed to start an async load request for asset '%s'."), *Params.SoundCue.ToSoftObjectPath().ToString());
	}
}

void AIBaseCharacter::SpawnSoundAttachedCallback(const FSpawnAttachedSoundParams Params)
{
	// Callback complete, get the async loading handle that matches this callback in order to retrieve the loaded asset.
	// Once this function ends, this handle will no longer have any references, allowing the GC to *safely* collect the sound.
	
	TSharedPtr<FStreamableHandle> Handle;
	if (!AsyncLoadingHandles.RemoveAndCopyValue(Params.GetRequestId(), Handle))
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnSoundAttachedCallback called with no async loading handle for request %llu."), Params.GetRequestId());
		return;
	}

	USoundBase* const LoadedSound = Params.SoundCue.Get();
	if (!IsValid(LoadedSound))
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnSoundAttachedCallback failed due to soft asset '%s' failing to load."), *Params.SoundCue.ToSoftObjectPath().ToString());
		return;
	}
	
	if (Params.bShouldFollow)
	{
		UAudioComponent* const AudioComponent = UGameplayStatics::SpawnSoundAttached(LoadedSound, GetMesh(), Params.AttachName, FVector::ZeroVector,
			EAttachLocation::SnapToTarget, true, Params.VolumeMultiplier, Params.PitchMultiplier);

		if (!AudioComponent)
		{
			//UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnSoundAttachedCallback failed to spawn an audio component for sound '%s'."), *Params.SoundCue.ToSoftObjectPath().ToString());
			return;
		}
		
		if (Params.bIsAnimNotify && Params.GrowthParameterName != NAME_None)
		{
			AudioComponent->SetFloatParameter(Params.GrowthParameterName, GetGrowthPercent());
			AnimNotifySounds.Add(AudioComponent);
		}
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), LoadedSound, GetMesh()->GetComponentLocation(), Params.VolumeMultiplier, Params.PitchMultiplier);
	}
}

void AIBaseCharacter::SpawnSoundFootstepAsync(FSpawnFootstepSoundParams& Params)
{
	checkf(IsInGameThread(), TEXT("Cannot call SpawnSoundFootstepAsync() outside the game thread."));
	
	if (Params.SoundCue.IsNull())
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter:SpawnSoundFootstepAsync: Sound '%s' not found when attempting to async load."), *Params.SoundCue.ToSoftObjectPath().ToString());
		return;
	};

	Params.SetRequestId(NextAsyncLoadingRequestId++);
	
	FStreamableManager& StreamableManager = UIGameplayStatics::GetStreamableManager(this);
	const FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::SpawnSoundFootstepCallback, Params);

	if (const TSharedPtr<FStreamableHandle> SoundHandle = StreamableManager.RequestAsyncLoad(Params.SoundCue.ToSoftObjectPath(), Delegate, FStreamableManager::AsyncLoadHighPriority))
	{
		AsyncLoadingHandles.Add(Params.GetRequestId(), SoundHandle);
	}
	else
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter:SpawnSoundFootstepAsync: Failed to start an async load request for asset '%s'."), *Params.SoundCue.ToSoftObjectPath().ToString());
	}
}

void AIBaseCharacter::SpawnSoundFootstepCallback(const FSpawnFootstepSoundParams Params)
{
#if !UE_SERVER
	TSharedPtr<FStreamableHandle> Handle;
	if (!AsyncLoadingHandles.RemoveAndCopyValue(Params.GetRequestId(), Handle))
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnSoundFootstepCallback called with no async loading handle for request %llu."), Params.GetRequestId());
		return;
	}

	USoundBase* const LoadedSound = Params.SoundCue.Get();
	if (!IsValid(LoadedSound))
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnSoundFootstepCallback failed due to soft asset '%s' failing to load."), *Params.SoundCue.ToSoftObjectPath().ToString());
		return;
	}

	UAudioComponent* SpawnedComponent = nullptr;
	
	if (Params.bShouldFollow)
	{
		SpawnedComponent = UGameplayStatics::SpawnSoundAttached(LoadedSound, GetMesh(), Params.AttachName, FVector::ZeroVector, EAttachLocation::SnapToTarget,
			true, Params.VolumeMultiplier, Params.PitchMultiplier);
	}
	else 
	{
		SpawnedComponent = UGameplayStatics::SpawnSoundAtLocation(GetMesh(), LoadedSound, Params.FootstepLocation, FRotator::ZeroRotator,
			Params.VolumeMultiplier, Params.PitchMultiplier);
	}
	
	if (SpawnedComponent)
	{
		if (Params.GrowthParameterName != NAME_None)
		{
			SpawnedComponent->SetFloatParameter(Params.GrowthParameterName, GetGrowthPercent());
		}

		if (Params.FootstepTypeParameterName != NAME_None)
		{
			SpawnedComponent->SetIntParameter(Params.FootstepTypeParameterName, static_cast<int32>(Params.FootstepType));
		}

		if (!Params.bShouldFollow)
		{
			SpawnedComponent->Play();
		}
	}
	else if (IsRunningDedicatedServer())
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnSoundFootstepCallback failed to spawn an audio component to play sound '%s'"), *Params.SoundCue.ToSoftObjectPath().ToString());
	}
#endif
}

void AIBaseCharacter::SpawnFootstepParticleAsync(FSpawnFootstepParticleParams& Params)
{
	checkf(IsInGameThread(), TEXT("Cannot call SpawnFootstepParticleAsync() outside the game thread."));

	if (Params.ParticleSystem.IsNull())
	{
		return;
	};
	
	Params.SetRequestId(NextAsyncLoadingRequestId++);
	
	FStreamableManager& StreamableManager = UIGameplayStatics::GetStreamableManager(this);
	const FStreamableDelegate Delegate = FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::SpawnFootstepParticleCallback, Params);

	if (const TSharedPtr<FStreamableHandle> SoundHandle = StreamableManager.RequestAsyncLoad(Params.ParticleSystem.ToSoftObjectPath(), Delegate, FStreamableManager::AsyncLoadHighPriority))
	{
		AsyncLoadingHandles.Add(Params.GetRequestId(), SoundHandle);
	}
	else
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter:SpawnFootstepParticleAsync: Failed to start an async load request for asset '%s'."), *Params.ParticleSystem.ToSoftObjectPath().ToString());
	}
}

void AIBaseCharacter::SpawnFootstepParticleCallback(const FSpawnFootstepParticleParams Params)
{
#if !UE_SERVER
	TSharedPtr<FStreamableHandle> Handle;
	if (!AsyncLoadingHandles.RemoveAndCopyValue(Params.GetRequestId(), Handle))
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnFootstepParticleCallback called with no async loading handle for request %llu."), Params.GetRequestId());
		return;
	}

	UParticleSystem* const LoadedParticle = Params.ParticleSystem.Get();
	if (!IsValid(LoadedParticle))
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnFootstepParticleCallback failed due to soft asset '%s' failing to load."), *Params.ParticleSystem.ToSoftObjectPath().ToString());
		return;
	}
	
	UParticleSystemComponent* const PSC = UGameplayStatics::SpawnEmitterAtLocation(GetMesh(), LoadedParticle, Params.Location, Params.Rotation, true);
	
	if (PSC)
	{
		// Nick 3.12.21: I couldn't find any particles using the float size parameter, it may be unused
		// Scaling the particles based on size only with growth just in case this gets called with non-1 values sometimes
		if (UIGameplayStatics::IsGrowthEnabled(this))
		{
			PSC->SetRelativeScale3D(FVector(Params.Size));
		}
			
		PSC->SetFloatParameter(TEXT("Size"), Params.Size);
	}
	else if (IsRunningDedicatedServer())
	{
		UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::SpawnFootstepParticleCallback failed to spawn particle system component for asset '%s'."), *Params.ParticleSystem.ToSoftObjectPath().ToString());
	}
#endif
}

void AIBaseCharacter::ReplicateHit(float DamageTaken, const FPOTDamageInfo& DamageInfo, APawn* PawnInstigator, AActor* DamageCauser, bool bKilled, FVector HitLoc, FRotator HitRot, FName BoneName)
{
	UWorld* World = GetWorld();
	if (!World) return;

	AGameStateBase* GameStateBase = World->GetGameState();
	if (!GameStateBase) return;

	const float ServerWorldTime = GameStateBase->GetServerWorldTimeSeconds();
	const float TimeoutTime = ServerWorldTime + 1.f;

	if (ServerWorldTime <= GetLastTakeHitInfo().Timestamp && PawnInstigator == GetLastTakeHitInfo().PawnInstigator.Get() && DamageInfo.DamageType == GetLastTakeHitInfo().DamageInfo.DamageType)
	{
		// Same frame damage / Redundant death take hit, ignore it
		if (bKilled && GetLastTakeHitInfo().bKilled) return;

		DamageTaken += GetLastTakeHitInfo().ActualDamage;
	}

	FTakeHitInfo& MutableLastTakeHitInfo = GetLastTakeHitInfo_Mutable();
	MutableLastTakeHitInfo.ActualDamage = DamageTaken;
	MutableLastTakeHitInfo.PawnInstigator = Cast<AIBaseCharacter>(PawnInstigator);
	MutableLastTakeHitInfo.DamageCauser = DamageCauser;
	//LastTakeHitInfo.DamageTypeFlags = 0;
	//LastTakeHitInfo.SetDamageEvent(DamageEvent);
	MutableLastTakeHitInfo.DamageInfo = DamageInfo;
	MutableLastTakeHitInfo.bKilled = bKilled;
	MutableLastTakeHitInfo.HitLocation = HitLoc;
	MutableLastTakeHitInfo.HitRotation = HitRot;
	MutableLastTakeHitInfo.Timestamp = TimeoutTime;
	MutableLastTakeHitInfo.BoneName = BoneName;
}

void AIBaseCharacter::SetHasDiedInWater(bool bNewHasDiedInWater)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bDiedInWater, bNewHasDiedInWater, this);
}

FTakeHitInfo& AIBaseCharacter::GetLastTakeHitInfo_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, LastTakeHitInfo, this);
	return LastTakeHitInfo;
}

void AIBaseCharacter::SetLastTakeHitInfo(const FTakeHitInfo& NewLastTakeHitInfo)
{
	LastTakeHitInfo = NewLastTakeHitInfo;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, LastTakeHitInfo, this);
}

void AIBaseCharacter::OnRep_LastTakeHitInfo()
{
	/*if (LastTakeHitInfo.DamageTypeClass == nullptr)
	{
		if (LastTakeHitInfo.bKilled)
		{
			OnDeath(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.DamageTypeFlags, LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get(), LastTakeHitInfo.HitLocation, LastTakeHitInfo.HitRotation);
			return;
		}

		PlayHit(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.DamageTypeFlags, LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get(), LastTakeHitInfo.bKilled, LastTakeHitInfo.HitLocation, LastTakeHitInfo.HitRotation);
	}
	else
	{
		if (LastTakeHitInfo.bKilled)
		{
			OnDeath(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get(), LastTakeHitInfo.HitLocation, LastTakeHitInfo.HitRotation);
			return;
		}

		PlayHit(LastTakeHitInfo.ActualDamage, LastTakeHitInfo.GetDamageEvent(), LastTakeHitInfo.PawnInstigator.Get(), LastTakeHitInfo.DamageCauser.Get(), LastTakeHitInfo.bKilled, LastTakeHitInfo.HitLocation, LastTakeHitInfo.HitRotation);
	}*/

	if (GetLastTakeHitInfo().bKilled)
	{
		OnDeath(GetLastTakeHitInfo().ActualDamage, GetLastTakeHitInfo().DamageInfo, GetLastTakeHitInfo().PawnInstigator.Get(), GetLastTakeHitInfo().DamageCauser.Get(), GetLastTakeHitInfo().HitLocation, GetLastTakeHitInfo().HitRotation);
		return;
	}

	PlayHit(GetLastTakeHitInfo().ActualDamage, GetLastTakeHitInfo().DamageInfo, GetLastTakeHitInfo().PawnInstigator.Get(), GetLastTakeHitInfo().DamageCauser.Get(), GetLastTakeHitInfo().bKilled, GetLastTakeHitInfo().HitLocation, GetLastTakeHitInfo().HitRotation, GetLastTakeHitInfo().BoneName);
}

void AIBaseCharacter::ReplicateParticle(FDamageParticleInfo DamageInfo)
{
	// Calculate the maximum number of particles based on the total types of damage effects available
	// Removing for to account for EDamageEffectType with no effects associated

	const int32 MaxParticles = DamageEffectParticles.Num();
	
	if (GetLocalRole() == ROLE_Authority)
	{
		const UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);
		const AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession());
		check(Session);

		if (!Session)
		{
			UE_LOG(TitansCharacter, Error, TEXT("AIBaseCharacter::ReplicateParticle - Session can't be found"));
			return;
		}
		
		TArray<FDamageParticleInfo>& MutableReplicatedDamageParticles = GetReplicatedDamageParticles_Mutable();

		// Make sure to have at least one available 
		const int32 MaxEffectCurrentType = FMath::Max(Session->MaximumDamageEffectsPerType.FindRef(DamageInfo.DamageEffectType), 1) ;

		/** Hold the number of Damage particle per type */
		TMap<EDamageEffectType, int32> ParticlesPerType {};
		
		int32 RequestedParticleCount = 0;
		int32 HighestCountParticle = -1;
		int32 HighestParticleCountFirstIndex = -1;
		
		// Collect info on how many of each type exists
		for (int32 i = 0; i < MutableReplicatedDamageParticles.Num(); i++)
		{
			const FDamageParticleInfo& DamageParticle = MutableReplicatedDamageParticles[i];

			if (DamageParticle.DamageEffectType == DamageInfo.DamageEffectType)
			{
				RequestedParticleCount++;
				if (RequestedParticleCount >= MaxEffectCurrentType)
				{
					//Already have hit the maximum for this type of particle.
					return;
				}
			}
			
			int32& ParticleCount = ParticlesPerType.FindOrAdd(DamageParticle.DamageEffectType);
			ParticleCount++;
			if (ParticleCount > HighestCountParticle)
			{
				HighestCountParticle = ParticleCount;
				HighestParticleCountFirstIndex = i;
			}
		}

		// If max damage particles reached then ensure at least one of this type of particles is active
		if (GetReplicatedDamageParticles().Num() >= MaxParticles && HighestCountParticle > 1)
		{
			// We have one type of effect which more than one particle, remove it.
			MutableReplicatedDamageParticles.RemoveAt(HighestParticleCountFirstIndex);
		}
		
		MutableReplicatedDamageParticles.Add(DamageInfo);
	}
}

void AIBaseCharacter::ReplicateParticle(FCosmeticParticleInfo EffectInfo)
{
	//GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::ReplicateParticle()")));

	if (GetLocalRole() == ROLE_Authority)
	{
		// Once additional cosmetic particles are added, optimize how the different effects are mixed (similar to what is being done for damage particles).
		// For now, just add the effect to the array.
		GetReplicatedCosmeticParticles_Mutable().Add(EffectInfo);
	}
}
void AIBaseCharacter::StartCheckingLocalParticles()
{
	if (!GetWorldTimerManager().IsTimerActive(TimerHandle_CheckLocalParticles))
	{
		GetWorldTimerManager().SetTimer(TimerHandle_CheckLocalParticles, this, &AIBaseCharacter::CheckLocalParticles, 1.0f, true);
	}
}

void AIBaseCharacter::StopCheckingLocalParticles()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_CheckLocalParticles);
}

void AIBaseCharacter::CheckLocalParticles()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer())
	{
		return;
	}
	
	if (PreviewDamageParticleLoadHandle)
	{
		const FPOTParticleDefinition& FXInfo = DamageEffectParticles.FindChecked(DamageEffectLoadingType);
		if (!FXInfo.Attribute.IsValid())
		{
			if (FMath::IsNearlyZero(AbilitySystem->GetNumericAttribute(FXInfo.Attribute)))
			{
				PreviewDamageParticleLoadHandle->CancelHandle();
			}
		}
	}

	//This could be more efficient. We may want to rework as more cosmetic effects are added.
	TArray<TPair<float, float>> WetnessInfo;
	if (AbilitySystem->WetnessEffect != nullptr)
	{
		FGameplayEffectQuery Query = FGameplayEffectQuery();
		Query.EffectDefinition = AbilitySystem->WetnessEffect;
		WetnessInfo = AbilitySystem->GetActiveEffectsTimeRemainingAndDuration(Query);
	}
	if (PreviewCosmeticParticleLoadHandle &&
		CosmeticEffectLoadingType == ECosmeticEffectType::WET
		&& WetnessInfo.IsEmpty())
	{
		PreviewCosmeticParticleLoadHandle->CancelHandle();
	}

	if (IsAlive())
	{
		StartCheckingLocalParticles();

		// Find particles that shouldn't exist before spawning a new one
		for (int i = LocalDamageEffectParticles.Num() - 1; i >= 0; i--)
		{
			if (LocalDamageEffectParticles.IsValidIndex(i))
			{
				bool bShouldRemove = true;
				for (const FDamageParticleInfo& ReplicatedDamageParticle: GetReplicatedDamageParticles())
				{
					// If hit location and damage effect type exist in both arrays then it shouldn't be removed
					if (LocalDamageEffectParticles[i].HitLocation == ReplicatedDamageParticle.HitLocation && LocalDamageEffectParticles[i].DamageEffectType == ReplicatedDamageParticle.DamageEffectType)
					{
						bShouldRemove = false;
						break;
					}
				}

				if (bShouldRemove)
				{
					DeregisterDamageParticle(LocalDamageEffectParticles[i]);
				}
			}
		}

		for (int i = LocalCosmeticEffectParticles.Num() - 1; i >= 0; i--)
		{
			if (LocalCosmeticEffectParticles.IsValidIndex(i))
			{
				bool bShouldRemove = true;
				for (const FCosmeticParticleInfo& ReplicatedCosmeticParticle : GetReplicatedCosmeticParticles())
				{
					// If the cosmetic effect type exist in both arrays then it shouldn't be removed
					if (LocalCosmeticEffectParticles[i].CosmeticEffectType == ReplicatedCosmeticParticle.CosmeticEffectType)
					{
						bShouldRemove = false;
						break;
					}
				}

				if (bShouldRemove)
				{
					DeregisterCosmeticParticle(LocalCosmeticEffectParticles[i]);
				}
			}
		}

	}
	else
	{
		DeregisterAllDamageParticles();
		DeregisterAllCosmeticParticles();
	}
#endif
}

TArray<FDamageParticleInfo>& AIBaseCharacter::GetReplicatedDamageParticles_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, ReplicatedDamageParticles, this);
	return ReplicatedDamageParticles;
}

void AIBaseCharacter::OnRep_ReplicatedDamageParticles()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;

	CheckLocalParticles();

	if (IsAlive())
	{
		// Spawn any particles that doesn't already exist.
		for (const FDamageParticleInfo& DamageParticle : GetReplicatedDamageParticles())
		{
			// Already Exists
			bool bActive = false;

			for (const FDamageParticleInfo& LocalDamageParticle : LocalDamageEffectParticles)
			{
				if (DamageParticle.HitLocation == LocalDamageParticle.HitLocation && DamageParticle.DamageEffectType == LocalDamageParticle.DamageEffectType)
				{
					bActive = true;
					break;
				}
			}

			// If not active/doesn't exist add it to pending list.
			if (!bActive)
			{
				RegisterDamageParticle(FDamageParticleInfo(DamageParticle.DamageEffectType, DamageParticle.HitLocation, DamageParticle.HitRotation));
			}
		}
	}
#endif
}

TArray<FCosmeticParticleInfo>& AIBaseCharacter::GetReplicatedCosmeticParticles_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, ReplicatedCosmeticParticles, this);
	return ReplicatedCosmeticParticles;
}

void AIBaseCharacter::OnRep_ReplicatedCosmeticParticles()
{
#if !UE_SERVER
	if (IsRunningDedicatedServer()) return;

	CheckLocalParticles();

	if (IsAlive())
	{
		// Spawn any particles that doesn't already exist.
		for (const FCosmeticParticleInfo& CosmeticParticle : GetReplicatedCosmeticParticles())
		{
			// Already Exists
			bool bActive = false;
			for (const FCosmeticParticleInfo& LocalCosmeticParticle : LocalCosmeticEffectParticles)
			{
				if (CosmeticParticle.CosmeticEffectType == LocalCosmeticParticle.CosmeticEffectType)
				{
					bActive = true;
					break;
				}
			}

			// If not active/doesn't exist add it to pending list.
			if (!bActive)
			{
				RegisterCosmeticParticle(FCosmeticParticleInfo(CosmeticParticle.CosmeticEffectType, CosmeticParticle.EffectRotation));
			}
		}
	}
#endif
}

void AIBaseCharacter::RemoveReplicatedDamageParticle(EDamageEffectType DamageEffectTypeToRemove)
{
	bool bNeedsDirtying = false;
	for (int32 Index = GetReplicatedDamageParticles().Num() - 1; Index >= 0; --Index)
	{
		// No longer active and should be removed
		if (GetReplicatedDamageParticles()[Index].DamageEffectType == DamageEffectTypeToRemove)
		{
			ReplicatedDamageParticles.RemoveAt(Index);
			bNeedsDirtying = true;
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, ReplicatedDamageParticles, this);
	}
}

void AIBaseCharacter::RemoveReplicatedCosmeticParticle(ECosmeticEffectType CosmeticEffectTypeToRemove)
{
	bool bNeedsDirtying = false;
	for (int32 Index = GetReplicatedCosmeticParticles().Num() - 1; Index >= 0; --Index)
	{
		// No longer active and should be removed
		if (GetReplicatedCosmeticParticles()[Index].CosmeticEffectType == CosmeticEffectTypeToRemove)
		{
			ReplicatedCosmeticParticles.RemoveAt(Index);
			bNeedsDirtying = true;
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, ReplicatedCosmeticParticles, this);
	}
}

TSoftObjectPtr<UFXSystemAsset> AIBaseCharacter::GetDamageParticle(const FDamageParticleInfo& DamageParticleInfo)
{
	return DamageEffectParticles.FindRef(DamageParticleInfo.DamageEffectType).FXAsset;
}

TSoftObjectPtr<UFXSystemAsset> AIBaseCharacter::GetCosmeticParticle(const FCosmeticParticleInfo CosmeticParticleInfo)
{
	return CosmeticEffectParticles.FindRef(CosmeticParticleInfo.CosmeticEffectType);
}

void AIBaseCharacter::RegisterDamageParticle(FDamageParticleInfo DamageParticleInfo)
{
	TSoftObjectPtr<UFXSystemAsset> SoftDamageParticle = GetDamageParticle(DamageParticleInfo);

	LoadDamageParticle(DamageParticleInfo);
}

void AIBaseCharacter::RegisterCosmeticParticle(FCosmeticParticleInfo CosmeticParticleInfo)
{
	TSoftObjectPtr<UFXSystemAsset> SoftCosmeticParticle = GetCosmeticParticle(CosmeticParticleInfo);

	LoadCosmeticParticle(CosmeticParticleInfo);
}

void AIBaseCharacter::GetParticleLocation(const FVector& WorldPosition, FClosestPointOnPhysicsAsset& ClosestPointOnPhysicsAsset, bool bApproximate) const
{
	const USkeletalMeshComponent* SkeletalMeshComp = GetMesh();
	const USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset();
	const UPhysicsAsset* PhysicsAsset = SkeletalMeshComp->GetPhysicsAsset();
	const FReferenceSkeleton* RefSkeleton = SkeletalMesh ? &SkeletalMesh->GetRefSkeleton() : nullptr;
	if (PhysicsAsset && RefSkeleton)
	{
		const TArray<FTransform>& BoneTransforms = SkeletalMeshComp->GetComponentSpaceTransforms();

		if (!BoneTransforms.IsEmpty())
		{
			constexpr bool bHasMasterPoseComponent = false;
			const FVector ComponentPosition = SkeletalMeshComp->GetComponentTransform().InverseTransformPosition(WorldPosition);

			float CurrentClosestDistance = FLT_MAX;
			int32 CurrentClosestBoneIndex = INDEX_NONE;
			const UBodySetup* CurrentClosestBodySetup = nullptr;

#if UE_BUILD_DEVELOPMENT
			UE_LOG(LogTemp, Verbose, TEXT("BoneTransforms: %s"), *FString::FromInt(BoneTransforms.Num()));
#endif
			for (const UBodySetup* BodySetupInstance : PhysicsAsset->SkeletalBodySetups)
			{
				ClosestPointOnPhysicsAsset.Distance = FLT_MAX;
				const FName BoneName = BodySetupInstance->BoneName;
				const int32 BoneIndex = RefSkeleton->FindBoneIndex(BoneName);
				if (BoneIndex != INDEX_NONE)
				{
					//const FTransform BoneTM = bHasMasterPoseComponent ? GetBoneTransform(BoneIndex) : BoneTransforms[BoneIndex];
					FTransform BoneTM;
					if (bHasMasterPoseComponent)
					{
						BoneTM = SkeletalMeshComp->GetBoneTransform(BoneIndex);
					}
					if (BoneTransforms.IsValidIndex(BoneIndex))
					{
						BoneTM = BoneTransforms[BoneIndex];
					}
					const float Dist = bApproximate ? (BoneTM.GetLocation() - ComponentPosition).SizeSquared() : BodySetupInstance->GetShortestDistanceToPoint(ComponentPosition, BoneTM);

					if (Dist < CurrentClosestDistance)
					{
						CurrentClosestDistance = Dist;
						CurrentClosestBoneIndex = BoneIndex;
						CurrentClosestBodySetup = BodySetupInstance;

						if (Dist <= 0.f) { break; }
					}
				}
			}

			if (CurrentClosestBoneIndex >= 0)
			{
				const FTransform BoneTM = bHasMasterPoseComponent ? SkeletalMeshComp->GetBoneTransform(CurrentClosestBoneIndex) : (BoneTransforms[CurrentClosestBoneIndex] * SkeletalMeshComp->GetComponentTransform());
				ClosestPointOnPhysicsAsset.Distance = CurrentClosestBodySetup->GetClosestPointAndNormal(WorldPosition, BoneTM, ClosestPointOnPhysicsAsset.ClosestWorldPosition, ClosestPointOnPhysicsAsset.Normal);
				ClosestPointOnPhysicsAsset.BoneName = CurrentClosestBodySetup->BoneName;

				//DrawDebugSphere(GetWorld(), ClosestPointOnPhysicsAsset.ClosestWorldPosition, 32, 32, FColor::Red, false, 10.f);
			}
		}
	}
}

void AIBaseCharacter::LoadDamageParticle(FDamageParticleInfo DamageParticleInfo, bool bRegister /*= true*/)
{
	if (DamageParticleInfo.HitLocation.IsZero())
	{
		return;
	}

	const TSoftObjectPtr<UFXSystemAsset> SoftDamageParticle = GetDamageParticle(DamageParticleInfo);
	
	//  Async Load the particle
	if (IsAlive())
	{
		DamageEffectLoadingType = DamageParticleInfo.DamageEffectType;

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		PreviewDamageParticleLoadHandle = Streamable.RequestAsyncLoad(
			SoftDamageParticle.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnDamageParticleLoaded, DamageParticleInfo, bRegister),
			FStreamableManager::DefaultAsyncLoadPriority, false);
	}
}

void AIBaseCharacter::LoadCosmeticParticle(FCosmeticParticleInfo CosmeticParticleInfo, bool bRegister /*= true*/)
{
	const TSoftObjectPtr<UFXSystemAsset> SoftCosmeticParticle = GetCosmeticParticle(CosmeticParticleInfo);

	// Async Load the particle
	if (IsAlive())
	{
		CosmeticEffectLoadingType = CosmeticParticleInfo.CosmeticEffectType;

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		PreviewCosmeticParticleLoadHandle = Streamable.RequestAsyncLoad(
			SoftCosmeticParticle.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnCosmeticParticleLoaded, CosmeticParticleInfo, bRegister),
			FStreamableManager::DefaultAsyncLoadPriority, false);
	}
}

void AIBaseCharacter::OnDamageParticleLoaded(FDamageParticleInfo DamageParticleInfo, bool bRegister /*= true*/)
{
	if (!IsValid(this))
	{
		return;
	}
	if (UFXSystemAsset* LoadedParticle = GetDamageParticle(DamageParticleInfo).Get())
	{
		if (const USkeletalMeshComponent* const Mesh3P = GetMesh())
		{
			FClosestPointOnPhysicsAsset ClosestWorldPosition;

			const FRotator HitRotationDifference = (DamageParticleInfo.HitRotation - GetActorRotation());
			const FVector HitRelativeLocationCorrected = DamageParticleInfo.HitLocation.RotateAngleAxis(HitRotationDifference.Yaw, FVector(0.0f, 0.0f, 1.0f));

			FVector WorldLocation;
			if (DamageParticleInfo.BoneName == NAME_None)
			{
				WorldLocation = GetActorLocation() + HitRelativeLocationCorrected;
				GetParticleLocation(WorldLocation, ClosestWorldPosition, true);
			}
			else
			{
				const FVector BoneLocation = Mesh3P->GetSocketLocation(DamageParticleInfo.BoneName);
				WorldLocation = BoneLocation + HitRelativeLocationCorrected;
				ClosestWorldPosition.BoneName = DamageParticleInfo.BoneName;
				ClosestWorldPosition.ClosestWorldPosition = WorldLocation;
				ClosestWorldPosition.Normal = (WorldLocation - BoneLocation);
				ClosestWorldPosition.Normal = ClosestWorldPosition.Normal.GetSafeNormal();
			}

			const FRotator HitNormalRot = FRotationMatrix::MakeFromX((ClosestWorldPosition.Normal * -1)).Rotator();

			FDamageParticleInfo NewDamageParticle = DamageParticleInfo;
			UFXSystemComponent* TempParticleRef = nullptr;
			if (LoadedParticle->IsA<UNiagaraSystem>())
			{
				TempParticleRef = UNiagaraFunctionLibrary::SpawnSystemAttached(
					Cast<UNiagaraSystem>(LoadedParticle),
					GetMesh(),
					ClosestWorldPosition.BoneName,
					ClosestWorldPosition.ClosestWorldPosition,
					HitNormalRot,
					FVector(1.f),
					EAttachLocation::KeepWorldPosition,
					false,
					ENCPoolMethod::None,
					true,
					false);
			}
			else
			{
				TempParticleRef = UGameplayStatics::SpawnEmitterAttached(
					Cast<UParticleSystem>(LoadedParticle),
					GetMesh(),
					ClosestWorldPosition.BoneName,
					ClosestWorldPosition.ClosestWorldPosition,
					HitNormalRot,
					GetMesh()->GetComponentScale(),
					EAttachLocation::KeepWorldPosition,
					true);
			}

			NewDamageParticle.DamageParticle = TempParticleRef;

			if (bRegister)
			{
				LocalDamageEffectParticles.Add(NewDamageParticle);
			}
		}
	}
}

void AIBaseCharacter::OnCosmeticParticleLoaded(FCosmeticParticleInfo CosmeticParticleInfo, bool bRegister /*= true*/)
{
	if (!IsValid(this))
	{
		return;
	}
	
	if (UFXSystemAsset* LoadedParticle = GetCosmeticParticle(CosmeticParticleInfo).Get())
	{
		if (const USkeletalMeshComponent* const Mesh3P = GetMesh())
		{
			FClosestPointOnPhysicsAsset ClosestWorldPosition;

			FVector WorldLocation;
			if (CosmeticParticleInfo.BoneName == NAME_None)
			{
				WorldLocation = GetActorLocation();
				GetParticleLocation(WorldLocation, ClosestWorldPosition, true);
			}
			else
			{
				const FVector BoneLocation = Mesh3P->GetSocketLocation(CosmeticParticleInfo.BoneName);
				WorldLocation = BoneLocation;
				ClosestWorldPosition.Normal = (WorldLocation - BoneLocation);
				ClosestWorldPosition.Normal = ClosestWorldPosition.Normal.GetSafeNormal();
			}

			FRotator EffectNormalRot = FRotationMatrix::MakeFromX((ClosestWorldPosition.Normal * -1)).Rotator();

			FCosmeticParticleInfo NewCosmeticParticle = CosmeticParticleInfo;

			UFXSystemComponent* TempParticleRef = nullptr;
			if (LoadedParticle->IsA<UNiagaraSystem>())
			{
				TempParticleRef = UNiagaraFunctionLibrary::SpawnSystemAttached(
					Cast<UNiagaraSystem>(LoadedParticle),
					GetMesh(),
					NAME_None,
					FVector(0.f),
					FRotator(0.f),
					FVector(1.f),
					EAttachLocation::SnapToTarget,
					true,
					ENCPoolMethod::None,
					true,
					false);
			}
			else
			{
				TempParticleRef = UGameplayStatics::SpawnEmitterAttached(
					Cast<UParticleSystem>(LoadedParticle),
					GetMesh(),
					NAME_None,
					FVector(0.f),
					FRotator(0.f),
					GetMesh()->GetComponentScale(),
					EAttachLocation::KeepWorldPosition,
					true);
			}

			NewCosmeticParticle.CosmeticParticle = TempParticleRef;

			if (bRegister)
			{
				LocalCosmeticEffectParticles.Add(NewCosmeticParticle);
			}
		}
	}
}

void AIBaseCharacter::DeregisterDamageParticle(FDamageParticleInfo DamageParticleInfo, bool bDeregisterAll /*= false*/)
{
	// Cancel loading new particle if one of it's kind are trying to be removed.
	if (PreviewDamageParticleLoadHandle.IsValid() && DamageEffectLoadingType == DamageParticleInfo.DamageEffectType)
	{
		PreviewDamageParticleLoadHandle->CancelHandle();
	}

	if (LocalDamageEffectParticles.IsEmpty())
	{
		return;
	}
	
	for (int i = 0; i < LocalDamageEffectParticles.Num(); i++)
	{
		FDamageParticleInfo* CurrentParticle = &LocalDamageEffectParticles[i];
		if (!CurrentParticle || CurrentParticle->DamageEffectType != DamageParticleInfo.DamageEffectType || !CurrentParticle->DamageParticle || DamageParticleInfo.HitLocation != CurrentParticle->HitLocation)
		{
			continue;
		}

		CurrentParticle->DamageParticle->DeactivateImmediate();
		if (!bDeregisterAll)
		{
			LocalDamageEffectParticles.RemoveAt(i);
			return;
		}
		
		CurrentParticle->DamageParticle = nullptr;
	}
}

void AIBaseCharacter::DeregisterCosmeticParticle(FCosmeticParticleInfo CosmeticParticleInfo, bool bDeregisterAll /*= false*/)
{
	// Cancel loading new particle if one of it's kind are trying to be removed.
	if (PreviewCosmeticParticleLoadHandle.IsValid() && CosmeticEffectLoadingType == CosmeticParticleInfo.CosmeticEffectType)
	{
		PreviewCosmeticParticleLoadHandle->CancelHandle();
	}

	if (LocalCosmeticEffectParticles.IsEmpty())
	{
		return;
	}
	for (int i = 0; i < LocalCosmeticEffectParticles.Num(); i++)
	{
		FCosmeticParticleInfo* CurrentParticle = &LocalCosmeticEffectParticles[i];
		if (CurrentParticle->CosmeticParticle && CurrentParticle->CosmeticEffectType == CosmeticParticleInfo.CosmeticEffectType)
		{
			CurrentParticle->CosmeticParticle->DeactivateImmediate();

			if (!bDeregisterAll)
			{
				LocalCosmeticEffectParticles.RemoveAt(i);
				return;
			}
			
			CurrentParticle->CosmeticParticle = nullptr;
		}
	}
}

void AIBaseCharacter::DeregisterAllDamageParticles()
{
	for (const auto& EffectParticle : DamageEffectParticles)
	{
		DeregisterDamageParticle(FDamageParticleInfo(EffectParticle.Key, FVector(ForceInitToZero), FRotator(ForceInitToZero)), true);
	} 
	
	LocalDamageEffectParticles.Empty();

	StopCheckingLocalParticles();
}

void AIBaseCharacter::DeregisterAllCosmeticParticles()
{
	for (const auto& EffectParticle : CosmeticEffectParticles)
	{
		DeregisterCosmeticParticle(FCosmeticParticleInfo(EffectParticle.Key, FRotator(ForceInitToZero)), true);
	}

	LocalCosmeticEffectParticles.Empty();

	StopCheckingLocalParticles();
}

bool AIBaseCharacter::CanSprint()
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return ICharMove && 
		(GetStamina() > 0) && 
		GetCharacterMovement() && 
		!IsJumpProvidingForce() && 
		(!IsSwimming() || ICharMove->bCanUseAdvancedSwimming) && 
		!IsInitiatedJump() && 
		!GetCharacterMovement()->IsFalling() && 
		!IsLimpingRaw() && 
		!IsFeignLimping() && 
		!IsRestingOrSleeping() && 
		(GetCurrentMovementMode() == ECustomMovementMode::BASICMOVEMENT || (GetCurrentMovementMode() == ECustomMovementMode::PRECISEMOVEMENT && IsSwimming() && bCanSprintDuringPreciseMovementSwimming)) &&
		!HasBrokenLegs() && !IsExhausted();
}

bool AIBaseCharacter::CanTrot() const
{
	bool CanTrot = GetCharacterMovement() && !IsLimpingRaw() && !IsFeignLimping() && !IsRestingOrSleeping() && !bIsCrouched && !HasBrokenLegs() && !bDesiresPreciseMovement;

	bool Swimming = !GetCharacterMovement()->IsSwimming() && !GetCharacterMovement()->IsInWater();
	if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		Swimming = Swimming || ICharMove->bCanUseAdvancedSwimming;		
	}

	return Swimming && CanTrot;
}

bool AIBaseCharacter::CanTurnInPlace()
{
	if (bIsBasicInputDisabled || IsRestingOrSleeping() || IsLimpingRaw() || IsFeignLimping() || IsEatingOrDrinking() || GetCharacterMovement()->IsInWater() || GetCharacterMovement()->IsSwimming()) return false;

	// Make sure the player isn't playing a use montage / prevent them from alt turning
	if (IsMontagePlaying(CurrentUseAnimMontage)) return false;

	return true;
}


//TODO: Remove This
bool AIBaseCharacter::IsSprinting() const
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return (ICharMove && ICharMove->IsSprinting());
}

bool AIBaseCharacter::IsSwimming() const
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return (ICharMove && ICharMove->IsSwimming());
}

bool AIBaseCharacter::IsFlying() const
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return (ICharMove && ICharMove->IsFlying());
}

//TODO: Remove This
bool AIBaseCharacter::IsTrotting() const
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return (ICharMove && ICharMove->IsTrotting());
}

//TODO: Remove This
bool AIBaseCharacter::IsWalking() const
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return (ICharMove && ICharMove->IsWalking());
}

//TODO: Remove This
bool AIBaseCharacter::IsNotMoving() const
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	return (ICharMove && ICharMove->IsNotMoving());
}

bool AIBaseCharacter::IsAtWaterSurface() const
{
	//FName TraceTag("IsAtWaterSurface");
	//GetWorld()->DebugDrawTraceTag = TraceTag;

	float SpaceNeededToFloatScaled = SpaceNeededToFloat;
	float FloatOffsetScaled = FloatOffset;

	if (const AIDinosaurCharacter* IDinosaurCharacter = Cast<AIDinosaurCharacter>(this))
	{
		SpaceNeededToFloatScaled = (SpaceNeededToFloat * IDinosaurCharacter->GetScaleForGrowth(GetGrowthPercent()));
		FloatOffsetScaled = (FloatOffset * IDinosaurCharacter->GetScaleForGrowth(GetGrowthPercent()));
	}

	// Line Trace from top of capsule to detect water below
	FVector TraceStart = GetActorLocation();
	const float ScaledCapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + FloatOffsetScaled;

	TraceStart.Z += ScaledCapsuleHalfHeight;
	FVector TraceEnd = FVector(TraceStart.X, TraceStart.Y, (TraceStart.Z - SpaceNeededToFloatScaled));

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);
	//TraceParams.TraceTag = FName("IsAtWaterSurface");
	FCollisionObjectQueryParams ObjectTraceParams;
	ObjectTraceParams.AddObjectTypesToQuery(ECC_GameTraceChannel8);

	FHitResult Hit(ForceInit);
	return !GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectTraceParams, TraceParams);
}

bool AIBaseCharacter::IsInOrAboveWater(FHitResult* OutHit, const FVector* CastStart) const
{
	FVector TraceStart = GetActorLocation();
	if (CastStart)
	{
		TraceStart = *CastStart;
	}

	TraceStart.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight();	
	FVector TraceEnd = FVector(TraceStart.X, TraceStart.Y, (TraceStart.Z - 25000));

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);
	TraceParams.AddIgnoredActor(Cast<AActor>(GetCurrentlyCarriedObject().Object.Get()));
	//TraceParams.TraceTag = FName("IsAtWaterSurface");
	FCollisionObjectQueryParams ObjectTraceParams;
	ObjectTraceParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTraceParams.AddObjectTypesToQuery(ECC_GameTraceChannel8);

	FHitResult Hit(ForceInit);
	bool bSuccess = GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectTraceParams, TraceParams);
	if (!Cast<AIWater>(Hit.GetActor()))
	{
		return false;
	}
	if (OutHit)
	{
		*OutHit = Hit;
	}
	return bSuccess;
}

bool AIBaseCharacter::IsInWater(float PercentageNeededToTrigger /*= 50.0f*/, FVector StartLocation /*= FVector(ForceInit)*/) const
{
	//FName TraceTag("IsAtWaterSurface");
	//GetWorld()->DebugDrawTraceTag = TraceTag;

	// Line Trace from top of capsule to x% of the way to the bottom to see if large part of capsule is in water
	FVector TraceStart = StartLocation;
	if (StartLocation == FVector(ForceInit))
	{
		TraceStart = GetActorLocation();
	}
	TraceStart.Z += GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector TraceEnd = FVector(TraceStart.X, TraceStart.Y, (TraceStart.Z - ((GetCapsuleComponent()->GetScaledCapsuleHalfHeight() * 2.0f) * (PercentageNeededToTrigger / 100))));

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(this);
	//TraceParams.TraceTag = FName("IsAtWaterSurface");
	FCollisionObjectQueryParams ObjectTraceParams;
	ObjectTraceParams.AddObjectTypesToQuery(ECC_GameTraceChannel8);

	FHitResult Hit(ForceInit);
	return GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectTraceParams, TraceParams);
}

void AIBaseCharacter::AdjustCapsuleWhenSwimming(ECustomMovementType NewMovementType)
{
	if (UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		if (ICharMove->bCanUseAdvancedSwimming)
		{
			AIBaseCharacter* DefaultCharacter = GetClass()->GetDefaultObject<AIBaseCharacter>();

			if (NewMovementType == ECustomMovementType::SWIMMING)
			{
				// See if collision is already at desired size.
				if (GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()) return;

				GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
			}
			else if (NewMovementType == ECustomMovementType::DIVING)
			{
				// See if collision is already at desired size.
				if (GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == ICharMove->GetCrouchedHalfHeight()) return;

				GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), ICharMove->GetCrouchedHalfHeight(), true);
			}
		}
	}
}

ECustomMovementType AIBaseCharacter::GetCustomMovementType()
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	if (!ICharMove) return ECustomMovementType::NONE;

	const FAttachTarget& AttachTarget = GetAttachTarget();

	if (AttachTarget.IsValid())
	{
		switch (AttachTarget.AttachType)
		{
		case EAttachType::AT_Latch:
			return ECustomMovementType::LATCHED;
		default:
			return ECustomMovementType::NONE;
		}
	}

	// Swimming
	if (ICharMove->IsSwimming() || ICharMove->IsInWater())
	{
		if (IsAtWaterSurface())
		{
			//AdjustCapsuleWhenSwimming(ECustomMovementType::SWIMMING);
			return ECustomMovementType::SWIMMING;
		}
		else
		{
			//AdjustCapsuleWhenSwimming(ECustomMovementType::DIVING);
			return ECustomMovementType::DIVING;
		}
	}

	// Flying
	if (ICharMove->IsFlying()) return ECustomMovementType::FLYING;

	// Falling
	if (ICharMove->IsFalling()) return ECustomMovementType::FALLING;

	// Jumping
	if (IsJumpProvidingForce() || IsInitiatedJump()) return ECustomMovementType::JUMPING;

	// Limping
	if (ICharMove->bWantsToLimp) return ECustomMovementType::LIMPING;

	// Not Moving
	if (ICharMove->bWantsToStop) return ECustomMovementType::STANDING;

	// Crouched
	if (bIsCrouched) return ECustomMovementType::CROUCHING;

	// Sprinting
	if (ICharMove->IsSprinting()) return ECustomMovementType::SPRINTING;

	// Trotting
	if (ICharMove->IsTrotting()) return ECustomMovementType::TROTTING;

	// Walking
	return GetVelocity().SizeSquared() > 100.f ? ECustomMovementType::WALKING : ECustomMovementType::STANDING;
}

ECustomMovementMode AIBaseCharacter::GetCustomMovementMode()
{
	UICharacterMovementComponent* ICharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	if (!ICharMove) return ECustomMovementMode::NONE;

	if (GetAttachTarget().IsValid()) return ECustomMovementMode::NONE;

	// Turn In Place or Precise Movement
	if (ICharMove->bWantsToPreciseMove)
	{
		if (!IsSwimming() && !IsFlying() && (GetVelocity().Size() == 0) && ICharMove->bUseControllerDesiredRotation)
		{
			return ECustomMovementMode::TURNINPLACE;
		}
		else
		{
			return ECustomMovementMode::PRECISEMOVEMENT;
		}
	}

	// Walking
	return ECustomMovementMode::BASICMOVEMENT;
}

bool AIBaseCharacter::IsLimping() const
{
	if (GetCurrentMovementType() == ECustomMovementType::LIMPING) return true;

	UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());
	if (!CharMove) return false;

	return CharMove->IsLimping() || IsLimpingRaw();
}

float AIBaseCharacter::GetLimpThreshold() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetLimpHealthThresholdAttribute());
}

bool AIBaseCharacter::HasBrokenLegs() const
{
	return GetLegDamage() > 0.f;
}

float AIBaseCharacter::GetLegDamage() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetLegDamageAttribute());
}

bool AIBaseCharacter::AreLegsBroken() const
{
	return GetLegDamage() > 0.f;
}

bool AIBaseCharacter::IsResting() const
{
	return GetRestingStance() == EStanceType::Resting;
}

bool AIBaseCharacter::IsAttacking() const
{
	return bAttacking;
}

float AIBaseCharacter::GetSprintingSpeedModifier() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetSprintingSpeedMultiplierAttribute());
}

float AIBaseCharacter::GetTrottingSpeedModifier() const
{
	return AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetTrottingSpeedMultiplierAttribute());
}

//UAnimMontage* AIBaseCharacter::GetDrinkAnimMontage()
//{
//	return DrinkAnimMontage;
//}

TSoftObjectPtr<UAnimMontage> AIBaseCharacter::GetDrinkAnimMontageSoft()
{
	return DrinkAnimMontage;
}

bool AIBaseCharacter::CanDrinkWater_Implementation()
{
	return !IsEatingOrDrinking() && !IsResting() && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround() && !IsUsingAbility();
}

void AIBaseCharacter::StartDrinkingWater(AIWater* Water)
{
	//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::StartDrinkingWater %s"), *Water->WaterTag.ToString());
	bIsDrinking = true;
	ForceUncrouch();

	if (HasAuthority())
	{
		ResetServerUseTimer();

		if (Water)
		{
			// If water is valid then make weak ptrs for the delegate to try to avoid the water getting GC'd - Poncho

			// Bind New Delegate
			const TWeakObjectPtr<AIWater> WaterWeakPtr = Water;
			ServerUseTimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this, WaterWeakPtr]()
			{
				if (!WaterWeakPtr.IsValid())
				{
					return;
				}
				DrinkWater(WaterWeakPtr.Get());
			});
		}
		else
		{
			ServerUseTimerDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::DrinkWater, Water);
		}

		// Setup Timer to Restore Water Per Tick while Being Used
		GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, WaterConsumedTickRate, true);
	}
}

void AIBaseCharacter::StopDrinkingWater()
{
	//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::StopDrinkingWater"));
	if (CurrentUseAnimMontage)
	{
		StopAnimMontage(CurrentUseAnimMontage);
	}

	CurrentUseAnimMontage = nullptr;
	bIsDrinking = false;

	if (HasAuthority())
	{
		ResetServerUseTimer();
	}
}

void AIBaseCharacter::StartDrinkingWaterFromLandscape()
{
	LoadUseAnim(0.0f, DrinkAnimMontage, true);
}

void AIBaseCharacter::StartEatingBody(AIBaseCharacter* Body)
{
	bIsEating = true;
	ForceUncrouch();

	if (HasAuthority())
	{
		if (IsValid(Body))
		{
			Body->ActivelyEatingBody.Add(this);
		}

		ResetServerUseTimer();

		// Bind New delgate
		ServerUseTimerDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::EatBody, Body);

		// Setup Timer to Restore Food Per Tick while Being Used
		GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, FoodConsumedTickRate, true);
	}
}

void AIBaseCharacter::StartEatingPlant(UIFocusTargetFoliageMeshComponent* Plant)
{
	bIsEating = true;
	ForceUncrouch();

	if (HasAuthority())
	{
		ResetServerUseTimer();

		// Bind New delegate
		ServerUseTimerDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::EatPlant, Plant);

		// Setup Timer to Restore Food Per Tick while Being Used
		GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, FoodConsumedTickRate, true);
	}
}

void AIBaseCharacter::StartEatingFood(AIFoodItem* Food)
{
	bIsEating = true;
	ForceUncrouch();

	if (HasAuthority())
	{
		ResetServerUseTimer();

		// Bind New delegate
		TWeakObjectPtr<AIFoodItem> FoodWeakPtr = Food;
		ServerUseTimerDelegate = FTimerDelegate::CreateWeakLambda(this, [this, FoodWeakPtr]
		{
			if (!FoodWeakPtr.IsValid())
			{
				return;
			}

			EatFood(FoodWeakPtr.Get());
		});

		// Setup Timer to Restore Food Per Tick while Being Used
		GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, Food->IsBeingSwallowed() ? (IUsableInterface::Execute_GetPrimaryUseDuration(Food, this) - 0.5f) : FoodConsumedTickRate, !Food->IsBeingSwallowed());
	}
}


void AIBaseCharacter::StartEatingCritter(class AICritterPawn* Critter)
{
	bIsEating = true;

	if (HasAuthority())
	{
		if (IsValid(Critter))
		{
			Critter->ActivelyEatingBody.Add(this);
		}

		ResetServerUseTimer();

		// Bind New delegate
		ServerUseTimerDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::EatCritter, Critter);

		// Setup Timer to Restore Food Per Tick while Being Used
		GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, FoodConsumedTickRate, true);
	}
}

void AIBaseCharacter::StartDisturbingCritter(AIAlderonCritterBurrowSpawner* CritterBurrow)
{
	bIsDisturbing = true;

	if (HasAuthority())
	{
		ResetServerUseTimer();

		// Bind New delegate
		ServerUseTimerDelegate = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::DisturbCritter, CritterBurrow);

		// Setup Timer to Restore Food Per Tick while Being Used
		const int32 RandDeviation = WITH_EDITOR ? 5 : FMath::RandRange(10, 20);
		GetWorldTimerManager().SetTimer(ServerUseTimerHandle, ServerUseTimerDelegate, RandDeviation, false);
	}
}

void AIBaseCharacter::StopDisturbingCritter(AIAlderonCritterBurrowSpawner* CritterBurrow)
{
	bIsDisturbing = false;

	if (HasAuthority())
	{
		ForceStopEating();

		if (CritterBurrow)
		{
			CritterBurrow->SetIsInteractionInProgress(false);
		}
	}
}

bool AIBaseCharacter::IsDisturbingCritter() const
{
	return bIsDisturbing;
}

void AIBaseCharacter::DisturbCritter(AIAlderonCritterBurrowSpawner* CritterBurrow)
{
	if (HasAuthority())
	{
		StopDisturbingCritter(CritterBurrow);

		if (CritterBurrow)
		{
			CritterBurrow->TrySpawnCritter();
			const float TimeStamp = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
			CritterBurrow->SetLastSpawnTimeStamp(TimeStamp);
		}
	}
}

void AIBaseCharacter::ResetServerUseTimer()
{
	// Clear Timer
	if (GetWorldTimerManager().TimerExists(ServerUseTimerHandle))
	{
		GetWorldTimerManager().ClearTimer(ServerUseTimerHandle);
	}

	// Unbind Use Delegate
	if (ServerUseTimerDelegate.IsBound())
	{
		ServerUseTimerDelegate.Unbind();
	}
}

void AIBaseCharacter::StopAllAnimMontages()
{
	USkeletalMeshComponent* UseMesh = GetMesh(); 
	if (UseMesh && UseMesh->AnimScriptInstance)
	{
		UseMesh->AnimScriptInstance->Montage_Stop(0.0f);
	} 
}

void AIBaseCharacter::DestroyCosmeticComponents()
{
	// Done in a shitty way to not crash PS5
	TArray<UActorComponent*> ComponentsToDestroy;
	ComponentsToDestroy.Reserve(5);
	ComponentsToDestroy.Add(ThirdPersonSpringArmComponent);
	
	if (IsValid(SplashParticleComponent))
	{
		SplashParticleComponent->DestroyComponent();
	}

	if (IsValid(AudioSplashComp))
	{
		AudioSplashComp->DestroyComponent();
	}
	
	if (MapIconComponent)
	{
		MapIconComponent->UnregisterComponent();
		MapIconComponent->DestroyComponent();
		MapIconComponent = nullptr;
	}

	if (HasAuthority())
	{
		GetReplicatedDamageParticles_Mutable().Empty();
		GetReplicatedCosmeticParticles_Mutable().Empty();
	}

	OnRep_ReplicatedDamageParticles();
	OnRep_ReplicatedCosmeticParticles();

	ThirdPersonSpringArmComponent = nullptr;

	if (!IsAlive())
	{
#if !UE_SERVER
		if (!IsRunningDedicatedServer())
		{
			ComponentsToDestroy.Add(BreatheTimeline);

			StopBreathing();
			StopBlinking(true);
		}
#endif
		// Cant destroy ability system yet for some reason
		//ComponentsToDestroy.Add(AbilitySystem);
		//AbilitySystem = nullptr;
	}

	for (UActorComponent* Component : ComponentsToDestroy)
	{
		if (Component == nullptr) continue;

		if (!IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer() && !IsAlive())
	{
		// Stop blinking and breathing when dead
		USkeletalMeshComponent* SkelMesh = GetMesh();
		SkelMesh->SetMorphTarget(NAME_Breathe, 0);
		SkelMesh->SetMorphTarget(NAME_Blink, 1);
	}
#endif
}

UAudioComponent* AIBaseCharacter::PlayCharacterSound(USoundCue* CueToPlay)
{
	// Invalid Cue / Error Handling
	if (!CueToPlay)
	{
		UE_LOG(TitansCharacter, Error, TEXT("IBaseCharacter::PlayCharacterSound - CueToPlay is Null"));
		return nullptr;
	}

	return UGameplayStatics::SpawnSoundAttached(CueToPlay, RootComponent, NAME_None, FVector::ZeroVector, EAttachLocation::SnapToTarget, true);
}

uint8 AIBaseCharacter::GetFocusPriority_Implementation()
{
	return static_cast<uint8>(EFocusPriority::FP_MEDIUM);
}

FVector AIBaseCharacter::GetUseLocation_Implementation(class AIBaseCharacter* User, int32 Item)
{
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		return CharacterMesh->GetSocketLocation(PelvisBoneName);
	}

	return GetActorLocation();
}

bool AIBaseCharacter::IsUseMeshStatic_Implementation(class AIBaseCharacter* User, int32 Item)
{
	return UsableFoodData.bStaticFoodMesh;
}

TSoftObjectPtr<USkeletalMesh> AIBaseCharacter::GetUseSkeletalMesh_Implementation(class AIBaseCharacter* User, int32 Item)
{
	return UsableFoodData.SkeletalFoodMesh;
}

TSoftObjectPtr<UStaticMesh> AIBaseCharacter::GetUseStaticMesh_Implementation(class AIBaseCharacter* User, int32 Item)
{
	return UsableFoodData.StaticFoodMesh;
}

FVector AIBaseCharacter::GetUseOffset_Implementation(class AIBaseCharacter* User, int32 Item)
{
	return UsableFoodData.FoodMeshOffset;
}

FRotator AIBaseCharacter::GetUseRotation_Implementation(class AIBaseCharacter* User, int32 Item)
{
	return UsableFoodData.FoodMeshRotation;
}

bool AIBaseCharacter::CanBeFocusedOnBy_Implementation(AIBaseCharacter * User)
{
	return IsValid(User) && !IsAlive();
}

void AIBaseCharacter::OnBecomeFocusTarget_Implementation(class AIBaseCharacter* Focuser)
{
	if (Cast<AIAdminCharacter>(Focuser))
	{
		USkeletalMeshComponent* SkeletalMesh = FindComponentByClass<USkeletalMeshComponent>();
		SkeletalMesh->SetRenderCustomDepth(true);
		SkeletalMesh->SetCustomDepthStencilValue(STENCIL_ITEMHIGHLIGHT);
	}
}

void AIBaseCharacter::OnStopBeingFocusTarget_Implementation(class AIBaseCharacter* Focuser)
{
	USkeletalMeshComponent* SkeletalMesh = FindComponentByClass<USkeletalMeshComponent>();
	SkeletalMesh->SetRenderCustomDepth(false);
}

void AIBaseCharacter::GetFocusTargetOutlineMeshComponents_Implementation(TArray<UMeshComponent*>& OutOutlineMeshComponents)
{
	if (GetMesh() && !IsPendingKillPending() && !IsAlive())
	{
		OutOutlineMeshComponents.Add(GetMesh());
	}
}

float AIBaseCharacter::GetPrimaryUseDuration_Implementation(AIBaseCharacter* User)
{
	// Return INDEX_NONE for interactive use that can be canceled
	return INDEX_NONE;
}

TSoftObjectPtr<UAnimMontage> AIBaseCharacter::GetPrimaryUserAnimMontageSoft_Implementation(class AIBaseCharacter* User)
{
	return User->GetEatAnimMontageSoft();
}

bool AIBaseCharacter::CanPrimaryBeUsedBy_Implementation(AIBaseCharacter* User)
{
	if (AIDinosaurCharacter* DinoUser = Cast<AIDinosaurCharacter>(User))
	{
		if (IsPendingKillPending()) return false;
		if (DinoUser->IsEatingOrDrinking() || DinoUser->IsResting()) return false;
		if (!User->IsHungry()) return false;
		if (User->IsUsingAbility()) return false;
		if (!User->ContainsFoodTag(NAME_Meat)) return false;

		if (UCharacterMovementComponent* CMC = DinoUser->GetCharacterMovement())
		{
			if (!CMC->IsMovingOnGround() && !DinoUser->IsSwimming()) return false;
		}
	}

	return true;
}

bool AIBaseCharacter::UsePrimary_Implementation(AIBaseCharacter* User, int32 Item)
{
	if (!User || User->IsPendingKillPending()) return false;

	User->StartEatingBody(this);

	return true;
}

void AIBaseCharacter::EndUsedPrimaryBy_Implementation(AIBaseCharacter* User)
{
	if (User && !User->IsPendingKillPending())
	{
		User->StopEating();
	}
}

void AIBaseCharacter::OnClientCancelUsePrimaryCorrection_Implementation(class AIBaseCharacter* User)
{
	if (User && !User->IsPendingKillPending())
	{
		User->StopEating();
	}
}

float AIBaseCharacter::GetCarryUseDuration_Implementation(class AIBaseCharacter* User)
{
	// Return INDEX_NONE for interactive use that can be canceled
	return 0.0f;
}

bool AIBaseCharacter::CanCarryBeUsedBy_Implementation(AIBaseCharacter* User)
{
	if (IsPendingKillPending()) return false;
	if (User->IsEatingOrDrinking() || User->IsResting() || User->IsUsingAbility()) return false;
	if (IsAlive()) return false;
	if (IsHidden()) return false;
	if (GetCurrentBodyFood() <= 0) return false;
	if (bCarryInProgress) return false;

	return true;
}

bool AIBaseCharacter::UseCarry_Implementation(AIBaseCharacter* User, int32 Item)
{
	if (!User || User->IsPendingKillPending()) return false;

	if (CanTakeQuestItem(User))
	{
		SpawnQuestItem(User);
	}
	else
	{
		SpawnMeatChunk(User);
	}

	return true;
}

void AIBaseCharacter::EndUsedCarryBy_Implementation(AIBaseCharacter* User)
{
	// do nothing yet
}

void AIBaseCharacter::OnClientCancelUseCarryCorrection_Implementation(AIBaseCharacter* User)
{
	// do nothing yet
}

void AIBaseCharacter::SpawnMeatChunk(class AIBaseCharacter* Eater, bool bSkipAnimOnPickup /*= false*/, bool bAffectFoodValue /*= false*/, bool bAllowPickupWhileFalling /*= false*/)
{
	if (HasAuthority())
	{
		// Only allow taking up to a specific percentage of body food while alive
		if (IsAlive() && GetCurrentBodyFood() <= GetMaxBodyFood() * MinAliveBodyFoodPercentage + 1.0f)
		{
			return;
		}

		if (Eater && GetCurrentBodyFood() > 0 && MeatChunkClassesSoft.Num() > 0 && MeatChunkEater == nullptr)
		{
			MeatChunkEater = Eater;
			bMeatChunkSkipAnimOnPickup = bSkipAnimOnPickup;
			bMeatChunkAllowPickupWhileFalling = bAllowPickupWhileFalling;
			for (const FMeatChunkData& MeatChunkSoftRef : MeatChunkClassesSoft)
			{
				MeatChunksToLoad.Add(FMeatChunkData(MeatChunkSoftRef));
			}
			StartLoadingMeatChunk();
		}
	}
}

void AIBaseCharacter::StartLoadingMeatChunk()
{
	if (MeatChunksToLoad.Num() > 0)
	{
		// Meatchunk already loaded
		if (MeatChunksToLoad[0].MeatChunkSoft.IsValid() && MeatChunksToLoad[0].MeatChunkSoft->GetDefaultObject())
		{
			return OnMeatChunkLoaded(FMeatChunkData(MeatChunksToLoad[0].MeatChunkSoft.Get(), MeatChunksToLoad[0].MeatChunkSoft, MeatChunksToLoad[0].MeatChunkFoodValue));
		}

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		MeatChunkLoadHandle = Streamable.RequestAsyncLoad(
			MeatChunksToLoad[0].MeatChunkSoft.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnMeatChunkLoaded, FMeatChunkData(nullptr, MeatChunksToLoad[0].MeatChunkSoft, MeatChunksToLoad[0].MeatChunkFoodValue)),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
}

void AIBaseCharacter::OnMeatChunkLoaded(FMeatChunkData MeatChunkHard)
{
	MeatChunkHard.MeatChunkHard = MeatChunkHard.MeatChunkSoft.Get();
	if (!MeatChunkHard.MeatChunkHard) return;

	if (!MeatChunkEater) return;

	AllowedMeatChunks.Add(MeatChunkHard);
	MeatChunksToLoad.Remove(MeatChunkHard);

	if (MeatChunksToLoad.Num() == 0)
	{
		if (GetCurrentBodyFood() > 0 && AllowedMeatChunks.Num() > 0)
		{
			TArray<FMeatChunkData> AllowedMeatChunkClassesAndFoodAmount;
			TSubclassOf<AIMeatChunk> MeatChunkPtr = nullptr;

			for (const FMeatChunkData& MeatChunkType: AllowedMeatChunks)
			{
				if (AIMeatChunk* MeatChunkRef = MeatChunkType.MeatChunkHard.GetDefaultObject())
				{
					if (ICarryInterface::Execute_GetCarryData(MeatChunkRef).CarriableSize == MeatChunkEater->GetMaxCarriableSize())
					{
						AllowedMeatChunkClassesAndFoodAmount.Add(MeatChunkType);
					}
				}
			}

			FMeatChunkData MeatChunkClass;
			int RandomThreshold = FMath::RandRange(0, 100);
			int CurrentThreshold = 0;
			int i = 0;

			for (const FMeatChunkData& MeatChunkType: AllowedMeatChunkClassesAndFoodAmount)
			{
				i++;

				CurrentThreshold += 100 / AllowedMeatChunkClassesAndFoodAmount.Num();

				if (CurrentThreshold >= RandomThreshold)
				{
					MeatChunkClass = MeatChunkType;
					break;
				}

				// Last resort
				if (i == AllowedMeatChunkClassesAndFoodAmount.Num())
				{
					MeatChunkClass = MeatChunkType;
				}
			}

			if (IsAlive() && GetCurrentBodyFood() - MeatChunkClass.MeatChunkFoodValue < GetMaxBodyFood() * MinAliveBodyFoodPercentage)
			{
				MeatChunkClass.MeatChunkFoodValue = GetCurrentBodyFood() - (GetMaxBodyFood() * MinAliveBodyFoodPercentage);
			}

			if (MeatChunkClass.MeatChunkFoodValue > GetCurrentBodyFood())
			{
				MeatChunkClass.MeatChunkFoodValue = GetCurrentBodyFood();
			}

			AIMeatChunk* MeatChunkRef = Cast<AIMeatChunk>(MeatChunkClass.MeatChunkHard->GetDefaultObject());
			if (!MeatChunkRef) return;

			FTransform BodyTransformIgnoringScale = GetActorTransform();
			BodyTransformIgnoringScale.SetLocation(GetPelvisLocation());
			BodyTransformIgnoringScale.SetScale3D(FVector(MeatChunkRef->GetRequiredScaleToCarry(MeatChunkEater)));

			AIMeatChunk* SpawnedMeatChunk = GetWorld()->SpawnActorDeferred<AIMeatChunk>(MeatChunkClass.MeatChunkHard, BodyTransformIgnoringScale, nullptr, MeatChunkEater, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			if (SpawnedMeatChunk)
			{
				SpawnedMeatChunk->SetupMeatChunk(MeatChunkClass.MeatChunkFoodValue, MeatChunkEater, this, bMeatChunkSkipAnimOnPickup, bMeatChunkAllowPickupWhileFalling);

				// Finish Spawning Actor
				UGameplayStatics::FinishSpawningActor(SpawnedMeatChunk, BodyTransformIgnoringScale);

				//RestoreBodyFood(MeatChunkClass.MeatChunkFoodValue * -1.0f);

				MeatChunkEater = nullptr;
				AllowedMeatChunks.Empty();
				MeatChunksToLoad.Empty();
			}
		}
	}
	else
	{
		StartLoadingMeatChunk();
	}
}

void AIBaseCharacter::SpawnQuestItem(class AIBaseCharacter* Eater)
{
	if (HasAuthority())
	{
		if (!HasSpawnedQuestItem() && (QuestItemClass.Get() || QuestItemClass.ToSoftObjectPath().IsValid()))
		{
			MeatChunkEater = Eater;
			StartLoadingQuestItem();
		}
	}
}

void AIBaseCharacter::StartLoadingQuestItem()
{
	if (QuestItemClass.ToSoftObjectPath().IsValid())
	{
		// Meatchunk already loaded
		if (QuestItemClass.Get())
		{
			return OnQuestItemLoaded();
		}

		// Start Async Loading
		FStreamableManager& Streamable = UIGameplayStatics::GetStreamableManager(this);
		MeatChunkLoadHandle = Streamable.RequestAsyncLoad(
			QuestItemClass.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::OnQuestItemLoaded),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
}

void AIBaseCharacter::OnQuestItemLoaded()
{
	TSubclassOf<AIQuestItem> QuestItem = QuestItemClass.Get();
	if (!HasSpawnedQuestItem() && QuestItem && MeatChunkEater)
	{
		FTransform BodyTransformIgnoringScale = GetActorTransform();
		BodyTransformIgnoringScale.SetLocation(FVector(BodyTransformIgnoringScale.GetLocation().X, BodyTransformIgnoringScale.GetLocation().Y, BodyTransformIgnoringScale.GetLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

		BodyTransformIgnoringScale.SetLocation(GetPelvisLocation());

		BodyTransformIgnoringScale.SetScale3D(FVector(1.0f, 1.0f, 1.0f));

		AIQuestItem* SpawnedQuestItem = GetWorld()->SpawnActorDeferred<AIQuestItem>(QuestItem, BodyTransformIgnoringScale, nullptr, MeatChunkEater, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!SpawnedQuestItem)
		{
			return;
		}

		SpawnedQuestItem->SetTrophyCharacterId(MeatChunkEater->GetCharacterID());
		SpawnedQuestItem->SetTrophyAlderonId(GetCombatLogAlderonId());

		if (AIPlayerState* IPlayerState = MeatChunkEater->GetPlayerState<AIPlayerState>())
		{
			SpawnedQuestItem->SetTrophyCreatorAlderonId(IPlayerState->GetAlderonID());
		}

		MeatChunkEater = nullptr;

		// Finish Spawning Actor
		UGameplayStatics::FinishSpawningActor(SpawnedQuestItem, BodyTransformIgnoringScale);
		SetHasSpawnedQuestItem(true);

		SpawnedQuestItem->ToggleVisibility(false);
	}

	if (bDelayDestructionTillQuestItemSpawned) Destroy();
}

FRotator AIBaseCharacter::GetBaseAimRotation() const
{
	if (bUseSeparateAimRotation)
	{
		return Super::GetBaseAimRotation() + AimRotation + DefaultAimOffset;
	} else {
		return Super::GetBaseAimRotation();
	}
}

void AIBaseCharacter::CameraIn_Implementation()
{
	if (UIFullMapWidget::StaticMapActive(this)) return;
	if (UIInteractionMenuWidget::StaticInteractionMenuActive(this)) return;

	if (ThirdPersonSpringArmComponent)
	{
		SetDesireSpringArmLength(DesireSpringArmLength - CameraDollyTargetAdjustSize);
	}
}

void AIBaseCharacter::CameraOut_Implementation()
{
	if (UIFullMapWidget::StaticMapActive(this)) return;
	if (UIInteractionMenuWidget::StaticInteractionMenuActive(this)) return;

	if (ThirdPersonSpringArmComponent)
	{
		SetDesireSpringArmLength(DesireSpringArmLength + CameraDollyTargetAdjustSize);
	}
}

void AIBaseCharacter::OnCameraZoom(const FInputActionValue& Value)
{
	if (UIFullMapWidget::StaticMapActive(this)) return;
	if (UIInteractionMenuWidget::StaticInteractionMenuActive(this)) return;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	
	if (IGameInstance->IsInputHardwareGamepad() && (!IGameHUD || !IGameHUD->bTabSystemActive))
	{	// Only zoom on gamepad when tab system is open
		return;
	}

	if (IGameHUD && IGameHUD->bQuestWindowActive) return;

	if (!Value.IsNonZero()) return;
	
	if (Value.GetValueType() == EInputActionValueType::Boolean)
	{
		if (Value.Get<float>() > 0.f)
		{
			CameraIn();
		}
		else
		{
			CameraOut();
		}
	}
	else if (Value.GetValueType() == EInputActionValueType::Axis1D)
	{

		if (ThirdPersonSpringArmComponent)
		{
			SetDesireSpringArmLength(DesireSpringArmLength + Value.Get<float>());
		}
	}
}

FVector2D AIBaseCharacter::GetCurrentCameraDistanceRange() const
{
	const bool bIsCarried = (GetAttachTarget().IsValid() && GetAttachTarget().AttachType == EAttachType::AT_Carry);
	return bIsCarried ? CameraDistanceRangeWhileCarried : CameraDistanceRange;
}

FVector AIBaseCharacter::GetHeadLocation() const
{
	if (GetMesh() && GetMesh()->GetBoneIndex(HeadBoneName) != INDEX_NONE)
	{
		return GetMesh()->GetBoneLocation(HeadBoneName);
	} else {
		return GetActorLocation() + (FVector(0.f, 0.f, bIsCrouched ? CrouchedEyeHeight : BaseEyeHeight) * GetActorScale().Size());
	}
}

FVector AIBaseCharacter::GetPelvisLocation() const
{
	if (GetPelvisLocationRaw() != FVector::ZeroVector)
	{
		return GetPelvisLocationRaw();
	}
	
	return GetActorLocation();
}

void AIBaseCharacter::UpdateCameraOffset(float DeltaTime)
{
	if (!IsAlive()) return;

	if (bEnableNewCameraSystem)
	{
		UpdateCameraOffsetBP(DeltaTime);
	}
	else
	{
		//need to scale camera offsets by actor scale (for growth)
		const float ActorScale = GetActorScale().Size();

		FRotator ControlRot = GetControlRotation();
		FRotator ActorRot = GetActorRotation();
		FRotator DeltaRot = (ControlRot - ActorRot).GetNormalized();

		const float YawDeltaSize = FMath::Abs(DeltaRot.Yaw);
		const float FacingReversedAlpha = FMath::Clamp((YawDeltaSize - 90.f) / 90.f, 0.f, 1.f);
		const float TurnAlpha = FMath::Clamp(FMath::Abs(DeltaRot.Yaw / 90.f) * (1 - FMath::Abs(DeltaRot.Pitch / 90)), 0.f, 1.f);
		const bool bTurnRight = (DeltaRot.Yaw > 0);
		//FVector NewCamOffset = FMath::Lerp(FMath::Lerp(EvaluateCentreCameraOffsetPitchBlend(DeltaRot.Pitch), bTurnRight ? EvaluateRightCameraOffsetPitchBlend(DeltaRot.Pitch) : EvaluateLeftCameraOffsetPitchBlend(DeltaRot.Pitch), TurnAlpha), EvaluateReversedCameraOffsetPitchBlend(DeltaRot.Pitch), FacingReversedAlpha) * ActorScale;

		float CamSpeedOffset = 0.f;

		const float ScaledDesiredSpringArmLength = DesireSpringArmLength * ActorScale;
		const float ScaledCameraDollyRate = CameraDollyRate * ActorScale;
		const FVector2D ScaledCameraDistanceRange = GetCurrentCameraDistanceRange() * ActorScale;

		//abort if we dnt have a spring arm
		if (!IsValid(ThirdPersonSpringArmComponent)) return;

		//Update spring arm length
		if (ThirdPersonSpringArmComponent->TargetArmLength != ScaledDesiredSpringArmLength)
		{
			float TargetArmLength = ScaledDesiredSpringArmLength;
			float ArmLengthDelta = DeltaTime * ((TargetArmLength > ThirdPersonSpringArmComponent->TargetArmLength) ? FMath::Min(ScaledCameraDollyRate, TargetArmLength - ThirdPersonSpringArmComponent->TargetArmLength) : FMath::Max(-ScaledCameraDollyRate, TargetArmLength - ThirdPersonSpringArmComponent->TargetArmLength));
			ThirdPersonSpringArmComponent->TargetArmLength += ArmLengthDelta;
		}

		//Clamp spring arm length
		ThirdPersonSpringArmComponent->TargetArmLength = FMath::Clamp(ThirdPersonSpringArmComponent->TargetArmLength, ScaledCameraDistanceRange.X, ScaledCameraDistanceRange.Y);

		//make sure we have a camera component
		UCameraComponent* CamComp = FindComponentByClass<UCameraComponent>();
		if (!IsValid(CamComp)) return;

		//default spring arm location is the default 
		FVector NewSpingArmLoc = GetMesh()->GetComponentTransform().TransformPosition(GetDefault<AIBaseCharacter>(GetClass())->ThirdPersonSpringArmComponent->GetRelativeLocation() * ActorScale);
		FVector NewSocketOffset = GetDefault<AIBaseCharacter>(GetClass())->ThirdPersonSpringArmComponent->SocketOffset * ActorScale;

		//abort if we don't haev a contribution curve
		if (!CameraDistanceToHeadMovementContributionCurve) return;

		//scale to the head offset
		const float DistancePct = FMath::GetRangePct(ScaledCameraDistanceRange, ThirdPersonSpringArmComponent->TargetArmLength);
		const float Alpha = CameraDistanceToHeadMovementContributionCurve->GetFloatValue(DistancePct);

		//there will be no socket offset or target offset where the camera is in full ots
		ThirdPersonSpringArmComponent->TargetOffset = GetDefault<AIBaseCharacter>(GetClass())->ThirdPersonSpringArmComponent->TargetOffset * (1 - Alpha) * ActorScale;
		ThirdPersonSpringArmComponent->SocketOffset = GetDefault<AIBaseCharacter>(GetClass())->ThirdPersonSpringArmComponent->SocketOffset * (1 - Alpha) * ActorScale;

		//set the target spring arm rotation to the control rotation
		FRotator TargetSpringArmRotation = ControlRot;
		//if (bTurnRight && (CamOffsetCentre.SpringArmMaxPitchCurve || CamOffsetRight.SpringArmMaxPitchCurve))
		//{
		//	//Constrain the spring arms pitch rotaion betwee -90 and the maximimum allowed by SpringArmMaxPitchCurve (pct of distance from farthest to nearest as alpha).
		//	//Camera is also deconstrained by the degree to which the camera is facing the opposite direction from the viewed character
		//	TargetSpringArmRotation.Pitch = FMath::ClampAngle(FRotator::NormalizeAxis(TargetSpringArmRotation.Pitch), -90, FMath::Lerp(CamOffsetCentre.SpringArmMaxPitchCurve ? FMath::Lerp(CamOffsetCentre.SpringArmMaxPitchCurve->GetFloatValue(DistancePct), 90.f, FacingReversedAlpha) : 90, CamOffsetRight.SpringArmMaxPitchCurve ? FMath::Lerp(CamOffsetRight.SpringArmMaxPitchCurve->GetFloatValue(DistancePct), 90.f, FacingReversedAlpha) : 90, TurnAlpha));
		//}
		//else if (!bTurnRight && (CamOffsetCentre.SpringArmMaxPitchCurve || CamOffsetLeft.SpringArmMaxPitchCurve))
		//{
		//	//as above for the left curve
		//	TargetSpringArmRotation.Pitch = FMath::ClampAngle(FRotator::NormalizeAxis(TargetSpringArmRotation.Pitch), -90, FMath::Lerp(CamOffsetCentre.SpringArmMaxPitchCurve ? FMath::Lerp(CamOffsetCentre.SpringArmMaxPitchCurve->GetFloatValue(DistancePct), 90.f, FacingReversedAlpha) : 90, CamOffsetLeft.SpringArmMaxPitchCurve ? FMath::Lerp(CamOffsetLeft.SpringArmMaxPitchCurve->GetFloatValue(DistancePct), 90.f, FacingReversedAlpha) : 90, TurnAlpha));
		//}

		ThirdPersonSpringArmComponent->SetWorldRotation(TargetSpringArmRotation); //TODO: lerp
		CamComp->SetWorldRotation(ControlRot); //cam comp is always %100 the control rotation
		//ThirdPersonSpringArmComponent->bDoCollisionTest = Alpha == 0;

		//apply camera zoom effect based on relative velocity compared to camera direction
		const float TargetCamSpeedEffectAlpha = FMath::Clamp(FMath::GetRangePct(CameraRotRelativeSpeedRange, GetVelocity().Size2D() * (FVector::DotProduct(GetVelocity().GetSafeNormal(), CamComp->GetForwardVector()))), 0.f, 1.f);

		if (CurrentSpeedBasedCameraEffectAlpha > TargetCamSpeedEffectAlpha)
		{
			CurrentSpeedBasedCameraEffectAlpha = FMath::Max(CurrentSpeedBasedCameraEffectAlpha - (SpeedBasedCameraEffectLerpRate * DeltaTime), TargetCamSpeedEffectAlpha);
		}
		else if (CurrentSpeedBasedCameraEffectAlpha < TargetCamSpeedEffectAlpha)
		{
			CurrentSpeedBasedCameraEffectAlpha = FMath::Min(CurrentSpeedBasedCameraEffectAlpha + (SpeedBasedCameraEffectLerpRate * DeltaTime), TargetCamSpeedEffectAlpha);
		}

		//CamComp->SetFieldOfView(FMath::GetRangeValue(CameraRotRelativeFOVRange, CurrentSpeedBasedCameraEffectAlpha));
	}
}

void AIBaseCharacter::OnPushComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AIBaseCharacter* OtherBaseActor = Cast<AIBaseCharacter>(OtherActor);
	if (this != OtherActor && OtherBaseActor != nullptr)
	{
		PushingCharacters.AddUnique(OtherBaseActor);
	}
}

void AIBaseCharacter::OnPushComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	PushingCharacters.RemoveSingleSwap(Cast<AIBaseCharacter>(OtherActor));
}

void AIBaseCharacter::PushCharacter(float DeltaTime)
{
	if (PushingCharacters.Num() == 0 || !bPushEnabled) return;

	float Weight = AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetCombatWeightAttribute());

	for (int i(PushingCharacters.Num() - 1); i >= 0; i--)
	{
		if (PushingCharacters.IsValidIndex(i) && PushingCharacters[i] != nullptr)
		{
			FVector PushingCharacterToThis = GetActorLocation() - PushingCharacters[i]->GetActorLocation();
			FVector PushDirection = PushingCharacterToThis.GetSafeNormal();

			PushDirection.X = FMath::RoundHalfFromZero(PushDirection.X);
			PushDirection.Y = FMath::RoundHalfFromZero(PushDirection.Y);

			float TargetWeight = PushingCharacters[i]->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetCombatWeightAttribute());
			float WeightRatio = TargetWeight / Weight;

			FVector ResultingVector = PushDirection * BasePushForce * DeltaTime * WeightRatio;

			/*FVector ResultingVectorNew = FVector(
				ResultingVector.X + ResultingVector.Z,
				ResultingVector.Y + ResultingVector.Z,
				0.f
			);*/

			AddActorWorldOffset(ResultingVector, true);
		}
	}
}

void AIBaseCharacter::SetDesireSpringArmLength(const float NewDesireSpringArmLength)
{
	const FVector2D Range = GetCurrentCameraDistanceRange();
	DesireSpringArmLength = FMath::Clamp(NewDesireSpringArmLength, Range.X, Range.Y);
}

void AIBaseCharacter::SliceThroughBone(const FName BoneName)
{
	//if (USkeletalMeshComponent* SkMesh = Cast<USkeletalMeshComponent>(GetMesh())) SkMesh->Get
}

void AIBaseCharacter::OnRep_ActiveQuests()
{
	//UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::OnRep_ActiveQuests()"));

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (IsLocallyControlled())
		{
			UIGameplayStatics::GetIPlayerController(this)->ClientClearPreviousQuestMarkers();

			const TArray<UIQuest*>& QuestsToCheck = GetActiveQuests();
			TArray<FQuestDescriptiveTextCooldown> TempQuestDescriptiveCooldowns = QuestDescriptiveCooldowns;

			for (FQuestDescriptiveTextCooldown QuestDescriptiveCooldown: TempQuestDescriptiveCooldowns)
			{
				bool bTaskFound = false;
				for (UIQuest* Quest : QuestsToCheck)
				{
					if (!Quest) continue;
					for (UIQuestBaseTask* QuestTask : Quest->GetQuestTasks())
					{
						if (!QuestTask) continue;
						if (QuestTask == QuestDescriptiveCooldown.QuestItemTask)
						{
							bTaskFound = true;
							break;
						}
					}

					if (bTaskFound) break;
				}

				if (!bTaskFound)
				{
					QuestDescriptiveCooldowns.Remove(QuestDescriptiveCooldown);
				}
			}

			const AIPlayerState* const IPlayerState = GetPlayerState<AIPlayerState>();
			if (IPlayerState && !IPlayerState->GetPlayerGroupActor())
			{
				CheckIfUserShouldBeNotifiedToClaimRewards();
			}
		}
	}
#endif

	const TArray<UIQuest*>& QuestsToCheck = GetActiveQuests();
	for (UIQuest* Quest : QuestsToCheck)
	{
		if (!Quest) continue;

		if (!Quest->QuestData)
		{
			if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
			{
				if (IWorldSettings->QuestManager)
				{
					IWorldSettings->QuestManager->LoadQuestData(Quest->GetQuestId(), FQuestDataLoaded::CreateUObject(this, &AIBaseCharacter::OnActiveQuestDataLoaded, Quest));
				}
			}
		}
		else
		{
			OnActiveQuestDataLoaded(Quest->QuestData, Quest);
		}
	}
}

void AIBaseCharacter::OnRep_UncollectedRewardQuests()
{
	for (UIQuest* Quest : GetUncollectedRewardQuests())
	{
		if (!Quest) continue;

		if (!Quest->QuestData)
		{
			if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
			{
				if (IWorldSettings->QuestManager)
				{
					IWorldSettings->QuestManager->LoadQuestData(Quest->GetQuestId(), FQuestDataLoaded::CreateUObject(this, &AIBaseCharacter::OnUncollectedRewardQuestDataLoaded, Quest));
				}
			}
		}
	}

	UpdateUnclaimedQuestRewardsTimer();
	
	if (IsLocallyControlled()) 
	{
		CheckIfUserShouldBeNotifiedToClaimRewards();
	}
}
void AIBaseCharacter::OnRep_LocationsProgress()
{
	const AIPlayerController* IPlayerController = Cast<AIPlayerController>(GetController());
	if (!IPlayerController) return;
	if (AIGameHUD* const IGameHUD = Cast<AIGameHUD>(IPlayerController->GetHUD()))
	{
		IGameHUD->UpdateQuestCompletionPercentage();
	}

}

void AIBaseCharacter::CheckIfUserShouldBeNotifiedToClaimRewards()
{
	if (IsLocallyControlled()) 
	{
		FString Key = HaveMetMaxUnclaimedRewards() ? "ClaimRewardsForNewQuest" : "";
		Client_NotifyUserToClaimRewards(Key, "");
	}
	else
	{
		AIPlayerState* const IPlayerState = GetPlayerState<AIPlayerState>();
		if (!IPlayerState || !IPlayerState->GetPlayerGroupActor()) 
		{
			return;
		}

		IPlayerState->GetPlayerGroupActor()->ValidateAndPropagateGroupMemberQuestAvailabilityStatus();
	}
}

void AIBaseCharacter::Client_NotifyUserToClaimRewards_Implementation(const FString& Key, const FString& User)
{
	// The checks are split out this way because the pointers are daisy-chained in such a way that attempting to simplify produces worse code.
	AIPlayerController* PC = GetController<AIPlayerController>();
	if (!ensureAlways(PC) || !ensureAlways(PC->GetHUD<AIGameHUD>())) return;

	AIGameHUD* HUD = PC->GetHUD<AIGameHUD>();
		
	UIMainGameHUDWidget* ActiveMainGameHUDWidget = Cast<UIMainGameHUDWidget>(HUD->ActiveHUDWidget);
	if (!ensureAlways(ActiveMainGameHUDWidget) || !ensureAlways(IsValid(ActiveMainGameHUDWidget->W_QuestTracking))) return;

	ActiveMainGameHUDWidget->W_QuestTracking->NotifyUserToClaimQuestRewards(Key, User);
}

void AIBaseCharacter::UpdateUnclaimedQuestRewardsTimer()
{
	UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
	if (GetUncollectedRewardQuests().Num() > 0 && (!IGameUserSettings || IGameUserSettings->bShowUnclaimedQuestRewardsNotifications))
	{
		if (!GetWorldTimerManager().IsTimerActive(TimerHandle_UnclaimedQuestRewards))
		{
			GetWorldTimerManager().SetTimer(TimerHandle_UnclaimedQuestRewards, this, &AIBaseCharacter::OnUnclaimedQuestRewardsTimer, UnclaimedQuestRewardsNoticeTime, false);
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(TimerHandle_UnclaimedQuestRewards);
	}
}

void AIBaseCharacter::OnUnclaimedQuestRewardsTimer()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_UnclaimedQuestRewards);

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	if (IGameHUD)
	{
		IGameHUD->ShowUnclaimedQuestRewardsNotice();
	}
	
	UpdateUnclaimedQuestRewardsTimer();
}

void AIBaseCharacter::QuestTasksReplicated(UIQuest* Quest)
{
	if (!ensure(Quest))
	{
		return;
	}
	if (AITutorialManager* ITutorialManager = UIGameplayStatics::GetTutorialManager(this))
	{
		bool bAllTasksLoaded = true;
		for (UIQuestBaseTask* Task : Quest->GetQuestTasks())
		{
			if (!IsValid(Task))
			{
				bAllTasksLoaded = false;
				break;
			}
		}

		if (bAllTasksLoaded)
		{
			// Remove highlights for quests that are completed/failed etc
			ITutorialManager->ClearOldQuestHighlights();
			ITutorialManager->ClearAllHighlights(true, Quest);

			for (UIQuestBaseTask* Task : Quest->GetQuestTasks())
			{
				if (!ensure(Task))
				{
					continue; // Task should be loaded here
				}

				TArray<FTutorialHighlight> Highlights;
				for (FTutorialHighlight Highlight : Task->TutorialHighlights)
				{
					Highlight.bFromQuest = true;
					Highlight.Quest = Quest;
					Highlights.Add(Highlight);
				}

				// Add all highlights at once so that skippable highlights 
				// at the beginning of the array don't immediately get skipped
				ITutorialManager->AddHighlightedWidgetTargetStructs(Highlights);
			}
		}
		else
		{
			// Task is not replicated yet, set a timer
			FTimerHandle TimerHandle_QuestTaskLoad;
			FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::QuestTasksReplicated, Quest);
			GetWorldTimerManager().SetTimer(TimerHandle_QuestTaskLoad, Del, 0.5f, false);
		}
	}
}

void AIBaseCharacter::OnActiveQuestDataLoaded(UQuestData* QuestData, UIQuest* Quest)
{
	//UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::OnActiveQuestDataLoaded()"));

	if (!Quest) return;
	if (!QuestData || !QuestData->IsValidLowLevel()) return;

	Quest->QuestData = QuestData;

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (IsLocallyControlled())
		{
			// Update tutorial widget highlights
			if (AITutorialManager* ITutorialManager = UIGameplayStatics::GetTutorialManager(this))
			{
				// Remove highlights for quests that are completed/failed etc
				ITutorialManager->ClearOldQuestHighlights();

				// Check for all tasks being loaded
				bool bAllTasksLoaded = true;
				for (UIQuestBaseTask* Task : Quest->GetQuestTasks())
				{
					if (!Task)
					{
						bAllTasksLoaded = false;
						break;
					}
				}
				if (!bAllTasksLoaded)
				{
					// Task is not replicated yet, set a timer
					FTimerHandle TimerHandle_QuestTaskLoad;
					FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::QuestTasksReplicated, Quest);
					GetWorldTimerManager().SetTimer(TimerHandle_QuestTaskLoad, Del, 0.5f, false);
				}
				else
				{
					// Task is replicated, update highlights
					QuestTasksReplicated(Quest);
				}
			}

			if (QuestData->QuestShareType == EQuestShareType::Survival)
			{
				StartUpdateNearestItem();
				bHasSurvivalQuest = true;
			}

			bool bFoundSurvivalQuest = false;
			bool bAllQuestsLoaded = true;
			UIQuest* FoundHomeCaveTrackingQuest = nullptr;
			UIQuest* FoundWaystoneTrackingQuest = nullptr;
			for (UIQuest* IQuest : GetActiveQuests())
			{
				if (!IQuest || !IQuest->QuestData)
				{
					bAllQuestsLoaded = false;
					break;
				}

				if (IQuest->QuestData->QuestShareType == EQuestShareType::Survival)
				{
					bFoundSurvivalQuest = true;
				}

				for (UIQuestBaseTask* IQuestBaseTask : IQuest->GetQuestTasks())
				{
					if (!IQuestBaseTask) continue;

					if (UIQuestKillTask* IQuestKillTask = Cast<UIQuestKillTask>(IQuestBaseTask))
					{
						if (!IQuestKillTask->bIsCritter) continue;

						bHasCritterQuest = true;
						break;
					}
				}

				if (IQuest->QuestData->QuestTag == NAME_HomeCaveTracking)
				{
					FoundHomeCaveTrackingQuest = IQuest;

					// Needed since quest is assigned upon pickup so it can't get the descriptive text till the replication event has happened.
					if (IQuest->QuestData->QuestType == EQuestType::TrophyDelivery)
					{
						ShowQuestDescriptiveText();
					}
				}
				else if (IQuest->QuestData->QuestTag == NAME_WaystoneTracking)
				{
					FoundWaystoneTrackingQuest = IQuest;
				}
			}

			if (bAllQuestsLoaded)
			{
				if (!bFoundSurvivalQuest)
				{
					bHasSurvivalQuest = false;
				}

				if (bHasCritterQuest)
				{
					StartUpdateNearestBurrow();
				}

				if (FoundHomeCaveTrackingQuest)
				{
					StartTrackingHomeCave(FoundHomeCaveTrackingQuest);
				}
				if (FoundWaystoneTrackingQuest)
				{
					StartTrackingWaystone(FoundWaystoneTrackingQuest);
				}

			}

			if (Quest->GetQuestTasks().Num() == 0 || !Quest->GetQuestTasks()[0])
			{
				StartActiveQuestInitializationCheck();
				return;
			}

			UpdateQuestMarker();


		}
	}
#endif
}

void AIBaseCharacter::OnUncollectedRewardQuestDataLoaded(UQuestData* QuestData, UIQuest* Quest)
{
	if (!Quest) return;
	if (!QuestData || !QuestData->IsValidLowLevel()) return;

	Quest->QuestData = QuestData;
}

void AIBaseCharacter::UpdateQuestMarker()
{
	//UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateQuestMarker()"));

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (!IsLocallyControlled()) return;

		AIPlayerController* PlayerController = UIGameplayStatics::GetIPlayerController(this);
		if (PlayerController)
		{
			for (const FMapMarkerInfo& QuestMapMarker : QuestMapMarkers)
			{
				UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(QuestMapMarker, true, true);
			}

			QuestMapMarkers.Empty();
		}
		
		if (GetActiveQuests().Num() == 0) return;
		const TArray<UIQuest*>& QuestsToCheck = GetActiveQuests();

		bool bShouldStartQuestCheckAgain = false;
		StopActiveQuestInitializationCheck();

		for (UIQuest* Quest : QuestsToCheck)
		{
			if (!Quest || !Quest->QuestData || Quest->QuestData->QuestType == EQuestType::Hunting) continue;

			TArray<UIQuestBaseTask*> QuestTasks;

			if (Quest->QuestData->bInOrderQuestTasks)
			{
				QuestTasks.Add(Quest->GetActiveTask());
			}
			else
			{
				QuestTasks = Quest->GetQuestTasks();
			}

			for (UIQuestBaseTask* IQuestBaseTask : QuestTasks)
			{
				if (!IQuestBaseTask) continue;

				if (IQuestBaseTask->IsCompleted()) continue;
			
				if (UIQuestExploreTask* ExploreTask = Cast<UIQuestExploreTask>(IQuestBaseTask)) 
				{
					if (ExploreTask->bIsMoveToQuest && IQuestBaseTask->GetWorldLocation() == FVector(EForceInit::ForceInitToZero))
					{
						IQuestBaseTask->SetWorldLocation(Quest->QuestData->WorldLocation);
					}
				}

				// Wait until the worldlocation and all extramapmarkers are set
				for (const FVector& Location : IQuestBaseTask->GetExtraMapMarkers())
				{
					if (Location == FVector(EForceInit::ForceInitToZero))
					{
						//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::UpdateQuestMarker() - Location == FVector(EForceInit::ForceInitToZero) %s"), *IQuestBaseTask->Tag.ToString()));
						bShouldStartQuestCheckAgain = true;
					}
				}

				if (IQuestBaseTask->GetWorldLocation() == FVector(EForceInit::ForceInitToZero))
				{
					//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::UpdateQuestMarker() - IQuestBaseTask->WorldLocation == FVector(EForceInit::ForceInitToZero) %s"), *IQuestBaseTask->Tag.ToString()));
					bShouldStartQuestCheckAgain = true;
				}
				else
				{
					if (PlayerController)
					{
						FMapMarkerInfo QuestMapMarker = FMapMarkerInfo(nullptr, IQuestBaseTask->GetWorldLocation());
						PlayerController->ClientAddQuestMapMarker(QuestMapMarker, Quest, Quest->QuestData);
						QuestMapMarkers.Add(QuestMapMarker);
					}
				}

				
				for (const FVector& Location : IQuestBaseTask->GetExtraMapMarkers())
				{
					if (Location != FVector(EForceInit::ForceInitToZero))
					{
						if (PlayerController)
						{ 
							FMapMarkerInfo QuestMapMarker = FMapMarkerInfo(nullptr, Location);
							PlayerController->ClientAddQuestMapMarker(QuestMapMarker, Quest, Quest->QuestData);
							QuestMapMarkers.Add(QuestMapMarker);
						}
					}
				}
			}
		}

		if (bShouldStartQuestCheckAgain)
		{
			StartActiveQuestInitializationCheck();
		}
	}
#endif
}

void AIBaseCharacter::ServerForceLocationCheck_Implementation()
{
	//if (LastLocationTag == NAME_None) return;
	//
	//if (AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this))
	//{
	//	if (AIQuestManager* QuestMgr = IWorldSettings->QuestManager)
	//	{
	//		QuestMgr->OnLocationDiscovered(LastLocationTag, LocationDisplayName, this, LastLocationEntered, false);
	//	}
	//}
}

bool AIBaseCharacter::AddLocalQuestFailure(FPrimaryAssetId QuestId)
{
	if (AIPlayerController* IPlayerController = GetController<AIPlayerController>())
	{
		IPlayerController->OnLocalWorldQuestFailureDelayChanged(true);
	}

	bool bAlreadyExists = false;
	for (FQuestCooldown QuestCooldown : LocalQuestFailures)
	{
		if (QuestCooldown.QuestId == QuestId)
		{
			bAlreadyExists = true;
			break;
		}
	}

	if (!bAlreadyExists)
	{
		float CleanupDelay = (UE_BUILD_SHIPPING) ? 300.f : 20.0f;
		LocalQuestFailures.Add(FQuestCooldown(QuestId, GetCharacterID(), CleanupDelay));
	}

	return !bAlreadyExists;
}

bool AIBaseCharacter::RemoveLocalQuestFailure(FPrimaryAssetId QuestId)
{
	if (LocalQuestFailures.Num() == 0) return false;

	bool bAlreadyExists = false;
	for (int i = LocalQuestFailures.Num(); i-- > 0;)
	{
		if (LocalQuestFailures[i].QuestId == QuestId)
		{
			LocalQuestFailures.RemoveAt(i);
			bAlreadyExists = true;
			break;
		}
	}

	if (LocalQuestFailures.Num() == 0)
	{
		if (AIPlayerController* IPlayerController = GetController<AIPlayerController>())
		{
			IPlayerController->OnLocalWorldQuestFailureDelayChanged(false);
		}
	}

	return bAlreadyExists;
}

void AIBaseCharacter::StartActiveQuestInitializationCheck()
{
	if (!GetWorldTimerManager().IsTimerActive(TimerHandle_ActiveQuestInitializationCheck))
	{
		GetWorldTimerManager().SetTimer(TimerHandle_ActiveQuestInitializationCheck, this, &AIBaseCharacter::UpdateQuestMarker, 5.0f, true);
	}
}

void AIBaseCharacter::StopActiveQuestInitializationCheck()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_ActiveQuestInitializationCheck);
}

void AIBaseCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	if (HasAuthority())
	{
		if (GetController<AIPlayerController>())
		{
			if (LastLocationTag != NAME_None)
			{
				if (AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this))
				{
					if (AIQuestManager* QuestMgr = IWorldSettings->QuestManager)
					{
						if (!QuestMgr->HasLocalQuest(this))
						{
							QuestMgr->AssignRandomLocalQuest(this);
						}
					}
				}
			}
		}
	}

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);

		// Clear focus
		if (HasUsableFocus())
		{
			SetFocusedObjectAndItem(nullptr, INDEX_NONE, true);
		}

		if (GetController<AIPlayerController>() == nullptr)
		{
			if (MapIconComponent)
			{
				MapIconComponent->DestroyComponent(false);
				MapIconComponent = nullptr;
			}

			UpdateQuestMarker();

			return;
		}

		if (IsLocallyControlled())
		{
			AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
			check(IWorldSettings);
			if (!IWorldSettings) return;

			const bool bAllowUnderwater = PlayerStartTag != NAME_Flyer && PlayerStartTag != NAME_Land;
			const bool bOnlyUnderwater = PlayerStartTag == NAME_Aquatic;
			
			if (AIWaystoneManager* IWaystoneManager = IWorldSettings->WaystoneManager)
			{
				IWaystoneManager->UpdateWaystoneIcons(bAllowUnderwater, bOnlyUnderwater);
			}

			if (AIHomeCaveManager* IHomeCaveManager = IWorldSettings->HomeCaveManager)
			{
				IHomeCaveManager->SetupMapIcon(bAllowUnderwater, bOnlyUnderwater);
			}
			
			if (!MapIconComponent)
			{
				MapIconComponent = NewObject<UMapIconComponent>(this, TEXT("MapIconComp"));
				MapIconComponent->RegisterComponent();
				MapIconComponent->SetIconRotates(true);
				MapIconComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);

				LoadAndUpdateMapIcon();
			}

			for (UIQuest* IQuest : GetActiveQuests())
			{
				if (!IQuest || !IQuest->QuestData) continue;
				if (IQuest->QuestData->QuestShareType == EQuestShareType::Survival && !bHasSurvivalQuest)
				{
					StartUpdateNearestItem();
					bHasSurvivalQuest = true;
				}
				else if (IQuest->QuestData->QuestTag == NAME_HomeCaveTracking)
				{
					StartTrackingHomeCave(IQuest);
				}
				else if (IQuest->QuestData->QuestTag == NAME_WaystoneTracking)
				{
					StartTrackingWaystone(IQuest);
				}

				for (UIQuestBaseTask* IQuestBaseTask : IQuest->GetQuestTasks())
				{
					if (!IQuestBaseTask) continue;

					if (UIQuestKillTask* IQuestKillTask = Cast<UIQuestKillTask>(IQuestBaseTask))
					{
						if (!IQuestKillTask->bIsCritter) continue;

						bHasCritterQuest = true;
						break;
					}
				}
			}

			if (bHasCritterQuest)
			{
				StartUpdateNearestBurrow();
			}

			AIPlayerController* IPlayerController = GetController<AIPlayerController>();
			check(IPlayerController);

			IPlayerController->UpdateNearbyUsableItems();

			AIGameHUD* IGameHUD = IPlayerController->GetHUD<AIGameHUD>();
			check(IGameHUD);

			ServerForceLocationCheck();

			//UIGameUserSettings* UserSettings = Cast<UIGameUserSettings>(GEngine->GameUserSettings);
			//if (UserSettings && UserSettings->bShowTutorials && IsAlive())
			//{
			//	IGameHUD->ShowTutorialScreen();
			//}

			if (UIMainGameHUDWidget* IMainGameHUDWidget = Cast<UIMainGameHUDWidget>(IGameHUD->ActiveHUDWidget))
			{
				if (UITabMenu* ITabMenu = IMainGameHUDWidget->TabMenu)
				{
					ITabMenu->UpdateOverlayVisibilities();
				}
			}

			HighlightFocusedObject();

			bAutoRunActive = false;
			bDesiresToSprint = false;
			bDesiredUncrouch = false;
			bDesiresPreciseMovement = false;

			UpdateCameraLag();

			UpdateQuestMarker();
		}

		//ShouldBlockOnPreload returns false if it's not world composition or if it's adedicated server
		if (ShouldBlockOnPreload())
		{

			//Requesting level load, even if all the levels are loaded, will cause the load check to return true for at least one frame, 
			//so PotentiallySetPreloadingClientArea will not early out. This is good because it usually takes a frame for world streaming to kick in
			//and it's safe because if it's really loaded then it will just finish the frame after.

			//UE_LOG(LogTemp, Log, TEXT("FALLTHROUGHWORLD: Checking if loading in progress: %d"), IGameInstance->LoadingInProgress());
			IGameInstance->RequestWaitForLevelLoading();

			PotentiallySetPreloadingClientArea();
		}

		if (AIPlayerController* PC = GetController<AIPlayerController>() )
		{
			if (IsValid(AbilitySystem))
			{

				AbilitySystem->InitAbilityActorInfo(PC, this);
				
			}
			
		}
	}
#endif
}

void AIBaseCharacter::UpdateCameraLag()
{
	if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		if (ThirdPersonSpringArmComponent)
		{
			ThirdPersonSpringArmComponent->bEnableCameraLag = bEnableNewCameraSystem && (IGameUserSettings->bCameraSmoothing || IGameUserSettings->bCameraSmoothingVertical);
			ThirdPersonSpringArmComponent->bEnableCameraLagXY = IGameUserSettings->bCameraSmoothing;
			ThirdPersonSpringArmComponent->bEnableCameraLagZ = IGameUserSettings->bCameraSmoothingVertical;
		}
	}
}

void AIBaseCharacter::Restart()
{
	if (IsCombatLogAI()) return;

	Super::Restart();
}

float AIBaseCharacter::GetMaxJawOpenAmountByGrowth(float GrowthPercentage)
{
	return (CurrentMaxJawOpenAmount / GetActorScale3D().Z) * GrowthPercentage;
}

float AIBaseCharacter::GetJawOpenRequirementScaled(AActor* CarriedObject, float DeltaSeconds)
{
	if (!CarriedObject) return 0.0f;

	const float CarriedObjectJawReq = ICarryInterface::Execute_GetCarryData(CarriedObject).JawOpenRequirement;
	const float JawOpenRequirement = CarriedObjectJawReq / CurrentMaxJawOpenAmount;

	return JawOpenRequirement;
}

/** Method that allows an actor to replicate sub objects on its actor channel */
bool AIBaseCharacter::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWrite = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);
	
	for (UIQuest* ActiveQuest : GetActiveQuests())
	{
		if (IsValid(ActiveQuest) && ActiveQuest->IsValidLowLevel() && !ActiveQuest->GetPlayerGroupActor())
		{
			bWrite |= Channel->ReplicateSubobject(ActiveQuest, *Bunch, *RepFlags);
			bWrite |= ActiveQuest->ReplicateSubobjects(Channel, Bunch, RepFlags);
		}
	}

	for (UIQuest* UncollectedRewardQuest : GetUncollectedRewardQuests())
	{
		if (IsValid(UncollectedRewardQuest) && UncollectedRewardQuest->IsValidLowLevel())
		{
			bWrite |= Channel->ReplicateSubobject(UncollectedRewardQuest, *Bunch, *RepFlags);
			bWrite |= UncollectedRewardQuest->ReplicateSubobjects(Channel, Bunch, RepFlags);
		}
	}

	return bWrite;
}

void AIBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	FDoRepLifetimeParams Params{};
	Params.bIsPushBased = true;

	// Replicate to every client
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, RemoteCarriableObject, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bHasSpawnedQuestItem, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CombatLogAlderonId, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bStunned, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, LastTakeHitInfo, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, ReplicatedDamageParticles, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, Stance, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, ActiveQuests, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, PelvisLocation, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bDiedInWater, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, BodyFoodPercentage, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bWaystoneInProgress, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bCombatLogAI, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, AttachTargetInternal, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsBeingDragged, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bLeftHatchlingCave, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bReadyToLeaveHatchlingCave, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, DamageWounds, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, ReplicatedCosmeticParticles, Params);
	// Aim Pitch / Yaw for Simulated Clients - Replay Version, replicate to every client.
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, RemoteDesiredAimPitchReplay, Params);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, RemoteDesiredAimYawReplay, Params);
	
	// Killer Character / Alderon Ids
	FDoRepLifetimeParams SimulatedOnlyParams{};
	SimulatedOnlyParams.bIsPushBased = true;
	SimulatedOnlyParams.Condition = COND_SimulatedOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, KillerCharacterId, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, KillerAlderonId, SimulatedOnlyParams);
	

	// Aim Pitch / Yaw for Simulated Clients - No Replay Version
	FDoRepLifetimeParams SimulatedOnlyNoReplayParams{};
	SimulatedOnlyNoReplayParams.bIsPushBased = true;
	SimulatedOnlyNoReplayParams.Condition = COND_SimulatedOnlyNoReplay;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, RemoteDesiredAimPitch, SimulatedOnlyNoReplayParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, RemoteDesiredAimYaw, SimulatedOnlyNoReplayParams);
	
	// Simulated Only
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsLimping, SimulatedOnlyParams);

	// Replicate to every client except the owner as they are updated locally
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsJumping, SimulatedOnlyParams);

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsNosediving, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsGliding, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CurrentlyUsedObject, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, ReplicatedAccelerationNormal, SimulatedOnlyParams); 

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CurrentMovementType, SimulatedOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CurrentMovementMode, SimulatedOnlyParams); 
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, LaunchVelocity, SimulatedOnlyParams);

	FDoRepLifetimeParams AutonomousOnlyParams{};
	AutonomousOnlyParams.bIsPushBased = true;
	AutonomousOnlyParams.Condition = COND_AutonomousOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, PausedGameplayEffectsList, AutonomousOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsFeignLimping, AutonomousOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, SlottedAbilityCategories, AutonomousOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, SlottedAbilityAssetsArray, AutonomousOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, SlottedActiveAndPassiveEffects, AutonomousOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, UnlockedAbilities, AutonomousOnlyParams);
	//DOREPLIFETIME_CONDITION(AIBaseCharacter, SlottedAbilityIds, COND_AutonomousOnly);

	FDoRepLifetimeParams InitialOnlyParams{};
	InitialOnlyParams.bIsPushBased = true;
	InitialOnlyParams.Condition = COND_InitialOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CharacterName, InitialOnlyParams);

	FDoRepLifetimeParams InitialOrOwnerParams{};
	InitialOrOwnerParams.bIsPushBased = true;
	InitialOrOwnerParams.Condition = COND_InitialOrOwner;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bGender, InitialOrOwnerParams); // -- unused, leftover for mod support just in case. Use IDinosaurCharacter::bGender instead.
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bIsAIControlled, InitialOrOwnerParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CharacterID, InitialOrOwnerParams);
	
	// Owner Only
	FDoRepLifetimeParams OwnerOnlyParams{};
	OwnerOnlyParams.bIsPushBased = true;
	OwnerOnlyParams.Condition = COND_OwnerOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, Marks, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, GE_WellRestedBonus, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, LocationsProgress, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, HomeCaveSaveableInfo, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, CurrentInstance, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, TutorialPrompt, OwnerOnlyParams); 
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bPreloadingClientArea, OwnerOnlyParams); 
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, UnlockedSubspeciesIndexes, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, UncollectedRewardQuests, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bLocalAreaLoading, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bEnableDetachFallDamageCheck, OwnerOnlyParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(AIBaseCharacter, bEnableDetachGroundTraces, OwnerOnlyParams);
}

void AIBaseCharacter::DirtySerializedNetworkFields()
{
#if WITH_PUSH_MODEL
	for (TFieldIterator<FProperty> PropIt(GetClass()); PropIt; ++PropIt)
	{
		const FProperty* const Property = *PropIt;

		if (!Property->HasAnyPropertyFlags(CPF_Net | CPF_SaveGame)) 
		{
			continue;
		}

		// UNetPushModelHelpers::MarkPropertyDirty(this, PropertyName) basically calls "MARK_PROPERTY_DIRTY_UNSAFE" after checking for CPF_Net.
		// Call it ourselves to save performance. -Poncho

		MARK_PROPERTY_DIRTY_UNSAFE(this, Property->RepIndex);
	}
#endif
}

void AIBaseCharacter::ProcessCompletedTraceSet(const FQueuedTraceSet& TraceSet)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::ProcessCompletedTraceSet"))
	SCOPE_CYCLE_COUNTER(STAT_ProcessCompletedTraceSet);

	//START_PERF_TIME();

	check(TraceSet.CharacterPtr.Get() == this);

	DamagedActors.Reset(LastFramesDamagedActors.Num());

	for (int32 i = 0; i < TraceSet.Items.Num(); i++)
	{
		bool bBlocking = ProcessDamageResultsArray(TraceSet.Items[i]);
	}

	for (const TWeakObjectPtr<AActor>& ActPtr : DamagedActors)
	{
		float& HitTime = LastFramesDamagedActors.FindOrAdd(ActPtr);
		// 		HitTime = 0.f;
	}

	//END_PERF_TIME();
	//WARN_PERF_TIME_STATIC(1);
}

bool AIBaseCharacter::ProcessDamageResultsArray(const FQueuedTraceItem& Item)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::ProcessDamageResultsArray"))

	bool bBlockingHit = false;

	for (const FHitResult& Result : Item.OutResults)
	{
		bBlockingHit |= ProcessDamageHitResult(Result, Item);
	}

	return bBlockingHit;
}

FGameplayEventData AIBaseCharacter::GenerateEventData(const FHitResult& Result, const FWeaponDamageConfiguration& DamageConfig)
{
	AIBaseCharacter* HitChar = Cast<AIBaseCharacter>(Result.GetActor());
	FGameplayEventData EventData = FGameplayEventData();
	EventData.EventMagnitude = 1.f;
	EventData.EventTag = DamageConfig.AttackTag;
	EventData.Instigator = this;
	AbilitySystem->GetOwnedGameplayTags(EventData.InstigatorTags);
	EventData.OptionalObject = this;
	EventData.Target = Result.GetActor();
	EventData.TargetData = UAbilitySystemBlueprintLibrary::AbilityTargetDataFromHitResult(Result);

	if (auto TargetInterface = TScriptInterface<IGameplayTagAssetInterface>(HitChar))
	{
		TargetInterface->GetOwnedGameplayTags(EventData.TargetTags);
	}

	FGameplayEffectContextHandle GECHandle = AbilitySystem->MakeEffectContext();
	GECHandle.AddHitResult(Result);
	GECHandle.AddOrigin(Result.Location);
	GECHandle.AddInstigator(this, this);

	FPOTGameplayEffectContext* POTEffectContext = StaticCast<FPOTGameplayEffectContext*>(GECHandle.Get());
	POTEffectContext->SetDamageType(EDamageType::DT_ATTACK);
	POTEffectContext->SetEventMagnitude(1.0f);
	POTEffectContext->SetDirection(-Result.Normal);
	POTEffectContext->SetKnockbackMode(DamageConfig.KnockbackMode);
	POTEffectContext->SetKnockbackForce(DamageConfig.KnockbackForce);

	POTEffectContext->SetChargedDuration(DamageConfig.ChargedDuration);
	POTEffectContext->SetMovementSpeed(AbilitySystem->GetTrackedSpeed());
	POTEffectContext->SetSmoothMovementSpeed(AbilitySystem->GetSmoothedTrackedSpeed());

	EventData.ContextHandle = GECHandle;
	return EventData;
}

bool AIBaseCharacter::ProcessDamageHitResult(FHitResult Result, const FQueuedTraceItem& Item)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::ProcessDamageHitResult"));

	if (Result.MyBoneName.IsNone()) // if the hit result does not add the instigator's bone, add it to use for spikes damage.
	{
		Result.MyBoneName = Item.ShapeName;
	}

	AActor* HitActor = Result.GetActor();
	UPrimitiveComponent* ResultComponent = Result.GetComponent();

	if (HitActor == nullptr || ResultComponent == nullptr || ResultComponent->GetCollisionResponseToChannel(ECC_WeaponTrace) == ECR_Ignore) return false;

	AIBaseCharacter* HitChar = Cast<AIBaseCharacter>(HitActor);
	if (HitChar)
	{
		//UE_LOG(LogTemp, Log, TEXT("Processing damage between %s and %s"), *GetName(), *HitChar->GetName());
	}

	bool bIsFriendly = !AbilitySystem->IsHostile(HitActor) && !AbilitySystem->IsNeutral(HitActor);

	if (!Item.bFriendlyFire && bIsFriendly)
	{
		return false;
	}

	bool bHitLastFrame = LastFramesDamagedActors.Contains(HitActor);
	if (bHitLastFrame || DamagedActors.Contains(HitActor))
	{
		//Mark this actor is already hit and skip it.
		if (bHitLastFrame)
		{
			DamagedActors.AddUnique(HitActor);
		}

		//UE_LOG(LogTemp, Log, TEXT("Hit Last Frame || Damaged Already"));
		return false;
	}

	bool bBlockingHit = false;

	//We need this check in case we overlap 2 components of the same actor at once
	//(which shouldn't happen)
	if (!DamagedActors.Contains(HitActor))
	{
		bool bBlockingBodyHit = false;
		bBlockingHit = Result.bBlockingHit;
		bool bHitActorIsBlocking = false;
		bool bIsHostile = AbilitySystem->IsHostile(HitActor);

		if (HitChar != nullptr)
		{
			if (!HitChar->AbilitySystem->IsAlive())
			{
				return false;
			}

			//bHitActorIsBlocking = TargetChar->HasMatchingGameplayTag(UWaAbilitySystemGlobals::Get().BlockBuffTag);
			//bBlockingBodyHit = TargetChar->IsBlockingBody(Result.BoneName);
		}

		ReportAINoise(Result.Location);

		FGameplayEventData EventData;
		EventData = GenerateEventData(Result, CurrentDamageConfiguration);
		
		bool bSomethingInterferingWithHit = false;
		UWorld* World = GetWorld();
		if (CheckTraceType != ECheckTraceType::CTT_NONE && World != nullptr && Result.GetActor()->Implements<UAbilitySystemInterface>())
		{
			FVector OwnerPoint = GetActorLocation();
			FVector TargetPoint = Result.GetActor()->GetActorLocation();

			if (CheckTraceType == ECheckTraceType::CTT_BOUNDSCENTER)
			{
				OwnerPoint = GetMesh()->Bounds.Origin;

				TArray<USkeletalMeshComponent*> SkelComps;
				Result.GetActor()->GetComponents<USkeletalMeshComponent>(SkelComps);
				for (auto* PotentialComponent : SkelComps)
				{
					if (PotentialComponent)
					{
						TargetPoint = PotentialComponent->Bounds.Origin;
						break;
					}
				}
			}

			FCollisionQueryParams TraceParams;
			TraceParams.AddIgnoredActors({ this, TWeakObjectPtr<AActor>(HitActor) });
			bool bTest = false;
			FCollisionObjectQueryParams COQP(ObjectTypesToQuery);
#if WITH_EDITOR
			FHitResult HitResult1;
			FHitResult HitResult2;
			bSomethingInterferingWithHit = World->LineTraceSingleByObjectType(HitResult1, Result.ImpactPoint, OwnerPoint, COQP, TraceParams);
			bTest = World->LineTraceSingleByObjectType(HitResult2, Result.TraceStart, TargetPoint, COQP, TraceParams);
#else
			bSomethingInterferingWithHit = World->LineTraceTestByObjectType(Result.ImpactPoint, OwnerPoint, COQP, TraceParams);
			bTest = World->LineTraceTestByObjectType(Result.TraceStart, TargetPoint, COQP, TraceParams);
#endif

#if WITH_EDITOR
			UE_LOG(LogTemp, Log, TEXT("Interfere: %s %s"), (bSomethingInterferingWithHit ? TEXT("TRUE") : TEXT("FALSE")), (bTest ? TEXT("TRUE") : TEXT("FALSE")))
				bSomethingInterferingWithHit = bSomethingInterferingWithHit || bTest;
			if (bTest == true)
			{
				//UE_LOG(LogTemp, Log, TEXT("Interferes With: %s"), *HitResult2.Actor->GetName())
			}
#endif

		}
		else if (IsLocallyControlled())
		{
			if (AICritterPawn* CritterActor = Cast<AICritterPawn>(HitActor))
			{
				if (!CritterActor->IsDead() && abs((GetActorLocation() - CritterActor->GetActorLocation()).Size()) <= 5000)
				{
					ServerTryDamageCritter(EventData, CritterActor);
				}
			}
			else if (AIFish* FishActor = Cast<AIFish>(HitActor))
			{
				if (ObjectExistsAndImplementsCarriable(HitActor) && ShouldSkipInteractAnimation(HitActor))
				{
					UPOTGameplayAbility* CurrentAttack = AbilitySystem->GetCurrentAttackAbility();
					//If this attack is allowed to catch carriables (most likely fish) then try to carry it.
					// Otherwise, just kill the fish and use existing logic to make it float to the surface.
					if (CurrentAttack->bCanCatchCarriable) 
					{
						if (TryCarriableObject(HitActor, true))
						{
							// replicate if successful
							if (GetLocalRole() != ROLE_Authority)
							{
								ServerCarryObject(HitActor, true);
							}
						}
					}
					else
					{
						ServerKillFish(FishActor);
					}
				}
			}
		}

		if (!bSomethingInterferingWithHit)
		{
			ProcessDamageEvent(EventData, CurrentDamageConfiguration);
		}

		DamagedActors.AddUnique(HitActor);

		if (CurrentDamageConfiguration.bDisableBodyOnHit)
		{
			TArray<FBodyShapes> Shapes;
			//GetDamageBodiesForSlotName(Item.SlotName, Shapes);
			for (FBodyShapes& BShapes : Shapes)
			{
				if (Result.MyBoneName == BShapes.Name)
				{
					BShapes.bEnabled = false;
					break;
				}
			}
		}

#if DEBUG_WEAPONS
		if (CVarDebugSweeps->GetInt() > 0)
		{
			DrawDebugPoint(GetWorld(), Result.ImpactPoint, 10.f, FColor::Green, false, 5.f);
		}
#endif
	}

	return bBlockingHit;
}

void AIBaseCharacter::ServerKillFish_Implementation(AIFish* TargetFish)
{
	if (!TargetFish)
	{
		return;
	}
	if (TargetFish->IsDead())
	{
		return;
	}
	if (!IsFishInKillDistance(TargetFish))
	{
		return;
	}
	TargetFish->KillFish(this);
	FVector3d VecStart = TargetFish->GetActorTransform().GetLocation();
	FVector3d VecEnd = FVector(VecStart.X, VecStart.Y, VecStart.Z - (IsFlying() ? 250000 : 5000));
	FTransform PotentialDropLocation = GetDropTransform(VecStart, VecEnd, TargetFish, false);
	ICarryInterface::Execute_Dropped(TargetFish, this, FTransform(PotentialDropLocation.GetRotation(), PotentialDropLocation.GetLocation(), TargetFish->GetActorScale3D()));
}

void AIBaseCharacter::ProcessDamageEvent(const FGameplayEventData& EventData, const FWeaponDamageConfiguration& DamageConfig)
{

	FScopedPredictionWindow NewScopedWindow(AbilitySystem, true);
	AbilitySystem->HandleGameplayEvent(DamageConfig.AttackTag, &EventData);

	if (HasAuthority() && GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		Client_ProcessEvent(EventData);
	}

	if (!DamageConfig.bCanSendRevengeEvents)
	{
		return;
	}

	// Send an event to the target as well, so revenge abilities can be triggered.
	AIBaseCharacter* const HitChar = Cast<AIBaseCharacter>(const_cast<AActor*>(EventData.Target.Get()));
	
	if (!HitChar)
	{
		return;
	}

	const FGameplayTag TargetEventTag = FGameplayTag::RequestGameplayTag(NAME_EventMontageSharedGetHit);
	FGameplayEventData RevengeEventData = EventData;
	RevengeEventData.EventTag = TargetEventTag;

	if (HitChar->AbilitySystem)
	{
		HitChar->AbilitySystem->HandleGameplayEvent(TargetEventTag, &RevengeEventData);
	}

	if (HasAuthority() && GetNetMode() == ENetMode::NM_DedicatedServer)
	{
		HitChar->Client_ProcessEvent(RevengeEventData);
	}
	
}

void AIBaseCharacter::Client_ProcessEvent_Implementation(const FGameplayEventData& EventData)
{
	AbilitySystem->HandleGameplayEvent(EventData.EventTag, &EventData);
}

void AIBaseCharacter::FilterDamageBodies(const TArray<FName>& EnabledBodies)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::FilterDamageBodies"))

	// UE_LOG(LogTemp, Error, TEXT("%s: FilterDamageBodies -> Weapon 554"), *WeaponOwnerWaCharacter->GetName());
	for (auto& KVP : DamageShapeMap)
	{
		for (auto& Body : KVP.Value.DamageShapes)
		{
			Body.bEnabled = EnabledBodies.Contains(Body.Name);
		}
	}
}

void AIBaseCharacter::StartDamage(const FWeaponDamageConfiguration& DamageConfig)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::StartDamage"))

	if (DamageConfig.BodyFilter.Num() > 0)
	{
		FilterDamageBodies(DamageConfig.BodyFilter);
	}

	if (UPOTGameplayAbility* CurrentAttackingAbility = AbilitySystem->GetCurrentAttackAbility())
	{
		EnableDamage(DamageConfig);
	}
}

void AIBaseCharacter::SetAllDamageBodiesEnabled(const bool bEnabled)
{
	for (auto& KVP : DamageShapeMap)
	{
		for (auto& Body : KVP.Value.DamageShapes)
		{
			Body.bEnabled = bEnabled;
		}
	}
}

void AIBaseCharacter::StopDamageAndClearDamagedActors()
{
	DisableDamage(true, true);
}

void AIBaseCharacter::ClearDamagedActors()
{
	DamagedActors.Empty();
	LastFramesDamagedActors.Empty();
}

void AIBaseCharacter::EnableDamage(const FWeaponDamageConfiguration& DamageConfiguration)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::EnableDamage"))

	if (bDoingDamage)
	{
		DisableDamage();
		//return;
	}

	UPOTGameplayAbility* CurrentAttackAbility = AbilitySystem->GetCurrentAttackAbility();
	if (CurrentAttackAbility == nullptr)
	{
		return;
	}

	float Scale = GetMesh()->GetComponentTransform().GetMaximumAxisScale();
	if (!FMath::IsNearlyEqual(Scale, CachedBodyOwnerScale, 0.05f))
	{
		InitializeCombatData();
	}

	CurrentTraceGroup++;
	TracePointIndex = 0;
	LastHitTime = 0.f;
	DamageSweepsCounter = 0;

	CurrentAttackAbility->GetTimedTraceTransforms(CurrentSectionBones);
	if (CurrentSectionBones.Num() > 0)
	{
		if (CurrentSectionBones[0].LocalBones.Num() > 0)
		{
			checkf(CurrentSectionBones[0].LocalBones.Num() > 0,
				TEXT("My name: %s Attacking ability: %s"),
				*GetName(),
				(AbilitySystem->GetCurrentAttackAbility()) ?
				(*AbilitySystem->GetCurrentAttackAbility()->GetName()) :
				*FString("")
			);
		}
	}

	ClearDamagedActors();

	CurrentDamageConfiguration = DamageConfiguration;

	bDoingDamage = true;

}

void AIBaseCharacter::DisableDamage(const bool bDoFinalTrace /*= true*/, const bool bResetBodyFilter /*= true*/)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::DisableDamage"))

	//START_PERF_TIME();

	if (!bDoingDamage)
	{
		return;
	}

	if (bDoFinalTrace)
	{
		TracePointIndex++;
		DoDamageSweeps(0.0f);
	}

	if (bResetBodyFilter)
	{
		SetAllDamageBodiesEnabled(true);
	}

	bDoingDamage = false;
	TracePointIndex = 0;
	DamageSweepsCounter = 0;

	//END_PERF_TIME();
	//WARN_PERF_TIME_STATIC(1);
}


void AIBaseCharacter::ReportAINoise(const FVector& Location)
{
	UAISense_Hearing::ReportNoiseEvent(
		GetWorld(),
		Location,
		AINoiseSpec.Loudness,
		this,
		AINoiseSpec.MaxRange,
		AINoiseSpec.Tag
	);
}

bool AIBaseCharacter::GetDamageBodiesForMesh_Implementation(const USkeletalMeshComponent* InMesh, TArray<FBodyShapes>& OutDamageBodies)
{
	if (DamageShapeMap.Num() == 0)
	{
		InitializeCombatData();
	}

	if (const FBodyShapeSet* Set = DamageShapeMap.Find(InMesh))
	{
		OutDamageBodies = Set->DamageShapes;
		return true;
	}

	return false;
}

FName AIBaseCharacter::GetNearestPounceAttachSocket(const FHitResult& HitResult)
{
	AIBaseCharacter* PounceTarget = Cast<AIBaseCharacter>(HitResult.GetActor());

	if (!IsValid(PounceTarget) || !PounceTarget->IsAlive())
	{
		return NAME_None;
	}
	
	FName NearestSocketName = NAME_None;
	double NearestSocketDistSq = DBL_MAX;
	
	FVector Location = GetActorLocation();

	if (USkeletalMeshComponent* PounceTargetMesh = PounceTarget->GetMesh())
	{
		for (const FName& SocketName : PounceTarget->PounceAttachSockets)
		{
			if (PounceTargetMesh->DoesSocketExist(SocketName))
			{
				FVector SocketLocation = PounceTargetMesh->GetSocketLocation(SocketName);
				double SocketDistSq = FVector::DistSquared(SocketLocation, HitResult.ImpactPoint);
				//double SocketDistSq = FVector::DistSquared(SocketLocation, Location);

				if (SocketDistSq < NearestSocketDistSq)
				{
					NearestSocketName = SocketName;
					NearestSocketDistSq = SocketDistSq;
				}
			}
		}
	}

	return NearestSocketName;
}

void AIBaseCharacter::InitializeCombatData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::InitializeCombatData"))

	CachedBodyOwnerScale = GetMesh()->GetComponentTransform().GetMaximumAxisScale();

	DamageShapeMap.Empty();
	TArray<FBodyShapes> DamageShapes;
	TMultiMap<FName, int32> BodyNameLookupMap;

	GetBodies(DamageBodies, GetMesh(), DamageShapes, BodyNameLookupMap, CachedBodyOwnerScale);
	DamageShapeMap.Emplace(GetMesh(), FBodyShapeSet(DamageShapes, BodyNameLookupMap));

	static const FName WeaponTraceName(TEXT("WeaponDamageTrace"));
	WeaponTraceParams = FCollisionQueryParams(WeaponTraceName, bSweepComplex);
	WeaponTraceParams.bTraceComplex = bSweepComplex;
	WeaponTraceParams.bReturnPhysicalMaterial = true;

	WeaponTraceParams.AddIgnoredActor(this);
}

int32 AIBaseCharacter::GetBodies(const TMap<FName, FDamageBodies>& Bodies, const USkeletalMeshComponent* DamageComp, TArray<FBodyShapes>& OutShapes, TMultiMap<FName, int32>& OutBodyNameLookupMap, const float CharOwnerScale)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::GetBodies"))

	const static float ERORR_TOLERANCE = 0.01;

	auto ActualMethod = [&, this]() -> int32
	{
		int32 TotalCount = 0;
		TArray<USkeletalBodySetup*> SkeletalBodySetups;
		if (DamageComp->SkeletalMesh != nullptr && DamageComp->SkeletalMesh->GetPhysicsAsset() != nullptr)
		{
			SkeletalBodySetups = DamageComp->SkeletalMesh->GetPhysicsAsset()->SkeletalBodySetups;
		}

		for (const TPair<FName, FDamageBodies>& BoneShapes : Bodies)
		{
			FBodyInstance* Inst = DamageComp->GetBodyInstance(BoneShapes.Key);
			if (Inst == nullptr)
			{
				continue;
			}

			TArray<FPhysicsShapeHandle, FDefaultAllocator> InstShapes;
			TArray<FCollisionShape> InstUShapes;

			FPhysicsCommand::ExecuteRead(Inst->GetActorReferenceWithWelding(), [&](const FPhysicsActorHandle& Actor)
			{
				//Body->GetUnrealWorldTransform_AssumesLocked(true);
				Inst->GetAllShapes_AssumesLocked(InstShapes);
			});

			// This is probably useless
			TotalCount += InstShapes.Num();
			InstUShapes.Reserve(InstShapes.Num());

			const FTransform& SocketTransform = DamageComp->GetSocketTransform(BoneShapes.Key, RTS_Component);
			TArray<FTransform> ShapeTransforms;
			TArray<FTransform> ShapeLocalTransforms;

			USkeletalBodySetup* CorrectBodySetup = nullptr;
			for (USkeletalBodySetup* SBS : SkeletalBodySetups)
			{
				if (SBS->BoneName == BoneShapes.Key)
				{
					CorrectBodySetup = SBS;
					break;
				}
			}

			TArray<FPhysicsShapeHandle, FDefaultAllocator> InstShapesToUse;
			for (FPhysicsShapeHandle& PXS : InstShapes)
			{
				FTransform ShapeLocalTransform = GetShapeLocalTransform(PXS, true, CharOwnerScale);

				FCollisionShape UShape;
				if (!WaPShape2UCollision(UShape, PXS))
				{
					checkf(false, TEXT("Unable to initialze weapon."));
				}

				bool bIsThisDamageBody = false;

				if (CorrectBodySetup != nullptr)
				{
					FPhysicsGeometryCollection GeoCollection = FPhysicsInterface::GetGeometryCollection(PXS);

					if (UShape.IsCapsule())
					{
						const Chaos::TCapsule<float>& CapsuleGeometry = GeoCollection.GetCapsuleGeometry();

						if (CorrectBodySetup->AggGeom.SphylElems.Num() == 1)
						{
							bIsThisDamageBody = true;
						}
						else
						{
							for (const FKSphylElem& Caps : CorrectBodySetup->AggGeom.SphylElems)
							{
								if (FMath::IsNearlyEqual((float)(Caps.Radius * CharOwnerScale), (float)CapsuleGeometry.GetRadius(), ERORR_TOLERANCE) &&
									FMath::IsNearlyEqual((float)(Caps.Length * 0.5f * CharOwnerScale), (float)CapsuleGeometry.GetHeight() / 2, ERORR_TOLERANCE))
								{
									if (BoneShapes.Value.GeomNames.Contains(Caps.GetName()))
									{
										bIsThisDamageBody = true;
										break;
									}
								}
							}
						}

					}
					else if (UShape.IsSphere())
					{
						const Chaos::TSphere<Chaos::FReal, 3>& SphereGeometry = GeoCollection.GetSphereGeometry();

						if (CorrectBodySetup->AggGeom.SphereElems.Num() == 1)
						{
							bIsThisDamageBody = true;
						}
						else
						{
							for (const FKSphereElem& Sphere : CorrectBodySetup->AggGeom.SphereElems)
							{
								if (FMath::IsNearlyEqual((float)(Sphere.Radius * CharOwnerScale), (float)SphereGeometry.GetRadius(), ERORR_TOLERANCE))
								{
									if (BoneShapes.Value.GeomNames.Contains(Sphere.GetName()))
									{
										bIsThisDamageBody = true;
										break;
									}
								}
							}
						}
					}
					else if (UShape.IsBox())
					{
						const Chaos::TBox<Chaos::FReal, 3>& BoxGeometry = GeoCollection.GetBoxGeometry();

						if (CorrectBodySetup->AggGeom.BoxElems.Num() == 1)
						{
							bIsThisDamageBody = true;
						}
						else
						{
							for (const FKBoxElem& Box : CorrectBodySetup->AggGeom.BoxElems)
							{
								FVector HalfExtents = BoxGeometry.Extents() / 2;
								
								if (FMath::IsNearlyEqual((float)Box.X * 0.5f * CharOwnerScale, (float)(HalfExtents.X), ERORR_TOLERANCE) &&
									FMath::IsNearlyEqual((float)Box.Y * 0.5f * CharOwnerScale, (float)(HalfExtents.Y), ERORR_TOLERANCE) &&
										FMath::IsNearlyEqual((float)Box.Z * 0.5f * CharOwnerScale, (float)(HalfExtents.Z), ERORR_TOLERANCE))
								{
									if (BoneShapes.Value.GeomNames.Contains(Box.GetName()))
									{
										bIsThisDamageBody = true;
										break;
									}
								}
							}
						}
					}
				}

				if (bIsThisDamageBody)
				{
					InstUShapes.Emplace(UShape);
					InstShapesToUse.Emplace(PXS);
					ShapeLocalTransforms.Emplace(ShapeLocalTransform);
					ShapeTransforms.Emplace(ShapeLocalTransform * SocketTransform);
				}
			}

			int32 ShapeIndex = OutShapes.Add(FBodyShapes(BoneShapes.Key, InstShapesToUse, InstUShapes, ShapeTransforms,
				ShapeLocalTransforms, DamageComp));
			OutBodyNameLookupMap.Emplace(BoneShapes.Key, ShapeIndex);
		}

		return TotalCount;
	};

	return ActualMethod();
}

void AIBaseCharacter::DoDamageSweeps(const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::DoDamageSweeps"))

	SCOPE_CYCLE_COUNTER(STAT_DoDamageSweeps);

	UPOTGameplayAbility* CurrentAttackAbility = AbilitySystem->GetCurrentAttackAbility();
	if (CurrentAttackAbility == nullptr)
	{
		//UE_LOG(LogTemp, Error, TEXT("AIBaseCharacter::DoDamageSweeps - CurrentAttackAbility == nullptr"));
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::DoDamageSweeps - CurrentAttackAbility == nullptr")));
		return;
	}

	const float CurrentMontageTime = AbilitySystem->GetMontageTimeInCurrentMove();
	if (CurrentMontageTime == 0.0f) return;

	//if (!ensureMsgf(CurrentSectionBones.Num() > 0, TEXT("CurrentSectionBones empty")))
	//{
		CurrentAttackAbility->GetTimedTraceTransforms(CurrentSectionBones);
	//}

	if (CurrentSectionBones.Num() == 0)
	{
		CurrentAttackAbility->GatherWeaponTraceData(nullptr);
	}

	QueueShapeTraces(GetWorld(), CurrentSectionBones, WeaponTraceParams, CurrentMontageTime, DeltaTime, true);
}

bool AIBaseCharacter::QueueShapeTraces(UWorld* World, const TArray<FTimedTraceBoneGroup> SectionBones, const FCollisionQueryParams& TraceParams, const float CurrentMontageTime, const float DeltaTime, bool bStopOnBlock /*= true*/)
{
	//GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Green, FString::Printf(TEXT("AIBaseCharacter::QueueShapeTraces")));
	//UE_LOG(LogTemp, Log, TEXT("AIBaseCharacter::QueueShapeTraces"));
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::QueueShapeTraces"))

	if (SectionBones.Num() == 0) return false;

	FQueuedTraceSet TraceSet(this);
	TraceSet.MontageTime = CurrentMontageTime;

	FTimedTraceBoneGroup ClosestTraceBoneGroup;
	FTimedTraceBoneGroup NextTraceBoneGroup;

	for (int32 i = 0; i < SectionBones.Num(); i++)
	{
		if (SectionBones[i].MontageTime <= CurrentMontageTime)
		{
			ClosestTraceBoneGroup = SectionBones[i];
		}
		else
		{
			NextTraceBoneGroup = SectionBones[i];
			break;
		}
	}

	GEngine->AddOnScreenDebugMessage(-1, DeltaTime, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::QueueShapeTraces | CurrentMontageTime %f - SectionBonesFirst: %f - SectionBonesLast: %f"), CurrentMontageTime, SectionBones[0].MontageTime, SectionBones[SectionBones.Num() - 1].MontageTime));

	TArray<FBodyShapes> Shapes;
	GetDamageBodiesForMesh(GetMesh(), Shapes);

	TArray<FAlderonBoneData, TInlineAllocator<16>> NextBoneData;

	for (int32 j = 0; j < Shapes.Num(); j++)
	{
		if (!Shapes[j].bEnabled || !AbilitySystem->IsAlive()) continue;
		if (!ClosestTraceBoneGroup.LocalBones.Contains(Shapes[j].Name) || !NextTraceBoneGroup.LocalBones.Contains(Shapes[j].Name)) continue;

		for (int32 k = 0; k < Shapes[j].Shapes.Num(); k++)
		{
			const FTransform& ShapeLocalTransform = Shapes[j].ShapeLocalTransforms[k];

			FTransform StartTraceTransform = ShapeLocalTransform;
			StartTraceTransform *= GetMesh()->GetSocketTransform(Shapes[j].Name, ERelativeTransformSpace::RTS_World);

			if (PreviousBoneData.Num() > 0)
			{
				for (FAlderonBoneData BoneData : PreviousBoneData)
				{
					if (BoneData.BoneName == Shapes[j].Name)
					{
						StartTraceTransform = BoneData.BoneTransform;
						break;
					}
				}
			}

			FTransform EndTraceTransform = ShapeLocalTransform;
			EndTraceTransform *= GetMesh()->GetSocketTransform(Shapes[j].Name, ERelativeTransformSpace::RTS_World);

			NextBoneData.Add(FAlderonBoneData(Shapes[j].Name, EndTraceTransform));

			UPOTGameplayAbility* WGA = AbilitySystem->GetCurrentAttackAbility();

			TraceSet.AddTrace(Shapes[j].Name,
				StartTraceTransform.GetLocation(),
				EndTraceTransform.GetLocation(),
				EndTraceTransform.GetRotation(),
				Shapes[j].UShapes[k],
				TraceParams,
				bStopOnBlock,
				0,
				NextTraceBoneGroup.MontageTime - CurrentMontageTime,
				CurrentDamageConfiguration.TraceChannel,
				WGA != nullptr ? WGA->bFriendlyFire : false);

#if DEBUG_WEAPONS
			DebugTraceItem(TraceSet.Items[TraceSet.Items.Num() - 1]);
#endif
		}
	}

	if (TraceSet.Items.Num() > 0)
	{
		QueueTraces(TraceSet);
	}

	PreviousBoneData = NextBoneData;

	return TraceSet.Items.Num() > 0;
}

void AIBaseCharacter::QueueTraces(const FQueuedTraceSet& QueueSet)
{
	FWeaponTraceWorker::Get()->QueueTraces(QueueSet);
}

void AIBaseCharacter::UpdateOverlaps(float DeltaTime)
{
	if (bDoingOverlapChecks && !IsStunned() && !IsHomecaveBuffActive())
	{
#if UE_SERVER
		if (IsRunningDedicatedServer())
		{
			if (bCanAttackOnServer)
			{
				CheckOverlaps(DeltaTime);
			}
		}
#else
		CheckOverlaps(DeltaTime);
#endif
	}
}

void AIBaseCharacter::StartOverlapChecks()
{
	if (bDoingOverlapChecks)
	{
		StopOverlapChecks();
	}

	if (!AbilitySystem->GetCurrentAttackAbility())
	{
		return;
	}

	bDoingOverlapChecks = true;
}

void AIBaseCharacter::StopOverlapChecks()
{
	PreviouslyOverlappedTargetActors.Empty();
	bDoingOverlapChecks = false;
}

void AIBaseCharacter::CheckOverlaps(float DeltaTime)
{
	if (!AbilitySystem)
	{
		return;
	}

	//Get Current Montage Time
	const float CurrentMontageTime = AbilitySystem->GetMontageTimeInCurrentMove();
	if (CurrentMontageTime <= 0.0f)
	{
		return;
	}

	UPOTGameplayAbility* const CurrentAbility = AbilitySystem->GetCurrentAttackAbility();
	if (!CurrentAbility)
	{
		return;
	}

	//Loop through each timed config to check for new triggers
	for(FTimedOverlapConfiguration &Config : CurrentAbility->TimedOverlapConfigurations)
	{
		if (Config.TriggerNextTimestamp(CurrentMontageTime))
		{
			TArray<FHitResult> HitResults{};
			TArray<AActor*> TargetActors{};
			const UPOTTargetType* TargetTypeCDO = Config.AnimOverlapConfiguration.AOETargetType.GetDefaultObject();
			if (!TargetTypeCDO)
			{
				continue;
			}

			TargetTypeCDO->GetTargets(this, FGameplayEventData(), HitResults, TargetActors);

			//Check if a new actor has been hit and send event information about it.
			for (const FHitResult& HitResult : HitResults)
			{
				if (PreviouslyOverlappedTargetActors.Contains(HitResult.GetActor()))
				{
					continue;
				}

				PreviouslyOverlappedTargetActors.Add(HitResult.GetActor());
				const FGameplayEventData EventData = GenerateEventData(HitResult, Config.AnimOverlapConfiguration.DamageConfiguration);
				ProcessDamageEvent(EventData, Config.AnimOverlapConfiguration.DamageConfiguration);
			}
		}
	}
}

#if DEBUG_WEAPONS

void AIBaseCharacter::DebugTraceItem(const FQueuedTraceItem& Item)
{
	FQuat AverageRotation = Item.Rotation;

	if (CVarDebugSweeps->GetInt() > 0)
	{
		// 		UE_LOG(WaWeapon, Log, TEXT("WEAWPONAA: bDoPredicionTraces = %s"), (bPredicted ? TEXT("TRUE") : TEXT("FALSE")));
		if (Item.Shape.IsBox())
		{
			DrawDebugBox(GetWorld(), Item.End, Item.Shape.GetBox(), AverageRotation, FColor::Red, false, 1.f);
		}
		else if (Item.Shape.IsCapsule())
		{
			DrawDebugCapsule(GetWorld(), Item.End, Item.Shape.GetCapsuleHalfHeight(), Item.Shape.GetCapsuleRadius(), AverageRotation, FColor::Red, false, 1.f);
		}
		else if (Item.Shape.IsSphere())
		{
			DrawDebugSphere(GetWorld(), Item.End, Item.Shape.GetSphereRadius(), 32, FColor::Red, false, 1.f);
		}

		DrawDebugLine(GetWorld(), Item.Start, Item.End, FColor::Yellow, false, 1.f);
	}
}
#endif

bool AIBaseCharacter::WaPShape2UCollision(FCollisionShape& OutShape, const FPhysicsShapeHandle& ShapeHandle) const
{
	if (!ShapeHandle.IsValid())
	{
		return false;
	}

	ECollisionShapeType GeometryType = FPhysicsInterface::GetShapeType(ShapeHandle);
	Chaos::FPerShapeData* PXS = ShapeHandle.Shape;
	if (PXS == nullptr)
	{
		return false;
	}

	FPhysicsGeometryCollection GeoCollection = FPhysicsInterface::GetGeometryCollection(ShapeHandle);

	if (GeometryType == ECollisionShapeType::Sphere)
	{
		const Chaos::TSphere<Chaos::FReal, 3>& Geometry = GeoCollection.GetSphereGeometry();
		OutShape = FCollisionShape::MakeSphere(Geometry.GetRadius());
		return true;
	}
	else if (GeometryType == ECollisionShapeType::Box)
	{
		const Chaos::TBox<Chaos::FReal, 3>& Geometry = GeoCollection.GetBoxGeometry();
		FVector HalfExtent = FVector(Geometry.Extents()) / 2;
		OutShape = FCollisionShape::MakeBox(HalfExtent);
		return true;
	}
	else if (GeometryType == ECollisionShapeType::Capsule)
	{
		const Chaos::TCapsule<Chaos::FReal>& Geometry = GeoCollection.GetCapsuleGeometry();
		OutShape = FCollisionShape::MakeCapsule(Geometry.GetRadius(), Geometry.GetHeight() / 2 + Geometry.GetRadius());
		return true;
	}

	return false;
}

FTransform AIBaseCharacter::GetShapeLocalTransform(FPhysicsShapeHandle& PXS, const bool bConvertCapsuleRotation /*= true*/, const float OwnerCharComponentScale /*= 1.f*/)
{
	FTransform LocalTransform = FPhysicsInterface::GetLocalTransform(PXS);

	if (OwnerCharComponentScale != 1.f)
	{
		LocalTransform.ScaleTranslation(1 / OwnerCharComponentScale);
	}

	FTransform BakedTransform = FTransform::Identity;
	FPhysicsGeometryCollection GeoCollection = FPhysicsInterface::GetGeometryCollection(PXS);

	if (GeoCollection.GetType() == ECollisionShapeType::Capsule)
	{
		const Chaos::TCapsule<Chaos::FReal>& CapsuleGeometry = GeoCollection.GetCapsuleGeometry();
		BakedTransform.SetLocation(CapsuleGeometry.GetCenter());
		BakedTransform.SetRotation(CapsuleGeometry.GetRotationOfMass());
	}
	else if (GeoCollection.GetType() == ECollisionShapeType::Sphere)
	{
		const Chaos::TSphere<Chaos::FReal, 3>& SphereGeometry = GeoCollection.GetSphereGeometry();
		BakedTransform.SetLocation(SphereGeometry.GetCenter());
	}
	else if (GeoCollection.GetType() == ECollisionShapeType::Box)
	{
		const Chaos::TBox<Chaos::FReal, 3>& BoxGeometry = GeoCollection.GetBoxGeometry();
		BakedTransform.SetLocation(BoxGeometry.GetCenter());
		BakedTransform.SetRotation(BoxGeometry.GetRotationOfMass());	
	}

	return BakedTransform * LocalTransform;
}

FText AIBaseCharacter::GetInteractionName_Implementation()
{
	return InteractionPromptData.InteractionName;
}

TSoftObjectPtr<UTexture2D> AIBaseCharacter::GetInteractionImage_Implementation()
{
	return InteractionPromptData.InteractionImage;
}

FText AIBaseCharacter::GetPrimaryInteraction_Implementation(class AIBaseCharacter* User)
{
	return InteractionPromptData.PrimaryInteraction;
}

FText AIBaseCharacter::GetCarryInteraction_Implementation(class AIBaseCharacter* User)
{
	return CanTakeQuestItem(User) ? TrophyInteractionName : InteractionPromptData.CarryInteraction;
}

FText AIBaseCharacter::GetNoInteractionReason_Implementation(class AIBaseCharacter* User)
{
	if (!User->ContainsFoodTag(NAME_Meat) || User->IsUsingAbility())
	{
		return InteractionPromptData.InteractionFailureReasonOne;
	}

	if (!User->IsHungry())
	{
		return InteractionPromptData.InteractionFailureReasonTwo;
	}

	return InteractionPromptData.InteractionFailureReasonFallback;
}

int32 AIBaseCharacter::GetInteractionPercentage_Implementation()
{
	return ((GetCurrentBodyFood() / GetMaxBodyFood()) * 100.0f);
}

bool AIBaseCharacter::GetGodmode() const
{
	return !CanBeDamaged();
}

void AIBaseCharacter::SetIsNosediving(bool bNewIsNosediving)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bIsNosediving, bNewIsNosediving, this);
}

void AIBaseCharacter::SetIsGliding(bool bNewIsGliding)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bIsGliding, bNewIsGliding, this);
}

void AIBaseCharacter::SetCurrentMovementType(ECustomMovementType NewCurrentMovementType)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, CurrentMovementType, NewCurrentMovementType, this);
}

void AIBaseCharacter::SetCurrentMovementMode(ECustomMovementMode NewCurrentMovementMode)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, CurrentMovementMode, NewCurrentMovementMode, this);
}

bool AIBaseCharacter::IsNosediving()
{
	if (IsFlying()) // Old replays can have broken nosedive/glide data.
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);
		if (IGameInstance && IGameInstance->IsWatchingReplay() && IGameInstance->bIsOldReplay)
		{
			return GetActorRotation().Vector().GetSafeNormal().Z < -0.3f;
		}
	}

	return IsNosedivingRaw();
}
bool AIBaseCharacter::IsGliding()
{
	if (IsFlying()) // Old replays can have broken nosedive/glide data.
	{
		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);
		if (IGameInstance && IGameInstance->IsWatchingReplay() && IGameInstance->bIsOldReplay)
		{
			return IsNosedivingRaw();
		}
	}
	return IsGlidingRaw();
}


void AIBaseCharacter::SetGodmode(bool bEnabled)
{
	if (AbilitySystem != nullptr)
	{
		UPOTAbilitySystemGlobals& PASG = UPOTAbilitySystemGlobals::Get();
		if (bEnabled)
		{
			AbilitySystem->AddLooseGameplayTag(PASG.GodmodeTag);
		}
		else
		{
			AbilitySystem->RemoveLooseGameplayTag(PASG.GodmodeTag, 500);
		}
	}
	SetCanBeDamaged(!bEnabled);
}

bool AIBaseCharacter::IsExhausted() const
{
	if (AbilitySystem != nullptr)
	{
		UPOTAbilitySystemGlobals& PASG = UPOTAbilitySystemGlobals::Get();
		return AbilitySystem->HasMatchingGameplayTag(PASG.StaminaExhaustionTag);
	}
	return false;
}

FVector AIBaseCharacter::GetSafeTeleportLocation(FVector InLocation, bool& FoundSafeLocation)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::GetSafeTeleportLocation"))

	FVector FromLocation = InLocation;
	FVector ToLocation = InLocation - FVector{ 0, 0, 100000 };

	TArray<FHitResult> HitResults = UIGameplayStatics::LineTraceAllByChannel(GetWorld(), FromLocation, ToLocation, COLLISION_DINOCAPSULE);
	if (HitResults.Num() == 0) // cast from the player pos if there is ground underneath otherwise cast from way up high 
	{
		FromLocation += FVector{ 0, 0, 100000 };
		HitResults = UIGameplayStatics::LineTraceAllByChannel(GetWorld(), FromLocation, ToLocation, COLLISION_DINOCAPSULE);
	}

	FVector TraceDirection = (ToLocation - FromLocation).GetSafeNormal();
	FVector UnsafeLocation = FVector::ZeroVector;

	FoundSafeLocation = false;
	for (FHitResult& Hit : HitResults)
	{
		if (Cast<ABlockingVolume>(Hit.GetActor())) continue;
		if (Hit.bBlockingHit && (Cast<ALandscapeProxy>(Hit.GetActor()) || Cast<AStaticMeshActor>(Hit.GetActor()) || Cast<UStaticMeshComponent>(Hit.GetComponent())))
		{
			UnsafeLocation = Hit.ImpactPoint;
			FoundSafeLocation = true;
			break;
		}
	}
	if (!FoundSafeLocation)
	{
		return InLocation;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		UnsafeLocation += FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
	}
	FVector JiggleLocation = UnsafeLocation;
	if (!GetWorld()->FindTeleportSpot(this, JiggleLocation, this->GetActorRotation()))
	{
		for (int i = 0; i < 10; i++)
		{
			// Jiggle location in all directions to find safe spot
			JiggleLocation = UnsafeLocation;
			JiggleLocation += FVector{ 100.f * i, 0, 0 }; // right
			if (GetWorld()->FindTeleportSpot(this, JiggleLocation, this->GetActorRotation())) break;
			JiggleLocation += FVector{ -200.f * i, 0, 0 }; // left
			if (GetWorld()->FindTeleportSpot(this, JiggleLocation, this->GetActorRotation())) break;
			JiggleLocation += FVector{ 100.f * i, 100.f * i, 0 }; // forward
			if (GetWorld()->FindTeleportSpot(this, JiggleLocation, this->GetActorRotation())) break;
			JiggleLocation += FVector{ 0, -200.f * i, 0 }; // backward
			if (GetWorld()->FindTeleportSpot(this, JiggleLocation, this->GetActorRotation())) break;
		}
	}

	FHitResult HitResult;
	FCollisionQueryParams CollisionQueryParams;
	CollisionQueryParams.AddIgnoredActor(this);
	GetWorld()->LineTraceSingleByChannel(HitResult, JiggleLocation + FVector(0, 0, 500), JiggleLocation - FVector(0, 0, 10000), COLLISION_DINOCAPSULE, CollisionQueryParams);
	if (HitResult.bBlockingHit)
	{
		JiggleLocation = HitResult.ImpactPoint;
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			// Need to add the half height again after line trace
			JiggleLocation += FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
		}
	}

	FoundSafeLocation = true;
	return JiggleLocation;
}

bool AIBaseCharacter::InputToggleNameTags()
{
	if (UIAdminPanelMain::StaticAdminPanelActive(this))
	{
		// Don't want to use the shortcut key when the admin panel is active
		return false;
	}

	const AIPlayerController* const IPlayerController = Cast<AIPlayerController>(GetController());
	if (!IPlayerController) 
	{
		return false;
	}

	bool bAllowed = false;

	if (AIChatCommandManager::Get(this)->CheckAdmin(IPlayerController)) 
	{
		bAllowed = true; // Allow shortcut if admin
	}
	else
	{
		const AIPlayerState* const IPlayerState = GetPlayerState<AIPlayerState>();

		if (!IPlayerState) 
		{
			return false;
		}
		if (IPlayerState->GetPlayerRole().bAssigned && IPlayerState->GetPlayerRole().bSpectatorAccess) 
		{
			bAllowed = true; // Allow shortcut if role allows spectator
		}
	}

	if (!bAllowed) 
	{
		return false;
	}

	AIGameHUD* const IGameHUD = IPlayerController->GetHUD<AIGameHUD>();
	if (!IGameHUD)
	{
		return false;
	}

	IGameHUD->ToggleNameTags();

	return true;	
}

void AIBaseCharacter::ServerBetterTeleport(const FVector& DestLocation, const FRotator& DestRotation, bool bAddHalfHeight, bool bForce)
{
	// A better teleport function that helps prevent client movement mismatch
	// and accounts for character half height

	FVector HalfHeightLocation = DestLocation;
	if (bAddHalfHeight)
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			HalfHeightLocation += FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
		}
	}

	if (UCharacterMovementComponent* CharacterMovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent()))
	{
		CharacterMovementComponent->StopMovementImmediately();
	}

	TeleportTo(HalfHeightLocation, DestRotation, false, bForce);
	ClientBetterTeleport(HalfHeightLocation, DestRotation, bForce);

}

void AIBaseCharacter::ClientBetterTeleport_Implementation(const FVector& DestLocation, const FRotator& DestRotation, bool bForce)
{
	if (UCharacterMovementComponent* CharacterMovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent()))
	{
		CharacterMovementComponent->StopMovementImmediately(); 
	}

	// Set BetterTeleportTransform so that the rotation gets updated properly 
	// when the character has fully loaded in.
	BetterTeleportTransform = FTransform(DestRotation, DestLocation);
	SetActorLocationAndRotation(DestLocation, DestRotation, false, nullptr, ETeleportType::TeleportPhysics);

	// In some situations there is no loading so we can also set a short timer to 
	// correct the rotation. This won't be noticed because the loading screen
	// will be active.
	FTimerDelegate TimerDel;
	TimerDel.BindUObject(this, &AIBaseCharacter::EnsureTeleportedRotation, DestRotation);
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, TimerDel, .3f, false);

}

void AIBaseCharacter::EnsureTeleportedRotation(FRotator DestRotation)
{
	SetActorRotation(DestRotation);
	if (AController* ThisController = GetController())
	{
		ThisController->SetControlRotation(FRotator::MakeFromEuler(FVector(0,-20,GetActorRotation().Yaw)));
	}
}

bool AIBaseCharacter::TeleportTo(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest, bool bNoCheck)
{
	StopAllInteractions();
	ClientCancelUseObject(GetWorld()->TimeSeconds, true);

	if (HasAuthority() && !IsLocallyControlled() && IsNetMode(NM_ListenServer))
	{
		if (UCharacterMovementComponent* CharacterMovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent()))
		{
			CharacterMovementComponent->StopMovementImmediately();
		}
		if (ULevelStreaming* LevelStreaming = UIGameplayStatics::GetLevelForLocation(this, DestLocation))
		{
			if (LevelStreaming->GetLevelStreamingState() != ELevelStreamingState::LoadedVisible && !GetCurrentInstance())
			{
				// if the level is not visible we need to make it visible for collision to be
				// handled before teleporting. Delay the teleport until the level is loaded
				AILevelStreamingLoader* StreamingLoader = GetWorld()->SpawnActor<AILevelStreamingLoader>(DestLocation, DestRotation);
				check (StreamingLoader);
				if (StreamingLoader)
				{				
					AILevelStreamingLoader::FLevelStreamingLoaderComplete OnComplete;
					OnComplete.BindUObject(this, &AIBaseCharacter::TryTeleportTo, DestLocation, DestRotation);
					StreamingLoader->Setup(DestLocation, OnComplete);
					return true;
				}
			}
		}
	}

	if (HasAuthority())
	{
		if (!IsLocallyControlled())
		{
			if (UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
			{
				CharMove->GetPredictionData_Server_ICharacter()->bForceNextClientAdjustment = true;
			}
		}
		LastTeleportTime = GetWorld()->GetTimeSeconds();
		SetPreloadingClientArea(true);

		UnlatchAllDinosAndSelf();
	}

	// When teleporting, POI overlapps do not fire in a predicatble/correct order. We use bIsTeleporting to detect this in POI Begin/End overlaps to correct for this.
	TGuardValue<bool>(bIsTeleporting, true);
	return Super::TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
}

void AIBaseCharacter::TryTeleportTo(FVector Location, FRotator Rotation)
{
	TeleportTo(Location, Rotation);
}

void AIBaseCharacter::SetDesiredAimRotation(FRotator NewAimRotation)
{
	DesiredAimRotation = NewAimRotation;
}

void AIBaseCharacter::ShowQuestDescriptiveText()
{
	UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
	if (!IGameUserSettings || !IGameUserSettings->bShowQuestDescriptivePrompt) return;

	UObject* ObjectToUse = FocusedObject.Object.Get();

	if (!ObjectToUse) return;

	FText QuestItemName;
	FName QuestItemTag;
	TSoftObjectPtr<UTexture2D> InteractionImage;

	if (AIBaseItem* IBaseItem = Cast<AIBaseItem>(ObjectToUse))
	{
		QuestItemName = IBaseItem->InteractionPromptData.InteractionName;
		QuestItemTag = IBaseItem->QuestTag;
		InteractionImage = IBaseItem->InteractionPromptData.InteractionImage;
	}
	else if (AIFish* IFish = Cast<AIFish>(ObjectToUse))
	{
		QuestItemName = IFish->InteractionPromptData.InteractionName;
		QuestItemTag = IFish->QuestTag;
		InteractionImage = IFish->InteractionPromptData.InteractionImage;
	}

	if (QuestItemTag == NAME_None) return;

	FText QuestDescriptiveText;
	for (UIQuest* ActiveQuest: GetActiveQuests())
	{
		if (!ActiveQuest) continue;

		for(UIQuestBaseTask* IQuestBaseTask: ActiveQuest->GetQuestTasks())
		{
			UIQuestItemTask* QuestItemTask = Cast<UIQuestItemTask>(IQuestBaseTask);
			if (!QuestItemTask) continue;

			if (QuestItemTask->GetItemDescriptiveText(QuestItemTag, QuestDescriptiveText))
			{
				FQuestDescriptiveTextCooldown QuestDescriptiveCooldown = FQuestDescriptiveTextCooldown(QuestItemTask, GetWorld()->TimeSeconds);
				// Make Sure there isn't an active cooldown
				if (IsQuestDescriptiveCooldownActive(QuestDescriptiveCooldown)) return;

				// Mark as shown for 5 minutes
				QuestDescriptiveCooldowns.Add(QuestDescriptiveCooldown);
				AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
				if (!IGameHUD) return;

				IGameHUD->ShowQuestDescriptiveText(QuestItemName, QuestDescriptiveText, InteractionImage);
				return;
			}
		}
	}
}

bool AIBaseCharacter::IsQuestDescriptiveCooldownActive(FQuestDescriptiveTextCooldown QuestDescriptiveCooldown)
{
	for (FQuestDescriptiveTextCooldown QDTC: QuestDescriptiveCooldowns)
	{
		if (QDTC == QuestDescriptiveCooldown) return true;
	}

	return false;
}

void AIBaseCharacter::UpdateQuestsFromTeleport()
{
	if (HasAuthority())
	{
		if (AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>())
		{
			if (IPlayerState->GetPlayerGroupActor() && IPlayerState->GetPlayerGroupActor()->GetGroupLeader() == IPlayerState)
			{
				IPlayerState->GetPlayerGroupActor()->FailGroupQuest();
			}

			if (AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this))
			{
				if (AIQuestManager* QuestMgr = IWorldSettings->QuestManager)
				{
					TArray<UIQuest*, TInlineAllocator<5>> ActiveLocalWorldQuests;
					TArray<UIQuest*, TInlineAllocator<5>> ActiveGroupQuests;

					for (UIQuest* ActiveQuest : GetActiveQuests())
					{
						if (!ActiveQuest || !ActiveQuest->QuestData) continue;

						if (ActiveQuest->QuestData->QuestShareType == EQuestShareType::LocalWorld)
						{
							ActiveLocalWorldQuests.Add(ActiveQuest);
						}
						else if (ActiveQuest->QuestData->QuestShareType == EQuestShareType::Group)
						{
							ActiveGroupQuests.Add(ActiveQuest);
						}
					}

					if (ActiveLocalWorldQuests.Num() > 0)
					{
						for (UIQuest* ActiveQuest : ActiveLocalWorldQuests)
						{
							QuestMgr->OnQuestFail(this, ActiveQuest, true);
						}
					}

					if (AIPlayerGroupActor* IPGA = IPlayerState->GetPlayerGroupActor())
					{
						if (IPGA->GetGroupLeader() == IPlayerState)
						{
							if (ActiveGroupQuests.Num() > 0)
							{
								for (UIQuest* ActiveQuest : ActiveGroupQuests)
								{
									QuestMgr->OnQuestFail(this, ActiveQuest, true);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool AIBaseCharacter::TeleportToFromCommand(const FVector& DestLocation, const FRotator& DestRotation, bool bIsATest /*= false*/, bool bNoCheck /*= false*/)
{
	// Fail area specific quests before leaving
	UpdateQuestsFromTeleport();

	return TeleportTo(DestLocation, DestRotation, bIsATest, bNoCheck);
}

void AIBaseCharacter::RefreshInstanceHiddenActors()
{
	// This runs just on client side so that we can hide things like height fog when in instance

	bool Hide = false;
	if (GetCurrentInstance())
	{
		Hide = true;
	}
	if (AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this))
	{
		for (AActor* Actor : IWorldSettings->ActorsToHideInInstance)
		{
			if (Actor)
			{
				Actor->SetActorHiddenInGame(Hide);
			}
		}
	}
}

void AIBaseCharacter::OnRep_IsBeingDragged()
{
	if (IsBeingDragged())
	{
		if (UCharacterMovementComponent* CharacterMovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent()))
		{
			CharacterMovementComponent->StopMovementImmediately();
			CharacterMovementComponent->DisableMovement();
			CharacterMovementComponent->SetComponentTickEnabled(false);
		}

		if (IsLocallyControlled())
		{
			if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
			{
				IGameHUD->QueueToast(DraggedToastIcon, FText::FromStringTable(TEXT("ST_CreatorMode"), TEXT("Admin")), FText::FromStringTable(TEXT("ST_CreatorMode"), TEXT("YouAreBeingDragged")));
			}
		}
	}
	else
	{
		if (UCharacterMovementComponent* CharacterMovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent()))
		{
			CharacterMovementComponent->SetMovementMode(MOVE_Falling);
			CharacterMovementComponent->SetComponentTickEnabled(true);
		}
	}
}

bool AIBaseCharacter::ToggleMap()
{
	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	if (!IGameHUD)
	{
		return false;
	}

	AIGameState* IGameState = Cast<AIGameState>(GetWorld()->GetGameState());
	if (!IGameState)
	{
		return false;
	}

	if (IGameHUD->bMapWindowActive)
	{
		IGameHUD->HideMapMenu();
		return true;
	}

	if (IGameHUD->bVocalWindowActive)
	{
		IGameHUD->HideVocalRadialMenu();
		return true;
	}

	if (IGameState->GetGameStateFlags().bAllowMap)
	{
		if (IGameHUD->bTabSystemActive)
		{
			IGameHUD->ToggleTab();
		}
		IGameHUD->ShowMapMenu();
		return true;
	}
	
	return false;
}

bool AIBaseCharacter::InputQuests()
{
	ToggleQuestWindow();
	return true;
}

void AIBaseCharacter::ToggleQuestWindow()
{
	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		IGameHUD->ToggleQuestWindow();
	}
}

void AIBaseCharacter::ReInitializePhysicsConstraints()
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UIGameplayStatics::ReInitializePhysicsConstraints(GetMesh(), FOnConstraintBroken::CreateUObject(this, &AIBaseCharacter::OnConstraintBrokenWrapperCopy));
	}
	
#endif
}

void AIBaseCharacter::OnConstraintBrokenWrapperCopy(int32 ConstraintIndex)
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->OnConstraintBroken.Broadcast(ConstraintIndex);
	}
}

void AIBaseCharacter::ApplyGrowthPenaltyNextSpawn(float GrowthPenaltyPercent, bool bSubtract /*= false*/, bool bOverrideLoseGrowthPastGrowthStages /*= false*/, bool bUpdateWellRestedGrowth /*= false*/)
{
	if (!AbilitySystem || GrowthPenaltyPercent <= 0.0f || !UIGameplayStatics::IsGrowthEnabled(this))
	{
		return;
	}

	const UIGameInstance* const IGameInstance = Cast<UIGameInstance>(GetGameInstance());
	if (!IGameInstance) return;

	const AIGameSession* const Session = Cast<AIGameSession>(IGameInstance->GetGameSession());
	if (!Session) return;

	float CurrentGrowth = GetGrowthPercent();

	// If the player is a hatchling, don't apply the penalty
	// bLoseGrowthPastGrowthStages is handled below
	if (!Session->bLoseGrowthPastGrowthStages && CurrentGrowth < HatchlingGrowthPercent)
	{
		return;
	}

	float NewGrowth = CurrentGrowth;
	if (Session->bLoseGrowthPastGrowthStages || bOverrideLoseGrowthPastGrowthStages)
	{
		if (bSubtract)
		{
			NewGrowth = CurrentGrowth - ((float)GrowthPenaltyPercent / 100.f);
		}
		else
		{
			float GrowthMultiplier = FMath::Clamp(1.f - ((float)GrowthPenaltyPercent / 100.f), 0.f, 1.f);
			NewGrowth = CurrentGrowth * GrowthMultiplier;
		}
	}
	else
	{
		float StageMinGrowth = FMath::Min(FMath::Floor(CurrentGrowth * 4.f) * 0.25f, 0.75f);

		if (bSubtract)
		{
			NewGrowth = FMath::Max(StageMinGrowth, CurrentGrowth - ((float)GrowthPenaltyPercent / 100.f));
		}
		else
		{
			float GrowthMultiplier = FMath::Clamp((float)GrowthPenaltyPercent / 100.f, 0.f, 1.f);
			NewGrowth = FMath::Max(StageMinGrowth, CurrentGrowth - (GrowthMultiplier * 0.25f));
		}
	}

	// Get min growth based on bLoseGrowthPastGrowthStages
	float MinGrowthAfterDeath = Session->bLoseGrowthPastGrowthStages ? Session->GetMinGrowthAfterDeath() : FMath::Max(Session->GetMinGrowthAfterDeath(), HatchlingGrowthPercent);

	// Clamp min growth if the tutorial cave is turned on.
	MinGrowthAfterDeath = Session->bServerHatchlingCaves ? FMath::Max(MinGrowthAfterDeath, Session->HatchlingCaveExitGrowth) : MinGrowthAfterDeath;

	NewGrowth = FMath::Clamp(NewGrowth, FMath::Max(MIN_GROWTH, MinGrowthAfterDeath), 1.0f);

	if (bUpdateWellRestedGrowth && GetGE_WellRestedBonus().IsValid())
	{
		float GrowthLoss = CurrentGrowth - NewGrowth;
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusStartedGrowthAttribute(), GetWellRestedBonusStartedGrowth() - GrowthLoss, EGameplayModOp::Override);
		UPOTAbilitySystemGlobals::ApplyDynamicGameplayEffect(this, UCoreAttributeSet::GetWellRestedBonusEndGrowthAttribute(), GetWellRestedBonusEndGrowth() - GrowthLoss, EGameplayModOp::Override);
	}

	CalculatedGrowthForRespawn = NewGrowth;
}

bool AIBaseCharacter::IsInCombat() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TEXT("AIBaseCharacter::IsInCombat"))

	if (!AbilitySystem) return false;

	FGameplayEffectQuery Query = FGameplayEffectQuery();
	Query.EffectDefinition = UPOTAbilitySystemGlobals::Get().InCombatEffect;

	TArray<FActiveGameplayEffectHandle> CombatHandles = AbilitySystem->GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : CombatHandles)
	{
		if (Handle.IsValid())
		{
			if (AbilitySystem->GetCurrentStackCount(Handle) > 0)
			{
				return true;
				break;
			}
		}
	}

	return false;
}

bool AIBaseCharacter::IsHomecaveBuffActive() const
{
	if (!AbilitySystem) return false;

	FGameplayEffectQuery Query = FGameplayEffectQuery();
	Query.EffectDefinition = UPOTAbilitySystemGlobals::Get().HomecaveExitBuffEffect;

	TArray<FActiveGameplayEffectHandle> HomecaveExitBuffHandles = AbilitySystem->GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : HomecaveExitBuffHandles)
	{
		if (Handle.IsValid())
		{
			if (AbilitySystem->GetCurrentStackCount(Handle) > 0)
			{
				return true;
				break;
			}
		}
	}

	return false;
}

bool AIBaseCharacter::IsHomecaveCampingDebuffActive() const
{
	if (!AbilitySystem) return false;

	FGameplayEffectQuery Query = FGameplayEffectQuery();
	Query.EffectDefinition = UPOTAbilitySystemGlobals::Get().HomecaveCampingDebuffEffect;

	TArray<FActiveGameplayEffectHandle> HomecaveCampingDebuffHandles = AbilitySystem->GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : HomecaveCampingDebuffHandles)
	{
		if (Handle.IsValid())
		{
			if (AbilitySystem->GetCurrentStackCount(Handle) > 0)
			{
				return true;
				break;
			}
		}
	}

	return false;
}

void AIBaseCharacter::OnRep_HomeCaveSaveableInfo()
{
	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		if (UIHomeCaveUnlockWidget* IHomeCaveUnlockWidget = Cast<UIHomeCaveUnlockWidget>(IGameHUD->GetWindowOfClass(UIHomeCaveUnlockWidget::StaticClass())))
		{
			IHomeCaveUnlockWidget->SetButtonsEnabled(true);
		}
		if (UIHomeCaveDecorPurchaseWidget* PurchaseWidget = Cast<UIHomeCaveDecorPurchaseWidget>(IGameHUD->GetWindowOfClass(UIHomeCaveDecorPurchaseWidget::StaticClass())))
		{
			PurchaseWidget->Refresh();
		}
		if (UIHomeCaveDecorPlacementWidget* PlacementWidget = Cast<UIHomeCaveDecorPlacementWidget>(IGameHUD->GetWindowOfClass(UIHomeCaveDecorPlacementWidget::StaticClass())))
		{
			PlacementWidget->Refresh();
		}
		if (UIHomeCaveCatalogueWidget* CatalogueWidget = Cast<UIHomeCaveCatalogueWidget>(IGameHUD->GetWindowOfClass(UIHomeCaveCatalogueWidget::StaticClass())))
		{
			CatalogueWidget->Refresh();
		}
	}
}

FInstanceLogoutSaveableInfo* AIBaseCharacter::GetInstanceLogoutInfo(const FString& LevelName)
{
	for (FInstanceLogoutSaveableInfo& LogoutInfo : InstanceLogoutInfo)
	{
		if (LogoutInfo.LoggedLevel.Equals(LevelName))
		{

			return &LogoutInfo;
		}
	}


	return nullptr;
}

bool AIBaseCharacter::LoggedOutInInstance(const FString& LevelName)
{
	if (FInstanceLogoutSaveableInfo* LogoutInfo = GetInstanceLogoutInfo(LevelName))
	{
		return true;
	}
	return false;
}

bool AIBaseCharacter::LoggedOutInOwnedInstance(const FString& LevelName)
{
	if (FInstanceLogoutSaveableInfo* LogoutInfo = GetInstanceLogoutInfo(LevelName))
	{

		if (LogoutInfo->Owned)
		{
			return true;
		}
	}

	return false;
}

void AIBaseCharacter::RemoveInstanceLogoutInfo(const FString& LevelName)
{
	for (int Index = InstanceLogoutInfo.Num() - 1; Index >= 0; Index--)
	{
		if (InstanceLogoutInfo[Index].LoggedLevel.Equals(LevelName))
		{
			//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, FString(TEXT("Removed ")) + InstanceLogoutInfo[Index].LoggedLevel);
			InstanceLogoutInfo.RemoveAt(Index);

		}
	}
}

void AIBaseCharacter::ServerStartHomeCaveDecoration_Implementation(UHomeCaveDecorationDataAsset* DecorationDataAsset)
{
	if (!GetCurrentInstance()) return;
	if (GetCurrentInstance()->GetOwner() != this) return;

	FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
	AssetLoader.RequestAsyncLoad
	(
		HomeCaveDecoratorComponentClassPtr.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::ServerStartHomeCaveDecorationStage1, DecorationDataAsset),
		FStreamableManager::AsyncLoadHighPriority, false
	);
}

void AIBaseCharacter::ServerStartHomeCaveDecorationStage1(UHomeCaveDecorationDataAsset* DecorationDataAsset)
{
	UClass* DecoratorComponentClass = HomeCaveDecoratorComponentClassPtr.Get();
	check(DecoratorComponentClass);
	if (!DecoratorComponentClass) return;

	UIHomeCaveDecoratorComponent* IHomeCaveDecoratorComponent = Cast<UIHomeCaveDecoratorComponent>(GetComponentByClass(DecoratorComponentClass));
	if (IHomeCaveDecoratorComponent) return;

	IHomeCaveDecoratorComponent = Cast<UIHomeCaveDecoratorComponent>(AddComponentByClass(DecoratorComponentClass, false, FTransform(), false));

	check(IHomeCaveDecoratorComponent);
	if (IHomeCaveDecoratorComponent)
	{
		IHomeCaveDecoratorComponent->Setup(DecorationDataAsset);
	}
}

void AIBaseCharacter::ClientFinishHomeCaveDecoration_Implementation()
{
	if (UIHomeCaveDecoratorComponent* HomeCaveDecoratorComp = Cast<UIHomeCaveDecoratorComponent>(GetComponentByClass(UIHomeCaveDecoratorComponent::StaticClass())))
	{
		HomeCaveDecoratorComp->Finish();
	}
	// If any Home cave widget is open, close it
	if (AIHUD* IHUD = UIGameplayStatics::GetIHUD(this))
	{
		// If either the first home cave widget (UIHomeCaveCustomiserWidget)
		// or the decoration widget is open then we can close all windows
		if (IHUD->GetWindowOfClass(UIHomeCaveCustomiserWidget::StaticClass()) ||
			IHUD->GetWindowOfClass(UIHomeCaveDecorationWidget::StaticClass()))
		{
			IHUD->CloseAllWindows();
		}
	}
}

void AIBaseCharacter::OnRep_Marks(int32 OldMarks)
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		if (IsLocallyControlled())
		{
			UICommonMarks::UpdateMarks(GetMarks(), this);

			if (OldMarks < GetMarks())
			{
				CheckForPurchasableSkin();
				CheckForPurchasableAbility();
			}
		}
	}
#endif
}

void AIBaseCharacter::SetMarks(int NewMarks)
{
	if (GetMarks() != NewMarks)
	{
		const int32 OldMarks = GetMarks();
		Marks = NewMarks;

		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, Marks, this);

		// Cache Marks in Player State to show on player list
		AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
		if (IPlayerState)
		{
			IPlayerState->SetMarksTemp(GetMarks());
		}

		if (HasAuthority() && IsLocallyControlled())
		{
			OnRep_Marks(OldMarks);
		}
	}
}

bool AIBaseCharacter::IsValidatedCheck()
{
	AIPlayerController* IPlayerController = Cast<AIPlayerController>(GetController());
	if (!IPlayerController) return false;

	return IPlayerController->IsValidatedCheck();
}

AIPlayerCaveBase* AIBaseCharacter::GetCurrentInstance() const
{
	return NativeGetCurrentInstance();
}	

void AIBaseCharacter::SetCurrentInstance(AIPlayerCaveBase* Instance)
{
	CurrentInstance = Instance;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, CurrentInstance, this);

	if (AbilitySystem != nullptr)
	{
		UPOTAbilitySystemGlobals& PASG = UPOTAbilitySystemGlobals::Get();
		// If we are in an instance we can apply FriendlyInstanceTag which prevents
		// receiving any damage
		if (!Instance)
		{
			AbilitySystem->RemoveLooseGameplayTag(PASG.FriendlyInstanceTag);
		}
		AbilitySystem->CancelAllActiveAbilities();
	}
	if (AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>())
	{
		IPlayerState->SetCachedCurrentInstance(Instance);
	}	
	if (HasAuthority() && IsLocallyControlled())
	{
		OnRep_CurrentInstance();
	}
}

void AIBaseCharacter::OnRep_CurrentInstance()
{
	if (!IsRunningDedicatedServer() && IsLocallyControlled())
	{
		
		if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
		{
			if (UIMainGameHUDWidget* IMainGameHUDWidget = Cast<UIMainGameHUDWidget>(IGameHUD->ActiveHUDWidget))
			{
				IMainGameHUDWidget->RefreshMiniMapVisibility();
			}
		}

		RefreshMapIcon();
		RefreshInstanceHiddenActors();

	}
}

void AIBaseCharacter::ClientFinishInstanceTeleport_Implementation()
{
	
}

void AIBaseCharacter::ExitInstanceFromDeath(const FTransform& OverrideReturnTransform)
{
	check(HasAuthority());
	if (!HasAuthority()) 
	{
		UE_LOG(LogTemp, Log, TEXT("Exit Instance From Death does not have authority"));
		return;
	}

	AIPlayerCaveBase* Instance = GetCurrentInstance();
	if (!Instance) 
	{
		UE_LOG(LogTemp, Log, TEXT("Exit Instance From Death does not have an Instance"));
		return;
	}

	SetCurrentInstance(nullptr);

	Instance->RemovePlayer(this, true);

	RemoveInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));

	InstanceId = FPrimaryAssetId();

	UIGameplayStatics::GetIGameModeChecked(this)->SaveCharacter(this, ESavePriority::High);

	ClientExitInstance();
}

void AIBaseCharacter::ExitInstance(const FTransform& OverrideReturnTransform)
{
	check(HasAuthority());
	if (!HasAuthority())
	{
		UE_LOG(LogTemp, Log, TEXT("Exit Instance does not have authority"));
		return;
	}

	AIPlayerCaveBase* Instance = GetCurrentInstance();
	if (!Instance) 
	{
		UE_LOG(LogTemp, Log, TEXT("Exit Instance does not have an Instance"));
		InstanceId = FPrimaryAssetId();
		return;
	}

	if (IsWaystoneInProgress())
	{
		CancelWaystoneInviteCharging();
	}

	SetCurrentInstance(nullptr);

	FInstanceLogoutSaveableInfo* InstanceLogoutSaveableInfo = GetInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));
	check(InstanceLogoutSaveableInfo);
	if (InstanceLogoutSaveableInfo)
	{
		FVector RetLoc = OverrideReturnTransform.GetLocation().IsZero() ? InstanceLogoutSaveableInfo->CaveReturnLocation : OverrideReturnTransform.GetLocation();
		FRotator RetRot = OverrideReturnTransform.GetLocation().IsZero() ? FRotator(0, InstanceLogoutSaveableInfo->CaveReturnYaw, 0) : OverrideReturnTransform.GetRotation().Rotator();


		if (AIWorldSettings* IWorldSettings = AIWorldSettings::Get(this))
		{
			if (!IWorldSettings->IsInWorldBounds(RetLoc))
			{
				UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ExitInstance: Return location out of bounds, resetting"));
				// If leaving a cave and we are outside of the world bounds, reset the return location
				FTransform SpawnTransform = UIGameplayStatics::GetIGameModeChecked(this)->FindRandomSpawnPointForCharacter(this, Cast<AIPlayerState>(GetPlayerState()));

				RetLoc = SpawnTransform.GetLocation();
			}
		}


		ServerBetterTeleport(RetLoc, RetRot, false);
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Tried to exit instance with no logout save information"));

		if (!FallbackCaveReturnPosition.Equals(FVector::ZeroVector, 0.01f))
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ExitInstance(): Tried to exit instance with no logout save information, using fallback cave return"));

			FVector RetLoc = FallbackCaveReturnPosition;
			FRotator RetRot = FallbackCaveReturnRotation;
			RetRot.Roll = 0;

			if (AIWorldSettings* IWorldSettings = AIWorldSettings::Get(this))
			{
				if (!IWorldSettings->IsInWorldBounds(RetLoc))
				{
					UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ExitInstance: Return location out of bounds, resetting"));
					// If leaving a cave and we are outside of the world bounds, reset the return location
					FTransform SpawnTransform = UIGameplayStatics::GetIGameModeChecked(this)->FindRandomSpawnPointForCharacter(this, Cast<AIPlayerState>(GetPlayerState()));

					RetLoc = SpawnTransform.GetLocation();
				}
			}
			// If there is no logout save information then use the fallback cave location if it exists
			ServerBetterTeleport(RetLoc, RetRot, false);
		}
		else if (!SaveCharacterPosition.Equals(FVector::ZeroVector, 0.01f))
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ExitInstance(): Tried to exit instance with no logout save information, using save character position"));

			FVector RetLoc = SaveCharacterPosition;
			FRotator RetRot = SaveCharacterRotation;

			if (AIWorldSettings* IWorldSettings = AIWorldSettings::Get(this))
			{
				if (!IWorldSettings->IsInWorldBounds(RetLoc))
				{
					UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ExitInstance: Return location out of bounds, resetting"));
					// If leaving a cave and we are outside of the world bounds, reset the return location
					FTransform SpawnTransform = UIGameplayStatics::GetIGameModeChecked(this)->FindRandomSpawnPointForCharacter(this, Cast<AIPlayerState>(GetPlayerState()));

					RetLoc = SpawnTransform.GetLocation();
				}
			}

			// If there is no fallback cave information then use the save character position
			ServerBetterTeleport(RetLoc, RetRot, false);
		}
		else
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ExitInstance(): Tried to exit instance with no logout save information, using new spawn"));

			// If everything fails, make new spawn
			AIGameMode* IGameMode = UIGameplayStatics::GetIGameModeChecked(this);
			if (!IGameMode)
			{
				UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ExitInstance(): IGameMode Null"));
			}
			else
			{
				FTransform SpawnTransform = UIGameplayStatics::GetIGameModeChecked(this)->FindRandomSpawnPointForCharacter(this, Cast<AIPlayerState>(GetPlayerState()));

				ServerBetterTeleport(SpawnTransform.GetLocation(), SpawnTransform.Rotator(), false);
			}
		}
	}

	// Just using an unbound delegate, the timer is used just for checking whether it is active or not
	FTimerDelegate TimerDel = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::HomeCaveCooldownExpire);
	GetWorldTimerManager().SetTimer(HomeCaveEnterCooldown, TimerDel, 5, false);

	if (UIHomeCaveDecoratorComponent* HomeCaveDecoratorComp = Cast<UIHomeCaveDecoratorComponent>(GetComponentByClass(UIHomeCaveDecoratorComponent::StaticClass())))
	{
		HomeCaveDecoratorComp->ClientFinish();
	}

	if (AIPlayerController* IPlayerController = GetController<AIPlayerController>())
	{
		IPlayerController->ClientShowLoadingScreen();
	}

	Instance->RemovePlayer(this, false);

	RemoveInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));

	InstanceId = FPrimaryAssetId();

	ToggleHomecaveExitbuff(true);
	ToggleInHomecaveEffect(false);

	UIGameplayStatics::GetIGameModeChecked(this)->SaveCharacter(this, ESavePriority::High);

	ClientExitInstance();

	if (IsCampingHomecave())
	{
		bHomecaveNearby = true;
		ToggleHomecaveCampingDebuff(true);
	}
}

void AIBaseCharacter::HomeCaveCooldownExpire()
{
	if (!HasAuthority())
	{
		return;
	}

	if (bHasPendingHomeCaveTeleport && PendingHomeCaveReturn.GetLocation() != FVector::ZeroVector)
	{
		AIPlayerController* IPlayerController = GetController<AIPlayerController>();
		check(IPlayerController);
		if (IPlayerController)
		{
			IPlayerController->ServerSpawnAndEnterInstance(PendingHomeCaveReturn);
		}

		PendingHomeCaveReturn = FTransform();
		bHasPendingHomeCaveTeleport = false;
	}
}

void AIBaseCharacter::ClientExitInstance_Implementation()
{
	if (AIHUD* IHUD = UIGameplayStatics::GetIHUD(this))
	{
		if (IHUD->GetWindowOfClass(UIHomeCaveCustomiserWidget::StaticClass()) ||
			IHUD->GetWindowOfClass(UIHomeCaveDecorationWidget::StaticClass()))
		{
			// close all windows if we have any sort of home cave window open
			IHUD->CloseAllWindows();
		}
	}

	OnExitInstance.Broadcast();

	if (UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this))
	{
		IGameInstance->SetLevelStreamingEnabled(true);
	}

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	check(IGameHUD);
	if (!IGameHUD)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ClientExitInstance_Implementation: IGameHUD null"));
		return;
	}

	UIMainGameHUDWidget* IMainGameHUDWidget = Cast<UIMainGameHUDWidget>(IGameHUD->ActiveHUDWidget);
	check(IMainGameHUDWidget);
	if (!IMainGameHUDWidget)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ClientExitInstance_Implementation: IMainGameHUDWidget null"));
		return;
	}

	IMainGameHUDWidget->HideTutorialPromptWidget();

	if (UIGameUserSettings* IGameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings()))
	{
		if (!IGameUserSettings->bToggleSprint && bDesiresToSprint)
		{
			OnStopSprinting();
		}
	}

	CurrentInstance = nullptr;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, CurrentInstance, this);
	
	OnRep_CurrentInstance();
	bAwaitingCaveResponse = false;
}

void AIBaseCharacter::ClientInCombatWarning_Implementation()
{
	if (IsRunningDedicatedServer())
	{
		return;
	}

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	check(IGameHUD);
	if (!IGameHUD)
	{
		return;
	}

	bAwaitingCaveResponse = false;
	
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_InCombatWarning))
	{
		return;
	}

	IGameHUD->QueueToast(HomeCaveToastIconObjectPtr, FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("InCombatTitle")), FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("InCombatDescription")));

	FTimerDelegate Del{};
	GetWorldTimerManager().SetTimer(TimerHandle_InCombatWarning, Del, 8.0f, false);

}

void AIBaseCharacter::ClientEnterInstance_Implementation()
{
	bAwaitingCaveResponse = false;

	if (UIGameInstance* const IGameInstance = UIGameplayStatics::GetIGameInstance(this))
	{
		IGameInstance->SetLevelStreamingEnabled(false);
	}

	const AIGameState* const IGameState = UIGameplayStatics::GetIGameStateChecked(this);

	if (!IGameState)
	{
		return;
	}

	if (IGameState->GetGameStateFlags().bGrowthWellRestedBuff &&
		UIGameplayStatics::IsGrowthEnabled(this) &&
		GetGrowthPercent() < 1.0f &&
		GetCurrentInstance()->IsA<AIPlayerCaveMain>())
	{
		AIGameHUD* const IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
		check(IGameHUD);

		if (!IGameHUD)
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ClientEnterInstance_Implementation: IGameHUD null"));
			return;
		}

		if (!IsWellRested())
		{
			IGameHUD->ShowHomecaveWellRestTip();
		}
	}
}

void AIBaseCharacter::ServerExitInstance_Implementation(const FTransform& OverrideReturnTransform)
{
	ExitInstance(OverrideReturnTransform);
}

void AIBaseCharacter::SendHomeCaveToastToParty(const AIPlayerState* IPlayerState, bool bEntered, bool bOwned)
{
	if (!IPlayerState) return;
	if (AIPlayerGroupActor* IPlayerGroupActor = IPlayerState->GetPlayerGroupActor())
	{
		for (AIPlayerState* GroupMember : IPlayerGroupActor->GetGroupMembers())
		{
			if (GroupMember != IPlayerState)
			{
				if (AIBaseCharacter* GroupCharacter = Cast<AIBaseCharacter>(GroupMember->GetPawn()))
				{
					GroupCharacter->ClientPlayerHomecaveToast(IPlayerState, bEntered, bOwned);
				}
			}
		}
	}
	
}

void AIBaseCharacter::ClientPlayerHomecaveToast_Implementation(const AIPlayerState* IPlayerState, bool bEntered, bool bOwned)
{
	if (!ensure(IPlayerState))
	{
		return;
	}

	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("Name"), FText::FromString(IPlayerState->GetPlayerName()));

		if (bEntered)
		{
			if (bOwned)
			{
				IGameHUD->QueueToast(HomeCaveToastIconObjectPtr, FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerEnteredTitle")), FText::Format(FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerEnteredOwnedDescription")), Arguments));
			}
			else
			{
				IGameHUD->QueueToast(HomeCaveToastIconObjectPtr, FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerEnteredTitle")), FText::Format(FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerEnteredOtherDescription")), Arguments));
			}
		}
		else
		{
			if (bOwned)
			{
				IGameHUD->QueueToast(HomeCaveToastIconObjectPtr, FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerExitTitle")), FText::Format(FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerExitOwnedDescription")), Arguments));
			}
			else
			{
				IGameHUD->QueueToast(HomeCaveToastIconObjectPtr, FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerExitTitle")), FText::Format(FText::FromStringTable(TEXT("ST_HomeCave"), TEXT("PlayerExitOtherDescription")), Arguments));
			}
		}
	}

}

void AIBaseCharacter::SetCharacterName(const FString& NewCharacterName)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, CharacterName, NewCharacterName, this);
}

TArray<int>& AIBaseCharacter::GetUnlockedSubspeciesIndexes_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, UnlockedSubspeciesIndexes, this);
	return UnlockedSubspeciesIndexes;
}

void AIBaseCharacter::ServerKickPlayerFromOwnedInstance_Implementation(AIPlayerState* IPlayerState)
{
	if (AIPlayerCaveBase* IPlayerCaveBase = GetCurrentInstance())
	{
		if (AIBaseCharacter* KickedBaseCharacter = Cast<AIBaseCharacter>(IPlayerState->GetPawn()))
		{
			if (KickedBaseCharacter->GetCurrentInstance() == IPlayerCaveBase)
			{
				KickedBaseCharacter->ExitInstance(FTransform());
			}
		}
	}
}

void AIBaseCharacter::AddOwnedDecoration(UHomeCaveDecorationDataAsset* DataAsset, int Amount)
{
	check(DataAsset);
	if (!DataAsset) return;
	check(Amount > 0);
	if (Amount <= 0) return;
	// If there is no such bought decoration, add one, otherwise get the existing and add the amount
	FHomeCaveDecorationPurchaseInfo* PurchaseInfo = GetOwnedDecoration(DataAsset);
	if (!PurchaseInfo)
	{
		FHomeCaveDecorationPurchaseInfo NewPurchaseInfo;
		NewPurchaseInfo.DataAsset = DataAsset;
		NewPurchaseInfo.AmountOwned = Amount;
		GetHomeCaveSaveableInfo_Mutable().BoughtDecorations.Add(NewPurchaseInfo);
	}
	else
	{
		PurchaseInfo->AmountOwned += Amount;
	}

	if (HasAuthority() && IsLocallyControlled())
	{
		OnRep_HomeCaveSaveableInfo();
	}
}

FHomeCaveDecorationPurchaseInfo* AIBaseCharacter::GetOwnedDecoration(UHomeCaveDecorationDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset) return nullptr;

#if UE_SERVER
	START_PERF_TIME();
#endif

	for (FHomeCaveDecorationPurchaseInfo& PurchaseInfo : GetHomeCaveSaveableInfo_Mutable().BoughtDecorations)
	{
		if (PurchaseInfo.DataAsset == DataAsset)
		{

#if UE_SERVER
			END_PERF_TIME();
			WARN_PERF_TIME_STATIC(1);
#endif
			return &PurchaseInfo;
		}
	}

#if UE_SERVER
	END_PERF_TIME();
	WARN_PERF_TIME_STATIC(1);
#endif
	return nullptr;
}

void AIBaseCharacter::RemoveOwnedDecoration(UHomeCaveDecorationDataAsset* DataAsset, int Amount)
{
	check(DataAsset);
	if (!DataAsset) return;
	check(Amount > 0);
	if (Amount <= 0) return;

	START_PERF_TIME();
	// Find a matching bought decoration, if it exists, minus the amount. If it will be less than 1 remove it completely
	FHomeCaveSaveableInfo& MutableHomeCaveSaveableInfo = GetHomeCaveSaveableInfo_Mutable();
	for (int Index = 0; Index < MutableHomeCaveSaveableInfo.BoughtDecorations.Num(); Index++)
	{
		FHomeCaveDecorationPurchaseInfo& PurchaseInfo = MutableHomeCaveSaveableInfo.BoughtDecorations[Index];
		if (PurchaseInfo.DataAsset == DataAsset)
		{
			if (PurchaseInfo.AmountOwned - Amount <= 0)
			{
				MutableHomeCaveSaveableInfo.BoughtDecorations.RemoveAt(Index);
			}
			else
			{
				PurchaseInfo.AmountOwned -= Amount;
			}
			break;
		}
	}

	if (HasAuthority() && IsLocallyControlled())
	{
		OnRep_HomeCaveSaveableInfo();
	}

	END_PERF_TIME();
	WARN_PERF_TIME_STATIC(1);
}

int AIBaseCharacter::GetOwnedDecorationCount(UHomeCaveDecorationDataAsset* DataAsset)
{
	if (!DataAsset) return 0;
	if (FHomeCaveDecorationPurchaseInfo* PurchaseInfo = GetOwnedDecoration(DataAsset))
	{
		return PurchaseInfo->AmountOwned;
	}

	return 0;
}


void AIBaseCharacter::ServerRequestDecorationPurchase_Implementation(UHomeCaveDecorationDataAsset* DataAsset, int Amount)
{
	check(Amount > 0);
	if (Amount <= 0) return;

	check(DataAsset);
	if (!DataAsset) return;

	int TotalCost = DataAsset->MarksCost * Amount;

	if (GetMarks() < TotalCost) return;
	AddMarks(-TotalCost);

	AddOwnedDecoration(DataAsset, Amount);

	ClientConfirmDecorationPurchase();
}

void AIBaseCharacter::ServerRequestDecorationSale_Implementation(UHomeCaveDecorationDataAsset* DataAsset, int Amount)
{
	check(Amount > 0);
	if (Amount <= 0) return;

	check(DataAsset);
	if (!DataAsset) return;

	int TotalCost = DataAsset->MarksCost * Amount;

	if (GetOwnedDecorationCount(DataAsset) < Amount) return;
	AddMarks((int32)(TotalCost * HomeCaveRefundMultiplier));

	RemoveOwnedDecoration(DataAsset, Amount);

	ClientConfirmDecorationPurchase();
}

void AIBaseCharacter::ServerRequestExtensionPurchase_Implementation(UHomeCaveExtensionDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset) return;

	if (!DataAsset->bIsBaseUpgrade)
	{
		AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
		check(IPlayerState);
		if (!IPlayerState) return;

		UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
		check(IGameInstance);
		if (!IGameInstance) return;

		EGameOwnership Ownership = IGameInstance->GetGameOwnership(IPlayerState);

		if (Ownership == EGameOwnership::Free)
		{
			ClientConfirmExtensionPurchase();
			return;
		}
	}

	if (GetMarks() < DataAsset->MarksCost) return;
	AddMarks(-DataAsset->MarksCost);
	
	AddOwnedExtension(DataAsset);

	ClientConfirmExtensionPurchase();
}

void AIBaseCharacter::ServerRequestExtensionSale_Implementation(UHomeCaveExtensionDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset) return;

	check(IsExtensionOwned(DataAsset));
	if (!IsExtensionOwned(DataAsset)) return;

	if (IsExtensionActive(DataAsset))
	{
		// The extension is active as one of the rooms and the player must swap it for another room first
		// before it can be sold
		ClientExtensionSaleFailed();
		return;
	}

	AddMarks((int32)(DataAsset->MarksCost * HomeCaveRefundMultiplier));
	
	RemoveOwnedExtension(DataAsset);

	ClientConfirmExtensionPurchase();
}

void AIBaseCharacter::ClientExtensionSaleFailed_Implementation()
{
	if (AIHUD* IHUD = UIGameplayStatics::GetIHUD(this))
	{
		if (Handle_ExtensionSaleFailedWidget.IsValid() && Handle_ExtensionSaleFailedWidget->IsLoadingInProgress())
		{
			Handle_ExtensionSaleFailedWidget->CancelHandle();
			Handle_ExtensionSaleFailedWidget = nullptr;
		}
		
		FAsyncWindowOpenedDelegate OnWindowOpened;
		OnWindowOpened.BindUObject(this, &AIBaseCharacter::ExtensionSaleFailedMessageBoxLoaded);
		Handle_ExtensionSaleFailedWidget = IHUD->OpenWindowAsyncHandle(ExtensionSaleFailedMessageBoxClassPtr, false, true, false, false, true, OnWindowOpened);
	}
}

void AIBaseCharacter::ExtensionSaleFailedMessageBoxLoaded(UIUserWidget* LoadedWidget)
{
	if (UIMessageBox* IMessageBox = Cast<UIMessageBox>(LoadedWidget))
	{
		FFormatNamedArguments Arguments;
		IMessageBox->SetTextAndTitle(
			FText::FromStringTable(FName(TEXT("ST_HomeCave")), FString(TEXT("RoomIsActiveNoSale"))),
			FText::FromStringTable(FName(TEXT("ST_HomeCave")), FString(TEXT("RoomIsActiveNoSaleMessage")))
		);
		if (AIHUD* IHUD = UIGameplayStatics::GetIHUD(this))
		{
			if (UIHomeCaveRoomPurchaseWidget* RoomPurchaseWidget = Cast<UIHomeCaveRoomPurchaseWidget>(IHUD->GetWindowOfClass(UIHomeCaveRoomPurchaseWidget::StaticClass())))
			{
				RoomPurchaseWidget->SetInputEnabled(true);
				RoomPurchaseWidget->SetVisibility(ESlateVisibility::Collapsed);
				IMessageBox->SetWidgetToShowOnClose(RoomPurchaseWidget);
			}
		}
	}

	Handle_ExtensionSaleFailedWidget = nullptr;
}

void AIBaseCharacter::ClientConfirmDecorationPurchase_Implementation()
{
	if (AIHUD* IHUD = UIGameplayStatics::GetIHUD(this))
	{
		if (UIHomeCaveDecorPurchaseWidget* PurchaseWidget = Cast<UIHomeCaveDecorPurchaseWidget>(IHUD->GetWindowOfClass(UIHomeCaveDecorPurchaseWidget::StaticClass())))
		{
			PurchaseWidget->Refresh();
		}
		if (UIHomeCaveCatalogueWidget* CatalogueWidget = Cast<UIHomeCaveCatalogueWidget>(IHUD->GetWindowOfClass(UIHomeCaveCatalogueWidget::StaticClass())))
		{
			CatalogueWidget->Refresh();
		}
	}
}

void AIBaseCharacter::ClientConfirmExtensionPurchase_Implementation()
{
	if (AIHUD* IHUD = UIGameplayStatics::GetIHUD(this))
	{
		if (UIHomeCaveRoomPurchaseWidget* PurchaseWidget = Cast<UIHomeCaveRoomPurchaseWidget>(IHUD->GetWindowOfClass(UIHomeCaveRoomPurchaseWidget::StaticClass())))
		{
			PurchaseWidget->Refresh();
		}
		if (UIHomeCaveRoomCatalogueWidget* CatalogueWidget = Cast<UIHomeCaveRoomCatalogueWidget>(IHUD->GetWindowOfClass(UIHomeCaveRoomCatalogueWidget::StaticClass())))
		{
			CatalogueWidget->Refresh();
		}
	}
}

void AIBaseCharacter::RetryEnterInstance()
{
	ServerEnterInstance(RetryTile, RetryOverrideTransform, RetryReturnTransform);
}

void AIBaseCharacter::ServerEnterInstance(const FInstancedTile& Tile, const FTransform& OverrideTransform, const FTransform& ReturnTransform)
{
	if (IsCarryingObject())
	{
		AIQuestItem* QuestItem = Cast<AIQuestItem>(GetCurrentlyCarriedObject().Object.Get());

		if (!QuestItem)
		{
			EndCarrying(GetCurrentlyCarriedObject().Object.Get(), true);
		}
		else
		{
			AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
			if (!IWorldSettings) return;

			AIQuestManager* QuestMgr = IWorldSettings->QuestManager;
			if (!QuestMgr) return;

			bool bTaskFound = false;

			UIQuest* TrophyQuest = nullptr;
			for (UIQuest* Quest : GetActiveQuests())
			{
				if (!Quest || Quest->QuestData->QuestType != EQuestType::TrophyDelivery) continue;

				for (UIQuestBaseTask* QuestTask : Quest->GetQuestTasks())
				{
					if (!QuestTask) continue;

					if (QuestTask->Tag == QuestItem->QuestTag)
					{
						TrophyQuest = Quest;
						bTaskFound = true;
						break;
					}
				}

				if (bTaskFound) break;
			}

			if (bTaskFound)
			{
				FQuestHomecaveDecorationReward QuestHomeCaveDecorationReward;
				QuestHomeCaveDecorationReward.Decoration = QuestItem->Decoration;
				TrophyQuest->HomecaveDecorationRewards.Add(QuestHomeCaveDecorationReward);
				QuestMgr->OnItemDelivered(QuestItem->QuestTag, this, TrophyQuest);
				QuestItem->Destroy();
			}
		}
	}

	check(Tile.SpawnedTile);
	if (!Tile.SpawnedTile) 
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerEnterInstance(): Tile.SpawnedTile null"));
		return;
	}

	if (!Tile.SpawnedTile->IsLoaded())
	{
		RetryTile = Tile;
		RetryOverrideTransform = OverrideTransform;
		RetryReturnTransform = ReturnTransform;
		Tile.SpawnedTile->OnFinishLoad.AddUniqueDynamic(this, &AIBaseCharacter::RetryEnterInstance);
		return;
	}

	if (GetCurrentInstance())
	{
		if (GetCurrentInstance() == Tile.SpawnedTile)
		{ // If we are already inside the instance then just do a normal teleport

			if (OverrideTransform.Equals(FTransform()))
			{
				FTransform TPTransform = GetCurrentInstance()->GetActorTransform();
				bool FoundSpawn = Tile.SpawnedTile->FindSpawnLocation(TPTransform, this);
				check(FoundSpawn);
				ServerBetterTeleport(TPTransform.GetLocation(), TPTransform.GetRotation().Rotator(), true);
			}
			else
			{
				ServerBetterTeleport(OverrideTransform.GetLocation(), OverrideTransform.GetRotation().Rotator(), true);
			}
			ClientEnterInstance();
			return;

		}
		else
		{
			ExitInstance(ReturnTransform);
		}
	}

	RemoveInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));

	FInstanceLogoutSaveableInfo NewLogoutInfo;
	NewLogoutInfo.LoggedLevel = UGameplayStatics::GetCurrentLevelName(this);
	NewLogoutInfo.CaveReturnLocation = ReturnTransform.Equals(FTransform()) ? GetActorLocation() : ReturnTransform.GetLocation();
	NewLogoutInfo.CaveReturnYaw = ReturnTransform.Equals(FTransform()) ? GetActorRotation().Yaw : ReturnTransform.GetRotation().Rotator().Yaw;
	NewLogoutInfo.Owned = this == Tile.SpawnedTile->GetOwner();
	InstanceLogoutInfo.Add(NewLogoutInfo);

	FallbackCaveReturnPosition = ReturnTransform.Equals(FTransform()) ? GetActorLocation() : ReturnTransform.GetLocation();
	FallbackCaveReturnRotation = ReturnTransform.Equals(FTransform()) ? GetActorRotation() : ReturnTransform.GetRotation().Rotator();
	FallbackCaveReturnRotation.Roll = 0;

	Tile.SpawnedTile->AddPlayer(this, OverrideTransform);

	if (Cast<AIPlayerCaveMain>(Tile.SpawnedTile))
	{
		// Instance is a home cave so we can submit for home cave quest
		ServerSubmitGenericTask(NAME_FindHomeCave);

		bHomecaveNearby = false;
		HomecaveNearbyInitialTimestamp = -1.0f;
		
		ToggleHomecaveExitbuff(false);
		ToggleInHomecaveEffect(true);
	}


	InstanceId = Tile.TileId;

	UIGameplayStatics::GetIGameModeChecked(this)->SaveCharacter(this, ESavePriority::High);
}

bool AIBaseCharacter::IsExtensionOwned(UHomeCaveExtensionDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::IsExtensionOwned(): DataAsset null"));
		return false;
	}
#if UE_SERVER
	START_PERF_TIME();
#endif
	for (const FHomeCaveExtensionPurchaseInfo& PurchaseInfo : GetHomeCaveSaveableInfo().BoughtExtensions)
	{
		if (PurchaseInfo.DataAsset == DataAsset) 
		{
#if UE_SERVER
			END_PERF_TIME();
			WARN_PERF_TIME_STATIC(1);
#endif
			return true;
		}
	}

#if UE_SERVER
	END_PERF_TIME();
	WARN_PERF_TIME_STATIC(1);
#endif
	return false;
}

bool AIBaseCharacter::IsExtensionActive(UHomeCaveExtensionDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::IsExtensionActive(): DataAsset null"));
		return false;
	}
	if (DataAsset == nullptr) return false;
	if (GetHomeCaveSaveableInfo().ActiveLeftExtensionDataAsset == DataAsset) return true;
	if (GetHomeCaveSaveableInfo().ActiveMiddleExtensionDataAsset == DataAsset) return true;
	if (GetHomeCaveSaveableInfo().ActiveRightExtensionDataAsset == DataAsset) return true;
	if (GetHomeCaveSaveableInfo().ActiveBaseRoomDataAsset == DataAsset) return true;
	return false;
}

void AIBaseCharacter::AddOwnedExtension(UHomeCaveExtensionDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::AddOwnedExtension(): DataAsset null"));
		return;
	}
	check(!IsExtensionOwned(DataAsset));
	if (IsExtensionOwned(DataAsset))
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::AddOwnedExtension(): Extension Already Owned: %s"), *DataAsset->ExtensionName.ToString());
		return;
	}
	FHomeCaveExtensionPurchaseInfo ExtensionPurchaseInfo;
	ExtensionPurchaseInfo.DataAsset = DataAsset;
	GetHomeCaveSaveableInfo_Mutable().BoughtExtensions.Add(ExtensionPurchaseInfo);
}

void AIBaseCharacter::RemoveOwnedExtension(UHomeCaveExtensionDataAsset* DataAsset)
{
	check(DataAsset);
	if (!DataAsset)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::RemoveOwnedExtension(): DataAsset null"));
		return;
	}
	START_PERF_TIME();

	for (int Index = 0; Index < GetHomeCaveSaveableInfo().BoughtExtensions.Num(); Index++)
	{
		if (DataAsset == GetHomeCaveSaveableInfo().BoughtExtensions[Index].DataAsset)
		{
			GetHomeCaveSaveableInfo_Mutable().BoughtExtensions.RemoveAt(Index);
			break;
		}
	}

	END_PERF_TIME();
	WARN_PERF_TIME_STATIC(1);
}

void AIBaseCharacter::OutsideWorldBounds()
{
	UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::OutsideWorldBounds(): IBaseCharacter has moved outside of the world bounds!! This can be due to instances spawning outside of the world bounds."));

	// If we are in an instance then leave it.
	// The default behavior to being outside
	// world bounds is to become destroyed.
	// Ideally we would not go outside of world bounds,
	// but just in-case it does happen, we have an error
	// message and the player does not get deleted.
	if (GetCurrentInstance())
	{
		ExitInstance(FTransform());
	}
	else
	{
		Super::OutsideWorldBounds();
	}
}

void AIBaseCharacter::ServerRequestHomeCaveReset_Implementation()
{
	if (AIPlayerCaveMain* IPlayerCaveMain = Cast<AIPlayerCaveMain>(GetCurrentInstance()))
	{
		IPlayerCaveMain->ResetAllRooms(this);
	}
}

int AIBaseCharacter::GetBoughtRoomSocketCount()
{
	int Count = 0;
	if (GetHomeCaveSaveableInfo().LeftRoomBought) Count++;
	if (GetHomeCaveSaveableInfo().MiddleRoomBought) Count++;
	if (GetHomeCaveSaveableInfo().RightRoomBought) Count++;
	return Count;
}

bool AIBaseCharacter::IsRoomSocketBought(ECaveUpgrade CaveUpgrade)
{
	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance) return false;

	AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
	check(IPlayerState);
	if (!IPlayerState) return false;

	switch (CaveUpgrade)
	{
	case ECaveUpgrade::CU_Left:
		return GetHomeCaveSaveableInfo().LeftRoomBought || (GetHomeCaveSaveableInfo().ActiveLeftExtensionDataAsset != nullptr);
	case ECaveUpgrade::CU_Right:
		return GetHomeCaveSaveableInfo().RightRoomBought || (GetHomeCaveSaveableInfo().ActiveRightExtensionDataAsset != nullptr);
	case ECaveUpgrade::CU_Middle:
		return GetHomeCaveSaveableInfo().MiddleRoomBought || (GetHomeCaveSaveableInfo().ActiveMiddleExtensionDataAsset != nullptr);
	default:
		break;
	}

	return false;
}

void AIBaseCharacter::ServerRequestRoomSocketPurchase_Implementation(ECaveUpgrade CaveUpgrade)
{
	if (IsRoomSocketBought(CaveUpgrade)) return;

	AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
	check(IPlayerState);
	if (!IPlayerState) return;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance) return;

	EGameOwnership Ownership = IGameInstance->GetGameOwnership(IPlayerState);

	if (Ownership == EGameOwnership::Free)
	{
		// Dont allow room socket purchase on free
		return;
	}

	int BoughtRoomSockets = GetBoughtRoomSocketCount();
	check (BoughtRoomSockets < 3);
	if (BoughtRoomSockets >= 3) return;

	int SelectedPrice = GetRoomSocketPrice();
	if (GetMarks() < SelectedPrice) return;

	switch (CaveUpgrade)
	{
	case ECaveUpgrade::CU_Left:
		GetHomeCaveSaveableInfo_Mutable().LeftRoomBought = true;
		break;
	case ECaveUpgrade::CU_Right:
		GetHomeCaveSaveableInfo_Mutable().RightRoomBought = true;
		break;
	case ECaveUpgrade::CU_Middle:
		GetHomeCaveSaveableInfo_Mutable().MiddleRoomBought = true;
		break;
	default:
		break;
	}

	AddMarks(-SelectedPrice);

	if (HasAuthority() && IsLocallyControlled())
	{
		OnRep_HomeCaveSaveableInfo();
	}
}

int AIBaseCharacter::GetRoomSocketPrice()
{
	int BoughtRoomSockets = GetBoughtRoomSocketCount();
	check (BoughtRoomSockets < 3);
	if (BoughtRoomSockets >= 3) return 0;
	int SelectedPrice = RoomSocketPrices[BoughtRoomSockets];

	return SelectedPrice;
}

int AIBaseCharacter::GetRoomSocketRefundAmount()
{
	int Amount = 0;
	int BoughtRoomSockets = GetBoughtRoomSocketCount();
	if (BoughtRoomSockets > 0) Amount += RoomSocketPrices[0];
	if (BoughtRoomSockets > 1) Amount += RoomSocketPrices[1];
	if (BoughtRoomSockets > 2) Amount += RoomSocketPrices[2];
	
	return Amount;
}

bool AIBaseCharacter::IsReadyToLeaveHatchlingCave() const
{
	if (GetActiveQuests().Num() == 0)
	{
		// If the player has no active quests then they shouldn't be in the cave
		return true;
	}
	if (!IsReadyToLeaveHatchlingCaveRaw()) 
	{
		return false;
	}

	return true;
}

void AIBaseCharacter::DisableCollisionCapsules(bool bPermanentlyIgnoreAllChannels)
{
	UE_LOG(LogTemp, Verbose, TEXT("AIBaseCharacter::DisableCollisionCapsules: bPermanentlyIgnoreAllChannels (%s)"), bPermanentlyIgnoreAllChannels ? TEXT("True") : TEXT("Fasle"));

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		if (bPermanentlyIgnoreAllChannels)
		{
			CapsuleComp->SetCollisionResponseToAllChannels(ECR_Ignore);
		}
	}

	if (UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		CharMove->DisableAdditionalCapsules(bPermanentlyIgnoreAllChannels);
	}
}

void AIBaseCharacter::EnableCollisionCapsules()
{
	UE_LOG(LogTemp, Verbose, TEXT("AIBaseCharacter::EnableCollisionCapsules"));

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UICharacterMovementComponent* CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement()))
	{
		CharMove->EnableAdditionalCapsules();
	}
}

void AIBaseCharacter::SetCollisionResponseProfile(FName NewCollisionProfile)
{
	if (NewCollisionProfile == NAME_None)
	{
		return;
	}

	UCapsuleComponent* const CapsuleComp = GetCapsuleComponent();
	
	if (ensureAlways(CapsuleComp))
	{
		CapsuleComp->SetCollisionProfileName(NewCollisionProfile);
	}
	
	UICharacterMovementComponent* const CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());

	if (!ensureAlways(CharMove))
	{
		return;
	}

	for (UCapsuleComponent* const AdditionalCapsule : CharMove->GetAdditionalCapsules())
	{
		if (!ensureAlways(AdditionalCapsule))
		{
			continue;
		}
		AdditionalCapsule->SetCollisionProfileName(NewCollisionProfile);
	}
}

void AIBaseCharacter::SetCollisionResponseProfilesToDefault()
{
	UCapsuleComponent* const CapsuleComp = GetCapsuleComponent();

	if (ensureAlways(CapsuleComp))
	{
		const UCapsuleComponent* const DefaultCapsuleComponent = GetClass()->GetDefaultObject<AIBaseCharacter>()->GetCapsuleComponent();
		if (ensureAlways(DefaultCapsuleComponent))
		{
			CapsuleComp->SetCollisionEnabled(DefaultCapsuleComponent->GetCollisionEnabled());
			CapsuleComp->SetCollisionObjectType(DefaultCapsuleComponent->GetCollisionObjectType());
			CapsuleComp->SetCollisionResponseToChannels(DefaultCapsuleComponent->GetCollisionResponseToChannels());
		}
	}

	UICharacterMovementComponent* const CharMove = Cast<UICharacterMovementComponent>(GetCharacterMovement());

	if (!ensureAlways(CharMove))
	{
		return;
	}

	CharMove->SetCollisionResponseToDefault();	
}

void AIBaseCharacter::ServerRequestLeaveCave_Implementation(bool bHatchling)
{
	if (bHatchling)
	{
		if (!IsReadyToLeaveHatchlingCave())
		{
			// If the player is not ready to leave then show the not ready prompt
			ClientLeaveCavePrompt(KEY_TutorialExitPromptNotReady, false, bHatchling);
			return;
		}

		AIHatchlingCave* const IHatchlingCave = Cast<AIHatchlingCave>(GetCurrentInstance());
		check(IHatchlingCave);
		if (!IHatchlingCave)
		{
			UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerRequestLeaveCave_Implementation: IHatchlingCave nullptr"));
			return;
		}

		AIPlayerState* const ThisPlayerState = GetPlayerState<AIPlayerState>();
		check(ThisPlayerState);
		if (!ThisPlayerState)
		{
			UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerRequestLeaveCave_Implementation: ThisPlayerState nullptr"));
			return;
		}

		if (AIPlayerGroupActor* IPlayerGroupActor = ThisPlayerState->GetPlayerGroupActor())
		{
			if (IPlayerGroupActor->GetGroupLeader() == ThisPlayerState)
			{
				// Player is in a group and is the leader so check all group members
				bool bAllPlayersReady = true;
				for (AIPlayerState* GroupPlayerState : IPlayerGroupActor->GetGroupMembers())
				{
					if (AIBaseCharacter* GroupBaseCharacter = GroupPlayerState->GetPawn<AIBaseCharacter>())
					{
						// Check if the player is in your instance
						// Check the player is in the hatchling cave
						// and check they are ready to leave and or num of quests for everyone

						// The group player should be in the same instance as the leader.
						check(GroupBaseCharacter->GetCurrentInstance() == IHatchlingCave);

						if (!GroupBaseCharacter->IsReadyToLeaveHatchlingCave())
						{
							bAllPlayersReady = false;
							break;
						}
					}
				}

				if (bAllPlayersReady)
				{
					// Group leader and all players are ready
					ClientLeaveCavePrompt(KEY_TutorialExitPromptGroupLeaderMsg, true, bHatchling);
				}
				else
				{
					// Group leader and some players are not ready
					ClientLeaveCavePrompt(KEY_TutorialExitPromptGroupLeaderNotReadyMsg, false, bHatchling);
				}
			}
			else
			{
				// Player is not the leader of the group so they can't leave until the leader wants
				ClientLeaveCavePrompt(KEY_TutorialExitPromptGroupMemberMsg, false, bHatchling);
			}

		}
		else
		{
			// Player is solo so we can just consider which solo prompt to show
			if (IHatchlingCave->GetPlayerCount() > 1)
			{
				// Solo and is only player in cave
				ClientLeaveCavePrompt(KEY_TutorialExitPromptSoloGroupMsg, true, bHatchling);
			}
			else
			{
				// Solo and there are more players in cave
				ClientLeaveCavePrompt(KEY_TutorialExitPromptSoloMsg, true, bHatchling);
			}
		}
	}
	else
	{
		UWorld* World = GetWorld();
		if (!World) return;

		AIGameMode* IGameMode = World->GetAuthGameMode<AIGameMode>();
		if (!IGameMode) return;

		AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
		if (!IWorldSettings) return;

		// Get game state
		AIGameState* const IGameState = Cast<AIGameState>(World->GetGameState());
		const int32 PlayerCount = IGameState->PlayerArray.Num();
		bool bNearbyPlayer = false;
		const int32 NearbyPlayerDistanceRequirement = IWorldSettings && IWorldSettings->HomeCaveManager ? IWorldSettings->HomeCaveManager->GetRequiredDistanceFromHomecave() : 20000;

		// Make sure at least 1 player on server
		if (PlayerCount > 0)
		{
			// All Connected Clients
			const TArray<APlayerState*> TempPlayersArray = IGameState->PlayerArray;

			FInstanceLogoutSaveableInfo* InstanceLogoutSaveableInfo = GetInstanceLogoutInfo(UGameplayStatics::GetCurrentLevelName(this));
			check(InstanceLogoutSaveableInfo);
			if (InstanceLogoutSaveableInfo)
			{
				const FVector MyLocation = InstanceLogoutSaveableInfo->CaveReturnLocation;

				for (APlayerState* PS : TempPlayersArray)
				{
					if (!IsValid(PS)) continue;

					AIPlayerController* IPlayerController = Cast<AIPlayerController>(PS->GetOwner());
					if (!IsValid(IPlayerController)) continue;

					AIBaseCharacter* IBC = IPlayerController->GetPawn<AIBaseCharacter>();
					if (!IsValid(IBC) || !IBC->IsAlive() || IBC->IsCombatLogAI()) continue;

					if (IBC == this) continue;

					if (AIAdminCharacter* IAC = Cast<AIAdminCharacter>(IBC)) continue;

					const FVector CharacterLocation = IBC->GetActorLocation();

					if ((CharacterLocation - MyLocation).Size() <= NearbyPlayerDistanceRequirement)
					{
						IBC->UpdateHomeCaveDistanceTracking(true);

						bNearbyPlayer = true;
						break;
					}
				}
			}
		}

		ClientLeaveCavePrompt(bNearbyPlayer ? FString(TEXT("PlayerNearHomecaveExit")) : FString(TEXT("NoPlayersNearHomecaveExit")), true, bHatchling);
	}
}

void AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation()
{
	// This RPC func should only be called if the player is ready
	check (IsReadyToLeaveHatchlingCave());
	if (!IsReadyToLeaveHatchlingCave())
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: Player tried to leave hatchling cave when wasn't ready"))
		return;
	}

	AIHatchlingCave* const IHatchlingCave = Cast<AIHatchlingCave>(GetCurrentInstance());
	check(IHatchlingCave);
	if (!IHatchlingCave)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: IHatchlingCave nullptr"));
		return;
	}

	AIPlayerState* ThisPlayerState = GetPlayerState<AIPlayerState>();
	check(ThisPlayerState);
	if (!ThisPlayerState)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: ThisPlayerState nullptr"));
		return;
	}

	AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
	check (IWorldSettings);
	if (!IWorldSettings)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: IWorldSettings nullptr"));
		return;
	}

	AIQuestManager* QuestMgr = IWorldSettings->QuestManager;
	check(QuestMgr);
	if (!QuestMgr)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: QuestMgr nullptr"));
		return;
	}

	AIGameMode* IGameMode = UIGameplayStatics::GetIGameModeChecked(this);
	if (!IGameMode)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: IGameMode nullptr"));
		return;
	}

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	check(IGameState);
	if (!IGameState)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: IGameState nullptr"));
		return;
	}

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: IGameInstance nullptr"));
		return;
	}

	AIGameSession* Session = Cast<AIGameSession>(IGameInstance->GetGameSession());
	check(Session);
	if (!Session)
	{
		UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: Session nullptr"));
		return;
	}

	const bool bHomeCavesEnabled = UIGameplayStatics::AreHomeCavesEnabled(this);
	const bool bWaystoneEnabled = UIGameplayStatics::AreWaystonesEnabled(this);

	FTransform SpawnTransform = FTransform();

	if (AIPlayerGroupActor* IPlayerGroupActor = ThisPlayerState->GetPlayerGroupActor())
	{
		// In a group so we can TP all players who are in the same instance

		check (IPlayerGroupActor->GetGroupLeader() == ThisPlayerState);
		if (IPlayerGroupActor->GetGroupLeader() != ThisPlayerState)
		{
			UE_LOG(TitansLog, Error, TEXT("AIBaseCharacter::ServerTryLeaveHatchlingCave_Implementation: GroupLeader != ThisPlayerState"));
			return;
		}

		FName OverridePlayerStartTag = NAME_None;

		// If the group leader is a flyer
		if (PlayerStartTag == NAME_Flyer)
		{
			for (AIPlayerState* OtherPlayerState : IPlayerGroupActor->GetGroupMembers())
			{
				if (AIBaseCharacter* OtherBaseCharacter = OtherPlayerState->GetPawn<AIBaseCharacter>())
				{
					// If they are grouped with non-flyers
					if (OtherBaseCharacter->PlayerStartTag != NAME_Flyer)
					{
						// Start at a land spawn point instead of flyer
						OverridePlayerStartTag = NAME_Land;
						break;
					}
				}
			}
		}

		SpawnTransform = UIGameplayStatics::GetIGameModeChecked(this)->FindRandomSpawnPointForTag(GetCombinedPlayerStartTag(nullptr, OverridePlayerStartTag), PlayerStartTag);

		TArray<APlayerState*> PlayerStatesToTeleport{};
		PlayerStatesToTeleport.Reserve(IPlayerGroupActor->GetGroupMembers().Num());

		for (AIPlayerState* OtherPlayerState : IPlayerGroupActor->GetGroupMembers())
		{
			if (AIBaseCharacter* OtherBaseCharacter = OtherPlayerState->GetPawn<AIBaseCharacter>())
			{
				// All group members should be in the same instance and ready to leave by this function
				check (OtherBaseCharacter->GetCurrentInstance() == IHatchlingCave && OtherBaseCharacter->IsReadyToLeaveHatchlingCave());
				if (OtherBaseCharacter->GetCurrentInstance() == IHatchlingCave && OtherBaseCharacter->IsReadyToLeaveHatchlingCave())
				{
					PlayerStatesToTeleport.Add(OtherPlayerState);
					QuestMgr->OnPlayerSubmitGenericTask(OtherBaseCharacter, TEXT("ExitTutorial"));
					OtherBaseCharacter->SetHasLeftHatchlingCave(true);
				}
			}
		}

		// The correct instance will be set through this group teleport function, so there is no need to call ExitInstance()
		AIChatCommandManager::TeleportGroupLocation(this, PlayerStatesToTeleport, SpawnTransform.GetLocation(), nullptr);

		// Give all teleported group members quests
		// Assign random quest last after teleport as location quests are assigned based on what area you are in
		for (const APlayerState* OtherPlayerState : PlayerStatesToTeleport)
		{
			if (AIBaseCharacter* OtherBaseCharacter = OtherPlayerState->GetPawn<AIBaseCharacter>())
			{
				QuestMgr->ClearCompletedQuests(OtherBaseCharacter);
				QuestMgr->AssignRandomQuest(OtherBaseCharacter);
				if (bHomeCavesEnabled) 
				{
					QuestMgr->AssignQuest(HomeCaveTrackingQuest, OtherBaseCharacter, true);
				}
				if (bWaystoneEnabled)
				{
					QuestMgr->AssignQuest(WaystoneTrackingQuest, OtherBaseCharacter, true);
				}

				// Ensure we have enough growth to reach juvenile. with modded growth rates, the character can be a lot more than juvenile by the end of the tutorial
				const float PendingGrowth = UIGameplayStatics::GetTotalPendingGrowth(OtherBaseCharacter);
				const float CurrentGrowth = OtherBaseCharacter->GetGrowthPercent();
				const float TotalGrowth = CurrentGrowth + PendingGrowth;
				const float RequiredGrowth = IGameState->GetGameStateFlags().HatchlingCaveExitGrowth + 0.001f; 
				if (TotalGrowth < RequiredGrowth)
				{
					const float RewardGrowth = RequiredGrowth - TotalGrowth;
					const float GrowthReward = OtherBaseCharacter->GetGrowthPercent() <= Session->HatchlingCaveExitGrowth && !OtherBaseCharacter->HasLeftHatchlingCave() ? QuestMgr->BabyGrowthRewardRatePerMinute : QuestMgr->GrowthRewardRatePerMinute;
					UPOTAbilitySystemGlobals::RewardGrowthConstantRate(OtherBaseCharacter, RewardGrowth * Session->QuestGrowthMultiplier, GrowthReward / 60.f);
				}
			}
		}
	}
	else
	{
		// Not in a group so we can just TP this character
		QuestMgr->OnPlayerSubmitGenericTask(this, TEXT("ExitTutorial"));
		SetHasLeftHatchlingCave(true);

		SpawnTransform = UIGameplayStatics::GetIGameModeChecked(this)->FindRandomSpawnPointForCharacter(this, Cast<AIPlayerState>(GetPlayerState()));

		ExitInstance(SpawnTransform);

		// Assign random quest last after teleport as location quests are assigned based on what area you are in
		QuestMgr->ClearCompletedQuests(this);
		QuestMgr->AssignRandomPersonalQuests(this);
		if (bHomeCavesEnabled)
		{
			QuestMgr->AssignQuest(HomeCaveTrackingQuest, this, true);
		}
		if (bWaystoneEnabled)
		{
			QuestMgr->AssignQuest(WaystoneTrackingQuest, this, true);
		}

		// Ensure we have enough growth to reach juvenile. with modded growth rates, the character can be a lot more than juvenile by the end of the tutorial
		const float PendingGrowth = UIGameplayStatics::GetTotalPendingGrowth(this);
		const float CurrentGrowth = GetGrowthPercent();
		const float TotalGrowth = CurrentGrowth + PendingGrowth;
		const float RequiredGrowth = IGameState->GetGameStateFlags().HatchlingCaveExitGrowth + 0.001f;
		if (TotalGrowth < RequiredGrowth)
		{
			const float RewardGrowth = RequiredGrowth - TotalGrowth;
			const float GrowthReward = GetGrowthPercent() <= Session->HatchlingCaveExitGrowth && !HasLeftHatchlingCave() ? QuestMgr->BabyGrowthRewardRatePerMinute : QuestMgr->GrowthRewardRatePerMinute;
			UPOTAbilitySystemGlobals::RewardGrowthConstantRate(this, RewardGrowth * Session->QuestGrowthMultiplier, GrowthReward / 60.f);
		}
	}
}

void AIBaseCharacter::ServerSubmitGenericTask_Implementation(FName TaskKey)
{
	AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
	if (IWorldSettings)
	{
		if (AIQuestManager* QuestMgr = IWorldSettings->QuestManager)
		{
			QuestMgr->OnPlayerSubmitGenericTask(this, TaskKey);
		}
	}
}

void AIBaseCharacter::UpdateCompletedQuests(FPrimaryAssetId QuestId, EQuestType QuestType, int32 Max)
{
	RecentCompletedQuests.Add(FRecentQuest(QuestId, QuestType));
	const int32 AmountOfQuestsToRemove = RecentCompletedQuests.Num() - Max;
	if (AmountOfQuestsToRemove > 0)
	{
		RecentCompletedQuests.RemoveAt(0, AmountOfQuestsToRemove);
	}
}

void AIBaseCharacter::UpdateFailedQuests(FPrimaryAssetId QuestId, EQuestType QuestType, int32 Max)
{
	RecentFailedQuests.Add(FRecentQuest(QuestId, QuestType));
	const int32 AmountOfQuestsToRemove = RecentFailedQuests.Num() - Max;
	if (AmountOfQuestsToRemove > 0)
	{
		RecentFailedQuests.RemoveAt(0, AmountOfQuestsToRemove);
	}
}

TConstArrayView<FRecentQuest> AIBaseCharacter::GetRecentCompletedQuests() const
{
	return MakeArrayView(RecentCompletedQuests);
}

TConstArrayView<FRecentQuest> AIBaseCharacter::GetRecentFailedQuests() const
{
	return MakeArrayView(RecentFailedQuests);
}

TArray<UIQuest*>& AIBaseCharacter::GetActiveQuests_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, ActiveQuests, this);
	return ActiveQuests;
}

TArray<UIQuest*>& AIBaseCharacter::GetUncollectedRewardQuests_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, UncollectedRewardQuests, this);
	return UncollectedRewardQuests;
}

TArray<FLocationProgress>& AIBaseCharacter::GetLocationsProgress_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, LocationsProgress, this);
	return LocationsProgress;
}

void AIBaseCharacter::ClientLeaveCavePrompt_Implementation(const FString& Prompt, bool bCanLeave, bool bHatchlingCave)
{
	if (AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this)))
	{
		FAsyncWindowOpenedDelegate OnOpen;
		OnOpen.BindUObject(this, &AIBaseCharacter::CavePromptLoaded, Prompt, bCanLeave, bHatchlingCave);

		// if map view open, close it correctly to renable input
		if (IGameHUD->bMapWindowActive)
		{
			IGameHUD->HideMapMenu();
		}

		// if interaction menu is open, close it correctly to reenable input
		if (IGameHUD->bInteractMenuActive)
		{
			IGameHUD->HideAndCancelInteractionMenu();
		}

		if (Handle_CavePromptWidgetLoaded.IsValid() && Handle_CavePromptWidgetLoaded->IsLoadingInProgress())
		{
			Handle_CavePromptWidgetLoaded->CancelHandle();
			Handle_CavePromptWidgetLoaded = nullptr;
		}

		if (bCanLeave)
		{
			IGameHUD->CloseAllWindows();
			Handle_CavePromptWidgetLoaded = IGameHUD->OpenWindowAsyncHandle(HatchlingExitPromptYesNoClassPtr, true, true, false, false, true, OnOpen);
		}
		else
		{
			IGameHUD->CloseAllWindows();
			Handle_CavePromptWidgetLoaded = IGameHUD->OpenWindowAsyncHandle(HatchlingExitPromptClassPtr, true, true, false, false, true, OnOpen);
		}
		
	}

	bDesiresPreciseMovement = false;
}

void AIBaseCharacter::CavePromptLoaded(UIUserWidget* IUserWidget, FString Prompt, bool bCanLeave, bool bHatchlingCave)
{
	Handle_CavePromptWidgetLoaded = nullptr;
	
	UIMessageBox* const IMessageBox = Cast<UIMessageBox>(IUserWidget);
	if (!IMessageBox)
	{
		return;
	}

	if (bHatchlingCave)
	{
		const FText Title = FText::FromStringTable(ST_QuestNames, KEY_TutorialExitPromptTitle);
		const FText Message = FText::FromStringTable(ST_QuestNames, Prompt);
		IMessageBox->SetTextAndTitle(Title, Message);
		IMessageBox->SetYesButtonText(FText::FromStringTable(ST_QuestNames, KEY_TutorialExitPromptYes));
		IMessageBox->OnMessageBoxClosed.AddDynamic(this, &AIBaseCharacter::LeaveHatchlingCavePromptClosed);
	}
	else
	{
		const FText Title = FText::FromStringTable(ST_HomeCave, KEY_ExitHomeCaveTitle);
		const FText Message = FText::FromStringTable(ST_HomeCave, Prompt);
		IMessageBox->SetTextAndTitle(Title, Message);
		IMessageBox->SetYesButtonText(FText::FromStringTable(ST_HomeCave, KEY_ExitHomeCave));
		IMessageBox->OnMessageBoxClosed.AddDynamic(this, &AIBaseCharacter::LeaveHomeCavePromptClosed);
	}

	TWeakObjectPtr<UIMessageBox> IMessageBoxWeak = IMessageBox;
	OnExitInstance.AddWeakLambda(this, [IMessageBoxWeak]()
		{
			if (!IMessageBoxWeak.IsValid())
			{
				return;
			}
			IMessageBoxWeak->CloseWithResult(EMessageBoxResult::No);
		});
}

void AIBaseCharacter::SetActualGrowth(float Growth)
{
	ActualGrowth = Growth;
	bNoGrowthSave = true;
}

void AIBaseCharacter::SetGrowthUsedLast(bool bGrowthUsed)
{
	bGrowthUsedLast = bGrowthUsed;
}

void AIBaseCharacter::LeaveHatchlingCavePromptClosed(EMessageBoxResult Result)
{
	if (Result == EMessageBoxResult::Yes)
	{
		ServerTryLeaveHatchlingCave();
	}
	else 
	{
		//ServerResetPositionHatchlingCave();
	}
}

void AIBaseCharacter::LeaveHomeCavePromptClosed(EMessageBoxResult Result)
{
	if (Result == EMessageBoxResult::Yes)
	{
		ServerExitInstance(FTransform());
	}
	else
	{
		bAwaitingCaveResponse = false;
	}
}

void AIBaseCharacter::ServerResetPositionHatchlingCave_Implementation()
{
	if (AIHatchlingCave* IHatchlingCave = Cast<AIHatchlingCave>(GetCurrentInstance()))
	{
		FTransform Teleport;
		if (IHatchlingCave->FindSpawnLocation(Teleport, this))
		{
			ServerBetterTeleport(Teleport.GetLocation(), Teleport.GetRotation().Rotator(), true);
		}
	}
}

void AIBaseCharacter::AwardHomeCaveDecoration(const FQuestHomecaveDecorationReward& HomecaveDecorationReward)
{
	check (!HomecaveDecorationReward.Decoration.IsNull() && HomecaveDecorationReward.Amount > 0);
	if (HomecaveDecorationReward.Decoration.IsNull()) return;
	if (HomecaveDecorationReward.Amount <= 0) return;

	if (HomecaveDecorationReward.Decoration.Get())
	{
		HomeCaveDecorationAwardLoaded(HomecaveDecorationReward);
	}
	else
	{
		FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
		AssetLoader.RequestAsyncLoad
		(
			HomecaveDecorationReward.Decoration.ToSoftObjectPath(),
			FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::HomeCaveDecorationAwardLoaded, HomecaveDecorationReward),
			FStreamableManager::AsyncLoadHighPriority, false
		);
	}
}

void AIBaseCharacter::HomeCaveDecorationAwardLoaded(FQuestHomecaveDecorationReward HomecaveDecorationReward)
{
	check(HomecaveDecorationReward.Amount > 0);
	if (HomecaveDecorationReward.Amount <= 0) return;
	if (UHomeCaveDecorationDataAsset* DataAsset = HomecaveDecorationReward.Decoration.Get())
	{
		AddOwnedDecoration(DataAsset, HomecaveDecorationReward.Amount);
		ClientOnHomeCaveDecorationAward(DataAsset->IconTexture, DataAsset->DecorationName, HomecaveDecorationReward.Amount);
	}
}

void AIBaseCharacter::ClientOnHomeCaveDecorationAward_Implementation(const TSoftObjectPtr<UTexture2D>& IconTexture, const FText& Name, int Amount)
{
	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	check (IGameHUD);
	if (!IGameHUD)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ClientOnHomeCaveDecorationAward_Implementation: IGameHUD null"));
		return;
	}

	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("Amount"), Amount);
	Arguments.Add(TEXT("Name"), Name);

	IGameHUD->QueueToast(IconTexture, FText::FromStringTable(TEXT("ST_Quests"), TEXT("HomecaveDecorationReward")), FText::Format(FText::FromStringTable(TEXT("ST_Quests"), TEXT("HomecaveDecorationRewardDesc")), Arguments));
}

void AIBaseCharacter::ClientRequestChatEnabledForQuest_Implementation()
{
#if PLATFORM_CONSOLE
	// If Parental Controls restrict chatting then we should auto complete the quest
	UIGameInstance* IGameInstance = Cast<UIGameInstance>(UIGameplayStatics::GetIGameInstance(this));
	if (IGameInstance && !IGameInstance->IsOnlineCommunicationAllowed())
	{
		ServerSubmitGenericTask(NAME_SendChatMessage);
		return;
	}
#endif
	
	// If chat is disabled then we can autocomplete this quest
	UIGameUserSettings* UserSettings = Cast<UIGameUserSettings>(GEngine->GameUserSettings);
	if (UserSettings && !UserSettings->bShowChat)
	{
		ServerSubmitGenericTask(NAME_SendChatMessage);
		return;
	}

	// If player is muted then autocomplete the quest
	bool bMuted = (IAlderonRemoteConfig::GetMuteInfo().bMuted);

	AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
	if (IPlayerState)
	{
		if (IPlayerState->IsServerMuted())
		{
			bMuted = true;
		}
	}

	if (bMuted)
	{
		ServerSubmitGenericTask(NAME_SendChatMessage);
		return;
	}
	
}

void AIBaseCharacter::StartUpdateNearestItem()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_NearestItem)) return;

	GetWorldTimerManager().SetTimer(TimerHandle_NearestItem, this, &AIBaseCharacter::UpdateNearestItem, 3.0f, true);
}

void AIBaseCharacter::StopUpdateNearestItem()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_NearestItem);

	// Remove Old Marker
	if (FMapMarkerInfo() != SurvivalMapMarker)
	{
		NearestItem = nullptr;
		UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(SurvivalMapMarker, true, true);
	}
}

void AIBaseCharacter::UpdateNearestItem()
{
	SCOPE_CYCLE_COUNTER(STAT_UpdateNearestItem);

	if (!ShouldUpdateNearestItem())
	{
		// Remove Old Marker
		if (FMapMarkerInfo() != SurvivalMapMarker)
		{
			NearestItem = nullptr;
			UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(SurvivalMapMarker, true, true);
		}

		NearestItem = nullptr;
		SurvivalMapMarker = FMapMarkerInfo();
		return;
	}

	AIPlayerController* IPC = UIGameplayStatics::GetIPlayerController(this);
	if (!IPC) return;

	FName SurvivalQuestTag = NAME_None;

	UIQuest* SurvivalQuest = nullptr;
	for (UIQuest* ActiveQuest : GetActiveQuests())
	{
		if (!ActiveQuest || !ActiveQuest->QuestData || ActiveQuest->IsCompleted()) continue;
		if (ActiveQuest->QuestData->QuestShareType != EQuestShareType::Survival) continue;

		for (UIQuestBaseTask* IQuestBaseTask : ActiveQuest->GetQuestTasks())
		{
			if (!IQuestBaseTask) continue;

			if (UIQuestPersonalStat* IPersonalStatTask = Cast<UIQuestPersonalStat>(IQuestBaseTask))
			{
				SurvivalQuest = ActiveQuest;
				SurvivalQuestTag = IPersonalStatTask->Tag;
				break;
			}
		}

		if (SurvivalQuestTag != NAME_None) break;
	}

	if (SurvivalQuestTag != NAME_Food)
	{
		StopUpdateNearestItem();
		return;
	}

	AActor* NewNearestItem = nullptr;

	const bool bNearestItemValid = IsValid(NearestItem);
	const FVector PlayerLocation = GetActorLocation();
	int32 NearestItemDistance = bNearestItemValid ? (NearestItem->GetActorLocation() - GetActorLocation()).Size() : 99999999;
	FVector NearestItemLocation;
	if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
	{
		// Get Nearest Item
		for (AActor* ActorItem : IPC->UsableNearbyItems)
		{
			if (!IsValid(ActorItem)) continue;

			if (ActorItem->GetClass()->ImplementsInterface(UCarryInterface::StaticClass()) && ICarryInterface::Execute_IsCarried(ActorItem)) continue;

			// Only show nearby items within world bounds
			const FVector ActorLocation = ActorItem->GetActorLocation();
			if (!IWorldSettings->IsInWorldBounds(ActorLocation)) continue;

			if (AIFoodItem* IFoodItem = Cast<AIFoodItem>(ActorItem))
			{
				if (!IFoodItem->HasFoodTag(this)) continue;
				if (!IFoodItem || !IFoodItem->IsVisible()) continue;
				if (IFoodItem->GetFoodValue() <= 0.0f) continue;
			}
			else if (AIFish* IFish = Cast<AIFish>(ActorItem))
			{
				if (!IFish->IsEdible(this)) continue;
			}
			else if (AIMeatChunk* IMeatChunk = Cast<AIMeatChunk>(ActorItem))
			{
				if (IMeatChunk->GetFoodValue() <= 0.0f) continue;
			}
			else if (AIAlderonCritterBurrowSpawner* ICritterBurrow = Cast<AIAlderonCritterBurrowSpawner>(ActorItem))
			{
				if (!IUsableInterface::Execute_CanPrimaryBeUsedBy(ICritterBurrow, this)) continue;
				if (!ICritterBurrow->CanDinosaurEatCritters(this)) continue;
			}

			int32 Distance = (PlayerLocation - ActorLocation).Size();
			if (NearestItemDistance > Distance)
			{
				NewNearestItem = ActorItem;
				NearestItemDistance = Distance;
				NearestItemLocation = ActorItem->GetActorLocation();
			}
		}
	}

	if (NearestItem != NewNearestItem && NewNearestItem && SurvivalQuest)
	{
		// Remove Old Marker
		if (FMapMarkerInfo() != SurvivalMapMarker)
		{
			NearestItem = nullptr;
			UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(SurvivalMapMarker, true, true);
		}

		NearestItem = NewNearestItem;
		SurvivalMapMarker = FMapMarkerInfo(GetPlayerState<AIPlayerState>(), NearestItemLocation);

		// Add New Marker
		UIGameplayStatics::GetIPlayerController(this)->ClientAddQuestMapMarker(SurvivalMapMarker, SurvivalQuest, SurvivalQuest->QuestData);
	}
	else if (!NearestItem)
	{
		// Remove Old Marker
		if (FMapMarkerInfo() != SurvivalMapMarker)
		{
			NearestItem = nullptr;
			UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(SurvivalMapMarker, true, true);
		}
	}
	else if (bNearestItemValid)
	{
		// Remove Old Marker as it has moved
		if (FMapMarkerInfo() != SurvivalMapMarker && SurvivalMapMarker.WorldLocation != NearestItem->GetActorLocation())
		{
			UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(SurvivalMapMarker, true, true);
		}

		SurvivalMapMarker = FMapMarkerInfo(GetPlayerState<AIPlayerState>(), NearestItem->GetActorLocation());
		UIGameplayStatics::GetIPlayerController(this)->ClientAddQuestMapMarker(SurvivalMapMarker, SurvivalQuest, SurvivalQuest->QuestData);
	}
}

bool AIBaseCharacter::ShouldUpdateNearestItem()
{
	return GetGrowthPercent() <= 0.5f;
}

void AIBaseCharacter::StartUpdateNearestBurrow()
{
	if (GetWorldTimerManager().IsTimerActive(TimerHandle_NearestBurrow)) return;

	GetWorldTimerManager().SetTimer(TimerHandle_NearestBurrow, this, &AIBaseCharacter::UpdateNearestBurrow, 3.0f, true);
}

void AIBaseCharacter::StopUpdateNearestBurrow()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_NearestBurrow);

	// Remove Old Markers
	for (FMapMarkerInfo MapMarker : CritterBurrowMapMarkers) 
	{
		UIGameplayStatics::GetIPlayerController(this)->ClientRemoveUserMapMarker(MapMarker, true, true);
	}
}

AActor* AIBaseCharacter::FindNearestBurrow(const TArray<FPrimaryAssetId>& CritterIds)
{
	AIPlayerController* IPC = UIGameplayStatics::GetIPlayerController(this);
	if (!IPC) return nullptr;
	
	AActor* NearestBurrow = nullptr;
	const FVector PlayerLocation = GetActorLocation();
	int32 NearestBurrowDistance = 99999999;
	FVector NearestBurrowLocation;
	if (AIWorldSettings* IWorldSettings = Cast<AIWorldSettings>(GetWorldSettings()))
	{
		// Get Nearest Item
		for (AActor* ActorItem : IPC->UsableNearbyItems)
		{
			if (!IsValid(ActorItem)) continue;

			if (ActorItem->GetClass()->ImplementsInterface(UCarryInterface::StaticClass()) && ICarryInterface::Execute_IsCarried(ActorItem)) continue;

			// Only show nearby items within world bounds
			const FVector ActorLocation = ActorItem->GetActorLocation();
			if (!IWorldSettings->IsInWorldBounds(ActorLocation)) continue;

			bool bContainsCritter = false;
			if (AIAlderonCritterBurrowSpawner* ICritterBurrow = Cast<AIAlderonCritterBurrowSpawner>(ActorItem))
			{
				for (const FPrimaryAssetId& CritterId : CritterIds)
				{
					if (ICritterBurrow->GetSpawnableCritterIds().Contains(CritterId))
					{
						bContainsCritter = true;
						break;
					}
				}
			}
			else
			{
				continue;
			}

			if (!bContainsCritter) continue;

			int32 Distance = (PlayerLocation - ActorLocation).Size();
			if (NearestBurrowDistance > Distance)
			{
				NearestBurrow = ActorItem;
				NearestBurrowDistance = Distance;
				NearestBurrowLocation = ActorItem->GetActorLocation();
			}
		}
	}

	return NearestBurrow;
}

void AIBaseCharacter::UpdateNearestBurrow()
{
	//SCOPE_CYCLE_COUNTER(STAT_UpdateNearestBurrow);

	if (!ShouldUpdateNearestBurrow())
	{
		return;
	}
	
	AIPlayerController* IPC = UIGameplayStatics::GetIPlayerController(this);
	if (!IPC) return;

	// Remove Old Markers
	for (FMapMarkerInfo MapMarker : CritterBurrowMapMarkers)
	{
		IPC->ClientRemoveUserMapMarker(MapMarker, true, true);
	}

	bool BurrowQuestFound = false;
	TArray<FPrimaryAssetId> CritterIds;
	for (UIQuest* ActiveQuest : GetActiveQuests())
	{
		if (!ActiveQuest || !ActiveQuest->QuestData || ActiveQuest->IsCompleted()) continue;

		for (UIQuestBaseTask* IQuestBaseTask : ActiveQuest->GetQuestTasks())
		{
			if (!IQuestBaseTask) continue;
			if (IQuestBaseTask->IsCompleted()) continue;

			if (UIQuestKillTask* IKillTask = Cast<UIQuestKillTask>(IQuestBaseTask))
			{
				if (!IKillTask->bIsCritter) continue;
				BurrowQuestFound = true;

				if (ActiveQuest->QuestData->bInOrderQuestTasks && IQuestBaseTask != ActiveQuest->GetActiveTask()) continue;

				AActor* NearestBurrow = FindNearestBurrow(IKillTask->GetCharacterAssetIds());
				if (!NearestBurrow) continue;

				// Add New Marker
				FMapMarkerInfo MapMarker = FMapMarkerInfo(GetPlayerState<AIPlayerState>(), NearestBurrow->GetActorLocation());
				IPC->ClientAddQuestMapMarker(MapMarker, ActiveQuest, ActiveQuest->QuestData);
				CritterBurrowMapMarkers.Add(MapMarker);
				break;
			}
		}
	}

	if (!BurrowQuestFound)
	{
		StopUpdateNearestBurrow();
	}
}

bool AIBaseCharacter::ShouldUpdateNearestBurrow()
{
	return bHasCritterQuest;
}

void AIBaseCharacter::UpdateHomeCaveDistanceTracking(bool bHomecaveInRange)
{
	if (!UIGameplayStatics::AreHomeCavesEnabled(this))
	{
		return;
	}

	// Remove Gameplay Effect as we no longer have a nearby homecave
	if (!bHomecaveInRange && GE_HomecaveCampingDebuff.IsValid())
	{
		ToggleHomecaveCampingDebuff(false);
	}

	if (bHomecaveInRange != bHomecaveNearby)
	{
		bHomecaveNearby = bHomecaveInRange;
		HomecaveNearbyInitialTimestamp = GetWorld()->TimeSeconds;
	}
	else if (bHomecaveInRange && !GE_HomecaveCampingDebuff.IsValid())
	{
		int32 HomecaveCampingDelay = 180;

		UIGameInstance* GI = Cast<UIGameInstance>(GetGameInstance());
		check(GI);

		if (AIGameSession* Session = Cast<AIGameSession>(GI->GetGameSession()))
		{
			if (Session->bOverrideHomecaveCampingDelay)
			{
				HomecaveCampingDelay = Session->HomecaveCampingDelay;
			}
		}

		if (HomecaveNearbyInitialTimestamp != -1.0f && (GetWorld()->TimeSeconds >= (HomecaveNearbyInitialTimestamp + HomecaveCampingDelay)))
		{
			ToggleHomecaveCampingDebuff(true);
		}
	}
}

void AIBaseCharacter::UpdateCampingHomecaveData(bool bWasCampingHomecave)
{
	if (MapName.IsEmpty()) return;
	
	for (FCampingHomecaveData& CampingHomecave: CampingHomecaveData)
	{
		if (CampingHomecave.MapName == MapName)
		{
			CampingHomecave.bWasCampingHomecave = bWasCampingHomecave;
			return;
		}
	}

	CampingHomecaveData.Add(FCampingHomecaveData(MapName, bWasCampingHomecave));
}

bool AIBaseCharacter::IsCampingHomecave()
{
	if (!UIGameplayStatics::AreHomeCavesEnabled(this))
	{
		return false;
	}

	if (MapName.IsEmpty()) return false;
	
	for (const FCampingHomecaveData& CampingHomecave : CampingHomecaveData)
	{
		if (CampingHomecave.MapName == MapName)
		{
			return CampingHomecave.bWasCampingHomecave;
		}
	}
	return false;
}

void AIBaseCharacter::OnEnteringHomecave()
{
	ToggleHomecaveExitbuff(true);
}

void AIBaseCharacter::ToggleInHomecaveEffect(bool bEnable)
{
	if (bEnable && !UIGameplayStatics::AreHomeCavesEnabled(this))
	{
		return;
	}

	if (bEnable)
	{
		if (GE_InHomecaveEffect.IsValid()) return;
		
		UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
		if (PAGS.InHomecaveEffect != nullptr)
		{
			UGameplayEffect* InHomecaveEffect = PAGS.InHomecaveEffect->GetDefaultObject<UGameplayEffect>();
			GE_InHomecaveEffect = AbilitySystem->ApplyGameplayEffectToSelf(InHomecaveEffect, 1.0f, AbilitySystem->MakeEffectContext());
		    
			// Pause gameplay effects when inside Homecave
			if (AbilitySystem->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(FName(TEXT("Buff.InHomecave")))))
			{
				RemoveAllGameplayEffectAndAddToPausedList();
			}
		}
	}
	else
	{
		if (AbilitySystem && GE_InHomecaveEffect.IsValid())
		{
			AbilitySystem->RemoveActiveGameplayEffect(GE_InHomecaveEffect);
			GE_InHomecaveEffect.Invalidate();
		}
	}
}

void AIBaseCharacter::OnRep_PausedGameplayEffectsList()
{
	OnUIPausedEffectCreated.Broadcast();
}

void AIBaseCharacter::RemoveAllGameplayEffectAndAddToPausedList()
{
	// remove all gameplay effects with tag
	static TArray<FGameplayTag> ListOfTagsToPause = {
		FGameplayTag::RequestGameplayTag("Debuff"),
		FGameplayTag::RequestGameplayTag("Active"),
		FGameplayTag::RequestGameplayTag("Cooldown")
	};

	static FGameplayTagContainer TagsToPause = FGameplayTagContainer::CreateFromArray(ListOfTagsToPause);
	static FGameplayEffectQuery const QueryToFindEffect = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(TagsToPause);

	const TArray<FActiveGameplayEffectHandle> AllActiveGE{ AbilitySystem->GetActiveEffects(QueryToFindEffect) };

	for (const FActiveGameplayEffectHandle& ActiveGEHandle : AllActiveGE)
	{
		// check if gameplay effect is duration based
		float RemainingDuration = AbilitySystem->GetActiveGameplayEffect(ActiveGEHandle)->GetTimeRemaining(GetWorld()->GetTimeSeconds());
		if (RemainingDuration > 0.f && RemainingDuration != INFINITY)
		{
			// locally remove the effect and save the state
			AddEffectToPausedList(ActiveGEHandle, RemainingDuration);
		}
	}

	if (!IsRunningDedicatedServer() && !PausedGameplayEffectsList.IsEmpty())
	{
		OnUIPausedEffectCreated.Broadcast();
	}
}

void AIBaseCharacter::ReassignAllGameplayEffectFromPausedList()
{
	// reapply all the gameplay effects
	for (const FSavedGameplayEffectData& SinglePausedEffect : PausedGameplayEffectsList)
	{
		if (SinglePausedEffect.Effect)
		{
			RemoveEffectFromPausedList(SinglePausedEffect.Effect, SinglePausedEffect.Duration);
		}
	}

	PausedGameplayEffectsList.Empty();
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, PausedGameplayEffectsList, this);
}

void AIBaseCharacter::AddEffectToPausedList(FActiveGameplayEffectHandle ActiveGEHandle, float RemainingTime)
{
	const UGameplayEffect* GEInstance = AbilitySystem->GetGameplayEffectCDO(ActiveGEHandle);

	if (!GEInstance)
	{
		return;
	}

	const FString& ActiveEffectName = GEInstance->GetName();

	// if list already has the data, then modify it
	FSavedGameplayEffectData* FoundPausedEffect = PausedGameplayEffectsList.FindByPredicate([ActiveEffectName](const FSavedGameplayEffectData& SinglePausedEffect) {
		return SinglePausedEffect.Effect->GetName() == ActiveEffectName;
	});

	if (FoundPausedEffect)
	{
		FoundPausedEffect->Duration = RemainingTime;
	}
	else
	{
		PausedGameplayEffectsList.Add(FSavedGameplayEffectData(GEInstance, RemainingTime));
	}

	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, PausedGameplayEffectsList, this);

	if (AbilitySystem && ActiveGEHandle.IsValid())
	{
		AbilitySystem->RemoveActiveGameplayEffect(ActiveGEHandle);
		ActiveGEHandle.Invalidate();
	}
}

void AIBaseCharacter::RemoveEffectFromPausedList(const UGameplayEffect* Effect, float RemainingTime)
{
	// Apply effect as usual
	const FActiveGameplayEffectHandle NewHandle = AbilitySystem->ApplyGameplayEffectToSelf(Effect, 1.f, AbilitySystem->MakeEffectContext());
	AbilitySystem->OnUIPausedEffectRemoved.Broadcast(Effect);

	if (!NewHandle.IsValid())
	{
		return;
	}

	float DurationDiff = RemainingTime - AbilitySystem->GetGameplayEffectDuration(NewHandle);
	AbilitySystem->ModifyActiveGameplayEffectDurationByHandle(NewHandle, DurationDiff);
	const FActiveGameplayEffect* ActiveEffect = AbilitySystem->GetActiveGameplayEffect(NewHandle);
	AbilitySystem->Client_OnEffectRemoved(*ActiveEffect);

	// Check if effect was a CD, then apply to ability
	TArray<FGameplayAbilitySpecHandle> AllAbilities{};
	AbilitySystem->GetAllAbilities(AllAbilities);

	for (const FGameplayAbilitySpecHandle& SingleAbilitySpec : AllAbilities)
	{
		UGameplayAbility* SingleAbility = AbilitySystem->FindAbilitySpecFromHandle(SingleAbilitySpec)->Ability;
		if (SingleAbility->GetCooldownGameplayEffect() == Effect)
		{
			AbilitySystem->ApplyAbilityCooldown(SingleAbility->GetClass(), RemainingTime);
			return;
		}
	}
}

void AIBaseCharacter::ToggleHomecaveCampingDebuff(bool bEnable)
{
	if (bEnable && !UIGameplayStatics::AreHomeCavesEnabled(this))
	{
		return;
	}

	UpdateCampingHomecaveData(bEnable);

	if (bEnable)
	{
		UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
		if (PAGS.HomecaveCampingDebuffEffect != nullptr)
		{
			UGameplayEffect* HomecaveCampingDebuffEffect = PAGS.HomecaveCampingDebuffEffect->GetDefaultObject<UGameplayEffect>();
			GE_HomecaveCampingDebuff = AbilitySystem->ApplyGameplayEffectToSelf(HomecaveCampingDebuffEffect, 1.0f, AbilitySystem->MakeEffectContext());
		}
	}
	else
	{
		if (AbilitySystem && GE_HomecaveCampingDebuff.IsValid())
		{
			AbilitySystem->RemoveActiveGameplayEffect(GE_HomecaveCampingDebuff);
			GE_HomecaveCampingDebuff.Invalidate();
		}
	}
}

void AIBaseCharacter::ToggleHomecaveExitbuff(bool bEnable)
{
	if (bEnable && !UIGameplayStatics::AreHomeCavesEnabled(this))
	{
		return;
	}

	if (bEnable)
	{
		UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
		if (PAGS.HomecaveExitBuffEffect != nullptr)
		{
			UGameplayEffect* HomecaveExitBuffEffect = PAGS.HomecaveExitBuffEffect->GetDefaultObject<UGameplayEffect>();

			// Apply HomecaveExitBuff effect to yourself
			GE_HomecaveExitBuff = AbilitySystem->ApplyGameplayEffectToSelf(HomecaveExitBuffEffect, 1.0f, AbilitySystem->MakeEffectContext());
		}
	}
	else
	{
		//If the homecave buff is not valid, check the list of current effects and see if we can find it in there.
		//This can happen if you log back in with the buff still applied. The GE_HomecaveExitBuff variable looses reference to the effect.
		if (AbilitySystem && !GE_HomecaveExitBuff.IsValid())
		{
			UPOTAbilitySystemGlobals& PAGS = UPOTAbilitySystemGlobals::Get();
			if (PAGS.HomecaveExitBuffEffect != nullptr)
			{
				TArray<FActiveGameplayEffectHandle> ActiveEffects = AbilitySystem->GetActiveEffects(FGameplayEffectQuery());
				UGameplayEffect* HomecaveExitBuffEffect = PAGS.HomecaveExitBuffEffect->GetDefaultObject<UGameplayEffect>();

				for (FActiveGameplayEffectHandle Handle : ActiveEffects)
				{
					if (AbilitySystem->GetGameplayEffectDefForHandle(Handle) == HomecaveExitBuffEffect)
					{
						GE_HomecaveExitBuff = Handle;
						break;
					}
				}
			}
		}

		if (AbilitySystem && GE_HomecaveExitBuff.IsValid())
		{
			AbilitySystem->RemoveActiveGameplayEffect(GE_HomecaveExitBuff);
			GE_HomecaveExitBuff.Invalidate();
		}
	}
}

FHomeCaveDecorationSaveInfo* AIBaseCharacter::GetSavedDecoration(FGuid Guid)
{
	if (!Guid.IsValid()) return nullptr;

	for (FHomeCaveDecorationSaveInfo& SaveInfo : GetHomeCaveSaveableInfo_Mutable().SavedDecorations)
	{
		if (SaveInfo.Guid == Guid)
		{
			return &SaveInfo;
		}
	}

	return nullptr;
}

void AIBaseCharacter::RemoveSavedDecoration(FGuid Guid)
{
	bool bNeedsDirtying = false;
	
	for (int Index = 0; Index < GetHomeCaveSaveableInfo().SavedDecorations.Num(); Index++)
	{
		if (GetHomeCaveSaveableInfo().SavedDecorations[Index].Guid == Guid)
		{
			HomeCaveSaveableInfo.SavedDecorations.RemoveAt(Index);
			bNeedsDirtying = true;
		}
	}

	if (bNeedsDirtying)
	{
		MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, HomeCaveSaveableInfo, this);
	}
}

void AIBaseCharacter::QueueTutorialPrompt(const FTutorialPrompt& NewTutorialPrompt)
{
	TutorialPrompt = NewTutorialPrompt;
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, TutorialPrompt, this);

	if (IsLocallyControlled() && !IsRunningDedicatedServer())
	{
		OnRep_TutorialPrompt();
	}
}


void AIBaseCharacter::OnRep_TutorialPrompt()
{
	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	check(IGameHUD);
	if (!IGameHUD)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::OnRep_TutorialPrompt: IGameHUD null"));
		return;
	}

	UIMainGameHUDWidget* IMainGameHUDWidget = Cast<UIMainGameHUDWidget>(IGameHUD->ActiveHUDWidget);
	check(IMainGameHUDWidget);
	if (!IMainGameHUDWidget)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::OnRep_TutorialPrompt: IMainGameHUDWidget null"));
		return;
	}

	if (GetTutorialPrompt().IQuest == nullptr)
	{
		IMainGameHUDWidget->HideTutorialPromptWidget();
		return;
	}

	IMainGameHUDWidget->ShowTutorialPrompt(GetTutorialPrompt());
}


void AIBaseCharacter::StartTrackingHomeCave(UIQuest* IQuest)
{
	CurrentHomeCaveTrackingQuest = IQuest;
	GetWorldTimerManager().SetTimer(TimerHandle_HomeCaveTracking, this, &AIBaseCharacter::UpdateHomeCaveTracking, 5.0f, true, .0f);
}

void AIBaseCharacter::ClientStopTrackingHomeCave_Implementation()
{
	CurrentHomeCaveTrackingQuest = nullptr;
	if (HomeCaveTrackingMarker != FMapMarkerInfo())
	{
		AIPlayerController* IPlayerController = GetController<AIPlayerController>();
		if (!IPlayerController)
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateHomeCaveTracking: IPlayerController null"));
		}
		else
		{
			IPlayerController->ClientRemoveUserMapMarker(HomeCaveTrackingMarker, true, true);
		}

		HomeCaveTrackingMarker = FMapMarkerInfo();
	}
	GetWorldTimerManager().ClearTimer(TimerHandle_HomeCaveTracking);
}

void AIBaseCharacter::UpdateHomeCaveTracking()
{
	AIPlayerController* IPlayerController = GetController<AIPlayerController>();
	if (!IPlayerController)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::UpdateHomeCaveTracking: IPlayerController null")));
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateHomeCaveTracking: IPlayerController null"));
		ClientStopTrackingHomeCave();
		return;
	}

	if (HomeCaveTrackingMarker != FMapMarkerInfo())
	{
		IPlayerController->ClientRemoveUserMapMarker(HomeCaveTrackingMarker, true, true);
		HomeCaveTrackingMarker = FMapMarkerInfo();
	}

	check (CurrentHomeCaveTrackingQuest);
	if (!CurrentHomeCaveTrackingQuest)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::UpdateHomeCaveTracking: CurrentHomeCaveTrackingQuest null")));
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateHomeCaveTracking: CurrentHomeCaveTrackingQuest null"));
		ClientStopTrackingHomeCave();
		return;
	} 

	AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
	check (IWorldSettings);
	if (!IWorldSettings) return;

	AIHomeCaveManager* IHomeCaveManager = IWorldSettings->HomeCaveManager;
	if (!IHomeCaveManager)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::UpdateHomeCaveTracking: IHomeCaveManager null")));
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateHomeCaveTracking: IHomeCaveManager null"));
		ClientStopTrackingHomeCave();
		return;
	}

	FVector ClosestLocation = FVector::ZeroVector;
	bool bInstance = false;
	bool bAllowUnderwater = PlayerStartTag != NAME_Flyer && PlayerStartTag != NAME_Land;
	bool bOnlyUnderwater = PlayerStartTag == NAME_Aquatic;

	if (AIPlayerCaveMain* IPlayerCaveMain = Cast<AIPlayerCaveMain>(GetCurrentInstance()))
	{
		if (AIHomeRock* IHomeRock = IPlayerCaveMain->GetIHomeRock())
		{
			ClosestLocation = IHomeRock->GetActorLocation();
			bInstance = true;
		}
	}
	else
	{
		float ClosestDistance = TNumericLimits<float>::Max();

		for (FVector HomeCaveLocation : IHomeCaveManager->GetHomeCaveLocations(bAllowUnderwater, bOnlyUnderwater))
		{
			float Distance = FVector::Distance(HomeCaveLocation, GetActorLocation());
			if (Distance < ClosestDistance)
			{
				ClosestLocation = HomeCaveLocation;
				ClosestDistance = Distance;
			}
		}

		if (ClosestLocation == FVector::ZeroVector)
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, FString::Printf(TEXT("AIBaseCharacter::UpdateHomeCaveTracking: Could not find home cave location")));
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateHomeCaveTracking: Could not find home cave location"));
			ClientStopTrackingHomeCave();
			return;
		}
	}

	if (ClosestLocation != FVector::ZeroVector)
	{
		FMapMarkerInfo MapMarkerInfo;
		MapMarkerInfo.OverrideIcon = HomeCaveTrackingMapIconSoft;
		MapMarkerInfo.WorldLocation = ClosestLocation;
		MapMarkerInfo.bShowInInstance = bInstance;
		IPlayerController->ClientAddQuestMapMarker(MapMarkerInfo, CurrentHomeCaveTrackingQuest, CurrentHomeCaveTrackingQuest->QuestData);
		HomeCaveTrackingMarker = MapMarkerInfo;
	}
}

void AIBaseCharacter::StartTrackingWaystone(UIQuest* IQuest)
{
	CurrentWaystoneTrackingQuest = IQuest;
	GetWorldTimerManager().SetTimer(TimerHandle_WaystoneTracking, this, &AIBaseCharacter::UpdateWaystoneTracking, 5.0f, true, .0f);
}

void AIBaseCharacter::ClientStopTrackingWaystone_Implementation()
{
	CurrentWaystoneTrackingQuest = nullptr;
	if (WaystoneTrackingMarker != FMapMarkerInfo())
	{
		AIPlayerController* IPlayerController = GetController<AIPlayerController>();
		if (!IPlayerController)
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ClientStopTrackingWaystone_Implementation: IPlayerController null"));
		}
		else
		{
			IPlayerController->ClientRemoveUserMapMarker(WaystoneTrackingMarker, true, true);
		}

		WaystoneTrackingMarker = FMapMarkerInfo();
	}
	GetWorldTimerManager().ClearTimer(TimerHandle_WaystoneTracking);
}

void AIBaseCharacter::UpdateWaystoneTracking()
{
	AIPlayerController* IPlayerController = GetController<AIPlayerController>();
	if (!IPlayerController)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateWaystoneTracking: IPlayerController null"));
		ClientStopTrackingWaystone();
		return;
	}

	if (WaystoneTrackingMarker != FMapMarkerInfo())
	{
		IPlayerController->ClientRemoveUserMapMarker(WaystoneTrackingMarker, true, true);
		WaystoneTrackingMarker = FMapMarkerInfo();
	}

	check(CurrentWaystoneTrackingQuest);
	if (!CurrentWaystoneTrackingQuest)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateWaystoneTracking: CurrentWaystoneTrackingQuest null"));
		ClientStopTrackingWaystone();
		return;
	}

	AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this);
	check(IWorldSettings);
	if (!IWorldSettings) return;

	AIWaystoneManager* IWaystoneManager = IWorldSettings->WaystoneManager;
	if (!IWaystoneManager)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateWaystoneTracking: IWaystoneManager null"));
		ClientStopTrackingWaystone();
		return;
	}

	FVector ClosestLocation = FVector::ZeroVector;
	bool bInstance = false;

	if (!GetCurrentInstance())
	{
		float ClosestDistance = TNumericLimits<float>::Max();

		bool bAllowUnderwater = PlayerStartTag != NAME_Flyer && PlayerStartTag != NAME_Land;
		bool bOnlyUnderwater = PlayerStartTag == NAME_Aquatic;
		
		for (FVector HomeCaveLocation : IWaystoneManager->GetWaystoneLocations(bAllowUnderwater, bOnlyUnderwater))
		{
			float Distance = FVector::Distance(HomeCaveLocation, GetActorLocation());
			if (Distance < ClosestDistance)
			{
				ClosestLocation = HomeCaveLocation;
				ClosestDistance = Distance;
			}
		}

		if (ClosestLocation == FVector::ZeroVector)
		{
			UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::UpdateWaystoneTracking: Could not find Waystone location"));
			ClientStopTrackingWaystone();
			return;
		}

	}

	if (ClosestLocation != FVector::ZeroVector)
	{
		FMapMarkerInfo MapMarkerInfo;
		MapMarkerInfo.OverrideIcon = WaystoneTrackingMapIconSoft;
		MapMarkerInfo.WorldLocation = ClosestLocation;
		MapMarkerInfo.bShowInInstance = bInstance;
		IPlayerController->ClientAddQuestMapMarker(MapMarkerInfo, CurrentWaystoneTrackingQuest, CurrentWaystoneTrackingQuest->QuestData);
		WaystoneTrackingMarker = MapMarkerInfo;
	}
}

bool AIBaseCharacter::IsReadyToCheckPurchasableSkin()
{
	if (GetWorldTimerManager().TimerExists(TimerHandle_ShowSkinAvailableCooldown)) 
	{
		return false;
	}
	if (!HasLeftHatchlingCave()) 
	{
		return false;
	}

	return true;
}

void AIBaseCharacter::CheckForPurchasableSkin()
{
	if (!IsReadyToCheckPurchasableSkin()) return;

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableItems: IGameInstance nullptr"));
		return;	
	}

	// The LoadCharacterDataHandle keeps the CharacterDataAsset in memory until it is used in CheckForPurchasableSkin_PostSkinsLoad
	LoadCharacterDataHandle = IGameInstance->AsyncLoadCharacterData(CharacterDataAssetId, FCharacterDataAssetLoaded::CreateUObject(this, &AIBaseCharacter::CheckForPurchasableSkin_PostCharLoad), ESkinLoadType::All);
}

void AIBaseCharacter::CheckForPurchasableSkin_PostCharLoad(bool bSuccess, FPrimaryAssetId CharacterAssetId, UCharacterDataAsset* CharacterDataAsset)
{
	check (bSuccess);
	if (!bSuccess) 
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableSkin_PostCharLoad: !bSuccess"));
		LoadCharacterDataHandle.Reset();
		return;
	}

	check(CharacterDataAsset);
	if (!CharacterDataAsset)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableSkin_PostCharLoad: !CharacterDataAsset"));
		LoadCharacterDataHandle.Reset();
		return;
	}

	TArray<FPrimaryAssetId> SkinDataAssets;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	TArray<FPrimaryAssetId> ModSkins = UIGameplayStatics::GetIGameInstance(this)->GetModSkins(CharacterAssetId);

	SkinDataAssets.Reserve(CharacterDataAsset->Skins.Num() + ModSkins.Num());

	for (int i = 0; i < CharacterDataAsset->Skins.Num(); i++)
	{
		FPrimaryAssetId SkinAssetId = CharacterDataAsset->Skins[i];
		if (SkinAssetId.IsValid())
		{
			SkinDataAssets.Add(SkinAssetId);
		}
	}

	for (FPrimaryAssetId ModSkinAssetId : ModSkins)
	{
		SkinDataAssets.Add(ModSkinAssetId);
	}

	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableItems: IGameInstance nullptr"));
		return;
	}

	IGameInstance->AsyncLoadCharacterSkinData(CharacterDataAssetId, SkinDataAssets, FCharacterDataSkinLoaded::CreateUObject(this, &AIBaseCharacter::CheckForPurchasableSkin_PostSkinsLoad), ESkinLoadType::All);
}

void AIBaseCharacter::CheckForPurchasableSkin_PostSkinsLoad(bool bSuccess, FPrimaryAssetId CharacterAssetId, UCharacterDataAsset* CharacterDataAsset, TArray<FPrimaryAssetId> SkinAssetIds, TArray<USkinDataAsset*> SkinDataAssets, TSharedPtr<FStreamableHandle> Handle)
{
	check(bSuccess);
	if (!bSuccess)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableSkin_PostSkinsLoad: !bSuccess"));
		LoadCharacterDataHandle.Reset();
		return;
	}

	check(CharacterDataAsset);
	if (!CharacterDataAsset)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableSkin_PostSkinsLoad: !CharacterDataAsset"));
		LoadCharacterDataHandle.Reset();
		return;
	}

	AIPlayerController* IPlayerController = GetController<AIPlayerController>();
	check (IPlayerController);
	if (!IPlayerController)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::CheckForPurchasableSkin_PostSkinsLoad: !IPlayerController"));
		LoadCharacterDataHandle.Reset();
		return;
	}

	int HighestPrice = 0;
	USkinDataAsset* HighestPriceSkin = nullptr;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	// Find the highest price skin that is afforded
	for (const FPrimaryAssetId& SkinDataAssetId : CharacterDataAsset->Skins)
	{
		if (!SkinDataAssetId.IsValid()) continue;

		USkinDataAsset* SkinDataAsset = UIGameInstance::GetPrimaryAssetFromHandle<USkinDataAsset>(SkinDataAssetId, Handle);
		if (!SkinDataAsset) {
			SkinDataAsset = UIGameInstance::LoadSkinData(SkinDataAssetId);
			if (!SkinDataAsset) continue;
		}

		if (!SkinDataAsset->IsEnabled())
		{
			continue;
		}

		// Don't show the same skin twice
		if (ShownAvailableSkinAssets.Contains(SkinDataAsset))
		{
			if (SkinDataAsset->PurchaseCostMarks > HighestPrice)
			{
				HighestPrice = SkinDataAsset->PurchaseCostMarks;
				HighestPriceSkin = nullptr;
			}
			continue;
		}

		// Player must afford skin
		if (SkinDataAsset->PurchaseCostMarks > GetMarks()) continue;

		// Player must "Own" skin i.e. have required entitlement
		if (!UIGameInstance::PlayerOwnsSkin(SkinDataAsset)) continue;

		// Player must not have skin unlocked, so it can be purchased
		if (IPlayerController->IsSkinUnlocked(SkinDataAsset)) continue;

		if (SkinDataAsset->PurchaseCostMarks >= HighestPrice)
		{
			HighestPrice = SkinDataAsset->PurchaseCostMarks;
			HighestPriceSkin = SkinDataAsset;
		}
	}

	if (HighestPriceSkin)
	{
		ShowSkinAvailableForPurchaseToast(HighestPriceSkin);
	}

	LoadCharacterDataHandle.Reset();
}

void AIBaseCharacter::ShowSkinAvailableForPurchaseToast(USkinDataAsset* Skin)
{
	check(Skin);
	if (!Skin)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ShowSkinAvailableForPurchaseToast: !Skin"));
		return;
	}

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	check (IGameHUD);
	if (!IGameHUD)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ShowSkinAvailableForPurchaseToast: !IGameHUD"));
		return;
	}
	
	ShownAvailableSkinAssets.Add(Skin);

	// Set cooldown for 1 minutes between each toast
	GetWorldTimerManager().SetTimer(TimerHandle_ShowSkinAvailableCooldown, 60.f, false);
	IGameHUD->QueueToast(Skin->Icon, Skin->DisplayName, FText::FromStringTable(TEXT("ST_CharacterEdit"), TEXT("SkinAvailable")), Skin->PurchaseCostMarks);
}

void AIBaseCharacter::AsyncGetAllDinosaurAbilities(FDinosaurAbilitiesLoaded OnLoad)
{
	if (!OnLoad.IsBound()) return;

	FString SanitizedName = UIAbilitySlotsEditor::GetSanitizedDinosaurName(this);

	TArray<FPrimaryAssetId> AssetIds;
	TArray<FPrimaryAssetId> CurrentDinosaurAttacks;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());
	if (AssetManager.GetPrimaryAssetIdList(UTitanAssetManager::AbilityAssetType, AssetIds))
	{
		for (const FPrimaryAssetId& AbilityAssetId : AssetIds)
		{
			if (!AssetManager.GetPrimaryAssetPath(AbilityAssetId).ToString().Contains(SanitizedName))
			{
				continue;
			}

			UPOTAbilityAsset* AbilityAsset = AssetManager.ForceLoadAbility(AbilityAssetId);

			if (UIAbilitySlotsEditor::IsAbilityEnabled(AbilityAsset) &&
				UIAbilitySlotsEditor::CheckAbilityCompatibilityWithCharacter(this, AbilityAsset))
			{
				CurrentDinosaurAttacks.Add(AbilityAssetId);
			}
		}
	}
	
	FStreamableHandleDelegate Delegate = FStreamableHandleDelegate::CreateUObject(this, &AIBaseCharacter::AsyncGetAllDinosaurAbilities_PostLoad, CurrentDinosaurAttacks, OnLoad);
	AssetManager.PreloadPrimaryAssetsWithHandle(CurrentDinosaurAttacks, {"UI"}, true, Delegate);
}

void AIBaseCharacter::BP_AsyncGetAllDinosaurAbilities(FOnDinoAbilitiesLoadedDynamic OnLoad)
{
	// Blueprint compatible wrapper for AsyncGetAllDinosaurAbilities
	AsyncGetAllDinosaurAbilities(FDinosaurAbilitiesLoaded::CreateWeakLambda(this, [this, OnLoad](TArray<UPOTAbilityAsset*> Result)
		{
			OnLoad.ExecuteIfBound(Result, this);
		}));
}

void AIBaseCharacter::AsyncGetAllDinosaurAbilities_PostLoad(TSharedPtr<FStreamableHandle> Handle, TArray<FPrimaryAssetId> CurrentDinosaurAttacks, FDinosaurAbilitiesLoaded OnLoad)
{
	if (!OnLoad.IsBound()) return;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	TArray<UPOTAbilityAsset*> AbilityAssets;

	for (FPrimaryAssetId& AbilityAssetId : CurrentDinosaurAttacks)
	{
		UPOTAbilityAsset* AbilityDataAsset = UIGameInstance::GetPrimaryAssetFromHandle<UPOTAbilityAsset>(AbilityAssetId, Handle);
		check (AbilityDataAsset);
		if (!AbilityDataAsset) continue;

		AbilityAssets.Add(AbilityDataAsset);
	}

	OnLoad.Execute(AbilityAssets);

	if (Handle.IsValid()) 
	{
		Handle.Reset();
	}
}

bool AIBaseCharacter::IsReadyToCheckPurchasableAbility()
{
	if (GetWorldTimerManager().TimerExists(TimerHandle_ShowAbilityAvailableCooldown)) 
	{
		return false;
	}
	if (!HasLeftHatchlingCave()) 
	{
		return false;
	}
	return true;
}

void AIBaseCharacter::CheckForPurchasableAbility()
{
	if (!IsReadyToCheckPurchasableAbility()) return;

	AsyncGetAllDinosaurAbilities(FDinosaurAbilitiesLoaded::CreateUObject(this, &AIBaseCharacter::CheckForPurchasableAbility_PostAbilityLoad));
}

void AIBaseCharacter::CheckForPurchasableAbility_PostAbilityLoad(TArray<UPOTAbilityAsset*> Abilities)
{
	int HighestPrice = 0;
	UPOTAbilityAsset* HighestPriceAbility = nullptr;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	// Find the highest price skin that is afforded
	for (UPOTAbilityAsset* Ability : Abilities)
	{
		// Avoid showing same ability twice
		if (ShownAvailableAbilityAssets.Contains(Ability))
		{
			if (Ability->UnlockCost > HighestPrice)
			{
				HighestPrice = Ability->UnlockCost;
				HighestPriceAbility = nullptr;
			}
			continue;
		}

		// Can't afford
		if (Ability->UnlockCost > GetMarks()) continue;

		// Ability must not be unlocked already
		if (IsAbilityUnlocked(Ability->GetPrimaryAssetId())) continue;

		// Ability must not be disabled
		if (!Ability->IsEnabled()) continue;

		// ability must meet growth requirement
		if (!IsGrowthRequirementMetForAbility(Ability)) continue;

		if (Ability->UnlockCost >= HighestPrice)
		{
			HighestPrice = Ability->UnlockCost;
			HighestPriceAbility = Ability;
		}
	}

	if (HighestPriceAbility)
	{
		ShowAbilityAvailableForPurchaseToast(HighestPriceAbility);
	}
}

void AIBaseCharacter::ShowAbilityAvailableForPurchaseToast(UPOTAbilityAsset* Ability)
{
	check(Ability);
	if (!Ability)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ShowAbilityAvailableForPurchaseToast: !Ability"));
		return;
	}

	AIGameHUD* IGameHUD = Cast<AIGameHUD>(UIGameplayStatics::GetIHUD(this));
	check(IGameHUD);
	if (!IGameHUD)
	{
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ShowAbilityAvailableForPurchaseToast: !IGameHUD"));
		return;
	}

	ShownAvailableAbilityAssets.Add(Ability);

	if (UTexture2D* Texture = Cast<UTexture2D>(Ability->Icon.GetResourceObject()))
	{
		// Set cooldown for 1 minutes between each toast
		GetWorldTimerManager().SetTimer(TimerHandle_ShowAbilityAvailableCooldown, 60.f, false);
		IGameHUD->QueueToast(Texture, Ability->Name, FText::FromStringTable(TEXT("ST_CharacterEdit"), TEXT("AbilityAvailable")), Ability->UnlockCost);
	}
}

void AIBaseCharacter::SnapToGround()
{

	FVector FromLocation = GetActorLocation();
	FVector ToLocation = FromLocation - FVector{ 0, 0, 200 };
	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	CollisionQueryParams.AddIgnoredActor(this);
	TArray<FHitResult> HitResults;
	GetWorld()->LineTraceMultiByChannel(HitResults, FromLocation, ToLocation, COLLISION_DINOCAPSULE, CollisionQueryParams);

	FVector NewLocation = FromLocation;
	bool bFoundSafeLocation = false;
	for (FHitResult& Hit : HitResults)
	{
		if (Cast<ABlockingVolume>(Hit.GetActor())) continue;
		if (Hit.bBlockingHit && (Cast<ALandscapeProxy>(Hit.GetActor()) || Cast<AStaticMeshActor>(Hit.GetActor())))
		{
			NewLocation = Hit.ImpactPoint + FVector(0, 0, 5);
			bFoundSafeLocation = true;
			break;
		}
	}
	if (!bFoundSafeLocation)
	{
		return;
	}

	if (GetCapsuleComponent())
	{
		NewLocation += FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}

	// don't adjust if the newlocation is above the current player location
	if (NewLocation.Z > GetActorLocation().Z) return;

	SetActorLocation(NewLocation, false, nullptr, ETeleportType::ResetPhysics);
}

bool AIBaseCharacter::IsUsingAbility() const
{
	if (!AbilitySystem) return false;

	return AbilitySystem->HasAnyActiveGameplayAbility();
}


void AIBaseCharacter::ServerNotifyFinishedClientLoad_Implementation()
{
	// Release client after 1 second
	FTimerHandle Handle;
	TWeakObjectPtr<AIBaseCharacter> WeakThis = this;

	// handle removing combat log ai here instead of before client finishes
	AIGameMode* const IGameMode = UIGameplayStatics::GetIGameMode(this);
	if (IsCombatLogAI() && IGameMode)
	{
		IGameMode->RemoveCombatLogAI(GetCharacterID(), false, false);
		SetIsAIControlled(false);
	}

	FTimerDelegate Del = FTimerDelegate::CreateLambda([WeakThis]()
	{
		if (!WeakThis.Get()) return;
		WeakThis->SetPreloadingClientArea(false);
		WeakThis->ClearCombatLogRespawningUser();
	});

	check(GetWorld());
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(Handle, Del, 1.0f, false);
}

bool AIBaseCharacter::ServerNotifyFinishedClientLoad_Validate()
{
	return true;
}

void AIBaseCharacter::ClientLoadedAutoRecord()
{
	FTimerHandle Handle;
	TWeakObjectPtr<AIBaseCharacter> WeakThis = this;

	FTimerDelegate Del = FTimerDelegate::CreateLambda([WeakThis]()
		{
			if (!WeakThis.Get()) return;
			// Start Auto Recording the Replay if it is enabled in settings
			
			UIGameUserSettings* const GameUserSettings = Cast<UIGameUserSettings>(GEngine->GetGameUserSettings());
			UIGameInstance*	const IGameInstance = UIGameplayStatics::GetIGameInstance(WeakThis.Get());
			
			if (!GameUserSettings || !IGameInstance)
			{
				return;
			}
			
			// Calling GetCanRecord here with bNotifyUser = false since the first attempt can happen in the loading screen sometimes
			// dont want to send a toast with an error then another one with success
			UAlderonReplaySubsystem* const AlderonReplaySubsystem = IGameInstance->GetSubsystem<UAlderonReplaySubsystem>();
			if (AlderonReplaySubsystem && GameUserSettings->GetReplayAutoRecord() && AlderonReplaySubsystem->GetCanRecord(false))
			{
				WeakThis->StartAutoRecording();
			}
		});

	check(GetWorld());
	if (!GetWorld()) return;

	GetWorld()->GetTimerManager().SetTimer(Handle, Del, 1.0f, false);
}

void AIBaseCharacter::StartAutoRecording()
{
	if (UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this))
	{
		if (UAlderonReplaySubsystem* AlderonReplaySubsystem = IGameInstance->GetSubsystem<UAlderonReplaySubsystem>())
		{
			if (!AlderonReplaySubsystem->IsRecording())
			{
				IGameInstance->ReplayStartRecording(true);
			}
		}
	}
}

TArray<FSlottedAbilities>& AIBaseCharacter::GetSlottedAbilityAssetsArray_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, SlottedAbilityAssetsArray, this);
	return SlottedAbilityAssetsArray;
}

void AIBaseCharacter::PrepareCooldownsForSave()
{
	if (!bHasRestoredSavedCooldowns)
	{
		return;
	}

	SlottedAbilityCooldowns.Empty();

	if (!IsAlive())
	{
		return;
	}

	UPOTAbilitySystemComponent* POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!POTAbilitySystemComponent) return;

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	for (const FActionBarAbility& SlottedAbilityCategory : GetSlottedAbilityCategories())
	{
		FPrimaryAssetId SlottedAbility;
		if (GetAbilityForSlot(SlottedAbilityCategory, SlottedAbility))
		{
			UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(SlottedAbility);
			if (LoadedAbility)
			{
				float CooldownTime = POTAbilitySystemComponent->GetCooldownTimeRemaining(LoadedAbility->GrantedAbility);
				if (CooldownTime >= 1)
				{
					FDateTime Now = FDateTime::UtcNow();
					Now += FTimespan(0, 0, (int)CooldownTime);
					int64 UnixTime = Now.ToUnixTimestamp();
					
					FSavedAbilityCooldown NewSavedCooldown = FSavedAbilityCooldown();
					NewSavedCooldown.SlottedAbility = SlottedAbilityCategory;
					NewSavedCooldown.ExpirationUnixTime = UnixTime;
					SlottedAbilityCooldowns.Add(NewSavedCooldown);
				}
			}
		}
	}
}

void AIBaseCharacter::AddCooldownsAfterLoad()
{
	bHasRestoredSavedCooldowns = true;
	UPOTAbilitySystemComponent* POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!POTAbilitySystemComponent) return;

	if (SlottedAbilityCooldowns.Num() == 0)
	{
		return;
	}

	UTitanAssetManager& AssetManager = static_cast<UTitanAssetManager&>(UAssetManager::Get());

	FDateTime Now = FDateTime::UtcNow();
	int64 UnixTimeNow = Now.ToUnixTimestamp();
	Now += FTimespan(0, 0, 1);
	int64 UnixTimeOneSecond = Now.ToUnixTimestamp(); // Add 1 second to now time, because less than 1 second will be inaccurate

	for (const FSavedAbilityCooldown& SavedCooldown : SlottedAbilityCooldowns)
	{
		if (SavedCooldown.ExpirationUnixTime < UnixTimeOneSecond)
		{
			continue; // Already expired
		}
		FPrimaryAssetId SlottedAbility;
		if (GetAbilityForSlot(SavedCooldown.SlottedAbility, SlottedAbility))
		{
			UPOTAbilityAsset* LoadedAbility = AssetManager.ForceLoadAbility(SlottedAbility);
			if (LoadedAbility)
			{
				float SecondsRemaining = (float)(SavedCooldown.ExpirationUnixTime - UnixTimeNow);
				POTAbilitySystemComponent->ApplyAbilityCooldown(LoadedAbility->GrantedAbility, SecondsRemaining);
			}
		}
	}

	SlottedAbilityCooldowns.Empty();

}

void AIBaseCharacter::PrepareGameplayEffectsForSave()
{
	if (!bHasRestoredSavedEffects || !IsAlive())
	{
		return;
	}

	const UPOTAbilitySystemComponent* POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!POTAbilitySystemComponent)
	{
		return;
	}

	ActiveGameplayEffects.Empty();

	const TArray<FActiveGameplayEffectHandle> ActiveEffectHandles{ POTAbilitySystemComponent->GetActiveEffects(FGameplayEffectQuery()) }; //  ::MakeQuery_MatchNoOwningTags(TagsToIgnore)) };

	for (const FActiveGameplayEffectHandle ActiveEffectHandle : ActiveEffectHandles)
	{
		const FActiveGameplayEffect* ActiveEffect = POTAbilitySystemComponent->GetActiveGameplayEffect(ActiveEffectHandle);

		if (!ActiveEffect)
		{
			continue;
		}

		float RemainingTime = ActiveEffect->GetTimeRemaining(POTAbilitySystemComponent->GetWorld()->GetTimeSeconds());

		if (RemainingTime <= 0.f)
		{
			continue;
		}

		ActiveGameplayEffects.Add(FSavedGameplayEffectData(ActiveEffect->Spec.Def, RemainingTime));
	}
}

void AIBaseCharacter::AddGameplayEffectsAfterLoad()
{
	bHasRestoredSavedEffects = true;

	UPOTAbilitySystemComponent* POTAbilitySystemComponent = Cast<UPOTAbilitySystemComponent>(GetAbilitySystemComponent());
	if (!POTAbilitySystemComponent || ActiveGameplayEffects.Num() == 0)
	{
		return;
	}

	for (const FSavedGameplayEffectData& SavedEffect : ActiveGameplayEffects)
	{
		const FActiveGameplayEffectHandle ActiveEffectHandle = POTAbilitySystemComponent->ApplyGameplayEffectToSelf(SavedEffect.Effect, 1.f, AbilitySystem->MakeEffectContext());

		if (!ActiveEffectHandle.IsValid())
		{
			continue;
		}

		POTAbilitySystemComponent->ModifyActiveGameplayEffectDurationByHandle(ActiveEffectHandle, SavedEffect.Duration - POTAbilitySystemComponent->GetGameplayEffectDuration(ActiveEffectHandle));
	}

	ActiveGameplayEffects.Empty();
}

TArray<FActionBarAbility>& AIBaseCharacter::GetSlottedAbilityCategories_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, SlottedAbilityCategories, this);
	return SlottedAbilityCategories;
}

void AIBaseCharacter::OnUpdateSimulatedPosition(const FVector& OldLocation, const FQuat& OldRotation)
{
	Super::OnUpdateSimulatedPosition(OldLocation, OldRotation);
}

void AIBaseCharacter::PostNetReceiveLocationAndRotation()
{
	UCharacterMovementComponent* CharacterMovementComponent = Cast<UCharacterMovementComponent>(GetMovementComponent());
	if (!CharacterMovementComponent)
	{
		return;
	}
	
	if (GetLocalRole() != ROLE_SimulatedProxy)
	{
		return;
	}

	// Don't change transform if using relative position (it should be nearly the same anyway, or base may be slightly out of sync)
	if (!ReplicatedBasedMovement.HasRelativeLocation())
	{
		const FRepMovement& ConstRepMovement = GetReplicatedMovement();

		const FVector OldLocation = GetActorLocation();
		const FVector NewLocation = FRepMovement::RebaseOntoLocalOrigin(ConstRepMovement.Location, this);

		const FQuat OldRotation = GetActorQuat();

		CharacterMovementComponent->bNetworkSmoothingComplete = false;
		CharacterMovementComponent->bJustTeleported |= (OldLocation != NewLocation);
		CharacterMovementComponent->SmoothCorrection(OldLocation, OldRotation, NewLocation, ConstRepMovement.Rotation.Quaternion());
		OnUpdateSimulatedPosition(OldLocation, OldRotation);
	}
	CharacterMovementComponent->bNetworkUpdateReceived = true;
	
}

void AIBaseCharacter::SetIsStunned(bool bNewIsStunned)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bStunned, bNewIsStunned, this);
}

void AIBaseCharacter::OnRep_LaunchVelocity(FVector OldLaunchVelocity)
{
	
}

TOptional<float> AIBaseCharacter::GetSafeHeightIfUnderTheWorld(bool bForce /*= false*/) const
{
	TOptional<float> PotentiallySafeHeight{};

	check(IsLocallyControlled());
	if (!IsLocallyControlled())
	{	
		// This check should only run on the client, it can be expensive on the server
		return PotentiallySafeHeight;
	}

	if (IsPreloadingClientArea() && !bForce)
	{	
		// World is potentially not loaded at this point
		return PotentiallySafeHeight;
	}

	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	CollisionQueryParams.AddIgnoredActor(this);

	if (AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this))
	{
		for (AActor* ExcludedActor : IWorldSettings->FallThroughWorldExcludedActors)
		{
			CollisionQueryParams.AddIgnoredActor(ExcludedActor);
		}
	}

	{	
		// Initial Ground Check
		FVector FromLocation = GetActorLocation();
		FVector ToLocation = FromLocation - FVector{ 0, 0, 1000000 };
		FHitResult HitResult;
		if (GetWorld()->LineTraceSingleByChannel(HitResult, FromLocation, ToLocation, COLLISION_DINOCAPSULE, CollisionQueryParams))
		{
			if (HitResult.GetActor() && !Cast<ABlockingVolume>(HitResult.GetActor()))
			{
				//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, HitResult.GetActor()->GetName());
				return PotentiallySafeHeight; // Trace hit something beneath us, so we are not underneath the world.
			}
		}
	}

	{	
		// Find if a safe space above us exists
		FVector ToLocation = GetActorLocation();
		for (int Index = 1; Index < 50; Index++)
		{
			// Check in increments to ensure we can teleport back into a tunnel, rather than the ground above the tunnel
			FVector FromLocation = ToLocation + (FVector(0, 0, 150 * Index));
			FHitResult HitResult;
			if (GetWorld()->LineTraceSingleByChannel(HitResult, FromLocation, ToLocation, COLLISION_DINOCAPSULE, CollisionQueryParams))
			{
				PotentiallySafeHeight = HitResult.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
				return PotentiallySafeHeight; // Trace hit something above us, so we are definitely below the world, since nothing is underneath us now.
			}
		}
	}

	// Nothing beneath us, but nothing above us either, so we are not beneath the world.
	return PotentiallySafeHeight;
}

void AIBaseCharacter::PeriodicGroundCheck()
{
	if (!IsLocallyControlled())
	{
		// This check should only run on the client, it can be expensive on the server.
		// Also shouldn't be run on non-owned characters
		return;
	}

	if (Cast<AIAdminCharacter>(this))
	{
		return;
	}

	const TOptional<float> PotentiallySafeHeight = GetSafeHeightIfUnderTheWorld();
	if (!GetAttachTarget().IsValid() && PotentiallySafeHeight.IsSet() && (PotentiallySafeHeight.GetValue() > GetActorLocation().Z))
	{
		// We are under the world so ask server for adjustment.
		//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Client Requesting Adjust"));
		ServerFellThroughWorldAdjustment(PotentiallySafeHeight.GetValue());
	}
	else if (PeriodicUnderWorldCheckSeconds > 0)
	{
		// Not under the world, so we can make a new timer
		//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Client No Adjust"));
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::PeriodicGroundCheck);
		GetWorldTimerManager().SetTimer(TimerHandle_UnderWorldCheck, Del, PeriodicUnderWorldCheckSeconds, false);
	}
}

bool AIBaseCharacter::ServerFellThroughWorldAdjustment_Validate(float RequestedHeight)
{
	return true;
}

void AIBaseCharacter::ServerFellThroughWorldAdjustment_Implementation(float RequestedHeight)
{
	if (GetAttachTarget().IsValid())
	{
		return;
	}

	const UWorld* const World = GetWorld();
	if (!World) return;

	const AGameStateBase* const GameStateBase = World->GetGameState();
	if (!GameStateBase) return;

	const float CurrentTimeSeconds = GameStateBase->GetServerWorldTimeSeconds();

	if ((CurrentTimeSeconds - LastRepositionIfObstructedTime) < RepositionIfObstructedDelay)
	{
		return;
	}

	if (RequestedHeight < GetActorLocation().Z)
	{
		FString AlderonIdString = TEXT("No Player State");
		if (const AIPlayerState* IPlayerState = GetPlayerState<const AIPlayerState>())
		{
			AlderonIdString = IPlayerState->GetAlderonID().ToDisplayString();
		}
	
		UE_LOG(TitansLog, Warning, TEXT("AIBaseCharacter::ServerFellThroughWorldAdjustment: RequestedHeight is less than GetActorLocation().Z for player (%s)"), *AlderonIdString);
		return; // Adjustment should only be upwards
	}

	FVector ActorLocation = GetActorLocation();
	FVector FromLocation = FVector(ActorLocation.X, ActorLocation.Y, RequestedHeight);
	FVector ToLocation = FromLocation - FVector(0, 0, 100000);

	FCollisionQueryParams CollisionQueryParams = FCollisionQueryParams::DefaultQueryParam;
	CollisionQueryParams.AddIgnoredActor(this);

	if (AIWorldSettings* IWorldSettings = AIWorldSettings::GetWorldSettings(this))
	{
		for (AActor* ExcludedActor : IWorldSettings->FallThroughWorldExcludedActors)
		{
			CollisionQueryParams.AddIgnoredActor(ExcludedActor);
		}
	}

	FHitResult HitResult;
	if (GetWorld()->LineTraceSingleByChannel(HitResult, FromLocation, ToLocation, COLLISION_DINOCAPSULE, CollisionQueryParams))
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Server Accept Adjust"));
		FVector FinalAdjustment = FVector(ActorLocation.X, ActorLocation.Y, HitResult.ImpactPoint.Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		TeleportTo(FinalAdjustment, GetActorRotation(), false, true);
	}
	else
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Server No Adjust"));
	}
	ClientAckFellThroughWorldAdjustment(); // Acknowledge the request so client can continue underworld check
}

void AIBaseCharacter::ClientAckFellThroughWorldAdjustment_Implementation()
{
	//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Client Ack adjust"));
	if (PeriodicUnderWorldCheckSeconds > 0)
	{
		// Request was acknowledged, so start checking for ground again
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::PeriodicGroundCheck);
		GetWorldTimerManager().SetTimer(TimerHandle_UnderWorldCheck, Del, PeriodicUnderWorldCheckSeconds, false);
	}
}

bool AIBaseCharacter::IsValidatingInstance() const
{
	if (AIPlayerController* IPlayerController = Cast<AIPlayerController>(GetController()))
	{
		return IPlayerController->IsValidatingInstance();
	}
	return false;
}

FHomeCaveSaveableInfo& AIBaseCharacter::GetHomeCaveSaveableInfo_Mutable()
{
	MARK_PROPERTY_DIRTY_FROM_NAME(AIBaseCharacter, HomeCaveSaveableInfo, this);
	return HomeCaveSaveableInfo;
}

void AIBaseCharacter::ClientShowWaystonePrompt_Implementation()
{
	if (!UIGameplayStatics::IsInGameWaystoneAllowed(this))
	{
		return;
	}
	AIHUD* IHud = UIGameplayStatics::GetIHUD(this);
	check(IHud);
	if (!IHud)
	{
		return;
	}

	if (WaystonePromptMessageBox)
	{
		// Already open
		return;
	}

	WaystonePromptMessageBox = IHud->ShowMessageBox(EMessageBoxType::YesNo);
	if (!WaystonePromptMessageBox)
	{
		return;
	}
	
	FText Title = FText::FromStringTable(TEXT("ST_Waystone"), TEXT("Waystone"));
	FText Message = FText::FromStringTable(TEXT("ST_Waystone"), TEXT("WaystoneAcceptPendingPrompt"));
	WaystonePromptMessageBox->SetTextAndTitle(Title, Message);

	WaystonePromptMessageBox->OnMessageBoxClosed.AddDynamic(this, &AIBaseCharacter::WaystonePromptClosed);
}

void AIBaseCharacter::WaystonePromptClosed(EMessageBoxResult Result)
{
	if (Result == EMessageBoxResult::Yes)
	{
		ServerAcceptPendingWaystoneInvite();
	}

	WaystonePromptMessageBox = nullptr;
}

void AIBaseCharacter::ServerAcceptPendingWaystoneInvite_Implementation()
{
	if (!UIGameplayStatics::IsInGameWaystoneAllowed(this))
	{
		return;
	}

	AIPlayerController* IPlayerController = GetController<AIPlayerController>();
	check(IPlayerController);
	if (!IPlayerController)
	{
		return;
	}

	if (!IPlayerController->HasWaystoneInviteEffect())
	{
		return;
	}

	const FWaystoneInvite& WaystoneInvite = IPlayerController->GetWaystoneInvite();
	if (WaystoneInvite.WaystoneTag == NAME_None)
	{
		return;
	}

	IPlayerController->RemoveWaystoneInviteEffect(false);

	AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(this);
	check(WS);
	if (!WS)
	{
		return;
	}

	AIWaystoneManager* WM = WS->WaystoneManager;
	check(WM);
	if (!WM)
	{
		return;
	}

	if (WM->CooldownInProgress(WaystoneInvite.WaystoneTag))
	{
		return;
	}

	IPlayerController->ApplyWaystoneInviteEffect(true);

	GetWorldTimerManager().ClearTimer(TimerHandle_WaystoneCharging);
	FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &AIBaseCharacter::WaystoneInviteFullyCharged);
	GetWorldTimerManager().SetTimer(TimerHandle_WaystoneCharging, Del, 30.0f, false);

	SetIsWaystoneInProgress(true);

	if (HasAuthority() && !IsRunningDedicatedServer())
	{
		OnRep_WaystoneInProgress();
	}
}

void AIBaseCharacter::CancelWaystoneInviteCharging()
{
	GetWorldTimerManager().ClearTimer(TimerHandle_WaystoneCharging);

	SetIsWaystoneInProgress(false);

	if (HasAuthority() && !IsRunningDedicatedServer())
	{
		OnRep_WaystoneInProgress();
	}

	AIPlayerController* IPlayerController = GetController<AIPlayerController>();
	check(IPlayerController);
	if (!IPlayerController)
	{
		return;
	}

	IPlayerController->RemoveWaystoneInviteEffect(true);

	const FWaystoneInvite& WaystoneInvite = IPlayerController->GetWaystoneInvite();

	AIPlayerController* SenderPlayerController = nullptr;
	if (WaystoneInvite.InviteFromPlayerState)
	{
		SenderPlayerController = Cast<AIPlayerController>(WaystoneInvite.InviteFromPlayerState->GetPlayerController());
	}
	check(SenderPlayerController);

	// Clear sender's outgoing invite
	if (SenderPlayerController)
	{
		check(SenderPlayerController->GetOutgoingWaystoneInvite().InviteFromPlayerState == GetPlayerState());
		if (SenderPlayerController->GetOutgoingWaystoneInvite().InviteFromPlayerState == GetPlayerState())
		{
			SenderPlayerController->SetOutgoingWaystoneInvite(FWaystoneInvite());
		}
	}

	IPlayerController->SetWaystoneInvite(FWaystoneInvite());
}

void AIBaseCharacter::WaystoneInviteFullyCharged()
{
	if (!UIGameplayStatics::IsInGameWaystoneAllowed(this))
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(TimerHandle_WaystoneCharging);

	SetIsWaystoneInProgress(false);

	if (HasAuthority() && !IsRunningDedicatedServer())
	{
		OnRep_WaystoneInProgress();
	}

	AIPlayerController* IPlayerController = GetController<AIPlayerController>();
	check(IPlayerController);
	if (!IPlayerController)
	{
		return;
	}

	const FWaystoneInvite& WaystoneInvite = IPlayerController->GetWaystoneInvite();

	if (WaystoneInvite.WaystoneTag == NAME_None)
	{
		return;
	}

	AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(this);
	check(WS);
	if (!WS)
	{
		return;
	}

	AIWaystoneManager* WM = WS->WaystoneManager;
	check(WM);
	if (!WM)
	{
		return;
	}

	if (WM->CooldownInProgress(WaystoneInvite.WaystoneTag))
	{
		return;
	}

	// Spawn effects on player location
	WM->SendSpawnFX(GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

	// Register the cooldown
	WM->AddWaystoneData(WaystoneInvite.WaystoneTag);

	FVector TargetLocation = WM->GetWaystoneLocation(WaystoneInvite.WaystoneTag);

	bool bFoundSafe = false;
	FVector SafeLocation = GetSafeTeleportLocation(TargetLocation, bFoundSafe);

	FVector FinalLocation = FVector::ZeroVector;

	if (bFoundSafe)
	{
		FinalLocation = SafeLocation;
	}
	else
	{
		FinalLocation = TargetLocation;
	}

	if (GetCurrentInstance())
	{
		FTransform Transform = GetActorTransform();
		Transform.SetLocation(FinalLocation);
		ExitInstance(Transform);
	}
	else
	{
		TeleportTo(FinalLocation, GetActorRotation(), false, true);
	}

	WM->SendSpawnFX(FinalLocation - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));

	if (AIGameSession::UseWebHooks(WEBHOOK_PlayerWaystone))
	{
		AIPlayerState* IPlayerState = GetPlayerState<AIPlayerState>();
		if (IPlayerState && WaystoneInvite.InviteFromPlayerState)
		{
			TMap<FString, TSharedPtr<FJsonValue>> WebHookProperties
			{
				{ TEXT("InviterName"), MakeShareable(new FJsonValueString(WaystoneInvite.InviteFromPlayerState->GetPlayerName()))},
				{ TEXT("InviterAlderonId"), MakeShareable(new FJsonValueString(WaystoneInvite.InviteFromPlayerState->GetAlderonID().ToDisplayString()))},
				{ TEXT("TeleportedPlayerName"), MakeShareable(new FJsonValueString(IPlayerState->GetPlayerName()))},
				{ TEXT("TeleportedPlayerAlderonId"), MakeShareable(new FJsonValueString(IPlayerState->GetAlderonID().ToDisplayString()))},
			};
				
			AIGameSession::TriggerWebHookFromContext(this, WEBHOOK_PlayerWaystone, WebHookProperties);
		}
	}

	AIPlayerController* SenderPlayerController = nullptr;
	if (WaystoneInvite.InviteFromPlayerState)
	{
		SenderPlayerController = Cast<AIPlayerController>(WaystoneInvite.InviteFromPlayerState->GetPlayerController());
	}
	check(SenderPlayerController);

	// Clear sender's outgoing invite
	if (SenderPlayerController)
	{
		check(SenderPlayerController->GetOutgoingWaystoneInvite().InviteFromPlayerState == GetPlayerState());
		if (SenderPlayerController->GetOutgoingWaystoneInvite().InviteFromPlayerState == GetPlayerState())
		{
			SenderPlayerController->SetOutgoingWaystoneInvite(FWaystoneInvite());
		}
	}

	// Reset Invite
	IPlayerController->SetWaystoneInvite(FWaystoneInvite());
}

void AIBaseCharacter::SetIsWaystoneInProgress(bool bNewIsWaystoneInProgress)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bWaystoneInProgress, bNewIsWaystoneInProgress, this);
}

void AIBaseCharacter::OnRep_WaystoneInProgress()
{
	if (!UIGameplayStatics::IsInGameWaystoneAllowed(this))
	{
		return;
	}
	AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(this);
	check(WS);
	if (!WS)
	{
		return;
	}

	AIWaystoneManager* WM = WS->WaystoneManager;
	check(WM);
	if (!WM)
	{
		return;
	}

	if (WM->WaystoneInProgressFX.IsNull())
	{
		return;
	}

	if (IsWaystoneInProgress())
	{
		if (!WaystoneInProgressFX)
		{
			FStreamableManager& AssetLoader = UIGameplayStatics::GetStreamableManager(this);
			TSharedPtr<FStreamableHandle> Handle = AssetLoader.RequestAsyncLoad(
				WM->WaystoneInProgressFX.ToSoftObjectPath(),
				FStreamableDelegate::CreateUObject(this, &AIBaseCharacter::WaystoneInProgressFXLoaded), FStreamableManager::AsyncLoadHighPriority, false);

		}
	}
	else
	{
		if (WaystoneInProgressFX)
		{
			WaystoneInProgressFX->Destroy();
			WaystoneInProgressFX = nullptr;
		}
	}
}

void AIBaseCharacter::WaystoneInProgressFXLoaded()
{
	if (!UIGameplayStatics::IsInGameWaystoneAllowed(this))
	{
		return;
	}
	if (WaystoneInProgressFX)
	{
		WaystoneInProgressFX->Destroy();
		WaystoneInProgressFX = nullptr;
	}

	AIWorldSettings* WS = AIWorldSettings::GetWorldSettings(this);
	check(WS);
	if (!WS)
	{
		return;
	}

	AIWaystoneManager* WM = WS->WaystoneManager;
	check(WM);
	if (!WM)
	{
		return;
	}

	if (WM->WaystoneInProgressFX.IsNull())
	{
		return;
	}

	if (!IsWaystoneInProgress())
	{
		return;
	}

	UClass* LoadedFX = WM->WaystoneInProgressFX.Get();
	check(LoadedFX);
	if (!LoadedFX)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FTransform BodyTransform = GetActorTransform();
	BodyTransform.SetLocation(BodyTransform.GetLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
	WaystoneInProgressFX = GetWorld()->SpawnActor<AActor>(LoadedFX, BodyTransform, SpawnParams);
}

float AIBaseCharacter::GetStaminaCarryCostMultiplier() const
{
	return StaminaCarryCostMultiplier;
}

float AIBaseCharacter::GetStaminaBadWeatherCostMultiplier() const
{
	return StaminaBadWeatherCostMultiplier;
}

FSpawnFootstepSoundParams::FSpawnFootstepSoundParams()
	: SoundCue(nullptr)
	, FootstepLocation(FVector::ZeroVector)
	, FootstepType(EFootstepType::FT_STEP)
	, AttachName(NAME_None)
	, GrowthParameterName(NAME_None)
	, bShouldFollow(true)
	, VolumeMultiplier(1.0f)
	, PitchMultiplier(1.0f)
{
}

FName AIBaseCharacter::BP_GetCombinedPlayerStartTag(bool bUseOverrideGrowth, float OverrideGrowthPercent, const FName& OverridePlayerStartTag) const
{
	return GetCombinedPlayerStartTag(bUseOverrideGrowth ? &OverrideGrowthPercent : nullptr, OverridePlayerStartTag);
}

FName AIBaseCharacter::GetCombinedPlayerStartTag(const float* OverrideGrowthPercent, const FName& OverridePlayerStartTag) const
{
	FString GrowthTag;
	if (!OverrideGrowthPercent)
	{
		GrowthTag = GetGrowthPercent() <= 0.3f ? TEXT("Juvenile") : TEXT("");
	} 
	else
	{
		GrowthTag = *OverrideGrowthPercent <= 0.3f ? TEXT("Juvenile") : TEXT("");
	}
	return FName((OverridePlayerStartTag.IsNone() ? PlayerStartTag.ToString() : OverridePlayerStartTag.ToString()) + GrowthTag);
}

bool AIBaseCharacter::GetInteractParticle_Implementation(FInteractParticleData& OutParticleData)
{
	if (InteractParticle.PSTemplate.IsNull())
	{
		return false;
	}
	else
	{
		OutParticleData = InteractParticle;
		return true;
	}
}

ECarriableSize AIBaseCharacter::GetMaxCarriableSize()
{
	float currentGrowth = GetGrowthPercent();

	//select the right map and return the first value that matches current size
	if (OverrideDefaultCarriableSizeLogic)
	{
		for (const TPair<float, ECarriableSize>& pair : OverrideCarriableSizeMap)
		{
			if (pair.Key >= currentGrowth)
			{
				return pair.Value;
			}
		}
	}
	else if (MaxCarriableSize == ECarriableSize::MEDIUM)
	{
		for (const TPair<float, ECarriableSize>& pair : MediumCarriableSizeMap)
		{
			if (pair.Key >= currentGrowth)
			{
				return pair.Value;
			}
		}
	}
	else if (MaxCarriableSize == ECarriableSize::LARGE)
	{
		for (const TPair<float, ECarriableSize>& pair : LargeCarriableSizeMap)
		{
			if (pair.Key >= currentGrowth)
			{
				return pair.Value;
			}
		}
	}

	//if no appropriate map simply return the default max size
	return MaxCarriableSize;
}

// This function used to only return true
bool AIBaseCharacter::CanHaveQuests() const
{
	return !HaveMetMaxUnclaimedRewards();
}

bool AIBaseCharacter::HaveMetMaxUnclaimedRewards() const 
{
	bool bHasMetMaxUnclaimedRewards = false;

	const AIWorldSettings* WorldSettings = AIWorldSettings::Get(this);
	check(WorldSettings);
	if (WorldSettings)
	{
		const AIQuestManager* QuestMgr = WorldSettings->QuestManager;
		check(QuestMgr);
		if (QuestMgr && GetUncollectedRewardQuests().Num() > 0 && QuestMgr->ShouldEnableMaxUnclaimedRewards())
		{
			bHasMetMaxUnclaimedRewards = (GetUncollectedRewardQuests().Num() + GetActiveQuests().Num()) >= QuestMgr->GetMaxUnclaimedRewards();
		}
	}

	return bHasMetMaxUnclaimedRewards;
}

void AIBaseCharacter::SetReplicatedAccelerationNormal(const FVector& NewAccelerationNormal)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, ReplicatedAccelerationNormal, NewAccelerationNormal, this);
}

void AIBaseCharacter::SetIsBeingDragged(bool bNewBeingDragged)
{
	if (bNewBeingDragged && IsAttached())
	{
		ClearAttachTarget();
	}

	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bIsBeingDragged, bNewBeingDragged, this);
}

void AIBaseCharacter::SetHasLeftHatchlingCave(bool bNewLeftHatchlingCave)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bLeftHatchlingCave, bNewLeftHatchlingCave, this);
}

void AIBaseCharacter::SetReadyToLeaveHatchlingCave(bool bReady)
{
	COMPARE_ASSIGN_AND_MARK_PROPERTY_DIRTY(AIBaseCharacter, bReadyToLeaveHatchlingCave, bReady, this);
}
