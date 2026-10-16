// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcTacticsConfig;

namespace ArcTacticsConfigLint
{
	enum class ESeverity : uint8
	{
		Warning,
		Info
	};

	struct FFinding
	{
		ESeverity Severity = ESeverity::Warning;
		FString Code;
		FString Node;
		FString Message;
	};

	TArray<FFinding> Run(const UArcTacticsConfig& Config);
}
