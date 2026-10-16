// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAssetDefinition_ArcEcotopeLowGroundCoverSpeciesModel.h"

#include "SpeciesEditor/ArcEcotopeSpeciesEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcEcotopeLowGroundCoverSpeciesModel)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcEcotopeLowGroundCoverSpeciesModel"

FText UArcAssetDefinition_ArcEcotopeLowGroundCoverSpeciesModel::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc Ecotope Low Ground Cover Species");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcEcotopeLowGroundCoverSpeciesModel::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("ArcEcotope", "Arc Ecotope")) };
	return Categories;
}

EAssetCommandResult UArcAssetDefinition_ArcEcotopeLowGroundCoverSpeciesModel::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcEcotopeSpeciesModel* Model : OpenArgs.LoadObjects<UArcEcotopeSpeciesModel>())
	{
		FArcEcotopeSpeciesEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Model);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
