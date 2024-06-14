// Copyright 2015-2019 Alderon Games Pty Ltd, All Rights Reserved.

#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "ITypes.h"
#include "StateAdjustableCapsuleComponent.h"
#include "ICharacterMovementComponent.generated.h"

class AIDinosaurCharacter;

struct FICharacterMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{
public:
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;

	FVector ResponseLaunchVelocity;
	bool bHasLaunchVelocity = false;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;

};

struct FPOTCharacterNetworkMoveData : public FCharacterNetworkMoveData
{
public:
	virtual ~FPOTCharacterNetworkMoveData()
	{
	}

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;

	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;

	FRotator Rotation = FRotator::ZeroRotator;
	FVector LaunchVelocity = FVector::ZeroVector;
	bool bWantsSituationalAuthority = false;
	bool bRotateToDirection = false;
};


struct FPOTCharacterNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{
public:

	/**
	 * Default constructor. Sets data storage (NewMoveData, PendingMoveData, OldMoveData) to point to default data members. Override those pointers to instead point to custom data if you want to use derived classes.
	 */
	FPOTCharacterNetworkMoveDataContainer()
	{
		NewMoveData = &POTMoveData[0];
		PendingMoveData = &POTMoveData[1];
		OldMoveData = &POTMoveData[2];
	}

	virtual ~FPOTCharacterNetworkMoveDataContainer()
	{
	}

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character* ClientNewMove, const FSavedMove_Character* ClientPendingMove, const FSavedMove_Character* ClientOldMove) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;

private:

	FPOTCharacterNetworkMoveData POTMoveData[3];
};

UCLASS()
class PATHOFTITANS_API UICharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()
	UICharacterMovementComponent(const FObjectInitializer& ObjectInitialiser);

protected:
	//The current water level at the location of this character.
	float WaterLevel;

	FICharacterMoveResponseDataContainer MoveResponseDataContainer;
	FPOTCharacterNetworkMoveDataContainer POTMoveDataContainer;

public:
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Wa Character Movement")
	TArray<FSurfaceFX> FootstepFX;

	UFUNCTION(BlueprintPure, Category = "Wa Character Movement")
	FORCEINLINE float GetWaterLevel() const { return WaterLevel; }

	void ManualSendClientCameraUpdate();

	// Movement Type Functions
	bool IsAtWaterSurface() const;
	bool IsMovingForward() const;
	bool IsMoving() const;

	bool bReceivedRep = false;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override; 
	virtual void SetDefaultMovementMode() override;
	virtual void SimulateMovement(float DeltaTime) override;
	virtual void FindFloor(const FVector& CapsuleLocation, FFindFloorResult& OutFloorResult, bool bCanUseCachedLocation, const FHitResult* DownwardSweepResult = NULL) const override;
	virtual void SetBase(UPrimitiveComponent* NewBase, const FName BoneName = NAME_None, bool bNotifyActor = true) override;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PoT - Character Movement")
	bool bCanUsePreciseNavigation;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PoT - Character Movement")
	bool bCanUsePreciseSwimming;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PoT - Character Movement")
	bool bCanUsePreciseSwimmingStrafe;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PoT - Character Movement")
	bool bCanUsePreciseSwimmingPitchControl;

	//When an ability is used that uses Forced Rotation, smoothly rotate to the desired rotation rather than snapping to it.
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PoT - Character Movement")
	bool bUseSmoothForcedRotation = true;

	/**
	 * Allows sprinting in water
	 */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PoT - Character Movement")
	bool bCanUseAdvancedSwimming;

	bool bRotateToDirection = false;

	//The dot tolerance after which the collision will be ignored. Used to handle slope movement properly.
	UPROPERTY(Category = "Character Movement (Multi Capsule Collision)", EditAnywhere, AdvancedDisplay)
	float AdditionalCapsuleDotTolerance = 0.69f;
	UPROPERTY(Category = "Character Movement (Multi Capsule Collision)", EditAnywhere, AdvancedDisplay)
	float AdditionalCapsuleMaxDotTolerance = 0.90f;

	bool CanUseSituationalAuthority() const;
	bool CanUseTolerantAuthority() const;

	virtual float ImmersionDepth() const override;

