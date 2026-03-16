// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"

class FArcConditionImGuiDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	int32 SelectedEntityIdx = -1;
	TArray<FMassEntityHandle> Entities;

	// Per-condition apply amounts (one drag value per condition)
	float ApplyAmounts[13] = { 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f, 10.f };
};
