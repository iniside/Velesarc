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

#include "Components/GameFrameworkComponent.h"
#include "Components/GameFrameworkInitStateInterface.h"
#include "ArcItemsComponent.generated.h"

/*
 * TODO Possibly delete this component, or treat is bare bones base implementation of
 * accessing items from ItemsStore Where other components can use it as base for
 * implementation if they also need to interact with items store.
 *
 * Right now there is to many functionality packed and it will be removed anyway.
 */
////
UCLASS(Blueprintable, ClassGroup = (Arc), meta = (BlueprintSpawnableComponent))
class ARCCORE_API UArcItemsComponent : public UGameFrameworkComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcItemsComponent(const FObjectInitializer& ObjectInitializer);

	static const FName NAME_ActorFeatureName;
	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	
protected:
	virtual void OnRegister() override;

	// Called when the game starts
	virtual void BeginPlay() override;

};