protected:

	UPROPERTY(BlueprintReadWrite)
	bool bIsInHotAir = false;

	virtual void OnTimeDiscrepancyDetected(float CurrentTimeDiscrepancy, float LifetimeRawTimeDiscrepancy, float Lifetime, float CurrentMoveError) override;


	// Caching the situational authority result to reduce the amount of physics tests needed
	mutable bool bCachedSituationalAuth = false;
	mutable float CachedSituationalAuthTime = 0;
	float CacheRelevantDuration = 0.5f;

	// Tolerance to allow server to ignore corrections. The more corrections, the higher this tolerance will go.
	// This can help prevent too many corrections due to packet loss, but will not allow hackers to have a constant authoritive position.
	float AuthoritiveTolerance = 2000.0f;
	float AuthoritiveBuildup = 0.0f;
	float MaxAuthoritiveBuildup = 3000.0f;
	float AuthoritiveToleranceCooldown = 1000.0f;

	// If location difference is larger than this, situational and tolerant authority is ignored and a correction must be made.
	float MaxLocationDiffBeforeCorrection = 600.0f;

public:
#pragma region GlobalStateFlags
	/**
	 * Used to start a new type of movement
	 */

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsToKnockback : 1;

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsToSprint : 1;

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsToTrot: 1;

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsToLimp : 1;

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsToStop : 1;

	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsToPreciseMove : 1;

	// Situational Authority can be activated in situations where lots of rubber banding is unavoidable.
	// This is mostly when multiple players are interacting at high velocities. When this is activated,
	// no server corrections will be sent, and the client position will be trusted (in most cases).
	// When a client requests this, it will be validated on the server before being trusted.
	UPROPERTY(BlueprintReadWrite, Category = CharacterMovement)
	uint8 bWantsSituationalAuthority : 1;
#pragma endregion

