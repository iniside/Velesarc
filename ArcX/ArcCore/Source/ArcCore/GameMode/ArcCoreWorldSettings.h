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


#include "ArcNamedPrimaryAssetId.h"
#include "GameFramework/WorldSettings.h"
#include "ArcCoreWorldSettings.generated.h"

class UArcPlaylistDefinition;

/**
 * The default world settings object, used primarily to set the default game mode in PIE
 * if one isn't specified in developer settings
 */
UCLASS()
class ARCCORE_API AArcCoreWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	AArcCoreWorldSettings(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif

protected:
	// The default experience to use for this map during PIE /Script/ArcCore.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = GameMode, meta = (AllowedClasses = "/Script/ArcCore.ArcExperienceDefinition", DisplayThumbnail = false))
	FArcNamedPrimaryAssetId DefaultExperience;

public:
	const FPrimaryAssetId& GetDefaultExperience() const
	{
		return DefaultExperience;
	}

#if WITH_EDITORONLY_DATA
	// Is this level part of a front-end or other standalone experience?
	// When set, the net mode will be forced to Standalone when you hit Play in the editor
	UPROPERTY(EditDefaultsOnly
		, Category = PIE)
	bool ForceStandaloneNetMode = false;
#endif
};
