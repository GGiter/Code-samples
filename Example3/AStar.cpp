// Fill out your copyright notice in the Description page of Project Settings.

#include "AStar.h"
#include "Runtime/Core/Public/HAL/RunnableThread.h"
#include "Algo/Reverse.h"
#include "Kismet/GameplayStatics.h"
#include "FAStarNT.h"

FAStar* FAStar::Runnable = nullptr;

bool FAStar::Init()
{
	return true;
}

uint32 FAStar::Run()
{
	FPlatformProcess::Sleep(0.01);
	FindPath();
	return 0;
}

void FAStar::Stop()
{
	StopTaskCounter.Increment();
}

void FAStar::RetracePath()
{
	ACellBase* CurrentCell = Target;
	while (CurrentCell != Start)
	{
		Path.Add(CurrentCell);
		CurrentCell = CurrentCell->GetParent();
	}
	Algo::Reverse(Path);
	bFinish = true;
	
}

void FAStar::EnsureCompletion()
{
	Stop();
	Thread->WaitForCompletion();
}

void FAStar::Shutdown()
{
	if (Runnable)
	{
		Runnable->EnsureCompletion();
		delete Runnable;
		Runnable = nullptr;
	}
}

bool FAStar::IsThreadFinished()
{
	if (Runnable) return Runnable->IsFinished();
	return true;
}
TArray<ACellBase*> FAStar::GetPath()
{
	TArray<ACellBase*> Empty;
	if (Runnable) return Runnable->Path;
	return Empty;
}

FAStar * FAStar::JoyInit(ACellBase* StartCell, ACellBase* TargetCell, AGridGenerator* GridGenerator, TArray<ACellBase*> CellPath)
{
	if (!Runnable && FPlatformProcess::SupportsMultithreading())
	{
		Runnable = new FAStar(StartCell,TargetCell,GridGenerator,CellPath);
	}
	return Runnable;
}

FAStar::FAStar(ACellBase* StartCell, ACellBase* TargetCell, AGridGenerator* GridGenerator, TArray<ACellBase*> CellPath)
{
	Start = StartCell;
	Target = TargetCell;
	Grid = GridGenerator;
	Path = CellPath;
	
	Thread = FRunnableThread::Create(this, *FString("AStar" + FString::FromInt(FMath::RandRange(0.0f,1000.0f))), 0, TPri_AboveNormal);
}

FAStar::~FAStar()
{
	delete Thread;
	Thread = nullptr;
}
float FAStar::GetDistance(ACellBase* CellA, ACellBase* CellB)
{
	return uint16(FVector::Distance(CellA->GetActorLocation(), CellB->GetActorLocation()));
}

void FAStar::FindPath()
{	
	Path = UFAStarNT::GetPath(Start,Target,Grid);
	bFinish = true;

}
