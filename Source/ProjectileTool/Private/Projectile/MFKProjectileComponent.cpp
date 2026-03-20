// Fill out your copyright notice in the Description page of Project Settings.

#include "ProjectileTool/Public/Projectile/MFKProjectileComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Components/PrimitiveComponent.h"


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


void UMFKProjectileComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplyTickRatePreset();

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (OwnerActor)
	{
		CachedMovementDirection = OwnerActor->GetActorForwardVector();
		if (!CachedMovementDirection.Normalize())
		{
			CachedMovementDirection = FVector::ForwardVector;
		}
		StartLocation = OwnerActor->GetActorLocation();
	}
	SpawnTime = World ? World->GetTimeSeconds() : 0.f;

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
	const FVector Forward = CachedMovementDirection;
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

	const float DeltaSize = InDelta.Size();
	if (DeltaSize <= UE_KINDA_SMALL_NUMBER)
	{
		return InDelta;
	}

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

	const float BlendAlpha = (MicroWobbleBlendInSec > 0.f) ? FMath::Clamp(ElapsedTime / MicroWobbleBlendInSec, 0.f, 1.f) : 1.f;
	const float TimeWave = 2.f * UE_PI * MicroWobbleFrequencyHz * ElapsedTime;
	const float PitchWave = FMath::Sin(TimeWave + MicroWobbleRandomPhaseA);
	const float YawWave = FMath::Cos(TimeWave * 1.173f + MicroWobbleRandomPhaseB);

	const float WobbleRad = FMath::DegreesToRadians(MicroWobbleAmplitudeDeg * BlendAlpha);
	const FQuat QPitch(Right, WobbleRad * PitchWave);
	const FQuat QYaw(Up, WobbleRad * YawWave);
	const FVector WobbledDir = (QYaw * QPitch).RotateVector(BaseDir).GetSafeNormal();
	return WobbledDir * DeltaSize;
}


