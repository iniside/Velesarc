// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBuilderSettings.h"

#define LOCTEXT_NAMESPACE "ArcBuilder"

UArcBuilderSettings::UArcBuilderSettings()
{
	CategoryName = FName(TEXT("Arc Game"));
	SectionName = FName(TEXT("Arc Builder"));
}

#if WITH_EDITOR
FText UArcBuilderSettings::GetSectionText() const
{
	return LOCTEXT("ArcBuilderSection", "Arc Builder");
}
#endif

#undef LOCTEXT_NAMESPACE
