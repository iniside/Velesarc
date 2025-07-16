/**
 * This file is part of ArcX.
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

#include "ArcCore/AbilitySystem/GameplayCues/ArcCueTranslatorSettings.h"

UArcCueTranslatorSettings::UArcCueTranslatorSettings()
{
}

void UArcCueTranslatorSettings::PostInitProperties()
{
	Super::PostInitProperties();

	FString D = GetDefaultConfigFilename();
	LoadConfig(GetClass()
		, *D);
}

/** Gets the settings container name for the settings, either Project or Editor */
FName UArcCueTranslatorSettings::GetContainerName() const
{
	return TEXT("Project");
}

/** Gets the category for the settings, some high level grouping like, Editor, Engine,
 * Game...etc. */
FName UArcCueTranslatorSettings::GetCategoryName() const
{
	return TEXT("Arc Game Core");
}

/** The unique name for your section of settings, uses the class's FName. */
FName UArcCueTranslatorSettings::GetSectionName() const
{
	return TEXT("Gameplay Cues");
}

#if WITH_EDITOR
void UArcCueTranslatorSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/** Gets the section text, uses the classes DisplayName by default. */
FText UArcCueTranslatorSettings::GetSectionText() const
{
	return FText::FromString("Gameplay Cues");
}

/** Gets the description for the section, uses the classes ToolTip by default. */
FText UArcCueTranslatorSettings::GetSectionDescription() const
{
	return FText::FromString("Gameplay Cues Translator Settings");
}
#endif
