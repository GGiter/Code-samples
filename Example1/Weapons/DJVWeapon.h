// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "DJVWeapon.generated.h"

class ADJVCharacter;
class UAnimSequence;
class UCameraShake;
class UParticleSystem;
class UDataTable;
class UGameplayEffect;
class UAbilitySystemComponent;
class USkeletalMeshComponent;
class UDJVWeaponAttributeSet;

namespace EWeaponState
{
    enum Type
    {
        Idle,
        Equipping,
        Firing,
        Reloading,
    };
}

UENUM(BlueprintType)
enum class EShootingMode : uint8
{
    AutoFire UMETA(DisplayName = "Auto"),
    SingleFire UMETA(DisplayName = "Single"),
    BurstFire UMETA(DisplayName = "Burst")
};

USTRUCT()
struct FWeaponRecoilData
{
    GENERATED_USTRUCT_BODY()

    /** Vertical Recoil Curve */
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    UCurveFloat* VerticalCurve;

    /** Horizontal Recoil Curve */
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    UCurveFloat* HorizontalCurve;

    /** Vertical Recoil Recovery speed */
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float RecoverVerticalSpeed;

    /** Horizontal Recoil Recovery speed */
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float RecoverHorizontalSpeed;

    /** Delay until Recoil Recovery starts*/
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float RecoveryDelay;

    /** Recoil coefficient multiplier while character moves*/
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float MoveCoefficient;

    /** Aiming Down Sights coefficient multiplier*/
    UPROPERTY(EditDefaultsOnly, Category = "Recoil")
    float AimCoefficient;

    /** Initialize Defaults*/
    FWeaponRecoilData()
    {
        RecoverVerticalSpeed = 15.0f;
        RecoverHorizontalSpeed = 15.0f;
        RecoveryDelay = 0.1f;
        MoveCoefficient = 0.1f;
        AimCoefficient = 0.5f;
    }
};

USTRUCT()
struct FWeaponData
{
    GENERATED_USTRUCT_BODY()

    /** Infinite ammo for reloads */
    UPROPERTY(EditDefaultsOnly, Category = "Ammo")
    bool bInfiniteAmmo;

    /** Infinite ammo in clip, no reload required */
    UPROPERTY(EditDefaultsOnly, Category = "Ammo")
    bool bInfiniteClip;

    /** Max ammo */
    UPROPERTY(EditDefaultsOnly, Category = "Ammo")
    int32 MaxAmmo;

    /** Clip size */
    UPROPERTY(EditDefaultsOnly, Category = "Ammo")
    int32 AmmoPerClip;

    /** Initial clips */
    UPROPERTY(EditDefaultsOnly, Category = "Ammo")
    int32 InitialClips;

    /** How many rounds would a weapon fire in a minute if it didn’t have to reload*/
    UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
    float RateOfFire;

    /** Failsafe reload duration if weapon doesn't have any animation for it */
    UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
    float NoAnimReloadDuration;

    /** Weapon shooting mode (ex.: Auto, Burst, Single)*/
    UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
    EShootingMode ShootingMode;

    /** Amount of shots the weapon should do in one burst */
    UPROPERTY(EditDefaultsOnly, Category = "WeaponStats")
    uint8 BurstFireShotsBeforePause;

    /** defaults */
    FWeaponData()
    {
        bInfiniteAmmo = false;
        bInfiniteClip = false;
        MaxAmmo = 100;
        AmmoPerClip = 20;
        InitialClips = 4;
        BurstFireShotsBeforePause = 3;
        RateOfFire = 600;
        NoAnimReloadDuration = 1.0f;
        ShootingMode = EShootingMode::AutoFire;
    }
};

/**
* The purpose of this struct is to hold Animation Montage (or AnimSequence in case of Weapons) related data both for pawn and for weapon
*/
USTRUCT()
struct FWeaponAnim
{
    GENERATED_USTRUCT_BODY()

    /** Animation played on pawn (1st person view) */
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* Pawn1P;

    /** Animation played on pawn (3rd person view) */
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    UAnimMontage* Pawn3P;

    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    bool bHasWeaponAnims = false;

