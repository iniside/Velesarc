// Copyright Lukasz Baran. All Rights Reserved.

#include "Extensions/ArcAbilityPresetToolbarExtension.h"

#include "Extensions/ArcAbilityPresetEditorExtension.h"
#include "Presets/ArcAbilityPresetGenerator.h"
#include "Presets/ArcAbilityTreePreset.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"

#include "IDetailsView.h"
#include "IStateTreeEditorHost.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "ToolMenu.h"
#include "ToolMenuContext.h"
#include "ToolMenuDelegates.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ArcAbilityPresetToolbarExtension"

namespace ArcAbilityPresetToolbarExtensionLocal
{
	UStateTree* FindPresetTree(const FToolMenuContext& Context, TSharedPtr<IStateTreeEditorHost>& OutHost)
	{
		UStateTreeEditorContext* EditorContext = Context.FindContext<UStateTreeEditorContext>();
		if (EditorContext == nullptr || !EditorContext->EditorHostInterface.IsValid())
		{
			return nullptr;
		}

		UStateTree* StateTree = EditorContext->EditorHostInterface->GetStateTree();
		if (StateTree == nullptr)
		{
			return nullptr;
		}

		UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
		if (EditorData == nullptr || EditorData->Schema == nullptr || !EditorData->Schema->IsA<UArcAbilityStateTreeSchema>())
		{
			return nullptr;
		}

		UArcAbilityPresetEditorExtension* Extension = EditorData->GetExtension<UArcAbilityPresetEditorExtension>();
		if (Extension == nullptr || Extension->SourcePreset == nullptr)
		{
			return nullptr;
		}

		if (Extension->SourcePreset->HasAnyClassFlags(CLASS_Abstract) || Extension->SourcePreset->GetDefaultObject<UArcAbilityTreePreset>() == nullptr)
		{
			return nullptr;
		}

		OutHost = EditorContext->EditorHostInterface;
		return StateTree;
	}

	bool IsResetVisible(const FToolMenuContext& Context)
	{
		TSharedPtr<IStateTreeEditorHost> Host;
		return FindPresetTree(Context, Host) != nullptr;
	}

	void ExecuteReset(const FToolMenuContext& Context)
	{
		TSharedPtr<IStateTreeEditorHost> Host;
		UStateTree* StateTree = FindPresetTree(Context, Host);
		if (StateTree == nullptr)
		{
			return;
		}

		ArcAbilityPresetGenerator::ResetToPreset(*StateTree, Host->GetAssetDetailsView());
	}

	void BuildResetEntry(FToolMenuSection& Section)
	{
		FToolUIAction Action;
		Action.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteReset);
		Action.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsResetVisible);

		Section.AddEntry(FToolMenuEntry::InitToolBarButton(
			TEXT("ArcResetToPreset"),
			FToolUIActionChoice(Action),
			LOCTEXT("ResetToPresetLabel", "Reset to Preset"),
			LOCTEXT("ResetToPresetTooltip", "Rebuild this ability tree from its source preset, discarding manual edits."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Refresh")));
	}
}

void ArcAbilityPresetToolbarExtension::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

	UToolMenu* ToolMenu = UToolMenus::Get()->ExtendMenu(TEXT("AssetEditor.StateTreeEditor.ToolBar"));
	if (ToolMenu == nullptr)
	{
		return;
	}

	FToolMenuSection& Section = ToolMenu->FindOrAddSection(TEXT("ArcAbilityPreset"));
	Section.AddDynamicEntry(
		TEXT("ArcAbilityPresetReset"),
		FNewToolMenuSectionDelegate::CreateStatic(&ArcAbilityPresetToolbarExtensionLocal::BuildResetEntry));
}

#undef LOCTEXT_NAMESPACE
