// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/StructView.h"
#include "Templates/Function.h"
#include "UObject/PrimaryAssetId.h"

class FAssetDataTagMap;
class FStructProperty;
class UScriptStruct;
struct FInstancedStruct;

struct FArcJsonAssetTypeInfo
{
	FString Extension;
	const UScriptStruct* Struct = nullptr;
	FPrimaryAssetType PrimaryAssetType;
	FName IdProperty;
	FName TypeProperty;
	FText DisplayName;
	FLinearColor Color = FLinearColor::White;
	TFunction<void(const FInstancedStruct& Record, FAssetDataTagMap& Tags)> AddAssetRegistryTags;
};

namespace ArcJsonAssets
{
	ARCCORE_API bool RegisterType(const FArcJsonAssetTypeInfo& Info);
	ARCCORE_API bool UnregisterType(const FString& Extension);
	ARCCORE_API TConstArrayView<FArcJsonAssetTypeInfo> GetTypes();

	ARCCORE_API const FArcJsonAssetTypeInfo* FindTypeByExtension(FStringView Extension);
	ARCCORE_API const FArcJsonAssetTypeInfo* FindTypeByFilename(FStringView Filename);
	ARCCORE_API const FStructProperty* FindIdProperty(const FArcJsonAssetTypeInfo& Info);
	ARCCORE_API const FStructProperty* FindTypeProperty(const FArcJsonAssetTypeInfo& Info);
	ARCCORE_API FPrimaryAssetId MakeRecordId(const FArcJsonAssetTypeInfo& Info, FConstStructView Record, FString* OutError = nullptr);
}
