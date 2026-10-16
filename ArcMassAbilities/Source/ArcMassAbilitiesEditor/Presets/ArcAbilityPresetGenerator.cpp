// Copyright Lukasz Baran. All Rights Reserved.

#include "Presets/ArcAbilityPresetGenerator.h"

#include "Presets/ArcAbilityTreePreset.h"
#include "Extensions/ArcAbilityPresetEditorExtension.h"
#include "Abilities/Schema/ArcAbilityStateTreeSchema.h"

#include "Editor.h"
#include "IDetailsView.h"
#include "ScopedTransaction.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/MessageDialog.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeViewModel.h"

#define LOCTEXT_NAMESPACE "ArcAbilityPresetGenerator"

namespace ArcAbilityPresetGeneratorLocal
{
	void CollectCompilerMessages(const FStateTreeCompilerLog& Log, FArcAbilityPresetResult& OutResult)
	{
		const TArray<TSharedRef<FTokenizedMessage>> Messages = Log.ToTokenizedMessages();
		for (const TSharedRef<FTokenizedMessage>& Message : Messages)
		{
			const FString MessageText = Message->ToText().ToString();
			if (Message->GetSeverity() == EMessageSeverity::Error)
			{
				OutResult.Errors.Add(MessageText);
			}
			else
			{
				OutResult.Warnings.Add(MessageText);
			}
		}
	}

	bool IsTreeEffectivelyEmpty(const UStateTreeEditorData& EditorData)
	{
		if (!EditorData.Evaluators.IsEmpty() || !EditorData.GlobalTasks.IsEmpty())
		{
			return false;
		}

		if (EditorData.SubTrees.IsEmpty())
		{
			return true;
		}

		if (EditorData.SubTrees.Num() > 1)
		{
			return false;
		}

		const UStateTreeState* RootState = EditorData.SubTrees[0];
		return RootState != nullptr
			&& RootState->Type == EStateTreeStateType::State
			&& RootState->Children.IsEmpty()
			&& RootState->Tasks.IsEmpty()
			&& RootState->EnterConditions.IsEmpty()
			&& RootState->Considerations.IsEmpty()
			&& RootState->Transitions.IsEmpty();
	}

	FArcAbilityPresetResult BuildInto(UStateTree& StateTree, TSubclassOf<UArcAbilityTreePreset> PresetClass)
	{
		FArcAbilityPresetResult Result;

		if (PresetClass == nullptr || PresetClass->HasAnyClassFlags(CLASS_Abstract))
		{
			Result.Errors.Add(TEXT("No concrete preset class given."));
			return Result;
		}

		UArcAbilityTreePreset* Preset = PresetClass->GetDefaultObject<UArcAbilityTreePreset>();
		if (Preset == nullptr)
		{
			Result.Errors.Add(FString::Printf(TEXT("Preset class '%s' has no default object."), *PresetClass->GetPathName()));
			return Result;
		}

		UStateTreeEditorData* EditorData = UStateTreeEditingSubsystem::GetEditorData(&StateTree);
		EditorData->Modify();
		EditorData->SubTrees.Empty();
		EditorData->Evaluators.Empty();
		EditorData->GlobalTasks.Empty();

		Preset->BuildTree(*EditorData);

		// CompileStateTree runs ValidateStateTree itself; an explicit call here would be a second mutating self-heal pass.
		FStateTreeCompilerLog Log;
		const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(&StateTree, Log);

		CollectCompilerMessages(Log, Result);
		Result.bSuccess = bCompiled;

		if (bCompiled)
		{
			UArcAbilityPresetEditorExtension* Extension = EditorData->GetOrCreateExtension<UArcAbilityPresetEditorExtension>();
			Extension->SourcePreset = PresetClass;
			Extension->HashAtGeneration = UStateTreeEditingSubsystem::CalculateStateTreeHash(&StateTree);
		}

		return Result;
	}
}

