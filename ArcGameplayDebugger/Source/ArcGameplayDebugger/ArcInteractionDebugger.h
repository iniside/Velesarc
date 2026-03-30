// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcCoreInteractionSourceComponent;

class FArcInteractionDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawCandidatesPanel();
	void DrawDetailPanel();
	void DrawViewportDebug();

	int32 SelectedCandidateIdx = INDEX_NONE;
	float LastRefreshTime = 0.f;
};
