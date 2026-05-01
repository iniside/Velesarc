// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyEditor/Contributors/ArcBuildingProductionDrawContributor.h"
#include "EditorVisualization/ArcEditorEntityDrawComponent.h"
#include "Mass/ArcEconomyBuildingTrait.h"
#include "Mass/ArcEconomyFragments.h"
#include "Items/ArcItemDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"

namespace ArcBuildingProductionDrawContributor
{
	static const FColor LabelColor = FColor(255, 220, 80);
	static const FColor GatherColor = FColor(100, 220, 100);
	static const float LabelLineHeight = 120.f;
	static const float LabelBaseOffsetZ = 200.f;

	static FString GetOutputName(const FArcNamedPrimaryAssetId& NamedId)
	{
#if WITH_EDITORONLY_DATA
		if (!NamedId.DisplayName.IsEmpty())
		{
			return NamedId.DisplayName;
		}
#endif
		return NamedId.AssetId.PrimaryAssetName.ToString();
	}
} // namespace ArcBuildingProductionDrawContributor

TSubclassOf<UMassEntityTraitBase> FArcBuildingProductionDrawContributor::GetTraitClass() const
{
	return UArcEconomyBuildingTrait::StaticClass();
}

FString FArcBuildingProductionDrawContributor::FormatRecipeInfo(const UArcRecipeDefinition* Recipe) const
{
	if (!Recipe)
	{
		return TEXT("<null recipe>");
	}

	const FString OutputName = ArcBuildingProductionDrawContributor::GetOutputName(Recipe->OutputItemDefinition);
	FString Result = FString::Printf(TEXT("%dx %s"), Recipe->OutputAmount, *OutputName);

	// Collect ingredient descriptions
	TArray<FString> IngredientParts;
	for (const FInstancedStruct& IngredientStruct : Recipe->Ingredients)
	{
		const FArcRecipeIngredient* Base = IngredientStruct.GetPtr<FArcRecipeIngredient>();
		if (!Base)
		{
			continue;
		}

		const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>();
		if (ItemDef)
		{
			const FString IngredientName = ArcBuildingProductionDrawContributor::GetOutputName(ItemDef->ItemDefinitionId);
			IngredientParts.Add(FString::Printf(TEXT("%dx %s"), Base->Amount, *IngredientName));
		}
		else
		{
			// Tag-based or other ingredient — show slot name if available
			const FString SlotLabel = Base->SlotName.IsEmpty()
				? TEXT("ingredient")
				: Base->SlotName.ToString();
			IngredientParts.Add(FString::Printf(TEXT("%dx %s"), Base->Amount, *SlotLabel));
		}
	}

	if (IngredientParts.Num() > 0)
	{
		Result += TEXT(" (needs: ");
		Result += FString::Join(IngredientParts, TEXT(", "));
		Result += TEXT(")");
	}

	return Result;
}

void FArcBuildingProductionDrawContributor::CollectShapes(
	UArcEditorEntityDrawComponent& DrawComponent,
	const UMassEntityTraitBase* Trait,
	TConstArrayView<FTransform> Transforms,
	const FArcEditorDrawContext& Context)
{
	const UArcEconomyBuildingTrait* BuildingTrait = Cast<UArcEconomyBuildingTrait>(Trait);
	if (!BuildingTrait)
	{
		return;
	}

	const FArcBuildingFragment& BuildingConfig = BuildingTrait->BuildingConfig;
	const FArcBuildingEconomyConfig& EconomyConfig = BuildingTrait->EconomyConfig;

	// Build label lines
	TArray<FString> LabelLines;
	LabelLines.Add(BuildingConfig.BuildingName.ToString());

	if (EconomyConfig.IsGatheringBuilding())
	{
		// Gathering building — list gathered items
		for (const TObjectPtr<UArcItemDefinition>& Item : EconomyConfig.GatherOutputItems)
		{
			if (!Item)
			{
				continue;
			}
			LabelLines.Add(FString::Printf(TEXT("[gather] %s"), *Item->GetName()));
		}
	}
	else
	{
		// Production building — list recipes
		for (const TObjectPtr<UArcRecipeDefinition>& Recipe : EconomyConfig.AllowedRecipes)
		{
			LabelLines.Add(FormatRecipeInfo(Recipe.Get()));
		}
	}

	for (const FTransform& Transform : Transforms)
	{
		const FVector Base = Transform.GetLocation();

		for (int32 LineIndex = 0; LineIndex < LabelLines.Num(); ++LineIndex)
		{
			const FVector LabelPos = Base + FVector(
				0.f,
				0.f,
				ArcBuildingProductionDrawContributor::LabelBaseOffsetZ
					+ static_cast<float>(LineIndex) * ArcBuildingProductionDrawContributor::LabelLineHeight);

			const FColor LineColor = (LineIndex == 0)
				? ArcBuildingProductionDrawContributor::LabelColor
				: (EconomyConfig.IsGatheringBuilding()
					? ArcBuildingProductionDrawContributor::GatherColor
					: ArcBuildingProductionDrawContributor::LabelColor);

			DrawComponent.AddText(LabelPos, LabelLines[LineIndex], LineColor);
		}
	}
}
