// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FConstStructView;
class FProperty;
class UScriptStruct;

namespace ArcImGuiReflection
{
	/** Draw all properties of a struct view as an ImGui tree (read-only). */
	void DrawStructView(const TCHAR* Label, FConstStructView StructView);

	/** Draw all properties of a struct using reflection. */
	void DrawStructProperties(const UScriptStruct* Struct, const void* Data);

	/** Draw a single property value. */
	void DrawProperty(const FProperty* Property, const void* ContainerPtr);
}
