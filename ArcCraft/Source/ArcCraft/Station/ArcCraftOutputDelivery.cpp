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

#include "ArcCraft/Station/ArcCraftOutputDelivery.h"

#include "Mass/CommandBuffer.h"
#include "Mass/Commands.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "MassSignalSubsystem.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemId.h"
#include "Items/ArcItemStackMethod.h"

// -------------------------------------------------------------------
// FArcCraftOutputDelivery (base)
// -------------------------------------------------------------------

EArcCraftDeliveryDisposition FArcCraftOutputDelivery::DeliverOutput_Mass(
	FMassExecutionContext& Context,
	FMassEntityManager& EntityManager,
	FMassEntityHandle StationEntity,
	FMassEntityHandle InstigatorEntity,
	const FGuid& EntryId,
	const TArray<FArcItemSpec>& Specs) const
{
	// Deferred Set so the write lands on the game thread at the next flush —
	// processors call this from worker threads.
	Context.Defer().PushCommand<FMassDeferredCommand<EMassCommandOperationType::Set>>(
		[StationEntity, Specs](FMassEntityManager& Mgr)
		{
			FArcCraftOutputFragment* OutputFrag = Mgr.GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
			if (!OutputFrag)
			{
				return;
			}

			for (const FArcItemSpec& Spec : Specs)
			{
				const FArcItemDefinitionData* OutputDef = Spec.GetItemDefinition();
				const FArcItemStackMethod* StackMethod = OutputDef ? OutputDef->GetStackMethod<FArcItemStackMethod>() : nullptr;
				FArcItemSpec SpecCopy = Spec;
				// TryStackSpec returns true only when SpecCopy merged into an existing OutputItems
				// line — that line keeps its already-stamped id, so do NOT restamp. Otherwise we
				// append a NEW line and stamp it with a fresh stable id so output can be addressed
				// by id (mirrors deposit's DepositSpec.SetItemId stamping precedent).
				if (!StackMethod || !StackMethod->TryStackSpec(OutputFrag->OutputItems, MoveTemp(SpecCopy)))
				{
					FArcItemSpec NewSpec = Spec;
					NewSpec.SetItemId(FArcItemId::Generate());
					OutputFrag->OutputItems.Add(MoveTemp(NewSpec));
				}
			}

			// OutputItems mutated → notify subscribers. Resolve the signal subsystem from the
			// world (the lambda runs on the game thread at flush), matching deposit's InputChanged.
			if (UWorld* World = Mgr.GetWorld())
			{
				if (UMassSignalSubsystem* Sig = World->GetSubsystem<UMassSignalSubsystem>())
				{
					Sig->SignalEntity(ArcCraftSignals::OutputChanged, StationEntity);
				}
			}
		});

	// Base/store delivery never relocates the queue entry; the processor advances it.
	return EArcCraftDeliveryDisposition::ResolvedSync;
}
