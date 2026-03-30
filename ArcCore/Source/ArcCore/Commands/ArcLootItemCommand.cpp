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

#include "ArcCore/Commands/ArcLootItemCommand.h"

#include "ArcCore/Items/ArcItemsStoreComponent.h"
#include "ArcCore/Items/ArcItemSpec.h"
#include "ArcCore/Items/Loot/ArcLootFragments.h"

#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityUtils.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcLootItem, Log, All);

bool FArcLootItemCommand::CanSendCommand() const
{
	if (!TargetItemsStore)
	{
		return false;
	}

	if (!LootEntity.IsValid())
	{
		return false;
	}

	return true;
}

bool FArcLootItemCommand::Execute()
{
	if (!TargetItemsStore)
	{
		UE_LOG(LogArcLootItem, Warning, TEXT("Execute: TargetItemsStore is null."));
		return false;
	}

	UWorld* World = TargetItemsStore->GetWorld();
	if (!World)
	{
		UE_LOG(LogArcLootItem, Warning, TEXT("Execute: No world available."));
		return false;
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(*World);

	if (!EntityManager.IsEntityValid(LootEntity))
	{
		UE_LOG(LogArcLootItem, Warning, TEXT("Execute: LootEntity is not valid."));
		return false;
	}

	FArcLootContainerFragment* LootFragment = EntityManager.GetFragmentDataPtr<FArcLootContainerFragment>(LootEntity);
	if (!LootFragment)
	{
		UE_LOG(LogArcLootItem, Warning, TEXT("Execute: LootEntity has no FArcLootContainerFragment."));
		return false;
	}

	if (LootFragment->Items.IsEmpty())
	{
		UE_LOG(LogArcLootItem, Log, TEXT("Execute: LootEntity container is empty, nothing to transfer."));
		return true;
	}

	for (FArcItemSpec& ItemSpec : LootFragment->Items)
	{
		TargetItemsStore->AddItem(ItemSpec, FArcItemId());
	}

	UE_LOG(LogArcLootItem, Log, TEXT("Transferred %d items from loot entity."), LootFragment->Items.Num());

	return true;
}