    /** Animation played on the weapon (1st person view)*/
    UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (EditCondition = "bHasWeaponAnims"))
    UAnimSequence* Weapon1P;

    /** Animation played on the weapon (3rd person view)*/
    UPROPERTY(EditDefaultsOnly, Category = "Animation", meta = (EditCondition = "bHasWeaponAnims"))
    UAnimSequence* Weapon3P;
};

UCLASS()
class DEJAVU_API ADJVWeapon : public AActor, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    ADJVWeapon();

protected:

    virtual void BeginPlay() override;

    /** perform initial setup */
    virtual void PostInitializeComponents() override;

    virtual void Destroyed() override;

    //////////////////////////////////////////////////////////////////////////
    // Replication & Effects

    UFUNCTION()
    void OnRep_OwnerPawn();

    UFUNCTION()
    void OnRep_Reload();

    UFUNCTION()
    void OnRep_BurstCounter();

    /** Called in network play to do the cosmetic fx for firing */
    virtual void PlayWeaponFireEffects();

    /** Called in network play to stop cosmetic fx (e.g. for a looping shot). */
    virtual void StopWeaponFireEffects();

public:
	// Ammo getters for HUD

	UFUNCTION(BlueprintPure,Category="Ammo")
		float GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "Ammo")
		float GetCurrentAmmoInClip() const { return CurrentAmmoInClip; }

    //////////////////////////////////////////////////////////////////////////
    // Weapon Equip

    /** Weapon is being equipped by owner pawn */
    virtual void OnEquip(ADJVWeapon* LastWeapon);

    /** Weapon is holstered by owner pawn */
    virtual void OnUnEquip();

    /** Consume a bullet */
    void UseAmmo();

    /** [server] Weapon was added to pawn's inventory */
    virtual void OnEnterInventory(ADJVCharacter* NewOwner);

    /** [server] Weapon was removed from pawn's inventory */
    virtual void OnLeaveInventory();

    //////////////////////////////////////////////////////////////////////////
    // Weapon Helpers

    /** Set the weapon's owning pawn */
    void SetOwningPawn(ADJVCharacter* NewOwner);

    //////////////////////////////////////////////////////////////////////////
    // Input

    /** [local + server] start weapon fire */
    virtual void StartFire();

    /** [local + server] stop weapon fire */
    virtual void StopFire();

    /** [all] start weapon reload */
    virtual void StartReload(bool bFromReplication = false);

    /** [local + server] interrupt weapon reload */
    virtual void StopReload();

    /** [server] performs actual reload */
    virtual void ReloadWeapon();

    //////////////////////////////////////////////////////////////////////////
    // Control

    /** Check if weapon can fire */
    bool CanFire() const;

    /** Check if weapon can be reloaded */
    bool CanReload() const;

    //////////////////////////////////////////////////////////////////////////
    // Reading data

    /** Check if mesh is already attached */
    bool IsAttachedToPawn() const;

    /** Check if weapon has infinite ammo*/
    bool HasInfiniteAmmo() const;

    /** Check if weapon has infinite clip */
    bool HasInfiniteClip() const;

    /** Check if weapon is reloading*/
    UFUNCTION(BlueprintCallable, Category = "Weapon")
    bool IsReloading() const;

    /** Get current weapon state */
    EWeaponState::Type GetCurrentState() const;

    /** Get weapon mesh (needs pawn owner to determine variant) */
    UFUNCTION(BlueprintCallable, Category = "Weapon|Mesh")
    USkeletalMeshComponent* GetWeaponMesh() const;

protected:

    UFUNCTION(BlueprintCallable, Category = "Abilities")
    UAbilitySystemComponent* GetAbilitySystemComponent() const;

    /** Get this pawn Attribute Set*/
    FORCEINLINE UDJVWeaponAttributeSet* GetAttributeSet() const { return AttributeSet; }

    /** Gets called on begin play, should init stats for weapon using data table */
    virtual void InitializeAttributeDefaults();

private:

    /** Ability system used for attribute interaction */
    UPROPERTY(VisibleAnywhere, Category = Components)
    UAbilitySystemComponent* AbilitySystem;

    /** Weapon damage attributes */
    UPROPERTY(EditDefaultsOnly, Category = "Abilities")
    UDJVWeaponAttributeSet* AttributeSet;

