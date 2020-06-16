// Fill out your copyright notice in the Description page of Project Settings.


#include "DJVWeapon.h"
#include "DJVTypes.h"
#include "Player/DJVCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "AbilitySystemComponent.h"
#include "DJVWeaponAttributeSet.h"
#include "TimerManager.h"
#include "..\..\Public\Weapons\DJVWeapon.h"

// Sets default values
ADJVWeapon::ADJVWeapon()
{
    USceneComponent* SceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
    RootComponent = SceneComp;

    Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh1P"));
    Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mesh1P->bOnlyOwnerSee = true;
    Mesh1P->bOwnerNoSee = false;
    Mesh1P->bReceivesDecals = false;
    Mesh1P->CastShadow = false;
    Mesh1P->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh1P->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    Mesh1P->SetupAttachment(RootComponent);

    Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh3P"));
    Mesh3P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    Mesh3P->bOnlyOwnerSee = false;
    Mesh3P->bOwnerNoSee = true;
    Mesh3P->bReceivesDecals = false;
    Mesh3P->CastShadow = true;
    Mesh3P->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    Mesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh3P->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    Mesh3P->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
    Mesh3P->SetupAttachment(RootComponent);

    bLoopedFireAnim = false;
    bPlayingFireAnim = false;
    bEquipped = false;
    bWantsToFire = false;
    bPendingReload = false;
    bPendingEquip = false;
    bShooting = false;
    bHasRecoilAndRecoilRecovery = true;

    CurrentState = EWeaponState::Idle;

    ShotsCount = 0;
    CurrentAmmo = 0;
    CurrentAmmoInClip = 0;
    BurstCounter = 0;
    LastFireTime = 0.0f;

    RecoilRecoveryStep = 0.01f;
    RecoilRecoveryVerticalValue = 0.0f;
    RecoilRecoveryHorizontalValue = 0.0f;
    RecoilRecoveryVerticalStep = 0.0f;
    RecoilRecoveryHorizontalStep = 0.0f;

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = ETickingGroup::TG_PrePhysics;

    // Create ability system component, and set it to be explicitly replicated
    AbilitySystem = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilitySystem->SetIsReplicated(true);

    AttributeSet = CreateDefaultSubobject<UDJVWeaponAttributeSet>(TEXT("Attributes"));

    SetRemoteRoleForBackwardsCompat(ENetRole::ROLE_SimulatedProxy);

    bReplicates = true;
    bNetUseOwnerRelevancy = true;
}

void ADJVWeapon::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (WeaponConfig.InitialClips)
    {
        CurrentAmmoInClip = WeaponConfig.AmmoPerClip;
        CurrentAmmo = WeaponConfig.AmmoPerClip * WeaponConfig.InitialClips;
    }

    DetachMeshFromPawn();

    TimeBetweenShots = 60.0f / FMath::Max(1.0f, WeaponConfig.RateOfFire);

    RecoilInterpolationStep = TimeBetweenShots / 5.0f;

    if (HasAuthority() && AbilitySystem && AttributeDefaults)
        InitializeAttributeDefaults();
}

void ADJVWeapon::BeginPlay()
{
    Super::BeginPlay();

    if (AbilitySystem)
        AbilitySystem->InitAbilityActorInfo(this, this);
}

void ADJVWeapon::Destroyed()
{
    Super::Destroyed();

    StopWeaponFireEffects();
}

/////////////////////////////////////////////////////////////////////////
// Control

bool ADJVWeapon::CanFire() const
{
    bool bCanFire = OwnerPawn && OwnerPawn->CanFire();
    bool bStateOKToFire = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
    return (bCanFire && bStateOKToFire && !bPendingReload);
}

bool ADJVWeapon::CanReload() const
{
    bool bCanReload = (!OwnerPawn || OwnerPawn->CanReload());
    bool bGotAmmo = (CurrentAmmoInClip < WeaponConfig.AmmoPerClip) && (CurrentAmmo - CurrentAmmoInClip > 0 || HasInfiniteAmmo());
    bool bStateOKToReload = ((CurrentState == EWeaponState::Idle) || (CurrentState == EWeaponState::Firing));
    return (bCanReload && bGotAmmo && bStateOKToReload);
}

//////////////////////////////////////////////////////////////////////////
// Weapon Usage

void ADJVWeapon::UseAmmo()
{
    if (!HasInfiniteClip())
        CurrentAmmoInClip--;

    if (!HasInfiniteAmmo())
        CurrentAmmo--;
}

