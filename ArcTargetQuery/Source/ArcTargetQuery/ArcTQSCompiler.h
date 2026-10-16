// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTQSQueryDefinition;
struct FArcTQSCompiledData;

#if WITH_EDITOR
struct ARCTARGETQUERY_API FArcTQSCompilerDiagnostic
{
	FGuid NodeID;
	FText Message;
	bool bIsError = true;
};

struct ARCTARGETQUERY_API FArcTQSCompiler
{
	static bool Compile(
		const UArcTQSQueryDefinition& Definition,
		uint32 Revision,
		FArcTQSCompiledData& OutData,
		TArray<FArcTQSCompilerDiagnostic>& OutDiagnostics);
};
#endif
