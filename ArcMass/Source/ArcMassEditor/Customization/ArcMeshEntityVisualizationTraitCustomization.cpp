// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMeshEntityVisualizationTraitCustomization.h"

#include "DetailLayoutBuilder.h"

TSharedRef<IDetailCustomization> FArcMeshEntityVisualizationTraitCustomization::MakeInstance()
{
	return MakeShared<FArcMeshEntityVisualizationTraitCustomization>();
}

void FArcMeshEntityVisualizationTraitCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}
