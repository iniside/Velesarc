/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "ArcCraft/Recipe/ArcRandomPoolDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UArcRandomPoolDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (Entries.Num() == 0)
	{
		Context.AddWarning(FText::FromString(TEXT("Random pool has no entries.")));
	}

	for (int32 Idx = 0; Idx < Entries.Num(); ++Idx)
	{
		const FArcRandomPoolEntry& Entry = Entries[Idx];

		if (Entry.Modifiers.Num() == 0)
		{
			Context.AddWarning(FText::Format(
				FText::FromString(TEXT("Pool entry {0} has no modifiers.")),
				FText::AsNumber(Idx)));
		}

		if (Entry.BaseWeight <= 0.0f)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Pool entry {0} has zero or negative base weight.")),
				FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}

		if (Entry.Cost < 0.0f)
		{
			Context.AddError(FText::Format(
				FText::FromString(TEXT("Pool entry {0} has negative cost.")),
				FText::AsNumber(Idx)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
