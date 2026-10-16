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
 * distributed under the LICENSE is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "MaterialProperty/ArcMaterialPropertyTableDetailCustomization.h"

#include "DetailLayoutBuilder.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"

TSharedRef<IDetailCustomization> FArcMaterialPropertyTableDetailCustomization::MakeInstance()
{
	return MakeShared<FArcMaterialPropertyTableDetailCustomization>();
}

void FArcMaterialPropertyTableDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.HideProperty(DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(FArcMaterialPropertyTableData, Rules), FArcMaterialPropertyTableData::StaticStruct()));
}
