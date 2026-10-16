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

#include "Kismet/BlueprintAsyncActionBase.h"
#include "UObject/ObjectMacros.h"

#include "AsyncAction_MassExperienceReady.generated.h"

class AGameStateBase;
class UArcMassExperienceDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FArcMassExperienceReadyAsyncDelegate);

/**
 * UAsyncAction_MassExperienceReady
 *
 * Blueprint async node that waits for the GameState to be valid and the active
 * experience to finish loading, then fires OnReady. Calls OnReady on the next
 * tick if the experience is already loaded when the action activates.
 */
UCLASS()
class ARCMASSGAMEPLAYFRAMEWORK_API UAsyncAction_MassExperienceReady : public UBlueprintAsyncActionBase
{
	GENERATED_UCLASS_BODY()

public:
	/** Waits for the experience to be determined and loaded. */
	UFUNCTION(BlueprintCallable
		, meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UAsyncAction_MassExperienceReady* WaitForMassExperienceReady(UObject* WorldContextObject);

	virtual void Activate() override;

public:
	/** Fired when the experience has been determined and is ready/loaded. */
	UPROPERTY(BlueprintAssignable)
	FArcMassExperienceReadyAsyncDelegate OnReady;

private:
	void Step1_HandleGameStateSet(AGameStateBase* GameState);

	void Step2_ListenToExperienceLoading(AGameStateBase* GameState);

	void Step3_HandleExperienceLoaded(const UArcMassExperienceDefinition* CurrentExperience);

	void Step4_BroadcastReady();

	TWeakObjectPtr<UWorld> WorldPtr;
};
