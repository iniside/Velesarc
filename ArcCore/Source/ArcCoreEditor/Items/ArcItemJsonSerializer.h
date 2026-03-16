// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class UArcItemDefinition;
struct FArcInstancedStruct;
struct FInstancedStruct;

/**
 * Utility for converting ArcItemDefinition to/from JSON.
 * Can be used from MCP commands, editor utilities, or automated tests.
 *
 * JSON format:
 * {
 *   "item_type": "Weapon",
 *   "stack_method": { "type": "ArcItemStackMethod_NonStackable", "properties": { ... } },
 *   "fragments": [
 *     { "type": "ArcItemFragment_UIDescription", "properties": { "DisplayName": "Sword" } }
 *   ],
 *   "scalable_float_fragments": [
 *     { "type": "ArcScalableFloatFragment_Damage", "properties": { ... } }
 *   ]
 * }
 */
class ARCCOREEDITOR_API FArcItemJsonSerializer
{
public:
	/** Export an item definition to a JSON object. */
	static TSharedPtr<FJsonObject> ExportToJson(const UArcItemDefinition* ItemDef);

	/** Import JSON into an existing item definition, replacing all fragments. */
	static bool ImportFromJson(UArcItemDefinition* ItemDef, const TSharedPtr<FJsonObject>& JsonObj, FString& OutError);

	/** Export an item definition to a JSON string. */
	static FString ExportToJsonString(const UArcItemDefinition* ItemDef);

	/** Import from a JSON string into an existing item definition. */
	static bool ImportFromJsonString(UArcItemDefinition* ItemDef, const FString& JsonString, FString& OutError);

private:
	/** Serialize a single FArcInstancedStruct to JSON. */
	static TSharedPtr<FJsonObject> FragmentToJson(const FArcInstancedStruct& Fragment);

	/** Serialize an FInstancedStruct to JSON. */
	static TSharedPtr<FJsonObject> InstancedStructToJson(const FInstancedStruct& Instance);

	/** Serialize all editable properties of a struct to a JSON object. */
	static TSharedPtr<FJsonObject> StructPropertiesToJson(const UScriptStruct* Struct, const void* StructMemory);

	/** Set properties on a struct instance from a JSON object. */
	static bool SetPropertiesFromJson(void* StructMemory, const UScriptStruct* Struct, const TSharedPtr<FJsonObject>& Props, FString& OutError);

	/** Find a UScriptStruct by name (with or without F prefix). */
	static UScriptStruct* FindStructByName(const FString& InName);
};
