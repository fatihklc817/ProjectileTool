// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "MFKProjectileComponent.generated.h"

class UPrimitiveComponent;

/** Tick guncelleme sikligi: 60 Hz+ akici hareket, dusuk degerler takilma yapar. */
UENUM(BlueprintType)
enum class EProjectileTickRatePreset : uint8
{
	/** 60 Hz - 60 fps ekranda akici, performans dostu. */
	Low     UMETA(DisplayName = "Low (60 Hz)"),
	/** 120 Hz - yuksek refresh rate ekranlarda akici. */
	Medium  UMETA(DisplayName = "Medium (120 Hz)"),
	/** Her frame - en yuksek hassasiyet. */
	Max     UMETA(DisplayName = "Max (Every Frame)")
};

/** Unreal OnComponentHit ile ayni parametreler (HitComponent = merminin root'u, OtherActor/OtherComp = carpidigi). */
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
	/** Mermi hizi (unite/saniye). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement", meta = (ClampMin = "0"))
	float Speed = 1000.f;

	/** Harekette owner'in root collision'ini kullan (sweep). Root UPrimitiveComponent olmali; carpinca durur. Kapaliysa collision yok. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement")
	bool bUseSweep = true;

	/** Root primitive'in MoveIgnoreActors'ina Owner/Instigator eklenir; ates eden vurulmaz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Movement")
	bool bIgnoreOwnerWhenMoving = true;

	// --- Performance ---
	/** Tick guncelleme sikligi: Low = 60 Hz, Medium = 120 Hz, Max = her frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Performance")
	EProjectileTickRatePreset TickRatePreset = EProjectileTickRatePreset::Max;

	// --- Lifetime & Range ---
	/** Maksimum yasam suresi (saniye). 0 = sinirsiz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Lifetime", meta = (ClampMin = "0"))
	float Lifetime = 0.f;

	/** Maksimum mesafe (unite). 0 = sinirsiz. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Projectile|Lifetime", meta = (ClampMin = "0"))
	float MaxRange = 0.f;

	// --- Events (Blueprint assignable) ---
	/** Carpismada tek sefer tetiklenir (cift hit engellenir). */
	UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
	FOnProjectileHitSignature OnProjectileHit;

	/** Lifetime asildiginda tetiklenir. */
	UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
	FOnProjectileLifetimeExpiredSignature OnProjectileLifetimeExpired;

	/** MaxRange asildiginda tetiklenir. */
	UPROPERTY(BlueprintAssignable, Category = "Projectile|Events")
	FOnProjectileMaxRangeReachedSignature OnProjectileMaxRangeReached;

protected:
	virtual void BeginPlay() override;

	/** Spawn anindaki ileri yon; Tick'te bu yonde hareket eder. */
	UPROPERTY()
	FVector CachedMovementDirection;

	/** Spawn anindaki konum (MaxRange hesabi icin). */
	UPROPERTY()
	FVector StartLocation;

	/** Spawn anindaki world time (Lifetime hesabi icin). */
	UPROPERTY()
	float SpawnTime = 0.f;

	/** Carptiktan veya sure/mesafe asildiktan sonra hareket durdurulur. */
	UPROPERTY()
	bool bMovementStopped = false;

	/** OnProjectileHit sadece ilk carpismada bir kez broadcast edilir. */
	UPROPERTY()
	bool bHasBroadcastHit = false;

	void ApplyTickRatePreset();
	void SetupMoveIgnoreActors();
	void BindToRootComponentHit();

	UFUNCTION()
	void OnRootComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
