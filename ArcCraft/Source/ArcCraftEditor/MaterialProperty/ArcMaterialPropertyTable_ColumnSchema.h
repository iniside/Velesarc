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
#include "Widgets/ArcModifierColumnSchema.h"

namespace ArcMaterialPropertyTable_ColumnSchema
{

/** Returns the column schema for Rule-kind rows (parent rows in the table). */
TArray<FArcModifierColumn> GetRuleRowSchema();

/** Returns the column schema for Modifier-kind rows (child modifiers under each rule). */
TArray<FArcModifierColumn> GetModifierChildRowSchema();

} // namespace ArcMaterialPropertyTable_ColumnSchema
