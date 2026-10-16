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

#include "Engine/AssetManager.h"
#include "Subsystems/EngineSubsystem.h"

#include "ArcMassExperienceManager.generated.h"

/**
 * UArcMassExperienceManager
 *
 * Engine subsystem providing arbitration between multiple PIE sessions activating
 * the same experience (first-in / last-out semantics for chaos-testing taps).
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassExperienceManager : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	//~USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	//~End of USubsystem interface

#if WITH_EDITOR
	void OnPlayInEditorBegun();

	void OnPlayInEditorFinished();

	static bool RequestToPerformActions(FPrimaryAssetId ExperienceID);

	static bool RequestToRemoveActions(FPrimaryAssetId ExperienceID);
#else
	static bool RequestToPerformActions(FPrimaryAssetId ExperienceID)
	{
		return true;
	}
	static bool RequestToRemoveActions(FPrimaryAssetId ExperienceID)
	{
		return true;
	}
#endif

private:
	/** Map of requests to active count per experience (first in / last out). */
	TMap<FPrimaryAssetId, int32> ExperienceRequestCountMap;

	void OnAssetManagerCreated();
};
