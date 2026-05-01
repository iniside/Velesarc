// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"

class UScriptStruct;
class FProperty;

/**
 * Generic ImGui widget for selecting a UScriptStruct type and editing its properties.
 * Filters struct list by a configurable base struct. Owns the edited FInstancedStruct.
 */
class FArcImGuiStructEditorWidget
{
public:
	/** Set the base UScriptStruct — only this struct and its descendants appear in the picker. */
	void SetBaseStruct(const UScriptStruct* InBaseStruct);

	/** Draw inline (no Begin/End) — embeddable in any debugger window. */
	void DrawInline();

	/** Returns true if the user has selected a struct type and the instance is valid. */
	bool HasValidInstance() const;

	/** Get the edited payload as FConstStructView for sending. */
	FConstStructView GetStructView() const;

	/** Reset to empty (clears type selection and data). */
	void Reset();

private:
	/** Rebuild CachedStructList from TObjectIterator filtered by BaseStruct. */
	void RebuildStructList();

	/** Draw the struct type picker button and popup. */
	void DrawStructPicker();

	/** Draw editable controls for all UPROPERTYs on the selected struct. */
	void DrawEditableStructProperties(const UScriptStruct* Struct, void* Data);

	/** Draw a single editable property. */
	void DrawEditableProperty(const FProperty* Property, void* ContainerPtr);

	/** Draw asset picker popup for TObjectPtr properties. Returns true if selection changed. */
	bool DrawAssetPicker(const FObjectPropertyBase* ObjProp, void* ContainerPtr);

	const UScriptStruct* BaseStruct = nullptr;
	const UScriptStruct* SelectedStruct = nullptr;
	FInstancedStruct EditedInstance;

	char StructFilterBuf[256] = {};
	char AssetFilterBuf[256] = {};
	TArray<const UScriptStruct*> CachedStructList;

	/** Per-property text buffers for string editing (keyed by property name). */
	TMap<FName, TArray<char>> StringBuffers;

	/** Cached asset lists per class for the asset picker popup (keyed by UClass*). */
	TMap<const UClass*, TArray<FAssetData>> CachedAssetLists;
};
