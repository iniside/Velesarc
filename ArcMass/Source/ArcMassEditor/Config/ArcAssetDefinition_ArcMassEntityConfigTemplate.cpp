// Copyright Lukasz Baran. All Rights Reserved.

#include "Config/ArcAssetDefinition_ArcMassEntityConfigTemplate.h"
#include "ArcAssetEditor_MassEntityConfig.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcMassEntityConfigTemplate)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassEntityConfigTemplate"

FText UArcAssetDefinition_ArcMassEntityConfigTemplate::GetAssetDisplayName() const
{
	return LOCTEXT("ArcMassEntityConfigTemplate", "Arc Mass Entity Config Template");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcMassEntityConfigTemplate::GetAssetCategories() const
{
	static FAssetCategoryPath AssetPaths[] = {
		FAssetCategoryPath(LOCTEXT("Gameplay", "Gameplay")),
		FAssetCategoryPath(LOCTEXT("MassCategory", "Arc Mass"))
	};
	return AssetPaths;
}

EAssetCommandResult UArcAssetDefinition_ArcMassEntityConfigTemplate::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcMassEntityConfigTemplate* TemplateAsset : OpenArgs.LoadObjects<UArcMassEntityConfigTemplate>())
	{
		FArcAssetEditor_MassEntityConfig::CreateEditor(
			OpenArgs.GetToolkitMode(),
			OpenArgs.ToolkitHost,
			TemplateAsset);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
