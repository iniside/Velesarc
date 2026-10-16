// Copyright Lukasz Baran. All Rights Reserved.

#include "Catalog/ArcTownBuildingDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownBuildingDefinition, Log, All);

void ArcTownBuildingDefinition::ResolvePlotSize(
	const UArcTownBuildingDefinition& Definition,
	FVector2D& OutPlotSizeMinXY,
	FVector2D& OutPlotSizeMaxXY)
{
	FVector2D PlotSizeMinXY = Definition.PlotSizeMinXY;
	PlotSizeMinXY.X = PlotSizeMinXY.X <= 0.0 ? Definition.FootprintXY.X : PlotSizeMinXY.X;
	PlotSizeMinXY.Y = PlotSizeMinXY.Y <= 0.0 ? Definition.FootprintXY.Y : PlotSizeMinXY.Y;

	FVector2D PlotSizeMaxXY = Definition.PlotSizeMaxXY;
	PlotSizeMaxXY.X = PlotSizeMaxXY.X <= 0.0 ? Definition.FootprintXY.X : PlotSizeMaxXY.X;
	PlotSizeMaxXY.Y = PlotSizeMaxXY.Y <= 0.0 ? Definition.FootprintXY.Y : PlotSizeMaxXY.Y;

	PlotSizeMaxXY.X = PlotSizeMaxXY.X < PlotSizeMinXY.X ? PlotSizeMinXY.X : PlotSizeMaxXY.X;
	PlotSizeMaxXY.Y = PlotSizeMaxXY.Y < PlotSizeMinXY.Y ? PlotSizeMinXY.Y : PlotSizeMaxXY.Y;

	OutPlotSizeMinXY = PlotSizeMinXY;
	OutPlotSizeMaxXY = PlotSizeMaxXY;
}

FVector2D ArcTownBuildingDefinition::ResolveDoorForwardXY(
	const UArcTownBuildingDefinition& Definition,
	const FArcTownBuildingVisual& Visual)
{
	if (Definition.bOverrideDoorForward)
	{
		const double YawRad = FMath::DegreesToRadians(Definition.DoorForwardYawDeg);
		return FVector2D(FMath::Cos(YawRad), FMath::Sin(YawRad));
	}

	return Visual.ResolveDoorForwardXY(Definition.DoorSocket);
}

#if WITH_EDITOR

namespace ArcTownBuildingDefinitionPrivate
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

	/** meta=(Categories) only filters the editor picker; Blueprint writes and text import bypass it. */
	bool AllTagsUnder(const FGameplayTagContainer& Tags, const FGameplayTag& Root)
	{
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.MatchesTag(Root))
			{
				return false;
			}
		}
		return true;
	}
}

EDataValidationResult UArcTownBuildingDefinition::IsDataValid(FDataValidationContext& Context) const
{
	using namespace ArcTownBuildingDefinitionPrivate;

	EDataValidationResult Result = Super::IsDataValid(Context);

	const FGameplayTag DistrictRoot = FGameplayTag::RequestGameplayTag(TEXT("Town.District"), false);
	const FGameplayTag BuildingRoot = FGameplayTag::RequestGameplayTag(TEXT("Town.Building"), false);

	const FArcTownBuildingVisual* const BuildingVisual = Visual.GetPtr();
	if (BuildingVisual == nullptr)
	{
		AddError(Context, Result, TEXT("Visual must name a representation for this building."));
	}
	else
	{
		FText Message;
		if (!BuildingVisual->IsValid(Message))
		{
			AddError(Context, Result, Message.ToString());
		}
		else if (!Message.IsEmpty())
		{
			AddWarning(Context, Message.ToString());
		}
	}

	if (FootprintXY.X <= 0.0 || FootprintXY.Y <= 0.0)
	{
		AddError(Context, Result, TEXT("FootprintXY must be greater than zero on both axes."));
	}

	if (PlotSizeMaxXY.X > 0.0 && PlotSizeMaxXY.X < PlotSizeMinXY.X)
	{
		AddError(Context, Result, TEXT("PlotSizeMaxXY.X must not be smaller than PlotSizeMinXY.X."));
	}

	if (PlotSizeMaxXY.Y > 0.0 && PlotSizeMaxXY.Y < PlotSizeMinXY.Y)
	{
		AddError(Context, Result, TEXT("PlotSizeMaxXY.Y must not be smaller than PlotSizeMinXY.Y."));
	}

	if (MaxHierarchy < MinHierarchy)
	{
		AddError(Context, Result, TEXT("MaxHierarchy must not be smaller than MinHierarchy."));
	}

	if (SelectionWeight < 0.0f)
	{
		AddError(Context, Result, TEXT("SelectionWeight must be non-negative."));
	}

	if (BuildingTags.IsEmpty())
	{
		AddError(Context, Result, TEXT("BuildingTags must not be empty."));
	}

	if (!AllTagsUnder(PreferredDistricts, DistrictRoot))
	{
		AddError(Context, Result, TEXT("PreferredDistricts must only contain tags under Town.District."));
	}

	if (!AllTagsUnder(PreferredBuildings, BuildingRoot))
	{
		AddError(Context, Result, TEXT("PreferredBuildings must only contain tags under Town.Building."));
	}

	if (!AllTagsUnder(DenyBuildings, BuildingRoot))
	{
		AddError(Context, Result, TEXT("DenyBuildings must only contain tags under Town.Building."));
	}

	if (InfluenceRadius <= 0.0)
	{
		AddError(Context, Result, TEXT("InfluenceRadius must be greater than zero."));
	}

	if (PreferredBuildings.HasAny(DenyBuildings))
	{
		AddError(Context, Result, TEXT("PreferredBuildings and DenyBuildings must not share a tag."));
	}

	return Result;
}

#endif
