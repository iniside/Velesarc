// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAssetDefinition_ArcTownBuildingDefinition.h"

#include "BuildingEditor/ArcTownBuildingEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcTownBuildingDefinition)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTownBuildingDefinition"

FText UArcAssetDefinition_ArcTownBuildingDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc Town Building Definition");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcTownBuildingDefinition::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = {
		FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Town", "Town"), ECategoryMenuType::Section)
	};
	return Categories;
}

EAssetCommandResult UArcAssetDefinition_ArcTownBuildingDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcTownBuildingDefinition* Definition : OpenArgs.LoadObjects<UArcTownBuildingDefinition>())
	{
		FArcTownBuildingEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Definition);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
