// Copyright Lukasz Baran. All Rights Reserved.

#include "Config/ArcAssetDefinition_ArcMassEntityConfigAsset.h"
#include "ArcAssetEditor_MassEntityConfig.h"
#include "Config/ArcMassEntityConfigHelpers.h"
#include "AssetDefinition.h"
#include "ContentBrowserMenuContexts.h"
#include "Misc/DelayedAutoRegister.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "ScopedTransaction.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAssetDefinition_ArcMassEntityConfigAsset)

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcMassEntityConfigAsset"

namespace ArcMassConfigActions_Private
{
	static void HandleOnUpdateFromTemplate(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (Context == nullptr)
		{
			return;
		}

		TSet<FName> EditableAssets;
		{
			const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
			EditableAssets.Reserve(SelectedItems.Num());
			for (const FContentBrowserItem& SelectedItem : SelectedItems)
			{
				if (SelectedItem.CanEdit())
				{
					EditableAssets.Add(SelectedItem.GetInternalPath());
				}
			}
		}
		ensure(!EditableAssets.IsEmpty());

		TArray<UArcMassEntityConfigAsset*> ConfigAssets = Context->LoadSelectedObjectsIf<UArcMassEntityConfigAsset>(
			[&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});

		// Filter to only those with a linked source template
		TArray<UArcMassEntityConfigAsset*> Variants;
		Variants.Reserve(ConfigAssets.Num());
		for (UArcMassEntityConfigAsset* ConfigAsset : ConfigAssets)
		{
			if (ConfigAsset != nullptr && ConfigAsset->ResolveSourceTemplate() != nullptr)
			{
				Variants.Add(ConfigAsset);
			}
		}

		if (Variants.IsEmpty())
		{
			return;
		}

		const bool bPressedOk = FArcMassEntityConfigHelpers::PreviewUpdate(nullptr, Variants);
		if (!bPressedOk)
		{
			return;
		}

		for (UArcMassEntityConfigAsset* Variant : Variants)
		{
			if (UArcMassEntityConfigTemplate* Template = Variant->ResolveSourceTemplate())
			{
				FScopedTransaction Transaction(LOCTEXT("UpdateFromTemplate_Transaction", "Update From Template"));
				Variant->Modify();
				Template->UpdateFromTemplate(Variant);
				Variant->MarkPackageDirty();
			}
		}
	}

	static void HandleOnSetSourceTemplate(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (Context == nullptr)
		{
			return;
		}

		TSet<FName> EditableAssets;
		{
			const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
			EditableAssets.Reserve(SelectedItems.Num());
			for (const FContentBrowserItem& SelectedItem : SelectedItems)
			{
				if (SelectedItem.CanEdit())
				{
					EditableAssets.Add(SelectedItem.GetInternalPath());
				}
			}
		}
		ensure(!EditableAssets.IsEmpty());

		TArray<UArcMassEntityConfigAsset*> ConfigAssets = Context->LoadSelectedObjectsIf<UArcMassEntityConfigAsset>(
			[&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});

		UArcMassEntityConfigTemplate* SelectedTemplate = nullptr;
		const bool bPressedOk = FArcMassEntityConfigHelpers::PickTemplateWithPreview(SelectedTemplate, ConfigAssets, EArcMassConfigMergeMode::Set);

		if (!bPressedOk)
		{
			return;
		}

		if (SelectedTemplate == nullptr)
		{
			return;
		}

		for (UArcMassEntityConfigAsset* ConfigAsset : ConfigAssets)
		{
			FScopedTransaction Transaction(LOCTEXT("SetSourceTemplate_Transaction", "Set Source Template"));
			ConfigAsset->Modify();
			SelectedTemplate->SetTemplate(ConfigAsset);
			ConfigAsset->MarkPackageDirty();
		}
	}

