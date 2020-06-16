// Fill out your copyright notice in the Description page of Project Settings.


#include "DJVWeaponInstant.h"
#include "DJVCharacter.h"
#include "DJVImpactEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Net/UnrealNetwork.h"

ADJVWeaponInstant::ADJVWeaponInstant() : ADJVWeapon()
{
    CurrentFiringSpread = 0.0f;
}

void ADJVWeaponInstant::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Reduce weapon spread back to 0 only when the weapon is Idle or Reloading
    if (CurrentState == EWeaponState::Idle || CurrentState == EWeaponState::Reloading)
    {
        CurrentFiringSpread =
            FMath::FInterpTo(
            CurrentFiringSpread,
            0.0f,
            DeltaTime,
            InstantConfig.FiringSpreadDecreasePerSecond
            );
    }
}

//////////////////////////////////////////////////////////////////////////
// Weapon usage helpers

float ADJVWeaponInstant::GetCurrentSpread() const
{
    float FinalSpread = InstantConfig.WeaponSpread + CurrentFiringSpread;

    if (OwnerPawn && OwnerPawn->IsAiming())
        FinalSpread *= InstantConfig.AimingSpreadModifier;

    return FinalSpread;
}

//////////////////////////////////////////////////////////////////////////
// Weapon usage

void ADJVWeaponInstant::FireWeapon()
{
    const int32 RandomSeed = FMath::Rand();
    FRandomStream WeaponRandomStream(RandomSeed);

    const float CurrentSpread = GetCurrentSpread();
    const float ConeHalfAngle = FMath::DegreesToRadians(CurrentSpread * 0.5f);

    const FVector AimDir = GetAdjustedAim();
    const FVector StartTrace = GetCameraFireStartLocation(AimDir);

    const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle, ConeHalfAngle);
    const FVector EndTrace = StartTrace + ShootDir * InstantConfig.WeaponRange;

    const FHitResult Impact = SendWeaponTrace(StartTrace, EndTrace);
    ProcessHit(Impact, StartTrace, ShootDir, RandomSeed, CurrentSpread);

    CurrentFiringSpread = FMath::Min(InstantConfig.FiringSpreadMax, CurrentFiringSpread + InstantConfig.FiringSpreadIncrement);
}

void ADJVWeaponInstant::ProcessHit(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir, int32 RandomSeed, float ReticleSpread)
{
    if (OwnerPawn && OwnerPawn->IsLocallyControlled() && GetNetMode() == ENetMode::NM_Client)
    {
        // if we're a client and we've hit something that is being controlled by the server
        if (Impact.GetActor() && Impact.GetActor()->GetRemoteRole() == ENetRole::ROLE_Authority)
        {
            // notify the server of the hit
            ServerNotifyHit(Impact, ShootDir, RandomSeed, ReticleSpread);
        }
        else if (Impact.GetActor() == nullptr)
        {
            if (Impact.bBlockingHit)
            {
                // notify the server of the hit
                ServerNotifyHit(Impact, ShootDir, RandomSeed, ReticleSpread);
            }
            else
            {
                // notify server of the miss
                ServerNotifyMiss(ShootDir, RandomSeed, ReticleSpread);
            }
        }
    }

    // process a confirmed hit
    ProcessHit_Confirmed(Impact, Origin, ShootDir, RandomSeed, ReticleSpread);
}

void ADJVWeaponInstant::ProcessHit_Confirmed(const FHitResult& Impact, const FVector& Origin, const FVector& ShootDir, int32 RandomSeed, float ReticleSpread)
{
    if (ShouldDealDamage(Impact.GetActor()))
    {
        UAbilitySystemComponent* ASC = GetAbilitySystemComponent();

        if (ASC)
        {
            UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Impact.GetActor());

            if (TargetASC)
            {
                FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
                EffectContext.AddSourceObject(this);
                EffectContext.AddHitResult(Impact);

                FGameplayEffectSpecHandle EffectSpec = ASC->MakeOutgoingSpec(FireGameplayEffect, UGameplayEffect::INVALID_LEVEL, EffectContext);

                if (EffectSpec.IsValid())
                    ASC->ApplyGameplayEffectSpecToTarget(*EffectSpec.Data.Get(), TargetASC);
            }
        }
    }

    // Play FX on remote clients
    if (HasAuthority())
    {
        HitNotify.Origin = Origin;
        HitNotify.RandomSeed = RandomSeed;
        HitNotify.ReticleSpread = ReticleSpread;
    }

    // Play FX locally
    if (GetNetMode() != ENetMode::NM_DedicatedServer)
    {
        const FVector EndTrace = Origin + ShootDir * InstantConfig.WeaponRange;
        const FVector EndPoint = Impact.GetActor() ? Impact.ImpactPoint : EndTrace;

        SpawnTrailEffect(EndPoint);
        SpawnImpactEffect(Impact);
    }
}

