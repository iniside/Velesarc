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

#pragma once


#include "Editor/Kismet/Public/BlueprintEditor.h"
#include "Editor/PropertyEditor/Public/PropertyEditorDelegates.h"
#include "Toolkits/IToolkitHost.h"
#include "Toolkits/SimpleAssetEditor.h"

class IDetailsView;
class SDockableTab;

class ARCCOREEDITOR_API FArcAssetEditor_ItemData : public FSimpleAssetEditor
{
public:
	/** Delegate that, given an array of assets, returns an array of objects to use in the
	 * details view of an FSimpleAssetEditor */
	DECLARE_DELEGATE_RetVal_OneParam(TArray<UObject*>
		, FGetDetailsViewObjects
		, const TArray<UObject*>&);

	/** The name given to all instances of this type of editor */
	static const FName ToolkitFName;

	static TSharedRef<FArcAssetEditor_ItemData> CreateItemEditor(const EToolkitMode::Type Mode
																 , const TSharedPtr<IToolkitHost>& InitToolkitHost
																 , UObject* ObjectToEdit
																 , FGetDetailsViewObjects GetDetailsViewObjects =
																		 FGetDetailsViewObjects());

	static TSharedRef<FArcAssetEditor_ItemData> CreateItemEditor(const EToolkitMode::Type Mode
																 , const TSharedPtr<IToolkitHost>& InitToolkitHost
																 , const TArray<UObject*>& ObjectsToEdit
																 , FGetDetailsViewObjects GetDetailsViewObjects =
																		 FGetDetailsViewObjects());
};
