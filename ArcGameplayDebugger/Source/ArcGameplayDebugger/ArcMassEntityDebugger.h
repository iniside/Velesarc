// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassEntityHandle.h"
#include "MassEntityTypes.h"

struct FMassArchetypeHandle;

class FArcMassEntityDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawEntityListPanel();
	void DrawEntityDetailPanel();
	void DrawFragmentProperties(const UScriptStruct* FragmentType, const void* FragmentData);
	void DrawPropertyRows(const FProperty* Prop, const void* ContainerPtr, int32 IndentLevel);
	void DrawLeafValue(const FProperty* Prop, const void* ContainerPtr);

	void RefreshEntityList();

	struct FEntityEntry
	{
		FMassEntityHandle Entity;
		FString Label;
	};

	TArray<FEntityEntry> CachedEntities;
	int32 SelectedEntityIndex = INDEX_NONE;
	char EntityFilterBuf[256] = {};
	float LastRefreshTime = 0.f;
};
