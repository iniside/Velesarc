// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcAssetDefinition_ArcTQSQueryDefinition.h"

#include "ArcTQSQueryEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcTQSQueryDefinition)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcTQSQueryDefinition"

FText UArcAssetDefinition_ArcTQSQueryDefinition::GetAssetDisplayName() const
{
	return LOCTEXT("DisplayName", "Arc TQS Query Definition");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcTQSQueryDefinition::GetAssetCategories() const
{
	static FAssetCategoryPath Categories[] = {
		FAssetCategoryPath(FAssetCategoryPath(LOCTEXT("ArcGame", "Arc Game")), LOCTEXT("Knowledge", "Knowledge"), ECategoryMenuType::Section)
	};
	return Categories;
}

EAssetCommandResult UArcAssetDefinition_ArcTQSQueryDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcTQSQueryDefinition* Definition : OpenArgs.LoadObjects<UArcTQSQueryDefinition>())
	{
		FArcTQSQueryEditor::CreateEditor(OpenArgs.GetToolkitMode(), OpenArgs.ToolkitHost, Definition);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
