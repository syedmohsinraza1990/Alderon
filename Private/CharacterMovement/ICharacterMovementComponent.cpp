// Copyright 2019-2022 Alderon Games Pty Ltd, All Rights Reserved.

#include "Components/ICharacterMovementComponent.h"
#include "AbilitySystemGlobals.h"
#include "Player/IBaseCharacter.h"
#include "Navigation/PathFollowingComponent.h"
#include "Player/Dinosaurs/IDinosaurCharacter.h"
#include "Player/IAdminCharacter.h"
#include "Landscape.h"
#include "GameFramework/SpringArmComponent.h"
#include "Stats/IStats.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/StateAdjustableCapsuleComponent.h"
#include "GameFramework/GameNetworkManager.h"
#include "DrawDebugHelpers.h"
#include "MultiCapsuleTraceWorker.h"
#include "Abilities/CoreAttributeSet.h"
#include "Abilities/POTAbilitySystemComponent.h"
#include "Engine/World.h"
#include "KismetAnimationLibrary.h"
#include "Abilities/Tasks/AbilityTask_ApplyRootMotion_Base.h"
#include "Online/IPlayerState.h"
#include "IGameInstance.h"
#include "IGameplayStatics.h"
#include "Online/IGameSession.h"
#include "Online/IGameState.h"
#include "IWorldSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogICharacterMovement, Log, All);

// CVars
namespace ICharacterMovementCVars
{

	static TAutoConsoleVariable<bool> CVarDisableInvalidSurfaceHandling(
		TEXT("pot.DisableInvalidSurfaceHandling"),
		false,
		TEXT("If > 0 Use Unreal's default way of handling standing on invalid surfaces (launches character).\n"),
		ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVarDebugInvalidSurfaceHandling(
		TEXT("pot.DebugInvalidSurfaceHandling"),
		false,
		TEXT("If true, show extra debugging information about the invalid surface system.\n"),
		ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVarDisableCMCAdditions(
		TEXT("pot.DisableCMCAdditions"),
		false,
		TEXT("If > 0 Certain overridden CMC methods will return default values.\n"),
		ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVarDisableCustomAcceleration(
		TEXT("pot.DisableCustomAcceleration"),
		false,
		TEXT("If > 0 default CMC acceleration will be used. \n"),
		ECVF_Cheat);

	/**
	 * The AlwaysMovingForward function can result in velocity differences between client and server
	 */
	static TAutoConsoleVariable<bool> CVarAlwaysMovingForward(
		TEXT("pot.AlwaysMovingForward"),
		false,
		TEXT("If > 0 CMC will consider the character to always be moving forward. \n"),
		ECVF_Cheat);

	static TAutoConsoleVariable<bool> CVarApproximateValidation(
		TEXT("pot.ApproximateMovementValidation"),
		false,
		TEXT("If 1 movements from clients will be approximated for verification. \n"),
		ECVF_Default);

	/*
	These console variables (NetUseBaseRelativeAcceleration and NetUseBaseRelativeVelocity) are already declared
	in CharacterMovementComponent.cpp, which is an engine class. We are declaring them here again so that we can
	use them without having to include CharacterMovement.cpp class here.
	*/
	static int32 NetUseBaseRelativeAcceleration = 1;
	FAutoConsoleVariableRef CVarNetUseBaseRelativeAcceleration(
		TEXT("p.NetUseBaseRelativeAcceleration"),
		NetUseBaseRelativeAcceleration,
		TEXT("If enabled, character acceleration will be treated as relative to dynamic movement bases."));

	static int32 NetUseBaseRelativeVelocity = 1;
	FAutoConsoleVariableRef CVarNetUseBaseRelativeVelocity(
		TEXT("p.NetUseBaseRelativeVelocity"),
		NetUseBaseRelativeVelocity,
		TEXT("If enabled, character velocity corrections will be treated as relative to dynamic movement bases."));
}
	
/**
 * Client replicates rotation in the saved moves
 */
//static TAutoConsoleVariable<bool> CVarSendRotationInSavedMoves(
//    TEXT("pot.ClientSavedMovesRotation"),
//    true,
//    TEXT("If > 0 client will replicate rotation to the server. \n"),
//    ECVF_Cheat);
//
//static TAutoConsoleVariable<bool> CVarEnableClientAuthoritativePosition(
//    TEXT("pot.ClientAuthoritativePosition"),
//    false,
//    TEXT("If 0, clients will not have authoritative position. This sets the value of GameNetworkManager::ClientAuthorativePosition\n"),
//    ECVF_Cheat);

/**
 * This will override the value set in config
 * This is the size of position error the server will tolerate before correcting the client
 * Note that this is used exclusively in the case of client authoritative position
 */
//static TAutoConsoleVariable<float> CVarMaximumClientError(
//    TEXT("pot.MaximumClientError"),
//    5.0,
//    TEXT("Sets AGameNetworkManager::MAXPOSITIONERRORSQUARED\n"),
//    ECVF_Cheat);

UICharacterMovementComponent::UICharacterMovementComponent(const FObjectInitializer& ObjectInitialiser) :
	Super(ObjectInitialiser)
{
	NetworkSmoothingMode = ENetworkSmoothingMode::Linear;
	bWantsToPreciseMove = false;

	// Multi Capsule Movement
	bAutoGatherNonRootCapsules = true;
	bUseMultiCapsuleCollision = true;
	AsyncCollisionMode = 1;
	IncrementalAsyncUpdateMode = 0;
	bTestTraceIfNoBlocking = true;
	bSkipUpdateChildTransforms = true;

	//GetMutableDefault<AGameNetworkManager>()->ClientAuthorativePosition = CVarEnableClientAuthoritativePosition->GetBool();
	//GetMutableDefault<AGameNetworkManager>()->MAXPOSITIONERRORSQUARED = FMath::Square(CVarMaximumClientError->GetFloat());

	SetMoveResponseDataContainer(MoveResponseDataContainer);
	SetNetworkMoveDataContainer(POTMoveDataContainer);
}

bool UICharacterMovementComponent::IsMovingForward() const
{
	if (ICharacterMovementCVars::CVarAlwaysMovingForward->GetBool())
	{
		return true;
	}
	if (!PawnOwner) return false;

	FRotator OwnerRot = PawnOwner->GetActorRotation();
	// Simulate the same level of precision as the server has
	OwnerRot.Yaw = FRotator::DecompressAxisFromByte(FRotator::CompressAxisToByte(OwnerRot.Yaw));

	return !PawnOwner->GetVelocity().IsZero()
		// Don't allow sprint while strafing sideways or standing still (1.0 is straight forward, -1.0 is backward while near 0 is sideways or standing still)
		&& (FVector::DotProduct(Velocity.GetSafeNormal2D(), OwnerRot.Vector()) > 0.1); // Changing this value to 0.1 allows for diagonal sprinting. (holding W+A or W+D keys)
}

bool UICharacterMovementComponent::IsMoving() const
{
	if (!PawnOwner) return false;
	return !PawnOwner->GetVelocity().IsZero();
}

bool UICharacterMovementComponent::IsTransitioningOutOfWaterBackwards()
{
	return bTransitioningPreciseMovement;
}
void UICharacterMovementComponent::CancelBackwardsTransition()
{
	bWantsToPreciseMove = false;
	bTransitioningPreciseMovement = false;
	elapsedTransitionPreciseMovementTime = 0.0f;
}

void UICharacterMovementComponent::SetDefaultMovementMode()
{
	// check for water volume
	if (CanEverSwim() && IsInWater())
	{
		SetMovementMode(DefaultWaterMovementMode);
	}
	else
	{
		AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner());
		AController* Controller = DinoOwner ? DinoOwner->GetController() : nullptr;
		bool NotNearGround = DinoOwner ? DinoOwner->SpaceBelowPawn(500.0f) : false;

		if (DinoOwner && Controller && DinoOwner->HasFlying() && NotNearGround)
		{
			SetMovementMode(MOVE_Flying);
		}
		else if (!CharacterOwner || MovementMode != DefaultLandMovementMode)
		{
			const float SavedVelocityZ = Velocity.Z;
			SetMovementMode(DefaultLandMovementMode);

			// Avoid 1-frame delay if trying to walk but walking fails at this location.
			//if (MovementMode == MOVE_Walking && GetMovementBase() == NULL)
			//{
				//Velocity.Z = SavedVelocityZ; // Prevent temporary walking state from zeroing Z velocity.
				//SetMovementMode(MOVE_Falling);
			//}
		}
	}
}

void UICharacterMovementComponent::SimulateMovement(float DeltaSeconds)
{
	if (bJustTeleported)
	{
		ICharacterMovementCVars::CVarAlwaysMovingForward->Set(false);
	}

	if (!HasValidData() || UpdatedComponent->Mobility != EComponentMobility::Movable || UpdatedComponent->IsSimulatingPhysics())
	{
		return;
	}

	const bool bIsSimulatedProxy = (CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy);

	const FRepMovement& ConstRepMovement = CharacterOwner->GetReplicatedMovement();

	// Workaround for replication not being updated initially
	if (!bReceivedRep &&
		bIsSimulatedProxy &&
		ConstRepMovement.Location.IsZero() &&
		ConstRepMovement.Rotation.IsZero() &&
		ConstRepMovement.LinearVelocity.IsZero())
	{
		return;
	}

	bReceivedRep = true;

	// If base is not resolved on the client, we should not try to simulate at all
	if (CharacterOwner->GetReplicatedBasedMovement().IsBaseUnresolved())
	{
		//UE_LOG(LogCharacterMovement, Verbose, TEXT("Base for simulated character '%s' is not resolved on client, skipping SimulateMovement"), *CharacterOwner->GetName());
		return;
	}


	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(CharacterOwner.Get());
	
	FVector OldVelocity;
	FVector OldLocation;

	// Scoped updates can improve performance of multiple MoveComponent calls.
	{
		FScopedMovementUpdate ScopedMovementUpdate(UpdatedComponent, bEnableScopedMovementUpdates ? EScopedUpdate::DeferredUpdates : EScopedUpdate::ImmediateUpdates);

		bool bHandledNetUpdate = false;
		if (bIsSimulatedProxy)
		{
			// Handle network changes
			if (bNetworkUpdateReceived)
			{
				bNetworkUpdateReceived = false;
				bHandledNetUpdate = true;
				//UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s received net update"), *CharacterOwner->GetName());
				if (bNetworkMovementModeChanged)
				{
					ApplyNetworkMovementMode(CharacterOwner->GetReplicatedMovementMode());
					bNetworkMovementModeChanged = false;
				}
				else if (bJustTeleported || bForceNextFloorCheck)
				{
					// Make sure floor is current. We will continue using the replicated base, if there was one.
					bJustTeleported = false;
					UpdateFloorFromAdjustment();
				}
			}
			else if (bForceNextFloorCheck)
			{
				UpdateFloorFromAdjustment();
			}
		}

		UpdateCharacterStateBeforeMovement(DeltaSeconds);

		if (MovementMode != MOVE_None)
		{
			//TODO: Also ApplyAccumulatedForces()?
			HandlePendingLaunch();
		}
		ClearAccumulatedForces();

		if (MovementMode == MOVE_None)
		{
			return;
		}

		const bool bSimGravityDisabled = (bIsSimulatedProxy && CharacterOwner->bSimGravityDisabled);
		const bool bZeroReplicatedGroundVelocity = (bIsSimulatedProxy && IsMovingOnGround() && ConstRepMovement.LinearVelocity.IsZero());

		// bSimGravityDisabled means velocity was zero when replicated and we were stuck in something. Avoid external changes in velocity as well.
		// Being in ground movement with zero velocity, we cannot simulate proxy velocities safely because we might not get any further updates from the server.
		if (bSimGravityDisabled || bZeroReplicatedGroundVelocity)
		{
			Velocity = FVector::ZeroVector;
		}

		MaybeUpdateBasedMovement(DeltaSeconds);

		// simulated pawns predict location
		OldVelocity = Velocity;
		OldLocation = UpdatedComponent->GetComponentLocation();

		UpdateProxyAcceleration();

		// May only need to simulate forward on frames where we haven't just received a new position update.
		if (!bHandledNetUpdate || !bNetworkSkipProxyPredictionOnNetUpdate)
		{
			//UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s simulating movement"), *GetNameSafe(CharacterOwner));
			FStepDownResult StepDownResult;
			if (IBaseCharacter)
			{
				MoveSmooth(Velocity + IBaseCharacter->GetLaunchVelocity(), DeltaSeconds, &StepDownResult);
				ApplyLaunchVelocityBraking(DeltaSeconds, 0);
			}
			else
			{
				MoveSmooth(Velocity, DeltaSeconds, &StepDownResult);
			}

			// find floor and check if falling
			if (IsMovingOnGround() || MovementMode == MOVE_Falling)
			{
				if (StepDownResult.bComputedFloor)
				{
					CurrentFloor = StepDownResult.FloorResult;
				}
				else if (Velocity.Z <= 0.f)
				{
					FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, Velocity.IsZero(), NULL);
				}
				else
				{
					CurrentFloor.Clear();
				}

				if (!CurrentFloor.IsWalkableFloor())
				{
					if (!bSimGravityDisabled)
					{
						// No floor, must fall.
						if (Velocity.Z <= 0.f || bApplyGravityWhileJumping || !CharacterOwner->IsJumpProvidingForce())
						{
							Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
						}
					}
					//SetMovementMode(MOVE_Falling);
				}
				else
				{
					// Walkable floor
					if (IsMovingOnGround())
					{
						AdjustFloorHeight();
						SetBase(CurrentFloor.HitResult.Component.Get(), CurrentFloor.HitResult.BoneName);
					}
					else if (MovementMode == MOVE_Falling)
					{
						if (CurrentFloor.FloorDist <= MIN_FLOOR_DIST || (bSimGravityDisabled && CurrentFloor.FloorDist <= MAX_FLOOR_DIST))
						{
							// Landed
							SetPostLandedPhysics(CurrentFloor.HitResult);
							if (bIsSimulatedProxy) // Do landed montage for simulated proxies
							{
								if (IBaseCharacter)
								{
									IBaseCharacter->Landed(CurrentFloor.HitResult);
								}
							}
						}
						else
						{
							if (!bSimGravityDisabled)
							{
								// Continue falling.
								Velocity = NewFallVelocity(Velocity, FVector(0.f, 0.f, GetGravityZ()), DeltaSeconds);
							}
							CurrentFloor.Clear();
						}
					}
				}
			}
		}
		else
		{
			//UE_LOG(LogCharacterMovement, Verbose, TEXT("Proxy %s SKIPPING simulate movement"), *GetNameSafe(CharacterOwner));
		}

		UpdateCharacterStateAfterMovement(DeltaSeconds);

		// consume path following requested velocity
		bHasRequestedVelocity = false;

		OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	} // End scoped movement update

	// Call custom post-movement events. These happen after the scoped movement completes in case the events want to use the current state of overlaps etc.
	CallMovementUpdateDelegate(DeltaSeconds, OldLocation, OldVelocity);

	//if (CharacterMovementCVars::BasedMovementMode == 0)
	//{
	//	SaveBaseLocation(); // behaviour before implementing this fix
	//}
	//else
	{
		MaybeSaveBaseLocation();
	}
	UpdateComponentVelocity();
	bJustTeleported = false;

	LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
	LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastUpdateVelocity = Velocity;
}

void UICharacterMovementComponent::FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult) const
{
	FFindFloorResult CurrentFloorResult = OutFloorResult;
	Super::FindFloor(CapsuleLocation, CurrentFloorResult, bCanUseCachedLocation, DownwardSweepResult);
	if (!Cast<ACharacter>(CurrentFloorResult.HitResult.GetActor()))
	{
		// Only set the new floor if it is not an ACharacter.
		OutFloorResult = CurrentFloorResult;
	}
}

void UICharacterMovementComponent::SetBase(UPrimitiveComponent* NewBase, const FName BoneName, bool bNotifyActor)
{
	if (NewBase && Cast<ACharacter>(NewBase->GetOwner()))
	{
		// Do not allow a character to be a movement base.
		return;
	}
	Super::SetBase(NewBase, BoneName, bNotifyActor);
}

bool UICharacterMovementComponent::IsNotMoving() const
{
	if (bWantsToStop) return true;
	return !IsMovingForward();
}

bool UICharacterMovementComponent::IsAtWaterSurface() const
{
	AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(PawnOwner);
	check(IBaseCharacter);

	return IsSwimming() && IBaseCharacter->IsAtWaterSurface();
}

bool UICharacterMovementComponent::IsSprinting() const
{
	//We can sprint in the water as long as we're aquatic. We can also sprint backwards in the water when we're aquatic
	const bool bCanSwimFastBackwards = IsSwimming() && bCanUseAdvancedSwimming;
	const bool bIsValidMovingBackwardsInWater = !IsMovingForward() && !bCanSwimFastBackwards && !PawnOwner->IsA<AIAdminCharacter>();
	const bool bNotMovingOnGroundAndCantFastSwim = !IsMovingOnGround() && IsSwimming() && !bCanUseAdvancedSwimming;

	if (bIsValidMovingBackwardsInWater || bNotMovingOnGroundAndCantFastSwim) return false;

	if (AIBaseCharacter* BaseCharacter = Cast<AIBaseCharacter>(PawnOwner)) // Disable sprinting while there is a forced movement speed, to prevent stamina loss
	{
		if (BaseCharacter->AbilitySystem && BaseCharacter->AbilitySystem->HasAbilityForcedMovementSpeed())
		{
			return false;
		}
	}

	return bWantsToSprint;
}

bool UICharacterMovementComponent::IsTrotting() const
{
	//Only allow advanced swimmers to trot while swimming
	bool isSwim = (IsSwimming() && bCanUseAdvancedSwimming);

	return bWantsToTrot && ((IsMovingOnGround() && IsMovingForward()) || isSwim);
}

bool UICharacterMovementComponent::IsLimping() const
{
	return bWantsToLimp;
}

bool UICharacterMovementComponent::IsTurningInPlace() const
{
	AIBaseCharacter* BaseCharacter = Cast<AIBaseCharacter>(PawnOwner);
	return (BaseCharacter->GetCurrentMovementMode() == ECustomMovementMode::TURNINPLACE);
}

bool UICharacterMovementComponent::ShouldUsePreciseMovement() const
{
	if (AIDinosaurCharacter* DinoCharacter = Cast<AIDinosaurCharacter>(PawnOwner))
	{
		if(DinoCharacter->AbilitySystem != nullptr)
		{
			if (DinoCharacter->AbilitySystem->HasAbilityForcedMovementSpeed())
			{
				return false;
			}
			
			if(UPOTGameplayAbility* CurrentAbility = DinoCharacter->AbilitySystem->GetCurrentAttackAbility())
			{
				if(CurrentAbility->bStopPreciseMovement)
				{
					return false;
				}
			}
		}
		
		// Allow precise move while swallowing foods
		const bool bEatConditionToPreciseMove = !DinoCharacter->IsEatingOrDrinking() || (DinoCharacter->IsEating() && DinoCharacter->IsCarryingObject()); 
		const bool bBasicPreciseMoveCondition = bWantsToPreciseMove && !DinoCharacter->bIsBasicInputDisabled;
		const bool bDinoActionsToPreciseMove = !DinoCharacter->IsRestingOrSleeping() && !DinoCharacter->IsDisturbingCritter();

		if (bBasicPreciseMoveCondition && bEatConditionToPreciseMove && bDinoActionsToPreciseMove)
		{
			if (bCanUsePreciseSwimming && IsSwimming() && DinoCharacter->IsAquatic())
			{
				return true;
			}
			else if (bCanUsePreciseNavigation && !IsSwimming())
			{
				return true;
			}
		}
	}

	return false;
}

bool UICharacterMovementComponent::ShouldUseControllerDesiredRotation(float NeededAmount /*= 10.0f*/) const
{
	bool bShouldUseControllerDesiredRotation = false;

	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		if (Velocity.Size() == 0.0f)
		{
			if ((fabsf(GetDifferenceBetweenPawnAndCameraYaw()) >= NeededAmount) || (DinoOwner->TurnInPlaceTargetYaw != 0.0f))
			{
				UpdateTurnInPlaceTargetYaw();

				bShouldUseControllerDesiredRotation = DinoOwner->TurnInPlaceTargetYaw != 0.0f;
			}
		}
		else if (!bShouldUseControllerDesiredRotation)
		{
			const FRotator DinoRotation = DinoOwner->GetActorRotation().GetInverse();
			const FRotator AccelerationRotation = GetCurrentAcceleration().Rotation();
			const float AccelerationDifference = UKismetMathLibrary::ComposeRotators(AccelerationRotation, DinoRotation).Yaw;

			bShouldUseControllerDesiredRotation = (AccelerationDifference > 0.0f ? AccelerationDifference > 50.0f : AccelerationDifference < -50.0f);
		}
	}

	return bShouldUseControllerDesiredRotation;
}

void UICharacterMovementComponent::UpdateTurnInPlaceTargetYaw() const
{
	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		const FRotator ActorRotation = CharacterOwner->GetActorRotation();

		DinoOwner->TurnInPlaceTargetYaw = UKismetMathLibrary::ComposeRotators(ActorRotation, FRotator(0.0f, GetDifferenceBetweenPawnAndCameraYaw(), 0.0f)).Yaw;
	}
}

EPreciseMovementDirection UICharacterMovementComponent::GetPreciseMovementDirection() const
{
	if (CharacterOwner)
	{
		const int32 MovementDirection = abs(FMath::TruncToInt(UKismetAnimationLibrary::CalculateDirection(CharacterOwner->GetVelocity(), CharacterOwner->GetActorRotation())));

		if (MovementDirection > 170)
		{
			return EPreciseMovementDirection::REVERSE;
		}
		else if (MovementDirection > 130)
		{
			return EPreciseMovementDirection::REVERSELEFTRIGHT;
		}
		else if (MovementDirection > 60)
		{
			return EPreciseMovementDirection::LEFTRIGHT;
		}
		else if (Velocity.Size() > 0)
		{
			return EPreciseMovementDirection::FORWARD;
		}
	}

	return EPreciseMovementDirection::NONE;
}

void UICharacterMovementComponent::SetSprinting(bool bSprinting)
{
	bWantsToSprint = bSprinting;
}

void UICharacterMovementComponent::SetTrotting(bool bTrotting)
{
	bWantsToTrot = bTrotting;
}

void UICharacterMovementComponent::SetLimping(bool bLimping)
{
	bWantsToLimp = bLimping;
}

void UICharacterMovementComponent::SetStopped(bool bStop)
{
	bWantsToStop = bStop;
}

void UICharacterMovementComponent::SetPreciseMovement(bool bPreciseMove)
{
	bWantsToPreciseMove = bPreciseMove;
}

float UICharacterMovementComponent::GetDifferenceBetweenPawnAndCameraYaw() const
{
	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		FRotator Delta = DinoOwner->GetLocalAimRotation();
		Delta.Normalize();

		return Delta.Yaw;
	}
	
	return false;
}

float UICharacterMovementComponent::GetMaxSpeedSlopeModifier() const
{
	if (ICharacterMovementCVars::CVarDisableCMCAdditions->GetBool())
	{
		return 1.;
	}

	if (UpdatedComponent)
	{
		if (!ShouldUsePreciseMovement())
		{
			float SlopeAngle = UKismetMathLibrary::MakeRotationFromAxes(FVector::CrossProduct(UpdatedComponent->GetRightVector(), CurrentFloor.HitResult.Normal).GetSafeNormal(0.0001f), UpdatedComponent->GetRightVector(), CurrentFloor.HitResult.Normal).Pitch * -1.0f;

			if (fabsf(SlopeAngle) > 15.0f)
			{
				SlopeAngle += (SlopeAngle > 0.0f ? -15.0f : 15.0f);

				// Calculate percentage that will affect allowed max speed. Clamped to 25% increase/decrease.
				return FMath::Clamp(SlopeAngle, -.25f, .25f);
			}
		}
	}

	return 0.0f;
}

