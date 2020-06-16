// Fill out your copyright notice in the Description page of Project Settings.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CellBase.generated.h"


UCLASS()
class INVADED_API ACellBase : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ACellBase();

	//A*Star alg
	uint16 GetFCost(){ return GCost + HCost; }
	uint16 GetHCost() const { return HCost; }
	uint16 GetGCost() const { return GCost; }
	
	void SetGCost(uint16 Cost);
	
	void SetHCost(uint16 Cost);
	
	void SetParent(ACellBase* Cell);

	int16 CompareCells(ACellBase* Cell);

	ACellBase* GetParent() const { return Parent; }

protected:
	//A*Star alg
	uint16 GCost;

	uint16 HCost;

	ACellBase* Parent;
	
};
