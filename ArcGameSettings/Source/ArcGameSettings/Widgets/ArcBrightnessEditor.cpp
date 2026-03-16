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
 * distributed under the LICENSE is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "Widgets/ArcBrightnessEditor.h"

#include "Engine/Engine.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBrightnessEditor)

void UArcBrightnessEditor::SetBrightness(float InValue)
{
	BrightnessValue = FMath::Clamp(InValue, 0.5f, 5.0f);

	if (GEngine)
	{
		GEngine->DisplayGamma = BrightnessValue;
	}

	OnBrightnessChanged(BrightnessValue);
}

void UArcBrightnessEditor::ApplyAndClose()
{
	// Persist via Model when detailed implementation is wired up.
	// BrightnessValue is already applied live via GEngine->DisplayGamma.
	DeactivateWidget();
}

void UArcBrightnessEditor::CancelAndClose()
{
	if (GEngine)
	{
		GEngine->DisplayGamma = InitialBrightness;
	}

	DeactivateWidget();
}