FArcAbilityPresetResult ArcAbilityPresetGenerator::ConfirmAndBuild(UStateTree& StateTree, TSubclassOf<UArcAbilityTreePreset> PresetClass, TSharedPtr<IDetailsView> DetailsViewToRefresh)
{
	FArcAbilityPresetResult Result;

	if (PresetClass == nullptr || PresetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		Result.Errors.Add(TEXT("No concrete preset class given."));
		return Result;
	}

	const UArcAbilityTreePreset* Preset = PresetClass->GetDefaultObject<UArcAbilityTreePreset>();
	if (Preset == nullptr)
	{
		Result.Errors.Add(FString::Printf(TEXT("Preset class '%s' has no default object."), *PresetClass->GetPathName()));
		return Result;
	}

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree.EditorData);
	if (EditorData == nullptr)
	{
		Result.Errors.Add(FString::Printf(TEXT("'%s' has no editor data."), *StateTree.GetPathName()));
		return Result;
	}

	const bool bTreeIsEmpty = ArcAbilityPresetGeneratorLocal::IsTreeEffectivelyEmpty(*EditorData);
	if (!bTreeIsEmpty)
	{
		UArcAbilityPresetEditorExtension* ExistingExtension = EditorData->GetExtension<UArcAbilityPresetEditorExtension>();
		const uint32 HashAtGeneration = ExistingExtension != nullptr ? ExistingExtension->HashAtGeneration : 0;
		const bool bUntouchedSincePreset = HashAtGeneration != 0 && UStateTreeEditingSubsystem::CalculateStateTreeHash(&StateTree) == HashAtGeneration;

		const FText Message = bUntouchedSincePreset
			? FText::Format(LOCTEXT("ConfirmRegenerate", "Regenerate this tree from preset '{0}'?"), Preset->GetDisplayName())
			: FText::Format(LOCTEXT("ConfirmDiscard", "This tree has content that was not generated from a preset. Generating from '{0}' will discard it.\n\nContinue?"), Preset->GetDisplayName());

		if (FMessageDialog::Open(EAppMsgType::OkCancel, Message) != EAppReturnType::Ok)
		{
			Result.bCancelled = true;
			return Result;
		}
	}

	{
		FScopedTransaction Transaction(LOCTEXT("GenerateStateTreeFromPreset", "Generate State Tree From Preset"));
		StateTree.Modify();
		EditorData->Modify();

		UArcAbilityPresetEditorExtension* Extension = EditorData->GetOrCreateExtension<UArcAbilityPresetEditorExtension>();
		Extension->Modify();

		Result = ArcAbilityPresetGeneratorLocal::BuildInto(StateTree, PresetClass);
	}

	if (!Result.bSuccess)
	{
		const FString FirstError = Result.Errors.Num() > 0 ? Result.Errors[0] : TEXT("Unknown error.");
		FMessageDialog::Open(EAppMsgType::Ok, FText::Format(LOCTEXT("GenerateFailed", "Failed to generate from preset:\n\n{0}\n\nThe tree was left in the failed generated state. Undo (Ctrl+Z) restores its previous content."), FText::FromString(FirstError)));
		return Result;
	}

	if (GEditor != nullptr)
	{
		UStateTreeEditingSubsystem* EditingSubsystem = GEditor->GetEditorSubsystem<UStateTreeEditingSubsystem>();
		if (EditingSubsystem != nullptr)
		{
			TSharedPtr<FStateTreeViewModel> ViewModel = EditingSubsystem->FindViewModel(&StateTree);
			if (ViewModel.IsValid())
			{
				ViewModel->ClearSelection();
			}

			EditingSubsystem->RequestUIRefresh(&StateTree);
		}
	}

	UStateTreeEditingSubsystem::MarkAsModified(&StateTree);

	if (DetailsViewToRefresh.IsValid())
	{
		DetailsViewToRefresh->ForceRefresh();
	}

	return Result;
}

FArcAbilityPresetResult ArcAbilityPresetGenerator::ResetToPreset(UStateTree& StateTree, TSharedPtr<IDetailsView> DetailsViewToRefresh)
{
	FArcAbilityPresetResult Result;

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree.EditorData);
	if (EditorData == nullptr)
	{
		Result.Errors.Add(FString::Printf(TEXT("'%s' has no editor data."), *StateTree.GetPathName()));
		return Result;
	}

	if (!EditorData->Schema || !EditorData->Schema->IsA<UArcAbilityStateTreeSchema>())
	{
		Result.Errors.Add(FString::Printf(TEXT("'%s' does not use the Arc ability schema."), *StateTree.GetPathName()));
		return Result;
	}

	UArcAbilityPresetEditorExtension* Extension = EditorData->GetExtension<UArcAbilityPresetEditorExtension>();
	if (Extension == nullptr)
	{
		Result.Errors.Add(FString::Printf(TEXT("'%s' was not generated from a preset."), *StateTree.GetPathName()));
		return Result;
	}

	const TSubclassOf<UArcAbilityTreePreset> PresetClass = Extension->SourcePreset;
	if (PresetClass == nullptr)
	{
		Result.Errors.Add(FString::Printf(TEXT("'%s' has no source preset."), *StateTree.GetPathName()));
		return Result;
	}

	if (PresetClass->HasAnyClassFlags(CLASS_Abstract))
	{
		Result.Errors.Add(FString::Printf(TEXT("Preset class '%s' is abstract."), *PresetClass->GetPathName()));
		return Result;
	}

	if (PresetClass->GetDefaultObject<UArcAbilityTreePreset>() == nullptr)
	{
		Result.Errors.Add(FString::Printf(TEXT("Preset class '%s' has no default object."), *PresetClass->GetPathName()));
		return Result;
	}

	return ConfirmAndBuild(StateTree, PresetClass, DetailsViewToRefresh);
}

#undef LOCTEXT_NAMESPACE
