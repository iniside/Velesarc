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

class SWidget;
class SArcModifierTreeView;
class UArcJsonRecordEditObject;
struct FInstancedStruct;

/**
 * Cells shared by the material, pool and static column schemas: the modifier fields common to every
 * FArcCraftModifier subtype, and the debug chips read from the tree view's current evaluation.
 */
namespace ArcModifierSharedCells
{
	/** Resolves the modifier struct a cell edits; null when the row no longer exists. */
	using FModifierLocator = TFunction<FInstancedStruct*(UArcJsonRecordEditObject*)>;

	/** Editable StatId of a FArcCraftModifier_Stats row; empty cell for other subtypes. */
	TSharedRef<SWidget> MakeStatIdCell(UArcJsonRecordEditObject* Record, FModifierLocator Locate);

	/** Editable SlotTags (chair tags, Modifier.*) of any FArcCraftModifier row. */
	TSharedRef<SWidget> MakeSlotTagsCell(UArcJsonRecordEditObject* Record, FModifierLocator Locate);

	/** Debug chip: band edges for the debug rarity, ArcCraftRarity::ComputeBandValue at U = 0 and U = 1. */
	TSharedRef<SWidget> MakeBandCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey);

	/** Debug chip: rule weight (ComputeRuleWeight) on rule rows; pool entry selection weight on entry rows. */
	TSharedRef<SWidget> MakeWeightCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey);

	/** Debug chip: required tags of a rule or pool entry row that the debug ingredients hold. */
	TSharedRef<SWidget> MakeMatchedTagsCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey);

	/** Debug chip: consumed amount share of the slots holding the row's required tags. */
	TSharedRef<SWidget> MakeIngredientShareCell(TWeakPtr<SArcModifierTreeView> TreeViewWeak, int32 RowKey);
}
