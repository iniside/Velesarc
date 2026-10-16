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

#include "ArcMarkupUIEditorModule.h"

#include "ArcFontToolset.h"
#include "ArcSvgAssetDetailCustomization.h"
#include "ArcSvgAssetThumbnailRenderer.h"
#include "ArcSvgToolset.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Svg/ArcSvgAsset.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

void FArcMarkupUIEditorModule::StartupModule()
{
	UThumbnailManager::Get().RegisterCustomRenderer(
		UArcSvgAsset::StaticClass(),
		UArcSvgAssetThumbnailRenderer::StaticClass());

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomClassLayout(
		UArcSvgAsset::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FArcSvgAssetDetailCustomization::MakeInstance));

	// Defer toolset registration: UToolsetRegistry::RegisterToolsetClass is a no-op until the
	// UToolsetRegistrySubsystem exists, which is later than this editor module's StartupModule.
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([]()
	{
		UToolsetRegistry::RegisterToolsetClass(UArcSvgToolset::StaticClass());
		UToolsetRegistry::RegisterToolsetClass(UArcFontToolset::StaticClass());
	});
}

void FArcMarkupUIEditorModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UArcSvgAsset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UArcSvgToolset::StaticClass());
		UToolsetRegistry::UnregisterToolsetClass(UArcFontToolset::StaticClass());

		if (FPropertyEditorModule* PropertyEditor = FModuleManager::Get().GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
		{
			PropertyEditor->UnregisterCustomClassLayout(UArcSvgAsset::StaticClass()->GetFName());
		}
	}
}

IMPLEMENT_MODULE(FArcMarkupUIEditorModule, ArcMarkupUIEditor)
