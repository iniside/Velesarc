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

#include "ArcJson.h"

#include "UObject/UObjectIterator.h"
#include "UObject/Class.h"

#include "ArcJsonIncludes.h"
#include "Misc/CoreDelegates.h"

#define LOCTEXT_NAMESPACE "FArcJsonModule"

void FArcJsonModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FArcJsonModule::OnAllModuleLoadingPhasesComplete);
}

void FArcJsonModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

void FArcJsonModule::OnAllModuleLoadingPhasesComplete()
{
	for (TObjectIterator<UScriptStruct> ScriptIt; ScriptIt; ++ScriptIt)
	{
		UScriptStruct* ScriptStruct = *ScriptIt;
		if (ScriptStruct == FArcCustomSerializer::StaticStruct())
		{
			continue;
		}
		
		if (!ScriptStruct->IsChildOf(FArcCustomSerializer::StaticStruct()))
		{
			continue;
		}

		if (FArcJsonSerializers::Serializers.Contains(ScriptStruct))
		{
			continue;
		}

		FInstancedStruct Ser = FInstancedStruct(ScriptStruct);

		FArcCustomSerializer* Ptr = Ser.GetMutablePtr<FArcCustomSerializer>();

		if (!Ptr->GetSupportedType())
		{
			continue;
		}
		FArcJsonSerializers::Serializers.Add(Ptr->GetSupportedType(), Ser);
	}
}


#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FArcJsonModule, ArcJson)