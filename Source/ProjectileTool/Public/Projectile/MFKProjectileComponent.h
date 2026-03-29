// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MFKProjectileComponent.generated.h"

class UPrimitiveComponent;
class UWorld;

/** Path mode used during free-flight movement. */
UENUM(BlueprintType)
enum class EProjectilePathMode : uint8
{
	None    UMETA(DisplayName = "None (Straight)"),
	Sine    UMETA(DisplayName = "Sine (Wave)"),
	Zigzag  UMETA(DisplayName = "Zigzag"),
	Spiral  UMETA(DisplayName = "Spiral"),
	Vertical360 UMETA(DisplayName = "Vertical 360")
};

UENUM(BlueprintType)
enum class EVertical360EaseType : uint8
{
	SmoothStep     UMETA(DisplayName = "SmoothStep (u*u*(3-2u))"),
	EaseInOutCubic UMETA(DisplayName = "EaseInOutCubic")
};

/** Tick update frequency preset. */
UENUM(BlueprintType)
enum class EProjectileTickRatePreset : uint8
{
	/** 60 Hz - smooth on 60 FPS displays, performance friendly. */
	Low     UMETA(DisplayName = "Low (60 Hz)"),
	/** 120 Hz - smoother on high refresh rate displays. */
	Medium  UMETA(DisplayName = "Medium (120 Hz)"),
	/** Every frame - highest precision. */
	Max     UMETA(DisplayName = "Max (Every Frame)")
};

