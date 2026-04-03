// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTQSDebuggerDrawComponent;

/**
 * ImGui-based debugger for the Target Query System.
 * Caches query history beyond the subsystem's 10s expiry window.
 * Registered in UArcGameplayDebuggerSubsystem under the AI menu.
 */
class FArcTQSDebugger
{
public:
	FArcTQSDebugger();
	~FArcTQSDebugger();

	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	struct FImpl;
	TUniquePtr<FImpl> Impl;
};