	static void HandleOnSetNewOrReplaceSourceTemplate(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (Context == nullptr)
		{
			return;
		}

		TSet<FName> EditableAssets;
		{
			const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
			EditableAssets.Reserve(SelectedItems.Num());
			for (const FContentBrowserItem& SelectedItem : SelectedItems)
			{
				if (SelectedItem.CanEdit())
				{
					EditableAssets.Add(SelectedItem.GetInternalPath());
				}
			}
		}
		ensure(!EditableAssets.IsEmpty());

		TArray<UArcMassEntityConfigAsset*> ConfigAssets = Context->LoadSelectedObjectsIf<UArcMassEntityConfigAsset>(
			[&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});

		UArcMassEntityConfigTemplate* SelectedTemplate = nullptr;
		const bool bPressedOk = FArcMassEntityConfigHelpers::PickTemplateWithPreview(SelectedTemplate, ConfigAssets, EArcMassConfigMergeMode::SetNewOrReplace);

		if (!bPressedOk)
		{
			return;
		}

		if (SelectedTemplate == nullptr)
		{
			return;
		}

		for (UArcMassEntityConfigAsset* ConfigAsset : ConfigAssets)
		{
			FScopedTransaction Transaction(LOCTEXT("SetNewOrReplaceSourceTemplate_Transaction", "Set New/Replace Source Template"));
			ConfigAsset->Modify();
			SelectedTemplate->SetNewOrReplaceTemplate(ConfigAsset);
			ConfigAsset->MarkPackageDirty();
		}
	}

	static bool IsTemplateActionVisible(const FToolMenuContext& MenuContext)
	{
		const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext);
		if (Context == nullptr)
		{
			return true;
		}

		TSet<FName> EditableAssets;
		for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
		{
			if (SelectedItem.CanEdit())
			{
				EditableAssets.Add(SelectedItem.GetInternalPath());
			}
		}

		TArray<UArcMassEntityConfigAsset*> ConfigAssets = Context->LoadSelectedObjectsIf<UArcMassEntityConfigAsset>(
			[&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});

		for (const UArcMassEntityConfigAsset* ConfigAsset : ConfigAssets)
		{
			if (ConfigAsset != nullptr && ConfigAsset->GetClass()->IsChildOf(UArcMassEntityConfigTemplate::StaticClass()))
			{
				return false;
			}
		}

		return true;
	}

	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UArcMassEntityConfigAsset::StaticClass());

			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateStatic([](FToolMenuSection& InSection)
			{
				{
					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnUpdateFromTemplate);
					UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsTemplateActionVisible);
					InSection.AddMenuEntry(
						"ArcMassConfig_UpdateFromTemplate",
						LOCTEXT("UpdateFromTemplateMenu", "Update From Template"),
						LOCTEXT("UpdateFromTemplateMenuTip", "Three-way merge from linked template"),
						FSlateIcon(),
						UIAction);
				}
				{
					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnSetSourceTemplate);
					UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsTemplateActionVisible);
					InSection.AddMenuEntry(
						"ArcMassConfig_SetSourceTemplate",
						LOCTEXT("SetSourceTemplateMenu", "Set Source Template"),
						LOCTEXT("SetSourceTemplateMenuTip", "Additively stamp template traits (keeps existing; never removes)"),
						FSlateIcon(),
						UIAction);
				}
				{
					FToolUIAction UIAction;
					UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnSetNewOrReplaceSourceTemplate);
					UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsTemplateActionVisible);
					InSection.AddMenuEntry(
						"ArcMassConfig_SetNewOrReplaceSourceTemplate",
						LOCTEXT("SetNewOrReplaceSourceTemplateMenu", "Set New/Replace Source Template"),
						LOCTEXT("SetNewOrReplaceSourceTemplateMenuTip", "Replace template-origin traits; preserve variant-only"),
						FSlateIcon(),
						UIAction);
				}
			}));
		}));
	});
} // namespace ArcMassConfigActions_Private

FText UArcAssetDefinition_ArcMassEntityConfigAsset::GetAssetDisplayName() const
{
	return LOCTEXT("ArcMassEntityConfigAsset", "Arc Mass Entity Config");
}

TConstArrayView<FAssetCategoryPath> UArcAssetDefinition_ArcMassEntityConfigAsset::GetAssetCategories() const
{
	static FAssetCategoryPath AssetPaths[] = {
		FAssetCategoryPath(LOCTEXT("Gameplay", "Gameplay")),
		FAssetCategoryPath(LOCTEXT("MassCategory", "Arc Mass"))
	};
	return AssetPaths;
}

EAssetCommandResult UArcAssetDefinition_ArcMassEntityConfigAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	for (UArcMassEntityConfigAsset* ConfigAsset : OpenArgs.LoadObjects<UArcMassEntityConfigAsset>())
	{
		FArcAssetEditor_MassEntityConfig::CreateEditor(
			OpenArgs.GetToolkitMode(),
			OpenArgs.ToolkitHost,
			ConfigAsset);
	}
	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE
