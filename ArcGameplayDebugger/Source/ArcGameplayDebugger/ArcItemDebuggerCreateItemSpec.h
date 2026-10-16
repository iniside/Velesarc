// Copyright Lukasz Baran. All Rights Reserved.
// GAS keystone retired (Bucket D 7); this debugger spec creator is a no-op stub
// pending Mass-side reimplementation.

#pragma once

#include "CoreMinimal.h"
#include "Items/ArcItemSpec.h"

class FArcItemDebuggerItemSpecCreator
{
public:
	void Initialize() {}
	void Uninitialize() {}
	void Reset() { ResultSpec = FArcItemSpec(); bFinished = false; bCancelled = false; }
	void Draw() {}
	void Begin(const FPrimaryAssetId&) { Reset(); bCancelled = true; }
	bool IsFinished() const { return bFinished; }
	bool WasCancelled() const { return bCancelled; }
	FArcItemSpec& GetResultSpec() { return ResultSpec; }

private:
	FArcItemSpec ResultSpec;
	bool bFinished = false;
	bool bCancelled = false;
};
