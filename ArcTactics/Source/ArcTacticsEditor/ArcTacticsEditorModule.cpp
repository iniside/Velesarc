// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsEditorModule.h"

#include "ArcTacticsConfigEditor.h"
#include "ArcTacticsEditorCommands.h"
#include "ArcTacticsEditorStyle.h"
#include "ArcTacticsToolset.h"
#include "AssetToolsModule.h"
#include "AssetTypeActions_Base.h"
#include "Decision/ArcTacticsConfig.h"
#include "IAssetTools.h"
#include "Misc/CoreDelegates.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#define LOCTEXT_NAMESPACE "ArcTacticsEditorModule"

namespace ArcTacticsEditorModulePrivate
{
	class FConfigAssetTypeActions final : public FAssetTypeActions_Base
	{
	public:
		virtual FText GetName() const override
		{
			return LOCTEXT("ConfigAssetName", "Arc Tactics Configuration");
		}

		virtual FColor GetTypeColor() const override
		{
			return FColor(70, 155, 105);
		}

		virtual UClass* GetSupportedClass() const override
		{
			return UArcTacticsConfig::StaticClass();
		}

		virtual uint32 GetCategories() override
		{
			return EAssetTypeCategories::Gameplay;
		}

		virtual void OpenAssetEditor(
			const TArray<UObject*>& InObjects,
			TSharedPtr<IToolkitHost> EditWithinLevelEditor) override
		{
			const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
				? EToolkitMode::WorldCentric
				: EToolkitMode::Standalone;
			for (UObject* Object : InObjects)
			{
				if (UArcTacticsConfig* Config = Cast<UArcTacticsConfig>(Object))
				{
					const TSharedRef<FArcTacticsConfigEditor> Editor = MakeShared<FArcTacticsConfigEditor>();
					Editor->Init(Mode, EditWithinLevelEditor, Config);
				}
			}
		}
	};
}

void FArcTacticsEditorModule::StartupModule()
{
	FArcTacticsEditorStyle::Register();
	FArcTacticsEditorCommands::Register();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
	AssetTypeActions = MakeShared<ArcTacticsEditorModulePrivate::FConfigAssetTypeActions>();
	AssetTools.RegisterAssetTypeActions(AssetTypeActions.ToSharedRef());

	ToolsetRegistrationHandle = FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda([]()
	{
		UToolsetRegistry::RegisterToolsetClass(UArcTacticsToolset::StaticClass());
	});
}

void FArcTacticsEditorModule::ShutdownModule()
{
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.Remove(ToolsetRegistrationHandle);
	if (UObjectInitialized())
	{
		UToolsetRegistry::UnregisterToolsetClass(UArcTacticsToolset::StaticClass());
	}
	if (AssetTypeActions.IsValid() && FModuleManager::Get().IsModuleLoaded(TEXT("AssetTools")))
	{
		FModuleManager::GetModuleChecked<FAssetToolsModule>(TEXT("AssetTools"))
			.Get()
			.UnregisterAssetTypeActions(AssetTypeActions.ToSharedRef());
	}
	AssetTypeActions.Reset();
	FArcTacticsEditorCommands::Unregister();
	FArcTacticsEditorStyle::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FArcTacticsEditorModule, ArcTacticsEditor)
