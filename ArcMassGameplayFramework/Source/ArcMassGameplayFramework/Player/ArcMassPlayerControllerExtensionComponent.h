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

#include "Components/ControllerComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"

#include "ArcMassPlayerControllerExtensionComponent.generated.h"

class UGameFrameworkComponentManager;
struct FActorInitStateChangedParams;

/**
 * UArcMassPlayerControllerExtensionComponent
 *
 * PlayerController-side feature for the Mass-driven game framework. Drives the
 * GameFramework init state machine:
 *
 *   Spawned -> DataAvailable: PC is valid and either has authority or is locally
 *       controlled.
 *   DataAvailable -> DataInitialized: all sibling features on the PC have
 *       reached DataAvailable.
 *   DataInitialized -> GameplayReady: all sibling features on the PC have
 *       reached DataInitialized.
 */
UCLASS(ClassGroup = (ArcMass), meta = (BlueprintSpawnableComponent))
class ARCMASSGAMEPLAYFRAMEWORK_API UArcMassPlayerControllerExtensionComponent
		: public UControllerComponent
		, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	UArcMassPlayerControllerExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Feature name used by IGameFrameworkInitStateInterface. */
	static const FName NAME_ActorFeatureName;

	//~ Begin IGameFrameworkInitStateInterface interface
	virtual FName GetFeatureName() const override
	{
		return NAME_ActorFeatureName;
	}

	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager
									, FGameplayTag CurrentState
									, FGameplayTag DesiredState) const override;

	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager
									   , FGameplayTag CurrentState
									   , FGameplayTag DesiredState) override;

	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;

	virtual void CheckDefaultInitialization() override;
	//~ End IGameFrameworkInitStateInterface interface

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
};
