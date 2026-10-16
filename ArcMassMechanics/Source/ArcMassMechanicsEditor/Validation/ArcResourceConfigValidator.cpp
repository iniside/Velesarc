// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsEditor/Validation/ArcResourceConfigValidator.h"

#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Misc/DataValidation.h"

#define LOCTEXT_NAMESPACE "ArcResourceConfigValidator"

bool UArcResourceConfigValidator::CanValidateAsset_Implementation(const FAssetData& InAssetData, UObject* InObject, FDataValidationContext& InContext) const
{
	return InObject && InObject->IsA<UArcResourceConfig>();
}

EDataValidationResult UArcResourceConfigValidator::ValidateLoadedAsset_Implementation(const FAssetData& InAssetData, UObject* InAsset, FDataValidationContext& Context)
{
	const UArcResourceConfig* Config = CastChecked<UArcResourceConfig>(InAsset);
	bool bFailed = false;

	for (int32 Index = 0; Index < Config->Resources.Num(); ++Index)
	{
		const FArcResourceDef& Resource = Config->Resources[Index];
		TArray<FText> Errors;

		if (!Resource.Value.IsValid())
		{
			Errors.Add(LOCTEXT("NoValue", "has no Value attribute"));
		}
		if (!Resource.Rate.GetPtr<FArcResourceRate>())
		{
			Errors.Add(LOCTEXT("NoRate", "has no Rate; every resource moves relative to its Target at a Rate"));
		}
		if (const FArcResourceRate_Pips* Pips = Resource.Rate.GetPtr<FArcResourceRate_Pips>())
		{
			if (!Pips->RegenPips.IsValid())
			{
				Errors.Add(LOCTEXT("NoRegenPips", "has a Pips Rate without RegenPips"));
			}
			if (!Pips->PerPip.IsValid())
			{
				Errors.Add(LOCTEXT("NoPerPip", "has a Pips Rate without PerPip"));
			}
		}
		if (!Resource.Target.GetPtr<FArcResourceTarget>())
		{
			Errors.Add(LOCTEXT("NoTarget", "has no Target; every resource moves relative to a Target"));
		}

		for (const FText& Error : Errors)
		{
			AssetFails(InAsset, FText::Format(LOCTEXT("ResourceError", "Resource {0} {1}."), FText::AsNumber(Index), Error));
			bFailed = true;
		}

		if (!Resource.Delta.IsValid())
		{
			AssetWarning(InAsset, FText::Format(LOCTEXT("NoDelta", "Resource {0} has no Delta meta, so its steps are dropped and its value never moves. Set Delta to a meta handled by MetaToResource with Op AddSigned and Resource = Value."),
				FText::AsNumber(Index)));
		}
	}

	if (!bFailed)
	{
		AssetPasses(InAsset);
	}
	return GetValidationResult();
}

#undef LOCTEXT_NAMESPACE