void ADJVWeapon::HandleFiring()
{
    if ((CurrentAmmoInClip > 0 || HasInfiniteClip()) && CanFire())
    {
        // Don't play weapon effects on a dedicated server
        if (GetNetMode() != ENetMode::NM_DedicatedServer)
            PlayWeaponFireEffects();

        if (OwnerPawn && OwnerPawn->IsLocallyControlled())
        {
            bShooting = true;

            FireWeapon();

            UseAmmo();

            // Start Weapon Recoil
            ClientHandleRecoil(bHasRecoilAndRecoilRecovery);

            if (WeaponConfig.ShootingMode == EShootingMode::BurstFire)
                ShotsCount++;

            // Update firing FX on remote clients if function was called on server
            BurstCounter++;
        }
    }
    else if (CanReload())
    {
        ShotsCount = WeaponConfig.BurstFireShotsBeforePause;
        StartReload();
    }
    else if (OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        // Stop weapon fire FX, but stay in Firing state
        if (BurstCounter > 0)
            OnBurstFinished();
    }

    if (OwnerPawn && OwnerPawn->IsLocallyControlled())
    {
        // Local client will notify server
        if (!HasAuthority())
            ServerHandleFiring();

        // Reload after firing last round
        if (CurrentAmmoInClip <= 0 && CanReload())
        {
            ShotsCount = WeaponConfig.BurstFireShotsBeforePause;

            StartReload();
        }

        // Setup refire timer
        bRefiring = (CurrentState == EWeaponState::Firing
                     && WeaponConfig.ShootingMode == EShootingMode::AutoFire 
                     && TimeBetweenShots);

        if (!bRefiring)
            bRefiring = CurrentState == EWeaponState::Firing
            && WeaponConfig.ShootingMode == EShootingMode::BurstFire;

        if (WeaponConfig.ShootingMode == EShootingMode::SingleFire ||
            (WeaponConfig.ShootingMode == EShootingMode::BurstFire &&
            ShotsCount >= WeaponConfig.BurstFireShotsBeforePause))
        {
            bRefiring = false;
            FTimerHandle TimerHandle_OnBurstFinished;
            GetWorldTimerManager().SetTimer(TimerHandle_OnBurstFinished, this, &ADJVWeapon::OnBurstFinished, FMath::Max<float>(TimeBetweenShots + TimerIntervalAdjustment, SMALL_NUMBER), false);
        }

        if (bRefiring)
        {
            GetWorldTimerManager().SetTimer(TimerHandle_HandleRefiring, this, &ADJVWeapon::HandleReFiring, FMath::Max<float>(TimeBetweenShots + TimerIntervalAdjustment, SMALL_NUMBER), false);
            TimerIntervalAdjustment = 0.f;
        }
    }

    LastFireTime = GetWorld()->GetTimeSeconds();
}

void ADJVWeapon::HandleReFiring()
{
    float CatchupTime = FMath::Max(0.0f, (GetWorld()->TimeSeconds - LastFireTime) - TimeBetweenShots);

    if (bAllowAutomaticWeaponCatchup)
        TimerIntervalAdjustment -= CatchupTime;

    HandleFiring();
}

bool ADJVWeapon::ServerHandleFiring_Validate()
{
    return true;
}

void ADJVWeapon::ServerHandleFiring_Implementation()
{
    const bool bShouldUpdateAmmo = (CurrentAmmoInClip > 0 && CanFire());

    HandleFiring();

    if (bShouldUpdateAmmo)
    {
        // Update ammo
        UseAmmo();

        // Update firing FX on remote clients
        BurstCounter++;
    }
}

void ADJVWeapon::OnBurstStarted()
{
    if (!bShooting)
    {
        ClientStopRecoilRecovery();

        // Start firing, can be delayed to satisfy TimeBetweenShots
        const float GameTime = GetWorld()->GetTimeSeconds();

        if ((LastFireTime && TimeBetweenShots) && ((LastFireTime + TimeBetweenShots) > GameTime))
        {
            GetWorldTimerManager().SetTimer(TimerHandle_HandleFiring, this, &ADJVWeapon::HandleFiring, LastFireTime + TimeBetweenShots - GameTime, false);
        }
        else
            HandleFiring();
    }
}

