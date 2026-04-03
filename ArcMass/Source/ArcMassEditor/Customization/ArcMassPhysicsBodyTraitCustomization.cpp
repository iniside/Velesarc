// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassPhysicsBodyTraitCustomization.h"

#include "ArcBodyInstanceExpander.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

void FArcMassPhysicsBodyTraitCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TSharedRef<IPropertyHandle> BodyTemplateHandle = DetailBuilder.GetProperty(TEXT("BodyTemplate"));
	DetailBuilder.HideProperty(BodyTemplateHandle);

	IDetailCategoryBuilder& PhysicsCategory = DetailBuilder.EditCategory("Physics");
	PhysicsCategory.AddCustomBuilder(MakeShared<FArcBodyInstanceNodeBuilder>(BodyTemplateHandle));
}
