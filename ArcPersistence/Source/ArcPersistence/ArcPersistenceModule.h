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

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Serialization/ArcReflectionSerializer.h"

class UObject;
class UStruct;

/**
 * Per-UStruct cached reflection plans, owned by the module for its lifetime.
 * Two maps: one for SaveGame-only plans, one for all-property plans.
 *
 * Values are held by TUniquePtr so the FArcTypePlan storage address is stable across
 * map rehashes. FArcPlanEntry::NestedPlan / InnerNestedPlan point into this cache; a
 * by-value TMap would rehash on a recursive insert and dangle those pointers.
 */
struct FArcTypePlanCache
{
	TMap<const UStruct*, TUniquePtr<FArcTypePlan>> SaveGamePlans;
	TMap<const UStruct*, TUniquePtr<FArcTypePlan>> AllPropertyPlans;
};

class FArcPersistenceModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Cached reflection plans, keyed by UStruct*. Accessed only on the game thread. */
	FArcTypePlanCache PlanCache;

private:
#if WITH_EDITOR
	/** Evicts cached plans for UStructs replaced by a Blueprint recompile. */
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

	FDelegateHandle ObjectsReplacedHandle;
#endif
};
