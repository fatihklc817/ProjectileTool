// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectileTool/Public/Projectile/MFKProjectileComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"


UMFKProjectileComponent::UMFKProjectileComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}


void UMFKProjectileComponent::ApplyTickRatePreset()
{
	switch (TickRatePreset)
	{
	case EProjectileTickRatePreset::Low:
		PrimaryComponentTick.TickInterval = 1.f / 60.f;  // 60 Hz - 60 fps'te akici
		break;
	case EProjectileTickRatePreset::Medium:
		PrimaryComponentTick.TickInterval = 1.f / 120.f; // 120 Hz
		break;
	case EProjectileTickRatePreset::Max:
	default:
		PrimaryComponentTick.TickInterval = 0.f; // every frame
		break;
	}
}


void UMFKProjectileComponent::UpdateVertical360Caches()
{
	bVertical360TimingValid = (Vertical360IntervalSec > 0.f && Vertical360DurationSec > 0.f);
	Vertical360CachedFirstDelay = Vertical360IntervalSec;
	Vertical360CachedDurationSec = Vertical360DurationSec;
	Vertical360CachedCycleLength = Vertical360DurationSec + Vertical360IntervalSec;
	Vertical360CachedInvDuration = (Vertical360DurationSec > UE_KINDA_SMALL_NUMBER)
		? (1.f / Vertical360DurationSec)
		: 0.f;
}


void UMFKProjectileComponent::UpdatePathModeCaches()
{
	CachedSineTwoPiFreq = 2.f * UE_PI * SineFrequency;
	CachedZigzagPeriod = (ZigzagFrequency > 0.f) ? (1.f / ZigzagFrequency) : 1.f;
	CachedZigzagLateralCoeff = 4.f * ZigzagAmplitude * ZigzagFrequency;
	CachedSpiralTwoPiFreq = 2.f * UE_PI * SpiralFrequency;
	CachedMicroWobbleTwoPiFreq = 2.f * UE_PI * MicroWobbleFrequencyHz;
}


void UMFKProjectileComponent::UpdateLifetimeRangeCaches()
{
	bHasLifetimeLimit = (Lifetime > 0.f);
	CachedExpiryTime = SpawnTime + Lifetime;
	bHasMaxRangeLimit = (MaxRange > 0.f);
	CachedMaxRangeSq = MaxRange * MaxRange;
}


void UMFKProjectileComponent::BeginPlay()
{
	Super::BeginPlay();

	HomingDbgOnce = FHomingDebugOnce();

	ApplyTickRatePreset();



	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	CachedOwnerActor = OwnerActor;
	CachedWorld = World;

	if (OwnerActor)
	{
		CachedMovementDirection = OwnerActor->GetActorForwardVector();
		if (!CachedMovementDirection.Normalize())
		{
			CachedMovementDirection = FVector::ForwardVector;
		}
		StartLocation = OwnerActor->GetActorLocation();
		if (bUseSweep)
		{
			CachedRootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
		}
		else
		{
			CachedRootPrimitive = nullptr;
		}
	}
	else
	{
		CachedRootPrimitive = nullptr;
	}

	GetPerpendicularAxes(CachedRight, CachedUp);
	HomingIntegratedForward = CachedMovementDirection;
	if (!HomingIntegratedForward.Normalize())
	{
		HomingIntegratedForward = FVector::ForwardVector;
	}

	UpdateVertical360Caches();

	SpawnTime = World ? World->GetTimeSeconds() : 0.f;
	UpdateLifetimeRangeCaches();
	UpdatePathModeCaches();

	// Vertical360 wobble her mermide ayni görünmesin.
	if (bVertical360RandomizeWobblePerProjectile)
	{
		Vertical360WobbleRandomPhaseRad = FMath::FRandRange(0.f, 2.f * UE_PI);
		Vertical360WobbleRandomAmplitudeScale = FMath::FRandRange(0.85f, 1.15f);
	}
	else
	{
		Vertical360WobbleRandomPhaseRad = 0.f;
		Vertical360WobbleRandomAmplitudeScale = 1.f;
	}

	if (bEnableMicroWobble && bRandomizeMicroWobblePerProjectile)
	{
		MicroWobbleRandomPhaseA = FMath::FRandRange(0.f, 2.f * UE_PI);
		MicroWobbleRandomPhaseB = FMath::FRandRange(0.f, 2.f * UE_PI);
	}
	else
	{
		MicroWobbleRandomPhaseA = 0.f;
		MicroWobbleRandomPhaseB = 0.f;
	}

	bHomingPastTargetAborted = false;
	bHomingGhostAimInitialized = false;
	HomingGhostWorldOffset = FVector::ZeroVector;
	HomingGhostBasisU = FVector::ZeroVector;
	HomingGhostBasisV = FVector::ZeroVector;

	HomingTurnRateEffectiveScale = 1.f;
	if (bEnableHoming)
	{
		const float P = FMath::Clamp(HomingFullTurnRateChance, 0.f, 1.f);
		float Lo = FMath::Clamp(HomingReducedTurnRateScaleMin, 0.f, 1.f);
		float Hi = FMath::Clamp(HomingReducedTurnRateScaleMax, 0.f, 1.f);
		if (Lo > Hi)
		{
			const float Tmp = Lo;
			Lo = Hi;
			Hi = Tmp;
		}
		const float ReducedRandom = (Lo < Hi) ? FMath::FRandRange(Lo, Hi) : Lo;
		if (P >= 1.f - UE_KINDA_SMALL_NUMBER)
		{
			HomingTurnRateEffectiveScale = 1.f;
		}
		else if (P <= UE_KINDA_SMALL_NUMBER)
		{
			HomingTurnRateEffectiveScale = ReducedRandom;
		}
		else if (FMath::FRand() < P)
		{
			HomingTurnRateEffectiveScale = 1.f;
		}
		else
		{
			HomingTurnRateEffectiveScale = ReducedRandom;
		}
	}

	// ProjectileMovementComponent would overwrite our movement — disable its tick
	if (OwnerActor)
	{
		if (UProjectileMovementComponent* PMC = OwnerActor->FindComponentByClass<UProjectileMovementComponent>())
		{
			PMC->SetComponentTickEnabled(false);
			PMC->SetUpdatedComponent(nullptr);
			if (bHomingDebugLog)
			{
				UE_LOG(LogTemp, Warning, TEXT("MFKProjectile: disabled ProjectileMovementComponent (tick + UpdatedComponent) to avoid conflict"));
			}
		}
	}

	// Resolve target by tag in own BeginPlay — ensures we set on the ticking component (fixes Level BP wrong-instance issue)
	if (bEnableHoming && HomingTargetTag.IsNone() == false)
	{
		TArray<AActor*> Found;
		UGameplayStatics::GetAllActorsWithTag(this, HomingTargetTag, Found);
		if (Found.Num() > 0 && IsValid(Found[0]))
		{
			HomingTargetActor = Found[0];
			bUseWorldLocationForHoming = false;
		}
	}

	bHasBroadcastHit = false;
	SetupMoveIgnoreActors();
	BindToRootComponentHit();
}


