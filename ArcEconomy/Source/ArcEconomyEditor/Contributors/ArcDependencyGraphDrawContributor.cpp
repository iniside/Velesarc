// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEconomyEditor/Contributors/ArcDependencyGraphDrawContributor.h"
#include "EditorVisualization/ArcEditorEntityDrawComponent.h"
#include "Mass/ArcSettlementTrait.h"
#include "Mass/ArcEconomyBuildingTrait.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcCraft/Recipe/ArcRecipeDefinition.h"
#include "ArcCraft/Recipe/ArcRecipeIngredient.h"
#include "Items/ArcItemDefinition.h"
#include "MassEntityConfigAsset.h"

namespace ArcDependencyGraphDrawContributor
{
	static const FColor ArrowColor = FColor(100, 150, 255);
	static const FColor MissingSupplyColor = FColor(255, 60, 60);
	static const FColor BuildingNameColor = FColor(200, 200, 255);
	static const float ZOffset = 100.f;
	static const float ArrowSize = 40.f;

	static FPrimaryAssetId GetOutputAssetId(const UArcRecipeDefinition* Recipe)
	{
		if (!Recipe)
		{
			return FPrimaryAssetId();
		}
		return Recipe->OutputItemDefinition.AssetId;
	}
} // namespace ArcDependencyGraphDrawContributor

