#pragma once
#include "MassEntityHandle.h"

class FArcNeedsDebugger
{
public:
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

	int32 SelectedEntityIdx = -1;

	TArray<FMassEntityHandle> Entities;
};
