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

#include "ArcCoreModule.h"

#if WITH_GAMEPLAY_DEBUGGER
#include "GameplayDebugger.h"
#endif

#include "JsonAssets/ArcJsonAssetPaths.h"
#include "Misc/CoreDelegates.h"
#include "Misc/PackageName.h"

#define LOCTEXT_NAMESPACE "FArcGameCoreModule"

void FArcCoreModule::OnAllModuleLoadingPhasesComplete()
{
}

void FArcCoreModule::StartupModule()
{
	// GAS Attributes gameplay-debugger category retired with GAS keystone (Bucket D 7).
	// OnPreWorldInitialization experience-loading lambda retired with Experience cluster
	// (Bucket B Cluster 2). Experience resolution now lives in
	// AArcMassGameMode::GetExperiencePrimaryId.

	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FArcCoreModule::OnAllModuleLoadingPhasesComplete);

	ArcJsonAssetPaths::InitializeContentRoots();
	ContentPathMountedHandle = FPackageName::OnContentPathMounted().AddStatic(&ArcJsonAssetPaths::HandleContentPathMounted);
	ContentPathDismountedHandle = FPackageName::OnContentPathDismounted().AddStatic(&ArcJsonAssetPaths::HandleContentPathDismounted);
}

void FArcCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules
	// that support dynamic reloading, we call this function before unloading the module.
	FPackageName::OnContentPathMounted().Remove(ContentPathMountedHandle);
	FPackageName::OnContentPathDismounted().Remove(ContentPathDismountedHandle);
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcCoreModule, ArcCore)
