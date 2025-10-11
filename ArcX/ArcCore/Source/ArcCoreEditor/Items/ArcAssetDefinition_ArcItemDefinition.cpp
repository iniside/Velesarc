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

#include "ArcAssetDefinition_ArcItemDefinition.h"
#include "ArcAssetEditor_ItemDefinition.h"
#include "ArcItemDefinitionFactory.h"
#include "ArcStaticItemHelpers.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "ContentBrowserMenuContexts.h"
#include "ISourceControlModule.h"
#include "ISourceControlProvider.h"
#include "ISourceControlRevision.h"
#include "MergeUtils.h"
#include "Kismet2/SClassPickerDialog.h"
#include "ObjectTools.h"
#include "SDetailsDiff.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "Core/ArcCoreAssetManager.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"

#define LOCTEXT_NAMESPACE "UArcAssetDefinition_ArcItemDefinition"
namespace Arcx::Editor
{
	void HandleOnUpdateItemFromTemplate(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TSet<FName> EditableAssets;
			{
				const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
				EditableAssets.Reserve(SelectedItems.Num());
				for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
				{
					if (SelectedItem.CanEdit())
					{
						EditableAssets.Add(SelectedItem.GetInternalPath());
					}
				}
			}
			ensure(!EditableAssets.IsEmpty());
			
			TArray<UArcItemDefinition*> ItemDefinitions = Context->LoadSelectedObjectsIf<UArcItemDefinition>([&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});

			if (ItemDefinitions[0]->GetSourceTemplate() == nullptr)
			{
				return;
			}
			
			const bool bPressedOk = FArcStaticItemHelpers::PreviewUpdate(nullptr, ItemDefinitions);

			if (!bPressedOk)
			{
				return;
			}
			
			for (UArcItemDefinition* ItemDefinition : ItemDefinitions)
			{
				if (ItemDefinition->GetSourceTemplate())
				{
					ItemDefinition->GetSourceTemplate()->UpdateFromTemplate(ItemDefinition);
					ItemDefinition->MarkPackageDirty();
				}
			}
		}
	}
	void HandleOnSetItemSourceTemplate(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TSet<FName> EditableAssets;
			{
				const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
				EditableAssets.Reserve(SelectedItems.Num());
				for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
				{
					if (SelectedItem.CanEdit())
					{
						EditableAssets.Add(SelectedItem.GetInternalPath());
					}
				}
			}
			ensure(!EditableAssets.IsEmpty());
			
			TArray<UArcItemDefinition*> ItemDefinitions = Context->LoadSelectedObjectsIf<UArcItemDefinition>([&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});
			UArcItemDefinitionTemplate* SelectedTemplate = nullptr;
			const bool bPressedOk = FArcStaticItemHelpers::PickItemSourceTemplateWithPreview(SelectedTemplate, ItemDefinitions, EArcMergeMode::Set);

			if (bPressedOk == false)
			{
				return;
			}

			// You can still click Ok and do not select template.
			if (SelectedTemplate == nullptr)
			{
				return;
			}

			
			
			for (UArcItemDefinition* ItemDefinition : ItemDefinitions)
			{
				SelectedTemplate->SetItemTemplate(ItemDefinition);
			}
		}
	}
	void HandleOnSetReplaceItemSourceTemplate(const FToolMenuContext& MenuContext)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			TSet<FName> EditableAssets;
			{
				const TArray<FContentBrowserItem>& SelectedItems = Context->GetSelectedItems();
				EditableAssets.Reserve(SelectedItems.Num());
				for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
				{
					if (SelectedItem.CanEdit())
					{
						EditableAssets.Add(SelectedItem.GetInternalPath());
					}
				}
			}
			ensure(!EditableAssets.IsEmpty());
			
			TArray<UArcItemDefinition*> ItemDefinitions = Context->LoadSelectedObjectsIf<UArcItemDefinition>([&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});
			
			UArcItemDefinitionTemplate* SelectedTemplate = nullptr;
			const bool bPressedOk = FArcStaticItemHelpers::PickItemSourceTemplateWithPreview(SelectedTemplate, ItemDefinitions, EArcMergeMode::SetNewOrReplace);

			if (bPressedOk == false)
			{
				return;
			}

			// You can still click Ok and do not select template.
			if (SelectedTemplate == nullptr)
			{
				return;
			}

			
			
			for (UArcItemDefinition* ItemDefinition : ItemDefinitions)
			{
				SelectedTemplate->SetNewOrReplaceItemTemplate(ItemDefinition);
			}
		}
	}
	bool IsTemplateFunctionVisible(const FToolMenuContext& MenuContext)
	{
		bool bCanEdit = true;
		TSet<FName> EditableAssets;
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
		{
			for (const FContentBrowserItem& SelectedItem : Context->GetSelectedItems())
			{
				if (SelectedItem.CanEdit())
				{
					EditableAssets.Add(SelectedItem.GetInternalPath());
				}
			}
			TArray<UArcItemDefinition*> ItemDefinitions = Context->LoadSelectedObjectsIf<UArcItemDefinition>([&EditableAssets](const FAssetData& AssetData)
			{
				return EditableAssets.Contains(*AssetData.GetObjectPathString());
			});

			for (UArcItemDefinition* ItemDef : ItemDefinitions)
			{
				if (ItemDef->GetClass()->IsChildOf(UArcItemDefinitionTemplate::StaticClass()))
				{
					bCanEdit = false;
					return false;
				}
			}
		}
		
		return bCanEdit;
	}
	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
			UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
			{
				FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
				UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UArcItemDefinition::StaticClass());
	        
				FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
				
				Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateStatic([](FToolMenuSection& InSection)
				{
					{
						const TAttribute<FText> Label = LOCTEXT("ArcAssetDefinition_UpdateFromTemplate", "Update From Template");
						const TAttribute<FText> ToolTip = LOCTEXT("ArcAssetDefinition_UpdateFromTemplateTip", "Update Fragments and properties from Template. Remove fragments not present in template, add new ones, and any existing in both left unchanged.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset");

						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnUpdateItemFromTemplate);
						UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsTemplateFunctionVisible);
						
						InSection.AddMenuEntry("ArcAssetDefinition_UpdateFromTemplate", Label, ToolTip, Icon, UIAction);
					}
					{
						const TAttribute<FText> Label = LOCTEXT("ArcAssetDefinition_SetSourceTemplate", "Set Source Template");
						const TAttribute<FText> ToolTip = LOCTEXT("ArcAssetDefinition_SetSourceTemplateTip", "Set New Item Template, preserving any existing item fragments.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset");

						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnSetItemSourceTemplate);
						UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsTemplateFunctionVisible);
						
						InSection.AddMenuEntry("ArcAssetDefinition_SetSourceTemplate", Label, ToolTip, Icon, UIAction);
					}
					{
						const TAttribute<FText> Label = LOCTEXT("ArcAssetDefinition_SetNewReplaceSourceTemplate", "Set New/Replace Source Template");
						const TAttribute<FText> ToolTip = LOCTEXT("ArcAssetDefinition_SetNewReplaceSourceTemplateTip", "Set or replace source Item Template. Resetting all properties and fragments to defaults from Template.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.DataAsset");

						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&HandleOnSetReplaceItemSourceTemplate);
						UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateStatic(&IsTemplateFunctionVisible);
						
						InSection.AddMenuEntry("ArcAssetDefinition_SetNewReplaceSourceTemplate", Label, ToolTip, Icon, UIAction);
					}
				}));
			}));
		});
}