protected:

    /** The data table to initialize attributes with on BeginPlay */
    UPROPERTY(EditAnywhere, Category = "Weapon | Attributes")
    UDataTable* AttributeDefaults;

protected:

    //////////////////////////////////////////////////////////////////////////
    // Weapon Equip

    /** Weapon is now equipped by owner pawn */
    virtual void OnEquipFinished();

    /** Attaches weapon mesh to pawn's mesh */
    virtual void AttachMeshToPawn();

    /** Detaches weapon mesh from pawn */
    virtual void DetachMeshFromPawn();

    //////////////////////////////////////////////////////////////////////////
    // Weapon Usage

    /** [local] weapon specific fire implementation */
    virtual void FireWeapon() { };

    /** [server] fire & update ammo */
    UFUNCTION(Reliable, server, WithValidation)
    void ServerHandleFiring();

    /** [local + server] handle weapon fire */
    void HandleFiring();

    /** [local + server] handle weapon refire */
    void HandleReFiring();

    /** [local + server] firing started */
    virtual void OnBurstStarted();

    /** [local + server] firing finished */
    virtual void OnBurstFinished();

    /** Update weapon state */
    void SetWeaponState(EWeaponState::Type NewState);

    /** Determine current weapon state */
    void DetermineWeaponState();

    //////////////////////////////////////////////////////////////////////////
    // Weapon usage helpers

    /** Play weapon animations on the character*/
    float PlayAnimation(const FWeaponAnim& Animation);

    /** Cancel playing weapon animations on the character */
    void StopAnimation(const FWeaponAnim& Animation);

    /** Play weapon animations on the weapon' 3rd or 1st Person Mesh*/
    float PlayWeaponAnimation(const FWeaponAnim& Animation, bool bIsFirstPerson = true);

    /** Cancel weapon animations on the weapon' 3rd or 1st Person Mesh*/
    void StopWeaponAnimation(const FWeaponAnim& Animation, bool bIsFirstPerson = true);

    /** Get the muzzle location of the weapon */
    FVector GetMuzzleLocation() const;

    /** Get the muzzle direction of the weapon */
    FVector GetMuzzleRotation() const;

    /** Get the aim of the weapon, allowing for adjustments to be made by the weapon */
    virtual FVector GetAdjustedAim() const;

    /** Get the originating location for camera fire */
    FVector GetCameraFireStartLocation(const FVector& AimDir) const;

    /** Find what this weapon hit */
    FHitResult SendWeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const;

    //////////////////////////////////////////////////////////////////////////
    // Input - server side

    UFUNCTION(Reliable, server, WithValidation)
    void ServerStartFire();

    UFUNCTION(Reliable, server, WithValidation)
    void ServerStopFire();

    UFUNCTION(Reliable, server, WithValidation)
    void ServerStartReload();

    UFUNCTION(Reliable, server, WithValidation)
    void ServerStopReload();

    //////////////////////////////////////////////////////////////////////////
    // Recoil

    /** Start Recoil*/
    UFUNCTION(Reliable, client)
    void ClientHandleRecoil(bool bRecoilOn = true);

    /** Start interpolating recoil from last frame*/
    UFUNCTION(Reliable, client)
    void ClientInterpolateRecoil();

    /** Start interpolating recoil from last frame*/
    UFUNCTION(Reliable, client)
    void ClientStopRecoilInterpolation();

    /** Stop Previous recoil*/
    UFUNCTION(Reliable, client)
    void ClientStopRecoil();

    /** Update current weapon recoil and adjust Controller rotation*/
    void UpdateRecoilAndControllerRotation();

    /** Add any recoil modifiers set into Recoil Config to the deltas*/
    void CalculateRecoilCoefficient(float& VerticalRecoilDelta, float& HorizontalRecoilDelta);

    /** Interpolate the recoil values from last shot and apply them to the Controller */
    void CalculateRecoilInterpolationStep(float& InterpolationVerticalRecoil, float& InterpolationHorizontalRecoil);

    //////////////////////////////////////////////////////////////////////////
    // Recoil Recovery

    /** Start delay for Recoil Recovery*/
    UFUNCTION(Reliable, client)
    void ClientStartRecoilRecoveryDelay();

    /** Start Recoil Recovery*/
    UFUNCTION(Reliable, client)
    void ClientStartRecoilRecovery();

    /** Stop Recoil Recovery*/
    UFUNCTION(Reliable, client)
    void ClientStopRecoilRecovery();

    void ApplyRecoilRecoveryOnController(float VerticalRecoveryRecoilDelta, float HorizontalRecoveryRecoilDelta);

