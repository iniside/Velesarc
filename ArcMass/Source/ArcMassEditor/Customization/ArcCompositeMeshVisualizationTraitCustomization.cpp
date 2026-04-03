// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCompositeMeshVisualizationTraitCustomization.h"

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FArcCompositeMeshVisualizationTraitCustomization::MakeInstance()
{
	return MakeShared<FArcCompositeMeshVisualizationTraitCustomization>();
}

void FArcCompositeMeshVisualizationTraitCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}
