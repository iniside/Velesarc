// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RmlUi/Core/Types.h"

namespace ArcRmlStrings
{
	inline FString ToFString(const Rml::String& Value)
	{
		return FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(Value.data()), static_cast<int32>(Value.size()));
	}

	inline Rml::String ToRmlString(const FString& Value)
	{
		const FTCHARToUTF8 Converted(*Value);
		return Rml::String(reinterpret_cast<const char*>(Converted.Get()), static_cast<size_t>(Converted.Length()));
	}
}
