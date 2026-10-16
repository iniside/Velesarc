// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcEconomyNames.h"

#include "ArcCraft/Recipe/ArcRecipeDefinitionData.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "Core/ArcCoreAssetManager.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/Fragments/ArcItemFragment_UIDescription.h"
#include "Mass/EntityManager.h"
#include "Misc/Paths.h"
#include "Sim/ArcBuildingFragments.h"

namespace ArcEconomyNames
{
	FString ItemName(const FPrimaryAssetId& Item)
	{
		if (!Item.IsValid())
		{
			return FString(TEXT("(none)"));
		}

		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(Item);
		if (Def == nullptr)
		{
			return FString::Printf(TEXT("%s (missing)"), *Item.ToString());
		}

		const FArcItemFragment_UIDescription* UIDesc = Def->FindFragment<FArcItemFragment_UIDescription>();
		if (UIDesc != nullptr && !UIDesc->ItemName.IsEmpty())
		{
			return UIDesc->ItemName.ToString();
		}

		return FPaths::GetBaseFilename(UArcCoreAssetManager::Get().GetJsonRecordFilename(Item));
	}

	FString RecipeName(const FPrimaryAssetId& RecipeId)
	{
		if (!RecipeId.IsValid())
		{
			return FString(TEXT("(none)"));
		}

		const FArcRecipeDefinitionData* Recipe = ArcCraftRecords::Find<FArcRecipeDefinitionData>(RecipeId);
		if (Recipe == nullptr)
		{
			return FString::Printf(TEXT("%s (missing)"), *RecipeId.ToString());
		}

		const FText DisplayName = Recipe->GetDisplayName();
		if (DisplayName.IsEmpty())
		{
			return FPaths::GetBaseFilename(UArcCoreAssetManager::Get().GetJsonRecordFilename(RecipeId));
		}

		return DisplayName.ToString();
	}

	FString BuildingName(const FMassEntityManager& Manager, FMassEntityHandle Entity)
	{
		if (!Entity.IsValid() || !Manager.IsEntityActive(Entity))
		{
			return FString(TEXT("---"));
		}

		const FArcWorldSimLabelFragment* Label = Manager.GetConstSharedFragmentDataPtr<FArcWorldSimLabelFragment>(Entity);
		if (Label == nullptr)
		{
			return Entity.DebugGetDescription();
		}

		if (Label->DisplayName.IsEmpty())
		{
			// "(unnamed)" means empty trait field, not missing entity.
			return FString(TEXT("(unnamed)"));
		}

		return Label->DisplayName;
	}
}
