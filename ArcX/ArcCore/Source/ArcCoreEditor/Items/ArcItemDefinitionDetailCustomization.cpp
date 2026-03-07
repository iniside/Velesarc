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

#include "ArcItemDefinitionDetailCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "StructUtils/InstancedStruct.h"

TSharedRef<IDetailCustomization> FArcItemDefinitionDetailCustomization::MakeInstance()
{
	return MakeShared<FArcItemDefinitionDetailCustomization>();
}

void FArcItemDefinitionDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	// Fragment sets are displayed in the editor's left panel, hide them from the details view
	DetailBuilder.HideProperty(DetailBuilder.GetProperty("FragmentSet"));
	DetailBuilder.HideProperty(DetailBuilder.GetProperty("ScalableFloatFragmentSet"));
#if WITH_EDITORONLY_DATA
	DetailBuilder.HideProperty(DetailBuilder.GetProperty("EditorFragmentSet"));
#endif
}
