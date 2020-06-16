// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "FAStarNT.generated.h"

/**
 * 
 */
UCLASS()
class INVADED_API UFAStarNT : public UObject
{
	GENERATED_BODY()
public:
	static TArray<class ACellBase*> GetPath(class ACellBase* StartCell,class ACellBase* TargetCell,class AGridGenerator* GridGenerator);
	static TArray<class ACellBase*> RetracePath(class ACellBase* Start, class ACellBase* Target);
	static float GetDistance(class ACellBase* CellA,class ACellBase* CellB);
};
