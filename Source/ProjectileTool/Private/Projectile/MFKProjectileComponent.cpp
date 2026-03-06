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

	// Movement delta
	const FVector Delta = CachedMovementDirection * Speed * DeltaTime;
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
		if (bMoved) return;
	}

	OwnerActor->SetActorLocation(NewLoc);
}

