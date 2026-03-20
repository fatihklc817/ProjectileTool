// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MFKProjectileComponent.generated.h"

class UPrimitiveComponent;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Sine", ClampMin = "0"))
	float SineAmplitude = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Sine", ClampMin = "0"))
	float SineFrequency = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Zigzag", ClampMin = "0"))
	float ZigzagAmplitude = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Zigzag", ClampMin = "0"))
	float ZigzagFrequency = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Spiral", ClampMin = "0"))
	float SpiralRadius = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Spiral", ClampMin = "0"))
	float SpiralFrequency = 1.f;

	/** Vertical 360: Duration of one 360-degree flip (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", ClampMin = "0.01"))
	float Vertical360DurationSec = 0.5f;

	/** Vertical 360:
	 *  - Delays the first flip by Vertical360IntervalSec.
	 *  - After each flip, waits the same duration before starting another (cooldown).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", ClampMin = "0.01"))
	float Vertical360IntervalSec = 1.0f;

	/** Total Vertical 360 flip count. 0 = infinite, 1 = single flip only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360", ClampMin = "0"))
	int32 Vertical360FlipCount = 0;

	/** Vertical 360 easing type (reduces abrupt start/stop feel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360"))
	EVertical360EaseType Vertical360EaseType = EVertical360EaseType::SmoothStep;

	/** Slightly wobble flip axis during Vertical 360 (less robotic motion). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360"))
	bool bVertical360WobbleEnabled = true;

	/** Wobble amplitude (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled", ClampMin = "0"))
	float Vertical360WobbleAmplitudeDeg = 10.f;

	/** Number of wobble cycles during one flip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled", ClampMin = "0.01"))
	float Vertical360WobbleCyclesPerTurn = 1.f;

	/** Randomize wobble per projectile (phase + slight amplitude variation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled"))
	bool bVertical360RandomizeWobblePerProjectile = true;

	/** Randomize wobble per flip cycle as well (each cycle feels different). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path", meta = (EditCondition = "PathMode == EProjectilePathMode::Vertical360 && bVertical360WobbleEnabled"))
	bool bVertical360RandomizeWobblePerFlip = true;

	/** Update actor rotation toward movement direction every frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Path")
	bool bOrientToVelocity = false;

	// --- Global Micro Wobble (applies to all path modes) ---
	/** Enables subtle global wobble on top of all path modes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble")
	bool bEnableMicroWobble = false;

	/** Maximum micro wobble angular amplitude (degrees). Keep small values for realism. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", ClampMin = "0"))
	float MicroWobbleAmplitudeDeg = 1.0f;

	/** Micro wobble oscillation frequency (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", ClampMin = "0"))
	float MicroWobbleFrequencyHz = 3.0f;

	/** Blend-in time to avoid sudden wobble at spawn (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble", ClampMin = "0"))
	float MicroWobbleBlendInSec = 0.2f;

	/** Randomize micro wobble phase per projectile so shots do not look identical. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|MicroWobble", meta = (EditCondition = "bEnableMicroWobble"))
	bool bRandomizeMicroWobblePerProjectile = true;

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

protected:
	virtual void BeginPlay() override;

	/** Forward direction captured at spawn; movement uses this direction. */
	UPROPERTY()
	FVector CachedMovementDirection;

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

	void ApplyTickRatePreset();
	void SetupMoveIgnoreActors();
	void BindToRootComponentHit();
	void GetPerpendicularAxes(FVector& OutRight, FVector& OutUp) const;
	FVector ComputeMovementDelta(float DeltaTime, float ElapsedTime) const;
	FVector ApplyGlobalMicroWobbleToDelta(const FVector& InDelta, float ElapsedTime) const;

	UFUNCTION()
	void OnRootComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
