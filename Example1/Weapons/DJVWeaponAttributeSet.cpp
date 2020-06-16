// Fill out your copyright notice in the Description page of Project Settings.


#include "DJVWeaponAttributeSet.h"

UDJVWeaponAttributeSet::UDJVWeaponAttributeSet()
{
    WeaponDamage = FGameplayAttributeData(10.f);
    WeaponDamageMultiplier = FGameplayAttributeData(1.f);
    WeaponDamageWeakSpotMultiplier = FGameplayAttributeData(3.f);
}