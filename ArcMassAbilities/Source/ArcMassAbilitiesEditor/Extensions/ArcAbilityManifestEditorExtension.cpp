// Copyright Lukasz Baran. All Rights Reserved.

#include "Extensions/ArcAbilityManifestEditorExtension.h"
#include "Abilities/Extensions/ArcAbilityManifestExtension.h"
#include "Effects/ArcEffectDefinition.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "MassEntityConfigAsset.h"
#include "StateTreeCompiler.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "StateTreeState.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitEnhancedInput.h"
#include "Abilities/Tasks/ArcAbilityTask_WaitEnhancedInputGated.h"
#include "Abilities/Tasks/ArcAbilityTask_InputPressed.h"
#include "Abilities/Tasks/ArcAbilityTask_InputReleased.h"
#include "Abilities/Tasks/ArcAbilityTask_InputReleasedOnce.h"
#include "Abilities/Tasks/ArcAbilityTask_ApplyEffect.h"
#include "Abilities/Tasks/ArcAbilityTask_ApplyEffectToTargets.h"
#include "Abilities/Tasks/ArcAbilityTask_SpawnActor.h"
#include "Abilities/Tasks/ArcAbilityTask_SpawnActorProjectile.h"
#include "Abilities/Tasks/ArcAbilityTask_SpawnMassEntity.h"
#include "Abilities/Tasks/ArcAbilityTask_SpawnMassProjectile.h"
#include "Logging/MessageLog.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityManifestEditorExtension)

namespace ArcAbilityManifestEditor
{
	/**
	 * POD sink that accumulates manifest-relevant references while walking the state tree.
	 * Decouples the walk logic from the specific destination (runtime extension or
	 * editor detail panel display).
	 */
	struct FAbilityManifestSink
	{
		TArray<TObjectPtr<UInputAction>>           InputActions;
		TArray<TObjectPtr<UInputMappingContext>>   InputMappingContexts;
		FGameplayTagContainer                       InputTags;
		TArray<TObjectPtr<UArcEffectDefinition>>   AppliedEffects;
		TArray<TSubclassOf<AActor>>                SpawnedActorClasses;
		TArray<TObjectPtr<UMassEntityConfigAsset>> SpawnedEntityConfigs;
		TArray<TObjectPtr<UScriptStruct>>          ClaimedSparseFragments;
		TArray<FString>                            SchedulingMisconfigurations;
		int32                                       GatedTaskCount = 0;

		bool IsEmpty() const
		{
			return InputActions.IsEmpty()
				&& InputMappingContexts.IsEmpty()
				&& InputTags.IsEmpty()
				&& AppliedEffects.IsEmpty()
				&& SpawnedActorClasses.IsEmpty()
				&& SpawnedEntityConfigs.IsEmpty()
				&& ClaimedSparseFragments.IsEmpty()
				&& SchedulingMisconfigurations.IsEmpty()
				&& GatedTaskCount == 0;
		}
	};

