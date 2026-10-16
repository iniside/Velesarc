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

#include "GameFramework/WorldSettings.h"

#include "ArcMassWorldSettings.generated.h"

class UArcMassExperienceDefinition;

/**
 * Per-map World Settings that opt the map into a default UArcMassExperienceDefinition.
 * Resolved by AArcMassGameMode::GetExperiencePrimaryId after URL options and command-line.
 *
 * Wire this class as the project default via [/Script/Engine.Engine] WorldSettingsClass=
 * in DefaultEngine.ini.
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API AArcMassWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	AArcMassWorldSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif

	/** Resolves DefaultExperience to its FPrimaryAssetId without forcing a load. */
	FPrimaryAssetId GetDefaultExperienceId() const;

protected:
	/** Default experience used during PIE / standalone if URL+command-line don't supply one. */
	UPROPERTY(EditDefaultsOnly, Category = GameMode, meta = (AllowedClasses = "/Script/ArcMassGameplayFramework.ArcMassExperienceDefinition", DisplayThumbnail = false))
	TSoftObjectPtr<UArcMassExperienceDefinition> DefaultExperience;

#if WITH_EDITORONLY_DATA
	/** When set, the net mode is forced to Standalone when you hit Play in the editor. */
	UPROPERTY(EditDefaultsOnly, Category = PIE)
	bool ForceStandaloneNetMode = false;
#endif
};
