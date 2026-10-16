// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"

class FProperty;
class FStructProperty;
class UScriptStruct;
struct FInstancedStruct;
struct yyjson_val;
struct yyjson_mut_doc;
struct yyjson_mut_val;

struct ARCCORE_API FArcJsonRecordReport
{
	TArray<FString> Errors;
	TArray<FString> Warnings;
};

namespace ArcJsonRecord
{
	ARCCORE_API bool ImportStruct(const UScriptStruct& Struct, void* Out, const yyjson_val* Object, FArcJsonRecordReport& Report);

	ARCCORE_API yyjson_mut_val* ExportStructDelta(yyjson_mut_doc* Doc, const UScriptStruct& Struct, const void* Value);

	ARCCORE_API bool ParseRecord(const UScriptStruct& Struct, TArray<uint8>& Utf8, FInstancedStruct& Out, FArcJsonRecordReport& Report);

	ARCCORE_API TArray<uint8> FormatRecord(const FInstancedStruct& Record);

	ARCCORE_API bool IsSerializedField(const UScriptStruct& Owner, const FProperty& Property);

	ARCCORE_API bool IsNativeTextStruct(const UScriptStruct& Struct);

	ARCCORE_API const FStructProperty* FindInstancedStructWrapperProperty(const UScriptStruct& Struct);

	ARCCORE_API bool RegisterInstancedStructWrapper(const UScriptStruct& Wrapper, FName InstancedStructProperty, TFunction<void(void* WrapperValue)> PostImport);

	ARCCORE_API void UnregisterInstancedStructWrapper(const UScriptStruct& Wrapper);
}