bool UICharacterMovementComponent::IsStandingOnCharacter() const
{
	AActor* MovementBaseActor = APawn::GetMovementBaseActor(GetCharacterOwner());
	if (MovementBaseActor && Cast<ACharacter>(MovementBaseActor) && !MovementBaseActor->CanBeBaseForCharacter(GetCharacterOwner()))
	{
		// The component our player is trying to stand on
		UPrimitiveComponent* IntersectingComponent = GetCharacterOwner()->GetMovementBase();
		if (Cast<UCapsuleComponent>(IntersectingComponent))
		{
			const UCapsuleComponent* OtherCapsule = Cast<UCapsuleComponent>(IntersectingComponent);
			const UCapsuleComponent* OwnerCapsule = GetCharacterOwner()->GetCapsuleComponent();
			const float OwnerBottom = OwnerCapsule->GetComponentLocation().Z - OwnerCapsule->Bounds.BoxExtent.Z;
			const float OtherTop = OtherCapsule->GetComponentLocation().Z + OtherCapsule->Bounds.BoxExtent.Z;
			const float OverlapMargin = OtherCapsule->GetScaledCapsuleRadius() + OwnerCapsule->GetScaledCapsuleRadius();
			const bool IsOnTopOf = (OwnerBottom + OverlapMargin) > OtherTop;

#if WITH_EDITOR
			if (ICharacterMovementCVars::CVarDebugInvalidSurfaceHandling->GetBool())
			{
				const FVector OwnerBottomPoint = OwnerCapsule->GetComponentLocation() - FVector(0, 0, OwnerCapsule->Bounds.BoxExtent.Z);
				// Draw the actual capsules that are relevant - this is useful over just using "show collision" because it's not immediately obvious which of the dinosaur's capsules are colliding
				DrawDebugCapsule(GetWorld(), OtherCapsule->GetComponentLocation(), OtherCapsule->GetScaledCapsuleHalfHeight(), OtherCapsule->GetScaledCapsuleRadius(), OtherCapsule->GetComponentRotation().Quaternion(), FColor::Yellow, true, 3.f);
				DrawDebugCapsule(GetWorld(), OwnerCapsule->GetComponentLocation(), OwnerCapsule->GetScaledCapsuleHalfHeight(), OwnerCapsule->GetScaledCapsuleRadius(), OwnerCapsule->GetComponentRotation().Quaternion(), IsOnTopOf ? FColor::Red : FColor::Green, true, 3.f);
				DrawDebugLine(GetWorld(),  OwnerBottomPoint, OwnerBottomPoint + FVector(0, 0, OverlapMargin), FColor::Orange, true, 3.f);
				DrawDebugSphere(GetWorld(), OtherCapsule->GetComponentLocation() + FVector(0, 0, OtherCapsule->Bounds.BoxExtent.Z), 5, 12, FColor::Yellow, true, 3.f);
				DrawDebugLine(GetWorld(), OwnerCapsule->GetComponentLocation() - FVector(0, 0, OwnerCapsule->Bounds.BoxExtent.Z), OtherCapsule->GetComponentLocation() + FVector(0, 0, OtherCapsule->Bounds.BoxExtent.Z), FColor::Blue, true, 3.f);
				UKismetSystemLibrary::PrintString(this, FString::Printf(TEXT("Extent Difference: %f IsOnTopOf: %hs"), OwnerBottom - OtherTop, IsOnTopOf ? "TRUE" : "FALSE"), true, true);
			}
#endif

			return IsOnTopOf;
		}
	}

	return false;
}

float UICharacterMovementComponent::GetMaxSpeed() const
{
	float MaxSpeed = Super::GetMaxSpeed();
	float MaxSpeedMultiplier = 1.0f;

	AIBaseCharacter* CharOwner = Cast<AIBaseCharacter>(PawnOwner);
	AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(PawnOwner);

	if (DinoOwner)
	{
		if (IsSwimming())
		{
			MaxSpeedMultiplier = DinoOwner->GetSwimSpeedGrowthMultiplier();
		}
		else if (IsFlying())
		{
			MaxSpeedMultiplier = DinoOwner->GetFlySpeedGrowthMultiplier();
		}
		else
		{
			MaxSpeedMultiplier = DinoOwner->GetSpeedGrowthMultiplier();
		}
	}

	if (CharOwner)
	{
		bool bOverrideSpeed = false;
		if (DinoOwner && !IsLimping())
		{
			if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(DinoOwner->AbilitySystem))
			{
				float AbilitySpeedOverride = AbilitySystem->GetAbilityForcedMovementSpeed();
				bOverrideSpeed = AbilitySpeedOverride > 0.0f;
				if (bOverrideSpeed)
				{
					MaxSpeed = AbilitySpeedOverride;
				}
			}
		}
		if (!bOverrideSpeed)
		{
			// Precise Movement is Active
			if (ShouldUsePreciseMovement() && DinoOwner && !IsSwimming())
			{
				if (IsCrouching())
				{
					// Is Reversing
					if (GetPreciseMovementDirection() == EPreciseMovementDirection::REVERSE)
					{
						MaxSpeed = MaxCrouchReverseSpeed;
					}
					// Is Strafing
					else if (GetPreciseMovementDirection() == EPreciseMovementDirection::REVERSELEFTRIGHT || GetPreciseMovementDirection() == EPreciseMovementDirection::LEFTRIGHT)
					{
						MaxSpeed = MaxCrouchStrafeSpeed;
					}
					// If Moving Forward
					else
					{
						MaxSpeed = MaxWalkSpeedCrouched;
					}
				}
				else
				{
					// Is Reversing
					if (GetPreciseMovementDirection() == EPreciseMovementDirection::REVERSE)
					{
						MaxSpeed = IsFlying() ? MaxPreciseFlySpeed : MaxWalkReverseSpeed;
					}
					// Is Strafing
					else if (GetPreciseMovementDirection() == EPreciseMovementDirection::REVERSELEFTRIGHT || GetPreciseMovementDirection() == EPreciseMovementDirection::LEFTRIGHT)
					{
						MaxSpeed = IsFlying() ? MaxPreciseFlySpeed : MaxWalkStrafeSpeed;
					}
					// If Moving Forward
					else
					{
						MaxSpeed = IsFlying() ? MaxPreciseFlySpeed : MaxWalkSpeed;
					}
				}
			}
			else
			{
				// Wants to Stop (Can't Move / Preloading / Resting etc) or Turn In Place
				if (bWantsToStop)
				{
					MaxSpeed = 0.f;
				}
				else if (IsFlying())
				{
					MaxSpeed = (bWantsToSprint && !IsLimping()) || Cast<AIAdminCharacter>(CharOwner) != nullptr ? MaxFlySpeed : MaxFlySlowSpeed;

					if (DinoOwner)
					{
						FVector cameraDirection = DinoOwner->GetControlRotation().Vector();

						FVector normalizedAccel = Acceleration.GetSafeNormal();
						FVector nonVerticalCameraDirection = FVector(cameraDirection.X, cameraDirection.Y, 0.0f).GetSafeNormal();

						float dot = nonVerticalCameraDirection.Dot(normalizedAccel);

						if (dot > 0.5f && cameraDirection.Z < 0.0f && FMath::IsNearlyZero(Acceleration.Z))
						{
							//Lerp between the current speed, and the maximum nosediving speed by how far the camera is looking down.
							float t = cameraDirection.Z * -1.0f;
							MaxSpeed = FMath::InterpEaseIn(MaxSpeed, MaxNosedivingSpeed, t, NosedivingEaseExponent);
						}
					}
				}
				else if (IsLimping())
				{
					if (IsSwimming()) 
					{
						return MaxSwimSlowSpeed * DinoOwner->GetMovementSpeedMultiplier();
					}
					else 
					{
						return MaxWalkSpeed * DinoOwner->GetMovementSpeedMultiplier();
					}
				}
				else if (CharOwner->bIsCrouched)
				{
					if (DinoOwner)
					{
						MaxSpeed = MaxWalkSpeedCrouched;
					}
					else {
						MaxSpeed = MaxWalkSpeedCrouched;
					}
				}
				else if (IsSwimming())
				{
					if (IsTrotting() && !bWantsToSprint && bCanUseAdvancedSwimming && CharOwner->IsAquatic())
					{
						MaxSpeed = MaxSwimTrotSpeed;
					}
					else
					{
						MaxSpeed = bWantsToSprint ? MaxSwimSpeed : MaxSwimSlowSpeed;
					}
				}
				else if (IsSprinting())
				{
					if (DinoOwner)
					{
						MaxSpeed = MaxRunSpeed;
						if (DinoOwner->IsAmbushReady())
						{
							MaxSpeed *= DinoOwner->AmbushSpeedModifier;
						}
					}

					if (AIAdminCharacter* IAdminCharacter = Cast<AIAdminCharacter>(CharOwner))
					{
						MaxSpeed *= 2;
						if (MaxSpeed < IAdminCharacter->MaxFlySpeed)
						{
							MaxSpeed = IAdminCharacter->MaxFlySpeed;
						}
					}
					else
					{
						MaxSpeed *= CharOwner->GetSprintingSpeedModifier();
					}
				}
				else if (IsTrotting())
				{
					if (DinoOwner)
					{
						MaxSpeed = MaxTrotSpeed;
					}

					MaxSpeed *= CharOwner->GetTrottingSpeedModifier();
				}
			}
		}

		//This is the global MovementSpeedMultiplier attribute. It's used for charge abilities etc. and is applied regardless on top of all other modifiers.
		MaxSpeedMultiplier *= CharOwner->GetMovementSpeedMultiplier();
	}

	
		
	MaxSpeed *= MaxSpeedMultiplier;

	if (CharOwner && CharOwner->IsHomecaveCampingDebuffActive() && CharOwner->GetCurrentInstance() == nullptr && !FMath::IsNearlyEqual(MaxSpeed, 0.0f))
	{
		MaxSpeed /= 2.0f;
	}

	// MaxSpeed + Percentage to be deducted or added depending on slope
	return (MaxSpeed + (MaxSpeed * (GetMaxSpeedSlopeModifier())));
}

float UICharacterMovementComponent::GetMaxAcceleration() const
{
	if (ICharacterMovementCVars::CVarDisableCMCAdditions->GetBool() || ICharacterMovementCVars::CVarDisableCustomAcceleration->GetBool())
	{
		return Super::GetMaxAcceleration();
	}

	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetOwner()))
	{
		const float MaxAccelerationMultiplier = DinoOwner->GetAccelerationGrowthMultiplier();

		float GroundAccelerationMultiplier = 1.0f;
		float SwimAccelerationMultiplier = 1.0f;
		if (DinoOwner->AbilitySystem) 
		{
			GroundAccelerationMultiplier = ShouldUsePreciseMovement() ?
				DinoOwner->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetGroundPreciseAccelerationMultiplierAttribute()) : 
				DinoOwner->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetGroundAccelerationMultiplierAttribute());

			SwimAccelerationMultiplier = DinoOwner->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetSwimmingAccelerationMultiplierAttribute());
		}

		const float Speed = Velocity.Size();
		float BaseAcceleration;

		if (IsSwimming() && bCanUseAdvancedSwimming)
		{
			BaseAcceleration = MaxSwimAcceleration * SwimAccelerationMultiplier;
		}
		else if (IsFlying())
		{
			BaseAcceleration = MaxFlyAcceleration;

			if (DinoOwner)
			{
				FVector cameraDirection = DinoOwner->GetControlRotation().Vector();

				FVector normalizedAccel = Acceleration.GetSafeNormal();
				FVector nonVerticalCameraDirection = FVector(cameraDirection.X, cameraDirection.Y, 0.0f).GetSafeNormal();

				float dot = nonVerticalCameraDirection.Dot(normalizedAccel);

				if (dot > 0.5f && cameraDirection.Z < 0.0f)
				{
					//Lerp between the current speed, and the maximum nosediving speed by how far the camera is looking down.
					float t = cameraDirection.Z * -1.0f;
					float MaxDivingAccel = FMath::InterpEaseOut(MaxFlyAcceleration, MaxDiveAcceleration, t, 2.0f);

					BaseAcceleration = MaxDivingAccel;
				}
			}
		}
		else if (Speed > (MaxRunAcceleration * MaxAccelerationMultiplier))
		{
			BaseAcceleration = MaxRunAcceleration * GroundAccelerationMultiplier;
		}
		else if (Speed > (MaxTrotAcceleration * MaxAccelerationMultiplier))
		{
			BaseAcceleration = MaxTrotAcceleration * GroundAccelerationMultiplier;
		}
		else if (Speed > (MaxWalkAcceleration * MaxAccelerationMultiplier))
		{
			BaseAcceleration = MaxWalkAcceleration * GroundAccelerationMultiplier;
		}
		else if (Speed > (MaxCrouchAcceleration * MaxAccelerationMultiplier))
		{
			BaseAcceleration = MaxCrouchAcceleration * GroundAccelerationMultiplier;
		}
		else
		{
			BaseAcceleration = MaxWalkAcceleration * GroundAccelerationMultiplier;
			GEngine->AddOnScreenDebugMessage(140981, 0.0f, FColor::Blue, FString::SanitizeFloat(BaseAcceleration));
		}

		if (DinoOwner && DinoOwner->IsHomecaveCampingDebuffActive() && DinoOwner->GetCurrentInstance() == nullptr && !FMath::IsNearlyEqual(BaseAcceleration, 0.0f))
		{
			return BaseAcceleration /= 2.0f;
		}

		return BaseAcceleration * MaxAccelerationMultiplier;
	}

	if (AIBaseCharacter* CharOwner = Cast<AIBaseCharacter>(PawnOwner))
	{
		if (bWantsToStop)
		{
			return 0.f;
		}
	}

	return Super::GetMaxAcceleration();
}

float UICharacterMovementComponent::GetMaxJumpHeight() const
{
	float JumpHeightMultiplier = 1.f;
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		JumpHeightMultiplier = ASC->GetNumericAttribute(UCoreAttributeSet::GetJumpForceMultiplierAttribute());
	}

	return JumpHeightMultiplier * Super::GetMaxJumpHeight();
	
}

FVector UICharacterMovementComponent::GetAirControl(float DeltaTime, float TickAirControl,
	const FVector& FallAcceleration)
{
	float AirControMultiplier = 1.f;
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		AirControMultiplier = ASC->GetNumericAttribute(UCoreAttributeSet::GetAirControlMultiplierAttribute());
	}

	return Super::GetAirControl(DeltaTime, TickAirControl * AirControMultiplier, FallAcceleration);
}


float UICharacterMovementComponent::GetGravityZ() const
{
	if (AIBaseCharacter* ICharOwner = Cast<AIBaseCharacter>(GetOwner()))
	{
		// Removed Checks:
		// ICharOwner->GetController() == nullptr
		// ICharOwner->bCombatLogAI

		if (ICharOwner->IsPreloadingClientArea() || ICharOwner->IsValidatingInstance()) return 0.0f;
	}

	return Super::GetGravityZ();
}

FRotator UICharacterMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetOwner()))
	{
		const float Speed = Velocity.Size();
		float RoundedSpeed = 50.f * (FMath::RoundToFloat(Speed / 50.f));

		return DinoOwner->GetRotationRateForSpeed(Speed) * DeltaTime;
	}
	
	return Super::GetDeltaRotation(DeltaTime);
}

void UICharacterMovementComponent::DisableAdditionalCapsules(bool bPermanentlyIgnoreAllChannels)
{
	for (UCapsuleComponent* AC : AdditionalCapsules)
	{
		if (AC != nullptr && AC != UpdatedComponent)
		{
			AC->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			if (bPermanentlyIgnoreAllChannels)
			{
				AC->SetCollisionResponseToAllChannels(ECR_Ignore);
			}
		}
	}
}

void UICharacterMovementComponent::EnableAdditionalCapsules()
{
	for (UCapsuleComponent* AC : AdditionalCapsules)
	{
		if (AC != nullptr && AC != UpdatedComponent)
		{
			AC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

const TArray<UCapsuleComponent*>& UICharacterMovementComponent::GetAdditionalCapsules()
{
	return AdditionalCapsules;
}

void UICharacterMovementComponent::SetCollisionResponseToDefault()
{
	for (UCapsuleComponent* CapComponent : GetAdditionalCapsules())
	{
		if (!ensureAlways(CapComponent))
		{
			continue;
		}
		const FStoredCollisionInfo* const DefaultCollision = AdditionalCapsuleDefaultCollisionProfiles.Find(CapComponent);
		if (!ensureAlways(DefaultCollision))
		{
			continue;
		}
		CapComponent->SetCollisionEnabled(DefaultCollision->CollisionEnabled);
		CapComponent->SetCollisionObjectType(DefaultCollision->ObjectType);
		CapComponent->SetCollisionResponseToChannels(DefaultCollision->Responses);
	}
}

bool UICharacterMovementComponent::CanTransitionToState(ECapsuleState InState) const
{
	for (UStateAdjustableCapsuleComponent* SACC : AdditionalAdjustableCapsules)
	{
		if (SACC != nullptr && !SACC->CanTransitionToState(InState))
		{
			return false;
		}
	}

	return true;
}

void UICharacterMovementComponent::PushOffBase()
{
	if (!UpdatedComponent || !GetMovementBase()) { return; }

	const FRotator LookAtRotation = UKismetMathLibrary::FindLookAtRotation(GetActorLocation(), GetMovementBase()->GetComponentLocation());
	FVector LookAwayDirection = LookAtRotation.GetNormalized().Vector() * -1;

	// Clear the vertical component and re-normalize so that the player always slides horizontally at a fixed velocity
	LookAwayDirection.Z = 0.f;
	LookAwayDirection.Normalize();

	// Set falling to prevent the player from doing anything
	SetMovementMode(MOVE_Falling);
	Velocity = LookAwayDirection * SlideOffBaseVelocity;
}

void UICharacterMovementComponent::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	if (AIBaseCharacter* ICharOwner = Cast<AIBaseCharacter>(GetCharacterOwner()))
	{
		bWantsToSprint = (Flags & FSavedMove_ICharacter::FLAG_Sprinting) != 0;
		bWantsToTrot = (Flags & FSavedMove_ICharacter::FLAG_Trotting) != 0;
		bWantsToLimp = (Flags & FSavedMove_ICharacter::FLAG_Limping) != 0;
		bWantsToStop = (Flags & FSavedMove_ICharacter::FLAG_Stop) != 0;
		bWantsToPreciseMove = (Flags & FSavedMove_ICharacter::FLAG_PreciseMove) != 0;

		// TODO: need to enforce all state corrections
		// Example: State Corrections
		//const bool bClientDesiresRest = (Flags & FSavedMove_ICharacter::FLAG_Resting) != 0;
		//ICharOwner->SetDesiresRest(bClientDesiresRest);
		//if (ICharOwner->DesiresRest() != bClientDesiresRest)
		//{
		//	//ClientDisableDesiresRest(); //if SetDesiresRest failed on the server we need to correct the client
		//}
	}
}

void UICharacterMovementComponent::JumpOff(AActor* MovementBaseActor)
{
	if (!ICharacterMovementCVars::CVarDisableInvalidSurfaceHandling->GetBool() && !ICharacterMovementCVars::CVarDisableCMCAdditions->GetBool())
	{
		// When enabled, this is handled via @CalcVelocity instead
		return;
	}

	return Super::JumpOff(MovementBaseActor);
}

bool UICharacterMovementComponent::CanAttemptJump() const
{
	return Super::CanAttemptJump() && GetMaxJumpHeight() > 0.f;
}

bool UICharacterMovementComponent::DoJump(bool bReplayingMoves)
{
	if (CharacterOwner && CharacterOwner->CanJump())
	{
		// Don't jump if we can't move up/down.
		if (!bConstrainToPlane || FMath::Abs(PlaneConstraintNormal.Z) != 1.f)
		{
			float JumpHeightMultiplier = 1.f; 
			if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
			{
				JumpHeightMultiplier = ASC->GetNumericAttribute(UCoreAttributeSet::GetJumpForceMultiplierAttribute());
			}

			if (JumpHeightMultiplier >= 0)
			{
				Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity * FMath::Pow(JumpHeightMultiplier, 0.5));
			}
			else
			{
				Velocity.Z = 0;
			}
			
			SetMovementMode(MOVE_Falling);
			return true;
		}
	}

	return false;
}

void UICharacterMovementComponent::UnCrouch(bool bClientSimulation /* = false */)
{
	if (!HasValidData())
	{
		return;
	}

	ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight())
	{
		if (!bClientSimulation)
		{
			CharacterOwner->bIsCrouched = false;
		}
		CharacterOwner->OnEndCrouch(0.f, 0.f);
		return;
	}

	const float CurrentCrouchedHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - OldUnscaledHalfHeight;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();

	// Grow to uncrouched size.
	check(CharacterOwner->GetCapsuleComponent());

	if (!bClientSimulation)
	{
		// Try to stay in place and see if the larger capsule fits. We use a slightly taller capsule to avoid penetration.
		const UWorld* MyWorld = GetWorld();
		const float SweepInflation = KINDA_SMALL_NUMBER * 10.f;
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(CrouchTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);

		// Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation - ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		bool bEncroached = true;

		if (!bCrouchMaintainsBaseLocation)
		{
			// Expand in place
			bEncroached = MyWorld->OverlapBlockingTestByChannel(PawnLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
			
			if (bEncroached)
			{
				// Try adjusting capsule position to see if we can avoid encroachment.
				if (ScaledHalfHeightAdjust > 0.f)
				{
					// Shrink to a short capsule, sweep down to base to find where that would hit something, and then try to stand up from there.
					float PawnRadius, PawnHalfHeight;
					CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
					const float ShrinkHalfHeight = PawnHalfHeight - PawnRadius;
					const float TraceDist = PawnHalfHeight - ShrinkHalfHeight;
					const FVector Down = FVector(0.f, 0.f, -TraceDist);

					FHitResult Hit(1.f);
					const FCollisionShape ShortCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, ShrinkHalfHeight);
					const bool bBlockingHit = MyWorld->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + Down, FQuat::Identity, CollisionChannel, ShortCapsuleShape, CapsuleParams);
					if (Hit.bStartPenetrating)
					{
						bEncroached = true;
					}
					else
					{
						// Compute where the base of the sweep ended up, and see if we can stand there
						const float DistanceToBase = (Hit.Time * TraceDist) + ShortCapsuleShape.Capsule.HalfHeight;
						const FVector NewLoc = FVector(PawnLocation.X, PawnLocation.Y, PawnLocation.Z - DistanceToBase + StandingCapsuleShape.Capsule.HalfHeight + SweepInflation + MIN_FLOOR_DIST / 2.f);
						bEncroached = MyWorld->OverlapBlockingTestByChannel(NewLoc, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
						if (!bEncroached)
						{
							// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
							UpdatedComponent->MoveComponent(NewLoc - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::None);
						}
					}
				}
			}
		}
		else
		{
			// Expand while keeping base location the same.
			FVector StandingLocation = PawnLocation + FVector(0.f, 0.f, StandingCapsuleShape.GetCapsuleHalfHeight() - CurrentCrouchedHalfHeight);
			bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				if (IsMovingOnGround())
				{
					// Something might be just barely overhead, try moving down closer to the floor to avoid it.
					const float MinFloorDist = KINDA_SMALL_NUMBER * 10.f;
					if (CurrentFloor.bBlockingHit && CurrentFloor.FloorDist > MinFloorDist)
					{
						StandingLocation.Z -= CurrentFloor.FloorDist - MinFloorDist;
						bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
					}
				}
			}

			if (!bEncroached)
			{
				// Commit the change in location.
				UpdatedComponent->MoveComponent(StandingLocation - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::None);
				bForceNextFloorCheck = true;
			}
		}

		// If still encroached then abort.
		if (bEncroached)
		{
			return;
		}

		CharacterOwner->bIsCrouched = false;
	}
	else
	{
		bShrinkProxyCapsule = true;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);

	const float MeshAdjust = ScaledHalfHeightAdjust;
	AdjustProxyCapsuleSize();
	CharacterOwner->OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset += FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

FRotator UICharacterMovementComponent::ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const
{
	//dinosaurs update their rotation during phys movement (affects everything so it must be done there)
	if (Cast<AIDinosaurCharacter>(GetCharacterOwner()) && Cast<APlayerController>(GetCharacterOwner()->GetController()))
	{
		return CurrentRotation;
	} else {
		return Super::ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRotation);
	}
}