/** Same parameters as Unreal OnComponentHit (HitComponent = projectile root, OtherActor/OtherComp = impacted target). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FOnProjectileHitSignature, UPrimitiveComponent*, HitComponent, AActor*, OtherActor, UPrimitiveComponent*, OtherComp, FVector, NormalImpulse, FHitResult, Hit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProjectileLifetimeExpiredSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnProjectileMaxRangeReachedSignature);


UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PROJECTILETOOL_API UMFKProjectileComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMFKProjectileComponent();

	// --- Movement ---
	/** Projectile speed (units/second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement", meta = (ClampMin = "0"))
	float Speed = 1000.f;

	/** Use owner's root collision during movement (sweep). Root must be a UPrimitiveComponent; projectile stops on blocking hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement")
	bool bUseSweep = true;

	/** Add Owner/Instigator to root primitive MoveIgnoreActors to avoid immediate self-hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement")
	bool bIgnoreOwnerWhenMoving = true;

	// --- Path (free-flight shape) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path")
	EProjectilePathMode PathMode = EProjectilePathMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Sine", EditConditionHides, ClampMin = "0"))
	float SineAmplitude = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Sine", EditConditionHides, ClampMin = "0"))
	float SineFrequency = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Zigzag", EditConditionHides, ClampMin = "0"))
	float ZigzagAmplitude = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Zigzag", EditConditionHides, ClampMin = "0"))
	float ZigzagFrequency = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Spiral", EditConditionHides, ClampMin = "0"))
	float SpiralRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Spiral", EditConditionHides, ClampMin = "0"))
	float SpiralFrequency = 1.f;

	/** Vertical 360: Duration of one 360-degree flip (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", EditConditionHides, ClampMin = "0.01"))
	float Vertical360DurationSec = 0.5f;

	/** Vertical 360:
	 *  - Delays the first flip by Vertical360IntervalSec.
	 *  - After each flip, waits the same duration before starting another (cooldown).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", EditConditionHides, ClampMin = "0.01"))
	float Vertical360IntervalSec = 1.0f;

	/** Total Vertical 360 flip count. 0 = infinite, 1 = single flip only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", EditConditionHides, ClampMin = "0"))
	int32 Vertical360FlipCount = 0;

	/** Vertical 360 easing type (reduces abrupt start/stop feel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", EditConditionHides))
	EVertical360EaseType Vertical360EaseType = EVertical360EaseType::SmoothStep;

	/** Slightly wobble flip axis during Vertical 360 (less robotic motion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", EditConditionHides))
	bool bVertical360WobbleEnabled = true;

	/** Wobble amplitude (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled", EditConditionHides, ClampMin = "0"))
	float Vertical360WobbleAmplitudeDeg = 10.f;

	/** Number of wobble cycles during one flip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled", EditConditionHides, ClampMin = "0.01"))
	float Vertical360WobbleCyclesPerTurn = 1.f;

	/** Randomize wobble per projectile (phase + slight amplitude variation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled", EditConditionHides))
	bool bVertical360RandomizeWobblePerProjectile = true;

	/** Randomize wobble per flip cycle as well (each cycle feels different). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled", EditConditionHides))
	bool bVertical360RandomizeWobblePerFlip = true;

	/** Update actor rotation toward movement direction every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path")
	bool bOrientToVelocity = false;

	// --- Global Micro Wobble (applies to all path modes) ---
	/** Enables subtle global wobble on top of all path modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble")
	bool bEnableMicroWobble = false;

	/** Maximum micro wobble angular amplitude (degrees). Keep small values for realism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", EditConditionHides, ClampMin = "0"))
	float MicroWobbleAmplitudeDeg = 1.0f;

	/** Micro wobble oscillation frequency (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", EditConditionHides, ClampMin = "0"))
	float MicroWobbleFrequencyHz = 3.0f;

	/** Blend-in time to avoid sudden wobble at spawn (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", EditConditionHides, ClampMin = "0"))
	float MicroWobbleBlendInSec = 0.2f;

	/** Randomize micro wobble phase per projectile so shots do not look identical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", EditConditionHides))
	bool bRandomizeMicroWobblePerProjectile = true;

	/** When homing is enabled, amplitude is multiplied by this (e.g. 0.15 = 15% of Micro Wobble Amplitude). 1 = no reduction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble && bEnableHoming", EditConditionHides, ClampMin = "0", ClampMax = "1"))
	float MicroWobbleScaleWhenHoming = 0.15f;

	// --- Homing (applies to all path modes) ---
	/** Enable homing toward target (actor or world location). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing")
	bool bEnableHoming = false;

	/** Target actor to home toward. Can be set in editor or at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides))
	TObjectPtr<AActor> HomingTargetActor = nullptr;

	/** If set, component finds target by this tag in BeginPlay (avoids Level BP reference issues). Overrides HomingTargetActor when resolved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides))
	FName HomingTargetTag;

	/**
	 * Maximum turn rate toward the aim point (deg/s). Actual rate per shot is HomingTurnRateDegPerSec * HomingTurnRateEffectiveScale (scale rolled at spawn from Homing Full Turn Rate Chance).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0"))
	float HomingTurnRateDegPerSec = 180.f;

	/** Delay before homing starts (seconds). During delay: straight line along spawn forward, no micro-wobble (Path None). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0"))
	float HomingAcquisitionDelaySec = 0.f;

	/**
	 * Path modes + homing: when distance to target is above this (uu), pattern runs in a target-aimed frame.
	 * When at or below this distance, path wobble stops and flight uses the current heading from the prior frame, steered toward the aim point by Homing Turn Rate (same as None homing), not an instant snap to target.
	 * 0 = never enter this terminal chase (pattern stays on until impact).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0"))
	float HomingPathTerminalDirectDistance = 300.f;

	/**
	 * 0–1: at spawn, probability this projectile uses the full Homing Turn Rate (scale = 1).
	 * Otherwise Homing Turn Rate is multiplied by Homing Reduced Turn Rate Scale for the whole flight (weaker tracking, still homes).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0", ClampMax = "1"))
	float HomingFullTurnRateChance = 1.f;

	/**
	 * When the full-turn-rate roll fails, turn rate scale is picked uniformly in [Min, Max] (0–1 each).
	 * If Min > Max in the editor, they are swapped at runtime.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0", ClampMax = "1"))
	float HomingReducedTurnRateScaleMin = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0", ClampMax = "1"))
	float HomingReducedTurnRateScaleMax = 0.45f;

	/**
	 * Radius (uu) of the error circle on the plane perpendicular to the projectile→target line at lock. One random point on that circle becomes the ghost aim.
	 * 0 = perfect lock on true target. Larger = further possible miss (still depends on turn rate, speed, motion).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides, ClampMin = "0"))
	float HomingAccuracyRadius = 0.f;

	/** Draw the accuracy circle (cyan) and chosen ghost point (yellow) in the editor/game view — for tuning only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides))
	bool bDrawDebugHomingGhost = false;

	/**
	 * When true, if the target lies behind the current flight direction (miss / overshoot), homing stops for this projectile.
	 * Movement coasts along the integrated velocity direction (+ micro wobble) instead of turning back toward the target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides))
	bool bHomingAbortWhenPastTarget = true;

	/**
	 * Abort when Dot(flightDir, toTarget) <= this (flightDir = last-frame velocity from HomingIntegratedForward; toTarget toward target).
	 * 0 = not in front hemisphere (side pass at 90° or behind). Negative (e.g. -0.15) = require a clearer overshoot. Positive = abort earlier while target is still slightly ahead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming && bHomingAbortWhenPastTarget", EditConditionHides))
	float HomingPastTargetDotThreshold = 0.f;

	/** When true, logs homing state to Output Log (for debugging). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Homing", meta = (EditCondition = "bEnableHoming", EditConditionHides))
	bool bHomingDebugLog = false;

	// --- Performance ---
	/** Tick update frequency: Low = 60 Hz, Medium = 120 Hz, Max = every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Performance")
	EProjectileTickRatePreset TickRatePreset = EProjectileTickRatePreset::Max;

	// --- Lifetime & Range ---
	/** Maximum lifetime (seconds). 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Lifetime", meta = (ClampMin = "0"))
	float Lifetime = 0.f;

	/** Maximum travel range (units). 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Lifetime", meta = (ClampMin = "0"))
	float MaxRange = 0.f;

	// --- Events (Blueprint assignable) ---
	/** Triggered once on impact (duplicate hit events are filtered). */
	UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
	FOnProjectileHitSignature OnProjectileHit;

	/** Triggered when lifetime expires. */
	UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
	FOnProjectileLifetimeExpiredSignature OnProjectileLifetimeExpired;

	/** Triggered when max range is reached. */
	UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
	FOnProjectileMaxRangeReachedSignature OnProjectileMaxRangeReached;

	/** Set homing target to an actor. Clears world location target. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Projectile|Homing")
	void SetHomingTarget(AActor* InTarget);

	/** Set homing target to a world location. Clears actor target. */
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Projectile|Homing")
	void SetHomingTargetWorldLocation(const FVector& WorldLocation);

	/** Clear homing target (actor and world location). */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Homing")
	void ClearHomingTarget();

	/** Find first actor with given tag in level and set as homing target. Use when level references fail in PIE. */
	UFUNCTION(BlueprintCallable, Category = "Projectile|Homing", meta = (DisplayName = "Set Homing Target By Tag"))
	void SetHomingTargetByTag(FName Tag);

