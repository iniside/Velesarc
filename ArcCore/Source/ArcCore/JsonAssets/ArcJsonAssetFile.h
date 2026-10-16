// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcJsonAssetTypeInfo;
struct FInstancedStruct;

namespace ArcJsonAssetFile
{
	ARCCORE_API bool ParseRecord(const FArcJsonAssetTypeInfo& Type, TArray<uint8>& Utf8, const FString& Filename, FInstancedStruct& OutRecord);
	ARCCORE_API bool ReadRecordFile(const FArcJsonAssetTypeInfo& Type, const FString& Filename, FInstancedStruct& OutRecord);

	ARCCORE_API TArray<uint8> FormatRecord(const FInstancedStruct& Record);
	ARCCORE_API bool WriteRecordFile(const FString& Filename, const FInstancedStruct& Record);
	ARCCORE_API bool WriteRecordFile(const FString& Filename, const FInstancedStruct& Record, TArray<uint8>& OutUtf8);
}