void UICharacterMovementComponent::OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float Lifetime, float CurrentMoveError)
{
	Super::OnTimeDiscrepancyDetected(CurrentTimeDiscrepancy, LifetimeRawTimeDiscrepancy, Lifetime, CurrentMoveError);

	const UIGameInstance* const IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance)
	{
		return;
	}
	AIGameSession* const Session = IGameInstance->GetGameSession();
	check(Session);
	if (!Session)
	{
		return;
	}

	if (!GetCharacterOwner())
	{
		return;
	}

	if (Session->SpeedhackThreshold <= 0 || Session->SpeedhackDetection <= 0)
	{
		// No action to be taken
		return;
	}

	if (FMath::Abs(Session->SpeedhackMinimumLifetimeDiscrepancy) < 0.01f)
	{
		// if zero, set very low to be "disabled"
		Session->SpeedhackMinimumLifetimeDiscrepancy = -9999999.9f;
	}

	if (FMath::Abs(Session->SpeedhackMaximumLifetimeDiscrepancy) < 0.01f)
	{
		// if zero, set very high to be "disabled"
		Session->SpeedhackMaximumLifetimeDiscrepancy = 9999999.9f;
	}

	if (LifetimeRawTimeDiscrepancy > Session->SpeedhackMinimumLifetimeDiscrepancy && LifetimeRawTimeDiscrepancy < Session->SpeedhackMaximumLifetimeDiscrepancy)
	{
		// Lifetime discrepancy is within allowable limits
		return;
	}

	DetectedSpeedhacks ++;

	if (DetectedSpeedhacks >= Session->SpeedhackThreshold)
	{
		DetectedSpeedhacks = 0;

		FString PlayerName = TEXT("ERROR");
		FString AGID = TEXT("ERROR");

		const AIPlayerState* const IPlayerState = Cast<AIPlayerState>(GetCharacterOwner()->GetPlayerState());
		check(IPlayerState);
		if (!IPlayerState)
		{
			return;
		}

		PlayerName = IPlayerState->GetPlayerName();
		AGID = IPlayerState->GetAlderonID().ToDisplayString();
		float Ping = IPlayerState->GetPing();
		
		float TimeDiscrepancyBias = 0.1f;
		if (LifetimeRawTimeDiscrepancy < 0.0f)
		{
			// Player is likely lagging but don't completely rule it out.
			TimeDiscrepancyBias *= 5.0f;
		}

		const float PercentageTimeDiscrepancy = FMath::Clamp(FMath::Abs(LifetimeRawTimeDiscrepancy / Lifetime) / TimeDiscrepancyBias, 0.0f, 1.0f);
		const float EstimatedHackerProbability = (1.0f - FMath::Pow(1.0f - PercentageTimeDiscrepancy, 2.0f)) * 100.0f;

		UE_LOG(TitansLog, Warning, TEXT("Speedhack detected! AGID: %s, Player Name: %s. CurrentTimeDiscrepancy: %f, LifetimeRawTimeDiscrepancy: %f, Lifetime: %f, CurrentMoveError %f, Pct Discrepancy %f, EstimatedHackerProbability %f, Ping %f"),
			*AGID,
			*PlayerName,
			CurrentTimeDiscrepancy,
			LifetimeRawTimeDiscrepancy,
			Lifetime,
			CurrentMoveError,
			PercentageTimeDiscrepancy,
			EstimatedHackerProbability,
			Ping);

		if (AIGameSession::UseWebHooks(WEBHOOK_PlayerHack))
		{
			TMap<FString, TSharedPtr<FJsonValue>> WebHookProperties
			{
				{ TEXT("ServerIP"), MakeShareable(new FJsonValueString(Session->GetServerListIP() + TEXT(":") + FString::FromInt(Session->GetServerListPort())))},
				{ TEXT("ServerName"), MakeShareable(new FJsonValueString(Session->GetServerName())) },
				{ TEXT("PlayerName"), MakeShareable(new FJsonValueString(IPlayerState->GetPlayerName())) },
				{ TEXT("PlayerAlderonId"), MakeShareable(new FJsonValueString(IPlayerState->GetAlderonID().ToDisplayString())) },
				{ TEXT("Platform"), MakeShareable(new FJsonValueString(PlatformToServerApiString(IPlayerState->GetPlatform()))) },
				{ TEXT("CurrentTimeDiscrepancy"), MakeShareable(new FJsonValueNumber(CurrentTimeDiscrepancy)) },
				{ TEXT("LifetimeRawTimeDiscrepancy"), MakeShareable(new FJsonValueNumber(LifetimeRawTimeDiscrepancy)) },
				{ TEXT("Lifetime"), MakeShareable(new FJsonValueNumber(Lifetime)) },
				{ TEXT("CurrentMoveError"), MakeShareable(new FJsonValueNumber(CurrentMoveError)) },
				{ TEXT("PercentageTimeDiscrepancy"), MakeShareable(new FJsonValueNumber(PercentageTimeDiscrepancy)) },
				{ TEXT("EstimatedHackerProbability"), MakeShareable(new FJsonValueNumber(EstimatedHackerProbability)) },
				{ TEXT("Ping"), MakeShareable(new FJsonValueNumber(Ping)) },
			};

			AIGameSession::TriggerWebHookFromContext(this, WEBHOOK_PlayerHack, WebHookProperties);
		}

		switch (Session->SpeedhackDetection)
		{
		case 1: // Log
			break;
		case 2: // Kick
			Session->KickPlayer(GetCharacterOwner()->GetController<APlayerController>(), FText::FromStringTable(TEXT("ST_AntiCheat"), TEXT("SpeedhackDetected")));
			break;
		case 3: // Ban
			Session->BanPlayer(GetCharacterOwner()->GetController<APlayerController>(), FText::FromStringTable(TEXT("ST_AntiCheat"), TEXT("SpeedhackDetected")));
			break;
		}
	}
	else if (GetCharacterOwner()->GetWorldTimerManager().GetTimerRemaining(TimerHandle_SpeedhackReduction) <= 0)
	{
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UICharacterMovementComponent::ReduceSpeedhackDetection);
		GetCharacterOwner()->GetWorldTimerManager().SetTimer(TimerHandle_SpeedhackReduction, Del, FMath::Max(60.0f / Session->SpeedhackThreshold, 1.0f), false);
	}
}

void UICharacterMovementComponent::TimeDilationManipulationDetected()
{
	if (bTimeDilationReportSent)
	{
		return;
	}
	
	AIBaseCharacter* const OwnerActor = Cast<AIBaseCharacter>(GetOwner());
	if (!ensureAlways(OwnerActor))
	{
		return;
	}

	const AIPlayerState* const OwnerPlayerState = OwnerActor->GetPlayerState<const AIPlayerState>();
	if (!ensureAlways(OwnerPlayerState))
	{
		return;
	}

	const UIGameInstance* const IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	if (!ensureAlways(IGameInstance))
	{
		return;
	}

	bTimeDilationReportSent = true;

	AIGameSession* const Session = IGameInstance->GetGameSession();
	if (ensureAlways(Session))
	{
		//Session->KickPlayer(OwnerActor->GetController<APlayerController>(), FText::FromStringTable(TEXT("ST_AntiCheat"), TEXT("SpeedhackDetected")));

		UE_LOG(TitansLog, Warning, TEXT("Time Dilation Manipulation Detected: PlayerName %s, AlderonID %s, Platform %s"),
			*OwnerPlayerState->GetPlayerName(),
			*OwnerPlayerState->GetAlderonID().ToDisplayString(),
			*PlatformToServerApiString(OwnerPlayerState->GetPlatform()));

		if (AIGameSession::UseWebHooks(WEBHOOK_PlayerHack))
		{
			TMap<FString, TSharedPtr<FJsonValue>> WebHookProperties
			{
				{ TEXT("ServerName"), MakeShareable(new FJsonValueString(Session->GetServerName()))},
				{ TEXT("ServerIP"), MakeShareable(new FJsonValueString(Session->GetServerListIP() + ":" + FString::FromInt(Session->GetServerListPort())))},
				{ TEXT("PlayerName"), MakeShareable(new FJsonValueString(OwnerPlayerState->GetPlayerName()))},
				{ TEXT("PlayerAlderonId"), MakeShareable(new FJsonValueString(OwnerPlayerState->GetAlderonID().ToDisplayString()))},
				{ TEXT("Platform"), MakeShareable(new FJsonValueString(PlatformToServerApiString(OwnerPlayerState->GetPlatform())))},
				{ TEXT("Reason"), MakeShareable(new FJsonValueString(TEXT("Time Dilation Manipulation")))},
			};
			
			AIGameSession::TriggerWebHookFromContext(OwnerActor, WEBHOOK_PlayerHack, WebHookProperties);
		}
	}	
}

void UICharacterMovementComponent::ReduceSpeedhackDetection()
{
	UIGameInstance* IGameInstance = UIGameplayStatics::GetIGameInstance(this);
	check(IGameInstance);
	if (!IGameInstance)
	{
		return;
	}
	AIGameSession* Session = IGameInstance->GetGameSession();
	check(Session);
	if (!Session)
	{
		return;
	}
	if (!GetCharacterOwner())
	{
		DetectedSpeedhacks = 0;
		return;
	}
	DetectedSpeedhacks--;
	if (DetectedSpeedhacks < 0)
	{
		DetectedSpeedhacks = 0;
	}
	if (DetectedSpeedhacks > 0)
	{
		FTimerDelegate Del = FTimerDelegate::CreateUObject(this, &UICharacterMovementComponent::ReduceSpeedhackDetection);
		GetCharacterOwner()->GetWorldTimerManager().SetTimer(TimerHandle_SpeedhackReduction, Del, FMath::Max(60.0f / Session->SpeedhackThreshold, 1.0f), false);
	}
}

void UICharacterMovementComponent::Launch(FVector const& LaunchVel)
{
	// Launch velocity handled in CalcVelocity.
	if (!IsSwimming())
	{
		SetMovementMode(MOVE_Falling);
	}
	bForceNextFloorCheck = true;
}

void UICharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (ICharacterMovementCVars::CVarDisableCMCAdditions->GetBool())
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
		return;
	}

	if (!ICharacterMovementCVars::CVarDisableInvalidSurfaceHandling->GetBool())
	{
		// This is done in CalcVelocity rather than in @JumpOff because it needs to be continuously updated until the base
		// is valid/can be stood on by this character
		if (IsStandingOnCharacter())
		{
			PushOffBase();
		}
	}

	AIAdminCharacter* AdminCharacter = Cast<AIAdminCharacter>(GetCharacterOwner());
	if (AdminCharacter && AdminCharacter->IsFollowingDinosaur())
	{
		Velocity = FVector::ZeroVector;
		PreLaunchVelocity = FVector::ZeroVector;
		bHasPreLaunchVelocity = false;
		Acceleration = PreLaunchVelocity;
		return;
	}

	AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner());
	
	// we should -NOT- have a change in rotation during this function. Rotation must change during PhysicsRotation
	FRotator PreVelocityRotation = UpdatedComponent->GetComponentRotation();
	ON_SCOPE_EXIT
	{
		//check(PreVelocityRotation == UpdatedComponent->GetComponentRotation());

		if (DinoOwner && DinoOwner->HasAuthority())
		{
			DinoOwner->SetReplicatedAccelerationNormal(Acceleration.GetSafeNormal());
		}
	};

	UpdateRotationMethod();

	if (bHasPreLaunchVelocity)
	{
		Velocity = PreLaunchVelocity;
		PreLaunchVelocity = FVector::ZeroVector;
	}

	if (DinoOwner)
	{
		if (DinoOwner->bIsCharacterEditorPreviewCharacter)
		{
			Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
			return;
		}

		if (bTransitioningPreciseMovement)
		{
			elapsedTransitionPreciseMovementTime += DeltaTime;
			if (elapsedTransitionPreciseMovementTime >= TransitionPreciseMovementTime)
			{			
				elapsedTransitionPreciseMovementTime = 0.0f;
				bTransitioningPreciseMovement = false;

				DinoOwner->OnStopPreciseMovement();
			}
		}

		if (!DinoOwner->IsPreloadingClientArea() && !DinoOwner->IsValidatingInstance())
		{
			if (DinoOwner->IsStunned())
			{
				CalcVelocityStunned(DeltaTime, Friction, bFluid, BrakingDeceleration);
				ApplyLaunchVelocity();
				return;
			}

			if (HasRootMotionSources())
			{
				CalcVelocityRootMotionSource(DeltaTime, Friction, bFluid, BrakingDeceleration);
				ApplyLaunchVelocity();
				return;
			}
			else
			{
				if ((ShouldUsePreciseMovement() || DinoOwner->IsStunned()) && !IsSwimming() && !IsFlying())
				{
					CalcVelocityPrecise(DeltaTime, Friction, bFluid, BrakingDeceleration);
					ApplyLaunchVelocity();
					return;
				}

				if (bCanUseAdvancedSwimming && IsSwimming())
				{
					CalcVelocitySwimming(DeltaTime, Friction, bFluid, BrakingDeceleration);
					ApplyLaunchVelocity();
					return;
				}

				if (IsFlying())
				{
					CalcVelocityFlying(DeltaTime, Friction, bFluid, BrakingDeceleration);
					ApplyLaunchVelocity();
					return;
				}

				if (!IsFalling())
				{
					CalcVelocityNormal(DeltaTime, Friction, bFluid, BrakingDeceleration);
					ApplyLaunchVelocity();
					return;
				}
			}
		}
		else
		{
			Velocity = FVector::ZeroVector;
			PreLaunchVelocity = FVector::ZeroVector;
			bHasPreLaunchVelocity = false;
			Acceleration = PreLaunchVelocity;
			return;
		}

	}
	// Ai use this
	ApplyLaunchVelocityBraking(DeltaTime, Friction);
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	ApplyLaunchVelocity();

}

void UICharacterMovementComponent::ApplyLaunchVelocity()
{
	PreLaunchVelocity = FVector::ZeroVector;
	bHasPreLaunchVelocity = false;

	AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner());
	if (!DinoOwner) return;
	if (!DinoOwner->GetLaunchVelocity().IsZero())
	{
		PreLaunchVelocity = Velocity;
		bHasPreLaunchVelocity = true;
		Velocity += DinoOwner->GetLaunchVelocity();
	}
}

void UICharacterMovementComponent::CalcVelocityRootMotionSource(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (HasValidData() && !HasAnimRootMotion() && DeltaTime >= MIN_TICK_TIME && (!CharacterOwner || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy))
	{
		
		// Rotation and Velocity is managed in the root motion. 
		// However acceleration can be modified here for e.g. AI.
	}
}

void UICharacterMovementComponent::CalcVelocityNormal(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (HasValidData() && !HasAnimRootMotion() && DeltaTime >= MIN_TICK_TIME && (!CharacterOwner || CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy))
	{
		AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);

		//Acceleration = ControllerRotation.RotateVector()
		//Friction = FMath::Max(0.f, Friction);
		const float MaxAccel = GetMaxAcceleration();
		float MaxSpeed = GetMaxSpeed();

		// Check if path following requested movement
		bool bZeroRequestedAcceleration = true;
		FVector RequestedAcceleration = FVector::ZeroVector;
		float RequestedSpeed = 0.0f;
		if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
		{
			RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel);
			bZeroRequestedAcceleration = false;
		}

		bool bAbilityMaxAccel = false;
		if (AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(GetCharacterOwner()))
		{
			if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(IBaseCharacter->AbilitySystem))
			{
				bAbilityMaxAccel = AbilitySystem->HasAbilityForcedMovementSpeed();
			}
		}

		if (bForceMaxAccel || bAbilityMaxAccel)
		{
			// Force acceleration at full speed.
			// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
			if (Acceleration.SizeSquared() > SMALL_NUMBER)
			{
				Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
			}
			else
			{
				Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
			}

			AnalogInputModifier = 1.f;
		}

		// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
		// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
		MaxSpeed = FMath::Max(RequestedSpeed, MaxSpeed * AnalogInputModifier);

		// Apply braking or deceleration
		const bool bZeroAcceleration = Acceleration.IsZero();
		const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

		FRotator ToRot = bZeroAcceleration ? UpdatedComponent->GetComponentRotation() : Acceleration.Rotation();
		FRotator FromRot = UpdatedComponent->GetComponentRotation();

		if (IsMovingOnGround())
		{
			ToRot.Pitch = 0.f;
			FromRot.Pitch = 0.f;
		}

		if (IGameState && IGameState->IsNewCharacterMovement())
		{
			// Set the desired "to" rotation to update in PhysicsRotation()
			CalculatedRotator = ToRot;
		}

		const float AngleTolerance = 5e-2f;
		const FRotator NewDirection = RotateToFrom(ToRot, FromRot, AngleTolerance, DeltaTime);
		Velocity = NewDirection.Vector() * Velocity.Size();
		FVector AppliedAcceleration = NewDirection.Vector() * Acceleration.Size();

		ApplyLaunchVelocityBraking(DeltaTime, Friction);

		// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
		if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
		{
			const FVector OldVelocity = Velocity;

			const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
			ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

			// Don't allow braking to lower us below max speed if we started above it.
			if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(AppliedAcceleration, OldVelocity) > 0.0f)
			{
				Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
			}
		}
		else if (!bZeroAcceleration)
		{
			// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
			const FVector AccelDir = AppliedAcceleration.GetSafeNormal();
			const float VelSize = Velocity.Size();
			Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
		}

		// Apply fluid friction
		if (bFluid)
		{
			Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
		}

		// Apply acceleration
		const float NewMaxSpeed = (IsExceedingMaxSpeed(MaxSpeed)) ? Velocity.Size() : MaxSpeed;
		Velocity += AppliedAcceleration * DeltaTime;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);

		if (!IGameState || !IGameState->IsNewCharacterMovement())
		{
			// Rotates Pawn to movement direction....
			UpdatedComponent->MoveComponent(FVector::ZeroVector, NewDirection, false);
			//if (UpdatedComponent->GetOwner()->GetNetMode() != ENetMode::NM_Client)
			//{
			//	GEngine->AddOnScreenDebugMessage(12312, 0.1f, FColor::Red, TEXT("Server UpdatedComponent CalcVelocityNormal"));
			//}
			//else
			//{
			//	GEngine->AddOnScreenDebugMessage(12313, 0.1f, FColor::Red, TEXT("Client UpdatedComponent CalcVelocityNormal"));
			//}
		}

		if (bUseRVOAvoidance)
		{
			CalcAvoidanceVelocity(DeltaTime);
		}
	}
	else
	{
		Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
	}
}

void UICharacterMovementComponent::CalcVelocityPrecise(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Do not update velocity when using root motion or when SimulatedProxy - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy))
	{
		return;
	}

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	double MaxCharacterSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxCharacterSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxInputSpeed = FMath::Max(MaxCharacterSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	float MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	FRotator ToRot = FRotator();
	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		const AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner());

		if (DinoOwner && DinoOwner->IsEatingOrDrinking() && !DinoOwner->IsCarryingObject())
		{
			// Don't rotate while eating/drinking, unless eating a carried piece of meat
			ToRot = UpdatedComponent->GetComponentRotation();
		}
		else
		{
			const float InputControlDifference = UKismetAnimationLibrary::CalculateDirection(Acceleration, FRotator(0.0f, GetCharacterOwner()->GetControlRotation().Yaw, 0.0f));

			const FRotator CharacterInputDirection = FRotator(0.0, GetCharacterOwner()->GetActorRotation().Yaw + InputControlDifference, 0.0);

			if (!DinoOwner->HasAuthority())
			{
				const float InputActorDifference = UKismetAnimationLibrary::CalculateDirection(Acceleration, FRotator(0.0f, GetCharacterOwner()->GetActorRotation().Yaw, 0.0f));

				const float AbsInputControlDifference = FMath::Abs(InputControlDifference);
				const float AbsInputActorDifference = FMath::Abs(InputActorDifference);

				if (bZeroAcceleration)
				{
					bRotateToDirection = false;
				}
				else
				{
					if (bRotateToDirection)
					{
						if ((AbsInputControlDifference > 50.0 && AbsInputActorDifference > 50.0)) // break the state if rotation is too much
						{
							bRotateToDirection = false;
						}
					}
					else if(AbsInputControlDifference > 0.1 && AbsInputControlDifference <= 50.0)
					{
						bRotateToDirection = true;
					}
				}
			}

			if (!bRotateToDirection)
			{
				float Length = Acceleration.Size();
				Acceleration = CharacterInputDirection.Vector() * Length;
			}

			ToRot = (bRotateToDirection) ? Acceleration.Rotation() : FRotator(0.0f, GetCharacterOwner()->GetControlRotation().Yaw, 0.0f);
		}
	}
	else
	{
		ToRot = GetPreciseMovementDirection() == EPreciseMovementDirection::FORWARD ? Acceleration.Rotation() : FRotator(0.0f, GetCharacterOwner()->GetControlRotation().Yaw, 0.0f);
	}


	FRotator FromRot = UpdatedComponent->GetComponentRotation();

	if (IsMovingOnGround())
	{
		ToRot.Pitch = 0.f;
		FromRot.Pitch = 0.f;
	}

	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		// Set the desired "to" rotation to update in PhysicsRotation()
		CalculatedRotator = ToRot;
	}

	const float AngleTolerance = 5e-2f;
	const FRotator NewDirection = RotateToFrom(ToRot, FromRot, AngleTolerance, DeltaTime);

	ApplyLaunchVelocityBraking(DeltaTime, Friction);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	if (!IGameState || !IGameState->IsNewCharacterMovement())
	{
		// Rotates Pawn to movement direction....
		UpdatedComponent->MoveComponent(FVector::ZeroVector, NewDirection, false);
	}
	else
	{
		double CurrentSpeed = Velocity.Size();
		float RotationMultiplierBase = 2.0; // base rotation speed

		if (CurrentSpeed <= MaxCharacterSpeed)
		{
			RotationMultiplier = RotationMultiplierBase;
		}
		else
		{
			RotationMultiplier = FMath::Max(FMath::Clamp(1.0 / ((((CurrentSpeed - MaxCharacterSpeed) / MaxCharacterSpeed) * 5.0) + 1.0), 0, 1.0) * RotationMultiplierBase, 0.1); // rotate less with a higher speed, max rotation at lower than max speed, minimum rotation 0.1
		}
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}

void UICharacterMovementComponent::CalcVelocityStunned(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
	MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	//FRotator ToRot = GetPreciseMovementDirection() == EPreciseMovementDirection::FORWARD ? Acceleration.Rotation() : FRotator(0.0f, GetCharacterOwner()->GetControlRotation().Yaw, 0.0f);
	FRotator ToRot = Acceleration.Rotation();
	FRotator FromRot = UpdatedComponent->GetComponentRotation();

	if (IsMovingOnGround())
	{
		ToRot.Pitch = 0.f;
		FromRot.Pitch = 0.f;
	}

	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		// Set the desired "to" rotation to update in PhysicsRotation()
		CalculatedRotator = ToRot;
	}

	const float AngleTolerance = 5e-2f;
	const FRotator NewDirection = RotateToFrom(ToRot, FromRot, AngleTolerance, DeltaTime);

	ApplyLaunchVelocityBraking(DeltaTime, Friction);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}

	// Apply input acceleration
	if (!bZeroAcceleration)
	{
		const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
	}

	// Apply additional requested acceleration
	if (!bZeroRequestedAcceleration)
	{
		const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
	}

	if (!IGameState || !IGameState->IsNewCharacterMovement())
	{
		// Rotates Pawn to movement direction....
		UpdatedComponent->MoveComponent(FVector::ZeroVector, NewDirection, false);
	}
}

float UICharacterMovementComponent::ImmersionDepth() const
{
	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(DinoOwner->AbilitySystem))
		{
			if (AbilitySystem->OverrideMovementImmersionDepth())
			{
				return 1.0f;
			}
		}
	}

	return Super::ImmersionDepth();
}