void UMFKProjectileComponent::BindToRootComponentHit()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor) return;

	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	if (RootPrimitive)
	{
		RootPrimitive->OnComponentHit.AddDynamic(this, &UMFKProjectileComponent::OnRootComponentHit);
	}
}


void UMFKProjectileComponent::OnRootComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (bMovementStopped || bHasBroadcastHit) return;

	bHasBroadcastHit = true;
	OnProjectileHit.Broadcast(HitComponent, OtherActor, OtherComp, NormalImpulse, Hit);
}


void UMFKProjectileComponent::GetPerpendicularAxes(FVector& OutRight, FVector& OutUp) const
{
	GetPerpendicularAxesForDirection(CachedMovementDirection, OutRight, OutUp);
}


void UMFKProjectileComponent::GetPerpendicularAxesForDirection(const FVector& ForwardIn, FVector& OutRight, FVector& OutUp)
{
	FVector Forward = ForwardIn;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	OutRight = FVector::CrossProduct(Forward, FVector::UpVector);
	if (OutRight.IsNearlyZero())
	{
		OutRight = FVector::CrossProduct(Forward, FVector::RightVector).GetSafeNormal();
	}
	else
	{
		OutRight.Normalize();
	}
	OutUp = FVector::CrossProduct(OutRight, Forward).GetSafeNormal();
}


