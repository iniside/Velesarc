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

/** Kind of row in the modifier tree view. */
enum class EArcModifierRowKind : uint8
{
	/** A container/rule row (e.g. a material property table rule). */
	Rule,
	/** A pool entry row (e.g. a random pool entry). */
	Entry,
	/** A terminal modifier row (FArcCraftModifier subtype). */
	Modifier,
	/** The sentinel "+Add" row at the bottom of a list. */
	AddRow,
};

/**
 * One row in the SArcModifierTreeView.
 * Each item may have child items (e.g. a Rule containing Modifiers).
 */
struct FArcModifierTreeItem
{
	/** What kind of row this is. */
	EArcModifierRowKind Kind = EArcModifierRowKind::Modifier;

	/** Index of this item inside its parent's array, or the top-level asset array.
	 *  For AddRow sentinels this is INDEX_NONE. */
	int32 ParentIndex = INDEX_NONE;

	/** Index of this item within its own level (i.e. within the parent's Children array
	 *  or the top-level array). */
	int32 ChildIndex = INDEX_NONE;

	/** Child items (e.g. modifiers inside a rule). */
	TArray<TSharedPtr<FArcModifierTreeItem>> Children;
};
