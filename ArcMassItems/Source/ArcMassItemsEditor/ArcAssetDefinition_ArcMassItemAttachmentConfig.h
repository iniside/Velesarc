// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Attachments/ArcMassItemAttachmentConfig.h"
#include "AssetDefinitionDefault.h"
#include "Misc/AssetCategoryPath.h"

#include "ArcAssetDefinition_ArcMassItemAttachmentConfig.generated.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassItemAttachmentConfig"

UCLASS()
class UArcAssetDefinition_ArcMassItemAttachmentConfig : public UAssetDefinitionDefault
{
	GENERATED_BODY()

public:
	virtual FText GetAssetDisplayName() const override
	{
		return LOCTEXT("DisplayName", "Arc Mass Item Attachment Config");
	}

	virtual FLinearColor GetAssetColor() const override
	{
		return FLinearColor(FColor(180, 130, 60));
	}

	virtual TSoftClassPtr<UObject> GetAssetClass() const override
	{
		return UArcMassItemAttachmentConfig::StaticClass();
	}

	virtual TConstArrayView<FAssetCategoryPath> GetAssetCategories() const override
	{
		static const FAssetCategoryPath Categories[] = {
			FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Items", "Items"), ECategoryMenuType::Section)
		};
		return Categories;
	}
};

#undef LOCTEXT_NAMESPACE