protected:
	virtual void BeginPlay() override;

	/** Forward direction captured at spawn; movement uses this direction. */
	UPROPERTY()
	FVector CachedMovementDirection;

	/** Right/up basis from CachedMovementDirection (computed once in BeginPlay; same as GetPerpendicularAxes). */
	UPROPERTY()
	FVector CachedRight;

	UPROPERTY()
	FVector CachedUp;

	/** Spawn location (used for max range calculation). */
	UPROPERTY()
	FVector StartLocation;

	/** Spawn world time (used for lifetime calculation). */
	UPROPERTY()
	float SpawnTime = 0.f;

	/** Movement is stopped after impact or when lifetime/range limit is reached. */
	UPROPERTY()
	bool bMovementStopped = false;

	/** Ensures OnProjectileHit is broadcast only once. */
	UPROPERTY()
	bool bHasBroadcastHit = false;

	/** Per-projectile random phase for Vertical360 wobble (radians). */
	UPROPERTY()
	float Vertical360WobbleRandomPhaseRad = 0.f;

	/** Per-projectile random amplitude scale for Vertical360 wobble. */
	UPROPERTY()
	float Vertical360WobbleRandomAmplitudeScale = 1.f;

	/** Per-projectile random phase for global micro wobble (radians). */
	UPROPERTY()
	float MicroWobbleRandomPhaseA = 0.f;

	/** Secondary random phase for global micro wobble (radians). */
	UPROPERTY()
	float MicroWobbleRandomPhaseB = 0.f;

	/** Vertical 360: cached from duration/interval at BeginPlay (avoids per-tick adds/divs). */
	UPROPERTY()
	float Vertical360CachedFirstDelay = 0.f;

	UPROPERTY()
	float Vertical360CachedCycleLength = 1.f;

	UPROPERTY()
	float Vertical360CachedInvDuration = 1.f;

	/** Duration of one flip (seconds), cached at BeginPlay. */
	UPROPERTY()
	float Vertical360CachedDurationSec = 0.5f;

	/** True when Vertical360 interval/duration are valid (cached at BeginPlay). */
	UPROPERTY()
	bool bVertical360TimingValid = false;

	/** Path math constants (BeginPlay). */
	UPROPERTY()
	float CachedSineTwoPiFreq = 0.f;

	UPROPERTY()
	float CachedZigzagPeriod = 1.f;

	UPROPERTY()
	float CachedZigzagLateralCoeff = 0.f;

	UPROPERTY()
	float CachedSpiralTwoPiFreq = 0.f;

	UPROPERTY()
	float CachedMicroWobbleTwoPiFreq = 0.f;

	UPROPERTY()
	bool bHasLifetimeLimit = false;

	UPROPERTY()
	float CachedExpiryTime = 0.f;

	UPROPERTY()
	bool bHasMaxRangeLimit = false;

	UPROPERTY()
	float CachedMaxRangeSq = 0.f;

	UPROPERTY()
	TObjectPtr<AActor> CachedOwnerActor = nullptr;

	UPROPERTY()
	TObjectPtr<UWorld> CachedWorld = nullptr;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> CachedRootPrimitive = nullptr;

	/** When true, use CachedHomingWorldLocation instead of HomingTargetActor. */
	UPROPERTY()
	bool bUseWorldLocationForHoming = false;

	/** World location target set by SetHomingTargetWorldLocation. */
	UPROPERTY()
	FVector CachedHomingWorldLocation = FVector::ZeroVector;

	/** World-space offset from true target to ghost aim (lies on circle of radius HomingAccuracyRadius in lock plane). */
	UPROPERTY()
	FVector HomingGhostWorldOffset = FVector::ZeroVector;

	/** Orthonormal axes in the lock plane (perpendicular to LOS at acquisition) for circle + debug. */
	UPROPERTY()
	FVector HomingGhostBasisU = FVector::ZeroVector;

	UPROPERTY()
	FVector HomingGhostBasisV = FVector::ZeroVector;

	/** True after HomingGhostWorldOffset is chosen for this shot. */
	UPROPERTY()
	bool bHomingGhostAimInitialized = false;

	/** Set true when bHomingAbortWhenPastTarget detects an overshoot; homing steer disabled until respawn. */
	UPROPERTY()
	bool bHomingPastTargetAborted = false;

	/** Per-shot multiplier on HomingTurnRateDegPerSec (1 = full, or random in reduced min–max range); set in BeginPlay. */
	UPROPERTY()
	float HomingTurnRateEffectiveScale = 1.f;

	/** When homing + Path None: forward axis is last frame's movement (after homing), not spawn — so turns accumulate. */
	UPROPERTY()
	FVector HomingIntegratedForward = FVector::ForwardVector;

	/** Resets each BeginPlay; used by homing debug logs (avoids static persisting across PIE runs). */
	struct FHomingDebugOnce
	{
		bool bTickOwner = false;
		bool bTryGetEntry = false;
		bool bTryGetOkActor = false;
		bool bTryGetWorldMismatch = false;
		bool bTryGetWorldLoc = false;
		bool bTryGetFailNoTarget = false;
		bool bSkipEnable = false;
		bool bSkipNoTarget = false;
		bool bSkipAcquisition = false;
		bool bSkipDeltaSmall = false;
		bool bSkipToTargetZero = false;
		bool bSkipTurnRate = false;
		bool bSkipNewDirZero = false;
		bool bTickDeltaDiff = false;
		float LastOkLogTime = -999.f;
	};
	mutable FHomingDebugOnce HomingDbgOnce;

	void ApplyTickRatePreset();
	void UpdateVertical360Caches();
	void UpdatePathModeCaches();
	void UpdateLifetimeRangeCaches();
	void SetupMoveIgnoreActors();
	void BindToRootComponentHit();
	void GetPerpendicularAxes(FVector& OutRight, FVector& OutUp) const;
	static void GetPerpendicularAxesForDirection(const FVector& ForwardIn, FVector& OutRight, FVector& OutUp);
	FVector ComputeMovementDelta(float DeltaTime, float ElapsedTime, const FVector& ProjectileWorldLocation, const FVector& HomingDesiredDir, bool bHomingDesiredDirValid) const;
	FVector ApplyGlobalMicroWobbleToDelta(const FVector& InDelta, float ElapsedTime) const;

	FVector ApplyHomingToDelta(const FVector& Delta, const FVector& ProjectileLoc, float DeltaTime, float ElapsedTime, const FVector& HomingDesiredDir, bool bHomingDesiredDirValid) const;

	bool TryGetHomingTargetLocation(FVector& OutLocation) const;

	void DrawDebugHomingGhostIfNeeded() const;

	void ApplyOrientToVelocityIfNeeded(AActor* OwnerActor, const FVector& Delta) const;

	UFUNCTION()
	void OnRootComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
