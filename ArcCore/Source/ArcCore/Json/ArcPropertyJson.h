// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"
#include "UObject/UObjectGlobals.h"

class FJsonObject;
class FJsonValue;
class UPropertyBag;
struct FInstancedPropertyBag;

struct ARCCORE_API FArcPropertyJsonReport
{
	TArray<FString> Errors;
	TArray<FString> Warnings;

	void Error(const FString& Message)
	{
		Errors.Add(Message);
	}

	void Warning(const FString& Message)
	{
		Warnings.Add(Message);
	}
};

struct FArcPropertyJsonHooks
{
	TFunction<TSharedPtr<FJsonValue>(FProperty* Property, const void* Value)> ExportStruct;
	TFunction<bool(
		const TSharedPtr<FJsonValue>& JsonValue,
		FProperty* Property,
		void* Value,
		const FString& Location,
		FArcPropertyJsonReport& Report)> ImportStruct;
	TFunction<bool(const UScriptStruct& Struct)> SkipPrepass;
};

struct FArcAssetPath
{
	FString PackageName;
	FString AssetName;
	FString ObjectPath;
};

namespace ArcPropertyJson
{
	ARCCORE_API FString GetObjectTypeName(const UObject* Type);

	ARCCORE_API TSharedPtr<FJsonValue> FindField(const FJsonObject& Object, const FString& Key);
	ARCCORE_API TSharedRef<FJsonObject> CloneObject(const FJsonObject& Object);
	ARCCORE_API TArray<TSharedPtr<FJsonValue>> ToJsonStrings(const TArray<FString>& Values);

	ARCCORE_API void CheckKeys(
		const FJsonObject& Object,
		TConstArrayView<const TCHAR*> Allowed,
		const FString& Location,
		FArcPropertyJsonReport& Report);
	ARCCORE_API TSharedPtr<FJsonObject> AsObject(
		const TSharedPtr<FJsonValue>& Value,
		const FString& Location,
		FArcPropertyJsonReport& Report);
	ARCCORE_API TSharedPtr<FJsonObject> GetObjectValue(
		const FJsonObject& Owner,
		const TCHAR* Key,
		const FString& Location,
		FArcPropertyJsonReport& Report);
	ARCCORE_API TArray<TSharedPtr<FJsonValue>> GetArrayValue(
		const FJsonObject& Owner,
		const TCHAR* Key,
		const FString& Location,
		FArcPropertyJsonReport& Report);
	ARCCORE_API bool GetBool(
		const FJsonObject& Owner,
		const TCHAR* Key,
		const FString& Location,
		FArcPropertyJsonReport& Report);
	ARCCORE_API bool GetString(
		const FJsonObject& Owner,
		const TCHAR* Key,
		const FString& Location,
		FString& OutValue,
		FArcPropertyJsonReport& Report);

	ARCCORE_API TSharedPtr<FJsonValue> ExportPropertyValue(
		const FProperty& Property,
		const void* ValuePtr,
		const FArcPropertyJsonHooks& Hooks);
	ARCCORE_API bool IsIdenticalProperty(const FProperty& Property, const void* Container, const void* DefaultContainer);
	ARCCORE_API TSharedRef<FJsonObject> ExportStructDelta(
		const UScriptStruct& Struct,
		const void* Memory,
		const FArcPropertyJsonHooks& Hooks,
		TFunctionRef<bool(const FProperty& Property)> SkipProperty);
	ARCCORE_API TArray<TSharedPtr<FJsonValue>> ExportBag(
		const FInstancedPropertyBag& Bag,
		const FArcPropertyJsonHooks& Hooks);

	ARCCORE_API bool ImportStruct(
		const UStruct& Struct,
		void* Memory,
		const TSharedPtr<FJsonObject>& Json,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks);
	ARCCORE_API bool ImportPropertyValue(
		FProperty& Property,
		void* ValuePtr,
		const TSharedPtr<FJsonValue>& Value,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks);
	ARCCORE_API void ImportBag(
		const TArray<TSharedPtr<FJsonValue>>& Entries,
		FInstancedPropertyBag& OutBag,
		const FString& Location,
		FArcPropertyJsonReport& Report,
		const FArcPropertyJsonHooks& Hooks,
		const UPropertyBag* ReuseDescsFrom = nullptr);

#if WITH_EDITOR
	ARCCORE_API TSharedRef<FJsonObject> DescribeField(
		const FProperty& Property,
		const void* DefaultContainer,
		const FArcPropertyJsonHooks& Hooks,
		TFunctionRef<void(FJsonObject& Field)> AddRoleFields);
#endif

	ARCCORE_API bool ParseAssetPath(
		const FString& InPath,
		const TCHAR* Example,
		FArcAssetPath& OutPath,
		FArcPropertyJsonReport& Report);
}
