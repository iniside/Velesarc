// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassGameplayFrameworkEditorModule.h"

#include "Styling/SlateStyleRegistry.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"

void FArcMassGameplayFrameworkEditorModule::StartupModule()
{
	if (!StyleSet.IsValid())
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		const FString IconDir = IPluginManager::Get().FindPlugin(TEXT("ArcMassGameplayFramework"))->GetBaseDir()
			+ TEXT("/Content/Editor/AssetIcons");

		StyleSet = MakeShared<FSlateStyleSet>(TEXT("ArcMassGameplayFrameworkEditor"));
		StyleSet->SetContentRoot(IconDir);
		StyleSet->SetCoreContentRoot(IconDir);

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassPawnData"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassPawnData.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassPawnData"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassPawnData.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassExperienceDefinition"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassExperienceDefinition.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassExperienceDefinition"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassExperienceDefinition.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassExperienceActionSet"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassExperienceActionSet.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassExperienceActionSet"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassExperienceActionSet.svg"), Icon16x16));

		StyleSet->Set(TEXT("ClassThumbnail.ArcMassInputActionConfig"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassInputActionConfig.svg"), Icon64x64));
		StyleSet->Set(TEXT("ClassIcon.ArcMassInputActionConfig"),
			new FSlateVectorImageBrush(IconDir + TEXT("/ArcMassInputActionConfig.svg"), Icon16x16));

		FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
	}
}

void FArcMassGameplayFrameworkEditorModule::ShutdownModule()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		StyleSet.Reset();
	}
}

IMPLEMENT_MODULE(FArcMassGameplayFrameworkEditorModule, ArcMassGameplayFrameworkEditor)