void ADJVWeapon::OnBurstFinished()
{
    // Don't stop the burst until the weapon has fully finished its burst fire
    if (CurrentState != EWeaponState::Reloading && WeaponConfig.ShootingMode == EShootingMode::BurstFire)
        if (ShotsCount < WeaponConfig.BurstFireShotsBeforePause)
            return;

    // stop firing FX on remote clients
    BurstCounter = 0;

    // stop firing FX locally, unless it's a dedicated server
    if (GetNetMode() != ENetMode::NM_DedicatedServer)
        StopWeaponFireEffects();

    bRefiring = false;

    GetWorldTimerManager().ClearTimer(TimerHandle_HandleFiring);
    GetWorldTimerManager().ClearTimer(TimerHandle_HandleRefiring);

    ClientStopRecoil();

    if (bHasRecoilAndRecoilRecovery && bShooting)
    {
        FTimerHandle TimerHandle_RecoilRecoveryDelay;
        GetWorldTimerManager().SetTimer(TimerHandle_RecoilRecoveryDelay, this, &ADJVWeapon::ClientStartRecoilRecoveryDelay, RecoilConfig.RecoveryDelay);
    }

    bShooting = false;

    if (WeaponConfig.ShootingMode == EShootingMode::BurstFire)
    {
        if (!bWantsToFire && CurrentState != EWeaponState::Reloading)
        {
            // Set it directly otherwise it might trigger stack overflow
            CurrentState = EWeaponState::Idle;
        }

        ShotsCount = 0;
    }

    // Reset firing interval adjustment
    TimerIntervalAdjustment = 0.0f;
}

void ADJVWeapon::ReloadWeapon()
{
    int32 ClipDelta = FMath::Min(WeaponConfig.AmmoPerClip - CurrentAmmoInClip, CurrentAmmo - CurrentAmmoInClip);

    if (HasInfiniteClip())
        ClipDelta = WeaponConfig.AmmoPerClip - CurrentAmmoInClip;

    if (ClipDelta > 0)
        CurrentAmmoInClip += ClipDelta;

    if (HasInfiniteClip())
        CurrentAmmo = FMath::Max(CurrentAmmoInClip, CurrentAmmo);
}

void ADJVWeapon::DetermineWeaponState()
{
    EWeaponState::Type NewState = EWeaponState::Idle;

    if (bEquipped)
    {
        if (bPendingReload)
        {
            if (!CanReload())
                NewState = CurrentState;
            else
                NewState = EWeaponState::Reloading;
        }
        else if (!bPendingReload && bWantsToFire && CanFire())
            NewState = EWeaponState::Firing;
    }
    else if (bPendingEquip)
        NewState = EWeaponState::Equipping;

    SetWeaponState(NewState);
}

void ADJVWeapon::SetWeaponState(EWeaponState::Type NewState)
{
    const EWeaponState::Type PrevState = CurrentState;

    if (bShooting && PrevState == EWeaponState::Firing && NewState != EWeaponState::Reloading && WeaponConfig.ShootingMode == EShootingMode::BurstFire &&
        ShotsCount < WeaponConfig.BurstFireShotsBeforePause)
        return;

    if (PrevState == EWeaponState::Firing && NewState != EWeaponState::Firing)
        OnBurstFinished();

    CurrentState = NewState;

    if (PrevState != EWeaponState::Firing && NewState == EWeaponState::Firing)
        OnBurstStarted();
}

//////////////////////////////////////////////////////////////////////////
// Weapon usage helpers

float ADJVWeapon::PlayAnimation(const FWeaponAnim& Animation)
{
    float Duration = 0.0f;
    if (OwnerPawn)
    {
        const bool bIsFirstPerson = OwnerPawn->IsFirstPerson();

        UAnimMontage* UseAnim = bIsFirstPerson ? Animation.Pawn1P : Animation.Pawn3P;

        if (UseAnim)
            Duration = OwnerPawn->PlayAnimMontage(UseAnim);

        if (Animation.bHasWeaponAnims)
        {
            float TempDuration = PlayWeaponAnimation(Animation, bIsFirstPerson);

            if (TempDuration > Duration)
                Duration = TempDuration;
        }
    }

    return Duration;
}

void ADJVWeapon::StopAnimation(const FWeaponAnim& Animation)
{
    if (OwnerPawn)
    {
        const bool bIsFirstPerson = OwnerPawn->IsFirstPerson();

        UAnimMontage* UseAnim = bIsFirstPerson ? Animation.Pawn1P : Animation.Pawn3P;

        if (UseAnim)
            OwnerPawn->StopAnimMontage(UseAnim);

        if(Animation.bHasWeaponAnims)
            StopWeaponAnimation(Animation, bIsFirstPerson);
    }
}

