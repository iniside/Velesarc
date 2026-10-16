// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

struct FArcPrimaryAssetIdSourceEntry
{
	FPrimaryAssetId Id;
	FString DisplayName;
};

DECLARE_DELEGATE_RetVal(TArray<FArcPrimaryAssetIdSourceEntry>, FArcGetPrimaryAssetIdSourceEntries);
DECLARE_DELEGATE_OneParam(FArcBrowseToPrimaryAssetId, const FPrimaryAssetId&);

struct FArcPrimaryAssetIdSource
{
	FArcGetPrimaryAssetIdSourceEntries GetEntries;
	FArcBrowseToPrimaryAssetId BrowseTo;
};

namespace ArcPrimaryAssetIdSources
{
	ARCCOREEDITOR_API void Register(const FPrimaryAssetType& Type, const FArcPrimaryAssetIdSource& Source);
	ARCCOREEDITOR_API void Unregister(const FPrimaryAssetType& Type);
	ARCCOREEDITOR_API const FArcPrimaryAssetIdSource* Find(const FPrimaryAssetType& Type);
}