#pragma region CharacterMovementParams
	UPROPERTY(Category = "Character Movement (General Settings)", EditDefaultsOnly, BlueprintReadWrite)
	uint8 bUseSeparateSwimmingBrakingFrictionFactor : 1;

	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", EditCondition = "bUseSeparateSwimmingBrakingFriction"))
	float BrakingFrictionSwimmingFactor = 2.0f;

	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", EditCondition = "bCanUsePreciseNavigation"))
	float BrakingFrictionPreciseFactor = 30.0f;

	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float BrakingFrictionKnockbackFactor = 1.0f;

	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float GroundHitSpeedLossMultiplier = 1.0f;

	//If an impact happens above this speed, the dino will die.
	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float MaxImpactSpeed = 6500.0f;

	UPROPERTY(Category = "Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	bool bCanTakeImpactDamage = true;

	UPROPERTY(Category = "Character Movement: (Knockback)", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0"))
	float BrakingDecelerationKnockback = 100.0f;

	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	float MaxTrotSpeed;

	UPROPERTY(Category = "Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	float MaxRunSpeed;

	UPROPERTY(Category = "Character Movement: Acceleration", EditAnywhere, BlueprintReadWrite)
	float MaxCrouchAcceleration;

	UPROPERTY(Category = "Character Movement: Acceleration", EditAnywhere, BlueprintReadWrite)
	float MaxWalkAcceleration;

	UPROPERTY(Category = "Character Movement: Acceleration", EditAnywhere, BlueprintReadWrite)
	float MaxTrotAcceleration;

	UPROPERTY(Category = "Character Movement: Acceleration", EditAnywhere, BlueprintReadWrite)
	float MaxRunAcceleration;

	/**
	 * When just holding acceleration in water
	 * MaxSwimSpeed is used while sprinting and swimming
	 */
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float MaxSwimSlowSpeed;

	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float MaxSwimTrotSpeed = 200.0f;

	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float MaxSwimAcceleration;

	//The amount of speed over the max speed that is required to take damage on impact
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float MaxSpeedThresholdImpact = 100.0f;

	UPROPERTY(Category = "Character Movement: Flying|Speeds", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxFlySlowSpeed;

	UPROPERTY(Category = "Character Movement: Flying|Speeds", EditAnywhere, BlueprintReadWrite)
	float MaxFlyDescentSpeed = 250.0f;

	UPROPERTY(Category = "Character Movement: Flying|Multipliers", EditAnywhere, BlueprintReadWrite)
	float DescentGravityMultiplier = 0.1f;

	//Increases friction on out of stamina, stops the playing from gaining a lot of speed while out of stamina.
	UPROPERTY(Category = "Character Movement: Flying|Multipliers", EditAnywhere, BlueprintReadWrite)
	float OutOfStamFrictionMultiplier = 4.1f;

	UPROPERTY(Category = "Character Movement: Flying|Multipliers", EditAnywhere, BlueprintReadWrite)
	float PrecisionFlyingPitchMultiplier = 0.2f;

	//Multiplys the max speed by this multiplier. So if multiplier is 0.5, then the spacebar will make the dino fly vertically at half of max speed.
	UPROPERTY(Category = "Character Movement: Flying|Multipliers", EditAnywhere, BlueprintReadWrite)
	float FlyUpwardsSpeedMultiplier = 0.5f;

	UPROPERTY(Category = "Character Movement: Flying|Acceleration", EditAnywhere, BlueprintReadWrite)
	float MaxFlyAcceleration;

	UPROPERTY(Category = "Character Movement: Flying|Acceleration", EditAnywhere, BlueprintReadWrite)
	float MaxDiveAcceleration = 180.0f;

	//Max possible speed from diving.
	UPROPERTY(Category = "Character Movement: Flying|Speeds", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxNosedivingSpeed = 6500.0f;

	UPROPERTY(Category = "Character Movement: Flying|Friction", EditAnywhere, BlueprintReadWrite)
	float FlyFriction = 0.05f;

	UPROPERTY(Category = "Character Movement: Flying|Friction", EditAnywhere, BlueprintReadWrite)
	float FlyPreciseFriction = 0.5f;

	//How fast the dino levels out when gliding
	UPROPERTY(Category = "Character Movement: Flying|Friction", EditAnywhere, BlueprintReadWrite)
	float VerticalGlidingFriction = 0.5f;

	UPROPERTY(Category = "Character Movement: Flying|Speeds", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float MaxPreciseFlySpeed = 250.0f;

	//The amount of speed you need to start flying
	UPROPERTY(Category = "Character Movement: Flying", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float StartFlyingMomentumThreshold;

	//The amount of speed you need to be below to start falling.
	UPROPERTY(Category = "Character Movement: Flying", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", UIMin = "0", ForceUnits = "cm/s"))
	float StartFallingSpeedThreshold = 300.0f;

	//The force that will be applied upwards when a dino is flying and in a hot air column
	UPROPERTY(Category = "Character Movement: Flying", EditAnywhere, BlueprintReadWrite)
	float FlyingHotAirColumnForce = 1200;

	//The exponent used for easing the maxspeed lerp. 0.0f = Linear. The higher the exponent, the more the player must look down to speed up.
	UPROPERTY(Category = "Character Movement: Flying", EditAnywhere, BlueprintReadWrite)
	float NosedivingEaseExponent = 2.3f;

	//The speed at which the dino will follow the camera's direction. E.g allos you to dive then pull up when lookign upwards. Setting it to 0 means no follow through
	UPROPERTY(Category = "Character Movement: Flying", EditAnywhere, BlueprintReadWrite)
	float MomentumFollowThroughSpeed = 50.0f;

	//Multiplys the current roll angle by this multiplier at high speeds, allows for more exagerated rolling at high speeds.
	UPROPERTY(Category = "Character Movement: Flying|High Speed Controls|Animation", EditAnywhere, BlueprintReadWrite)
	float SpeedAngleMultiplier = 2.5f;

	//The highest angle that the player can go up while traveling at high speeds. Increasing this means the player rises or falls quicker at high speeds
	UPROPERTY(Category = "Character Movement: Flying|High Speed Controls", EditAnywhere, BlueprintReadWrite)
	float SpeedMaxPitch = 20.0f;

	//The angle at which the diving animation will play
	UPROPERTY(Category = "Character Movement: Flying|Animation", EditAnywhere, BlueprintReadWrite)
	float AngleBeginDiving = 30.0f;

	//The angle at which the dino will turn when the camera is looking upwards
	UPROPERTY(Category = "Character Movement: Flying|Animation", EditAnywhere, BlueprintReadWrite)
	float AngleFlyUpwards = 47.5f;

	UPROPERTY(Category = "Character Movement: Flying|Animation|Smooth Turning", EditAnywhere, BlueprintReadWrite)
	bool UseSmoothFlyingMovement = false;

	UPROPERTY(Category = "Character Movement: Flying|Animation|Smooth Turning", EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "90", UIMin = "0", UIMax = "90"))
	float FlyingRollMaxAngle = 33.0f;

	//Multiplys the current roll angle by this multiplier, allows for more exagerated rolling.
	UPROPERTY(Category = "Character Movement: Flying|Animation|Smooth Turning", EditAnywhere, BlueprintReadWrite)
	float AngleMultiplier = 1.0f;

	//X = Pitch, Y = Roll, Z = Yaw
	UPROPERTY(Category = "Character Movement: Flying|Animation|Smooth Turning", EditAnywhere, BlueprintReadWrite)
	FVector FlyingSmoothTurningSpeed = FVector(8.0f, 2.0f, 2.25f);

	//Multiplied the max speed by this multiplier if carrying an object
	UPROPERTY(Category = "Character Movement: Flying|Multipliers", EditAnywhere, BlueprintReadWrite)
	float FlyCarrySpeedMultiplier = 0.9f;

	// Used in Precise Movement Only
	UPROPERTY(Category = "Character Movement: Precise", EditAnywhere, BlueprintReadWrite)
	float MaxWalkReverseSpeed;

	// Used in Precise Movement Only
	UPROPERTY(Category = "Character Movement: Precise", EditAnywhere, BlueprintReadWrite)
	float MaxWalkStrafeSpeed;

	// Used in Precise Movement Only
	UPROPERTY(Category = "Character Movement: Precise", EditAnywhere, BlueprintReadWrite)
	float MaxCrouchReverseSpeed;

	/*
	The delay between being in precise movement modeand being in normal movement mode when going backwards out of the water as an aquatic.
	If 0.0 then the delay is disabled.
	*/
	UPROPERTY(Category = "Character Movement: Precise", EditAnywhere, BlueprintReadWrite)
	float TransitionPreciseMovementTime = 0.0f;

	//If the player is an aquatic dino and is moving backwards out of the water onto land
	bool IsTransitioningOutOfWaterBackwards();
	void CancelBackwardsTransition();

	// If the player precise moved from swimming to falling, record this and use it to stop precise movement if they then go to land (Poncho)
	bool bPreciseMoveSwimmingToFalling = false;

	UPROPERTY(Category = "Character Movement", EditAnywhere, BlueprintReadWrite)
	float WalkMaxTurnRate;
	UPROPERTY(Category = "Character Movement", EditAnywhere, BlueprintReadWrite)
	float TrotMaxTurnRate;
	UPROPERTY(Category = "Character Movement", EditAnywhere, BlueprintReadWrite)
	float RunMaxTurnRate;

	// Used in Precise Movement Only
	UPROPERTY(Category = "Character Movement: Precise", EditAnywhere, BlueprintReadWrite)
	float MaxCrouchStrafeSpeed;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator MinimumRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator CrouchSpeedRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator WalkSpeedRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator TrotSpeedRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator RunSpeedRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator SwimSpeedRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator FlySpeedRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator TurnInPlaceStandingRotationRate;

	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator TurnInPlaceCrouchedRotationRate;

	// Rotation Rate when Precise Movement is active and standing
	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator PreciseMovementStandRotationRate;

	// Rotation Rate when Precise Movement is active and crouched
	UPROPERTY(Category = "Character Movement (Rotation Settings)", EditAnywhere, BlueprintReadWrite)
	FRotator PreciseMovementCrouchedRotationRate;

	/** FOR AQUATIC DINOSAURS ONLY: Water buoyancy. A ratio (1.0 = neutral buoyancy, 0.0 = no buoyancy) */
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float DivingBuoyancy = 0.8f;

	/** FOR AQUATIC DINOSAURS ONLY: Buoyancy if trying to rise to surface and is Aquatic (Should probably be less than at surface buoyancy) */
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float RisingBuoyancy = 1.2f;

	/** FOR AQUATIC DINOSAURS ONLY: Neutral Buoyancy if no Z Input Detected and is not at surface (Typically keep this the same thing as drowning buoyancy) */
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float UnderwaterNeutralBuoyancy = 0.95f;

	/** FOR AQUATIC AND NON-AQUATIC DINOSAURS: Buoyancy if has stamina or is aquatic and at water surface */
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float SurfaceBuoyancy = 1.5f;

	/** FOR NON-AQUATIC DINOSAURS ONLY: Buoyancy if out of stamina */
	UPROPERTY(Category = "Character Movement: Swimming", EditAnywhere, BlueprintReadWrite)
	float DrowningBuoyancy = 0.95f;
#pragma endregion 

#pragma region MovementModeFunctions
	// Activate Abilities / Change Movement Flags
	void SetSprinting(bool bSprinting);
	void SetTrotting(bool bTrotting);
	void SetLimping(bool bLimping);
	void SetStopped(bool bStop);
	void SetPreciseMovement(bool bPreciseMove);

	// Stateful Checks
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool IsSprinting() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool IsTrotting() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool IsLimping() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool IsNotMoving() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool IsTurningInPlace() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool ShouldUsePreciseMovement() const;

#pragma endregion

	/**
	 * Only used for precise movement mode
	 * Also updates TurnInPlaceTargetYaw
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	bool ShouldUseControllerDesiredRotation(float NeededAmount = 10.0f) const;

	/**
	 * Only used for precise movement mode, only called from ShouldUseControllerDesiredRotation
	 */
	void UpdateTurnInPlaceTargetYaw() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	EPreciseMovementDirection GetPreciseMovementDirection() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = CharacterMovement)
	virtual float GetDifferenceBetweenPawnAndCameraYaw() const;

	virtual float GetMaxSpeedSlopeModifier() const;

	void DisableAdditionalCapsules(bool bPermanentlyIgnoreAllChannels);

	void EnableAdditionalCapsules();

	const TArray<UCapsuleComponent*>& GetAdditionalCapsules();

	void SetCollisionResponseToDefault();

	bool CanTransitionToState(ECapsuleState InState) const;

	/**
	 * Pushes the character away from the base it's currently standing on
	 * Used to prevent dinosaur's stacking on top of each other
	 * This is used instead of @JumpOff because @JumpOff is only called once, when the invalid base is first detected
	 */
	void PushOffBase();

	bool IsStandingOnCharacter() const;

	/**
	 * Strength of the velocity applied when @PushOffBase slides the character
	 */
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category = "Character Movement: Jumping / Falling")
	float SlideOffBaseVelocity = 300.f;

	FRotator CalculatedRotator = FRotator();
	float RotationMultiplier = 1.f;

#pragma region CharacterMovementComponentOverrides
public:
	virtual void SetMovementMode(EMovementMode NewMovementMode, uint8 NewCustomMode = 0) override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxJumpHeight() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	virtual float GetGravityZ() const override;
	virtual FRotator GetDeltaRotation(float DeltaTime) const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void JumpOff(AActor* MovementBaseActor) override;
	virtual bool CanAttemptJump() const override;
	virtual bool DoJump(bool bReplayingMoves);
	virtual void UnCrouch(bool bClientSimulation /* = false */) override;
	virtual void PhysicsVolumeChanged(class APhysicsVolume* NewVolume) override;
protected:
	virtual FRotator ComputeOrientToMovementRotation(const FRotator& CurrentRotation, float DeltaTime, FRotator& DeltaRotation) const override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	/** Slows towards stop. */
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;
	virtual void ApplyLaunchVelocityBraking(float DeltaTime, float Friction);

	bool bHasPreLaunchVelocity = false;
	FVector PreLaunchVelocity = FVector::ZeroVector;
	void ApplyLaunchVelocity();

public:
	virtual void Launch(FVector const& LaunchVel) override;

#pragma endregion

	float ClientYawSentToServer;

protected:
	virtual void BeginPlay() override;

#pragma region VelocityModeCalculations
	void CalcVelocityRootMotionSource(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);
	void CalcVelocityNormal(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);
	void CalcVelocityPrecise(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);
	void CalcVelocityStunned(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);
	void CalcVelocitySwimming(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);
	void CalcVelocityFlying(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration);
#pragma endregion

	/**
	 * Sets bOrientRotationToMovement and bUseControllerDesiredRotation
	 * Updated in CalcVelocity
	 */
	virtual void UpdateRotationMethod();
	virtual void UpdateBuoyancy(const AIDinosaurCharacter& DinoOwner);

	virtual void ControlledCharacterMove(const FVector& InputVector, float DeltaSeconds) override;
	virtual void PerformMovement(float DeltaTime) override;

	virtual void PhysFlying(float deltaTime, int32 Iterations) override;
	virtual void PhysSwimming(float deltaTime, int32 Iterations) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;

	virtual void PhysFlyingApproximate(float deltaTime, int32 Iterations);
	virtual void PhysSwimmingApproximate(float deltaTime, int32 Iterations);
	virtual void PhysWalkingApproximate(float deltaTime, int32 Iterations);
	
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f, const FVector& MoveDelta = FVector::ZeroVector) override;

	friend class UPOTGameplayAbility;
#pragma region PredictionData
public:
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual class FNetworkPredictionData_Server* GetPredictionData_Server() const override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;
	virtual void ServerMove_HandleMoveData(const FCharacterNetworkMoveDataContainer& MoveDataContainer) override;
	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;
	virtual void ServerMoveHandleClientErrorWithRotation(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FPOTCharacterNetworkMoveData& MoveData);
	virtual bool ServerCheckClientErrorWithRotation(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FRotator& ClientWorldRotation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode, const FVector& LaunchVelocity);
	bool ServerAcceptClientAuthoritiveMove(float DeltaTime, const FVector& ClientLoc, const FRotator& ClientRotation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode, bool bFromAuthTolerance);
	virtual void ServerMoveHandleClientErrorApproximate(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FPOTCharacterNetworkMoveData& MoveData);

	virtual class FNetworkPredictionData_Client_ICharacter* GetPredictionData_Client_ICharacter() const;
	virtual class FNetworkPredictionData_Server_ICharacter* GetPredictionData_Server_ICharacter() const;

#pragma endregion
public:
	FRotator RotateToFrom(const FRotator ToRotation, const FRotator FromRotation, const float AngleTolerance, const float DeltaTime, const bool UseSmoothTurn = false) const;
	float GetDistanceBetween(const FRotator& Rot1, const FRotator& Rot2) const;

#pragma region MultiCapsuleMovement
public:
	//If this is true then the component will look for all non-root capsule components on this character and register them as additional movement capsules.
	//If it's false, additional capsules need to be registered manually with RegisterCapsulesAsAdditionalComponent
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	bool bAutoGatherNonRootCapsules;

	UFUNCTION(BlueprintCallable)
	void RegisterCapsulesAsAdditionalComponent(TArray<UCapsuleComponent*> InCapsules);

	void GatherAdditionalUpdatedComponents();

	UFUNCTION(BlueprintPure)
	FORCEINLINE bool IsUsingMultiCapsuleCollision() const
	{
		return bUseMultiCapsuleCollision;
	}

	void TimeDilationManipulationDetected();

	bool bTimeDilationReportSent = false;

protected:
	UPROPERTY(Config)
	bool bUseMultiCapsuleCollision;

	//0 - Disabled, 1 - Async only blocking hits, 2 - Full async
	UPROPERTY(Config)
	int32 AsyncCollisionMode;

	//0 - Disabled, 1 - Only non-root capsules, 2 - All capsules
	UPROPERTY(Config)
	int32 IncrementalAsyncUpdateMode;

	UPROPERTY(Config)
	bool bTestTraceIfNoBlocking;

	UPROPERTY(Config)
	bool bSkipUpdateChildTransforms;

	// Accumulated speedhack detections. Will decrease over time
	int32 DetectedSpeedhacks = 0;

	FTimerHandle TimerHandle_SpeedhackReduction;

	void ReduceSpeedhackDetection();

	//Just for debug purposes
	UPROPERTY(AdvancedDisplay, EditDefaultsOnly)
	bool bForceAsyncMCC;

	UPROPERTY(Transient)
	TMap<const class UPrimitiveComponent*, FHitResult> CachedHits;

private:
	int32 IncrementalUpdateIndex;

	bool bIsDescending = false;
	
	float elapsedTransitionPreciseMovementTime = 0.0f;
	bool bTransitioningPreciseMovement = false;

public:
	virtual void PhysicsRotation(float DeltaTime) override;

protected:
	virtual bool ResolvePenetrationImpl(const FVector& Adjustment, const FHitResult& Hit, const FQuat& Rotation) override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;
	virtual bool MoveUpdatedComponentImpl(const FVector& Delta, const FQuat& Rotation, bool bSweep, FHitResult* OutHit = NULL, ETeleportType Teleport = ETeleportType::None) override;
	
	void OnHitWall(const FHitResult& HitResult);

	bool ShouldIgnoreHitResult(const UWorld* InWorld, const UPrimitiveComponent* TestComponent, FHitResult const& TestHit, FVector const& MovementDirDenormalized, const AActor* MovingActor, EMoveComponentFlags MoveFlags);

	void ProcessCompletedAsyncSweepMulti(const FQueuedMultiCapsuleTrace& CompletedTrace);

	bool IsSurfacing(FHitResult HitResult, FVector TraceStart) const;

	void CalculateSweepData(class UPrimitiveComponent* ComponentToSimulate, const FVector& NewDelta, const FQuat& NewRotation, FVector& TraceStart, FVector& TraceEnd, FQuat& NewComponentQuat, FComponentQueryParams& QueryParams, FCollisionResponseParams& ResponseParams);

	friend class FMultiCapsuleTraceWorker;

	UPROPERTY(Transient, BlueprintReadOnly)
	TArray<UCapsuleComponent*> AdditionalCapsules;

	UPROPERTY(Transient)
	TMap<UCapsuleComponent*, FStoredCollisionInfo> AdditionalCapsuleDefaultCollisionProfiles;

	UPROPERTY(Transient, BlueprintReadOnly)
	TArray<class UStateAdjustableCapsuleComponent*> AdditionalAdjustableCapsules;

private:
	UPROPERTY(Transient)
	const class UPrimitiveComponent* LastHitComponent;

	bool MoveAdditionalCapsules(const FVector& Delta, const FQuat& NewRotation, FHitResult* OutHit);

	// A copy of UPrimitiveComponent::MoveComponentImpl() but without the actual movement
	// just calculating the delta for the additional capsule, sweeping that and checking if it would be penetrating
	bool SimulateMoveCapsuleComponent(UCapsuleComponent* ComponentToSimulate, const class USceneComponent* CharacterRootComponent, const FVector& NewDelta, const FQuat& NewRotation, FHitResult* OutHit = nullptr, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags);
private:
	
	bool AdditionalCapsuleComponentSweepMulti(TArray<struct FHitResult>& OutHits, class UPrimitiveComponent* PrimComp, const FVector& Start, const FVector& End, const FQuat& Rot, const FComponentQueryParams& Params, const FCollisionResponseParams& ResponseParam) const;

	bool ProcessHits(bool bHadBlockingHits, TArray<FHitResult>& Hits, const FVector& TraceStart, const FVector& TraceEnd, const FVector& NewDelta, const UPrimitiveComponent* ComponentToSimulate, EMoveComponentFlags MoveFlags, FHitResult* OutHit);

	bool ShouldUseAsyncMultiCapsuleCollision(const UPrimitiveComponent* PComp) const;
#pragma endregion

	bool bIsDoingApproxMove = false;
	FVector ApproxMoveOldLocation = FVector::ZeroVector;
	FRotator ApproxMoveOldRotator = FRotator::ZeroRotator;
};

/** FSavedMove_Character represents a saved move on the client that has been sent to the server and might need to be played back. */
class PATHOFTITANS_API FSavedMove_ICharacter: public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	FSavedMove_ICharacter() : Super() 
	{
		AccelMagThreshold = 0.33f;
		AccelDotThreshold = 0.3f;
		AccelDotThresholdCombine = 0.996f; // approx 5 degrees.
		MaxSpeedThresholdCombine = 3.33f;
	};
	virtual ~FSavedMove_ICharacter() {};

	// New Movement Flags
	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToTrot : 1;
	uint8 bSavedWantsToLimp : 1;
	uint8 bSavedWantsToStop : 1;
	uint8 bSavedWantsToPreciseMove : 1;
	uint8 bSavedWantsSituationalAuthority : 1;
	uint8 bSavedPreciseRotateToDirection : 1;
	FVector SavedLaunchVelocity = FVector::ZeroVector;
	FVector StartLaunchVelocity = FVector::ZeroVector;

	/** Clear saved move properties, so it can be re-used. */
	virtual void Clear() override;

	/** Called to set up this saved move (when initially created) to make a predictive correction. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;

	/** Set the properties describing the position, etc. of the moved pawn at the start of the move. */
	virtual void SetInitialPosition(ACharacter* C) override;

	/** @Return true if this move is an "important" move that should be sent again if not acked by the server */
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;

	/** Set the properties describing the final position, etc. of the moved pawn. */
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;

	/** @Return true if this move can be combined with NewMove for replication without changing any behavior */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InPawn, float MaxDelta) const override;

	/**Combine this move with an older moveand update relevant state. */ 
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;

	/** Called before ClientUpdatePosition uses this SavedMove to make a predictive correction	 */
	virtual void PrepMoveFor(ACharacter* C) override;

	/** @returns a byte containing encoded special movement information (jumping, crouching, etc.)	 */
	virtual uint8 GetCompressedFlags() const override;

	// Bit masks used by GetCompressedFlags() to encode movement information.
	enum ICharacterCompressedFlags
	{
		FLAG_PreciseMove = FLAG_Reserved_1,
		FLAG_Stop = FLAG_Reserved_2,
		FLAG_Sprinting = FLAG_Custom_0,
		FLAG_Trotting = FLAG_Custom_1,
		FLAG_Limping = FLAG_Custom_2,
		FLAG_Resting = FLAG_Custom_3
	};
};
class PATHOFTITANS_API FNetworkPredictionData_Client_ICharacter : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_ICharacter(const UCharacterMovementComponent& ClientMovement) : Super(ClientMovement) 
	{
		MaxSavedMoveCount = 96 * 4;
	};
	virtual ~FNetworkPredictionData_Client_ICharacter() {};

	/** Allocate a new saved move. Subclasses should override this if they want to use a custom move class. */
	virtual FSavedMovePtr AllocateNewMove() override;
};

class PATHOFTITANS_API FNetworkPredictionData_Server_ICharacter : public FNetworkPredictionData_Server_Character
{
public:
	typedef FNetworkPredictionData_Server_Character Super;
	
	FNetworkPredictionData_Server_ICharacter(const UCharacterMovementComponent& ServerMovement) : Super(ServerMovement)
	{
	}

	virtual ~FNetworkPredictionData_Server_ICharacter() {};

	FVector PendingLaunchVelocityAdjustment = FVector::ZeroVector;

	// If true, the client will always be adjusted in the next server move.
	bool bForceNextClientAdjustment = false;
};
