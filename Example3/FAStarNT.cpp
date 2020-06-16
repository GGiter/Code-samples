// Fill out your copyright notice in the Description page of Project Settings.

#include "FAStarNT.h"
#include "Actors/CellBase.h"
#include "Algo/Reverse.h"
#include "Environment/GridGenerator.h"

struct FAStarComparator
{
	FORCEINLINE bool operator()(const ACellBase& A,const ACellBase& B) const { return A.GetHCost() + A.GetGCost() > B.GetHCost() + B.GetGCost(); }
};
struct FCellWrapper
{
public:
	ACellBase* Cell;
	FCellWrapper(ACellBase* Value)
	{
		Cell = Value;
	}
	FCellWrapper()
	{
		Cell = nullptr;
	}
	
};
bool operator==(const FCellWrapper& A,const FCellWrapper& B) { return A.Cell == B.Cell; }
bool operator<(const FCellWrapper& A, const FCellWrapper& B) { return A.Cell->GetFCost() < B.Cell->GetFCost(); }
bool operator>(const FCellWrapper& A, const FCellWrapper& B) { return A.Cell->GetFCost() > B.Cell->GetFCost(); }
TArray<ACellBase*> UFAStarNT::GetPath(ACellBase* StartCell,ACellBase* TargetCell,AGridGenerator* GridGenerator)
{
	TArray<FCellWrapper> OpenSet;

	OpenSet.Add(FCellWrapper(StartCell));


	TSet<ACellBase*> ClosedSet;
	while (OpenSet.Num() > 0)
	{

		 FCellWrapper CurrentCell = OpenSet[0];
		for (int i = 1; i < OpenSet.Num(); ++i)
		{
			if (OpenSet[i].Cell->GetFCost() < CurrentCell.Cell->GetFCost() || OpenSet[i].Cell->GetFCost() == CurrentCell.Cell->GetFCost())
			{
				if (OpenSet[i].Cell->GetHCost() < CurrentCell.Cell->GetHCost())
					CurrentCell = OpenSet[i];
			}
		}
		OpenSet.Remove(CurrentCell);
				
		ClosedSet.Add(CurrentCell.Cell);
		if (CurrentCell.Cell == TargetCell)
		{

			return RetracePath(StartCell,TargetCell);
			
		}

	
		for (ACellBase* Neighbour : GridGenerator->GetNeighbours(CurrentCell.Cell))
		{

			if (!Neighbour->GetIsWalkable() || ClosedSet.Contains(Neighbour))
			{
				continue;
			}

			uint16 MovementCost = CurrentCell.Cell->GetGCost() + GetDistance(CurrentCell.Cell, Neighbour);
			if (MovementCost < Neighbour->GetGCost() || !OpenSet.Contains(FCellWrapper(Neighbour)))
			{

				Neighbour->SetGCost(MovementCost);
				Neighbour->SetHCost(GetDistance(Neighbour, TargetCell));
				Neighbour->SetParent(CurrentCell.Cell);

				if (!OpenSet.Contains(FCellWrapper(Neighbour)))
				{
					OpenSet.Add(FCellWrapper(Neighbour));
				}
			}
		}


	}
	return TArray<ACellBase*>();
}
TArray<ACellBase*> UFAStarNT::RetracePath(ACellBase* Start,ACellBase* Target)
{
	TArray<ACellBase*> Path;
	ACellBase* CurrentCell = Target;
	while (CurrentCell != Start)
	{
		Path.Add(CurrentCell);
		CurrentCell = CurrentCell->GetParent();
	}
	Algo::Reverse(Path);
	return Path;
}

float UFAStarNT::GetDistance(ACellBase * CellA, ACellBase * CellB)
{
	return uint16(FVector2D::Distance(FVector2D(CellA->GetActorLocation()), FVector2D(CellB->GetActorLocation())));
}
