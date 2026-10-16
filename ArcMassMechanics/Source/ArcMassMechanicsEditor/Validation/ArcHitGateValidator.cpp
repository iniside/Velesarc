// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsEditor/Validation/ArcHitGateValidator.h"

#include "ArcMassMechanics/Combat/ArcMechHitGate.h"
#include "Effects/ArcEffectDefinition.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcHitGateValidator"

bool UArcHitGateValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UArcEffectDefinition>();
}

EDataValidationResult UArcHitGateValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	bool bFailed = false;

	if (const UArcEffectDefinition* Definition = Cast<UArcEffectDefinition>(InAsset))
	{
		const int32 GateCount = Definition->Components.FilterByPredicate([](const FInstancedStruct& Component)
		{
			return Component.GetPtr<FArcMechHitGate>() != nullptr;
		}).Num();

		if (GateCount > 1)
		{
			AssetFails(InAsset, LOCTEXT("MultipleGates", "Every Hit Gate rolls the defenses on its own; keep one Hit Gate."));
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