FVector UMFKProjectileComponent::ApplyGlobalMicroWobbleToDelta(const FVector& InDelta, float ElapsedTime) const
{
	if (!bEnableMicroWobble || MicroWobbleAmplitudeDeg <= 0.f || MicroWobbleFrequencyHz <= 0.f)
	{
		return InDelta;
	}

	const float DeltaSizeSq = InDelta.SizeSquared();
	constexpr float MinLenSq = UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER;
	if (DeltaSizeSq <= MinLenSq)
	{
		return InDelta;
	}

	const float BlendAlpha = (MicroWobbleBlendInSec > 0.f) ? FMath::Clamp(ElapsedTime / MicroWobbleBlendInSec, 0.f, 1.f) : 1.f;
	const float EffectiveAmpDeg = (bEnableHoming && MicroWobbleScaleWhenHoming >= 0.f)
		? MicroWobbleAmplitudeDeg * FMath::Clamp(MicroWobbleScaleWhenHoming, 0.f, 1.f)
		: MicroWobbleAmplitudeDeg;
	// Skip cross products + quats when blended angle is visually negligible.
	constexpr float MicroWobbleNegligibleBlendDeg = 0.05f;
	if (EffectiveAmpDeg * BlendAlpha < MicroWobbleNegligibleBlendDeg)
	{
		return InDelta;
	}

	const float DeltaSize = FMath::Sqrt(DeltaSizeSq);
	const FVector BaseDir = InDelta / DeltaSize;
	FVector Right = FVector::CrossProduct(BaseDir, FVector::UpVector);
	if (Right.IsNearlyZero())
	{
		Right = FVector::CrossProduct(BaseDir, FVector::RightVector).GetSafeNormal();
	}
	else
	{
		Right.Normalize();
	}
	const FVector Up = FVector::CrossProduct(Right, BaseDir).GetSafeNormal();

	const float TimeWave = CachedMicroWobbleTwoPiFreq * ElapsedTime;
	const float PitchWave = FMath::Sin(TimeWave + MicroWobbleRandomPhaseA);
	const float YawWave = FMath::Cos(TimeWave * 1.173f + MicroWobbleRandomPhaseB);

	const float WobbleRad = FMath::DegreesToRadians(EffectiveAmpDeg * BlendAlpha);
	const FQuat QPitch(Right, WobbleRad * PitchWave);
	const FQuat QYaw(Up, WobbleRad * YawWave);
	const FQuat QCombined = QYaw * QPitch;
	const FVector WobbledDir = QCombined.RotateVector(BaseDir).GetSafeNormal();
	return WobbledDir * DeltaSize;
}


