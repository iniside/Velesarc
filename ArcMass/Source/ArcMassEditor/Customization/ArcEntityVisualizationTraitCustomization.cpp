// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEntityVisualizationTraitCustomization.h"

#include "ArcBodyInstanceExpander.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FArcEntityVisualizationTraitCustomization::MakeInstance()
{
	return MakeShared<FArcEntityVisualizationTraitCustomization>();
}

void FArcEntityVisualizationTraitCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> BodyTemplateHandle = DetailBuilder.GetProperty(TEXT("ExtractedBodyTemplate"));
	DetailBuilder.HideProperty(BodyTemplateHandle);

	IDetailCategoryBuilder& ExtractedCategory = DetailBuilder.EditCategory("Extracted");
	ExtractedCategory.AddCustomBuilder(MakeShared<FArcBodyInstanceNodeBuilder>(BodyTemplateHandle));
}