float ADJVWeapon::PlayWeaponAnimation(const FWeaponAnim& Animation, bool bIsFirstPerson /*= true*/)
{
    float Duration = 0.5f;

    USkeletalMeshComponent* UseMesh = bIsFirstPerson ? Mesh1P : Mesh3P;
    UAnimSequence* UseAnim = bIsFirstPerson ? Animation.Weapon1P : Animation.Weapon3P;

    if (UseMesh && UseAnim)
    {
        UseMesh->PlayAnimation(UseAnim, false);
        Duration = UseAnim->RateScale * UseAnim->GetPlayLength();
    }

    return Duration;
}

void ADJVWeapon::StopWeaponAnimation(const FWeaponAnim& Animation, bool bIsFirstPerson /*= true*/)
{
    USkeletalMeshComponent* UseMesh = bIsFirstPerson ? Mesh1P : Mesh3P;
    UAnimSequence* UseAnim = bIsFirstPerson ? Animation.Weapon1P : Animation.Weapon3P;

    if (UseMesh && UseAnim)
        UseMesh->PlayAnimation(nullptr, false);
}

FVector ADJVWeapon::GetMuzzleLocation() const
{
    USkeletalMeshComponent* UseMesh = GetWeaponMesh();
    return UseMesh->GetSocketLocation(MuzzleAttachPoint);
}

FVector ADJVWeapon::GetMuzzleRotation() const
{
    USkeletalMeshComponent* UseMesh = GetWeaponMesh();
    return UseMesh->GetSocketRotation(MuzzleAttachPoint).Vector();
}

FVector ADJVWeapon::GetAdjustedAim() const
{
    APlayerController* const PlayerController = Instigator ? Cast<APlayerController>(Instigator->Controller) : nullptr;

    FVector FinalAim = FVector::ZeroVector;

    if (PlayerController)
    {
        FVector CameraLocation;
        FRotator CameraRotation;

        PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);

        FinalAim = CameraRotation.Vector();
    }
    else if (Instigator)
        FinalAim = Instigator->GetBaseAimRotation().Vector();

    return FinalAim;
}

FVector ADJVWeapon::GetCameraFireStartLocation(const FVector& AimDir) const
{
    APlayerController const* PlayerController = Instigator ? Cast<APlayerController>(OwnerPawn->Controller) : nullptr;
    FVector OutStartTrace = FVector::ZeroVector;

    if (PlayerController)
    {
        // use player's camera
        FRotator TempRotation;
        PlayerController->GetPlayerViewPoint(OutStartTrace, TempRotation);

        // Adjust trace so there is nothing blocking the ray between the camera and the pawn, and calculate distance from adjusted start
        OutStartTrace = OutStartTrace + AimDir * ((Instigator->GetActorLocation() - OutStartTrace) | AimDir);
    }

    return OutStartTrace;
}

FHitResult ADJVWeapon::SendWeaponTrace(const FVector& StartTrace, const FVector& EndTrace) const
{
    // Perform trace to retrieve hit info
    FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(SendWeaponTrace), true, Instigator);
    TraceParams.bReturnPhysicalMaterial = true;

    FHitResult Result(ForceInit);
    GetWorld()->LineTraceSingleByChannel(Result, StartTrace, EndTrace, COLLISION_WEAPON, TraceParams);

    return Result;
}

//////////////////////////////////////////////////////////////////////////
// Weapon Equip

void ADJVWeapon::OnEquip(ADJVWeapon* LastWeapon)
{
    AttachMeshToPawn();

    bPendingEquip = true;

    DetermineWeaponState();

    // Only play animation if last weapon is valid
    if (LastWeapon)
    {
        float Duration = PlayAnimation(EquipAnim);
        if (Duration <= 0.0f)
        {
            // failsafe
            Duration = 0.5f;
        }

        EquipStartedTime = GetWorld()->GetTimeSeconds();
        EquipDuration = Duration;

        GetWorldTimerManager().SetTimer(TimerHandle_OnEquipFinished, this, &ADJVWeapon::OnEquipFinished, Duration, false);
    }
    else
        OnEquipFinished();
}