FText UArcAssetDefinition_ArcItemDefinition::GetAssetDisplayName(const FAssetData& AssetData) const
{
	if (AssetData.IsValid())
	{
		FPrimaryAssetId AssetId = UArcCoreAssetManager::Get().ExtractPrimaryAssetIdFromData(AssetData);		
		if (AssetId.IsValid())
		{
			return FText::Format(LOCTEXT("ItemDefinitionTypeName", "Item Definition ({0})")
				, FText::FromString(AssetId.ToString()));
		}
		else
		{
			return FText(LOCTEXT("ItemDefinitionTypeName", "Item Definition"));
		}
	}

	return FText::GetEmpty();
}

EAssetCommandResult UArcAssetDefinition_ArcItemDefinition::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	return Super::OpenAssets(OpenArgs);
}

EAssetCommandResult UArcAssetDefinition_ArcItemDefinition::PerformAssetDiff(const FAssetDiffArgs& DiffArgs) const
{
	if (DiffArgs.OldAsset == nullptr && DiffArgs.NewAsset == nullptr)
	{
		return EAssetCommandResult::Unhandled;
	}
	
	SDetailsDiff::CreateDiffWindow(DiffArgs.OldAsset, DiffArgs.NewAsset, DiffArgs.OldRevision, DiffArgs.NewRevision, UDataAsset::StaticClass());
	return EAssetCommandResult::Handled;
}

