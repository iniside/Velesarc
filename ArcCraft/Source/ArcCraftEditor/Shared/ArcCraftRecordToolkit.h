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

#pragma once

#include "CoreMinimal.h"
#include "Editing/ArcJsonRecordEditing.h"
#include "ScopedTransaction.h"
#include "UObject/PrimaryAssetId.h"

namespace ArcCraftRecordToolkit
{
	template <typename T>
	T* GetData(UArcJsonRecordEditObject* Record)
	{
		return Record != nullptr ? Record->GetMutableRecord().GetMutablePtr<T>() : nullptr;
	}

	template <typename TFunc>
	void Edit(UArcJsonRecordEditObject* Record, const FText& Description, TFunc&& Mutate)
	{
		if (Record == nullptr)
		{
			return;
		}
		const FScopedTransaction Transaction(Description);
		Record->Modify();
		Mutate();
		Record->PostEditChange();
	}

	FPrimaryAssetId GetRecordId(const UArcJsonRecordEditObject& Record);
	FText GetRecordName(const UArcJsonRecordEditObject* Record);
	FText GetDirtySuffix(const UArcJsonRecordEditObject* Record);
	bool Save(UArcJsonRecordEditObject* Record);
	bool ConfirmClose(UArcJsonRecordEditObject* Record, uint64& InOutDiscardFrame);
	void CommitClose(UArcJsonRecordEditObject* Record, uint64& InOutDiscardFrame);
	bool FocusOpenEditor(const FString& Filename);
}
