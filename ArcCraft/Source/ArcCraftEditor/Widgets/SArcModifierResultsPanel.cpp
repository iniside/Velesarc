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

#include "Widgets/SArcModifierResultsPanel.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SArcModifierResultsPanel"

// ---------------------------------------------------------------------------
// Row key encoding constants (must match ArcMaterialPropertyTable_DebugEval,
// ArcRandomPool_DebugEval, and ArcStaticModifierSet_DebugEval):
//   Parent row (flat or rule/entry level): key < 10000
//   Nested modifier row: key = ParentIdx * 10000 + ModIdx
// ---------------------------------------------------------------------------

namespace SArcModifierResultsPanelImpl
{

static const int32 NestedKeyThreshold = 10000;

/** True when this key encodes a nested modifier (child of a rule/entry). */
static bool IsNestedModifierKey(int32 Key)
{
	return Key >= NestedKeyThreshold;
}

/** Extract the parent index from a nested modifier key. */
static int32 ParentIndexFromKey(int32 Key)
{
	return Key / NestedKeyThreshold;
}

/** True when the row has eligible outputs worth displaying. */
static bool HasOutputs(const FArcModifierRowEvaluation& Eval)
{
	return Eval.bModifierEligible && Eval.ResolvedOutputs.Num() > 0;
}

} // namespace SArcModifierResultsPanelImpl

// ---------------------------------------------------------------------------
// Construct
// ---------------------------------------------------------------------------

void SArcModifierResultsPanel::Construct(const FArguments& InArgs)
{
	SAssignNew(Body, SVerticalBox);

	ChildSlot
	[
		SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("PanelTitle", "Simulated Output"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			.Padding(FMargin(6.0f, 4.0f))
			[
				Body.ToSharedRef()
			]
		]
	];

	Rebuild();
}

// ---------------------------------------------------------------------------
// SetEvaluation / Rebuild
// ---------------------------------------------------------------------------

void SArcModifierResultsPanel::SetEvaluation(const FArcModifierDebugEvaluation& InEvaluation)
{
	CachedEvaluation = InEvaluation;
	Rebuild();
}

