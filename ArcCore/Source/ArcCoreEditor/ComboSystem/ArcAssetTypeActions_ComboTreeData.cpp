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

#include "ComboSystem/ArcAssetTypeActions_ComboTreeData.h"
#include "AbilitySystem/Combo/ArcComboComponent.h"
#include "ArcComboTreeEditor.h"

#define LOCTEXT_NAMESPACE "FAssetTypeActions_ComboTreeName"

FText FArcAssetTypeActions_ComboTreeData::GetName() const
{
	return LOCTEXT("FAssetTypeActions_ComboTreeName"
		, "Combo Tree");
}

FColor FArcAssetTypeActions_ComboTreeData::GetTypeColor() const
{
	return FColor(221
		, 226
		, 26);
}

UClass* FArcAssetTypeActions_ComboTreeData::GetSupportedClass() const
{
	return UArcComboGraph::StaticClass();
}

void FArcAssetTypeActions_ComboTreeData::OpenAssetEditor(const TArray<UObject*>& InObjects
														 , TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	// FAssetTypeActions_Base::OpenAssetEditor(InObjects, EditWithinLevelEditor);
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
									? EToolkitMode::WorldCentric
									: EToolkitMode::Standalone;
	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UArcComboGraph* PropData = Cast<UArcComboGraph>(*ObjIt);
		if (PropData)
		{
			TSharedRef<FArcComboTreeEditor> NewTechTreeEditor(new FArcComboTreeEditor());
			NewTechTreeEditor->InitAbilityTreeEditor(Mode
				, EditWithinLevelEditor
				, PropData);
		}
	}
}

#undef LOCTEXT_NAMESPACE
