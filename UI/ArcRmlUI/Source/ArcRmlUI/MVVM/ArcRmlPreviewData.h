// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVM/ArcRmlModelTypes.h"

namespace ArcRmlPreviewData
{
	ARCRMLUI_API bool ShowsPreviewData();
	ARCRMLUI_API bool HasVariant(const FString& PreviewPath, const FString& Variant);
	ARCRMLUI_API TArray<FString> GetVariantNames(const FString& PreviewPath);
	ARCRMLUI_API bool LoadModel(const FString& PreviewPath, const FString& Variant, const FString& ModelName, FArcRmlValue& OutSnapshot);
}