protected:

    /** Camera shake on firing */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    TSubclassOf<UCameraShake> FireCameraShake;

    /** Name of bone/socket for muzzle in weapon mesh */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName MuzzleAttachPoint;

    /** Barrel Smoke upon firing */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    UParticleSystem* BarrelSmokeFX;

    /** Name of bone/socket for attaching the barrel smoke particle system */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName BarrelSmokeAttachPoint;

    /** Shells coming out of the gun upon firing */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    UParticleSystem* ShellsFX;

    /** Name of bone/socket for attaching the shells coming out of the gun particle system */
    UPROPERTY(EditDefaultsOnly, Category = "Effects")
    FName ShellsAttachPoint;

    /** Is fire animation looped? */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
    uint32 bLoopedFireAnim : 1;

    /** Is fire animation playing? */
    uint32 bPlayingFireAnim : 1;

    /** Fire animations */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
    FWeaponAnim FireAnim;

    /** Equip animations */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
    FWeaponAnim EquipAnim;

    /** Equip animations */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
    FWeaponAnim HolsterAnim;

    /** Reload animations */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Animation")
    FWeaponAnim ReloadAnim;

    UPROPERTY(EditDefaultsOnly, Category = "Weapon|Abilities")
    TSubclassOf<UGameplayEffect> FireGameplayEffect;

    /** Is equip animation playing? */
    uint32 bPendingEquip : 1;

    /** Is weapon currently equipped? */
    uint32 bEquipped : 1;

    /** Is weapon fire active? */
    uint32 bWantsToFire : 1;

    /** Is reload animation playing? */
    UPROPERTY(Transient, ReplicatedUsing = OnRep_Reload)
    uint32 bPendingReload : 1;

    /** Is weapon currently shooting? */
    UPROPERTY(Transient, Replicated)
    uint32 bShooting;

    /** Is weapon re-firing? */
    uint32 bRefiring;

    /** Is weapon currently reloading?*/
    UPROPERTY(Transient, Replicated)
    uint32 bReloading;

    /** Used for weapon burst to determine how many shots we fired so far*/
    uint8 ShotsCount;

    /** Adjustment to handle frame rate affecting actual timer interval. */
    UPROPERTY(Transient)
    float TimerIntervalAdjustment;

    /** Whether to allow automatic weapons to catch up with shorter re-fire cycles */
    UPROPERTY(Config)
    bool bAllowAutomaticWeaponCatchup = true;

    /** Current weapon state */
    EWeaponState::Type CurrentState;

    /** Time of last successful weapon fire */
    float LastFireTime;

    /** Time between two consecutive shots calculated based on RateOfFire*/
    float TimeBetweenShots;

    /** Last time when this weapon was switched to */
    float EquipStartedTime;

    /** How much time weapon needs to be equipped */
    float EquipDuration;

    /** Current total ammo */
    UPROPERTY(Transient, Replicated)
    int32 CurrentAmmo;

    /** Current ammo - inside clip */
    UPROPERTY(Transient, Replicated)
    int32 CurrentAmmoInClip;

    /** Burst counter, used for replicating fire events to remote clients */
    UPROPERTY(Transient, ReplicatedUsing = OnRep_BurstCounter)
    int32 BurstCounter;

    /** Handle for efficient management of StopReload timer */
    FTimerHandle TimerHandle_StopReload;

    /** Handle for efficient management of ReloadWeapon timer */
    FTimerHandle TimerHandle_ReloadWeapon;

    /** Handle for efficient management of OnEquipFinished timer */
    FTimerHandle TimerHandle_OnEquipFinished;

    /** Handle for efficient management of HandleFiring timer */
    FTimerHandle TimerHandle_HandleFiring;

    /** Handle for efficient management of HandleRefiring timer */
    FTimerHandle TimerHandle_HandleRefiring;

    /** Handle for efficient management of ClientInterpolateRecoil timer */
    FTimerHandle TimerHandle_InterpolateRecoil;

    /** Handle for efficient management of StartRecoilRecovery timer */
    FTimerHandle TimerHandle_StartRecoilRecovery;

    //////////////////////////////////////////////////////////////////////////
    // Recoil

    /** Last Recoil Progress*/
    float FinishRecoilTimePerShot;

    /** Used in case the weapon has stopped and started bursting immediately*/
    float StartRecoilTimePerShot;

    /** The rate at which the recoil increases*/
    float RecoilInterpolationStep;

    /** Used to Calculate how much vertical recoil should be added to Controller since last shot*/
    float NewInterpolationVerticalRecoil;

    /** Used to Calculate how much vertical recoil should be added to Controller since last shot*/
    float OldInterpolationVerticalRecoil;

    /** Used to Calculate how much horizontal recoil should be added to Controller since last shot*/
    float NewInterpolationHorizontalRecoil;

    /** Used to Calculate how much horizontal recoil should be added to Controller since last shot*/
    float OldInterpolationHorizontalRecoil;

    int32 RecoilSteps;

    bool bCheckFirstInterpolation;

    /** Stop Recoil*/
    bool bRecoilForceStop;

    //////////////////////////////////////////////////////////////////////////
    // Recoil Recovery

    /** Vertical Recoil Recovery to apply on Controller */
    float RecoilRecoveryVerticalValue;

    /** Horizontal Recoil Recovery to apply on Controller */
    float RecoilRecoveryHorizontalValue;

    /** How quickly the Recoil Recovery happens*/
    float RecoilRecoveryStep;

    /** The speed at which the Vertical Recoil Recovery Increases */
    float RecoilRecoveryVerticalStep;

    /** The speed at which the Horizontal Recoil Recovery Increases */
    float RecoilRecoveryHorizontalStep;