	/**
	 * Inspect a single editor node and append any manifest-relevant references to Sink.
	 * Also increments Sink.GatedTaskCount for each gated input task encountered.
	 *
	 * Dispatch order:
	 *   1. Check the node struct directly (FArcAbilityTask_ApplyEffect carries EffectDefinition on the node).
	 *   2. Fall through to instance-data struct checks for all other tasks.
	 */
	void VisitNode(const FStateTreeEditorNode& EditorNode, FAbilityManifestSink& Sink)
	{
		// --- Scheduling-misconfiguration lint ---
		// Must run BEFORE the per-task dispatches (which have early returns) so that
		// every FArcAbilityTaskBase-derived task — including ApplyEffect — is covered.

		const TStructView<FStateTreeNodeBase> NodeView = EditorNode.GetNode();
		if (const UScriptStruct* NodeStruct = NodeView.GetScriptStruct())
		{
			if (NodeStruct->IsChildOf(FArcAbilityTaskBase::StaticStruct()))
			{
				const FArcAbilityTaskBase* AsTaskBase =
					reinterpret_cast<const FArcAbilityTaskBase*>(NodeView.GetMemory());
				if (AsTaskBase && AsTaskBase->DrivesScheduledTick() && AsTaskBase->bConsideredForScheduling)
				{
					Sink.SchedulingMisconfigurations.Add(NodeStruct->GetName());
				}
			}
		}

		// --- Node-struct dispatch ---

		if (NodeView.GetScriptStruct() == FArcAbilityTask_ApplyEffect::StaticStruct())
		{
			const FArcAbilityTask_ApplyEffect* Task = NodeView.GetPtr<FArcAbilityTask_ApplyEffect>();
			if (Task && Task->EffectDefinition)
			{
				Sink.AppliedEffects.AddUnique(Task->EffectDefinition);
			}
			return;
		}

		// --- Instance-data struct dispatch ---

		const FStateTreeDataView InstanceView = EditorNode.GetInstance();
		// GetStruct() returns const UStruct*; StateTree instance views always hold UScriptStruct, never UClass.
		const UScriptStruct* InstStruct = static_cast<const UScriptStruct*>(InstanceView.GetStruct());
		if (!InstStruct)
		{
			return;
		}

		if (InstStruct == FArcAbilityTask_WaitEnhancedInputInstanceData::StaticStruct())
		{
			const FArcAbilityTask_WaitEnhancedInputInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_WaitEnhancedInputInstanceData>();
			if (Data)
			{
				if (Data->InputAction)         { Sink.InputActions.AddUnique(Data->InputAction); }
				if (Data->InputMappingContext) { Sink.InputMappingContexts.AddUnique(Data->InputMappingContext); }
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_WaitEnhancedInputGatedInstanceData::StaticStruct())
		{
			const FArcAbilityTask_WaitEnhancedInputGatedInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_WaitEnhancedInputGatedInstanceData>();
			if (Data)
			{
				if (Data->InputAction)         { Sink.InputActions.AddUnique(Data->InputAction); }
				if (Data->InputMappingContext) { Sink.InputMappingContexts.AddUnique(Data->InputMappingContext); }
			}
			++Sink.GatedTaskCount;
			return;
		}

		if (InstStruct == FArcAbilityTask_InputPressedInstanceData::StaticStruct())
		{
			const FArcAbilityTask_InputPressedInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_InputPressedInstanceData>();
			if (Data && Data->InputTag.IsValid())
			{
				Sink.InputTags.AddTag(Data->InputTag);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_InputReleasedInstanceData::StaticStruct())
		{
			const FArcAbilityTask_InputReleasedInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_InputReleasedInstanceData>();
			if (Data && Data->InputTag.IsValid())
			{
				Sink.InputTags.AddTag(Data->InputTag);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_InputReleasedOnceInstanceData::StaticStruct())
		{
			const FArcAbilityTask_InputReleasedOnceInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_InputReleasedOnceInstanceData>();
			if (Data && Data->InputTag.IsValid())
			{
				Sink.InputTags.AddTag(Data->InputTag);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_ApplyEffectToTargetsInstanceData::StaticStruct())
		{
			const FArcAbilityTask_ApplyEffectToTargetsInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_ApplyEffectToTargetsInstanceData>();
			if (Data && Data->EffectDefinition)
			{
				Sink.AppliedEffects.AddUnique(Data->EffectDefinition);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_SpawnActorInstanceData::StaticStruct())
		{
			const FArcAbilityTask_SpawnActorInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_SpawnActorInstanceData>();
			if (Data && Data->ActorClass)
			{
				Sink.SpawnedActorClasses.AddUnique(Data->ActorClass);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_SpawnActorProjectileInstanceData::StaticStruct())
		{
			const FArcAbilityTask_SpawnActorProjectileInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_SpawnActorProjectileInstanceData>();
			if (Data && Data->ActorClass)
			{
				Sink.SpawnedActorClasses.AddUnique(Data->ActorClass);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_SpawnMassEntityInstanceData::StaticStruct())
		{
			const FArcAbilityTask_SpawnMassEntityInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_SpawnMassEntityInstanceData>();
			if (Data && Data->EntityConfig)
			{
				Sink.SpawnedEntityConfigs.AddUnique(Data->EntityConfig);
			}
			return;
		}

		if (InstStruct == FArcAbilityTask_SpawnMassProjectileInstanceData::StaticStruct())
		{
			const FArcAbilityTask_SpawnMassProjectileInstanceData* Data =
				InstanceView.GetPtr<FArcAbilityTask_SpawnMassProjectileInstanceData>();
			if (Data && Data->EntityConfig)
			{
				Sink.SpawnedEntityConfigs.AddUnique(Data->EntityConfig);
			}
			return;
		}
	}

	/**
	 * Walk the condition nodes of a transition and visit each one.
	 * Condition nodes rarely carry manifest-relevant data in v0, but this keeps the
	 * walk uniform and future-proofs against conditions that reference input actions.
	 */
	void VisitTransitionConditions(const FStateTreeTransition& Transition, FAbilityManifestSink& Sink)
	{
		for (const FStateTreeEditorNode& CondNode : Transition.Conditions)
		{
			VisitNode(CondNode, Sink);
		}
	}

	/**
	 * Walk all task nodes, enter conditions, and transition conditions of a single state
	 * and accumulate results into Sink.
	 */
	void VisitState(const UStateTreeState& State, FAbilityManifestSink& Sink)
	{
		for (const FStateTreeEditorNode& CondNode : State.EnterConditions)
		{
			VisitNode(CondNode, Sink);
		}

		for (const FStateTreeEditorNode& TaskNode : State.Tasks)
		{
			VisitNode(TaskNode, Sink);
		}

		VisitNode(State.SingleTask, Sink);

		for (const FStateTreeTransition& Transition : State.Transitions)
		{
			VisitTransitionConditions(Transition, Sink);
		}
	}

	// -------------------------------------------------------------------------
	// Detail-panel formatting helpers
	// -------------------------------------------------------------------------

	template <typename T>
	FString FormatObjectList(TConstArrayView<TObjectPtr<T>> Items)
	{
		if (Items.IsEmpty())
		{
			return FString();
		}
		FString Result;
		for (const TObjectPtr<T>& Item : Items)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += GetNameSafe(Item.Get());
		}
		return Result;
	}

	FString FormatClassList(TConstArrayView<TSubclassOf<AActor>> Classes)
	{
		if (Classes.IsEmpty())
		{
			return FString();
		}
		FString Result;
		for (const TSubclassOf<AActor>& Cls : Classes)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			Result += GetNameSafe(Cls.Get());
		}
		return Result;
	}

	FString FormatStructList(TConstArrayView<TObjectPtr<UScriptStruct>> Structs)
	{
		if (Structs.IsEmpty())
		{
			return FString();
		}
		FString Result;
		for (const TObjectPtr<UScriptStruct>& Struct : Structs)
		{
			if (!Result.IsEmpty())
			{
				Result += TEXT(", ");
			}
			if (Struct)
			{
				Result += Struct->GetName();
			}
		}
		return Result;
	}

	void AddManifestRow(IDetailCategoryBuilder& Category, const FString& LabelText, const FString& ValueText)
	{
		if (ValueText.IsEmpty())
		{
			return;
		}

		Category.AddCustomRow(FText::FromString(LabelText))
			.NameContent()
			[
				SNew(STextBlock)
					.Text(FText::FromString(LabelText))
			]
			.ValueContent()
			[
				SNew(STextBlock)
					.Text(FText::FromString(ValueText))
					.AutoWrapText(true)
			];
	}

} // namespace ArcAbilityManifestEditor

// ---------------------------------------------------------------------------

void UArcAbilityManifestEditorExtension::PreValidate()
{
	// Mirror fields and GatedTaskCount intentionally NOT reset here. PreValidate
	// runs on every save / property edit while HandlePostInternalCompile only
	// runs on Compile, so clearing mirror state here would leave the row blank
	// most of the time. Instead the mirror reflects the last successful compile
	// and stays stable across edits; the next Compile overwrites it.
	bMultiGateConflict = false;
}

bool UArcAbilityManifestEditorExtension::HandlePostInternalCompile(
	const UE::StateTree::Compiler::FPostInternalContext& Context)
{
	const UStateTreeEditorData* EditorData = Context.GetEditorData();

	UArcAbilityManifestExtension* Manifest = Cast<UArcAbilityManifestExtension>(
		Context.AddExtension(UArcAbilityManifestExtension::StaticClass()));

	if (!Manifest)
	{
		Context.GetLog().Reportf(
			EMessageSeverity::Error,
			FStateTreeBindableStructDesc{},
			TEXT("Arc Ability Manifest: Failed to create UArcAbilityManifestExtension on the compiled StateTree. "
			     "Ensure ArcMassAbilities is loaded and the StateTree schema accepts extensions."));
		return false;
	}

	// Reset gated-task counter for this compile pass; PreValidate may have run before.
	GatedTaskCount = 0;

	ArcAbilityManifestEditor::FAbilityManifestSink Sink;

	EditorData->VisitHierarchy(
		[&Sink](UStateTreeState& State, UStateTreeState* /*ParentState*/) -> EStateTreeVisitor
		{
			ArcAbilityManifestEditor::VisitState(State, Sink);
			return EStateTreeVisitor::Continue;
		});

	// Populate the runtime extension from the sink.
	Manifest->InputActions           = Sink.InputActions;
	Manifest->InputMappingContexts   = Sink.InputMappingContexts;
	Manifest->InputTags              = Sink.InputTags;
	Manifest->AppliedEffects         = Sink.AppliedEffects;
	Manifest->SpawnedActorClasses    = Sink.SpawnedActorClasses;
	Manifest->SpawnedEntityConfigs   = Sink.SpawnedEntityConfigs;
	Manifest->ClaimedSparseFragments = Sink.ClaimedSparseFragments;

	// Copy into editor-extension mirror fields for the Details panel.
	InputActions           = Sink.InputActions;
	InputMappingContexts   = Sink.InputMappingContexts;
	InputTags              = Sink.InputTags;
	AppliedEffects         = Sink.AppliedEffects;
	SpawnedActorClasses    = Sink.SpawnedActorClasses;
	SpawnedEntityConfigs   = Sink.SpawnedEntityConfigs;
	ClaimedSparseFragments = Sink.ClaimedSparseFragments;

	GatedTaskCount = Sink.GatedTaskCount;

	if (GatedTaskCount > 1)
	{
		bMultiGateConflict = true;
		Context.GetLog().Reportf(
			EMessageSeverity::Warning,
			TEXT("Arc Ability Manifest: %d gated input tasks present. "
			     "Verify they are in mutually exclusive states or share a single sparse-fragment serial owner."),
			GatedTaskCount);
	}

	for (const FString& BadTaskName : Sink.SchedulingMisconfigurations)
	{
		Context.GetLog().Reportf(
			EMessageSeverity::Warning,
			TEXT("Arc Ability Manifest: task '%s' overrides DrivesScheduledTick()==true but has bConsideredForScheduling==true. "
			     "Set bConsideredForScheduling=false in the task's constructor — otherwise the state's tick aggregation forces "
			     "every-frame ticking and the scheduled wake never fires."),
			*BadTaskName);
	}

	return true;
}

void UArcAbilityManifestEditorExtension::CustomizeDetails(
	TNonNullPtr<UStateTreeState> State,
	IDetailLayoutBuilder& DetailBuilder)
{
	ArcAbilityManifestEditor::FAbilityManifestSink Sink;
	ArcAbilityManifestEditor::VisitState(*State, Sink);
	if (Sink.IsEmpty())
	{
		return;
	}

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(
		TEXT("AbilityManifestContribution"),
		NSLOCTEXT("ArcAbilityManifest", "AbilityManifestContribution", "Ability Manifest Contribution"),
		ECategoryPriority::Important);

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Input Actions"),
		ArcAbilityManifestEditor::FormatObjectList<UInputAction>(Sink.InputActions));

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Input Mapping Ctx"),
		ArcAbilityManifestEditor::FormatObjectList<UInputMappingContext>(Sink.InputMappingContexts));

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Input Tags"),
		Sink.InputTags.IsEmpty() ? FString() : Sink.InputTags.ToStringSimple());

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Effects"),
		ArcAbilityManifestEditor::FormatObjectList<UArcEffectDefinition>(Sink.AppliedEffects));

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Spawn Actors"),
		ArcAbilityManifestEditor::FormatClassList(Sink.SpawnedActorClasses));

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Spawn Entities"),
		ArcAbilityManifestEditor::FormatObjectList<UMassEntityConfigAsset>(Sink.SpawnedEntityConfigs));

	ArcAbilityManifestEditor::AddManifestRow(
		Category,
		TEXT("Sparse Fragments"),
		ArcAbilityManifestEditor::FormatStructList(Sink.ClaimedSparseFragments));

	if (Sink.GatedTaskCount > 0)
	{
		ArcAbilityManifestEditor::AddManifestRow(
			Category,
			TEXT("Gated Tasks"),
			FString::FromInt(Sink.GatedTaskCount));
	}
}
