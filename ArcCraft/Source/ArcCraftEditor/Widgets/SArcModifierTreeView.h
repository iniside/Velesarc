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
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/ArcModifierTreeItem.h"
#include "Widgets/ArcModifierColumnSchema.h"
#include "Widgets/ArcModifierDebugContext.h"

class ITableRow;
class STableViewBase;
class SHeaderRow;
class UArcJsonRecordEditObject;

DECLARE_DELEGATE(FArcOnModifierTreeRefresh);
DECLARE_DELEGATE_OneParam(FArcOnModifierRowSelected, TSharedPtr<FArcModifierTreeItem>);
DECLARE_DELEGATE_OneParam(FArcOnModifierRowAction, TSharedPtr<FArcModifierTreeItem> /*Selected, may be null*/);
DECLARE_DELEGATE_TwoParams(FArcOnModifierRowMove, int32 /*From*/, int32 /*To*/);
DECLARE_DELEGATE_OneParam(FArcOnDebugEvaluationChanged, const FArcModifierDebugEvaluation& /*NewEval*/);

/**
 * Optional callback that produces a debug evaluation from the current asset + context.
 * Injected per-asset so the tree view stays decoupled from specific evaluator functions.
 */
DECLARE_DELEGATE_RetVal_TwoParams(
	FArcModifierDebugEvaluation,
	FArcModifierDebugEvaluatorFn,
	UArcJsonRecordEditObject* /*Record*/,
	const FArcModifierDebugContext& /*Ctx*/);

/**
 * Callback that builds the flat/nested row list from the asset's current data.
 * Called by RebuildRows() each time the asset changes.
 */
DECLARE_DELEGATE_RetVal_TwoParams(
	TArray<TSharedPtr<FArcModifierTreeItem>>,
	FArcModifierBuildRowsFn,
	UArcJsonRecordEditObject* /*Record*/,
	const FArcModifierDebugContext& /*DebugCtx*/);

/**
 * Shared STreeView-based editor widget for all three modifier-bearing assets.
 *
 * Layout: vertical toolbar (Add, Duplicate, Delete, Move Up/Down, Expand/Collapse)
 *         above a STreeView with SHeaderRow.
 *
 * Column schema is injected per-asset so the same widget handles
 * static modifier set, random pool and material property table records.
 */
