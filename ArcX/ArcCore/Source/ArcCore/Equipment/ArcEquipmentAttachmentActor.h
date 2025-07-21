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


#include "Items/ArcItemId.h"
#include "GameFramework/Actor.h"
#include "ArcEquipmentAttachmentActor.generated.h"

/**
 * @class AArcEquipmentAttachmentActor
 *
 * @brief The class AArcEquipmentAttachmentActor is an actor class that represents an equipment attachment in the game.
 * It is created only on clients and standalone by default.
 *
 * It inherits from the AActor class and provides functionality for setting default values, handling game start events, and handling frame updates.
 */
UCLASS()
class ARCCORE_API AArcEquipmentAttachmentActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AArcEquipmentAttachmentActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};

class UArcItemsStoreComponent;

UINTERFACE()
class ARCCORE_API UArcEquipmentAttachmentInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCCORE_API IArcEquipmentAttachmentInterface
{
	GENERATED_BODY()

public:
	virtual void Initialize(const FArcItemId& ForItemId, UArcItemsStoreComponent* ItemsStoreComponent) = 0;
};
