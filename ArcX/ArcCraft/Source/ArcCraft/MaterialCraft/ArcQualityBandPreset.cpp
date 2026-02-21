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

#include "ArcCraft/MaterialCraft/ArcQualityBandPreset.h"

#include "Misc/DataValidation.h"

#if WITH_EDITOR

EDataValidationResult UArcQualityBandPreset::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (QualityBands.Num() == 0)
	{
		Context.AddWarning(FText::FromString(
			FString::Printf(TEXT("Quality band preset '%s' has no bands."),
				*PresetName.ToString())));
	}

	for (int32 BandIdx = 0; BandIdx < QualityBands.Num(); ++BandIdx)
	{
		const FArcMaterialQualityBand& Band = QualityBands[BandIdx];
		const FString BandName = Band.BandName.IsEmpty()
			? FString::Printf(TEXT("Band[%d]"), BandIdx)
			: Band.BandName.ToString();

		if (Band.BaseWeight <= 0.0f)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: BaseWeight must be > 0 (got %.3f)."),
					*BandName, Band.BaseWeight)));
			Result = EDataValidationResult::Invalid;
		}

		if (Band.Modifiers.Num() == 0)
		{
			Context.AddWarning(FText::FromString(
				FString::Printf(TEXT("%s: Band has no modifiers."), *BandName)));
		}

		if (Band.MinQuality < 0.0f)
		{
			Context.AddError(FText::FromString(
				FString::Printf(TEXT("%s: MinQuality must be >= 0 (got %.3f)."),
					*BandName, Band.MinQuality)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}

#endif
