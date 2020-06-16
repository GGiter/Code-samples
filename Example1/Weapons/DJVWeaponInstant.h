// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Weapons/DJVWeapon.h"
#include "DJVWeaponInstant.generated.h"

USTRUCT()
struct FInstantHitInfo
{
    GENERATED_USTRUCT_BODY()

    UPROPERTY()
    FVector Origin;

    UPROPERTY()
    float ReticleSpread;

    UPROPERTY()
    int32 RandomSeed;
};

USTRUCT()
struct FInstantWeaponData
{
    GENERATED_USTRUCT_BODY()

    /** Base weapon spread (degrees) */
    UPROPERTY(EditDefaultsOnly, Category = "Accuracy")
    float WeaponSpread;

    /** Targeting spread modifier */
    UPROPERTY(EditDefaultsOnly, Category = Accuracy)
    float AimingSpreadModifier;

    /** Firing Spread Decreasing Per Second after the burst was finished */
    UPROPERTY(EditDefaultsOnly, Category = "Accuracy")
    float FiringSpreadDecreasePerSecond;

    /** Continuous firing: spread increment */
    UPROPERTY(EditDefaultsOnly, Category = "Accuracy")
    float FiringSpreadIncrement;

    /** Continuous firing: max increment */
    UPROPERTY(EditDefaultsOnly, Category = "Accuracy")
    float FiringSpreadMax;

    /** Weapon range */
    UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
    float WeaponRange;

    /** Hit verification: scale for bounding box of hit actor */
    UPROPERTY(EditDefaultsOnly, Category = "HitVerification")
    float ClientSideHitLeeway;

    /** Hit verification: threshold for dot product between view direction and hit direction */
    UPROPERTY(EditDefaultsOnly, Category = "HitVerification")
    float AllowedViewDotHitDir;

    /** defaults */
    FInstantWeaponData()
    {
        WeaponSpread = 5.0f;
        AimingSpreadModifier = 0.25f;
        FiringSpreadDecreasePerSecond = 6.0f;
        FiringSpreadIncrement = 1.0f;
        FiringSpreadMax = 10.0f;
        WeaponRange = 8000.0f;
        ClientSideHitLeeway = 200.0f;
        AllowedViewDotHitDir = 0.8f;
    }
};

/**
 * 
 */
UCLASS()
class DEJAVU_API ADJVWeaponInstant : public ADJVWeapon
{
    GENERATED_BODY()

    virtual void Tick(float DeltaTime) override;

public:

    /** Sets default values for this actor's properties*/
    ADJVWeaponInstant();

    //////////////////////////////////////////////////////////////////////////
    // Reading data

    UFUNCTION(BlueprintCallable)
    float GetCurrentSpread() const;

protected:

    //////////////////////////////////////////////////////////////////////////
    // Weapon usage

    /** server notified of hit from client to verify */
    UFUNCTION(Reliable, server, WithValidation)
    void ServerNotifyHit(const FHitResult& Impact, FVector_NetQuantizeNormal ShootDir, int32 RandomSeed, float ReticleSpread);

    /** server notified of miss to show trail FX */
    UFUNCTION(Unreliable, server, WithValidation)
    void ServerNotifyMiss(FVector_NetQuantizeNormal ShootDir, int32 RandomSeed, float ReticleSpread);

    /** [local] weapon specific fire implementation */
    virtual void FireWeapon() override;

    /** [local + server] update recoil upon bursting is finished */
    virtual void OnBurstFinished() override;

    /** Check if weapon should deal damage to actor */
    bool ShouldDealDamage(AActor* TestActor) const;

    /** Process the weapon hit and notify the server if necessary */
    void ProcessHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir, int32 RandomSeed, float ReticleSpread);

    /** Continue processing the weapon hit, as if it has been confirmed by the server */
    void ProcessHit_Confirmed(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir, int32 RandomSeed, float ReticleSpread);

    //////////////////////////////////////////////////////////////////////////
    // Effects & Replication

    /** Spawn trail effect */
    void SpawnTrailEffect(const FVector& EndPoint);

    /** Spawn impact effect */
    void SpawnImpactEffect(const FHitResult& Impact);

    UFUNCTION()
    void OnRep_HitNotify();

    /** Called in network play to do the weapon cosmetic FX  */
    void SimulateHit(const FVector& ShotOrigin, int32 RandomSeed, float ReticleSpread);

protected:

    /** Instant Weapon config */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    FInstantWeaponData InstantConfig;

    /** Current spread from continuous firing */
    float CurrentFiringSpread;

    /** Smoke trail */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    UParticleSystem* TrailFX;

    /** Param name for beam target in smoke trail */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName TrailTargetParam;

    /** This will allow us to replicate the FX's of the weapon */
    UPROPERTY(Transient, ReplicatedUsing = OnRep_HitNotify)
    FInstantHitInfo HitNotify;

    /** Impact effects */
    UPROPERTY(EditDefaultsOnly, Category = Effects)
    TSubclassOf<class ADJVImpactEffect> ImpactTemplate;
};
