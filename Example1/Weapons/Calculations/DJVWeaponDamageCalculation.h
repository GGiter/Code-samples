// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "DJVWeaponDamageCalculation.generated.h"

/**
 * Calculates the damage to apply based on a weapons attribute 
 */
UCLASS()
class DEJAVU_API UDJVWeaponDamageCalculation : public UGameplayEffectExecutionCalculation
{
    GENERATED_BODY()

public:

    UDJVWeaponDamageCalculation();

public:

    virtual void Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;


};
