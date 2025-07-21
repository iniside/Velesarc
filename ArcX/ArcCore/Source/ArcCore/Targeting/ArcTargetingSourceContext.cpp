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

#include "ArcTargetingSourceContext.h"

#include "GameFramework/Actor.h"
#include "TargetingSystem/TargetingPreset.h"
#include "TargetingSystem/TargetingSubsystem.h"

DEFINE_TARGETING_DATA_STORE(FArcTargetingSourceContext);

FArcTargetingSourceContext& FArcTargetingSourceContext::FindOrAdd(FTargetingRequestHandle Handle)
{
	return UE::TargetingSystem::TTargetingDataStore<FArcTargetingSourceContext>::FindOrAdd(Handle);
}
FArcTargetingSourceContext* FArcTargetingSourceContext::Find(FTargetingRequestHandle Handle)
{
	return UE::TargetingSystem::TTargetingDataStore<FArcTargetingSourceContext>::Find(Handle);
}

FTargetingRequestHandle Arcx::MakeTargetRequestHandle(const UTargetingPreset* TargetingPreset
	, const FArcTargetingSourceContext& InSourceContext)
{
	FTargetingRequestHandle Handle = UTargetingSubsystem::CreateTargetRequestHandle();

	// store the task set
	if (TargetingPreset)
	{
		const FTargetingTaskSet*& TaskSet = FTargetingTaskSet::FindOrAdd(Handle);
		TaskSet = TargetingPreset->GetTargetingTaskSet();

		//TARGETING_LOG(Verbose, TEXT("UTargetingSubsystem::MakeTargetRequestHandle - [%s] is Adding TaskSet [0x%08x] for Handle [%d]"), *GetNameSafe(TargetingPreset), TaskSet, Handle.Handle);
	}
	else
	{
		//TARGETING_LOG(Warning, TEXT("UTargetingSubsystem::MakeTargetRequestHandle - NULL TargetingPreset, no tasks will be setup for the targeting handle [%d]"), Handle.Handle);
	}

	// store the source context
	FArcTargetingSourceContext& SourceContext = FArcTargetingSourceContext::FindOrAdd(Handle);
	SourceContext = InSourceContext;

	return Handle;
}
