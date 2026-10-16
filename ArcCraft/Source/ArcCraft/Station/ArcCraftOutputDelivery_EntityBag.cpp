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

#include "ArcCraft/Station/ArcCraftOutputDelivery_EntityBag.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "Engine/World.h"

#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftOutputResumeSubsystem.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Station/ArcCraftStationImpl.h"

#include "Bags/ArcBagOperations.h"
#include "Bags/ArcBagStoreFragment.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemSpec.h"
#include "Operations/ArcMassItemOperations.h"

EArcCraftDeliveryDisposition FArcCraftOutputDelivery_EntityBag::DeliverOutput_Mass(
	FMassExecutionContext& Context,
	FMassEntityManager& EntityManager,
	FMassEntityHandle StationEntity,
	FMassEntityHandle InstigatorEntity,
	const FGuid& EntryId,
	const TArray<FArcItemSpec>& Specs) const
{
	// Resolve delivery target: instigator (e.g. player backpack) or station entity.
	// For player hand-craft, Instigator == Station so this flag has no effect.
	const FMassEntityHandle Target = bDeliverToInstigator ? InstigatorEntity : StationEntity;

	// Resolve world + bridge/signal subsystems up front; they may be null on stripped/test worlds.
	UWorld* World = EntityManager.GetWorld();
	UArcCraftOutputResumeSubsystem* Resume = World ? World->GetSubsystem<UArcCraftOutputResumeSubsystem>() : nullptr;
	UMassSignalSubsystem* Signal = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;

	// No bag overlay on Target → graceful degrade to a plain store add so a bag-less entity is never
	// stranded. The bag overlay is a sparse fragment; presence is probed via the sparse accessor.
	const bool bHasBag = EntityManager
		.GetMutableSparseElementDataForEntity(FArcMassBagStoreFragment::StaticStruct(), Target)
		.IsValid();
	if (!bHasBag)
	{
		Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
			[Target, Specs](FMassEntityManager& Mgr)
			{
				for (const FArcItemSpec& Spec : Specs)
				{
					ArcMassItems::AddItem(Mgr, Target, FArcMassItemStoreFragment::StaticStruct(), Spec);
				}
			});
		return EArcCraftDeliveryDisposition::ResolvedSync;
	}

	// Bag present: defer one command that performs the authoritative adds and owns the
	// advance-or-block decision (add result IS the acceptance check — no separate pre-check).
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[StationEntity, EntryId, Target, Specs, Resume, Signal](FMassEntityManager& Mgr)
		{
			int32 NumDelivered = 0;
			while (NumDelivered < Specs.Num())
			{
				const FArcItemId Id = ArcMassItems::Bags::AddItemToFirstFreeSlot(Mgr, Target, Specs[NumDelivered]);
				if (!Id.IsValid())
				{
					break;
				}
				++NumDelivered;
			}

			FArcCraftQueueFragment* Q = Mgr.GetFragmentDataPtr<FArcCraftQueueFragment>(StationEntity);
			if (!Q)
			{
				return;
			}

			const int32 Idx = Q->Entries.IndexOfByPredicate(
				[&EntryId](const FArcCraftQueueEntry& E)
				{
					return E.EntryId == EntryId;
				});
			if (Idx == INDEX_NONE)
			{
				// Entry cancelled meanwhile; the delivered specs stay delivered.
				return;
			}

			FArcCraftQueueEntry& E = Q->Entries[Idx];
			if (NumDelivered == Specs.Num())
			{
				// One craft advances once, however many specs it delivered or how many resumes it took.
				E.bOutputBlocked = false;
				E.HeldOutput.Reset();
				ArcCraftStationImpl::AdvanceCompletedEntry(*Q, EntryId);
			}
			else
			{
				// Bag full: hold the exact built specs not yet delivered (recipes may carry random
				// rolls — don't rebuild) and pause the station's queue. Resume bridges the slot-freed
				// event back to us.
				E.bOutputBlocked = true;
				E.HeldOutput.Reset();
				E.HeldOutput.Append(Specs.GetData() + NumDelivered, Specs.Num() - NumDelivered);
				if (Resume)
				{
					Resume->RegisterBlocked(Target, StationEntity);
				}
			}

			if (Signal)
			{
				Signal->SignalEntity(ArcCraftSignals::QueueChanged, StationEntity);
			}
		});

	return EArcCraftDeliveryDisposition::DeferredResolve;
}