void ADJVWeapon::OnUnEquip()
{
    DetachMeshFromPawn();
    bEquipped = false;
    StopFire();

    if (bPendingReload)
    {
        StopAnimation(ReloadAnim);
        bPendingReload = false;

        GetWorldTimerManager().ClearTimer(TimerHandle_StopReload);
        GetWorldTimerManager().ClearTimer(TimerHandle_ReloadWeapon);
    }

    if (bPendingEquip)
    {
        StopAnimation(EquipAnim);
        bPendingEquip = false;

        GetWorldTimerManager().ClearTimer(TimerHandle_OnEquipFinished);
    }

    DetermineWeaponState();
}

void ADJVWeapon::OnEnterInventory(ADJVCharacter* NewOwner)
{
    SetOwningPawn(NewOwner);
}

void ADJVWeapon::OnLeaveInventory()
{
    if (IsAttachedToPawn())
        OnUnEquip();

    if (HasAuthority())
        SetOwningPawn(nullptr);
}

void ADJVWeapon::OnEquipFinished()
{
    AttachMeshToPawn();

    bEquipped = true;
    bPendingEquip = false;

    // Update state
    DetermineWeaponState();

    if (OwnerPawn)
    {
        // try to reload empty clip
        if (OwnerPawn->IsLocallyControlled() && CurrentAmmoInClip <= 0 && CanReload())
            StartReload();
    }
}

void ADJVWeapon::AttachMeshToPawn()
{
    if (OwnerPawn)
    {
        // Remove and hide both first and third person meshes
        DetachMeshFromPawn();

        // For locally controller players we attach both weapons and let bOnlyOwnerSee, bOwnerNoSee flags deal with visibility.
        FName WeaponAttachPoint = OwnerPawn->GetWeaponAttachPoint();
        if (OwnerPawn->IsLocallyControlled())
        {
            USkeletalMeshComponent* PawnMesh1P = OwnerPawn->GetSpecificPawnMesh(true);
            USkeletalMeshComponent* PawnMesh3P = OwnerPawn->GetSpecificPawnMesh(false);

            Mesh1P->SetHiddenInGame(false);
            Mesh3P->SetHiddenInGame(false);
            Mesh1P->AttachToComponent(PawnMesh1P, FAttachmentTransformRules::KeepRelativeTransform, WeaponAttachPoint);
            Mesh3P->AttachToComponent(PawnMesh3P, FAttachmentTransformRules::KeepRelativeTransform, WeaponAttachPoint);
        }
        else
        {
            USkeletalMeshComponent* UseWeaponMesh = GetWeaponMesh();
            USkeletalMeshComponent* UsePawnMesh = OwnerPawn->GetPawnMesh();
            UseWeaponMesh->AttachToComponent(UsePawnMesh, FAttachmentTransformRules::KeepRelativeTransform, WeaponAttachPoint);
            UseWeaponMesh->SetHiddenInGame(false);
        }
    }
}

void ADJVWeapon::DetachMeshFromPawn()
{
    Mesh1P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
    Mesh1P->SetHiddenInGame(true);

    Mesh3P->DetachFromComponent(FDetachmentTransformRules::KeepRelativeTransform);
    Mesh3P->SetHiddenInGame(true);
}

//////////////////////////////////////////////////////////////////////////
// Animation

bool ADJVWeapon::HasAnimation(FName AnimationName, bool bForFirstPerson /*= true*/)
{
    return bForFirstPerson ? Pawn1PAnims.Contains(AnimationName) : Pawn3PAnims.Contains(AnimationName);
}

UAnimSequence * ADJVWeapon::GetAnimation(FName AnimationName, bool bWantsFirstPerson /*= true*/)
{
    UAnimSequence* Animation = nullptr;

    if (HasAnimation(AnimationName, bWantsFirstPerson))
        Animation = bWantsFirstPerson ? Pawn1PAnims[AnimationName] : Pawn3PAnims[AnimationName];

    return Animation;
}

//////////////////////////////////////////////////////////////////////////
// Weapon Helpers

void ADJVWeapon::SetOwningPawn(ADJVCharacter* NewOwner)
{
    if (OwnerPawn != NewOwner)
    {
        OwnerPawn = NewOwner;
        Instigator = NewOwner;

        // Net owner for RPC calls
        SetOwner(NewOwner);
    }
}

//////////////////////////////////////////////////////////////////////////
// Input

void ADJVWeapon::StartFire()
{
    if (!HasAuthority())
        ServerStartFire();

    if (!bWantsToFire)
    {
        bWantsToFire = true;

        DetermineWeaponState();
    }
}

