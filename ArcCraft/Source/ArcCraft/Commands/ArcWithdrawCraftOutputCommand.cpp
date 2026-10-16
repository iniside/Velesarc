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

#include "ArcCraft/Commands/ArcWithdrawCraftOutputCommand.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassSignalSubsystem.h"
#include "Items/ArcItemSpec.h"
#include "ArcCraft/Mass/ArcCraftMassFragments.h"
#include "ArcCraft/Mass/ArcCraftQueueChangedSignal.h"
#include "ArcCraft/Station/ArcCraftStationXfer.h"

namespace ArcWithdrawCraftOutputCommandLocal
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcWithdrawCraftOutputCommand, Log, All);
}

// ---------------------------------------------------------------------------
// CanSendCommand
// ---------------------------------------------------------------------------

bool FArcWithdrawCraftOutputCommand::CanSendCommand() const
{
	return StationEntity.IsSet()
		&& InstigatorEntity.IsSet()
		&& ItemId.IsValid()
		&& Stacks > 0;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

bool FArcWithdrawCraftOutputCommand::Execute(UObject* WorldContext)
{
	UWorld* World = ResolveWorld(WorldContext);
	if (World == nullptr)
	{
		UE_LOGF(ArcWithdrawCraftOutputCommandLocal::LogArcWithdrawCraftOutputCommand, Warning,"FArcWithdrawCraftOutputCommand::Execute — WorldContext resolved to null world.");
		return false;
	}

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSub == nullptr)
	{
		UE_LOGF(ArcWithdrawCraftOutputCommandLocal::LogArcWithdrawCraftOutputCommand, Warning,"FArcWithdrawCraftOutputCommand::Execute — UMassEntitySubsystem not found.");
		return false;
	}

	FMassEntityManager& EM = MassSub->GetMutableEntityManager();

	if (!EM.IsEntityActive(StationEntity))
	{
		UE_LOGF(ArcWithdrawCraftOutputCommandLocal::LogArcWithdrawCraftOutputCommand, Warning,"FArcWithdrawCraftOutputCommand::Execute — StationEntity is not active.");
		return false;
	}
	if (!EM.IsEntityActive(InstigatorEntity))
	{
		UE_LOGF(ArcWithdrawCraftOutputCommandLocal::LogArcWithdrawCraftOutputCommand, Warning,"FArcWithdrawCraftOutputCommand::Execute — InstigatorEntity is not active.");
		return false;
	}

	// Direct regular-fragment access — non-_EntityStore stations never populate OutputItems, so the
	// IndexOfByKey lookup below fails for them and Execute returns false (per-policy correctness free).
	FArcCraftOutputFragment* OutputFrag = EM.GetFragmentDataPtr<FArcCraftOutputFragment>(StationEntity);
	if (OutputFrag == nullptr)
	{
		UE_LOGF(ArcWithdrawCraftOutputCommandLocal::LogArcWithdrawCraftOutputCommand, Warning,"FArcWithdrawCraftOutputCommand::Execute — Station entity lacks FArcCraftOutputFragment.");
		return false;
	}

	// Locate the output line by ItemId (stable ids stamped on append by the delivery write path).
	const int32 OutputIndex = OutputFrag->OutputItems.IndexOfByKey(ItemId);
	if (OutputIndex == INDEX_NONE)
	{
		return false;
	}

	FArcItemSpec& OutputSpec = OutputFrag->OutputItems[OutputIndex];
	const int32 Available = static_cast<int32>(OutputSpec.Amount);
	const int32 ToWithdraw = FMath::Min(Available, Stacks);

	FArcItemSpec WithdrawnSpec = OutputSpec;
	WithdrawnSpec.Amount = static_cast<uint16>(ToWithdraw);
	if (ToWithdraw < Available)
	{
		WithdrawnSpec.SetItemId(FArcItemId::Generate());
	}

	// Add-to-bag FIRST: deliver to the instigator (bag-aware spilling). DeliverSpecToInstigator
	// touches only the instigator's stores, so the OutputSpec reference / OutputIndex stay valid.
	const int32 Placed = ArcCraftStationXfer::DeliverSpecToInstigator(EM, InstigatorEntity, WithdrawnSpec);
	if (Placed <= 0)
	{
		// Nothing seated on the instigator — leave the station untouched, nothing lost.
		return false;
	}

	// Decrement or remove the output line by the placed count ONLY (never the request) so a
	// partial/full-bag fill leaves the unplaced remainder in the station.
	if (Placed >= Available)
	{
		OutputFrag->OutputItems.RemoveAt(OutputIndex);
	}
	else
	{
		OutputSpec.Amount = static_cast<uint16>(Available - Placed);
	}

	// Top-level server game-thread call (not inside a Mass processor) — mutate directly and signal
	// immediately; no deferred command needed.
	if (UMassSignalSubsystem* Sig = World->GetSubsystem<UMassSignalSubsystem>())
	{
		Sig->SignalEntity(ArcCraftSignals::OutputChanged, StationEntity);
	}

	return true;
}
