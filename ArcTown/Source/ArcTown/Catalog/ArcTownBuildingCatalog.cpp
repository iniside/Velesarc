// Copyright Lukasz Baran. All Rights Reserved.

#include "Catalog/ArcTownBuildingCatalog.h"

#include "Catalog/ArcTownBuildingDefinition.h"

#if WITH_EDITOR

namespace ArcTownBuildingCatalogPrivate
{
	void AddError(FDataValidationContext& Context, EDataValidationResult& Result, const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	}

	void AddWarning(FDataValidationContext& Context, const FString& Message)
	{
		Context.AddWarning(FText::FromString(Message));
	}
}

EDataValidationResult UArcTownBuildingCatalog::IsDataValid(FDataValidationContext& Context) const
{
	using namespace ArcTownBuildingCatalogPrivate;

	EDataValidationResult Result = Super::IsDataValid(Context);

	bool bAnyEntryHasDenyBuildings = false;

	for (int32 Index = 0; Index < Buildings.Num(); ++Index)
	{
		const UArcTownBuildingDefinition* const Building = Buildings[Index];
		if (Building == nullptr)
		{
			AddError(Context, Result, FString::Printf(TEXT("Buildings[%d] must not be empty."), Index));
			continue;
		}

		if (Buildings.Find(Buildings[Index]) != Index)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Buildings[%d] (%s) is already listed earlier in this catalog."), Index, *Building->GetName()));
		}

		if (!Building->DenyBuildings.IsEmpty())
		{
			bAnyEntryHasDenyBuildings = true;
		}

		Result = CombineDataValidationResults(Result, Building->IsDataValid(Context));
	}

	if (Buildings.Num() > 0 && !bAnyEntryHasDenyBuildings)
	{
		AddWarning(Context, TEXT("No entry in this catalog sets DenyBuildings; without any repulsion, placement will converge into a single blob."));
	}

	return Result;
}

#endif
