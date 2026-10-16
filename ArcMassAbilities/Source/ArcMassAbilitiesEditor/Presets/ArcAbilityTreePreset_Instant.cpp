// Copyright Lukasz Baran. All Rights Reserved.

#include "Presets/ArcAbilityTreePreset_Instant.h"

#include "Abilities/Tasks/ArcAbilityTask_ApplyEffectToTargets.h"
#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Abilities/Tasks/ArcAbilityTask_ReadGlobalTargeting.h"
#include "StateTreeEditorData.h"
#include "StateTreeState.h"
#include "StateTreeTasksStatus.h"
#include "StateTreeTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityTreePreset_Instant)

FText UArcAbilityTreePreset_Instant::GetDisplayName() const
{
	return NSLOCTEXT("ArcAbilityTreePreset", "InstantDisplayName", "Instant");
}

void UArcAbilityTreePreset_Instant::BuildTree(UStateTreeEditorData& EditorData) const
{
	UStateTreeState& RootState = EditorData.AddRootState();
	UStateTreeState& ActivateState = RootState.AddChildState(TEXT("Activate"));

	ActivateState.TasksCompletion = EStateTreeTaskCompletionType::All;

	ActivateState.AddTask<FArcAbilityTask_Commit>();
	const int32 TargetingIndex = ActivateState.Tasks.Num();
	ActivateState.AddTask<FArcAbilityTask_ReadGlobalTargeting>();
	const int32 ApplyIndex = ActivateState.Tasks.Num();
	ActivateState.AddTask<FArcAbilityTask_ApplyEffectToTargets>();

	ActivateState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	const bool bBound = EditorData.AddPropertyBinding(
		ActivateState.Tasks[TargetingIndex], TEXT("TargetDataHandle"),
		ActivateState.Tasks[ApplyIndex], TEXT("TargetDataHandle"));
	ensureAlwaysMsgf(bBound, TEXT("Instant preset failed to bind TargetDataHandle."));
}