void FArcDependencyGraphDrawContributor::CollectShapes(
	UArcEditorEntityDrawComponent& DrawComponent,
	const UMassEntityTraitBase* Trait,
	TConstArrayView<FTransform> Transforms,
	const FArcEditorDrawContext& Context)
{
	// Phase 1: Collect all settlement instances
	TArray<FSettlementInstance> Settlements;

	for (const FArcEditorDrawContext::FPlacedEntityInfo& Info : Context.AllPlacedEntities)
	{
		if (!Info.Config)
		{
			continue;
		}

		const UMassEntityTraitBase* FoundTrait = Info.Config->FindTrait(UArcSettlementTrait::StaticClass());
		if (!FoundTrait)
		{
			continue;
		}

		const UArcSettlementTrait* SettlementTrait = Cast<UArcSettlementTrait>(FoundTrait);
		if (!SettlementTrait)
		{
			continue;
		}

		const float Radius = SettlementTrait->SettlementConfig.SettlementRadius;

		for (const FTransform& Transform : Info.Transforms)
		{
			FSettlementInstance& Settlement = Settlements.AddDefaulted_GetRef();
			Settlement.Location = Transform.GetLocation();
			Settlement.Radius = Radius;
		}
	}

	// Phase 2: Collect all building instances
	TArray<FBuildingInstance> Buildings;

	for (const FArcEditorDrawContext::FPlacedEntityInfo& Info : Context.AllPlacedEntities)
	{
		if (!Info.Config)
		{
			continue;
		}

		const UMassEntityTraitBase* FoundTrait = Info.Config->FindTrait(UArcEconomyBuildingTrait::StaticClass());
		if (!FoundTrait)
		{
			continue;
		}

		const UArcEconomyBuildingTrait* BuildingTrait = Cast<UArcEconomyBuildingTrait>(FoundTrait);
		if (!BuildingTrait)
		{
			continue;
		}

		const FArcBuildingEconomyConfig& EconomyConfig = BuildingTrait->EconomyConfig;

		// Collect produced item IDs from recipes
		TArray<FPrimaryAssetId> ProducedItems;
		for (const TObjectPtr<UArcRecipeDefinition>& Recipe : EconomyConfig.AllowedRecipes)
		{
			if (!Recipe)
			{
				continue;
			}
			const FPrimaryAssetId OutputId = ArcDependencyGraphDrawContributor::GetOutputAssetId(Recipe.Get());
			if (OutputId.IsValid())
			{
				ProducedItems.AddUnique(OutputId);
			}
		}

		// Collect produced items from gathering
		for (const TObjectPtr<UArcItemDefinition>& GatherItem : EconomyConfig.GatherOutputItems)
		{
			if (!GatherItem)
			{
				continue;
			}
			const FPrimaryAssetId GatherId = GatherItem->GetPrimaryAssetId();
			if (GatherId.IsValid())
			{
				ProducedItems.AddUnique(GatherId);
			}
		}

		// Collect consumed item IDs from recipe ingredients (item-def based ingredients only)
		TArray<FPrimaryAssetId> ConsumedItems;
		for (const TObjectPtr<UArcRecipeDefinition>& Recipe : EconomyConfig.AllowedRecipes)
		{
			if (!Recipe)
			{
				continue;
			}
			for (const FInstancedStruct& IngredientStruct : Recipe->Ingredients)
			{
				const FArcRecipeIngredient_ItemDef* ItemDef = IngredientStruct.GetPtr<FArcRecipeIngredient_ItemDef>();
				if (!ItemDef)
				{
					continue;
				}
				const FPrimaryAssetId IngredientId = ItemDef->ItemDefinitionId.AssetId;
				if (IngredientId.IsValid())
				{
					ConsumedItems.AddUnique(IngredientId);
				}
			}
		}

		// Collect consumed items from ConsumptionNeeds
		for (const FArcBuildingConsumptionEntry& Need : EconomyConfig.ConsumptionNeeds)
		{
			if (!Need.Item)
			{
				continue;
			}
			const FPrimaryAssetId NeedId = Need.Item->GetPrimaryAssetId();
			if (NeedId.IsValid())
			{
				ConsumedItems.AddUnique(NeedId);
			}
		}

		const FString BuildingName = BuildingTrait->BuildingConfig.BuildingName.ToString();

		for (const FTransform& Transform : Info.Transforms)
		{
			FBuildingInstance& Building = Buildings.AddDefaulted_GetRef();
			Building.Location = Transform.GetLocation();
			Building.BuildingName = BuildingName;
			Building.ProducedItems = ProducedItems;
			Building.ConsumedItems = ConsumedItems;

			// Associate with nearest enclosing settlement
			float BestDistSq = MAX_FLT;
			int32 BestIndex = INDEX_NONE;
			for (int32 SettlementIdx = 0; SettlementIdx < Settlements.Num(); ++SettlementIdx)
			{
				const FSettlementInstance& Settlement = Settlements[SettlementIdx];
				const float DistSq = FVector::DistSquared2D(Building.Location, Settlement.Location);
				const float RadiusSq = Settlement.Radius * Settlement.Radius;
				if (DistSq <= RadiusSq && DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestIndex = SettlementIdx;
				}
			}
			Building.SettlementIndex = BestIndex;
		}
	}

	// Phase 3: Draw building name labels
	for (const FBuildingInstance& Building : Buildings)
	{
		if (!Building.BuildingName.IsEmpty())
		{
			const FVector TextLocation = Building.Location + FVector(0.f, 0.f, ArcDependencyGraphDrawContributor::ZOffset);
			DrawComponent.AddText(TextLocation, Building.BuildingName, ArcDependencyGraphDrawContributor::BuildingNameColor);
		}
	}

	// Phase 4: Draw dependency arrows and detect missing suppliers
	// For each consumer building that needs ItemX, find producer buildings in the same settlement
	for (const FBuildingInstance& Consumer : Buildings)
	{
		if (Consumer.SettlementIndex == INDEX_NONE || Consumer.ConsumedItems.IsEmpty())
		{
			continue;
		}

		TArray<FPrimaryAssetId> UnsatisfiedItems;
		int32 TextLineIndex = 0;

		for (const FPrimaryAssetId& ConsumedItem : Consumer.ConsumedItems)
		{
			bool bFoundProducer = false;

			for (const FBuildingInstance& Producer : Buildings)
			{
				if (&Producer == &Consumer)
				{
					continue;
				}
				if (Producer.SettlementIndex != Consumer.SettlementIndex)
				{
					continue;
				}
				if (!Producer.ProducedItems.Contains(ConsumedItem))
				{
					continue;
				}

				bFoundProducer = true;
				const FVector Start = Producer.Location + FVector(0.f, 0.f, ArcDependencyGraphDrawContributor::ZOffset);
				const FVector End = Consumer.Location + FVector(0.f, 0.f, ArcDependencyGraphDrawContributor::ZOffset);
				DrawComponent.AddArrow(Start, End, ArcDependencyGraphDrawContributor::ArrowColor, ArcDependencyGraphDrawContributor::ArrowSize);
			}

			if (!bFoundProducer)
			{
				const FVector TextLocation = Consumer.Location + FVector(0.f, 0.f, ArcDependencyGraphDrawContributor::ZOffset + 50.f + TextLineIndex * 30.f);
				const FString MissingText = FString::Printf(TEXT("Missing supplier: %s"), *ConsumedItem.PrimaryAssetName.ToString());
				DrawComponent.AddText(TextLocation, MissingText, ArcDependencyGraphDrawContributor::MissingSupplyColor);
				++TextLineIndex;
			}
		}
	}
}