void ADJVWeapon::StopFire()
{
    if ((!HasAuthority()) && OwnerPawn && OwnerPawn->IsLocallyControlled())
        ServerStopFire();

    if (bWantsToFire)
    {
        bWantsToFire = false;

        DetermineWeaponState();
    }
}

void ADJVWeapon::StartReload(bool bFromReplication)
{
    if (!bFromReplication && !HasAuthority())
        ServerStartReload();

    if (bFromReplication || CanReload())
    {
        bPendingReload = true;

        DetermineWeaponState();

        float AnimDuration = PlayAnimation(ReloadAnim);

        if (AnimDuration <= 0.0f)
            AnimDuration = WeaponConfig.NoAnimReloadDuration;

        GetWorldTimerManager().SetTimer(TimerHandle_StopReload, this, &ADJVWeapon::StopReload, AnimDuration, false);

        if (HasAuthority())
        {
            bReloading = true;
            GetWorldTimerManager().SetTimer(TimerHandle_ReloadWeapon, this, &ADJVWeapon::ReloadWeapon, FMath::Max(0.1f, AnimDuration - 0.1f), false);
        }
    }
}

void ADJVWeapon::StopReload()
{
    if (CurrentState == EWeaponState::Reloading)
    {
        bReloading = false;
        bPendingReload = false;
        DetermineWeaponState();

        StopAnimation(ReloadAnim);
    }
}

//////////////////////////////////////////////////////////////////////////
// Input - server side

bool ADJVWeapon::ServerStartFire_Validate()
{
    return true;
}

void ADJVWeapon::ServerStartFire_Implementation()
{
    StartFire();
}

bool ADJVWeapon::ServerStopFire_Validate()
{
    return true;
}

void ADJVWeapon::ServerStopFire_Implementation()
{
    StopFire();
}

bool ADJVWeapon::ServerStartReload_Validate()
{
    return true;
}

void ADJVWeapon::ServerStartReload_Implementation()
{
    StartReload();
}

bool ADJVWeapon::ServerStopReload_Validate()
{
    return true;
}

void ADJVWeapon::ServerStopReload_Implementation()
{
    StopReload();
}

//////////////////////////////////////////////////////////////////////////
// Recoil

void ADJVWeapon::ClientHandleRecoil_Implementation(bool bRecoilOn /*= true*/)
{
    // Stop previous recoil timer
    ClientStopRecoilInterpolation();

    if (bRecoilOn)
    {
        StartRecoilTimePerShot = FinishRecoilTimePerShot;
        FinishRecoilTimePerShot = StartRecoilTimePerShot + TimeBetweenShots;

        ClientInterpolateRecoil();

        GetWorldTimerManager().SetTimer(TimerHandle_InterpolateRecoil, this, &ADJVWeapon::ClientInterpolateRecoil, TimeBetweenShots / 5.0f, true);
    }
}

void ADJVWeapon::ClientInterpolateRecoil_Implementation()
{
    if (bCheckFirstInterpolation)
    {
        // Interpolation Range Calculation before interpolation step
        StartRecoilTimePerShot += RecoilInterpolationStep;

        RecoilSteps += 1;

        // Stop interpolation event if 5 steps are done
        if (RecoilSteps == 6)
        {
            ClientStopRecoilInterpolation();

            if (bRecoilForceStop)
            {
                bRecoilForceStop = false;

                FinishRecoilTimePerShot = 0.0f;
                StartRecoilTimePerShot = 0.0f;

                OldInterpolationVerticalRecoil = 0.0f;
                OldInterpolationHorizontalRecoil = 0.0f;
            }
        }
        else
           UpdateRecoilAndControllerRotation();
    }
    else
    {
        // Reset Recoil
        bCheckFirstInterpolation = true;
        RecoilSteps = 1;

        UpdateRecoilAndControllerRotation();
    }
}

void ADJVWeapon::ClientStopRecoil_Implementation()
{
    bRecoilForceStop = true;
}

void ADJVWeapon::UpdateRecoilAndControllerRotation()
{
    float VerticalRecoilDelta = 0.0f;

    if (RecoilConfig.VerticalCurve)
        VerticalRecoilDelta = RecoilConfig.VerticalCurve->GetFloatValue(StartRecoilTimePerShot);

    float HorizontalRecoilDelta = 0.0f;

    if (RecoilConfig.HorizontalCurve)
        HorizontalRecoilDelta = RecoilConfig.HorizontalCurve->GetFloatValue(StartRecoilTimePerShot);

    CalculateRecoilCoefficient(VerticalRecoilDelta, HorizontalRecoilDelta);
    CalculateRecoilInterpolationStep(VerticalRecoilDelta, VerticalRecoilDelta);
}

