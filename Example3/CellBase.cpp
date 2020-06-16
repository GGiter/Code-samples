// Fill out your copyright notice in the Description page of Project Settings.

#include "CellBase.h"



// Sets default values
ACellBase::ACellBase()
{

}

void ACellBase::SetGCost(uint16 Cost)
{
	GCost = Cost;
}

void ACellBase::SetHCost(uint16 Cost)
{
	HCost = Cost;
}

void ACellBase::SetParent(ACellBase * Cell)
{
	Parent = Cell;
}

int16 ACellBase::CompareCells(ACellBase * Cell)
{
	if (GetFCost() == Cell->GetFCost())
	{
		if (HCost > Cell->GetHCost())
			return -1;
		else if (HCost < Cell->GetHCost())
			return 1;
		else
			return 0;
	}
	else
	{
		if (GetFCost() > Cell->GetFCost())
			return -1;
		else if (GetFCost() < Cell->GetFCost())
			return 1;
	}
	return 0;
}