bool UArcAssetDefinition_ArcItemDefinition::CanMerge() const
{
	return true;
}


namespace Arcx
{
	struct ScopedMergeResolveTransaction
	{
		ScopedMergeResolveTransaction(UObject* InManagedObject, EMergeFlags InFlags)
			: ManagedObject(InManagedObject)
			, Flags(InFlags)
		{
			if (Flags & MF_HANDLE_SOURCE_CONTROL)
			{
				UndoHandler = NewObject<UArcUndoableResolveHandler>();
				UndoHandler->SetFlags(RF_Transactional);
				UndoHandler->SetManagedObject(ManagedObject);
			
				TransactionNum = GEditor->BeginTransaction(LOCTEXT("ResolveMerge", "ResolveAutoMerge"));
				ensure(UndoHandler->Modify());
				ensure(ManagedObject->Modify());
			}
		}

		void Cancel()
		{
			bCanceled = true;
		}
	
		~ScopedMergeResolveTransaction()
		{
			if (Flags & MF_HANDLE_SOURCE_CONTROL)
			{
				if (!bCanceled)
				{
					UndoHandler->MarkResolved();
					GEditor->EndTransaction();
				}
				else
				{
					ManagedObject->GetPackage()->SetDirtyFlag(false);
					GEditor->CancelTransaction(TransactionNum);
				}
			}
		}

		UObject* ManagedObject;
		UArcUndoableResolveHandler* UndoHandler = nullptr;
		EMergeFlags Flags;
		int TransactionNum = 0;
		bool bCanceled = false;
	};

	static TArray<uint8> SerializeToBinary(UObject* Object, UObject* Default = nullptr)
	{
		TArray<uint8> Result;
		FObjectWriter ObjectWriter(Result);
		ObjectWriter.SetIsPersistent(true);
		Default = Default ? ToRawPtr(Default) : ToRawPtr(Object->GetClass()->ClassDefaultObject);
		Object->GetClass()->SerializeTaggedProperties( ObjectWriter, reinterpret_cast<uint8*>(Object), Default->GetClass(), reinterpret_cast<uint8*>(Default));
		return Result;
	};

	static void DeserializeFromBinary(UObject* Object, const TArray<uint8>& Data, UObject* Default = nullptr)
	{
		FObjectReader ObjectReader(Data);
		ObjectReader.SetIsPersistent(true);
		Default = Default ? ToRawPtr(Default) : ToRawPtr(Object->GetClass()->ClassDefaultObject);
		Object->GetClass()->SerializeTaggedProperties(ObjectReader, reinterpret_cast<uint8*>(Object), Default->GetClass(), reinterpret_cast<uint8*>(Default));
	};
}

