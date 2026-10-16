// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"

namespace ArcTraceFileName
{
	inline FString MakeTraceFileName(const TCHAR* Infix)
	{
		FString BaseName = FApp::GetProjectName();
		if (!FApp::HasProjectName())
		{
			BaseName = FPlatformProcess::ExecutableName(false);
		}

		const FString Timestamp = FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
		return FString::Printf(TEXT("%s%s-%s.utrace"), *BaseName, Infix, *Timestamp);
	}
}
