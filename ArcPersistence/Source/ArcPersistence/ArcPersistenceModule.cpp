/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
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

#include "ArcPersistenceModule.h"

#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"

void FArcPersistenceModule::StartupModule()
{
#if WITH_EDITOR
	ObjectsReplacedHandle = FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(
		this, &FArcPersistenceModule::OnObjectsReplaced);
#endif
}

void FArcPersistenceModule::ShutdownModule()
{
	PlanCache.SaveGamePlans.Empty();
	PlanCache.AllPropertyPlans.Empty();

#if WITH_EDITOR
	if (ObjectsReplacedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectsReplaced.Remove(ObjectsReplacedHandle);
		ObjectsReplacedHandle.Reset();
	}
#endif
}

#if WITH_EDITOR
void FArcPersistenceModule::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	for (const TPair<UObject*, UObject*>& Pair : ReplacementMap)
	{
		const UStruct* OldStruct = Cast<UStruct>(Pair.Key);
		if (OldStruct != nullptr)
		{
			PlanCache.SaveGamePlans.Remove(OldStruct);
			PlanCache.AllPropertyPlans.Remove(OldStruct);
		}
	}
}
#endif

IMPLEMENT_MODULE(FArcPersistenceModule, ArcPersistence)