bool ADJVWeaponInstant::ServerNotifyHit_Validate(const FHitResult& Impact, FVector_NetQuantizeNormal ShootDir, int32 RandomSeed, float ReticleSpread)
{
    return true;
}

void ADJVWeaponInstant::ServerNotifyHit_Implementation(const FHitResult& Impact, FVector_NetQuantizeNormal ShootDir, int32 RandomSeed, float ReticleSpread)
{
    const float WeaponAngleDot = FMath::Abs(FMath::Sin(FMath::DegreesToRadians(ReticleSpread)));

    // If we have an instigator, calculate dot between  the view and the shot
    if (Instigator && (Impact.GetActor() || Impact.bBlockingHit))
    {
        const FVector Origin = GetMuzzleLocation();
        const FVector ViewDir = (Impact.Location - Origin).GetSafeNormal();

        // Is the angle between the hit and the view within allowed limits (limit + weapon max angle)
        const float ViewDotHitDir = FVector::DotProduct(Instigator->GetViewRotation().Vector(), ViewDir);

        if (ViewDotHitDir > InstantConfig.AllowedViewDotHitDir - WeaponAngleDot)
        {
            if (CurrentState != EWeaponState::Idle)
            {
                if (Impact.GetActor() == nullptr)
                {
                    if(Impact.bBlockingHit)
                        ProcessHit_Confirmed(Impact, Origin, ShootDir, RandomSeed, ReticleSpread);
                }
                else if (Impact.GetActor()->IsRootComponentStatic() || Impact.GetActor()->IsRootComponentStationary())
                {
                    // The hit usually doesn't have significant gameplay implications against static things
                    ProcessHit_Confirmed(Impact, Origin, ShootDir, RandomSeed, ReticleSpread);
                }
                else // Let's confirm that the hit was actually within client's bounding box tolerance
                {
                    // Get the component bounding box
                    const FBox HitBox = Impact.GetActor()->GetComponentsBoundingBox();

                    // Calculate the box extent and increase by a leeway
                    FVector BoxExtent = 0.5f * (HitBox.Max - HitBox.Min);
                    BoxExtent *= InstantConfig.ClientSideHitLeeway;

                    // Avoid precision errors with really thin objects
                    BoxExtent.X = FMath::Max(20.0f, BoxExtent.X);
                    BoxExtent.Y = FMath::Max(20.0f, BoxExtent.Y);
                    BoxExtent.Z = FMath::Max(20.0f, BoxExtent.Z);

                    // Get the box center
                    const FVector BoxCenter = (HitBox.Min + HitBox.Max) * 0.5f;

                    // Check whether this hit was within client tolerance
                    if (FMath::Abs(Impact.Location.X - BoxCenter.X) < BoxExtent.X &&
                        FMath::Abs(Impact.Location.Y - BoxCenter.Y) < BoxExtent.Y &&
                        FMath::Abs(Impact.Location.Z - BoxCenter.Z) < BoxExtent.Z)
                    {
                        ProcessHit_Confirmed(Impact, Origin, ShootDir, RandomSeed, ReticleSpread);
                    }
                    else
                        UE_LOG(LogTemp, Log, TEXT("%s Rejected client side hit of %s (outside bounding box tolerance)"), *GetNameSafe(this), *GetNameSafe(Impact.GetActor()));
                }
            }
        }
    }
}

bool ADJVWeaponInstant::ServerNotifyMiss_Validate(FVector_NetQuantizeNormal ShootDir, int32 RandomSeed, float ReticleSpread)
{
    return true;
}

