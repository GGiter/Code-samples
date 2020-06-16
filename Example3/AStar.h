// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Runtime/Core/Public/HAL/ThreadSafeCounter.h"
#include "Runtime/Core/Public/HAL/Runnable.h"
#include "CellBase.h"
#include "GridGenerator.h"

class INVADED_API FAStar : public FRunnable
{
public:
public:
	static FAStar* Runnable;

	FRunnableThread* Thread;

	FThreadSafeCounter StopTaskCounter;

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	float GetDistance(ACellBase* CellA, ACellBase* CellB);
	void FindPath();
	void RetracePath();
	void EnsureCompletion();

	static void Shutdown();

	static bool IsThreadFinished();
	static TArray<ACellBase*> GetPath();
	static FAStar* JoyInit(ACellBase* StartCell, ACellBase* TargetCell, AGridGenerator* GridGenerator, TArray<ACellBase*> CellPath);
	FAStar(ACellBase* StartCell, ACellBase* TargetCell, AGridGenerator* GridGenerator, TArray<ACellBase*> CellPath);
	TArray<ACellBase*> Path;
	~FAStar();
private:
	bool IsFinished() const
	{
		return bFinish;
	}
	bool bFinish = false;
	ACellBase* Start;
	ACellBase* Target;
	AGridGenerator* Grid;
	
};