FVector UMFKProjectileComponent::ComputeMovementDelta(float DeltaTime, float ElapsedTime, const FVector& ProjectileWorldLocation, const FVector& HomingDesiredDir, bool bHomingDesiredDirValid) const
{
	const float ForwardDist = Speed * DeltaTime;

	// Homing + None + acquisition delay: perfectly straight spawn-forward flight (no micro-wobble, no heading integration).
	const bool bHomingIntegratedNone = bEnableHoming && PathMode == EProjectilePathMode::None;
	const bool bInHomingAcquisitionDelay = bHomingIntegratedNone && HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec;
	if (bInHomingAcquisitionDelay)
	{
		return CachedMovementDirection * ForwardDist;
	}

	// Miss / overshoot: no loop-back — coast along last velocity (+ optional micro wobble).
	if (bHomingPastTargetAborted && bEnableHoming)
	{
		return ApplyGlobalMicroWobbleToDelta(HomingIntegratedForward.GetSafeNormal() * ForwardDist, ElapsedTime);
	}

	// Homing + None (after delay): integrate heading each frame so turn rate accumulates toward target.
	const FVector Forward = bHomingIntegratedNone
		? HomingIntegratedForward.GetSafeNormal()
		: CachedMovementDirection;

	if (PathMode == EProjectilePathMode::None && !bEnableMicroWobble)
	{
		return Forward * ForwardDist;
	}

	// Sine / Zigzag / Spiral / Vertical360: default frame is spawn-forward + CachedRight/Up.
	// With homing, use a frame aimed at the target so lateral motion orbits the LOS instead of drifting sideways in spawn space.
	FVector PathForward = Forward;
	FVector PathRight = CachedRight;
	FVector PathUp = CachedUp;

	if (PathMode != EProjectilePathMode::None && bEnableHoming && !bHomingPastTargetAborted)
	{
		const bool bPastAcquisition = !(HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec);
		if (bPastAcquisition)
		{
			FVector TargetLoc;
			if (TryGetHomingTargetLocation(TargetLoc))
			{
				const FVector ToTarget = TargetLoc - ProjectileWorldLocation;
				const float DistSq = ToTarget.SizeSquared();
				constexpr float MinDistSq = UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER;
				if (DistSq > MinDistSq)
				{
					const float Dist = FMath::Sqrt(DistSq);
					if (HomingPathTerminalDirectDistance > 0.f && Dist <= HomingPathTerminalDirectDistance)
					{
						const FVector ChaseForward = HomingIntegratedForward.GetSafeNormal();
						return ApplyGlobalMicroWobbleToDelta(ChaseForward * ForwardDist, ElapsedTime);
					}
					if (bHomingDesiredDirValid && !HomingDesiredDir.IsNearlyZero())
					{
						PathForward = HomingDesiredDir.GetSafeNormal();
					}
					else
					{
						PathForward = ToTarget / Dist;
					}
					GetPerpendicularAxesForDirection(PathForward, PathRight, PathUp);
				}
			}
		}
	}

	FVector BaseDelta = Forward * ForwardDist;

	switch (PathMode)
	{
	case EProjectilePathMode::None:
		BaseDelta = Forward * ForwardDist;
		break;

	case EProjectilePathMode::Sine:
	{
		const float Angle = CachedSineTwoPiFreq * ElapsedTime;
		const FVector LateralVelocity = SineAmplitude * CachedSineTwoPiFreq * FMath::Cos(Angle) * PathRight;
		BaseDelta = PathForward * ForwardDist + LateralVelocity * DeltaTime;
		break;
	}
	case EProjectilePathMode::Zigzag:
	{
		const float Phase = FMath::Fmod(ElapsedTime, CachedZigzagPeriod) / CachedZigzagPeriod;
		const float Sign = (Phase < 0.5f) ? 1.f : -1.f;
		const FVector LateralVelocity = Sign * CachedZigzagLateralCoeff * PathRight;
		BaseDelta = PathForward * ForwardDist + LateralVelocity * DeltaTime;
		break;
	}
	case EProjectilePathMode::Spiral:
	{
		const float Angle = CachedSpiralTwoPiFreq * ElapsedTime;
		const FVector LateralVelocity = SpiralRadius * CachedSpiralTwoPiFreq *
			(-FMath::Sin(Angle) * PathRight + FMath::Cos(Angle) * PathUp);
		BaseDelta = PathForward * ForwardDist + LateralVelocity * DeltaTime;
		break;
	}
	case EProjectilePathMode::Vertical360:
	{
		// Vertical 360:
		// Davranis:
		// - Ilk takla, IntervalSec kadar gecikmeden sonra baslar.
		// - Takla DurationSec boyunca devam eder (0->360).
		// - Takla bitince tekrar IntervalSec kadar bekleyip sonra yeniden baslar (cooldown).
		if (!bVertical360TimingValid)
		{
			return ApplyGlobalMicroWobbleToDelta(PathForward * ForwardDist, ElapsedTime);
		}

		// Ilk baslama gecikmesi (cached at BeginPlay)
		const float FirstDelay = Vertical360CachedFirstDelay;
		if (ElapsedTime < FirstDelay)
		{
			return ApplyGlobalMicroWobbleToDelta(PathForward * ForwardDist, ElapsedTime);
		}

		const float TimeSinceFirstStart = ElapsedTime - FirstDelay;
		const float CycleLength = Vertical360CachedCycleLength; // turn + cooldown
		const int32 CompletedCycles = static_cast<int32>(TimeSinceFirstStart / CycleLength);

		// Flip count limiti varsa, limitten sonra artik düz git.
		if (Vertical360FlipCount > 0 && CompletedCycles >= Vertical360FlipCount)
		{
			return ApplyGlobalMicroWobbleToDelta(PathForward * ForwardDist, ElapsedTime);
		}

		float CycleTime = FMath::Fmod(TimeSinceFirstStart, CycleLength);
		if (CycleTime < 0.f)
		{
			CycleTime += CycleLength;
		}

		// Takla bitince düz git.
		if (CycleTime > Vertical360CachedDurationSec)
		{
			return ApplyGlobalMicroWobbleToDelta(PathForward * ForwardDist, ElapsedTime);
		}

		// Easing ile taklaya yumuşak başlama/bitiş hissi
		float U = CycleTime * Vertical360CachedInvDuration; // 0..1
		U = FMath::Clamp(U, 0.f, 1.f);

		float UEased = U;
		switch (Vertical360EaseType)
		{
		case EVertical360EaseType::SmoothStep:
			// u*u*(3-2u)
			UEased = U * U * (3.f - 2.f * U);
			break;
		case EVertical360EaseType::EaseInOutCubic:
			// 4u^3 (u<0.5), 1 - (-2u+2)^3/2 (u>=0.5) — explicit cube avoids Pow
			if (U < 0.5f)
			{
				UEased = 4.f * U * U * U;
			}
			else
			{
				const float T = -2.f * U + 2.f;
				UEased = 1.f - (T * T * T) / 2.f;
			}
			break;
		default:
			UEased = U;
			break;
		}

		const float Angle = 2.f * UE_PI * UEased;

		// Wobble ile takla eksenini hafifçe sağ/sol oynat
		FVector Axis = PathRight;
		if (bVertical360WobbleEnabled && Vertical360WobbleAmplitudeDeg > 0.f && Vertical360WobbleCyclesPerTurn > 0.f)
		{
			float PerFlipPhase = 0.f;
			float PerFlipAmpScale = 1.f;
			if (bVertical360RandomizeWobblePerFlip)
			{
				// Her taklada deterministic random:
				// SpawnTime + CompletedCycles ile stabil fakat cycle'a gore degisen deger üretir.
				const float HashBase = (SpawnTime * 17.0f) + static_cast<float>(CompletedCycles + 1) * 23.417f;
				const float H1 = FMath::Frac(FMath::Sin(HashBase * 12.9898f) * 43758.5453f);
				const float H2 = FMath::Frac(FMath::Sin((HashBase + 1.0f) * 78.233f) * 14375.331f);
				PerFlipPhase = 2.f * UE_PI * H1;
				PerFlipAmpScale = FMath::Lerp(0.85f, 1.15f, H2);
			}

			const float WobblePhase = 2.f * UE_PI * (U * Vertical360WobbleCyclesPerTurn) + Vertical360WobbleRandomPhaseRad + PerFlipPhase;
			const float WobbleAmplitudeDeg = Vertical360WobbleAmplitudeDeg * Vertical360WobbleRandomAmplitudeScale * PerFlipAmpScale;
			const float WobbleRad = FMath::DegreesToRadians(WobbleAmplitudeDeg) * FMath::Sin(WobblePhase);
			Axis = FQuat(PathForward, WobbleRad).RotateVector(PathRight).GetSafeNormal();
		}

		// Cached perpendicular basis (same right-hand rule as GetPerpendicularAxes) —
		// forward'un up'a doğru "ilk önce yukarı dönmesi" icin ekseni tersleyerek kullanıyoruz.
		// Takla yonu terslenir (yukari yerine asagi gidiyorsa kullan).
		const FQuat Rot(Axis, Angle);
		const FVector RotatedForward = Rot.RotateVector(PathForward).GetSafeNormal();

		BaseDelta = RotatedForward * ForwardDist;
		break;
	}
	default:
		BaseDelta = Forward * ForwardDist;
		break;
	}

	return ApplyGlobalMicroWobbleToDelta(BaseDelta, ElapsedTime);
}