void UICharacterMovementComponent::CalcVelocitySwimming(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	const AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner());
	if (!DinoOwner)
	{
		return;
	}
	
	// Update buoyancy based on input and movement type
	UpdateBuoyancy(*DinoOwner);

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel);
		bZeroRequestedAcceleration = false;
	}

	const UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(DinoOwner->AbilitySystem);
	const bool bAbilityMaxAccel = AbilitySystem ? AbilitySystem->HasAbilityForcedMovementSpeed() : false;
	const bool bAbilityVerticalControl = AbilitySystem && AbilitySystem->HasAbilityVerticalControlInWater();

	if (bForceMaxAccel || bAbilityMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			if (bAbilityVerticalControl)
			{
				Acceleration = MaxAccel * UpdatedComponent->GetForwardVector();
				Acceleration.Z = 0.0f;
			}
			else
			{
				Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
			}
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	MaxSpeed = FMath::Max(RequestedSpeed, MaxSpeed * AnalogInputModifier);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bOnlyZAcceleration = (Acceleration.X == 0.0f && Acceleration.Y == 0.0f && !bZeroAcceleration);
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	FRotator ToRot = FRotator();
	const FRotator FromRot = UpdatedComponent->GetComponentRotation();

	const float ImmersionDepthValue = ImmersionDepth();
	const bool bPreciseMove = ShouldUsePreciseMovement();
	const bool bAtSurface = IsAtWaterSurface();

	const AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		if (DinoOwner->IsEatingOrDrinking() && !DinoOwner->IsCarryingObject())
		{
			ToRot = UpdatedComponent->GetComponentRotation();
		}
		else
		{

			if (!bPreciseMove)
			{
				if (bAtSurface)
				{
					ToRot = Acceleration.Size() > 0.1f ? (Acceleration * FVector(1, 1, 0)).Rotation() : UpdatedComponent->GetComponentRotation();
				}
				else
				{
					ToRot = Acceleration.Size() > 0.1f ? Acceleration.Rotation() : UpdatedComponent->GetComponentRotation();
				}
				if (bOnlyZAcceleration)
				{
					ToRot.Yaw = UpdatedComponent->GetComponentRotation().Yaw;
				}
			}
			else
			{
				float TargetPitch = 0;
				if (!bAtSurface)
				{
					const float ZAcceleration = Acceleration.Z;
					if (fabsf(ZAcceleration) > 0.0f && !DinoOwner->IsDiving())
					{
						TargetPitch = FRotator::NormalizeAxis(Acceleration.Rotation().Pitch);

						FVector NormalizedAccel = Acceleration * FVector(1, 1, 0);
						NormalizedAccel.Normalize();
						FVector NormalizedForward = DinoOwner->GetControlRotation().Vector() * FVector(1, 1, 0);
						NormalizedForward.Normalize();
						float DotProduct = FMath::Clamp(FVector::DotProduct(NormalizedAccel, NormalizedForward) * 2.0f, -1.0f, 1.0f);
						if (DotProduct < 0.0f)
						{
							TargetPitch *= -1.0f;
						}
					}
					else
					{
						TargetPitch = FRotator::NormalizeAxis(DinoOwner->GetControlRotation().Pitch);
					}
				}

				ToRot = DinoOwner->GetControlRotation();
				ToRot.Pitch = TargetPitch;
			}

			// Remap 0.5-1.0 to 0.0-1.0
			const float RemappedImmersionDepth = FMath::Clamp((ImmersionDepthValue - 0.5f) * 2.0f, 0.0f, 1.0f);
			const float PitchMax = FMath::Lerp(0, 60, RemappedImmersionDepth);

			ToRot.Pitch = FMath::Clamp(FRotator::NormalizeAxis(ToRot.Pitch), -PitchMax, PitchMax);
			ToRot.Roll = 0.0f;
		}

		// Set the desired "to" rotation to update in PhysicsRotation()
		CalculatedRotator = ToRot;
	}
	else
	{
		ToRot = (Acceleration.X == 0.0f && Acceleration.Y == 0.0f) ? UpdatedComponent->GetComponentRotation() : Acceleration.Rotation();
	}

	const float AngleTolerance = 5e-2f;
	const FRotator NewDirection = RotateToFrom(ToRot, FromRot, AngleTolerance, DeltaTime);
		
	const FVector AppliedAcceleration = NewDirection.Vector() * Acceleration.Size();

	ApplyLaunchVelocityBraking(DeltaTime, Friction);

	const FVector OldVelocity = Velocity;

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(AppliedAcceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = AppliedAcceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply acceleration
	if (bPreciseMove && bCanUsePreciseSwimmingStrafe)
	{
		// Apply input acceleration
		if (!bZeroAcceleration)
		{
			const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxSpeed) ? Velocity.Size() : MaxSpeed;
			FVector AddedVelocity = (Acceleration * DeltaTime) + (RequestedAcceleration * DeltaTime);
			if (bAtSurface)
			{
				AddedVelocity.Z = 0;
			}
			Velocity += AddedVelocity;
			Velocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);
		}
	}
	else
	{
		const float NewMaxSpeed = bVelocityOverMax ? Velocity.Size() : MaxSpeed;
		Velocity += AppliedAcceleration * DeltaTime;
		Velocity += RequestedAcceleration * DeltaTime;

		Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);	
	}

	if (bAbilityVerticalControl && !bAtSurface)
	{
		FRotator OnlyPitchAccel = Acceleration.Rotation();
		OnlyPitchAccel.Roll = 0.0f;
		OnlyPitchAccel.Yaw = 0.0f;

		const float curSpeed = Velocity.Size();

		const FRotator To = FRotator(DinoOwner->GetControlRotation() + OnlyPitchAccel);
		const FRotator From = Velocity.Rotation();

		FRotator result = RotateToFrom(To, From, AngleTolerance, DeltaTime).Clamp();

		const float MaxAngle = 5.0f;

		//Clamp Downwards
		if (result.Pitch <= 360.0f && result.Pitch > 180.0f)
		{
			result.Pitch = FMath::Clamp(result.Pitch, 270.0f + MaxAngle, 361.0f);
		}		
		//Clamp upwards
		else
		{
			result.Pitch = FMath::Clamp(result.Pitch, -1.0f, 90.0f - MaxAngle);
		}

		CalculatedRotator.Pitch = result.Pitch;
		Velocity = result.Vector() * Velocity.Size();
	}

	if (Velocity.Z > 0 && Velocity.Z > OldVelocity.Z)
	{
		// IF we are accelerating upwards, multiply with immersion depth to ensure we don't jump out of the water repeatedly
		const float UpwardsAcceleration = (Velocity.Z - OldVelocity.Z) * (ImmersionDepthValue * ImmersionDepthValue);
		Velocity.Z = OldVelocity.Z + UpwardsAcceleration;

		// Also apply a downwards force
		const float DownwardsForce = GetGravityZ() * DeltaTime * (1.0f - ImmersionDepthValue);
		Velocity.Z += DownwardsForce;

	}

	if (!IGameState || !IGameState->IsNewCharacterMovement())
	{
		// Rotates Pawn to movement direction....
		UpdatedComponent->MoveComponent(FVector::ZeroVector, NewDirection, false);
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}

void UICharacterMovementComponent::CalcVelocityFlying(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
 	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);

	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		Friction = FMath::Max(0.f, Friction);
		const float MaxAccel = GetMaxAcceleration();
		float MaxSpeed = GetMaxSpeed();

		if (DinoOwner->IsCarryingObject())
			MaxSpeed *= FlyCarrySpeedMultiplier;

		if (IsLimping()) {
			Friction *= 2.5f;
		}

		// Check if path following requested movement
		bool bZeroRequestedAcceleration = true;
		FVector RequestedAcceleration = FVector::ZeroVector;
		float RequestedSpeed = 0.0f;
		if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
		{
			RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel);
			bZeroRequestedAcceleration = false;
		}

		bool bAbilityMaxAccel = false;
		if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(DinoOwner->AbilitySystem))
		{
			bAbilityMaxAccel = AbilitySystem->HasAbilityForcedMovementSpeed();
		}

		if (bForceMaxAccel || bAbilityMaxAccel)
		{
			// Force acceleration at full speed.
			// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
			if (Acceleration.SizeSquared() > SMALL_NUMBER)
			{
				Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
			}
			else
			{
				Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
			}

			AnalogInputModifier = 1.f;
		}

		// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
		// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
		const float MaxInputSpeed = FMath::Max(MaxSpeed * AnalogInputModifier, GetMinAnalogSpeed());
		MaxSpeed = FMath::Max(RequestedSpeed, MaxInputSpeed);

		// Apply braking or deceleration
		const bool bZeroAcceleration = Acceleration.IsZero();
		const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

		FRotator ToRot = FRotator();
		FRotator FromRot = UpdatedComponent->GetComponentRotation();

		if (IGameState && IGameState->IsNewCharacterMovement())
		{
			if (ShouldUsePreciseMovement())
			{
				ToRot = FRotator(0.0f, GetCharacterOwner()->GetControlRotation().Yaw, 0.0f);
				CalculatedRotator = ToRot;
			}
			else
			{
				ToRot = (Acceleration.X == 0.0f && Acceleration.Y == 0.0f) ? UpdatedComponent->GetComponentRotation() : Acceleration.Rotation();
			}			
		}
		else
		{
			ToRot = (Acceleration.X == 0.0f && Acceleration.Y == 0.0f) ? UpdatedComponent->GetComponentRotation() : Acceleration.Rotation();
		}

		const float AngleTolerance = 5e-2f;
		const FRotator NewDirection = RotateToFrom(ToRot, FromRot, AngleTolerance, DeltaTime, UseSmoothFlyingMovement);

		FVector AppliedAcceleration = NewDirection.Vector() * Acceleration.Size();

		ApplyLaunchVelocityBraking(DeltaTime, Friction);

		// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
		if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
		{
			const FVector OldVelocity = Velocity;

			const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
			ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

			// Don't allow braking to lower us below max speed if we started above it.
			if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(AppliedAcceleration, OldVelocity) > 0.0f)
			{
				Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
			}
		}
		else if (!bZeroAcceleration)
		{
			// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
			const FVector AccelDir = AppliedAcceleration.GetSafeNormal();
			const float VelSize = Velocity.Size();
			Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
		}

		// Apply fluid friction
		if (bFluid)
		{
			Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
		}

		bool bOutOfStamina = DinoOwner->GetStamina() < FLT_EPSILON;

		// Apply input acceleration
		if (!bZeroAcceleration)
		{
			const float NewMaxInputSpeed = IsExceedingMaxSpeed(MaxInputSpeed) ? Velocity.Size() : MaxInputSpeed;

			//Stop the player from flying up if out of stamina.
			if (bOutOfStamina && Acceleration.Z > 0.0f)
				Acceleration.Z = 0.0f;

			Velocity.X += Acceleration.X * DeltaTime;
			Velocity.Y += Acceleration.Y * DeltaTime;

			//If the dino is flying upwards faster than they are allowed to, then disable the ability to gain height by holding space.
			if ((Acceleration.Z > 0.0f && Velocity.Z < MaxSpeed * FlyUpwardsSpeedMultiplier) || Acceleration.Z < 0.0f)
			{
				Velocity.Z += Acceleration.Z * DeltaTime;
			}

			FVector ClampedVelocity = Velocity.GetClampedToMaxSize(NewMaxInputSpeed);

			Velocity.X = ClampedVelocity.X;
			Velocity.Y = ClampedVelocity.Y;

			if (!bIsInHotAir || Velocity.Z < 0.0f)
			{
				Velocity.Z = ClampedVelocity.Z;
			}
		}

		// Apply additional requested acceleration
		if (!bZeroRequestedAcceleration)
		{
			const float NewMaxRequestedSpeed = IsExceedingMaxSpeed(RequestedSpeed) ? Velocity.Size() : RequestedSpeed;
			Velocity += RequestedAcceleration * DeltaTime;
			Velocity = Velocity.GetClampedToMaxSize(NewMaxRequestedSpeed);
		}

		if (!IGameState || !IGameState->IsNewCharacterMovement())
		{
			// Rotates Pawn to movement direction....
			UpdatedComponent->MoveComponent(FVector::ZeroVector, NewDirection, false);
		}

		if (bUseRVOAvoidance)
		{
			CalcAvoidanceVelocity(DeltaTime);
		}

		//Diving while holding W and looking down
		FRotator cameraRotation = DinoOwner->GetControlRotation();
		FVector cameraDirection = cameraRotation.Vector();

		//Getting the dot product of where you're facing, and the current acceleration will result in the input of the dinosaur.
		FVector normalizedAccel = Acceleration.GetSafeNormal();
		FVector nonVerticalCameraDirection = FVector(cameraDirection.X, cameraDirection.Y, 0.0f).GetSafeNormal();
		float dot = nonVerticalCameraDirection.Dot(normalizedAccel);

		const bool bFlyingForward = dot > 0.25f;
		const bool bFlyingUp = normalizedAccel.Z > 0.1f;
		const bool bFlyingDown = normalizedAccel.Z < -0.1f;

		bool bUsePreciseMovement = ShouldUsePreciseMovement() && !bOutOfStamina;

		float speed = Velocity.Size();

		//Diving
		if (cameraDirection.Z < 0.0f && bFlyingForward && !bFlyingUp && !bFlyingDown && !bUsePreciseMovement)
		{
			float t = cameraDirection.Z * -1.0f;
			speed *= 1.0f + t;

			FVector targetVelo = cameraDirection * speed;

			FVector distance = targetVelo - Velocity;

			//Clamp to MaxSpeed
			if (Velocity.Size() < MaxSpeed)
				Velocity += distance * DeltaTime;

			CalculatedRotator.Pitch = Velocity.Rotation().Pitch;
			DinoOwner->SetIsNosediving(cameraDirection.Z < -UKismetMathLibrary::DegSin(AngleBeginDiving));

			CalculatedRotator.Roll = 0.0f;
		}

		//Flying upwards
		else if (cameraDirection.Z > 0.0f && bFlyingForward && !bFlyingUp && !bFlyingDown && !bOutOfStamina && !bUsePreciseMovement)
		{
			FVector targetVelo = cameraDirection * speed;

			FVector distance = targetVelo - Velocity;

			Velocity += distance * DeltaTime;

			//Clamp upwards speed to half of max speed
			float upwardsSpeed = MaxSpeed * FlyUpwardsSpeedMultiplier;
			if (Velocity.Z > upwardsSpeed)
			{
				Velocity.Z += GetGravityZ() * DeltaTime;
			}

			//CalculatedRotator.Pitch = FMath::Clamp(Velocity.Rotation().Pitch + (AngleFlyUpwards * cameraDirection.Z), 0.f, 89.f);
			CalculatedRotator.Pitch = 0.0f;

		}
		else
		{
			CalculatedRotator.Pitch = 0.0f;
			DinoOwner->SetIsNosediving(false);
		}

		bool bShouldGlide = ((bOutOfStamina || bFlyingDown || (!bFlyingForward && !bFlyingUp)) && Acceleration.IsNearlyZero());
		bool bCanGlide = !DinoOwner->IsNosediving() && !bUsePreciseMovement;

		DinoOwner->SetIsGliding(bCanGlide && bShouldGlide);
		if (DinoOwner->IsGliding() || bUsePreciseMovement)
		{
			//Level out dino when gliding.
			Velocity.Z = Velocity.Z + ((-VerticalGlidingFriction) * Velocity.Z) * DeltaTime;

			if (!bUsePreciseMovement)
			{	
				//Makes dino face in the forward direction they are moving.
				FRotator glideRotator = Velocity.Rotation();
				glideRotator.Pitch = 0.0f;

				CalculatedRotator = glideRotator;
			}
		}

		float currentSpeed = Velocity.Size();

		FRotator velocityDirection = Velocity.Rotation();

		//Rotates the velocity to be in the direction of the camera overtime.
		//Is scaled based on your speed, so the faster you are the more control you have
		if (bFlyingForward && currentSpeed > 0.0f && MomentumFollowThroughSpeed > 0.0f && MaxNosedivingSpeed > 0.0f)
		{	
			FRotator targetRotation = cameraRotation;

			if (!FMath::IsNearlyZero(normalizedAccel.X))
			{
				FRotator accelRot = normalizedAccel.Rotation();
				targetRotation.Yaw = accelRot.Yaw;
			}

			if (!FMath::IsNearlyZero(normalizedAccel.Z) && !bOutOfStamina)
			{
				FRotator accelRot = normalizedAccel.Rotation();
				targetRotation.Pitch = FMath::Clamp(accelRot.Pitch, -SpeedMaxPitch, SpeedMaxPitch);
			}	
			
			float rotationSpeedRatio = currentSpeed / MaxNosedivingSpeed;

			FRotator smoothRotation = FMath::RInterpConstantTo(velocityDirection, targetRotation, DeltaTime, rotationSpeedRatio * MomentumFollowThroughSpeed);
			Velocity = smoothRotation.Vector() * currentSpeed;
		}

		//Gravity on slow down while flying and out of stamina
		float horizontalSpeed = FVector(Velocity.X, Velocity.Y, 0.0f).Size();
		if (((horizontalSpeed < StartFallingSpeedThreshold * DinoOwner->GetFlySpeedGrowthMultiplier() /*&& normalizedAccel.IsNearlyZero(0.01f)*/ && !bIsInHotAir && !DinoOwner->IsNosediving()) || bOutOfStamina) && !bUsePreciseMovement)
		{
			FVector gravity = FVector(0.0f, 0.0f, GetGravityZ());

			if (Velocity.Z > -MaxFlyDescentSpeed)
				Velocity += DescentGravityMultiplier * DeltaTime * gravity;

			bIsDescending = true;
		}
		else
		{
			bIsDescending = false;
		}

		//Roll Calculations, should come after any changes to Velocity
		if (!ShouldUsePreciseMovement())
		{
			CalculatedRotator.Yaw = velocityDirection.Yaw;

			//Only use Yaw for the rotators, as pitch and roll can cause calculation issues
			FRotator UpdatedRotator = UpdatedComponent->GetComponentTransform().Rotator();
			UpdatedRotator.Pitch = 0.0f;
			UpdatedRotator.Roll = 0.0f;

			FRotator VeloDirection = velocityDirection;
			VeloDirection.Pitch = 0.0f;
			VeloDirection.Roll = 0.0f;

			FQuat deltaAngle = UpdatedRotator.GetInverse().Quaternion() * VeloDirection.Quaternion();
			FRotator deltaRotator = deltaAngle.Rotator();

			float targetRoll = deltaRotator.Yaw;

			float speedMultiplier = currentSpeed > MaxFlySpeed ? (((currentSpeed - MaxFlySpeed) / (MaxNosedivingSpeed - MaxFlySpeed)) * SpeedAngleMultiplier) + 1.0f : 1.0f;
			CalculatedRotator.Roll = FMath::Clamp(targetRoll * AngleMultiplier * speedMultiplier, -FlyingRollMaxAngle, FlyingRollMaxAngle);
		}
	
		if (!bIsInHotAir && bOutOfStamina && !(cameraDirection.Z < -0.2f && bFlyingForward))
		{
			float prevSpeed = Velocity.Size();
			Velocity.Z = FMath::Clamp(Velocity.Z, -FLT_MAX, -MaxSpeed * FlyUpwardsSpeedMultiplier);
			Velocity = Velocity.GetClampedToMaxSize(prevSpeed);
		}
		else if (bIsInHotAir)
		{
			Velocity.Z += FlyingHotAirColumnForce * DeltaTime;
		}
	}
}

void UICharacterMovementComponent::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{

	if (ICharacterMovementCVars::CVarDisableCMCAdditions->GetBool())
	{
		Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
		return;
	}

	if (Velocity.IsZero() || !HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	float FrictionFactor = 0.0f;

	if (bUseSeparateSwimmingBrakingFrictionFactor && IsSwimming())
	{
		FrictionFactor = FMath::Max(0.f, BrakingFrictionSwimmingFactor);
	}
	else if (ShouldUsePreciseMovement() && IsMovingOnGround())
	{
		FrictionFactor = FMath::Max(0.f, BrakingFrictionPreciseFactor);
	}
	else
	{
		FrictionFactor = FMath::Max(0.f, BrakingFrictionFactor);
	}

	Friction = FMath::Max(0.f, Friction * FrictionFactor);
	BrakingDeceleration = FMath::Max(0.f, BrakingDeceleration);
	const bool bZeroFriction = (Friction == 0.f);
	const bool bZeroBraking = (BrakingDeceleration == 0.f);

	if (bZeroFriction && bZeroBraking)
	{
		return;
	}

	const FVector OldVel = Velocity;

	// subdivide braking to get reasonably consistent results at lower frame rates
	// (important for packet loss situations w/ networking)
	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	// Decelerate to brake to a stop
	const FVector RevAccel = (bZeroBraking ? FVector::ZeroVector : (-BrakingDeceleration * Velocity.GetSafeNormal()));
	while (RemainingTime >= MIN_TICK_TIME)
	{
		// Zero friction uses constant deceleration, so no need for iteration.
		const float dt = ((RemainingTime > MaxTimeStep && !bZeroFriction) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		Velocity = Velocity + ((-Friction) * Velocity + RevAccel) * dt;

		// Don't reverse direction
		if ((Velocity | OldVel) <= 0.f)
		{
			Velocity = FVector::ZeroVector;
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = Velocity.SizeSquared();
	if (VSizeSq <= KINDA_SMALL_NUMBER || (!bZeroBraking && VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY) && !IsSwimming()))
	{
		Velocity = FVector::ZeroVector;
	}
}

void UICharacterMovementComponent::ApplyLaunchVelocityBraking(float DeltaTime, float Friction)
{

	//if (IsFalling()) return;
	AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner());
	if (!DinoOwner) return;

	FVector LaunchVelocity = DinoOwner->GetLaunchVelocity();

	if (LaunchVelocity.IsZero() || !HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	float KnockbackFrictionFactor = BrakingFrictionKnockbackFactor;
	if (DinoOwner->AbilitySystem)
	{
		KnockbackFrictionFactor *= DinoOwner->AbilitySystem->GetNumericAttribute(UCoreAttributeSet::GetKnockbackTractionMultiplierAttribute());
	}

	float Amount = (DeltaTime * BrakingDecelerationKnockback) + (DeltaTime * KnockbackFrictionFactor * LaunchVelocity.Size());
	if (LaunchVelocity.Size() < Amount)
	{
		DinoOwner->SetLaunchVelocity(FVector::ZeroVector);
	}
	else
	{
		DinoOwner->SetLaunchVelocity(LaunchVelocity - (LaunchVelocity.GetSafeNormal() * Amount));
	}
	return;

	float FrictionFactor = BrakingFrictionFactor;

	const float MinLaunchFriction = 0.1f;
	Friction = FMath::Max(MinLaunchFriction, Friction * FrictionFactor);

	const FVector OldVel = LaunchVelocity;

	float RemainingTime = DeltaTime;
	const float MaxTimeStep = FMath::Clamp(BrakingSubStepTime, 1.0f / 75.0f, 1.0f / 20.0f);

	while (RemainingTime >= MIN_TICK_TIME)
	{
		const float dt = ((RemainingTime > MaxTimeStep) ? FMath::Min(MaxTimeStep, RemainingTime * 0.5f) : RemainingTime);
		RemainingTime -= dt;

		// apply friction and braking
		FVector ReverseAcceleration = ((-Friction) * LaunchVelocity + (-BrakingDecelerationKnockback * LaunchVelocity.GetSafeNormal())) * DeltaTime;
		LaunchVelocity = LaunchVelocity + ReverseAcceleration;

		// Don't reverse direction
		if ((LaunchVelocity | OldVel) <= 0.f)
		{
			LaunchVelocity = FVector::ZeroVector;
			DinoOwner->SetLaunchVelocity(LaunchVelocity);
			return;
		}
	}

	// Clamp to zero if nearly zero, or if below min threshold and braking.
	const float VSizeSq = LaunchVelocity.SizeSquared();
	if (VSizeSq <= KINDA_SMALL_NUMBER || (VSizeSq <= FMath::Square(BRAKE_TO_STOP_VELOCITY) && !IsSwimming()))
	{
		LaunchVelocity = FVector::ZeroVector;
	}
	DinoOwner->SetLaunchVelocity(LaunchVelocity);
}

void UICharacterMovementComponent::UpdateRotationMethod()
{
	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		CalculatedRotator = UpdatedComponent->GetComponentRotation();
		RotationMultiplier = 1.f;
	}

	if (IsSwimming())
	{
		if (ShouldUsePreciseMovement())
		{
			bOrientRotationToMovement = false;
			bUseControllerDesiredRotation = true;
		}
		else
		{
			bOrientRotationToMovement = true;
			bUseControllerDesiredRotation = false;
		}
	}
	else if (ShouldUsePreciseMovement())
	{
		if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
		{
			bUseControllerDesiredRotation = ShouldUseControllerDesiredRotation(DinoOwner->TurnInPlaceThreshold);
			bOrientRotationToMovement = !bUseControllerDesiredRotation;
		}
	}
	else
	{
		if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
		{
			DinoOwner->TurnInPlaceTargetYaw = 0.0f;

			bUseControllerDesiredRotation = false;
			bOrientRotationToMovement = true;
		}
	}
}

void UICharacterMovementComponent::UpdateBuoyancy(const AIDinosaurCharacter& DinoOwner)
{
	// Update buoyancy based on input and movement type
	// Sink if diving
	if (DinoOwner.IsDiving())
	{
		Buoyancy = DivingBuoyancy;
	}
	// If underwater and aquatic
	else if (DinoOwner.GetCurrentMovementType() == ECustomMovementType::DIVING && DinoOwner.IsAquatic())
	{
			
		const bool bOnlyZAcceleration = (Acceleration.X == 0.0f && Acceleration.Y == 0.0f && !Acceleration.IsZero());
		if (bOnlyZAcceleration)
		{
			if (Acceleration.Z > 0)
			{
				Buoyancy = RisingBuoyancy;
			}
			else
			{
				Buoyancy = DivingBuoyancy;
			}
		}
		else
		{
			Buoyancy = UnderwaterNeutralBuoyancy;
		}
	}
	else
	{
		// If has stamina or is aquatic then stay buoyant
		if ((DinoOwner.GetStamina() > 0) || DinoOwner.IsAquatic())
		{
			Buoyancy = SurfaceBuoyancy;
		}
		// Sink if out of stamina and not aquatic
		else
		{
			// temp fix for stuck at shore
			if (!DinoOwner.SpaceBelowPawn(DinoOwner.SpaceNeededToDive) && DinoOwner.IsAtWaterSurface())
			{
				Buoyancy = SurfaceBuoyancy;
			}
			// If drowning
			else
			{
				Buoyancy = DrowningBuoyancy;
			}
		}
	}
}

void UICharacterMovementComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AIDinosaurCharacter* const DinoCharacter = Cast<AIDinosaurCharacter>(PawnOwner);

	const bool bNeedToDoCombatLogAIMovement = 
		DinoCharacter && 
		DinoCharacter->HasAuthority() &&
		DinoCharacter->IsCombatLogAI() &&
		DinoCharacter->GetStamina() > 0.0f &&
		((IsSwimming() && 
		!DinoCharacter->IsAquatic()) || IsFalling());

	if (!bNeedToDoCombatLogAIMovement)
	{
		return;
	}

	ControlledCharacterMove(FVector::ZeroVector, DeltaTime);
}

void UICharacterMovementComponent::ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds)
{
	if (bWantsToKnockback) 
	{
		bWantsToKnockback = false;
	}

	Super::ControlledCharacterMove(InputVector, DeltaSeconds);
}

