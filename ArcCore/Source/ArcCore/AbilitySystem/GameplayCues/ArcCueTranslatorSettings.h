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

#pragma once
#include "ArcCueTranslatorTypes.h"

#include "Engine/DeveloperSettings.h"
#include "Fonts/SlateFontInfo.h"
#include "ArcCueTranslatorSettings.generated.h"

UCLASS(config = CueTranslator
	, DefaultConfig)
class ARCCORE_API UArcCueTranslatorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly
		, Config
		, Category = "Floating Text")
	TArray<FArcTranslatorUnit> TranslatorUnit;

	UPROPERTY(EditDefaultsOnly
		, Config
		, Category = "Floating Text")
	TArray<FArcTranslationUnit> TranslationUnits;

public:
	UArcCueTranslatorSettings();

	virtual void PostInitProperties() override;

	/** Gets the settings container name for the settings, either Project or Editor */
	virtual FName GetContainerName() const override;

	/** Gets the category for the settings, some high level grouping like, Editor, Engine,
	 * Game...etc. */
	virtual FName GetCategoryName() const override;

	/** The unique name for your section of settings, uses the class's FName. */
	virtual FName GetSectionName() const override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Gets the section text, uses the classes DisplayName by default. */
	virtual FText GetSectionText() const override;

	/** Gets the description for the section, uses the classes ToolTip by default. */
	virtual FText GetSectionDescription() const override;
#endif
};
