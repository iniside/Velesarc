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

#include "ArcCraft/Commands/ArcTransferItemFromStationCommand.h"

#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "ArcCraft/Mass/ArcCraftDeliveryConfigFragment.h"
#include "ArcCraft/Station/ArcCraftItemSource.h"

namespace ArcTransferItemFromStationCommandLocal
{
	DEFINE_LOG_CATEGORY_STATIC(LogArcTransferItemFromStationCommand, Log, All);
}

// ---------------------------------------------------------------------------
// CanSendCommand
// ---------------------------------------------------------------------------

bool FArcTransferItemFromStationCommand::CanSendCommand() const
{
	return StationEntity.IsSet()
		&& InstigatorEntity.IsSet()
		&& ItemId.IsValid()
		&& Stacks > 0;
}

// ---------------------------------------------------------------------------
// Execute
// ---------------------------------------------------------------------------

bool FArcTransferItemFromStationCommand::Execute(UObject* WorldContext)
{
	UWorld* World = ResolveWorld(WorldContext);
	if (World == nullptr)
	{
		UE_LOGF(ArcTransferItemFromStationCommandLocal::LogArcTransferItemFromStationCommand, Warning,"FArcTransferItemFromStationCommand::Execute — WorldContext resolved to null world.");
		return false;
	}

	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	if (MassSub == nullptr)
	{
		UE_LOGF(ArcTransferItemFromStationCommandLocal::LogArcTransferItemFromStationCommand, Warning,"FArcTransferItemFromStationCommand::Execute — UMassEntitySubsystem not found.");
		return false;
	}

	FMassEntityManager& EM = MassSub->GetMutableEntityManager();

	if (!EM.IsEntityActive(StationEntity))
	{
		UE_LOGF(ArcTransferItemFromStationCommandLocal::LogArcTransferItemFromStationCommand, Warning,"FArcTransferItemFromStationCommand::Execute — StationEntity is not active.");
		return false;
	}
	if (!EM.IsEntityActive(InstigatorEntity))
	{
		UE_LOGF(ArcTransferItemFromStationCommandLocal::LogArcTransferItemFromStationCommand, Warning,"FArcTransferItemFromStationCommand::Execute — InstigatorEntity is not active.");
		return false;
	}

	const FMassEntityView StationView(EM, StationEntity);

	const FArcCraftDeliveryConfigFragment* DeliveryCfg =
		StationView.GetConstSharedFragmentDataPtr<FArcCraftDeliveryConfigFragment>();
	if (DeliveryCfg == nullptr)
	{
		UE_LOGF(ArcTransferItemFromStationCommandLocal::LogArcTransferItemFromStationCommand, Warning,"FArcTransferItemFromStationCommand::Execute — Station entity lacks FArcCraftDeliveryConfigFragment.");
		return false;
	}

	const FArcCraftItemSource* Source =
		DeliveryCfg->IngredientSource.GetPtr<FArcCraftItemSource>();
	if (Source == nullptr)
	{
		UE_LOGF(ArcTransferItemFromStationCommandLocal::LogArcTransferItemFromStationCommand, Warning,"FArcTransferItemFromStationCommand::Execute — DeliveryConfig has no valid IngredientSource.");
		return false;
	}

	return Source->WithdrawItem_Mass(EM, StationEntity, InstigatorEntity, ItemId, Stacks);
}
