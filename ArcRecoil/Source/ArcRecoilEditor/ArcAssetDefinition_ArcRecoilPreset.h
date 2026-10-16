// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Config/ArcRecoilPreset.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcRecoilPreset.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcRecoilPreset"

UCLASS()
class UArcAssetDefinition_ArcRecoilPreset : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Recoil Preset");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(220, 120, 40));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcRecoilPreset::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Gameplay", "Gameplay"), ECategoryMenuType::Section)
		};
		return Categories;
	}

	virtual EAssetCommandResult OpenAssets(const FAssetOpenArgs& OpenArgs) const override;
};

#undef LOCTEXT_NAMESPACE