void ADJVWeapon::CalculateRecoilCoefficient(float& VerticalRecoilDelta, float& HorizontalRecoilDelta)
{
    // If the character is moving apply extra recoil
    if (OwnerPawn && OwnerPawn->IsMoving())
    {
        VerticalRecoilDelta *= RecoilConfig.MoveCoefficient;
        HorizontalRecoilDelta *= RecoilConfig.MoveCoefficient;
    }

    // If the character is Aiming Down Sights 
    if (OwnerPawn && OwnerPawn->IsAiming())
    {
        VerticalRecoilDelta *= RecoilConfig.AimCoefficient;
        HorizontalRecoilDelta *= RecoilConfig.AimCoefficient;
    }
}

void ADJVWeapon::CalculateRecoilInterpolationStep(float& InterpolationVerticalRecoil, float& InterpolationHorizontalRecoil)
{
    // Calculate Interpolation step based on previous values
    NewInterpolationVerticalRecoil = InterpolationVerticalRecoil;
    NewInterpolationHorizontalRecoil = InterpolationHorizontalRecoil;

    float LocalRecoilVerticalDelta = NewInterpolationVerticalRecoil - OldInterpolationVerticalRecoil;
    float LocalRecoilHorizontalDelta = NewInterpolationHorizontalRecoil - OldInterpolationVerticalRecoil;

    RecoilRecoveryVerticalValue += LocalRecoilVerticalDelta;
    RecoilRecoveryHorizontalValue += LocalRecoilHorizontalDelta;

    // Calculate and apply rotation to character
    if (OwnerPawn)
    {
        APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());

        if (PC)
        {
            PC->AddPitchInput(-LocalRecoilVerticalDelta);
            PC->AddYawInput(LocalRecoilHorizontalDelta);
        }
    }

    OldInterpolationVerticalRecoil = NewInterpolationVerticalRecoil;
    OldInterpolationHorizontalRecoil = NewInterpolationHorizontalRecoil;
}

void ADJVWeapon::ClientStartRecoilRecoveryDelay_Implementation()
{
    ClientStartRecoilRecovery();

    GetWorldTimerManager().SetTimer(TimerHandle_StartRecoilRecovery, this, &ADJVWeapon::ClientStartRecoilRecovery, RecoilRecoveryStep, true);
}

void ADJVWeapon::ClientStopRecoilInterpolation_Implementation()
{
    GetWorldTimerManager().ClearTimer(TimerHandle_InterpolateRecoil);

    bCheckFirstInterpolation = false;
}

//////////////////////////////////////////////////////////////////////////
// Recoil Recovery

void ADJVWeapon::ClientStartRecoilRecovery_Implementation()
{
    // Update Horizontal Recoil Recovery Step
    if (RecoilRecoveryHorizontalValue > 0.0f)
        RecoilRecoveryHorizontalStep = RecoilConfig.RecoverHorizontalSpeed * RecoilRecoveryStep;
    else if (RecoilRecoveryHorizontalValue == 0.0f)
        RecoilRecoveryHorizontalStep = 0.0f;
    else
        RecoilRecoveryHorizontalStep = (RecoilConfig.RecoverHorizontalSpeed * RecoilRecoveryStep) * -1;

    // Update Vertical Recoil Recovery Step
    RecoilRecoveryVerticalStep = RecoilConfig.RecoverVerticalSpeed * RecoilRecoveryStep;

    // Update Vertical And Horizontal Recoil Recovery based on Recoil Step
    // and also apply it on the Controller
    if (RecoilRecoveryVerticalValue > RecoilRecoveryVerticalStep)
    {
        RecoilRecoveryVerticalValue -= RecoilRecoveryVerticalStep;
        RecoilRecoveryHorizontalValue -= RecoilRecoveryHorizontalStep;

        ApplyRecoilRecoveryOnController(RecoilRecoveryVerticalStep, RecoilRecoveryHorizontalStep);
    }
    else
    {
        // If the recovery is done apply once more horizontal and vertical recovery values
        // on Controller and Stop Recoil Recovery
        ApplyRecoilRecoveryOnController(RecoilRecoveryVerticalStep, RecoilRecoveryHorizontalStep);

        RecoilRecoveryVerticalValue = 0.0f;
        RecoilRecoveryHorizontalValue = 0.0f;

        ClientStopRecoilRecovery();
    }
}

