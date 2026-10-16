// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcTacticsEditorClipboard.h"
#include "Decision/ArcTacticsCompiler.h"
#include "EditorUndoClient.h"

class UArcTacticsConfig;
class UScriptStruct;

enum class EArcTacticsEditorInsertLocation : uint8
{
	Before,
	After,
	Into
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FArcTacticsEditorSelectionChanged,
	const FArcTacticsEditorSelection&);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FArcTacticsEditorModelChanged,
	EArcTacticsAuthoringChange);
DECLARE_MULTICAST_DELEGATE(FArcTacticsEditorCompileResultsChanged);

class ARCTACTICSEDITOR_API FArcTacticsEditorViewModel final
	: public TSharedFromThis<FArcTacticsEditorViewModel>
	, public FEditorUndoClient
{
public:
	static TSharedRef<FArcTacticsEditorViewModel> Create(UArcTacticsConfig* InConfig);
	virtual ~FArcTacticsEditorViewModel() override;

	UArcTacticsConfig* GetConfig() const { return Config.Get(); }
	const FArcTacticsEditorSelection& GetSelection() const { return Selection; }
	void SetSelection(const FArcTacticsEditorSelection& InSelection);
	bool IsSelectionValid(const FArcTacticsEditorSelection& InSelection) const;
	bool FindSelectionForStructID(
		const FGuid& StructID,
		FArcTacticsEditorSelection& OutSelection) const;

	bool Compile();
	bool IsCompiledArtifactValid() const;
	const TArray<FArcTacticsCompilerDiagnostic>& GetCompileDiagnostics() const
	{
		return CompileDiagnostics;
	}

	bool CanAdd(
		EArcTacticsEditorItemKind Kind,
		const FArcTacticsEditorSelection& Parent,
		const UScriptStruct* NodeType,
		FText* OutError = nullptr) const;
	bool Add(
		EArcTacticsEditorItemKind Kind,
		const FArcTacticsEditorSelection& Parent,
		const UScriptStruct* NodeType);

	bool CanRename(const FArcTacticsEditorSelection& Item, FText* OutError = nullptr) const;
	bool Rename(const FArcTacticsEditorSelection& Item, FName NewName);

	bool CanReplaceType(
		const FArcTacticsEditorSelection& Item,
		const UScriptStruct* NodeType,
		FText* OutError = nullptr) const;
	bool ReplaceType(const FArcTacticsEditorSelection& Item, const UScriptStruct* NodeType);

	bool CanDelete(const FArcTacticsEditorSelection& Item, FText* OutError = nullptr) const;
	bool Delete(const FArcTacticsEditorSelection& Item);

	bool CanDuplicate(const FArcTacticsEditorSelection& Item, FText* OutError = nullptr) const;
	bool Duplicate(const FArcTacticsEditorSelection& Item);

	bool CanCopy(const FArcTacticsEditorSelection& Item, FText* OutError = nullptr) const;
	bool Copy(const FArcTacticsEditorSelection& Item);
	bool CanPaste(const FArcTacticsEditorSelection& Destination, FText* OutError = nullptr) const;
	bool Paste(const FArcTacticsEditorSelection& Destination);

	bool CanMove(
		const FArcTacticsEditorSelection& Item,
		const FArcTacticsEditorSelection& Target,
		EArcTacticsEditorInsertLocation Location,
		FText* OutError = nullptr) const;
	bool Move(
		const FArcTacticsEditorSelection& Item,
		const FArcTacticsEditorSelection& Target,
		EArcTacticsEditorInsertLocation Location);

	FArcTacticsEditorSelectionChanged& OnSelectionChanged() { return SelectionChanged; }
	FArcTacticsEditorModelChanged& OnModelChanged() { return ModelChanged; }
	FArcTacticsEditorCompileResultsChanged& OnCompileResultsChanged()
	{
		return CompileResultsChanged;
	}

	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	explicit FArcTacticsEditorViewModel(UArcTacticsConfig* InConfig);
	void Initialize();
	void HandleAuthoringChanged(EArcTacticsAuthoringChange Change);
	void ReconcileSelection();
	void SetCurrentSelection(const FArcTacticsEditorSelection& InSelection, bool bUpdatePreferred);

	TWeakObjectPtr<UArcTacticsConfig> Config;
	FDelegateHandle AuthoringChangedHandle;
	FArcTacticsEditorSelection Selection;
	FArcTacticsEditorSelection PreferredSelection;
	FArcTacticsEditorSelection PreferredFallbackSelection;
	FArcTacticsEditorSelectionChanged SelectionChanged;
	FArcTacticsEditorModelChanged ModelChanged;
	FArcTacticsEditorCompileResultsChanged CompileResultsChanged;
	TArray<FArcTacticsCompilerDiagnostic> CompileDiagnostics;
};
