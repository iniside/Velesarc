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

#include "Engine/DeveloperSettings.h"

#include "ArcMassGameFrameworkTestSettings.generated.h"

class APawn;
class UInputMappingContext;
class UMassEntityConfigAsset;
class UWorld;
class UArcMassExperienceDefinition;
class UArcMassInputActionConfig;
class UArcMassPawnData;

/**
 * Editable test fixtures for ArcMassGameFramework's CQTest suite. Tests pull asset
 * references from this settings object instead of hard-coding /Game/... paths in C++,
 * so the suite stays buildable on a clean checkout (fields default to null) while
 * letting a developer point it at real assets locally.
 *
 * Stored in DefaultGame.ini under [/Script/ArcMassGameFrameworkTest.ArcMassGameFrameworkTestSettings].
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "ArcMass Test Fixtures"))
class ARCMASSGAMEFRAMEWORKTEST_API UArcMassGameFrameworkTestSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UArcMassGameFrameworkTestSettings();

	//~UDeveloperSettings interface
	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Arc Game"));
	}

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	//~End of UDeveloperSettings interface

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftClassPtr<APawn> TestPawnClass;

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftObjectPtr<UMassEntityConfigAsset> TestEntityConfig;

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftObjectPtr<UArcMassExperienceDefinition> TestExperience;

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftObjectPtr<UArcMassPawnData> TestPawnData;

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftObjectPtr<UInputMappingContext> TestInputMappingContext;

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftObjectPtr<UArcMassInputActionConfig> TestInputActionConfig;

	UPROPERTY(EditAnywhere, Config, Category = "Test Fixtures")
	TSoftObjectPtr<UWorld> TestMap;
};