void ADJVWeaponInstant::ServerNotifyMiss_Implementation(FVector_NetQuantizeNormal ShootDir, int32 RandomSeed, float ReticleSpread)
{
    const FVector Origin = GetMuzzleLocation();

    // Play FX on remote clients
    HitNotify.Origin = Origin;
    HitNotify.RandomSeed = RandomSeed;
    HitNotify.ReticleSpread = ReticleSpread;

    // Play FX locally
    if (GetNetMode() != ENetMode::NM_DedicatedServer)
    {
        const FVector EndTrace = Origin + ShootDir * InstantConfig.WeaponRange;
        SpawnTrailEffect(EndTrace);
    }
}

void ADJVWeaponInstant::OnBurstFinished()
{
    Super::OnBurstFinished();
}

bool ADJVWeaponInstant::ShouldDealDamage(AActor* TestActor) const
{
    // If we're an actor on the server, or the actor's role is authoritative, we should register damage
    if (TestActor)
    {
        if (GetNetMode() != ENetMode::NM_Client ||
            TestActor->Role == ENetRole::ROLE_Authority ||
            TestActor->GetTearOff())
        {
            return true;
        }
    }

    return false;
}

//////////////////////////////////////////////////////////////////////////
// Effects & Replication

void ADJVWeaponInstant::OnRep_HitNotify()
{
    SimulateHit(HitNotify.Origin, HitNotify.RandomSeed, HitNotify.ReticleSpread);
}

void ADJVWeaponInstant::SimulateHit(const FVector & ShotOrigin, int32 RandomSeed, float ReticleSpread)
{
    // Let's recreate the effects on this remote client too
    FRandomStream WeaponRandomStream(RandomSeed);
    const float ConeHalfAngle = FMath::DegreesToRadians(ReticleSpread * 0.5f);

    const FVector StartTrace = ShotOrigin;
    const FVector AimDir = GetAdjustedAim();
    const FVector ShootDir = WeaponRandomStream.VRandCone(AimDir, ConeHalfAngle, ConeHalfAngle);
    const FVector EndTrace = StartTrace + ShootDir * InstantConfig.WeaponRange;

    FHitResult Impact = SendWeaponTrace(StartTrace, EndTrace);

    if (Impact.bBlockingHit)
    {
        SpawnImpactEffect(Impact);
        SpawnTrailEffect(Impact.ImpactPoint);
    }
    else
        SpawnTrailEffect(EndTrace);
}

void ADJVWeaponInstant::SpawnTrailEffect(const FVector& EndPoint)
{
    if (TrailFX)
    {
        const FVector Origin = GetMuzzleLocation();

        UParticleSystemComponent* TrailPSC = UGameplayStatics::SpawnEmitterAtLocation(this, TrailFX, Origin);

        if (TrailPSC)
            TrailPSC->SetVectorParameter(TrailTargetParam, EndPoint);
    }
}

void ADJVWeaponInstant::SpawnImpactEffect(const FHitResult& Impact)
{
    if (ImpactTemplate && Impact.bBlockingHit)
    {
        FHitResult UseImpact = Impact;

        // // Trace again to find component in case it was lost during replication
        if (!Impact.Component.IsValid())
        {
            const FVector StartTrace = Impact.ImpactPoint + Impact.ImpactNormal * 10.0f;
            const FVector EndTrace = Impact.ImpactPoint - Impact.ImpactNormal * 10.0f;

            FHitResult Hit = SendWeaponTrace(StartTrace, EndTrace);
            UseImpact = Hit;
        }

        FTransform const SpawnTransform(Impact.ImpactNormal.Rotation(), Impact. ImpactPoint);

        ADJVImpactEffect* ImpactEffectActor = GetWorld()->SpawnActorDeferred<ADJVImpactEffect>(ImpactTemplate, SpawnTransform);
        if (ImpactEffectActor)
        {
            ImpactEffectActor->SetSurfaceHit(Impact);
            UGameplayStatics::FinishSpawningActor(ImpactEffectActor, SpawnTransform);
        }
    }
}

void ADJVWeaponInstant::GetLifetimeReplicatedProps(TArray< FLifetimeProperty > & OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // replicate to everyone except the local client
    DOREPLIFETIME_CONDITION(ADJVWeaponInstant, HitNotify, COND_SkipOwner);
}