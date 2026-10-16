// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTargetingEditorModule.h"

#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

void FArcTargetingEditorModule::StartupModule()
{
	if (!StyleSet.IsValid())
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		const FString IconDir = IPluginManager::Get().FindPlugin(TEXT("ArcTargeting"))->GetBaseDir()
			+ TEXT("/Content/Editor/AssetIcons");

		StyleSet = MakeShared<FSlateStyleSet>(TEXT("ArcTargetingEditor"));
		StyleSet->SetContentRoot(IconDir);
		StyleSet->SetCoreContentRoot(IconDir);

		StyleSet->Set(TEXT("ClassThumbnail.ArcTargetingPreset"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcTargetingPreset.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcTargetingPreset"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcTargetingPreset.svg"), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}
}

void FArcTargetingEditorModule::ShutdownModule()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		StyleSet.Reset();
	}
}

IMPLEMENT_MODULE(FArcTargetingEditorModule, ArcTargetingEditor)
