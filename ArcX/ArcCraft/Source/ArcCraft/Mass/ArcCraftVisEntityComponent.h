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

#include "ArcMass/ArcVisEntityComponent.h"

#include "ArcCraftVisEntityComponent.generated.h"

class UArcItemsStoreComponent;
class UArcCraftStationComponent;

/**
 * Subclass of UArcVisEntityComponent for crafting stations.
 * Inherits the full vis system lifecycle (ISM ↔ Actor swaps, entity registration).
 *
 * On actor activation (ISM → Actor), copies entity item data into
 * UArcItemsStoreComponent(s) so UI/interaction systems can read items.
 * On deactivation (Actor → ISM), syncs store changes back to entity fragments.
 *
 * Use this INSTEAD of plain UArcVisEntityComponent on crafting station actors.
 */
UCLASS(ClassGroup = "ArcCraft", meta = (BlueprintSpawnableComponent))
class ARCCRAFT_API UArcCraftVisEntityComponent : public UArcVisEntityComponent
{
	GENERATED_BODY()

public:
	UArcCraftVisEntityComponent();

	/** Which items store class to populate with output items from entity. */
	UPROPERTY(EditAnywhere, Category = "Craft Vis")
	TSubclassOf<UArcItemsStoreComponent> OutputStoreClass;

	/** Which items store class to populate with input items from entity. */
	UPROPERTY(EditAnywhere, Category = "Craft Vis")
	TSubclassOf<UArcItemsStoreComponent> InputStoreClass;

	// Override vis lifecycle hooks from UArcVisEntityComponent
	virtual void NotifyVisActorCreated(FMassEntityHandle InEntityHandle) override;
	virtual void NotifyVisActorPreDestroy() override;

private:
	/** Copy entity output/input items + queue → actor-side stores/component for UI. */
	void SyncEntityToStores();

	/** Sync actor-side stores/component changes back → entity fragments. */
	void SyncStoresToEntity();

	/** Find a store component of the given class on the owning actor. */
	UArcItemsStoreComponent* FindStore(TSubclassOf<UArcItemsStoreComponent> StoreClass) const;

	/** Find the station component on the owning actor. */
	UArcCraftStationComponent* FindStationComponent() const;
};
