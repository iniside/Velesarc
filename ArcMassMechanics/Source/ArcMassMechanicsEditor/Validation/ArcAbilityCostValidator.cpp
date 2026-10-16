// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsEditor/Validation/ArcAbilityCostValidator.h"

#include "Abilities/ArcAbilityDefinition.h"
#include "ArcMassMechanics/Costs/ArcCostTypes.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcAbilityCostValidator"

namespace ArcAbilityCostValidatorPrivate
{
	void ValidateCost(const FInstancedStruct& CostStruct, const FString& Path, TArray<FText>& OutErrors)
	{
		const UScriptStruct* CostType = CostStruct.GetScriptStruct();
		if (!CostType)
		{
			OutErrors.Add(FText::Format(LOCTEXT("EmptyCost", "{0} is empty; pick a cost type or remove the entry."), FText::FromString(Path)));
			return;
		}

		if (const FArcCost_Set* Set = CostStruct.GetPtr<FArcCost_Set>())
		{
			for (int32 Index = 0; Index < Set->Costs.Num(); ++Index)
			{
				ValidateCost(Set->Costs[Index], FString::Printf(TEXT("%s.Costs[%d]"), *Path, Index), OutErrors);
			}
		}
	}
}

bool UArcAbilityCostValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	const UArcAbilityDefinition* Ability = Cast<UArcAbilityDefinition>(InObject);
	return Ability && Ability->Cost.IsValid();
}

EDataValidationResult UArcAbilityCostValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UArcAbilityDefinition* Ability = CastChecked<UArcAbilityDefinition>(InAsset);

	TArray<FText> Errors;
	ArcAbilityCostValidatorPrivate::ValidateCost(Ability->Cost, TEXT("Cost"), Errors);
	for (const FText& Error : Errors)
	{
		AssetFails(InAsset, Error);
	}

	if (Errors.IsEmpty())
	{
		AssetPasses(InAsset);
	}
	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