bool UMFKProjectileComponent::TryGetHomingTargetLocation(FVector& OutLocation) const
{
	if (bHomingDebugLog && !HomingDbgOnce.bTryGetEntry)
	{
		HomingDbgOnce.bTryGetEntry = true;
		UE_LOG(LogTemp, Warning, TEXT("[Homing] TryGetTarget: bUseWorldLoc=%d HasActor=%d IsValid=%d"), bUseWorldLocationForHoming, HomingTargetActor != nullptr, HomingTargetActor ? (IsValid(HomingTargetActor) ? 1 : 0) : 0);
	}

	// Prefer actor target
	if (!bUseWorldLocationForHoming && HomingTargetActor && IsValid(HomingTargetActor))
	{
		UWorld* OurWorld = GetWorld();
		UWorld* TargetWorld = HomingTargetActor->GetWorld();
		if (!OurWorld || !TargetWorld || OurWorld == TargetWorld)
		{
			OutLocation = HomingTargetActor->GetActorLocation();
			if (bHomingDebugLog && !HomingDbgOnce.bTryGetOkActor)
			{
				HomingDbgOnce.bTryGetOkActor = true;
				UE_LOG(LogTemp, Warning, TEXT("[Homing] TryGetTarget: OK (actor), Loc=%s"), *OutLocation.ToString());
			}
			return true;
		}
		if (bHomingDebugLog && !HomingDbgOnce.bTryGetWorldMismatch)
		{
			HomingDbgOnce.bTryGetWorldMismatch = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] TryGetTarget: FAIL (world mismatch)"));
		}
	}
	if (bUseWorldLocationForHoming)
	{
		OutLocation = CachedHomingWorldLocation;
		if (bHomingDebugLog && !HomingDbgOnce.bTryGetWorldLoc)
		{
			HomingDbgOnce.bTryGetWorldLoc = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] TryGetTarget: OK (world loc)"));
		}
		return true;
	}
	if (bHomingDebugLog && !HomingDbgOnce.bTryGetFailNoTarget)
	{
		HomingDbgOnce.bTryGetFailNoTarget = true;
		UE_LOG(LogTemp, Warning, TEXT("[Homing] TryGetTarget: FAIL (no target)"));
	}
	return false;
}


void UMFKProjectileComponent::DrawDebugHomingGhostIfNeeded() const
{
#if !UE_BUILD_SHIPPING
	if (!bDrawDebugHomingGhost || !GetWorld() || !bEnableHoming)
	{
		return;
	}
	if (!bHomingGhostAimInitialized || HomingAccuracyRadius <= UE_KINDA_SMALL_NUMBER || bHomingPastTargetAborted)
	{
		return;
	}
	if (HomingGhostBasisU.IsNearlyZero() || HomingGhostBasisV.IsNearlyZero())
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	FVector TargetLoc;
	if (!TryGetHomingTargetLocation(TargetLoc))
	{
		return;
	}
	const FVector AimPoint = TargetLoc + HomingGhostWorldOffset;
	{
		const int32 N = 32;
		FVector Prev = TargetLoc + HomingAccuracyRadius * HomingGhostBasisU;
		for (int32 i = 1; i <= N; ++i)
		{
			const float a = (2.f * UE_PI * i) / static_cast<float>(N);
			const FVector P = TargetLoc + HomingAccuracyRadius * (FMath::Cos(a) * HomingGhostBasisU + FMath::Sin(a) * HomingGhostBasisV);
			DrawDebugLine(World, Prev, P, FColor::Cyan, false, -1.f, 0, 2.f);
			Prev = P;
		}
	}
	const float GhostMarkerR = FMath::Clamp(HomingAccuracyRadius * 0.08f, 6.f, 40.f);
	DrawDebugSphere(World, AimPoint, GhostMarkerR, 8, FColor::Yellow, false, -1.f, 0, 2.f);
#endif
}