void UICharacterMovementComponent::PerformMovement(float DeltaTime)
{
	Super::PerformMovement(DeltaTime);
}

void UICharacterMovementComponent::PhysFlying(float deltaTime, int32 Iterations)
{
	// Copied and modified from the base class because we need support for landing on the ground
	if (Cast<AIAdminCharacter>(GetCharacterOwner()))
	{
		return Super::PhysFlying(deltaTime, Iterations);
	}

	/*if (bIsDoingApproxMove)
	{
		PhysFlyingApproximate(deltaTime, Iterations);
		return;
	}*/

	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		if (bCheatFlying && Acceleration.IsZero())
		{
			Velocity = FVector::ZeroVector;
		}

		float friction = ShouldUsePreciseMovement() ? FlyPreciseFriction : FlyFriction;

		if (Cast<AIDinosaurCharacter>(GetCharacterOwner())->GetStamina() < FLT_EPSILON)
		{
			friction *= OutOfStamFrictionMultiplier;
		}

		CalcVelocity(deltaTime, friction, true, GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);

	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	const float Speed = Velocity.Size();

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), true, Hit);

	// Fix: prevent underwater flying
	if (GetPhysicsVolume()->bWaterVolume)
	{
		if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
		{
			if (DinoOwner->GetCurrentMovementType() == ECustomMovementType::DIVING)
			{
				SetMovementMode(MOVE_Swimming);
				StartNewPhysics(deltaTime, Iterations);
				return;
			}
		}
	}

	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		// Disable flying in home caves.
		if (AIPlayerCaveMain* IPlayerCave = Cast<AIPlayerCaveMain>(DinoOwner->GetCurrentInstance()))
		{
			SetMovementMode(MOVE_Falling);
			StartNewPhysics(deltaTime, Iterations);
			return;
		}
	}

	if (Hit.bBlockingHit)
	{
		bool HitPawn = Cast<APawn>(Hit.HitObjectHandle.FetchActor()) != nullptr;

		HandleImpact(Hit, deltaTime, Adjusted);

		if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) && !HitPawn)
		{
			ProcessLanded(Hit, deltaTime, Iterations);
			return;
		}
		else
		{
			// Most of this code is copied and modified from UCharacterMovementComponent::PhysFalling
			
			// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
			if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(deltaTime, Adjusted, Hit) && !HitPawn)
			{
				const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
				FFindFloorResult FloorResult;
				FindFloor(PawnLocation, FloorResult, false);
				if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
				{
					ProcessLanded(FloorResult.HitResult, deltaTime, Iterations);
					return;
				}
			}

			// If we've changed physics mode, abort.
			if (!HasValidData() || !IsFlying())
			{
				return;
			}

			const FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, Hit.Normal, Hit);

			if ((Delta | Adjusted) > 0.f)
			{
				// Move in deflected direction.
				SafeMoveUpdatedComponent(Delta, UpdatedComponent->GetComponentQuat(), true, Hit);

				if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
				{
					const float NewSpeed = FMath::Clamp(Speed / MaxFlySpeed, 0.f, 1.0f) * MaxFlySpeed;

					// Limit the collision response speed so the dinosaur does not fly off at ludicrous speed
					Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation).GetSafeNormal() * NewSpeed;
				}

				if (Hit.bBlockingHit)
				{
					if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) && !HitPawn)
					{
						ProcessLanded(Hit, deltaTime, Iterations);
						return;
					}
				}

				// Do not recalculate the velocity below
				return;
			}
		}
	}

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}
}

void UICharacterMovementComponent::PhysSwimming(float deltaTime, int32 Iterations)
{
	if (bIsDoingApproxMove)
	{
		PhysSwimmingApproximate(deltaTime, Iterations);
		return;
	}
	
	Super::PhysSwimming(deltaTime, Iterations);
}

void UICharacterMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (bIsDoingApproxMove)
	{
		PhysWalkingApproximate(deltaTime, Iterations);
		return;
	}

	Super::PhysWalking(deltaTime, Iterations);
}

void UICharacterMovementComponent::PhysFlyingApproximate(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		if (bCheatFlying && Acceleration.IsZero())
		{
			Velocity = FVector::ZeroVector;
		}

		float friction = ShouldUsePreciseMovement() ? FlyPreciseFriction : FlyFriction;

		if (Cast<AIDinosaurCharacter>(GetCharacterOwner())->GetStamina() < FLT_EPSILON)
		{
			friction *= OutOfStamFrictionMultiplier;
		}

		CalcVelocity(deltaTime, friction, true, GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);
	
	const FVector Adjusted = Velocity * deltaTime;

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), false, Hit);
}

void UICharacterMovementComponent::PhysSwimmingApproximate(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	RestorePreAdditiveRootMotionVelocity();

	float NetFluidFriction = 0.f;
	float Depth = ImmersionDepth();
	float NetBuoyancy = Buoyancy * Depth;
	float OriginalAccelZ = Acceleration.Z;
	bool bLimitedUpAccel = false;

	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (Velocity.Z > 0.33f * MaxSwimSpeed) && (NetBuoyancy != 0.f))
	{
		//damp positive Z out of water
		Velocity.Z = FMath::Max<FVector::FReal>(0.33f * MaxSwimSpeed, Velocity.Z * Depth * Depth);
	}
	else if (Depth < 0.65f)
	{
		bLimitedUpAccel = (Acceleration.Z > 0.f);
		Acceleration.Z = FMath::Min<FVector::FReal>(0.1f, Acceleration.Z);
	}

	bJustTeleported = false;
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		const float Friction = 0.5f * GetPhysicsVolume()->FluidFriction * Depth;
		CalcVelocity(deltaTime, Friction, true, GetMaxBrakingDeceleration());
		Velocity.Z += GetGravityZ() * deltaTime * (1.f - NetBuoyancy);
	}

	ApplyRootMotionToVelocity(deltaTime);

	FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Adjusted, UpdatedComponent->GetComponentQuat(), false, Hit);
}

void UICharacterMovementComponent::PhysWalkingApproximate(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (!CharacterOwner || (!CharacterOwner->Controller && !bRunPhysicsWithNoController && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)))
	{
		Acceleration = FVector::ZeroVector;
		Velocity = FVector::ZeroVector;
		return;
	}

	if (!UpdatedComponent->IsQueryCollisionEnabled())
	{
		SetMovementMode(MOVE_Walking);
		return;
	}

	bJustTeleported = false;

	RestorePreAdditiveRootMotionVelocity();

	// Ensure velocity is horizontal.
	MaintainHorizontalGroundVelocity();
	const FVector OldVelocity = Velocity;
	Acceleration.Z = 0.f;

	// Apply acceleration
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, GroundFriction, false, GetMaxBrakingDeceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);

	// Compute move parameters
	const FVector Delta = deltaTime * Velocity;
	const bool bZeroDelta = Delta.IsNearlyZero();

	if (bZeroDelta)
	{
		return;
	}
	
	FindFloor(UpdatedComponent->GetComponentLocation(), CurrentFloor, true, nullptr);

	FHitResult Hit(1.f);
	const FVector RampVector = ComputeGroundMovementDelta(Delta, CurrentFloor.HitResult, CurrentFloor.bLineTrace);
	SafeMoveUpdatedComponent(RampVector, UpdatedComponent->GetComponentQuat(), false, Hit);

	// Allow overlap events and such to change physics state and velocity
	if (IsMovingOnGround())
	{
		// Make velocity reflect actual move
		if (!bJustTeleported && !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() && deltaTime >= MIN_TICK_TIME)
		{
			Velocity = (UpdatedComponent->GetComponentLocation() - ApproxMoveOldLocation) / deltaTime;
			MaintainHorizontalGroundVelocity();
		}
	}

	if (IsMovingOnGround())
	{
		MaintainHorizontalGroundVelocity();
	}
}

void UICharacterMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (Cast<AIAdminCharacter>(GetCharacterOwner()))
	{
		return Super::ProcessLanded(Hit, remainingTime, Iterations);
	}
	// Copied and modified from the base class because we needed support for transitioning from flying->ground

	if (CharacterOwner && CharacterOwner->ShouldNotifyLanded(Hit))
	{
		CharacterOwner->Landed(Hit);
	}

	if (GetGroundMovementMode() == MOVE_NavWalking)
	{
		// verify navmesh projection and current floor
		// otherwise movement will be stuck in infinite loop:
		// navwalking -> (no navmesh) -> falling -> (standing on something) -> navwalking -> ....

		const FVector TestLocation = GetActorFeetLocation();
		FNavLocation NavLocation;

		const bool bHasNavigationData = FindNavFloor(TestLocation, NavLocation);
		if (!bHasNavigationData || NavLocation.NodeRef == INVALID_NAVNODEREF)
		{
			SetGroundMovementMode(MOVE_Walking);
			//UE_LOG(LogNavMeshMovement, Verbose, TEXT("ProcessLanded(): %s tried to go to NavWalking but couldn't find NavMesh! Using Walking instead."), *GetNameSafe(CharacterOwner));
		}
	}

	SetPostLandedPhysics(Hit);

	if (AIBaseCharacter* BaseDino = Cast<AIBaseCharacter>(GetOwner()))
	{
		FRotator flattenedRotation = BaseDino->GetActorRotation();
		flattenedRotation.Pitch = 0.0f;
		flattenedRotation.Roll = 0.0f;

		BaseDino->SetActorRotation(flattenedRotation);

		//Resets the flying land lag for crouching.
		//Stops the player from immediately crouching when landing.
		BaseDino->TimeCantCrouchElapsed = 0.0f;
	}

	//Loose some speed when hitting the ground
	FVector horizontalVelocity = Velocity;
	horizontalVelocity.Z = 0.0f;

	if (horizontalVelocity.Size() > GetMaxSpeed())
	{
		Velocity *= GroundHitSpeedLossMultiplier;
	}

	IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
	if (PFAgent)
	{
		PFAgent->OnLanded();
	}

	StartNewPhysics(remainingTime, Iterations);
}

void UICharacterMovementComponent::HandleImpact(const FHitResult& Impact, float TimeSlice, const FVector& MoveDelta)
{
	Super::HandleImpact(Impact, TimeSlice, MoveDelta);

	if (!ensureAlways(CharacterOwner))
	{
		return;
	}

	AIDinosaurCharacter* const DinoOwner = Cast<AIDinosaurCharacter>(CharacterOwner);
	if (DinoOwner)
	{
		if (IsFlying())
		{
			DinoOwner->SetIsGliding(false);
			DinoOwner->SetIsNosediving(false); 

			if (!DinoOwner->IsHomecaveBuffActive())
			{
				DinoOwner->DamageImpact(Impact);
			}

			FRotator FlattenedRotation = DinoOwner->GetActorRotation();
			FlattenedRotation.Pitch = 0.0f;
			FlattenedRotation.Roll = 0.0f;

			DinoOwner->SetActorRotation(FlattenedRotation);
		}
	}

	if (Impact.bBlockingHit && TimeSlice <= 0.0001f && CharacterOwner->IsA(AIAdminCharacter::StaticClass()))
	{
		// Probably stuck in something. Try to fix.
		const FVector AdjustmentDelta = Impact.ImpactNormal * MoveDelta.Length();
		ResolvePenetration(AdjustmentDelta, Impact, UpdatedComponent->GetComponentRotation());
	}
}

FRotator UICharacterMovementComponent::RotateToFrom(const FRotator ToRotation, const FRotator FromRotation, const float AngleTolerance, const float DeltaTime, const bool UseSmoothTurn /*= false*/) const
{
	FRotator Result = FromRotation;

	// Accumulate a desired new rotation.
	if (!FromRotation.Equals(ToRotation, AngleTolerance) && DeltaTime > 0.f)
	{
		if (UseSmoothTurn)
		{
			//Use RInterpTo because using FInterpTo and going from angle 359 to 0 causes it to spin 360 degrees in the opposite direction
			// PITCH
			if (!FMath::IsNearlyEqual(FromRotation.Pitch, ToRotation.Pitch, AngleTolerance))
			{
				Result.Pitch = 0.0f;

				FRotator From = FRotator(FromRotation.Pitch, 0.0f, 0.0f);
				FRotator To = FRotator(ToRotation.Pitch, 0.0f, 0.0f);

				const double AdjustedXRotation = ShouldUsePreciseMovement() ? FlyingSmoothTurningSpeed.X * PrecisionFlyingPitchMultiplier : FlyingSmoothTurningSpeed.X;
				Result += FMath::RInterpTo(From, To, DeltaTime, AdjustedXRotation);
			}

			// YAW
			if (!FMath::IsNearlyEqual(FromRotation.Yaw, ToRotation.Yaw, AngleTolerance))
			{
				Result.Yaw = 0.0f;

				FRotator From = FRotator(0.0f, FromRotation.Yaw, 0.0f);
				FRotator To = FRotator(0.0f, ToRotation.Yaw, 0.0f);

				Result += FMath::RInterpTo(From, To, DeltaTime, FlyingSmoothTurningSpeed.Z);
			}

			// ROLL
			if (!FMath::IsNearlyEqual(FromRotation.Roll, ToRotation.Roll, AngleTolerance))
			{
				Result.Roll = 0.0f;

				FRotator From = FRotator(0.0f, 0.0f, FromRotation.Roll);
				FRotator To = FRotator(0.0f, 0.0f, ToRotation.Roll);

				Result += FMath::RInterpTo(From, To, DeltaTime, FlyingSmoothTurningSpeed.Y);
			}
		}
		else
		{
			const FRotator DeltaRot = GetDeltaRotation(DeltaTime);

			// PITCH
			if (!FMath::IsNearlyEqual(FromRotation.Pitch, ToRotation.Pitch, AngleTolerance))
			{
				Result.Pitch = FMath::FixedTurn(FromRotation.Pitch, ToRotation.Pitch, DeltaRot.Pitch);
			}

			// YAW
			if (!FMath::IsNearlyEqual(FromRotation.Yaw, ToRotation.Yaw, AngleTolerance))
			{
				Result.Yaw = FMath::FixedTurn(FromRotation.Yaw, ToRotation.Yaw, DeltaRot.Yaw);
			}

			// ROLL
			if (!FMath::IsNearlyEqual(FromRotation.Roll, ToRotation.Roll, AngleTolerance))
			{
				Result.Roll = FMath::FixedTurn(FromRotation.Roll, ToRotation.Roll, DeltaRot.Roll);
			}
		}
	}

	return Result;
}

FNetworkPredictionData_Client* UICharacterMovementComponent::GetPredictionData_Client() const
{
	// Should only be called on client or listen server (for remote clients) in network games
	check(CharacterOwner != NULL);
	checkSlow(CharacterOwner->GetRole() < ROLE_Authority || (CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy && GetNetMode() == NM_ListenServer));
	checkSlow(GetNetMode() == NM_Client || GetNetMode() == NM_ListenServer);

	if (!ClientPredictionData)
	{
		UICharacterMovementComponent* MutableThis = const_cast<UICharacterMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_ICharacter(*this);
	}

	return ClientPredictionData;
}

FNetworkPredictionData_Server* UICharacterMovementComponent::GetPredictionData_Server() const
{
	if (ServerPredictionData == nullptr)
	{
		UICharacterMovementComponent* MutableThis = const_cast<UICharacterMovementComponent*>(this);
		MutableThis->ServerPredictionData = new FNetworkPredictionData_Server_ICharacter(*this);
	}

	return ServerPredictionData;
}


#pragma region MultiCapsule

//Getting the required CVars
float GetInitialOverlapTolerance()
{
	static IConsoleVariable* CVarPtr = IConsoleManager::Get().FindConsoleVariable(TEXT("p.InitialOverlapTolerance"));
	if (ensure(CVarPtr))
	{
		return CVarPtr->GetFloat();
	}

	return 0.1f;
}

float GetPenetrationOverlapCheckInflation()
{
	static IConsoleVariable* CVarPtr = IConsoleManager::Get().FindConsoleVariable(TEXT("p.PenetrationOverlapCheckInflation"));
	if (ensure(CVarPtr))
	{
		return CVarPtr->GetFloat();
	}

	return 0.1f;
}

//Manually register additional capsules
void UICharacterMovementComponent::RegisterCapsulesAsAdditionalComponent(TArray<UCapsuleComponent*> InCapsules)
{
	if (!bUseMultiCapsuleCollision)
	{
		return;
	}

	if (CharacterOwner == nullptr || InCapsules.Num() == 0)
	{
		return;
	}

	for (UCapsuleComponent* AC : InCapsules)
	{
		if (AC != nullptr)
		{
			AC->MoveIgnoreActors.Add(CharacterOwner);
			AC->MoveIgnoreActors += UpdatedPrimitive->MoveIgnoreActors;
			AC->SetShouldUpdatePhysicsVolume(true);
			AC->SetNotifyRigidBodyCollision(false);
			AC->SetEnableGravity(false);
			AC->SetCanEverAffectNavigation(false);

			if (bEnablePhysicsInteraction)
			{
				AC->OnComponentBeginOverlap.AddUniqueDynamic(this, &UICharacterMovementComponent::CapsuleTouched);
				AC->SetGenerateOverlapEvents(true);
				AC->SetComponentTickEnabled(true);
			}
			else
			{
				AC->SetGenerateOverlapEvents(false);
				AC->SetComponentTickEnabled(false);
			}

			AdditionalCapsules.Emplace(AC);

			if (UStateAdjustableCapsuleComponent* SACC = Cast<UStateAdjustableCapsuleComponent>(AC))
			{
				AdditionalAdjustableCapsules.Emplace(SACC);
			}
		}
	}

}

//Automatically fetch non-root capsules. Called in BeginPlay
void UICharacterMovementComponent::GatherAdditionalUpdatedComponents()
{
	if (CharacterOwner == nullptr || !bUseMultiCapsuleCollision)
	{
		return;
	}

	for (UCapsuleComponent* AC : AdditionalCapsules)
	{
		if (AC != nullptr)
		{
			AC->MoveIgnoreActors.Empty();

			if (bEnablePhysicsInteraction)
			{
				AC->OnComponentBeginOverlap.RemoveAll(this);
			}
		}
	}

	const int32 ReserveNum = AdditionalCapsules.Num();
	AdditionalCapsules.Empty(ReserveNum);
	AdditionalCapsuleDefaultCollisionProfiles.Empty(ReserveNum);

	TArray<UCapsuleComponent*> CollisionComponents;
	CharacterOwner->GetComponents<UCapsuleComponent>(CollisionComponents);

	for (UCapsuleComponent* AC : CollisionComponents)
	{
		if (AC != nullptr && AC != CharacterOwner->GetRootComponent() && !AC->ComponentTags.Contains("IgnoreForMovement"))
		{
			AC->MoveIgnoreActors.Add(CharacterOwner);
			AC->MoveIgnoreActors += UpdatedPrimitive->MoveIgnoreActors;
			AC->SetShouldUpdatePhysicsVolume(true);
			AC->SetNotifyRigidBodyCollision(false);
			AC->SetEnableGravity(false);

			if (bEnablePhysicsInteraction)
			{
				AC->OnComponentBeginOverlap.AddUniqueDynamic(this, &UICharacterMovementComponent::CapsuleTouched);
			}

			AdditionalCapsules.Emplace(AC);
			AdditionalCapsuleDefaultCollisionProfiles.Add(AC, FStoredCollisionInfo(AC->GetCollisionObjectType(), AC->GetCollisionEnabled(), AC->GetCollisionResponseToChannels()));

			if (UStateAdjustableCapsuleComponent* SACC = Cast<UStateAdjustableCapsuleComponent>(AC))
			{
				AdditionalAdjustableCapsules.Emplace(SACC);
			}
		}
	}

	UCapsuleComponent* MainCapsule = Cast<UCapsuleComponent>(UpdatedComponent);
	if (ensure(MainCapsule))
	{
		AdditionalCapsules.Add(MainCapsule);
		AdditionalCapsuleDefaultCollisionProfiles.Add(MainCapsule, FStoredCollisionInfo(MainCapsule->GetCollisionObjectType(), MainCapsule->GetCollisionEnabled(), MainCapsule->GetCollisionResponseToChannels()));
	}

}

//Basically a copy of the default function from the CMC with the addition of simulating the other capsules and adjusting for those
bool UICharacterMovementComponent::MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& Rotation, bool bSweep, FHitResult* OutHit, ETeleportType Teleport)
{

	SCOPE_CYCLE_COUNTER(STAT_MultiCapsuleMoveUpdatedComponent);

	if (!UpdatedComponent)
	{
		return false;
	}

	if (!bUseMultiCapsuleCollision)
	{
		return Super::MoveUpdatedComponentImpl(Delta, Rotation, bSweep, OutHit, Teleport);
	}

	FQuat NewRotation = Rotation;
	FVector NewDelta = ConstrainDirectionToPlane(Delta);


	const float DeltaMoveSizeSq = NewDelta.SizeSquared();
	const float MinDistSq = FMath::Square(4.f * KINDA_SMALL_NUMBER);
	if (DeltaMoveSizeSq <= MinDistSq)
	{
		if (NewRotation.Equals(UpdatedComponent->GetComponentQuat(), SCENECOMPONENT_QUAT_TOLERANCE))
		{
			if (OutHit)
			{
				OutHit->Reset(1.f);
			}
			return true;
		}
		else
		{
			NewDelta = FVector::ZeroVector;
		}
	}

	UCapsuleComponent* AC = CharacterOwner->GetCapsuleComponent();
	if (CharacterOwner->HasAuthority() || CharacterOwner->IsLocallyControlled())
	{	
		//DrawDebugCapsule(GetWorld(), AC->GetComponentLocation(), AC->GetScaledCapsuleHalfHeight(), AC->GetScaledCapsuleRadius(), AC->GetComponentRotation().Quaternion(), CharacterOwner->HasAuthority() ? FColor::Yellow : FColor::Red, false, 0.1f);
	}

	//Run code below when not doing async traces
	const bool bSuccess = bSweep ? MoveAdditionalCapsules(NewDelta, NewRotation, OutHit) : true;

	bool bResolvePenetration = false;
	FVector PenetrationAdjustment = FVector::ZeroVector;	

	if (!bSuccess && OutHit)
	{
		AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);
		if (IGameState && IGameState->IsNewCharacterMovement())
		{
			if (NewDelta == FVector::ZeroVector)
			{	
				// If we have no delta movement, we won't be able to adjust properly using Time, so we need to push out of any overlap using the Penetration Depth.
				// We need to use ResolvePenetration to ensure we don't get put into another object.
				bResolvePenetration = true;
				PenetrationAdjustment = (OutHit->ImpactNormal * OutHit->PenetrationDepth) * FVector(1.0f, 1.0f, 0.0f);
			}
			else
			{
				bool bHittingWall = false;

				if (FVector::Distance(OutHit->ImpactPoint, OutHit->TraceStart) < 1.0f)
				{
					bHittingWall = true;
				}
				else
				{
					if (OutHit->Time < 0.05f)
					{
						bHittingWall = true;
					}
				}

				FVector DeltaNormal = NewDelta * FVector(1, 1, 0);
				DeltaNormal.Normalize();
				FVector FlatImpactNormal = OutHit->ImpactNormal * FVector(1, 1, 0);
				FlatImpactNormal.Normalize();

				float ImpactDotProduct = FVector::DotProduct(FlatImpactNormal, DeltaNormal);

				if ((bHittingWall && (ImpactDotProduct <= -0.35f)) || 
					(ImpactDotProduct < 0.f && Cast<AIBaseCharacter>(OutHit->GetActor()) && NewDelta.Size() > 1.f)
					)
				{
					OnHitWall(*OutHit);
				}
				NewDelta *= OutHit->Time;
			}

			if (IsFlying())	// If we are flying then don't let unreal do the penetration adjustment. it can create insane velocities.
			{
				OutHit->bStartPenetrating = false;
				NewDelta += OutHit->PenetrationDepth * OutHit->Normal;
			}
		}
		else
		{
			NewRotation = FQuat::Slerp(UpdatedComponent->GetComponentQuat(), Rotation, OutHit->Time);
			NewDelta *= OutHit->Time;
		}
	}
	bool bMoved = false;
	if (bResolvePenetration && OutHit)	// Penetration adjustment used when NewDelta is zero and we are rotating.
	{
		bMoved = true;

		bool bEncroached = false;
		for (UCapsuleComponent* AdditionalCapsule : AdditionalCapsules)
		{
			if (OverlapTest(
				AdditionalCapsule->GetComponentLocation() + PenetrationAdjustment,
				AdditionalCapsule->GetComponentQuat(),
				AdditionalCapsule->GetCollisionObjectType(),
				AdditionalCapsule->GetCollisionShape(GetPenetrationOverlapCheckInflation()),
				AdditionalCapsule->GetOwner()))
			{
				bEncroached = true;
				break;
			}
		}
		if (!bEncroached)
		{
			// Our adjustment will not put us into anything, it is safe to move using the adjustment.
			UpdatedComponent->MoveComponent(PenetrationAdjustment, NewRotation, false, nullptr, MoveComponentFlags, ETeleportType::TeleportPhysics);
		}
		else
		{
			// Unable to adjust, we can use a tiny value from the impact to help us unstuck if it happens
			FVector TinyAdjustment = OutHit->ImpactNormal * 0.01f;
			UpdatedComponent->MoveComponent(TinyAdjustment, NewRotation, false, nullptr, MoveComponentFlags, ETeleportType::TeleportPhysics);
		}
	}

	if (!bMoved)
	{
		UpdatedComponent->MoveComponent(NewDelta, NewRotation, false, nullptr, MoveComponentFlags, Teleport);
	}

	if (!bSkipUpdateChildTransforms)
	{
		UpdatedComponent->UpdateChildTransforms(EUpdateTransformFlags::PropagateFromParent, Teleport);
	}
	

	return bSuccess;

}

