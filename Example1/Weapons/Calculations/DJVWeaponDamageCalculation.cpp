// Fill out your copyright notice in the Description page of Project Settings.


#include "DJVWeaponDamageCalculation.h"
#include "DJVWeaponAttributeSet.h"
#include "DJVCharacterAttributeSet.h"
#include "DJVTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

struct DamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Damage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponDamage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponDamageMultiplier);
    DECLARE_ATTRIBUTE_CAPTUREDEF(WeaponDamageWeakSpotMultiplier);

    DamageStatics()
    {
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDJVCharacterAttributeSet, Damage, Target, false);

        DEFINE_ATTRIBUTE_CAPTUREDEF(UDJVWeaponAttributeSet, WeaponDamage, Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDJVWeaponAttributeSet, WeaponDamageMultiplier, Source, true);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDJVWeaponAttributeSet, WeaponDamageWeakSpotMultiplier, Source, true);
    }
};

static const DamageStatics& GetDamageStatics()
{
    static DamageStatics DmgStatics;
    return DmgStatics;
}

UDJVWeaponDamageCalculation::UDJVWeaponDamageCalculation()
{
    const DamageStatics& DmgStatics = GetDamageStatics();

    RelevantAttributesToCapture.Add(DmgStatics.DamageDef);
    RelevantAttributesToCapture.Add(DmgStatics.WeaponDamageDef);
    RelevantAttributesToCapture.Add(DmgStatics.WeaponDamageMultiplierDef);
    RelevantAttributesToCapture.Add(DmgStatics.WeaponDamageWeakSpotMultiplierDef);

    #if WITH_EDITOR
    InvalidScopedModifierAttributes.Add(DmgStatics.DamageDef);
    #endif
}

void UDJVWeaponDamageCalculation::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const DamageStatics& DmgStatics = GetDamageStatics();

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    FAggregatorEvaluateParameters EvaluationParameters;
    EvaluationParameters.SourceTags = SourceTags;
    EvaluationParameters.TargetTags = TargetTags;

    float WeaponDamage = 0.0f;
    float WeaponDamageMultiplier = 0.0f;
    float WeaponDamageWeakSpotMultiplier = 0.0f;

    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DmgStatics.WeaponDamageDef, EvaluationParameters, WeaponDamage);
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DmgStatics.WeaponDamageMultiplierDef, EvaluationParameters, WeaponDamageMultiplier);
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DmgStatics.WeaponDamageWeakSpotMultiplierDef, EvaluationParameters, WeaponDamageWeakSpotMultiplier);

    const FHitResult* HitResult = Spec.GetEffectContext().GetHitResult();

    // Used when hitting weak spots (ex.: Head)
    float WeakSpotMultiplier = 1.0f;
    
    if (HitResult != nullptr)
    {
        UPhysicalMaterial* HitPhysicalMaterial = HitResult->PhysMaterial.Get();
        EPhysicalSurface HitSurfaceType = UPhysicalMaterial::DetermineSurfaceType(HitPhysicalMaterial);

        if (HitSurfaceType == DEJAVU_SURFACE_Weak_Spot)
            WeakSpotMultiplier = WeaponDamageWeakSpotMultiplier;
    }

    float DamageToApply = WeaponDamage * WeaponDamageMultiplier * WeakSpotMultiplier;

    // Damage should only negate health
    if (DamageToApply > 0.f)
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(DmgStatics.DamageProperty, EGameplayModOp::Additive, DamageToApply));
}