FVector UMFKProjectileComponent::ApplyHomingToDelta(const FVector& InDelta, const FVector& ProjectileLoc, float DeltaTime, float ElapsedTime, const FVector& HomingDesiredDir, bool bHomingDesiredDirValid) const
{
	if (!bEnableHoming)
	{
		if (bHomingDebugLog && !HomingDbgOnce.bSkipEnable)
		{
			HomingDbgOnce.bSkipEnable = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (bEnable=%d)"), bEnableHoming);
		}
		return InDelta;
	}

	if (bHomingPastTargetAborted)
	{
		return InDelta;
	}

	FVector TargetLoc;
	if (!TryGetHomingTargetLocation(TargetLoc))
	{
		if (bHomingDebugLog && !HomingDbgOnce.bSkipNoTarget)
		{
			HomingDbgOnce.bSkipNoTarget = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (no target)"));
		}
		return InDelta;
	}

	if (HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec)
	{
		if (bHomingDebugLog && !HomingDbgOnce.bSkipAcquisition)
		{
			HomingDbgOnce.bSkipAcquisition = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (acquisition delay)"));
		}
		return InDelta;
	}

	const float DeltaSizeSq = InDelta.SizeSquared();
	constexpr float MinLenSq = UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER;
	if (DeltaSizeSq <= MinLenSq)
	{
		if (bHomingDebugLog && !HomingDbgOnce.bSkipDeltaSmall)
		{
			HomingDbgOnce.bSkipDeltaSmall = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (delta too small)"));
		}
		return InDelta;
	}

	const FVector CurrentDir = InDelta.GetSafeNormal();
	FVector ToTarget;
	if (bHomingDesiredDirValid && !HomingDesiredDir.IsNearlyZero())
	{
		ToTarget = HomingDesiredDir.GetSafeNormal();
	}
	else
	{
		ToTarget = (TargetLoc - ProjectileLoc).GetSafeNormal();
		if (ToTarget.IsNearlyZero())
		{
			if (bHomingDebugLog && !HomingDbgOnce.bSkipToTargetZero)
			{
				HomingDbgOnce.bSkipToTargetZero = true;
				UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (ToTarget zero)"));
			}
			return InDelta;
		}
	}

	const float EffectiveTurnRate = HomingTurnRateDegPerSec * HomingTurnRateEffectiveScale;
	if (EffectiveTurnRate <= 0.f)
	{
		if (bHomingDebugLog && !HomingDbgOnce.bSkipTurnRate)
		{
			HomingDbgOnce.bSkipTurnRate = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (turn rate 0)"));
		}
		return InDelta;
	}

	const FQuat Q = FQuat::FindBetweenNormals(CurrentDir, ToTarget);
	const float AngleRad = Q.GetAngle();
	const float MaxAngleRad = FMath::DegreesToRadians(EffectiveTurnRate * DeltaTime);

	FVector NewDir;
	if (AngleRad <= MaxAngleRad || AngleRad < UE_KINDA_SMALL_NUMBER)
	{
		NewDir = ToTarget;
	}
	else
	{
		const float T = MaxAngleRad / AngleRad;
		const FQuat PartialQ = FQuat::Slerp(FQuat::Identity, Q, T);
		NewDir = PartialQ.RotateVector(CurrentDir).GetSafeNormal();
	}

	if (NewDir.IsNearlyZero())
	{
		if (bHomingDebugLog && !HomingDbgOnce.bSkipNewDirZero)
		{
			HomingDbgOnce.bSkipNewDirZero = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: SKIP (NewDir zero)"));
		}
		return InDelta;
	}

	const float DeltaSize = FMath::Sqrt(DeltaSizeSq);
	FVector Result = NewDir * DeltaSize;

	if (bHomingDebugLog)
	{
		const float t = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		if (t - HomingDbgOnce.LastOkLogTime >= 1.f)
		{
			HomingDbgOnce.LastOkLogTime = t;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] ApplyHoming: OK (angle=%.1f deg) DeltaSize=%.1f"), FMath::RadiansToDegrees(AngleRad), DeltaSize);
		}
	}
	return Result;
}


