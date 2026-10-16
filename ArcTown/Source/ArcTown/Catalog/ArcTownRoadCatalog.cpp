// Copyright Lukasz Baran. All Rights Reserved.

#include "Catalog/ArcTownRoadCatalog.h"

#include "Catalog/ArcTownRoadDefinition.h"
#include "Simulation/ArcTownTraffic.h"

#if WITH_EDITOR

#define LOCTEXT_NAMESPACE "ArcTownRoadCatalog"

namespace ArcTownRoadCatalogPrivate
{
	void AddError(FDataValidationContext& Context, EDataValidationResult& Result, const FText& Message)
	{
		Context.AddError(Message);
		Result = EDataValidationResult::Invalid;
	}
}

EDataValidationResult UArcTownRoadCatalog::IsDataValid(FDataValidationContext& Context) const
{
	using namespace ArcTownRoadCatalogPrivate;

	EDataValidationResult Result = Super::IsDataValid(Context);

	if (ClassesByHierarchy.Num() > ArcTownTraffic::MaxRoadHierarchy + 1)
	{
		AddError(Context, Result, FText::Format(
			LOCTEXT("TooManyClasses", "ClassesByHierarchy is indexed by hierarchy class 0 to {0}; entries beyond that are never read."),
			FText::AsNumber(static_cast<int32>(ArcTownTraffic::MaxRoadHierarchy))));
	}

	int32 HighestFilled = INDEX_NONE;
	for (int32 Hierarchy = 0; Hierarchy < ClassesByHierarchy.Num(); ++Hierarchy)
	{
		if (ClassesByHierarchy[Hierarchy] != nullptr)
		{
			HighestFilled = Hierarchy;
		}
	}

	for (int32 Hierarchy = 0; Hierarchy < ClassesByHierarchy.Num(); ++Hierarchy)
	{
		const UArcTownRoadDefinition* const Definition = ClassesByHierarchy[Hierarchy];
		if (Definition == nullptr)
		{
			if (Hierarchy < HighestFilled)
			{
				AddError(Context, Result, FText::Format(
					LOCTEXT("ClassHole", "ClassesByHierarchy[{0}] is empty while a higher class is filled; roads promoted to that class would have no mesh."),
					FText::AsNumber(Hierarchy)));
			}
			continue;
		}

		Result = CombineDataValidationResults(Result, Definition->IsDataValid(Context));
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

#endif
