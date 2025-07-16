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

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "ArcExperienceManager.generated.h"

/**
 * Manager for experiences - primarily for arbitration between multiple PIE sessions
 */
UCLASS()
class ARCCORE_API UArcExperienceManager : public UEngineSubsystem
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
	// The map of requests to active count for a given experience
	// (to allow first in, last out load management during PIE)
	TMap<FPrimaryAssetId, int32> ExperienceRequestCountMap;

private:
	void OnAssetManagerCreated();

public:
	UFUNCTION(BlueprintCallable
		, Category = "Arc Core")
	FPrimaryAssetId GetPrimaryAssetIdFromUserFacingExperienceName(const FString& AdvertisedExperienceID);
};