public:

    //////////////////////////////////////////////////////////////////////////
    // Animation

    /**
    * Check if the animation exists for the selected view (1st or 3rd Person)
    * @param AnimationName to look for
    * @param WantsFirstPerson Whether searches in the 3rd or 1st Person AnimationSequence container
    */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    bool HasAnimation(FName AnimationName, bool bForFirstPerson = true);
    
    /**
    * Find and return 1st or 3rd Person Anim.
    *
    * @param AnimationName The key that is bounded to the animation.
    * @param bWantsFirstPerson Whether searches in the 3rd or 1st Person AnimationSequence container
    */
    UFUNCTION(BlueprintCallable, Category = "Animation")
    UAnimSequence* GetAnimation(FName AnimationName, bool bWantsFirstPerson = true);

protected:

    /**
    * The purpose of this container is to hold AnimationSequnce data that will be played by the 1st Person Pawn (ex: idle, jump, walk, run)
    */
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    TMap<FName, UAnimSequence*> Pawn1PAnims;

    /**
    * The purpose of this container is to hold AnimationSequnce data that will be played by the 3rd Person Pawn (ex: idle, jump, walk, run)
    */
    UPROPERTY(EditDefaultsOnly, Category = "Animation")
    TMap<FName, UAnimSequence*> Pawn3PAnims;

private:

    /** Weapon mesh: 1st person view */
    UPROPERTY(VisibleDefaultsOnly, Category = "Components")
    USkeletalMeshComponent* Mesh1P;

    /** Weapon mesh: 3rd person view */
    UPROPERTY(VisibleDefaultsOnly, Category = "Components")
    USkeletalMeshComponent* Mesh3P;

protected:

    /** Pawn owner*/
    UPROPERTY(Transient, ReplicatedUsing = OnRep_OwnerPawn)
    ADJVCharacter* OwnerPawn;

    /** Weapon data */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    FWeaponData WeaponConfig;

    /** Should this weapon have recoil and recoil recovery?*/
    UPROPERTY(EditDefaultsOnly, Category = "Weapon")
    bool bHasRecoilAndRecoilRecovery;

    /** Recoil data */
    UPROPERTY(EditDefaultsOnly, Category = "Weapon", meta = (EditCondition = "bHasRecoilAndRecoilRecovery"))
    FWeaponRecoilData RecoilConfig;
};