void UMFKProjectileComponent::SetHomingTarget_Implementation(AActor* InTarget)
{
	HomingTargetActor = InTarget;
	bUseWorldLocationForHoming = false;
	CachedHomingWorldLocation = FVector::ZeroVector;

	if (bHomingDebugLog)
	{
		UE_LOG(LogTemp, Warning, TEXT("MFKHoming SetHomingTarget called: InTarget=%s, IsValid=%d, Owner=%s"),
			InTarget ? *InTarget->GetName() : TEXT("nullptr"),
			InTarget ? (IsValid(InTarget) ? 1 : 0) : 0,
			GetOwner() ? *GetOwner()->GetName() : TEXT("nullptr"));
	}
}


void UMFKProjectileComponent::SetHomingTargetWorldLocation_Implementation(const FVector& WorldLocation)
{
	CachedHomingWorldLocation = WorldLocation;
	bUseWorldLocationForHoming = true;
}


void UMFKProjectileComponent::ClearHomingTarget()
{
	HomingTargetActor = nullptr;
	bUseWorldLocationForHoming = false;
}


void UMFKProjectileComponent::SetHomingTargetByTag(FName Tag)
{
	UWorld* World = GetWorld();
	if (!World || Tag.IsNone()) return;

	TArray<AActor*> Found;
	UGameplayStatics::GetAllActorsWithTag(World, Tag, Found);
	if (Found.Num() > 0 && IsValid(Found[0]))
	{
		SetHomingTarget(Found[0]);
	}
}


void UMFKProjectileComponent::ApplyOrientToVelocityIfNeeded(AActor* OwnerActor, const FVector& Delta) const
{
	if (!bOrientToVelocity || !OwnerActor)
	{
		return;
	}
	if (Delta.SizeSquared() <= UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER)
	{
		return;
	}
	OwnerActor->SetActorRotation(FRotationMatrix::MakeFromX(Delta).ToQuat());
}


void UMFKProjectileComponent::SetupMoveIgnoreActors()
{
	if (!bIgnoreOwnerWhenMoving) return;

	AActor* ProjectileActor = GetOwner();
	if (!ProjectileActor) return;

	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(ProjectileActor->GetRootComponent());
	if (!RootPrimitive) return;

	if (AActor* Owner = ProjectileActor->GetOwner())
	{
		RootPrimitive->MoveIgnoreActors.AddUnique(Owner);
	}
	if (APawn* Instigator = ProjectileActor->GetInstigator())
	{
		RootPrimitive->MoveIgnoreActors.AddUnique(Instigator);
	}
}


void UMFKProjectileComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bHomingDebugLog && bEnableHoming && !HomingDbgOnce.bTickOwner && GetOwner())
	{
		HomingDbgOnce.bTickOwner = true;
		UE_LOG(LogTemp, Warning, TEXT("[Homing] Tick Owner=%s (compare with SetHomingTarget Owner)"), *GetOwner()->GetName());
	}

	if (bMovementStopped) return;
	if (Speed <= 0.f || DeltaTime <= 0.f) return;
	if (CachedMovementDirection.IsNearlyZero()) return;

	AActor* OwnerActor = CachedOwnerActor.Get();
	UWorld* World = CachedWorld.Get();
	if (!OwnerActor)
	{
		OwnerActor = GetOwner();
	}
	if (!World)
	{
		World = GetWorld();
	}
	if (!OwnerActor || !World) return;

	const float CurrentTime = World->GetTimeSeconds();
	const FVector CurrentLoc = OwnerActor->GetActorLocation();

	// Lifetime check
	if (bHasLifetimeLimit && CurrentTime >= CachedExpiryTime)
	{
		bMovementStopped = true;
		SetComponentTickEnabled(false);
		OnProjectileLifetimeExpired.Broadcast();
		return;
	}

	// Movement delta (path modu + micro wobble + homing)
	const float ElapsedTime = CurrentTime - SpawnTime;

	if (bHomingAbortWhenPastTarget && bEnableHoming && !bHomingPastTargetAborted)
	{
		const bool bPastAcquisition = !(HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec);
		if (bPastAcquisition)
		{
			FVector PassCheckTargetLoc;
			if (TryGetHomingTargetLocation(PassCheckTargetLoc))
			{
				const FVector PassToT = (PassCheckTargetLoc - CurrentLoc).GetSafeNormal();
				if (!PassToT.IsNearlyZero())
				{
					const float ApproachDot = FVector::DotProduct(HomingIntegratedForward.GetSafeNormal(), PassToT);
					if (ApproachDot <= HomingPastTargetDotThreshold)
					{
						bHomingPastTargetAborted = true;
					}
				}
			}
		}
	}

	if (bEnableHoming && !bHomingPastTargetAborted)
	{
		const bool bGhostPastAcquisition = !(HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec);
		FVector GhostInitTargetLoc;
		if (bGhostPastAcquisition && !bHomingGhostAimInitialized && TryGetHomingTargetLocation(GhostInitTargetLoc))
		{
			if (HomingAccuracyRadius <= UE_KINDA_SMALL_NUMBER)
			{
				HomingGhostWorldOffset = FVector::ZeroVector;
				HomingGhostBasisU = FVector::ZeroVector;
				HomingGhostBasisV = FVector::ZeroVector;
				bHomingGhostAimInitialized = true;
			}
			else
			{
				const FVector Los = (GhostInitTargetLoc - CurrentLoc).GetSafeNormal();
				if (Los.IsNearlyZero())
				{
					HomingGhostWorldOffset = FVector::ZeroVector;
					HomingGhostBasisU = FVector::ZeroVector;
					HomingGhostBasisV = FVector::ZeroVector;
					bHomingGhostAimInitialized = true;
				}
				else
				{
					FVector U = FVector::CrossProduct(Los, FVector::UpVector);
					if (U.IsNearlyZero())
					{
						U = FVector::CrossProduct(Los, FVector::RightVector);
					}
					U.Normalize();
					const FVector V = FVector::CrossProduct(Los, U).GetSafeNormal();
					HomingGhostBasisU = U;
					HomingGhostBasisV = V;
					const float Angle = FMath::FRandRange(0.f, 2.f * UE_PI);
					HomingGhostWorldOffset = HomingAccuracyRadius * (FMath::Cos(Angle) * HomingGhostBasisU + FMath::Sin(Angle) * HomingGhostBasisV);
					bHomingGhostAimInitialized = true;
				}
			}
		}
	}

	FVector HomingDesiredDir = FVector::ZeroVector;
	bool bHomingDesiredDirValid = false;
	if (bEnableHoming && !bHomingPastTargetAborted)
	{
		const bool bPastAcquisition = !(HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec);
		FVector TargetLoc;
		if (bPastAcquisition && TryGetHomingTargetLocation(TargetLoc))
		{
			const FVector AimPoint = TargetLoc + HomingGhostWorldOffset;
			const FVector ToT = (AimPoint - CurrentLoc).GetSafeNormal();
			if (!ToT.IsNearlyZero())
			{
				HomingDesiredDir = ToT;
				bHomingDesiredDirValid = true;
			}
		}
	}

	const FVector Delta = ComputeMovementDelta(DeltaTime, ElapsedTime, CurrentLoc, HomingDesiredDir, bHomingDesiredDirValid);
	const FVector FinalDelta = ApplyHomingToDelta(Delta, CurrentLoc, DeltaTime, ElapsedTime, HomingDesiredDir, bHomingDesiredDirValid);
	const FVector NewLoc = CurrentLoc + FinalDelta;

	// Homing (any path): keep last velocity direction for terminal chase + None integration after acquisition delay.
	if (bEnableHoming)
	{
		if (HomingAcquisitionDelaySec > 0.f && ElapsedTime < HomingAcquisitionDelaySec)
		{
			HomingIntegratedForward = CachedMovementDirection.GetSafeNormal();
		}
		else if (FinalDelta.SizeSquared() > UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER)
		{
			HomingIntegratedForward = FinalDelta.GetSafeNormal();
		}
	}

	DrawDebugHomingGhostIfNeeded();

	if (bHomingDebugLog && bEnableHoming && !HomingDbgOnce.bTickDeltaDiff)
	{
		const float Diff = (Delta - FinalDelta).Size();
		if (Diff > 0.1f)
		{
			HomingDbgOnce.bTickDeltaDiff = true;
			UE_LOG(LogTemp, Warning, TEXT("[Homing] Tick: Delta!=FinalDelta (diff=%.2f) - homing modified movement"), Diff);
		}
	}

	// Max range check (after move)
	if (bHasMaxRangeLimit && FVector::DistSquared(StartLocation, NewLoc) >= CachedMaxRangeSq)
	{
		bMovementStopped = true;
		SetComponentTickEnabled(false);
		OnProjectileMaxRangeReached.Broadcast();
		return;
	}

	UPrimitiveComponent* RootPrimitive = CachedRootPrimitive.Get();
	if (!RootPrimitive)
	{
		RootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	}

	if (bUseSweep && RootPrimitive)
	{
		FHitResult Hit;
		const FQuat CurrentRot = OwnerActor->GetActorQuat();
		const bool bMoved = RootPrimitive->MoveComponent(FinalDelta, CurrentRot, true, &Hit, MOVECOMP_NoFlags, ETeleportType::None);

		if (!bMoved && Hit.bBlockingHit)
		{
			bMovementStopped = true;
			SetComponentTickEnabled(false);
			return;
		}
		if (bMoved)
		{
			ApplyOrientToVelocityIfNeeded(OwnerActor, FinalDelta);
			return;
		}
	}

	OwnerActor->SetActorLocation(NewLoc);
	ApplyOrientToVelocityIfNeeded(OwnerActor, FinalDelta);
}