void UICharacterMovementComponent::OnHitWall(const FHitResult& HitResult)
{
	if (AIDinosaurCharacter* DinoOwner = Cast<AIDinosaurCharacter>(GetCharacterOwner()))
	{
		if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(DinoOwner->AbilitySystem))
		{
			AbilitySystem->OnCharacterHitWall(HitResult);
		}
	}
}

void UICharacterMovementComponent::PhysicsVolumeChanged(APhysicsVolume* NewVolume)
{
	// Same as Super::PhysicsVolumeChanged but with a better transition from swimming to land
	ACharacter* Owner = GetCharacterOwner();
	AIDinosaurCharacter* IDinoChar = Cast<AIDinosaurCharacter>(Owner);

	if (IDinoChar)
	{
		IDinoChar->OnPhysicsVolumeChanged(NewVolume);
	}

	if (!HasValidData())
	{
		return;
	}

	if (NewVolume && NewVolume->bWaterVolume)
	{
		// just entered water
		if (!CanEverSwim())
		{
			// AI needs to stop any current moves
			IPathFollowingAgentInterface* PFAgent = GetPathFollowingAgent();
			if (PFAgent)
			{
				//PathFollowingComp->AbortMove(*this, FPathFollowingResultFlags::MovementStop);
				PFAgent->OnUnableToMove(*this);
			}
		}
		else if (!IsSwimming() && IDinoChar && !IDinoChar->IsAttached())
		{
			// Call CharacterOwner Landed event on water impact
			const FHitResult WaterHit = FHitResult(Owner, Owner->GetCapsuleComponent(), Owner->GetActorLocation(), FVector::ZeroVector);
			if (Owner && Owner->ShouldNotifyLanded(WaterHit) && !IDinoChar->IsAquatic() && Owner->bClientWasFalling)
			{
				Owner->Landed(WaterHit);
			}
			SetMovementMode(MOVE_Swimming);
		}
	}
	else if (IsSwimming())
	{
		bool bOnGround = false;
		if (Owner)
		{
			check(Owner);
			check(Owner->GetCapsuleComponent());
			if (Owner && Owner->GetCapsuleComponent())
			{
				UCapsuleComponent* CapsuleComp = Owner->GetCapsuleComponent();
				bOnGround = OverlapTest(Owner->GetActorLocation() - FVector(0, 0, CapsuleComp->GetScaledCapsuleHalfHeight() * 0.1f), Owner->GetActorQuat(), CapsuleComp->GetCollisionObjectType(), CapsuleComp->GetCollisionShape(GetPenetrationOverlapCheckInflation()), Owner);
			}
		}

		if (bOnGround)
		{
			// Just left the water but on ground, switch immediately to walking.
			SetMovementMode(MOVE_Walking);

			if (Owner)
			{
				//Fixes actor's pitch getting stuck when coming out of the water.
				FRotator Rot = Owner->GetActorRotation();
				Rot.Pitch = 0.0f;
				Owner->SetActorRotation(Rot);
			}
		}
		else
		{
			// just left the water - check if should jump out
			SetMovementMode(MOVE_Falling);

			FVector JumpDir(0.f);
			FVector WallNormal(0.f);
			if (Acceleration.Z > 0.f && ShouldJumpOutOfWater(JumpDir)
				&& ((JumpDir | Acceleration) > 0.f) && CheckWaterJump(JumpDir, WallNormal))
			{
				JumpOutOfWater(WallNormal);
				Velocity.Z = OutofWaterZ; //set here so physics uses this for remainder of tick
			}
		}

		if (IDinoChar)
		{
			IDinoChar->ApplyWetness();
		}
	}
}

//Goes through the additional capsules, simulate their movement and check which ones would have hit anything. Find the most penetrating hit and return it.
bool UICharacterMovementComponent::MoveAdditionalCapsules(const FVector& Delta, const FQuat& NewRotation, FHitResult* OutHit)
{
	SCOPE_CYCLE_COUNTER(STAT_MoveAdditionalCapsules);


	TArray<FHitResult> BlockedHits;
	TArray<const UCapsuleComponent*> BlockedComponents;
	LastHitComponent = nullptr;

	//Disable mesh overlap for the duration of this.
	CharacterOwner->GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	CharacterOwner->GetMesh()->SetGenerateOverlapEvents(false);


	for (int32 i = 0; i < AdditionalCapsules.Num(); i++)
	{
		UCapsuleComponent* AC = AdditionalCapsules[i];
		if (AC != nullptr)
		{
			if (CharacterOwner->HasAuthority() || CharacterOwner->IsLocallyControlled())
			{
				//DrawDebugCapsule(GetWorld(), AC->GetComponentLocation(), AC->GetScaledCapsuleHalfHeight(), AC->GetScaledCapsuleRadius(), AC->GetComponentRotation().Quaternion(), CharacterOwner->HasAuthority() ? FColor::Yellow : FColor::Red, false, 0.1f);
			}
			if (IncrementalAsyncUpdateMode == 2 || (IncrementalAsyncUpdateMode == 1 && AC != UpdatedComponent))
			{
				if (i == IncrementalUpdateIndex)
				{
					IncrementalUpdateIndex = ((IncrementalUpdateIndex + 1) % AdditionalCapsules.Num());
					continue;
				}
			}

			if (ShouldUseAsyncMultiCapsuleCollision(AC))
			{

				if (const FHitResult* HResult = CachedHits.Find(AC))
				{
					if (HResult->bBlockingHit)
					{
						BlockedHits.Add(*HResult);
						BlockedComponents.Add(AC);
					}
				}

				FVector TraceStart;
				FVector TraceEnd;
				FQuat NewCompQuat;
				FComponentQueryParams QueryParams;
				FCollisionResponseParams ResponseParams;
				CalculateSweepData(AC, Delta, NewRotation, TraceStart, TraceEnd, NewCompQuat, QueryParams, ResponseParams);

				FMultiCapsuleTraceWorker::Get()->QueueTraces(
					FQueuedMultiCapsuleTrace(GetWorld(), Cast<AIBaseCharacter>(CharacterOwner), AC, TraceStart,
						TraceEnd, Delta, NewCompQuat, QueryParams, ResponseParams, MoveComponentFlags));
			}
			else
			{
				FHitResult BlockedHit = FHitResult(1.f);

				if (!SimulateMoveCapsuleComponent(AC, UpdatedComponent, Delta, NewRotation, &BlockedHit, MoveComponentFlags))
				{
					BlockedHits.Add(BlockedHit);
					BlockedComponents.Add(AC);

					FHitResult& ComponentResult = CachedHits.FindOrAdd(AC);
					ComponentResult = BlockedHit;
				}
			}

			
		}
	}

	CharacterOwner->GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	//CharacterOwner->GetMesh()->SetGenerateOverlapEvents(true);


	if (BlockedHits.Num() > 0)
	{
		int32 BestHitIndex = INDEX_NONE;
		float BestHitTime = 1.f;

		for (int32 i = 0; i < BlockedHits.Num(); ++i)
		{
			if (BlockedHits[i].Time <= BestHitTime)
			{
				BestHitTime = BlockedHits[i].Time;
				BestHitIndex = i;

			}
		}

		if (BestHitIndex != INDEX_NONE)
		{
			if (OutHit)
			{
				*OutHit = BlockedHits[BestHitIndex];
			}
			LastHitComponent = BlockedComponents[BestHitIndex];
		}
		return false;
	}
	else
	{
		if (OutHit)
		{
			OutHit->Reset(1.f);
		}
		return true;
	}


}

void UICharacterMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoGatherNonRootCapsules && bUseMultiCapsuleCollision)
	{
		GatherAdditionalUpdatedComponents();
	}
}

//Basically a copy of the CMC function but with bSweep set to true in the MoveUpdatedComponent calls (since the CMC doesn't sweep when adjusting this kind of stuff because lol capsule
bool UICharacterMovementComponent::ResolvePenetrationImpl(const FVector& ProposedAdjustment, const FHitResult& Hit, const FQuat& Rotation)
{

	SCOPE_CYCLE_COUNTER(STAT_MultiCapsuleResolvePenetration);


	if (!bUseMultiCapsuleCollision)
	{
		return Super::ResolvePenetrationImpl(ProposedAdjustment, Hit, Rotation);
	}

	if (!LastHitComponent)
	{
		return false;
	}

	bool bMoved = false;

	// SceneComponent can't be in penetration, so this function really only applies to PrimitiveComponent.
	const FVector Adjustment = ConstrainDirectionToPlane(ProposedAdjustment);

	// Set rotation to use
	const FQuat MoveRotation = Rotation; // use new rotation when resolving penetration

	if (!Adjustment.IsZero())
	{
		// We really want to make sure that precision differences or differences between the overlap test and sweep tests don't put us into another overlap,
		// so make the overlap test a bit more restrictive.

		bool bEncroached = false;
		bEncroached = OverlapTest(Hit.TraceStart + Adjustment, LastHitComponent->GetComponentQuat(), LastHitComponent->GetCollisionObjectType(), LastHitComponent->GetCollisionShape(GetPenetrationOverlapCheckInflation()), LastHitComponent->GetOwner());

		if (!bEncroached)
		{
			// Move without sweeping.
			FHitResult EncroachHit(1.f);
			bMoved = MoveUpdatedComponent(Adjustment, MoveRotation, false, &EncroachHit, ETeleportType::None);
		}
		else
		{
			// Disable MOVECOMP_NeverIgnoreBlockingOverlaps if it is enabled, otherwise we wouldn't be able to sweep out of the object to fix the penetration.
			TGuardValue<EMoveComponentFlags> ScopedFlagRestore(MoveComponentFlags, EMoveComponentFlags(MoveComponentFlags & (~MOVECOMP_NeverIgnoreBlockingOverlaps)));

			// Try sweeping as far as possible...
			FHitResult SweepOutHit(1.f);

			bMoved = MoveUpdatedComponent(Adjustment, MoveRotation, true, &SweepOutHit, ETeleportType::None);

			// Still stuck?
			if (!bMoved && SweepOutHit.bStartPenetrating)
			{
				// Combine two MTD results to get a new direction that gets out of multiple surfaces.
				const FVector SecondMTD = GetPenetrationAdjustment(SweepOutHit);
				const FVector CombinedMTD = Adjustment + SecondMTD;
				if (SecondMTD != Adjustment && !CombinedMTD.IsZero())
				{
					bMoved = MoveUpdatedComponent(CombinedMTD, MoveRotation, true, &SweepOutHit, ETeleportType::None);
				}
			}

			// Still stuck?
			if (!bMoved)
			{
				// Try moving the proposed adjustment plus the attempted move direction. This can sometimes get out of penetrations with multiple objects
				const FVector MoveDelta = ConstrainDirectionToPlane(Hit.TraceEnd - Hit.TraceStart);
				if (!MoveDelta.IsZero())
				{
					FHitResult FallBacktHit(1.f);
					bMoved = MoveUpdatedComponent(Adjustment + MoveDelta, MoveRotation, true, &FallBacktHit, ETeleportType::None);
				}
			}
		}
	}

	bJustTeleported |= bMoved;
	return bJustTeleported;
}

//Again, copy of CMC function but with last call have bSweep set to true (rotating a capsule has no need for a sweep so default CMC doesn't set it to true)
void UICharacterMovementComponent::PhysicsRotation(float DeltaTime)
{
	AIDinosaurCharacter* IDinoChar = Cast<AIDinosaurCharacter>(CharacterOwner.Get());
	if (IDinoChar && IDinoChar->bIsCharacterEditorPreviewCharacter)
	{
		return;
	}

	const float AngleTolerance = 1e-3f;
	FRotator CurrentRotation = FRotator();
	CurrentRotation = UpdatedComponent->GetComponentRotation(); // Normalized
	CurrentRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): CurrentRotation"));

	if (IDinoChar)
	{
		//Fixes the issue where flying dino's roll gets stuck in water
		if (MovementMode == MOVE_Swimming)
		{
			FRotator noRollRotation = IDinoChar->GetActorRotation();
			noRollRotation.Roll = 0.0f;
			IDinoChar->SetActorRotation(noRollRotation);

			CalculatedRotator.Roll = 0.0f;
		}

		if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(IDinoChar->AbilitySystem))
		{
			if (AbilitySystem->HasAbilityForcedRotation())
			{
				FRotator ForcedRotation = AbilitySystem->GetAbilityForcedRotation();
				FRotator ToRotation = bUseSmoothForcedRotation ? RotateToFrom(ForcedRotation, CurrentRotation, AngleTolerance, DeltaTime) : ForcedRotation;

				FHitResult Hit(1.f);
				MoveUpdatedComponent(FVector::ZeroVector, ToRotation, true, &Hit);
				return;
			}
		}
	}

	AIGameState* IGameState = UIGameplayStatics::GetIGameState(this);

	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		// Accumulate a desired new rotation.
		if (!(CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy))
		{
			CurrentRotation = RotateToFrom(CalculatedRotator, CurrentRotation, AngleTolerance, DeltaTime * RotationMultiplier, IsFlying() && (UseSmoothFlyingMovement || ShouldUsePreciseMovement()));
			FHitResult Hit(1.f);
			MoveUpdatedComponent(FVector::ZeroVector, CurrentRotation, true, &Hit);
		}
	}

	if (ICharacterMovementCVars::CVarDisableCMCAdditions->GetBool())
	{
		return Super::PhysicsRotation(DeltaTime);
	}

	if (!bUseMultiCapsuleCollision)
	{
		return Super::PhysicsRotation(DeltaTime);
	}

	if (!(bOrientRotationToMovement || bUseControllerDesiredRotation))
	{
		return;
	}

	if (!HasValidData() || (!CharacterOwner->Controller && !bRunPhysicsWithNoController))
	{
		return;
	}

	if (!IGameState || !IGameState->IsNewCharacterMovement())
	{
		CurrentRotation = UpdatedComponent->GetComponentRotation(); // Normalized
		CurrentRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): CurrentRotation"));
	}

	FRotator DeltaRot = GetDeltaRotation(DeltaTime);
	DeltaRot.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): GetDeltaRotation"));

	if (IGameState && IGameState->IsNewCharacterMovement())
	{
		if (ShouldUsePreciseMovement())
		{
			return;
		}
	}
	FRotator DesiredRotation = CurrentRotation;
	if (bOrientRotationToMovement)
	{
		DesiredRotation = ComputeOrientToMovementRotation(CurrentRotation, DeltaTime, DeltaRot);
	}
	else if (CharacterOwner->Controller && bUseControllerDesiredRotation)
	{
		DesiredRotation = CharacterOwner->Controller->GetDesiredRotation();
	}
	else
	{
		return;
	}

	if (ShouldRemainVertical())
	{
		DesiredRotation.Pitch = 0.f;
		DesiredRotation.Yaw = FRotator::NormalizeAxis(DesiredRotation.Yaw);
		DesiredRotation.Roll = 0.f;

		// Fix for flyers rotation when swimming carrying over when going from water to land
		if (FMath::IsNearlyEqual(0, DeltaRot.Roll))
		{
			// After testing, 0.5 feels the most natural
			DeltaRot.Roll = 0.5f;
		}
	}
	else
	{
		DesiredRotation.Normalize();
	}

	if (!CurrentRotation.Equals(DesiredRotation, AngleTolerance))
	{
		// PITCH
		if (!FMath::IsNearlyEqual(CurrentRotation.Pitch, DesiredRotation.Pitch, AngleTolerance))
		{
			DesiredRotation.Pitch = FMath::FixedTurn(CurrentRotation.Pitch, DesiredRotation.Pitch, DeltaRot.Pitch);
		}

		// YAW
		if (!FMath::IsNearlyEqual(CurrentRotation.Yaw, DesiredRotation.Yaw, AngleTolerance))
		{
			DesiredRotation.Yaw = FMath::FixedTurn(CurrentRotation.Yaw, DesiredRotation.Yaw, DeltaRot.Yaw);
		}

		// ROLL
		if (!FMath::IsNearlyEqual(CurrentRotation.Roll, DesiredRotation.Roll, AngleTolerance))
		{
			DesiredRotation.Roll = FMath::FixedTurn(CurrentRotation.Roll, DesiredRotation.Roll, DeltaRot.Roll);
		}

		// Set the new rotation.
		DesiredRotation.DiagnosticCheckNaN(TEXT("CharacterMovementComponent::PhysicsRotation(): DesiredRotation"));

		FHitResult Hit(1.f);
		MoveUpdatedComponent(FVector::ZeroVector, DesiredRotation, true, &Hit);
	}
}

void UICharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	if (!bUseMultiCapsuleCollision)
	{
		Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
		return;
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_OnMovementUpdatedMultiCapsuleUpdateOverlaps);

		for (UCapsuleComponent* MCCC : AdditionalCapsules)
		{
			if (MCCC != nullptr)
			{
				MCCC->UpdateOverlaps();
				MCCC->UpdatePhysicsVolume(true);
			}
		}
	}

	LastHitComponent = nullptr;

}

bool UICharacterMovementComponent::IsSurfacing(FHitResult HitResult, FVector TraceStart) const
{
	AIBaseCharacter* const BaseCharacter = Cast<AIBaseCharacter>(GetOwner());

	TraceStart.Z += BaseCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector TraceEnd = FVector(TraceStart.X, TraceStart.Y, (TraceStart.Z - 1000));

	FCollisionQueryParams TraceParams{};
	TraceParams.AddIgnoredActor(BaseCharacter);
	
	if (const AActor* const CarriedObject = Cast<AActor>(BaseCharacter->GetCurrentlyCarriedObject().Object.Get()))
	{
		TraceParams.AddIgnoredActor(CarriedObject);
	}

	FCollisionObjectQueryParams ObjectTraceParams{};
	ObjectTraceParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTraceParams.AddObjectTypesToQuery(ECC_GameTraceChannel8);

	FHitResult Hit(ForceInit);
	GetWorld()->LineTraceSingleByObjectType(Hit, TraceStart, TraceEnd, ObjectTraceParams, TraceParams);
	
	const float RelativeZ = Hit.Location.Z - GetActorLocation().Z;
	const float HalfBodyHeight = BaseCharacter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	// Calculates the limits of how far the dino needs to be from the water based on the height of the dinosaur. 
	// 0.045 is just the sweet spot for this and can be changed if needed.
	const float WaterLimitUp = FMath::Clamp(HalfBodyHeight * 0.045f, 1.0f, 20.0f);
	const float WaterLimitLow = FMath::Clamp(HalfBodyHeight * 0.045f * -1.0, -1.0f, -20.0f);

	return RelativeZ < WaterLimitUp && RelativeZ > WaterLimitLow;
}

void UICharacterMovementComponent::SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode)
{
	if (Cast<AIAdminCharacter>(GetOwner()))
	{
		Super::SetMovementMode(MOVE_Flying);
	}
	else
	{
		//Fixes an issue where precise swimmers would flip between states which makes the swimmer go in and out of water 
		//looping forever instead of smoothly transitioning.
		bool bSwimmingToWalking = MovementMode == MOVE_Swimming && NewMovementMode == MOVE_Walking;
		bool bSwimmingToFalling = MovementMode == MOVE_Swimming && NewMovementMode == MOVE_Falling;
		AIBaseCharacter* const IBaseCharacter = Cast<AIBaseCharacter>(GetCharacterOwner());

		if (!bSwimmingToFalling || !bSwimmingToWalking)
		{
			bPreciseMoveSwimmingToFalling = false;
		}

		if (bCanUsePreciseSwimming)
		{
			if (IBaseCharacter)
			{
				bool WalkingToSwimming = MovementMode == MOVE_Walking && NewMovementMode == MOVE_Swimming;

				if (bSwimmingToWalking || bSwimmingToFalling)
				{
					if (IBaseCharacter->DesiresPreciseMove())
					{
						if (bSwimmingToFalling)
						{
							bPreciseMoveSwimmingToFalling = true;
						}
						else if (bSwimmingToWalking && bPreciseMoveSwimmingToFalling)
						{
							IBaseCharacter->OnStopPreciseMovement();
							bPreciseMoveSwimmingToFalling = false;
						}
					}
					else if (TransitionPreciseMovementTime > FLT_EPSILON)
					{
						//Diving while holding W and looking down
						FRotator cameraRotation = IBaseCharacter->GetControlRotation();
						FVector cameraDirection = cameraRotation.Vector();

						//Getting the dot product of where you're facing, and the current acceleration will result in the input of the dinosaur.
						FVector normalizedAccel = Acceleration.GetSafeNormal();
						FVector nonVerticalCameraDirection = FVector(cameraDirection.X, cameraDirection.Y, 0.0f).GetSafeNormal();
						float dot = nonVerticalCameraDirection.Dot(normalizedAccel);

						const bool bHoldingBackwards = dot < -0.5f;

						if (bHoldingBackwards)
						{
							IBaseCharacter->OnStartPreciseMovement();
							bTransitioningPreciseMovement = true;
							elapsedTransitionPreciseMovementTime = 0.0f;
						}
					}
				}
		
				if (WalkingToSwimming && IBaseCharacter->DesiresPreciseMove())
				{
					IBaseCharacter->OnStopPreciseMovement();
				}
			}
		}

		if (AIDinosaurCharacter* Dino = Cast<AIDinosaurCharacter>(GetOwner()))
		{
			if ((bSwimmingToWalking || bSwimmingToFalling) && Dino->IsDiving())
			{
				Dino->CancelDiveInProgress();
			}
		}

		if (NewMovementMode == MOVE_Falling && bSwimmingToFalling && IBaseCharacter)
		{
			const FHitResult WaterHit{};
			const FVector Start = GetActorLocation();
			const bool bIsSurfacing = IsSurfacing(WaterHit, Start);
			if (bIsSurfacing)
			{
				NewMovementMode = MOVE_Swimming;
				IBaseCharacter->OnStopPreciseMovement();
			}
			else
			{
				NewMovementMode = MOVE_Falling;
			}
		}

		Super::SetMovementMode(NewMovementMode, NewCustomMode);
	}
}

//Copies of non-class functions from CapsuleComponent.cpp
static void PullBackHit(FHitResult& Hit, const FVector& Start, const FVector& End, const float Dist)
{
	const float DesiredTimeBack = FMath::Clamp(0.1f, 0.1f / Dist, 1.f / Dist) + 0.001f;
	Hit.Time = FMath::Clamp(Hit.Time - DesiredTimeBack, 0.f, 1.f);
}

bool UICharacterMovementComponent::ShouldIgnoreHitResult(const UWorld* InWorld, const UPrimitiveComponent* TestComponent, FHitResult const& TestHit, FVector const& MovementDirDenormalized, const AActor* MovingActor, EMoveComponentFlags MoveFlags)
{
	if (TestHit.bBlockingHit)
	{
		// check "ignore bases" functionality
		if ((MoveFlags & MOVECOMP_IgnoreBases) && MovingActor)	//we let overlap components go through because their overlap is still needed and will cause beginOverlap/endOverlap events
		{
			// ignore if there's a base relationship between moving actor and hit actor
			AActor const* const HitActor = TestHit.GetActor();
			if (HitActor)
			{
				if (MovingActor->IsBasedOnActor(HitActor) || HitActor->IsBasedOnActor(MovingActor))
				{
					return true;
				}
			}
		}

		if (TestComponent != UpdatedComponent)
		{
			const float GroundDot = FMath::Abs((TestHit.ImpactNormal | FVector::UpVector));

			if (GroundDot > AdditionalCapsuleDotTolerance && GroundDot < AdditionalCapsuleMaxDotTolerance)
			{
				return true;
			}
			
		}

		// If we started penetrating, we may want to ignore it if we are moving out of penetration.
		// This helps prevent getting stuck in walls.
		if (TestHit.bStartPenetrating && !(MoveFlags & MOVECOMP_NeverIgnoreBlockingOverlaps))
		{
			const float DotTolerance = GetInitialOverlapTolerance();

			// Dot product of movement direction against 'exit' direction
			const FVector MovementDir = MovementDirDenormalized.GetSafeNormal();
			const float MoveDot = (TestHit.ImpactNormal | MovementDir);

			const bool bMovingOut = MoveDot > DotTolerance;

			// If we are moving out, ignore this result!
			if (bMovingOut)
			{
				return true;
			}
		}
	}

	return false;
}