class SArcModifierTreeView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcModifierTreeView) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UArcJsonRecordEditObject>, Record)
		/** Column schema for this asset type. Lifetime owned by the caller. */
		SLATE_ARGUMENT(TArray<FArcModifierColumn>, ColumnSchema)
		/** Label for the primary "+ Add" button (e.g. "+ Add Rule", "+ Add Entry"). Falls back to "+ Add" if empty. */
		SLATE_ARGUMENT(FText, AddTopLevelLabel)
		/** Which row kind is the draggable top-level kind for this asset.
		 *  Drag handles are suppressed on rows whose Kind != TopLevelKind.
		 *  MPT = Rule, RandomPool = Entry, StaticModifierSet = Modifier. */
		SLATE_ARGUMENT(EArcModifierRowKind, TopLevelKind)
		/** Delegate that rebuilds the row list from the current asset state. */
		SLATE_EVENT(FArcModifierBuildRowsFn, OnBuildRows)
		/** Optional: evaluator called on each SetDebugContext to populate CurrentEvaluation.
		 *  When unbound, evaluation is skipped and all row tints stay neutral. */
		SLATE_EVENT(FArcModifierDebugEvaluatorFn, OnEvaluateDebug)
		/** Called when the selection changes. */
		SLATE_EVENT(FArcOnModifierRowSelected, OnRowSelected)
		/** Add a top-level row (Rule / Entry / root Modifier). Always enabled. */
		SLATE_EVENT(FArcOnModifierRowAction, OnAddTopLevel)
		/** Add a child modifier under the currently selected Rule or Entry. Visible only when bound. */
		SLATE_EVENT(FArcOnModifierRowAction, OnAddChildOfSelected)
		/** Duplicate the selected row. */
		SLATE_EVENT(FArcOnModifierRowAction, OnDuplicateRow)
		/** Delete the selected row. */
		SLATE_EVENT(FArcOnModifierRowAction, OnDeleteRow)
		/** Reorder a top-level row: (FromIndex, ToIndex). */
		SLATE_EVENT(FArcOnModifierRowMove, OnReorderTopLevel)
		/** Fired after SetDebugContext updates CurrentEvaluation. */
		SLATE_EVENT(FArcOnDebugEvaluationChanged, OnEvaluationChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Full rebuild: re-run OnBuildRows, rebuild header, request list refresh. */
	void RebuildAll();

	/** Light refresh: keep existing rows but call RequestListRefresh() on the tree. */
	void RefreshRows();

	/** Update the debug context and rebuild. */
	void SetDebugContext(const FArcModifierDebugContext& NewContext);

	/** Move a row at SourceIndex to TargetIndex (within the top-level array). */
	void MoveRow(int32 SourceIndex, int32 TargetIndex);

	/**
	 * Select the row matching the given key triple, fire OnRowSelectedDelegate.
	 * Intended for callers that just called RebuildAll() and want to auto-select
	 * the newly added row. No-op if no matching item is found.
	 */
	void SelectByKey(EArcModifierRowKind Kind, int32 ParentIdx, int32 ChildIdx);

	TWeakObjectPtr<UArcJsonRecordEditObject> GetRecord() const { return RecordWeak; }

	/** Returns the most-recently computed debug evaluation (may be empty if no evaluator is bound). */
	const FArcModifierDebugEvaluation& GetCurrentEvaluation() const { return CurrentEvaluation; }

private:
	// STreeView callbacks
	TSharedRef<ITableRow> OnGenerateRow(
		TSharedPtr<FArcModifierTreeItem> Item,
		const TSharedRef<STableViewBase>& OwnerTable);

	void OnGetChildren(
		TSharedPtr<FArcModifierTreeItem> Item,
		TArray<TSharedPtr<FArcModifierTreeItem>>& OutChildren);

	void OnSelectionChanged(
		TSharedPtr<FArcModifierTreeItem> Item,
		ESelectInfo::Type SelectType);

	// Toolbar button handlers (expand/collapse remain as member functions)
	FReply OnExpandAllClicked();
	FReply OnCollapseAllClicked();

	void RebuildHeaderRow();

	TWeakObjectPtr<UArcJsonRecordEditObject> RecordWeak;
	TArray<FArcModifierColumn> ColumnSchema;
	FText AddTopLevelLabel;
	EArcModifierRowKind TopLevelKind = EArcModifierRowKind::Modifier;
	FArcModifierBuildRowsFn OnBuildRowsDelegate;
	FArcModifierDebugEvaluatorFn OnEvaluateDebugDelegate;
	FArcOnModifierRowSelected OnRowSelectedDelegate;
	FArcOnModifierRowAction OnAddTopLevelDelegate;
	FArcOnModifierRowAction OnAddChildOfSelectedDelegate;
	FArcOnModifierRowAction OnDuplicateRowDelegate;
	FArcOnModifierRowAction OnDeleteRowDelegate;
	FArcOnModifierRowMove OnReorderTopLevelDelegate;
	FArcOnDebugEvaluationChanged OnEvaluationChangedDelegate;
	FArcModifierDebugContext DebugContext;
	FArcModifierDebugEvaluation CurrentEvaluation;

	TArray<TSharedPtr<FArcModifierTreeItem>> RootItems;
	TSharedPtr<STreeView<TSharedPtr<FArcModifierTreeItem>>> TreeView;
	TSharedPtr<SHeaderRow> HeaderRow;
	TSharedPtr<FArcModifierTreeItem> SelectedItem;
};
