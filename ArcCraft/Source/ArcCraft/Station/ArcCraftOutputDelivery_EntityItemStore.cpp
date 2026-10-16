/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "ArcCraft/Station/ArcCraftOutputDelivery_EntityItemStore.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Operations/ArcMassItemOperations.h"
#include "Items/ArcItemSpec.h"

FArcCraftOutputDelivery_EntityItemStore::FArcCraftOutputDelivery_EntityItemStore()
{
	StoreType = FArcMassItemStoreFragment::StaticStruct();
}

EArcCraftDeliveryDisposition FArcCraftOutputDelivery_EntityItemStore::DeliverOutput_Mass(
	FMassExecutionContext& Context,
	FMassEntityManager& EntityManager,
	FMassEntityHandle StationEntity,
	FMassEntityHandle InstigatorEntity,
	const FGuid& EntryId,
	const TArray<FArcItemSpec>& Specs) const
{
	// Resolve delivery target: instigator (e.g. player backpack) or station entity.
	// For player simple-craft, Instigator == Station so this flag has no effect.
	const FMassEntityHandle TargetEntity = bDeliverToInstigator ? InstigatorEntity : StationEntity;

	// StoreType must be set; default ctor initialises it to FArcMassItemStoreFragment.
	const UScriptStruct* ResolvedStoreType = StoreType
		? StoreType.Get()
		: FArcMassItemStoreFragment::StaticStruct();

	// Push a deferred Set so the write lands on the game thread at the next flush —
	// processors call this from worker threads. Captures by value.
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[TargetEntity, ResolvedStoreType, Specs](FMassEntityManager& Mgr)
		{
			for (const FArcItemSpec& Spec : Specs)
			{
				ArcMassItems::AddItem(Mgr, TargetEntity, ResolvedStoreType, Spec);
			}
		});

	// Store delivery never relocates the queue entry; the processor advances it.
	return EArcCraftDeliveryDisposition::ResolvedSync;
}
