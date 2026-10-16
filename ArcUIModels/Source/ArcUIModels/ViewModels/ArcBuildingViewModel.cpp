// Copyright Lukasz Baran. All Rights Reserved.

#include "ViewModels/ArcBuildingViewModel.h"

#include "ArcBuildIngredient.h"
#include "ArcBuildingDefinition.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#define LOCTEXT_NAMESPACE "ArcBuildingViewModel"

namespace ArcBuildingVMPrivate
{
	// No `Building.Category.*` taxonomy exists in the tag config — only the
	// Building.NPC/Player/World ownership tags. TODO(user): when a category root is
	// introduced, key on it instead of this deepest-tag heuristic. Until then, pick the
	// most-specific (deepest) tag and use its leaf segment as the category label.
	// Deterministic on purpose: deepest wins, lexicographic tie-break — never rely on
	// FGameplayTagContainer iteration order.
	FText DeriveCategoryText(const FGameplayTagContainer& Tags)
	{
		FGameplayTag BestTag;
		int32 BestDepth = -1;
		FString BestString;
		for (const FGameplayTag& Tag : Tags)
		{
			const FString TagString = Tag.ToString();
			TArray<FString> Segments;
			TagString.ParseIntoArray(Segments, TEXT("."), true);
			const int32 Depth = Segments.Num();
			if (Depth > BestDepth || (Depth == BestDepth && TagString < BestString))
			{
				BestDepth = Depth;
				BestString = TagString;
				BestTag = Tag;
			}
		}

		if (!BestTag.IsValid())
		{
			return FText::GetEmpty();
		}

		TArray<FString> LeafSegments;
		BestTag.ToString().ParseIntoArray(LeafSegments, TEXT("."), true);
		const FString Leaf = LeafSegments.Num() > 0 ? LeafSegments.Last() : BestTag.ToString();
		return FText::FromString(Leaf);
	}

	// Grid-cell footprint from the definition. No occupancy-cell helper exists in the
	// builder subsystem/snapping/ghost code (only grid-snap math), so compute here.
	FIntPoint ComputeFootprint(const UArcBuildingDefinition& Def)
	{
		const double GridSize = static_cast<double>(FMath::Max(1, Def.DefaultGridSize));
		const int32 CellsX = FMath::Max(1, FMath::CeilToInt(Def.ApproximateSize.X / GridSize));
		const int32 CellsY = FMath::Max(1, FMath::CeilToInt(Def.ApproximateSize.Y / GridSize));
		return FIntPoint(CellsX, CellsY);
	}
}

void UArcBuildingViewModel::RefreshFrom(UArcBuildingDefinition* Def, const FArcMassItemStoreFragment& Inventory)
{
	if (Def == nullptr)
	{
		return;
	}

	Definition = Def;
	BuildingAssetId = Def->GetPrimaryAssetId();

	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, Def->DisplayName);
	UE_MVVM_SET_PROPERTY_VALUE(Description, Def->Description);
	UE_MVVM_SET_PROPERTY_VALUE(Icon, Def->Icon);
	UE_MVVM_SET_PROPERTY_VALUE(BuildingTags, Def->BuildingTags);

	SetCategoryText(ArcBuildingVMPrivate::DeriveCategoryText(Def->BuildingTags));

	const FIntPoint NewFootprint = ArcBuildingVMPrivate::ComputeFootprint(*Def);
	SetFootprintSize(NewFootprint);

	FFormatOrderedArguments FootprintArgs;
	FootprintArgs.Add(FText::AsNumber(NewFootprint.X));
	FootprintArgs.Add(FText::AsNumber(NewFootprint.Y));
	SetFootprintText(FText::Format(LOCTEXT("FootprintFmt", "{0} × {1}"), FootprintArgs));

	// TODO(user): bind real building health source (lives in MassEntityConfig attributes)
	SetHealthText(FText::FromString(TEXT("—")));

	TArray<TObjectPtr<UArcBuildingIngredientViewModel>> NewIngredients;
	NewIngredients.Reserve(Def->Ingredients.Num());
	bool bAllSatisfied = true;

	for (int32 i = 0; i < Def->Ingredients.Num(); ++i)
	{
		const FArcBuildIngredient* Ingredient = Def->Ingredients[i].GetPtr<FArcBuildIngredient>();
		if (Ingredient == nullptr)
		{
			continue;
		}

		// Recycle by slot index so widget entries keep stable child VMs across refreshes.
		UArcBuildingIngredientViewModel* Child = nullptr;
		if (Ingredients.IsValidIndex(i) && Ingredients[i] != nullptr)
		{
			Child = Ingredients[i].Get();
		}
		else
		{
			Child = NewObject<UArcBuildingIngredientViewModel>(this);
		}

		Child->RefreshFrom(*Ingredient, Inventory);

		if (!Child->GetbSatisfied())
		{
			bAllSatisfied = false;
		}

		NewIngredients.Add(Child);
	}

	SetIngredients(MoveTemp(NewIngredients));
	UE_MVVM_SET_PROPERTY_VALUE(bCanAfford, bAllSatisfied);
}

void UArcBuildingViewModel::SetbIsSelected(bool bNewIsSelected)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsSelected, bNewIsSelected);
}

void UArcBuildingViewModel::SetIngredients(TArray<TObjectPtr<UArcBuildingIngredientViewModel>>&& NewIngredients)
{
	UE_MVVM_SET_PROPERTY_VALUE(Ingredients, NewIngredients);
}

void UArcBuildingViewModel::SetCategoryText(const FText& NewCategoryText)
{
	UE_MVVM_SET_PROPERTY_VALUE(CategoryText, NewCategoryText);
}

void UArcBuildingViewModel::SetFootprintSize(FIntPoint NewFootprintSize)
{
	UE_MVVM_SET_PROPERTY_VALUE(FootprintSize, NewFootprintSize);
}

void UArcBuildingViewModel::SetFootprintText(const FText& NewFootprintText)
{
	UE_MVVM_SET_PROPERTY_VALUE(FootprintText, NewFootprintText);
}

void UArcBuildingViewModel::SetHealthText(const FText& NewHealthText)
{
	UE_MVVM_SET_PROPERTY_VALUE(HealthText, NewHealthText);
}

#undef LOCTEXT_NAMESPACE
