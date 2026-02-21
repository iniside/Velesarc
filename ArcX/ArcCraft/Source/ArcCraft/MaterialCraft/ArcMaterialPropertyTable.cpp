/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTable.h"
#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"

#include "Misc/DataValidation.h"

#if WITH_EDITOR

EDataValidationResult UArcMaterialPropertyTable::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Rules.Num() == 0)
	{
		Context.AddWarning(FText::FromString(
			FString::Printf(TEXT("Material property table '%s' has no rules."),
				*TableName.ToString())));
	}

	for (int32 RuleIdx = 0; RuleIdx < Rules.Num(); ++RuleIdx)
	{
		const FArcMaterialPropertyRule& Rule = Rules[RuleIdx];
		const FString RuleName = Rule.RuleName.IsEmpty()
			? FString::Printf(TEXT("Rule[%d]"), RuleIdx)
			: Rule.RuleName.ToString();

		if (Rule.TagQuery.IsEmpty())
		{
			Context.AddWarning(FText::FromString(
				FString::Printf(TEXT("%s: TagQuery is empty — rule will never match."), *RuleName)));
		}

		// Validate effective quality bands (inline or from preset)
		const TArray<FArcMaterialQualityBand>& Bands = Rule.GetEffectiveQualityBands();

		if (!Rule.QualityBandPreset.IsNull() && Rule.QualityBandPreset.LoadSynchronous() == nullptr)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: QualityBandPreset is set but cannot be loaded."), *RuleName)));
			Result = EDataValidationResult::Invalid;
		}

		if (Bands.Num() == 0)
		{
			Context.AddWarning(FText::FromString(
				FString::Printf(TEXT("%s: No quality bands defined (inline or preset)."), *RuleName)));
		}

		for (int32 BandIdx = 0; BandIdx < Bands.Num(); ++BandIdx)
		{
			const FArcMaterialQualityBand& Band = Bands[BandIdx];
			const FString BandName = Band.BandName.IsEmpty()
				? FString::Printf(TEXT("Band[%d]"), BandIdx)
				: Band.BandName.ToString();

			if (Band.BaseWeight <= 0.0f)
			{
				Context.AddError(FText::FromString(
					FString::Printf(TEXT("%s / %s: BaseWeight must be > 0 (got %.3f)."),
						*RuleName, *BandName, Band.BaseWeight)));
				Result = EDataValidationResult::Invalid;
			}

			if (Band.Modifiers.Num() == 0)
			{
				Context.AddWarning(FText::FromString(
					FString::Printf(TEXT("%s / %s: Band has no modifiers."),
						*RuleName, *BandName)));
			}

			if (Band.MinQuality < 0.0f)
			{
				Context.AddError(FText::FromString(
					FString::Printf(TEXT("%s / %s: MinQuality must be >= 0 (got %.3f)."),
						*RuleName, *BandName, Band.MinQuality)));
				Result = EDataValidationResult::Invalid;
			}
		}

		if (Rule.MaxContributions < 1)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: MaxContributions must be >= 1 (got %d)."),
					*RuleName, Rule.MaxContributions)));
			Result = EDataValidationResult::Invalid;
		}
	}

	if (BaseBandBudget < 0.0f)
	{
		Context.AddError(FText::FromString(
			FString::Printf(TEXT("BaseBandBudget must be >= 0 (got %.3f)."), BaseBandBudget)));
		Result = EDataValidationResult::Invalid;
	}

	if (BudgetPerQuality < 0.0f)
	{
		Context.AddError(FText::FromString(
			FString::Printf(TEXT("BudgetPerQuality must be >= 0 (got %.3f)."), BudgetPerQuality)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
