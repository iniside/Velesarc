// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTacticsConfig;
struct FArcTacticsCompiledData;

#if WITH_EDITOR
struct ARCTACTICS_API FArcTacticsCompilerDiagnostic
{
	FGuid NodeID;
	FText Message;
	bool bIsError = true;
};

struct ARCTACTICS_API FArcTacticsCompiler
{
	static bool Compile(
		const UArcTacticsConfig& Config,
		uint32 Revision,
		FArcTacticsCompiledData& OutData,
		TArray<FArcTacticsCompilerDiagnostic>& OutDiagnostics);
};
#endif