FVector UMFKProjectileComponent::ComputeMovementDelta(float DeltaTime, float ElapsedTime) const
{
	const FVector Forward = CachedMovementDirection;
	const float ForwardDist = Speed * DeltaTime;
	FVector BaseDelta = Forward * ForwardDist;

	switch (PathMode)
	{
	case EProjectilePathMode::None:
		BaseDelta = Forward * ForwardDist;
		break;

	case EProjectilePathMode::Sine:
	{
		FVector Right, Up;
		GetPerpendicularAxes(Right, Up);
		const float Angle = 2.f * UE_PI * SineFrequency * ElapsedTime;
		const FVector LateralVelocity = SineAmplitude * 2.f * UE_PI * SineFrequency * FMath::Cos(Angle) * Right;
		BaseDelta = Forward * ForwardDist + LateralVelocity * DeltaTime;
		break;
	}
	case EProjectilePathMode::Zigzag:
	{
		FVector Right, Up;
		GetPerpendicularAxes(Right, Up);
		const float Period = (ZigzagFrequency > 0.f) ? (1.f / ZigzagFrequency) : 1.f;
		const float Phase = FMath::Fmod(ElapsedTime, Period) / Period;
		const float Sign = (Phase < 0.5f) ? 1.f : -1.f;
		const FVector LateralVelocity = Sign * (4.f * ZigzagAmplitude * ZigzagFrequency) * Right;
		BaseDelta = Forward * ForwardDist + LateralVelocity * DeltaTime;
		break;
	}
	case EProjectilePathMode::Spiral:
	{
		FVector Right, Up;
		GetPerpendicularAxes(Right, Up);
		const float Angle = 2.f * UE_PI * SpiralFrequency * ElapsedTime;
		const FVector LateralVelocity = SpiralRadius * 2.f * UE_PI * SpiralFrequency *
			(-FMath::Sin(Angle) * Right + FMath::Cos(Angle) * Up);
		BaseDelta = Forward * ForwardDist + LateralVelocity * DeltaTime;
		break;
	}
	case EProjectilePathMode::Vertical360:
	{
		// Vertical 360:
		// Davranis:
		// - Ilk takla, IntervalSec kadar gecikmeden sonra baslar.
		// - Takla DurationSec boyunca devam eder (0->360).
		// - Takla bitince tekrar IntervalSec kadar bekleyip sonra yeniden baslar (cooldown).
		FVector Right, Up;
		GetPerpendicularAxes(Right, Up);

		if (Vertical360IntervalSec <= 0.f || Vertical360DurationSec <= 0.f)
		{
			return ApplyGlobalMicroWobbleToDelta(Forward * ForwardDist, ElapsedTime);
		}

		// Ilk baslama gecikmesi
		const float FirstDelay = Vertical360IntervalSec;
		if (ElapsedTime < FirstDelay)
		{
			return ApplyGlobalMicroWobbleToDelta(Forward * ForwardDist, ElapsedTime);
		}

		const float TimeSinceFirstStart = ElapsedTime - FirstDelay;
		const float CycleLength = Vertical360DurationSec + Vertical360IntervalSec; // turn + cooldown
		const int32 CompletedCycles = static_cast<int32>(TimeSinceFirstStart / CycleLength);

		// Flip count limiti varsa, limitten sonra artik düz git.
		if (Vertical360FlipCount > 0 && CompletedCycles >= Vertical360FlipCount)
		{
			return ApplyGlobalMicroWobbleToDelta(Forward * ForwardDist, ElapsedTime);
		}

		float CycleTime = FMath::Fmod(TimeSinceFirstStart, CycleLength);
		if (CycleTime < 0.f)
		{
			CycleTime += CycleLength;
		}

		// Takla bitince düz git.
		if (CycleTime > Vertical360DurationSec)
		{
			return ApplyGlobalMicroWobbleToDelta(Forward * ForwardDist, ElapsedTime);
		}

		// Easing ile taklaya yumuşak başlama/bitiş hissi
		float U = CycleTime / Vertical360DurationSec; // 0..1
		U = FMath::Clamp(U, 0.f, 1.f);

		float UEased = U;
		switch (Vertical360EaseType)
		{
		case EVertical360EaseType::SmoothStep:
			// u*u*(3-2u)
			UEased = U * U * (3.f - 2.f * U);
			break;
		case EVertical360EaseType::EaseInOutCubic:
			// 4u^3 (u<0.5), 1 - (-2u+2)^3/2 (u>=0.5)
			UEased = (U < 0.5f)
				? (4.f * U * U * U)
				: (1.f - FMath::Pow(-2.f * U + 2.f, 3.f) / 2.f);
			break;
		default:
			UEased = U;
			break;
		}

		const float Angle = 2.f * UE_PI * UEased;

		// Wobble ile takla eksenini hafifçe sağ/sol oynat
		FVector Axis = Right;
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
			Axis = FQuat(Forward, WobbleRad).RotateVector(Right).GetSafeNormal();
		}

		// GetPerpendicularAxes'in sağ el kuralindaki eksen yönü nedeniyle,
		// forward'un up'a doğru "ilk önce yukarı dönmesi" icin ekseni tersleyerek kullanıyoruz.
		// Takla yonu terslenir (yukari yerine asagi gidiyorsa kullan).
		const FQuat Rot(Axis, Angle);
		const FVector RotatedForward = Rot.RotateVector(Forward).GetSafeNormal();

		BaseDelta = RotatedForward * ForwardDist;
		break;
	}
	default:
		BaseDelta = Forward * ForwardDist;
		break;
	}

	return ApplyGlobalMicroWobbleToDelta(BaseDelta, ElapsedTime);
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

	if (bMovementStopped) return;
	if (Speed <= 0.f || DeltaTime <= 0.f) return;
	if (CachedMovementDirection.IsNearlyZero()) return;

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World) return;

	const float CurrentTime = World->GetTimeSeconds();
	const FVector CurrentLoc = OwnerActor->GetActorLocation();

	// Lifetime check
	if (Lifetime > 0.f && (CurrentTime - SpawnTime) >= Lifetime)
	{
		bMovementStopped = true;
		SetComponentTickEnabled(false);
		OnProjectileLifetimeExpired.Broadcast();
		return;
	}

	// Movement delta (path modu: None / Sine / Zigzag / Spiral)
	const float ElapsedTime = CurrentTime - SpawnTime;
	const FVector Delta = ComputeMovementDelta(DeltaTime, ElapsedTime);
	const FVector NewLoc = CurrentLoc + Delta;

	// Max range check (after move)
	if (MaxRange > 0.f && FVector::DistSquared(StartLocation, NewLoc) >= MaxRange * MaxRange)
	{
		bMovementStopped = true;
		SetComponentTickEnabled(false);
		OnProjectileMaxRangeReached.Broadcast();
		return;
	}

	USceneComponent* Root = OwnerActor->GetRootComponent();
	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Root);

	if (bUseSweep && RootPrimitive)
	{
		FHitResult Hit;
		const FQuat CurrentRot = OwnerActor->GetActorQuat();
		const bool bMoved = RootPrimitive->MoveComponent(Delta, CurrentRot, true, &Hit, MOVECOMP_NoFlags, ETeleportType::None);

		if (!bMoved && Hit.bBlockingHit)
		{
			bMovementStopped = true;
			SetComponentTickEnabled(false);
			return;
		}
		if (bMoved)
		{
			if (bOrientToVelocity && Delta.SizeSquared() > UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER)
			{
				OwnerActor->SetActorRotation(FRotationMatrix::MakeFromX(Delta).ToQuat());
			}
			return;
		}
	}

	OwnerActor->SetActorLocation(NewLoc);
	if (bOrientToVelocity && Delta.SizeSquared() > UE_KINDA_SMALL_NUMBER * UE_KINDA_SMALL_NUMBER)
	{
		OwnerActor->SetActorRotation(FRotationMatrix::MakeFromX(Delta).ToQuat());
	}
}

