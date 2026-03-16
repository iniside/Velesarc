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

#include "GameFeatureAction_WarmProjectilePool.h"

#include "ArcMass/Projectile/ArcProjectileActorPool.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_WarmProjectilePool)

void UGameFeatureAction_WarmProjectilePool::AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
{
	UWorld* World = WorldContext.World();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UArcProjectileActorPoolSubsystem* Pool = World->GetSubsystem<UArcProjectileActorPoolSubsystem>();
	if (!Pool)
	{
		return;
	}

	for (const FArcProjectilePoolEntry& Entry : PoolEntries)
	{
		if (Entry.ActorClass && Entry.Count > 0)
		{
			Pool->WarmPool(Entry.ActorClass, Entry.Count);
		}
	}
}