void SArcModifierResultsPanel::Rebuild()
{
	if (!Body.IsValid())
	{
		return;
	}

	Body->ClearChildren();

	// -----------------------------------------------------------------------
	// Collect eligible outputs from ByRowKey.
	//
	// Two shapes coexist in the map:
	//   A) Flat (StaticModifierSet): all keys < 10000, each key is a modifier index.
	//      No parent grouping — these are top-level modifiers.
	//   B) Nested (MPT / RandomPool): parent rows have key < 10000 (rule/entry),
	//      child modifier rows have key >= 10000 (ParentIdx * 10000 + ModIdx).
	//      In this shape, parent rows carry rule/entry-level eval but no outputs.
	//
	// Strategy: group nested modifier rows by their ParentIdx; emit a "Group N"
	// header before each group's lines.  Flat modifier rows (key < 10000,
	// bModifierEligible, non-empty outputs) are collected separately and emitted
	// without a group header.
	// -----------------------------------------------------------------------

	// Collect flat rows (StaticModifierSet shape).
	TArray<int32> FlatKeys;

	// Collect nested modifier rows grouped by parent index.
	// TMap<ParentIdx, sorted child keys>
	TMap<int32, TArray<int32>> NestedByParent;

	// Determine whether any nested modifier key is present.  When present, keys < 10000
	// are parent (rule/entry) rows, not flat modifier rows — skip them for output.
	bool bHasNestedKeys = false;
	for (const TPair<int32, FArcModifierRowEvaluation>& Pair : CachedEvaluation.ByRowKey)
	{
		if (SArcModifierResultsPanelImpl::IsNestedModifierKey(Pair.Key))
		{
			bHasNestedKeys = true;
			break;
		}
	}

	for (const TPair<int32, FArcModifierRowEvaluation>& Pair : CachedEvaluation.ByRowKey)
	{
		const int32 Key = Pair.Key;
		const FArcModifierRowEvaluation& Eval = Pair.Value;

		if (!SArcModifierResultsPanelImpl::HasOutputs(Eval))
		{
			continue;
		}

		if (SArcModifierResultsPanelImpl::IsNestedModifierKey(Key))
		{
			const int32 ParentIdx = SArcModifierResultsPanelImpl::ParentIndexFromKey(Key);
			TArray<int32>& Children = NestedByParent.FindOrAdd(ParentIdx);
			Children.Add(Key);
		}
		else if (!bHasNestedKeys)
		{
			// Flat shape: key IS a modifier index.
			FlatKeys.Add(Key);
		}
		// else: this is a parent (rule/entry) row in a nested shape — skip.
	}

	FlatKeys.Sort();
	for (TPair<int32, TArray<int32>>& Pair : NestedByParent)
	{
		Pair.Value.Sort();
	}

	// -----------------------------------------------------------------------
	// Render flat modifier rows (StaticModifierSet).
	// -----------------------------------------------------------------------

	for (const int32 Key : FlatKeys)
	{
		const FArcModifierRowEvaluation* Eval = CachedEvaluation.ByRowKey.Find(Key);
		if (Eval == nullptr) { continue; }

		// Modifier header: "Modifier N"
		Body->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 4.0f, 0.0f, 1.0f))
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("ModifierHeader", "Modifier {0}"),
				FText::AsNumber(Key)))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)))
		];

		for (const FText& Line : Eval->ResolvedOutputs)
		{
			Body->AddSlot()
			.AutoHeight()
			.Padding(FMargin(12.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(Line)
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 0.7f, 1.0f)))
			];
		}
	}

	// -----------------------------------------------------------------------
	// Render nested modifier rows grouped by parent (MPT / RandomPool).
	// -----------------------------------------------------------------------

	// Sort parent indices for stable top-to-bottom ordering.
	TArray<int32> SortedParents;
	NestedByParent.GetKeys(SortedParents);
	SortedParents.Sort();

	for (const int32 ParentIdx : SortedParents)
	{
		const TArray<int32>& ChildKeys = NestedByParent[ParentIdx];

		// Group header: "Group N" (rule/entry index).
		Body->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 6.0f, 0.0f, 2.0f))
		[
			SNew(STextBlock)
			.Text(FText::Format(
				LOCTEXT("GroupHeader", "Group {0}"),
				FText::AsNumber(ParentIdx)))
			.Font(FAppStyle::GetFontStyle("BoldFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.85f, 0.85f, 0.85f, 1.0f)))
		];

		for (const int32 ChildKey : ChildKeys)
		{
			const FArcModifierRowEvaluation* ChildEval = CachedEvaluation.ByRowKey.Find(ChildKey);
			if (ChildEval == nullptr) { continue; }

			// Modifier sub-header: "Modifier N"
			const int32 ModIdx = ChildKey % SArcModifierResultsPanelImpl::NestedKeyThreshold;
			Body->AddSlot()
			.AutoHeight()
			.Padding(FMargin(8.0f, 2.0f, 0.0f, 1.0f))
			[
				SNew(STextBlock)
				.Text(FText::Format(
					LOCTEXT("NestedModifierHeader", "Modifier {0}"),
					FText::AsNumber(ModIdx)))
				.Font(FAppStyle::GetFontStyle("SmallFont"))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.9f, 1.0f)))
			];

			for (const FText& Line : ChildEval->ResolvedOutputs)
			{
				Body->AddSlot()
				.AutoHeight()
				.Padding(FMargin(20.0f, 0.0f, 0.0f, 0.0f))
				[
					SNew(STextBlock)
					.Text(Line)
					.Font(FAppStyle::GetFontStyle("SmallFont"))
					.ColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 0.7f, 1.0f)))
				];
			}
		}
	}

	// -----------------------------------------------------------------------
	// Placeholder when nothing to show.
	// -----------------------------------------------------------------------

	const bool bHasAnyOutput = FlatKeys.Num() > 0 || SortedParents.Num() > 0;
	if (!bHasAnyOutput)
	{
		Body->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 2.0f))
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoOutputs",
				"No eligible modifiers — enable debug preview and adjust the context to see outputs."))
			.Font(FAppStyle::GetFontStyle("SmallFont"))
			.ColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f)))
			.AutoWrapText(true)
		];
	}
}

#undef LOCTEXT_NAMESPACE
