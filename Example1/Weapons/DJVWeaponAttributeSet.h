// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "Abilities/AbilityMacros.h"
#include "DJVWeaponAttributeSet.generated.h"

/**
 * Base attribute set for all weapons
 */
UCLASS()
class DEJAVU_API UDJVWeaponAttributeSet : public UAttributeSet
{
    GENERATED_BODY()

public:

    UDJVWeaponAttributeSet();

public:

    /** Damage this weapon deals per hit */
    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    FGameplayAttributeData WeaponDamage;
    ATTRIBUTE_ACCESSORS(UDJVWeaponAttributeSet, WeaponDamage)

    /** Multiplier for dealing damage */
    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    FGameplayAttributeData WeaponDamageMultiplier;
    ATTRIBUTE_ACCESSORS(UDJVWeaponAttributeSet, WeaponDamageMultiplier)

    /** Multiplier for dealing damage on weak spots (ex. Head)*/
    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    FGameplayAttributeData WeaponDamageWeakSpotMultiplier;
    ATTRIBUTE_ACCESSORS(UDJVWeaponAttributeSet, WeaponDamageWeakSpotMultiplier)
};
