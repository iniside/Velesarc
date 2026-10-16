// Copyright Lukasz Baran. All Rights Reserved.

#include "AssetDefinitions/ArcAssetDefinition_ArcEcotopeTreeSpeciesModel.h"

#include "SpeciesEditor/ArcEcotopeSpeciesEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcEcotopeTreeSpeciesModel)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcEcotopeTreeSpeciesModel"

FText UArcAssetDefinition_ArcEcotopeTreeSpeciesModel::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc Ecotope Tree Species");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcEcotopeTreeSpeciesModel::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = { FAssetCategoryPath(LOCTEXT("ArcEcotope", "Arc Ecotope")) };
	return Categories;
}

EAssetCommandResult UArcAssetDefinition_ArcEcotopeTreeSpeciesModel::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcEcotopeSpeciesModel* Model : OpenArgs.LoadObjects<UArcEcotopeSpeciesModel>())
	{
		FArcEcotopeSpeciesEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Model);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
