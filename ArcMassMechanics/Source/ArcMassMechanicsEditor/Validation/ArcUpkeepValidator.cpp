// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsEditor/Validation/ArcUpkeepValidator.h"

#include "ArcMassMechanics/Statuses/ArcStatusTasks.h"
#include "Effects/ArcEffectDefinition.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcUpkeepValidator"

bool UArcUpkeepValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	const UArcEffectDefinition* Definition = Cast<UArcEffectDefinition>(InObject);
	return Definition && Definition->Tasks.ContainsByPredicate([](const FInstancedStruct& Task)
	{
		return Task.GetPtr<FArcEffectTask_Upkeep>() != nullptr;
	});
}

EDataValidationResult UArcUpkeepValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UArcEffectDefinition* Definition = CastChecked<UArcEffectDefinition>(InAsset);
	bool bFailed = false;

	for (int32 TaskIndex = 0; TaskIndex < Definition->Tasks.Num(); ++TaskIndex)
	{
		const FArcEffectTask_Upkeep* Upkeep = Definition->Tasks[TaskIndex].GetPtr<FArcEffectTask_Upkeep>();
		if (!Upkeep)
		{
			continue;
		}

		if (!Upkeep->Resource.IsValid())
		{
			AssetFails(InAsset, FText::Format(LOCTEXT("NoResource", "Task {0} (Upkeep) has no Resource; the payer must hold the amount in it."), FText::AsNumber(TaskIndex)));
			bFailed = true;
		}
		if (!Upkeep->Meta.IsValid())
		{
			AssetFails(InAsset, FText::Format(LOCTEXT("NoMeta", "Task {0} (Upkeep) has no Meta; the upkeep writes the amount to the Meta, whose handler spends the resource, and fails without it."), FText::AsNumber(TaskIndex)));
			bFailed = true;
		}
	}

	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