//A copy of UPrimitiveComponent::MoveComponentImpl() but without the actual movement, just calculating the delta for the additional capsule, sweeping that and checking if it would be penetrating
bool UICharacterMovementComponent::SimulateMoveCapsuleComponent(UCapsuleComponent* ComponentToSimulate, const class USceneComponent* CharacterRootComponent, const FVector& NewDelta, const FQuat& NewRotation, FHitResult* OutHit /*= nullptr*/, EMoveComponentFlags MoveFlags /*= MOVECOMP_NoFlags*/)
{
	SCOPE_CYCLE_COUNTER(STAT_SimulateMoveCapsuleComponent);

	// static things can move before they are registered (e.g. immediately after streaming), but not after.
	if (ComponentToSimulate == nullptr || !IsValid(this) || !IsRegistered() || !GetWorld() || !bUseMultiCapsuleCollision)
	{
		if (OutHit)
		{
			OutHit->Init();
		}

		return false; // skip simulation
	}

	

	/*FVector TraceStart;
	FVector TraceEnd;
	FQuat NewCompQuat;
	FComponentQueryParams QueryParams;
	FCollisionResponseParams ResponseParams;
	CalculateSweepData(ComponentToSimulate, NewDelta, NewRotation, TraceStart, TraceEnd, NewCompQuat, QueryParams, ResponseParams);

	TArray<FHitResult> Hits;
	Hits.Empty();*/


	ComponentToSimulate->ConditionalUpdateComponentToWorld();




	const FQuat DeltaQuat = NewRotation * CharacterRootComponent->GetComponentQuat().Inverse();
	const FQuat NewCompQuat = DeltaQuat * ComponentToSimulate->GetComponentQuat();

	const FVector RootComponentLocation = CharacterRootComponent->GetComponentLocation();



	const FVector TraceStart = ComponentToSimulate->GetComponentLocation();

	const FVector DeltaLocation = TraceStart - RootComponentLocation;
	const FVector DeltaDir = DeltaLocation.GetSafeNormal();
	const float DeltaSize = DeltaLocation.Size();
	const FVector NewDir = DeltaQuat.RotateVector(DeltaDir);
	const FVector NewComponentLocation = RootComponentLocation + (NewDir * DeltaSize);

	const FVector TraceEnd = NewComponentLocation + NewDelta;



	TArray<FHitResult> Hits;
	Hits.Empty();

	FComponentQueryParams QueryParams(TEXT("SimMoveComponent"), GetOwner());
	FCollisionResponseParams ResponseParams;
	ComponentToSimulate->InitSweepCollisionParams(QueryParams, ResponseParams);

	const bool bHadBlockingHit = AdditionalCapsuleComponentSweepMulti(Hits, ComponentToSimulate, TraceStart, TraceEnd,
		NewCompQuat, QueryParams, ResponseParams);

	//return ProcessHits(bHadBlockingHit, Hits, TraceStart, TraceEnd, NewDelta, ComponentToSimulate, MoveFlags, OutHit);

	if (Hits.Num() > 0)
	{
		const float NewDeltaSize = NewDelta.Size();
		for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
		{
			PullBackHit(Hits[HitIdx], TraceStart, TraceEnd, NewDeltaSize);
		}
	}

	if (bHadBlockingHit)
	{
		SCOPE_CYCLE_COUNTER(STAT_SimulateMoveCapsuleComponentProcessBlockingHit);
		FHitResult BlockingHit(NoInit);
		BlockingHit.bBlockingHit = false;
		BlockingHit.Time = 1.f;

		int32 BlockingHitIndex = INDEX_NONE;
		float BlockingHitNormalDotDelta = BIG_NUMBER;
		for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
		{
			const FHitResult& TestHit = Hits[HitIdx];

			if (TestHit.bBlockingHit)
			{
				if (!ShouldIgnoreHitResult(GetWorld(), ComponentToSimulate, TestHit, NewDelta, GetOwner(), MoveFlags))
				{
					if (TestHit.Time == 0.f)
					{
						// We may have multiple initial hits, and want to choose the one with the normal most opposed to our movement.
						const float NormalDotDelta = (TestHit.ImpactNormal | NewDelta);
						if (NormalDotDelta < BlockingHitNormalDotDelta)
						{
							BlockingHitNormalDotDelta = NormalDotDelta;
							BlockingHitIndex = HitIdx;
						}
					}
					else if (BlockingHitIndex == INDEX_NONE)
					{
						// First non-overlapping blocking hit should be used, if an overlapping hit was not.
						// This should be the only non-overlapping blocking hit, and last in the results.
						BlockingHitIndex = HitIdx;
						break;
					}
				}
			}
		}

		// Update blocking hit, if there was a valid one.
		if (BlockingHitIndex >= 0)
		{
			BlockingHit = Hits[BlockingHitIndex];

			if (OutHit)
			{
				*OutHit = BlockingHit;
			}

			return false;
		}
		else
		{
			return true;
		}
	}


	return true;

	
}

bool UICharacterMovementComponent::AdditionalCapsuleComponentSweepMulti(TArray<struct FHitResult>& OutHits, 
	class UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, 
	const FComponentQueryParams& Params,
	const FCollisionResponseParams& ResponseParams) const
{
	SCOPE_CYCLE_COUNTER(STAT_SimulateMoveCapsuleComponentSweep);

	if (PrimComp == nullptr)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}

	//return GetWorld()->ComponentSweepMulti(OutHits, PrimComp, Start, End, Rot, Params);

	bool bDoTest = bTestTraceIfNoBlocking;
	if (bDoTest)
	{
		if (const FHitResult* HResult = CachedHits.Find(PrimComp))
		{
			bDoTest = !HResult->bBlockingHit;
		}
	}

	

	if (bDoTest && !World->SweepTestByChannel(Start, End, Rot, 
		PrimComp->GetCollisionObjectType(), PrimComp->GetCollisionShape(0.01f), Params))
	{
		return false;
	}

	return World->SweepMultiByChannel(OutHits, Start, End, Rot,
		PrimComp->GetCollisionObjectType(), PrimComp->GetCollisionShape(0.01f),  Params,
		ResponseParams);
}



void UICharacterMovementComponent::ProcessCompletedAsyncSweepMulti(const FQueuedMultiCapsuleTrace& CompletedTrace)
{
	TArray<FHitResult> BlockedHits;
	TArray<const UPrimitiveComponent*> BlockedComponents;

	for (const FHitResult& Hit : CompletedTrace.OutHits)
	{
		if (Hit.bBlockingHit)
		{
			BlockedHits.Add(Hit);
			BlockedComponents.Add(CompletedTrace.PrimComp.Get());
		}
	}
	

	FHitResult& ComponentResult = CachedHits.FindOrAdd(CompletedTrace.PrimComp.Get());
	ComponentResult.Init();
	ProcessHits(BlockedHits.Num() > 0, BlockedHits, CompletedTrace.Start, CompletedTrace.End, CompletedTrace.NewDelta, CompletedTrace.PrimComp.Get(), CompletedTrace.MoveFlags, &ComponentResult);
}

void UICharacterMovementComponent::CalculateSweepData(class UPrimitiveComponent* ComponentToSimulate, const FVector& NewDelta, const FQuat& NewRotation, FVector& TraceStart, FVector& TraceEnd,
	FQuat& NewCompQuat, FComponentQueryParams& QueryParams, FCollisionResponseParams& ResponseParams)
{
	if (ComponentToSimulate == nullptr)
	{
		return;
	}

	ComponentToSimulate->ConditionalUpdateComponentToWorld();

	const FQuat DeltaQuat = NewRotation * UpdatedComponent->GetComponentQuat().Inverse();
	NewCompQuat = DeltaQuat * ComponentToSimulate->GetComponentQuat();

	const FVector RootComponentLocation = UpdatedComponent->GetComponentLocation();



	TraceStart = ComponentToSimulate->GetComponentLocation();

	const FVector DeltaLocation = TraceStart - RootComponentLocation;
	const FVector DeltaDir = DeltaLocation.GetSafeNormal();
	const float DeltaSize = DeltaLocation.Size();
	const FVector NewDir = DeltaQuat.RotateVector(DeltaDir);
	const FVector NewComponentLocation = RootComponentLocation + (NewDir * DeltaSize);

	TraceEnd = NewComponentLocation + NewDelta;


	QueryParams = FComponentQueryParams(TEXT("SimMoveComponent"), GetOwner());
	ComponentToSimulate->InitSweepCollisionParams(QueryParams, ResponseParams);
}

bool UICharacterMovementComponent::ProcessHits(bool bHadBlockingHits, TArray<FHitResult>& Hits, const FVector& TraceStart, const FVector& TraceEnd, 
	const FVector& NewDelta, const UPrimitiveComponent* ComponentToSimulate, EMoveComponentFlags MoveFlags, FHitResult* OutHit)
{
	if (Hits.Num() > 0)
	{
		const float NewDeltaSize = NewDelta.Size();
		for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
		{
			PullBackHit(Hits[HitIdx], TraceStart, TraceEnd, NewDeltaSize);
		}
	}

	if (bHadBlockingHits)
	{
		SCOPE_CYCLE_COUNTER(STAT_SimulateMoveCapsuleComponentProcessBlockingHit);
		FHitResult BlockingHit(NoInit);
		BlockingHit.bBlockingHit = false;
		BlockingHit.Time = 1.f;

		int32 BlockingHitIndex = INDEX_NONE;
		float BlockingHitNormalDotDelta = BIG_NUMBER;
		for (int32 HitIdx = 0; HitIdx < Hits.Num(); HitIdx++)
		{
			const FHitResult& TestHit = Hits[HitIdx];

			if (TestHit.bBlockingHit)
			{
				if (!ShouldIgnoreHitResult(GetWorld(), ComponentToSimulate, TestHit, NewDelta, GetOwner(), MoveFlags))
				{
					if (TestHit.Time == 0.f)
					{
						// We may have multiple initial hits, and want to choose the one with the normal most opposed to our movement.
						const float NormalDotDelta = (TestHit.ImpactNormal | NewDelta);
						if (NormalDotDelta < BlockingHitNormalDotDelta)
						{
							BlockingHitNormalDotDelta = NormalDotDelta;
							BlockingHitIndex = HitIdx;
						}
					}
					else if (BlockingHitIndex == INDEX_NONE)
					{
						// First non-overlapping blocking hit should be used, if an overlapping hit was not.
						// This should be the only non-overlapping blocking hit, and last in the results.
						BlockingHitIndex = HitIdx;
						break;
					}
				}
			}
		}

		// Update blocking hit, if there was a valid one.
		if (BlockingHitIndex >= 0)
		{
			BlockingHit = Hits[BlockingHitIndex];

			if (OutHit)
			{
				*OutHit = BlockingHit;
			}

			return false;
		}
		else
		{
			return true;
		}
	}

	return true;
}

bool UICharacterMovementComponent::ShouldUseAsyncMultiCapsuleCollision(const UPrimitiveComponent* PComp) const
{
	//Swimming is just temporary until I figure out what's wrong with that
	if ((!bForceAsyncMCC && AsyncCollisionMode == 0) || IsSwimming() || PComp == nullptr || PComp == UpdatedComponent)
	{
		return false;
	}

	if (AsyncCollisionMode == 2)
	{
		return CharacterOwner->GetLocalRole() == ROLE_AutonomousProxy;
	}

	if (AsyncCollisionMode == 1)
	{
		if (const FHitResult* HResult = CachedHits.Find(PComp))
		{
			return HResult->bBlockingHit;
		}
	}

	return false;
}

bool UICharacterMovementComponent::CanUseSituationalAuthority() const
{
	// Situational Authority is allowed if the player is moving fast enough, and there are other players nearby.

	if (Velocity.Size() < 150) return false; // Too slow, doesn't need authority.

	UWorld* World = GetWorld();
	check(World);
	if (!World) return false;

	if (CurrentRootMotion.HasAdditiveVelocity())
	{
		return false;
	}

	AIBaseCharacter* OwnerBaseChar = Cast<AIBaseCharacter>(CharacterOwner);
	check(OwnerBaseChar);
	if (!OwnerBaseChar) return false;

	if (OwnerBaseChar->IsPreloadingClientArea())
	{
		return false;
	}

	check(OwnerBaseChar->GetCapsuleComponent());
	if (!OwnerBaseChar->GetCapsuleComponent()) return false;

	float WorldTime = World->GetTimeSeconds();
	if (bCachedSituationalAuth && (CachedSituationalAuthTime + CacheRelevantDuration) > WorldTime)
	{
		return true;
	}

	bCachedSituationalAuth = false;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(CharacterOwner);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	check(ObjectQueryParams.IsValid());

	float SphereSize = OwnerBaseChar->GetCapsuleComponent()->GetScaledCapsuleRadius() * 15.0f;
	FVector OverlapLocation = OwnerBaseChar->GetActorLocation() + (Velocity.GetSafeNormal() * SphereSize / 2);

	TArray<FOverlapResult> OverlapResults;
	World->OverlapMultiByObjectType(OverlapResults, OverlapLocation, FQuat::Identity, ObjectQueryParams, FCollisionShape::MakeSphere(SphereSize), QueryParams);
	//DrawDebugSphere(GetWorld(), OverlapLocation, SphereSize, 12, FColor::Yellow, false, 0.1f);

	for (const FOverlapResult& Result : OverlapResults)
	{
		if (Result.GetActor())
		{
			if (Cast<AIDinosaurCharacter>(Result.GetActor()) && Result.GetActor() != CharacterOwner)
			{
				bCachedSituationalAuth = true;
				CachedSituationalAuthTime = WorldTime;
				return true;
			}
		}
	}

	return false; // Only allow if there is nearby players. otherwise authority is not needed
}

bool UICharacterMovementComponent::CanUseTolerantAuthority() const
{
	UWorld* World = GetWorld();
	check(World);
	if (!World) return false;

	if (CurrentRootMotion.HasAdditiveVelocity())
	{
		return false;
	}

	AIBaseCharacter* OwnerBaseChar = Cast<AIBaseCharacter>(CharacterOwner);
	check(OwnerBaseChar);
	if (!OwnerBaseChar) return false;

	if (OwnerBaseChar->IsPreloadingClientArea())
	{
		return false;
	}

	return true;
}

#pragma endregion


void FPOTCharacterNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
	FCharacterNetworkMoveData::ClientFillNetworkMoveData(ClientMove, MoveType);
	const FSavedMove_ICharacter& IClientMove = static_cast<const FSavedMove_ICharacter&>(ClientMove);
	Rotation = ClientMove.SavedRotation;
	LaunchVelocity = IClientMove.SavedLaunchVelocity;
	bWantsSituationalAuthority = IClientMove.bSavedWantsSituationalAuthority;
	bRotateToDirection = IClientMove.bSavedPreciseRotateToDirection;
}

bool FPOTCharacterNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	if (!FCharacterNetworkMoveData::Serialize(CharacterMovement, Ar, PackageMap, MoveType))
	{
		return false;
	}
	bool bLocalSuccess = true;
	Rotation.NetSerialize(Ar, PackageMap, bLocalSuccess);
	LaunchVelocity.NetSerialize(Ar, PackageMap, bLocalSuccess);
	Ar.SerializeBits(&bRotateToDirection, 1);
	Ar.SerializeBits(&bWantsSituationalAuthority, 1);

	AActor* const OwnerActor = CharacterMovement.GetOwner();
	if (!ensureAlways(OwnerActor))
	{
		return !Ar.IsError();
	}

	UICharacterMovementComponent& ICharMove = static_cast<UICharacterMovementComponent&>(CharacterMovement);

	bool bTimeDilationUntouched = OwnerActor->CustomTimeDilation == 1.0f;
	Ar.SerializeBits(&bTimeDilationUntouched, 1);

	if (Ar.IsLoading())
	{
#if UE_SERVER || WITH_EDITOR
		if (!bTimeDilationUntouched)
		{
			// Time Dilation Manipulation Detected - Kick/Ban

			ICharMove.TimeDilationManipulationDetected();
		}
#endif
	}

	return !Ar.IsError();
}

void FPOTCharacterNetworkMoveDataContainer::ClientFillNetworkMoveData(const FSavedMove_Character* ClientNewMove, const FSavedMove_Character* ClientPendingMove, const FSavedMove_Character* ClientOldMove)
{
	FCharacterNetworkMoveDataContainer::ClientFillNetworkMoveData(ClientNewMove, ClientPendingMove, ClientOldMove);
}

bool FPOTCharacterNetworkMoveDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap)
{
	return FCharacterNetworkMoveDataContainer::Serialize(CharacterMovement, Ar, PackageMap);
}

void UICharacterMovementComponent::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	const FICharacterMoveResponseDataContainer& POTMoveResponse = static_cast<const FICharacterMoveResponseDataContainer&>(MoveResponse);
	if (!MoveResponse.IsGoodMove())
	{	
		UpdatedComponent->SetWorldRotation(MoveResponse.ClientAdjustment.NewRot, false, nullptr, ETeleportType::TeleportPhysics);
		if (AIBaseCharacter* IBaseCharacter = Cast<AIBaseCharacter>(GetCharacterOwner()))
		{
			if (POTMoveResponse.bHasLaunchVelocity)
			{
				IBaseCharacter->SetLaunchVelocity(POTMoveResponse.ResponseLaunchVelocity);
			}
			else
			{
				IBaseCharacter->SetLaunchVelocity(FVector::ZeroVector);
			}
		}
	}

	Super::ClientHandleMoveResponse(MoveResponse);
}

void UICharacterMovementComponent::ServerMove_HandleMoveData(const FCharacterNetworkMoveDataContainer& MoveDataContainer)
{
	Super::ServerMove_HandleMoveData(MoveDataContainer);
}

void UICharacterMovementComponent::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	if (!HasValidData() || !IsActive() || !IsComponentTickEnabled())
	{
		return;
	}

	const FPOTCharacterNetworkMoveData& POTMoveData = static_cast<const FPOTCharacterNetworkMoveData&>(MoveData);

	const float ClientTimeStamp = POTMoveData.TimeStamp;
	FVector_NetQuantize10 ClientAccel = POTMoveData.Acceleration;
	const uint8 ClientMoveFlags = POTMoveData.CompressedMoveFlags;
	const FRotator ClientControlRotation = POTMoveData.ControlRotation;

	// On engine versions 5.3 and greater, we need to use TransformDirectionToWorld if relative acceleration is used.
	// Convert the move's acceleration to worldspace if necessary
	if (ICharacterMovementCVars::NetUseBaseRelativeAcceleration && MovementBaseUtility::IsDynamicBase(MoveData.MovementBase))
	{
		MovementBaseUtility::TransformDirectionToWorld(MoveData.MovementBase, MoveData.MovementBaseBoneName, MoveData.Acceleration, ClientAccel);
	}

	bRotateToDirection = POTMoveData.bRotateToDirection;

	FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
	check(ServerData);

	if (!VerifyClientTimeStamp(ClientTimeStamp, *ServerData))
	{
		const float ServerTimeStamp = ServerData->CurrentClientTimeStamp;
		return;
	}

	bool bServerReadyForClient = true;
	APlayerController* const PC = Cast<APlayerController>(CharacterOwner->GetController());
	if (PC)
	{
		bServerReadyForClient = PC->NotifyServerReceivedClientData(CharacterOwner, ClientTimeStamp);
		PC->SetControlRotation(ClientControlRotation);
		if (!bServerReadyForClient)
		{
			ClientAccel = FVector::ZeroVector;
		}
	}

	const UWorld* const MyWorld = GetWorld();
	const float DeltaTime = ServerData->GetServerMoveDeltaTime(ClientTimeStamp, CharacterOwner->GetActorTimeDilation(*MyWorld));

	if (DeltaTime <= 0.0f)
	{
		return;
	}

	ServerData->CurrentClientTimeStamp = ClientTimeStamp;
	ServerData->ServerAccumulatedClientTimeStamp += DeltaTime;
	ServerData->ServerTimeStamp = MyWorld->GetTimeSeconds();
	ServerData->ServerTimeStampLastServerMove = ServerData->ServerTimeStamp;
	
	if (!bServerReadyForClient || MyWorld->GetWorldSettings()->GetPauserPlayerState() != nullptr)
	{
		return;
	}

	bool bClientIsPreloading = false;
	const AIBaseCharacter* const IBaseChar = Cast<const AIBaseCharacter>(CharacterOwner);
	if (IBaseChar)
	{
		bClientIsPreloading = IBaseChar->IsPreloadingClientArea();
	}
	
	bIsDoingApproxMove = ICharacterMovementCVars::CVarApproximateValidation->GetBool() && !bClientIsPreloading;
	
	ApproxMoveOldLocation = UpdatedComponent->GetComponentLocation();
	ApproxMoveOldRotator = UpdatedComponent->GetComponentRotation();

	// Perform actual movement
	if (PC)
	{
		PC->UpdateRotation(DeltaTime);
	}

	MoveAutonomous(ClientTimeStamp, DeltaTime, ClientMoveFlags, ClientAccel);
	
	// Validate move only after old and first dual portion, after all moves are completed.
	if (POTMoveData.NetworkMoveType == FCharacterNetworkMoveData::ENetworkMoveType::NewMove)
	{
		if (bIsDoingApproxMove)
		{
			ServerMoveHandleClientErrorApproximate(ClientTimeStamp, DeltaTime, ClientAccel, POTMoveData);
		}
		else
		{
			ServerMoveHandleClientErrorWithRotation(ClientTimeStamp, DeltaTime, ClientAccel, POTMoveData);
		}
	}

	bIsDoingApproxMove = false;
}

void UICharacterMovementComponent::ServerMoveHandleClientErrorApproximate(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FPOTCharacterNetworkMoveData& POTMoveData)
{
	const AIBaseCharacter* const IBaseChar = Cast<const AIBaseCharacter>(CharacterOwner);
	check(IBaseChar);
	
	FNetworkPredictionData_Server_ICharacter* const ServerData = GetPredictionData_Server_ICharacter();
	check(ServerData);
	
	if (!ensureAlways(UpdatedComponent))
	{
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = true;
		return;
	}

	const FVector& RelativeClientLoc = POTMoveData.Location;
	const FRotator& ClientRotation = POTMoveData.Rotation;
	UPrimitiveComponent* const ClientMovementBase = POTMoveData.MovementBase;
	const FName ClientBaseBoneName = POTMoveData.MovementBaseBoneName;
	const uint8 ClientMovementMode = POTMoveData.MovementMode;

	FVector ClientLoc = RelativeClientLoc;
	if (MovementBaseUtility::UseRelativeLocation(ClientMovementBase))
	{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION <= 0
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(ClientMovementBase, ClientBaseBoneName, BaseLocation, BaseRotation);
		ClientLoc += BaseLocation;
#else
		// On engine versions 5.1 and greater, we need to use TransformLocationToWorld to resolve the rotation of the based movement.
		MovementBaseUtility::TransformLocationToWorld(ClientMovementBase, ClientBaseBoneName, RelativeClientLoc, ClientLoc);
#endif
	}
	else
	{
		ClientLoc = FRepMovement::RebaseOntoLocalOrigin(ClientLoc, this);
	}
	
	const FVector CurrentLocation = UpdatedComponent->GetComponentLocation();

	static const float MaxClientRotationError = 0.66f; // Radians
	const bool bLocationAccepted = UKismetMathLibrary::GetPointDistanceToSegment(ClientLoc, ApproxMoveOldLocation, CurrentLocation) <= FMath::Max(FVector::Distance(ApproxMoveOldLocation, CurrentLocation), 100.0f);
	const bool bRotationAccepted = GetDistanceBetween(UpdatedComponent->GetComponentRotation(), ClientRotation) <= MaxClientRotationError;
	const bool bClientIsPreloading = IBaseChar->IsPreloadingClientArea();

	const bool bForceNextClientAdjustment = ServerData->bForceNextClientAdjustment;
	ServerData->bForceNextClientAdjustment = false;

	if (bLocationAccepted && bRotationAccepted && !bForceNextClientAdjustment)
	{
		if (!bClientIsPreloading)
		{
			UpdatedComponent->SetWorldLocationAndRotation(ClientLoc, ClientRotation);

			// Trust the client's movement mode.
			ApplyNetworkMovementMode(ClientMovementMode);

			// Update base and floor at new location.
			SetBase(ClientMovementBase, ClientBaseBoneName);

			// Even if base has not changed, we need to recompute the relative offsets (since we've moved).
			SaveBaseLocation();

			LastUpdateLocation = CurrentLocation;
			LastUpdateRotation = UpdatedComponent->GetComponentQuat();
			LastUpdateVelocity = Velocity;
		}

		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = true;
	}
	else
	{
		UPrimitiveComponent* const MovementBase = CharacterOwner->GetMovementBase();
		FName MovementBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;

		const FVector AdjustPoint = bForceNextClientAdjustment ? CurrentLocation : ApproxMoveOldLocation;
		ApproxMoveOldLocation = AdjustPoint;
		//UE_LOG(LogTemp, Warning, TEXT("Sending adjustment to %s"), *AdjustPoint.ToString());

		ServerData->PendingAdjustment.NewVel = Velocity;
		ServerData->PendingAdjustment.NewBase = MovementBase;
		ServerData->PendingAdjustment.NewBaseBoneName = MovementBaseBoneName;
		ServerData->PendingAdjustment.NewLoc = FRepMovement::RebaseOntoZeroOrigin(AdjustPoint, this);
		ServerData->PendingAdjustment.NewRot = UpdatedComponent->GetComponentRotation();
		ServerData->PendingLaunchVelocityAdjustment = IBaseChar->GetLaunchVelocity();

		ServerData->PendingAdjustment.bBaseRelativePosition = false;

		ServerData->LastUpdateTime = GetWorld()->TimeSeconds;
		ServerData->PendingAdjustment.DeltaTime = DeltaTime;
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = false;
		ServerData->PendingAdjustment.MovementMode = PackNetworkMovementMode();
	}
}