EAssetCommandResult UArcAssetDefinition_ArcItemDefinition::Merge(const FAssetManualMergeArgs& MergeArgs) const
{
	return MergeUtils::Merge(MergeArgs);
}

EAssetCommandResult UArcAssetDefinition_ArcItemDefinition::Merge(const FAssetAutomaticMergeArgs& MergeArgs) const
{
	return MergeUtils::Merge(MergeArgs);
}

void UArcUndoableResolveHandler::SetManagedObject(UObject* Object)
{
	ManagedObject = Object;
	const UPackage* Package = ManagedObject->GetPackage();
	const FString Filepath = FPaths::ConvertRelativePathToFull(Package->GetLoadedPath().GetLocalFullPath());
	
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	const FSourceControlStatePtr SourceControlState = Provider.GetState(Package, EStateCacheUsage::Use);
	BaseRevisionNumber = SourceControlState->GetResolveInfo().BaseRevision;
	CurrentRevisionNumber = FString::FromInt(SourceControlState->GetCurrentRevision()->GetRevisionNumber());
	CheckinIdentifier = SourceControlState->GetCheckInIdentifier();

	// save package and copy the package to a temp file so it can be reverted
	const FString BaseFilename = FPaths::GetBaseFilename(Filepath);
	BackupFilepath = FPaths::CreateTempFilename(*(FPaths::ProjectSavedDir()/TEXT("Temp")), *BaseFilename.Left(32));
	ensure(FPlatformFileManager::Get().GetPlatformFile().CopyFile(*BackupFilepath, *Filepath));
}

void UArcUndoableResolveHandler::MarkResolved()
{
	const UPackage* Package = ManagedObject->GetPackage();
	const FString Filepath = FPaths::ConvertRelativePathToFull(Package->GetLoadedPath().GetLocalFullPath());
	
	ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
	Provider.Execute(ISourceControlOperation::Create<FResolve>(), TArray{Filepath}, EConcurrency::Synchronous);
	bShouldBeResolved = true;
}

void UArcUndoableResolveHandler::PostEditUndo()
{
	if (bShouldBeResolved) // undo resolution
	{
		MarkResolved();
	}
	else // redo resolution
	{
		UPackage* Package = ManagedObject->GetPackage();
		const FString Filepath = FPaths::ConvertRelativePathToFull(Package->GetLoadedPath().GetLocalFullPath());
		
		// to force the file to revert to it's pre-resolved state, we must revert, sync back to base revision,
		// apply the conflicting changes, then sync forward again.
		ISourceControlProvider& Provider = ISourceControlModule::Get().GetProvider();
		{
			const TSharedRef<FSync> SyncOperation = ISourceControlOperation::Create<FSync>();
			SyncOperation->SetRevision(BaseRevisionNumber);
			Provider.Execute(SyncOperation, Filepath, EConcurrency::Synchronous);
		}
		
		ResetLoaders(Package);
		Provider.Execute( ISourceControlOperation::Create<FRevert>(), Filepath, EConcurrency::Synchronous);

		{
			const TSharedRef<FCheckOut> CheckoutOperation = ISourceControlOperation::Create<FCheckOut>();
			Provider.Execute(CheckoutOperation, CheckinIdentifier, {Filepath}, EConcurrency::Synchronous);
		}

		ensure(FPlatformFileManager::Get().GetPlatformFile().CopyFile(*Filepath, *BackupFilepath));
		
		{
			const TSharedRef<FSync> SyncOperation = ISourceControlOperation::Create<FSync>();
			SyncOperation->SetRevision(CurrentRevisionNumber);
			Provider.Execute(SyncOperation, Filepath, EConcurrency::Synchronous);
		}

		Provider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), {Filepath}, EConcurrency::Asynchronous);
	}
	UObject::PostEditUndo();
}

#undef LOCTEXT_NAMESPACE