void ADJVWeapon::ClientStopRecoilRecovery_Implementation()
{
    GetWorldTimerManager().ClearTimer(TimerHandle_StartRecoilRecovery);

    RecoilRecoveryVerticalValue = 0.0f;
    RecoilRecoveryHorizontalValue = 0.0f;
}

void ADJVWeapon::ApplyRecoilRecoveryOnController(float VerticalRecoveryRecoilDelta, float HorizontalRecoveryRecoilDelta)
{
    if (OwnerPawn)
    {
        APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());

        if (PC)
        {
            PC->AddPitchInput(VerticalRecoveryRecoilDelta);
            PC->AddYawInput(-HorizontalRecoveryRecoilDelta);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
// Ability System

void ADJVWeapon::InitializeAttributeDefaults()
{
    if (AbilitySystem)
        AbilitySystem->InitStats(UDJVWeaponAttributeSet::StaticClass(), AttributeDefaults);
}

//////////////////////////////////////////////////////////////////////////
// Replication & Effects

void ADJVWeapon::OnRep_OwnerPawn()
{
    if (OwnerPawn)
        OnEnterInventory(OwnerPawn);
    else
        OnLeaveInventory();
}

void ADJVWeapon::OnRep_Reload()
{
    if (bPendingReload)
        StartReload(true);
    else
        StopReload();
}

void ADJVWeapon::OnRep_BurstCounter()
{
    if (BurstCounter > 0)
        PlayWeaponFireEffects();
    else
        StopWeaponFireEffects();
}

void ADJVWeapon::PlayWeaponFireEffects()
{
    if (HasAuthority() && CurrentState != EWeaponState::Firing)
        return;

    if (bLoopedFireAnim || !bPlayingFireAnim)
    {
        PlayAnimation(FireAnim);
        bPlayingFireAnim = true;
    }

    APlayerController* PC = (OwnerPawn != nullptr) ? Cast<APlayerController>(OwnerPawn->Controller) : nullptr;
    if (PC != NULL && PC->IsLocalController())
    {
        if (FireCameraShake != NULL)
            PC->ClientPlayCameraShake(FireCameraShake, 1);

        if (BarrelSmokeFX)
            UGameplayStatics::SpawnEmitterAttached(BarrelSmokeFX, Mesh1P, BarrelSmokeAttachPoint, FVector(ForceInit), FRotator(0.0f, 180.0f, 0.0f));

        if (ShellsFX)
            UGameplayStatics::SpawnEmitterAttached(ShellsFX, Mesh1P, ShellsAttachPoint, FVector(ForceInit), FRotator(0.0f, 180.0f, 0.0f));
    }
}

void ADJVWeapon::StopWeaponFireEffects()
{
    if (bLoopedFireAnim && bPlayingFireAnim)
    {
        StopAnimation(FireAnim);
        bPlayingFireAnim = false;
    }
}

void ADJVWeapon::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADJVWeapon, OwnerPawn);

    DOREPLIFETIME_CONDITION(ADJVWeapon, CurrentAmmo, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ADJVWeapon, CurrentAmmoInClip, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ADJVWeapon, bShooting, COND_OwnerOnly);
    DOREPLIFETIME_CONDITION(ADJVWeapon, bReloading, COND_OwnerOnly);

    DOREPLIFETIME_CONDITION(ADJVWeapon, BurstCounter, COND_SkipOwner);
    DOREPLIFETIME_CONDITION(ADJVWeapon, bPendingReload, COND_SkipOwner);
}

//////////////////////////////////////////////////////////////////////////
// Reading data

bool ADJVWeapon::IsAttachedToPawn() const
{
    return bEquipped || bPendingEquip;
}

EWeaponState::Type ADJVWeapon::GetCurrentState() const
{
    return CurrentState;
}

USkeletalMeshComponent* ADJVWeapon::GetWeaponMesh() const
{
    return (OwnerPawn && OwnerPawn->IsFirstPerson()) ? Mesh1P : Mesh3P;
}

bool ADJVWeapon::HasInfiniteAmmo() const
{
    return WeaponConfig.bInfiniteAmmo;
}

bool ADJVWeapon::HasInfiniteClip() const
{
    return WeaponConfig.bInfiniteClip;
}

bool ADJVWeapon::IsReloading() const
{
    return bReloading;
}

UAbilitySystemComponent* ADJVWeapon::GetAbilitySystemComponent() const
{
    return AbilitySystem;
}