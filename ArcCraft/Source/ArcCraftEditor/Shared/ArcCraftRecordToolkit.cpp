/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "Shared/ArcCraftRecordToolkit.h"

#include "CoreGlobals.h"
#include "Editor.h"
#include "JsonAssets/ArcJsonAssetType.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Subsystems/AssetEditorSubsystem.h"

#define LOCTEXT_NAMESPACE "ArcCraftRecordToolkit"

namespace ArcCraftRecordToolkit
{
	FPrimaryAssetId GetRecordId(const UArcJsonRecordEditObject& Record)
	{
		const FArcJsonAssetTypeInfo* Type = ArcJsonAssets::FindTypeByFilename(Record.GetFilename());
		return Type != nullptr ? ArcJsonAssets::MakeRecordId(*Type, Record.GetRecord()) : FPrimaryAssetId();
	}

	FText GetRecordName(const UArcJsonRecordEditObject* Record)
	{
		return Record != nullptr ? FText::FromString(FPaths::GetBaseFilename(Record->GetFilename())) : FText::GetEmpty();
	}

	FText GetDirtySuffix(const UArcJsonRecordEditObject* Record)
	{
		return Record != nullptr && Record->IsDirty() ? LOCTEXT("DirtySuffix", "*") : FText::GetEmpty();
	}

	bool Save(UArcJsonRecordEditObject* Record)
	{
		if (Record == nullptr)
		{
			return false;
		}
		if (!Record->Save())
		{
			FMessageDialog::Open(EAppMsgType::Ok,
				FText::Format(LOCTEXT("SaveFailed", "{0} could not be written. See the output log."), FText::FromString(Record->GetFilename())),
				LOCTEXT("SaveFailedTitle", "Save failed"));
			return false;
		}
		return true;
	}

	bool ConfirmClose(UArcJsonRecordEditObject* Record, uint64& InOutDiscardFrame)
	{
		if (Record == nullptr || !Record->IsOpen() || !Record->IsDirty() || InOutDiscardFrame == GFrameCounter)
		{
			return true;
		}
		const EAppReturnType::Type Answer = FMessageDialog::Open(EAppMsgType::YesNoCancel,
			FText::Format(LOCTEXT("SaveBeforeClose", "Save changes to {0}?"), FText::FromString(FPaths::GetCleanFilename(Record->GetFilename()))),
			LOCTEXT("SaveBeforeCloseTitle", "Unsaved JSON asset"));
		if (Answer == EAppReturnType::Yes)
		{
			return Save(Record);
		}
		if (Answer == EAppReturnType::No)
		{
			// Discard waits for the committed close: an editor exit can still be cancelled by a later prompt this frame.
			InOutDiscardFrame = GFrameCounter;
			return true;
		}
		return false;
	}

	void CommitClose(UArcJsonRecordEditObject* Record, uint64& InOutDiscardFrame)
	{
		const bool bDiscard = InOutDiscardFrame == GFrameCounter;
		InOutDiscardFrame = MAX_uint64;
		if (bDiscard && Record != nullptr && Record->IsOpen())
		{
			Record->Discard();
		}
	}

	bool FocusOpenEditor(const FString& Filename)
	{
		UArcJsonRecordEditObject* Record = ArcJsonRecordEditing::FindRecord(Filename);
		if (Record == nullptr || GEditor == nullptr)
		{
			return false;
		}
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		return AssetEditorSubsystem != nullptr && AssetEditorSubsystem->FindEditorForAsset(Record, true) != nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
