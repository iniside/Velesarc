// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UArcCoreInteractionSourceComponent;
class UArcInteractionDebuggerDrawComponent;

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

	TWeakObjectPtr<AActor> DrawActor;
	TWeakObjectPtr<UArcInteractionDebuggerDrawComponent> DrawComponent;
	void EnsureDrawActor(UWorld* World);
	void DestroyDrawActor();
};