// Same as Super function but with rotation taken into consideration
void UICharacterMovementComponent::ServerMoveHandleClientErrorWithRotation(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FPOTCharacterNetworkMoveData& POTMoveData)
{
	const FVector& RelativeClientLoc = POTMoveData.Location;
	const FRotator& ClientRotation = POTMoveData.Rotation;
	UPrimitiveComponent* ClientMovementBase = POTMoveData.MovementBase;
	FName ClientBaseBoneName = POTMoveData.MovementBaseBoneName;
	uint8 ClientMovementMode = POTMoveData.MovementMode;

	if (!ShouldUsePackedMovementRPCs())
	{
		if (RelativeClientLoc == FVector(1.f, 2.f, 3.f)) // first part of double servermove
		{
			return;
		}
	}

	FNetworkPredictionData_Server_ICharacter* ServerData = static_cast<FNetworkPredictionData_Server_ICharacter*>(GetPredictionData_Server_Character());
	check(ServerData);

	// Don't prevent more recent updates from being sent if received this frame.
	// We're going to send out an update anyway, might as well be the most recent one.
	APlayerController* PC = Cast<APlayerController>(CharacterOwner->GetController());
	if ((ServerData->LastUpdateTime != GetWorld()->TimeSeconds))
	{
		const AGameNetworkManager* GameNetworkManager = (const AGameNetworkManager*)(AGameNetworkManager::StaticClass()->GetDefaultObject());
		if (GameNetworkManager->WithinUpdateDelayBounds(PC, ServerData->LastUpdateTime))
		{
			//GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Red, TEXT("Within update bounds"));
			return;
		}
	}

	// Offset may be relative to base component
	FVector ClientLoc = RelativeClientLoc;
	if (MovementBaseUtility::UseRelativeLocation(ClientMovementBase))
	{
#if ENGINE_MAJOR_VERSION <= 5 && ENGINE_MINOR_VERSION <= 0
		FVector BaseLocation;
		FQuat BaseRotation;
		MovementBaseUtility::GetMovementBaseTransform(ClientMovementBase, ClientBaseBoneName, BaseLocation, BaseRotation);
		ClientLoc += BaseLocation;
#else
		// On engine versions 5.1 and greater, we need to use TransformLocationToWorld to resolve the rotation of the based movement.
		MovementBaseUtility::TransformLocationToWorld(ClientMovementBase, ClientBaseBoneName, RelativeClientLoc, ClientLoc);
#endif
	}
	else
	{
		ClientLoc = FRepMovement::RebaseOntoLocalOrigin(ClientLoc, this);
	}

	FVector ServerLoc = UpdatedComponent->GetComponentLocation();

	bool bLargeLocationDifference = FVector::Distance(ClientLoc, ServerLoc) >= MaxLocationDiffBeforeCorrection;
	bool bUsingSituationalAuthority = POTMoveData.bWantsSituationalAuthority && !bLargeLocationDifference;
	if (bUsingSituationalAuthority)
	{	
		// If the client requests situational authority, we need to validate it on the server to prevent activating it when it is not possible.
		bUsingSituationalAuthority = CanUseSituationalAuthority();
	}

	bool bUsingAuthoritiveTolerance = false;
	bool bUsedAuthTolerance = false;
	if (!bUsingSituationalAuthority && !bLargeLocationDifference && CanUseTolerantAuthority())
	{ 
		// If we are not using situational authority, we can use an authority tolerance value to allow some corrections to be ignored, helping with packet loss.
		bUsingAuthoritiveTolerance = AuthoritiveBuildup < AuthoritiveTolerance;
	}

	// Client may send a null movement base when walking on bases with no relative location (to save bandwidth).
	// In this case don't check movement base in error conditions, use the server one (which avoids an error based on differing bases). Position will still be validated.
	if (ClientMovementBase == nullptr)
	{
		TEnumAsByte<EMovementMode> NetMovementMode(MOVE_None);
		TEnumAsByte<EMovementMode> NetGroundMode(MOVE_None);
		uint8 NetCustomMode(0);
		UnpackNetworkMovementMode(ClientMovementMode, NetMovementMode, NetCustomMode, NetGroundMode);
		if (NetMovementMode == MOVE_Walking)
		{
			ClientMovementBase = CharacterOwner->GetBasedMovement().MovementBase;
			ClientBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;
		}
	}

	// If base location is out of sync on server and client, changing base can result in a jarring correction.
	// So in the case that the base has just changed on server or client, server trusts the client (within a threshold)
	UPrimitiveComponent* MovementBase = CharacterOwner->GetMovementBase();
	FName MovementBaseBoneName = CharacterOwner->GetBasedMovement().BoneName;
	const bool bServerIsFalling = IsFalling();
	const bool bClientIsFalling = ClientMovementMode == MOVE_Falling;
	const bool bServerJustLanded = bLastServerIsFalling && !bServerIsFalling;
	const bool bClientJustLanded = bLastClientIsFalling && !bClientIsFalling;

	FVector RelativeLocation = ServerLoc;
	FVector RelativeVelocity = Velocity;
	bool bUseLastBase = false;
	bool bFallingWithinAcceptableError = false;

	AIBaseCharacter* IBaseChar = Cast<AIBaseCharacter>(CharacterOwner);
	check(IBaseChar);

	bool bClientIsPreloading = IBaseChar->IsPreloadingClientArea();
	bool bForceNextClientAdjustment = ServerData->bForceNextClientAdjustment;
	ServerData->bForceNextClientAdjustment = false;

	IBaseChar->SetLaunchVelocity(POTMoveData.LaunchVelocity);

	bool bAcceptAbilityMovement = false;
	if (UPOTAbilitySystemComponent* AbilitySystem = Cast<UPOTAbilitySystemComponent>(IBaseChar->AbilitySystem))
	{
		if (AbilitySystem->HasAbilityForcedMovementSpeed() || AbilitySystem->HasAbilityForcedRotation())
		{
			bAcceptAbilityMovement = true;
		}
	}

	// Compute the client error from the server's position
	// If client has accumulated a noticeable positional error, correct them.
	bNetworkLargeClientCorrection = ServerData->bForceClientUpdate;
	if (bForceNextClientAdjustment || bLargeLocationDifference || (!bAcceptAbilityMovement && !bUsingSituationalAuthority && (!bFallingWithinAcceptableError && ServerCheckClientErrorWithRotation(ClientTimeStamp, DeltaTime, Accel, ClientLoc, ClientRotation, RelativeClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode, POTMoveData.LaunchVelocity))))
	{
		if (bUsingAuthoritiveTolerance && !bLargeLocationDifference && IBaseChar->GetAttachParentActor() == nullptr && !bForceNextClientAdjustment)
		{ 
			// If we have some authoritive tolerance remaining then we can skip this correction
			bUsedAuthTolerance = ServerAcceptClientAuthoritiveMove(DeltaTime, ClientLoc, ClientRotation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode, true);
		}
		else
		{
			ServerData->PendingAdjustment.NewVel = Velocity;
			ServerData->PendingAdjustment.NewBase = MovementBase;
			ServerData->PendingAdjustment.NewBaseBoneName = MovementBaseBoneName;
			ServerData->PendingAdjustment.NewLoc = FRepMovement::RebaseOntoZeroOrigin(ServerLoc, this);
			ServerData->PendingAdjustment.NewRot = UpdatedComponent->GetComponentRotation();
			ServerData->PendingLaunchVelocityAdjustment = IBaseChar->GetLaunchVelocity();

			ServerData->PendingAdjustment.bBaseRelativePosition = bUseLastBase || MovementBaseUtility::UseRelativeLocation(MovementBase);
			ServerData->PendingAdjustment.bBaseRelativeVelocity = false;

			if (ServerData->PendingAdjustment.bBaseRelativePosition)
			{
				ServerData->PendingAdjustment.NewLoc = CharacterOwner->GetBasedMovement().Location;
				if (ICharacterMovementCVars::NetUseBaseRelativeVelocity)
				{
					// Store world velocity converted to local space of movement base
					ServerData->PendingAdjustment.bBaseRelativeVelocity = true;
					const FVector CurrentVelocity = ServerData->PendingAdjustment.NewVel;
					MovementBaseUtility::TransformDirectionToLocal(MovementBase, MovementBaseBoneName, CurrentVelocity, ServerData->PendingAdjustment.NewVel);
				}
			}

			ServerData->LastUpdateTime = GetWorld()->TimeSeconds;
			ServerData->PendingAdjustment.DeltaTime = DeltaTime;
			ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
			ServerData->PendingAdjustment.bAckGoodMove = false;
			ServerData->PendingAdjustment.MovementMode = PackNetworkMovementMode();
		}
	}
	else
	{
		// Don't use authoritative when attached to something, it can cause drifting
		if (!bClientIsPreloading && IBaseChar->GetAttachParentActor() == nullptr && (bAcceptAbilityMovement || bUsingSituationalAuthority || ServerShouldUseAuthoritativePosition(ClientTimeStamp, DeltaTime, Accel, ClientLoc, RelativeClientLoc, ClientMovementBase, ClientBaseBoneName, ClientMovementMode)))
		{
			ServerAcceptClientAuthoritiveMove(DeltaTime, ClientLoc, ClientRotation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode, false);
		}

		// acknowledge receipt of this successful servermove()
		ServerData->PendingAdjustment.TimeStamp = ClientTimeStamp;
		ServerData->PendingAdjustment.bAckGoodMove = true;
	}

	if (!bUsedAuthTolerance)
	{ 
		// If we did not use our authoritive tolerance now, decrease it to allow more tolerance later
		AuthoritiveBuildup -= DeltaTime * AuthoritiveToleranceCooldown;
		if (AuthoritiveBuildup < 0)
		{
			AuthoritiveBuildup = 0;
		}
		//GEngine->AddOnScreenDebugMessage(123123, 1.0f, FColor::Yellow, FString::SanitizeFloat(AuthoritiveBuildup));
	}

	ServerData->bForceClientUpdate = false;

	bLastClientIsFalling = bClientIsFalling;
	bLastServerIsFalling = bServerIsFalling;
	bLastServerIsWalking = MovementMode == MOVE_Walking;
}

bool UICharacterMovementComponent::ServerAcceptClientAuthoritiveMove(float DeltaTime, const FVector& ClientLoc, const FRotator& ClientRotation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode, bool bFromAuthTolerance)
{
	const FVector LocDiff = UpdatedComponent->GetComponentLocation() - ClientLoc; //-V595
	const FRotator RotDiff = UpdatedComponent->GetComponentRotation() - ClientRotation;
	float TotalRotError = GetDistanceBetween(UpdatedComponent->GetComponentRotation(), ClientRotation);
	if (!LocDiff.IsZero() || TotalRotError > KINDA_SMALL_NUMBER || ClientMovementMode != PackNetworkMovementMode() || GetMovementBase() != ClientMovementBase || (CharacterOwner && CharacterOwner->GetBasedMovement().BoneName != ClientBaseBoneName))
	{
		if (bFromAuthTolerance)
		{
			// Increase our authoritive buildup, increasing more depending on the difference in location by deltatime.
			// If location is larger or deltatime is smaller, we have a larger buildup.
			AuthoritiveBuildup += FMath::Sqrt(LocDiff.Size()) / DeltaTime;
			if (AuthoritiveBuildup > MaxAuthoritiveBuildup)
			{
				AuthoritiveBuildup = MaxAuthoritiveBuildup;
			}
			//GEngine->AddOnScreenDebugMessage(123123, 1.0f, FColor::Yellow, FString::SanitizeFloat(AuthoritiveBuildup));
			//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString(TEXT("AuthBuildup + ")) + FString::SanitizeFloat(LocDiff.Size() / DeltaTime));
		}

		// Just set the position. On subsequent moves we will resolve initially overlapping conditions.
		UpdatedComponent->SetWorldLocation(ClientLoc, false); //-V595
		UpdatedComponent->SetWorldRotation(ClientRotation, false);

		// Trust the client's movement mode.
		ApplyNetworkMovementMode(ClientMovementMode);

		// Update base and floor at new location.
		SetBase(ClientMovementBase, ClientBaseBoneName);
		UpdateFloorFromAdjustment();

		// Even if base has not changed, we need to recompute the relative offsets (since we've moved).
		SaveBaseLocation();

		LastUpdateLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;
		LastUpdateRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
		LastUpdateVelocity = Velocity;

		return true; // server accepted a new client auth move
	}

	return false; // server did not do anything with the client auth move
}

bool UICharacterMovementComponent::ServerCheckClientErrorWithRotation(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FRotator& ClientWorldRotation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode, const FVector& LaunchVelocity)
{
	AIBaseCharacter* OwnerBaseChar = Cast<AIBaseCharacter>(GetCharacterOwner());
	check(OwnerBaseChar);
	if (!OwnerBaseChar) return false;

	static const float MaxAngleDifference = 0.66f; // radians

	float TotalError = GetDistanceBetween(ClientWorldRotation, UpdatedComponent->GetComponentRotation());

	if (FMath::Abs(TotalError) > MaxAngleDifference)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString(TEXT("Performed rotation correction on ")) + FString(CharacterOwner->GetName()) + FString(TEXT(", distance ")) + FString::SanitizeFloat(TotalError));
		//return true;
	}

	if (FVector::Distance(OwnerBaseChar->GetLaunchVelocity(), LaunchVelocity) > 15)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString(TEXT("Performed launc vel correction on ")) + FString(CharacterOwner->GetName()));
		return true;
	}

	bool bMoveError = Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode);
	if (bMoveError)
	{
		//GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, FString(TEXT("Performed movement correction on ")) + FString(CharacterOwner->GetName()));
	}
	return bMoveError;
}

float UICharacterMovementComponent::GetDistanceBetween(const FRotator& Rot1, const FRotator& Rot2) const
{
	FQuat Rot1Quat = Rot1.Quaternion();
	FQuat Rot2Quat = Rot2.Quaternion();
	return Rot1Quat.AngularDistance(Rot2Quat);
}

void UICharacterMovementComponent::ManualSendClientCameraUpdate()
{
	// MarkForClientCameraUpdate is protected so expose it through this function. -Poncho
	MarkForClientCameraUpdate();
}

FNetworkPredictionData_Client_ICharacter* UICharacterMovementComponent::GetPredictionData_Client_ICharacter() const
{
	return static_cast<class FNetworkPredictionData_Client_ICharacter*>(GetPredictionData_Client());
}

FNetworkPredictionData_Server_ICharacter* UICharacterMovementComponent::GetPredictionData_Server_ICharacter() const
{
	return static_cast<class FNetworkPredictionData_Server_ICharacter*>(GetPredictionData_Server());
}

void FSavedMove_ICharacter::Clear()
{
	Super::Clear();

	bSavedWantsToSprint = false;
	bSavedWantsToTrot = false;
	bSavedWantsToLimp = false;
	bSavedWantsToStop = false;
	bSavedWantsToPreciseMove = false;
	bSavedWantsSituationalAuthority = false;
	bSavedPreciseRotateToDirection = false;
	SavedLaunchVelocity = FVector::ZeroVector;
}

void FSavedMove_ICharacter::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	UICharacterMovementComponent* CharMov = Cast<UICharacterMovementComponent>(C->GetCharacterMovement());
	if (CharMov)
	{
		//This is literally just the exact opposite of UpdateFromCompressed flags. We're taking the input
		//from the player and storing it in the saved move.
		bSavedWantsToSprint = CharMov->bWantsToSprint;
		bSavedWantsToTrot = CharMov->bWantsToTrot;
		bSavedWantsToLimp = CharMov->bWantsToLimp;
		bSavedWantsToStop = CharMov->bWantsToStop;
		bSavedWantsToPreciseMove = CharMov->bWantsToPreciseMove;
		bSavedWantsSituationalAuthority = CharMov->bWantsSituationalAuthority;
		bSavedPreciseRotateToDirection = CharMov->bRotateToDirection;

		AIBaseCharacter* OwnerBaseCharacter = Cast<AIBaseCharacter>(C);
		check(OwnerBaseCharacter);
		if (OwnerBaseCharacter)
		{
			StartLaunchVelocity = OwnerBaseCharacter->GetLaunchVelocity();
		}
	}
}

bool FSavedMove_ICharacter::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	if (!LastAckedMove.IsValid()) return false;
	
	FSavedMove_ICharacter* LastAckedIMove = static_cast<FSavedMove_ICharacter*>(LastAckedMove.Get());
	if (bSavedWantsToSprint != LastAckedIMove->bSavedWantsToSprint) return true;
	if (bSavedWantsToTrot != LastAckedIMove->bSavedWantsToTrot) return true;
	if (bSavedWantsToLimp != LastAckedIMove->bSavedWantsToLimp) return true;
	if (bSavedWantsToStop != LastAckedIMove->bSavedWantsToStop) return true;
	if (bSavedWantsToPreciseMove != LastAckedIMove->bSavedWantsToPreciseMove) return true;
	if (bSavedWantsSituationalAuthority != LastAckedIMove->bSavedWantsSituationalAuthority) return true;
	if (bSavedPreciseRotateToDirection != LastAckedIMove->bSavedPreciseRotateToDirection) return true;
	if (SavedLaunchVelocity != LastAckedIMove->SavedLaunchVelocity)
	{
		if ((FMath::Abs(SavedLaunchVelocity.Size() - LastAckedIMove->SavedLaunchVelocity.Size()) > 50) || ((SavedLaunchVelocity.GetSafeNormal() | LastAckedIMove->SavedLaunchVelocity.GetSafeNormal()) < AccelDotThreshold))
		{
			return true;
		}
	}
	return Super::IsImportantMove(LastAckedMove);
}

void FSavedMove_ICharacter::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	Super::PostUpdate(C, PostUpdateMode);
	UICharacterMovementComponent* CharMov = Cast<UICharacterMovementComponent>(C->GetCharacterMovement());
	check(CharMov);
	if (!CharMov) return;

	if (AIBaseCharacter* BaseCharacter = Cast<AIBaseCharacter>(C))
	{
		SavedLaunchVelocity = BaseCharacter->GetLaunchVelocity();
	}
	if (PostUpdateMode == EPostUpdateMode::PostUpdate_Replay)
	{
		CharMov->bWantsToSprint = bSavedWantsToSprint;
		CharMov->bWantsToTrot = bSavedWantsToTrot;
		CharMov->bWantsToLimp = bSavedWantsToLimp;
		CharMov->bWantsToStop = bSavedWantsToStop;
		CharMov->bWantsToPreciseMove = bSavedWantsToPreciseMove;
		CharMov->bWantsSituationalAuthority = bSavedWantsSituationalAuthority;
		CharMov->bRotateToDirection = bSavedPreciseRotateToDirection;
	}
	else
	{
		bSavedWantsSituationalAuthority = CharMov->CanUseSituationalAuthority();
	}
}

void FSavedMove_ICharacter::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);
	if (AIBaseCharacter* BaseCharacter = Cast<AIBaseCharacter>(C))
	{
		StartLaunchVelocity = BaseCharacter->GetLaunchVelocity();
	}
}

bool FSavedMove_ICharacter::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InPawn, float MaxDelta) const
{
	
	FSavedMove_ICharacter* NewIMove = static_cast<FSavedMove_ICharacter*>(NewMove.Get());
	if (bSavedWantsToSprint != NewIMove->bSavedWantsToSprint) return false;
	if (bSavedWantsToTrot != NewIMove->bSavedWantsToTrot) return false;
	if (bSavedWantsToLimp != NewIMove->bSavedWantsToLimp) return false;
	if (bSavedWantsToStop != NewIMove->bSavedWantsToStop) return false;
	if (bSavedWantsToPreciseMove != NewIMove->bSavedWantsToPreciseMove) return false;
	if (bSavedWantsSituationalAuthority != NewIMove->bSavedWantsSituationalAuthority) return false;
	if (bSavedPreciseRotateToDirection != NewIMove->bSavedPreciseRotateToDirection) return false;
	if (SavedLaunchVelocity != NewIMove->SavedLaunchVelocity)
	{
		if ((FMath::Abs(SavedLaunchVelocity.Size() - NewIMove->SavedLaunchVelocity.Size()) > 50) || ((SavedLaunchVelocity.GetSafeNormal() | NewIMove->SavedLaunchVelocity.GetSafeNormal()) < AccelDotThreshold))
		{
			return false;
		}
	}

	AIBaseCharacter* BaseCharacter = Cast<AIBaseCharacter>(InPawn);
	check(BaseCharacter);
	bool bCanCombine = Super::CanCombineWith(NewMove, InPawn, MaxDelta);

	return bCanCombine;
}

void FSavedMove_ICharacter::CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation)
{
	const FSavedMove_ICharacter* OldIMove = static_cast<const FSavedMove_ICharacter*>(OldMove);

	if (AIBaseCharacter* BaseCharacter = Cast<AIBaseCharacter>(InCharacter))
	{
		BaseCharacter->SetLaunchVelocity(OldIMove->StartLaunchVelocity);
	}

	Super::CombineWith(OldMove, InCharacter, PC, OldStartLocation);
}

void FSavedMove_ICharacter::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	UICharacterMovementComponent* CharMov = Cast<UICharacterMovementComponent>(C->GetCharacterMovement());
	if (CharMov)
	{
		CharMov->bWantsToSprint = bSavedWantsToSprint;
		CharMov->bWantsToTrot = bSavedWantsToTrot;
		CharMov->bWantsToLimp = bSavedWantsToLimp;
		CharMov->bWantsToStop = bSavedWantsToStop;
		CharMov->bWantsToPreciseMove = bSavedWantsToPreciseMove;
		CharMov->bWantsSituationalAuthority = bSavedWantsSituationalAuthority;
		CharMov->bRotateToDirection = bSavedPreciseRotateToDirection;
	}
}

uint8 FSavedMove_ICharacter::GetCompressedFlags() const
{
	uint8 CompressedFlags = Super::GetCompressedFlags();

	if (bSavedWantsToSprint)
	{
		CompressedFlags |= FLAG_Sprinting;
	}

	if (bSavedWantsToTrot)
	{
		CompressedFlags |= FLAG_Trotting;
	}

	if (bSavedWantsToLimp)
	{
		CompressedFlags |= FLAG_Limping;
	}

	if (bSavedWantsToStop)
	{
		CompressedFlags |= FLAG_Stop;
	}

	if (bSavedWantsToPreciseMove)
	{
		CompressedFlags |= FLAG_PreciseMove;
	}

	return CompressedFlags;
}

FSavedMovePtr FNetworkPredictionData_Client_ICharacter::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_ICharacter());
}

void FICharacterMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	FCharacterMoveResponseDataContainer::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	FNetworkPredictionData_Server_ICharacter* ServerData = static_cast<FNetworkPredictionData_Server_ICharacter*>(CharacterMovement.GetPredictionData_Server_Character());
	bHasRotation = true;
	if (AIBaseCharacter* CharacterOwner = Cast<AIBaseCharacter>(CharacterMovement.GetCharacterOwner()))
	{
		//ResponseLaunchVelocity = ServerData->PendingLaunchVelocityAdjustment;
		//bHasLaunchVelocity = ResponseLaunchVelocity.Size() > KINDA_SMALL_NUMBER;
		bHasLaunchVelocity = false;
	}
}

bool FICharacterMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap)
{
	if (!FCharacterMoveResponseDataContainer::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}
	if (IsCorrection())
	{
		Ar.SerializeBits(&bHasLaunchVelocity, 1);

		if (bHasLaunchVelocity)
		{
			bool bLocalSuccess = true;
			ResponseLaunchVelocity.NetSerialize(Ar, PackageMap, bLocalSuccess);
		}
	}
	return !Ar.IsError();
}

