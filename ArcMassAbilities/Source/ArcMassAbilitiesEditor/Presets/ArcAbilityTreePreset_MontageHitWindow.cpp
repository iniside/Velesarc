// Copyright Lukasz Baran. All Rights Reserved.

#include "Presets/ArcAbilityTreePreset_MontageHitWindow.h"

#include "Abilities/Tasks/ArcAbilityTask_ApplyEffectToTargets.h"
#include "Abilities/Tasks/ArcAbilityTask_Commit.h"
#include "Abilities/Tasks/ArcAbilityTask_ReadGlobalTargeting.h"
#include "Abilities/Tasks/ArcAbilityTask_PlayMontage.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorNode.h"
#include "StateTreeState.h"
#include "StateTreeTasksStatus.h"
#include "StateTreeTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcAbilityTreePreset_MontageHitWindow)

FText UArcAbilityTreePreset_MontageHitWindow::GetDisplayName() const
{
	return NSLOCTEXT("ArcAbilityTreePreset", "MontageHitWindowDisplayName", "Montage + Hit Window");
}

void UArcAbilityTreePreset_MontageHitWindow::BuildTree(UStateTreeEditorData& EditorData) const
{
	UStateTreeState& RootState = EditorData.AddRootState();
	UStateTreeState& AttackState = RootState.AddChildState(TEXT("Attack"));

	AttackState.TasksCompletion = EStateTreeTaskCompletionType::All;

	AttackState.AddTask<FArcAbilityTask_Commit>().GetNode().bConsideredForCompletion = false;

	AttackState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	UStateTreeState& WindupState = AttackState.AddChildState(TEXT("Windup"));
	TStateTreeEditorNode<FArcAbilityTask_PlayMontage>& MontageNode = WindupState.AddTask<FArcAbilityTask_PlayMontage>();
	MontageNode.Instance.GetMutable<FArcAbilityTask_PlayMontageInstanceData>().PlayRate = 1.0f;
	WindupState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::NextState);

	UStateTreeState& ImpactState = AttackState.AddChildState(TEXT("Impact"));
	ImpactState.TasksCompletion = EStateTreeTaskCompletionType::All;

	const int32 TargetingIndex = ImpactState.Tasks.Num();
	ImpactState.AddTask<FArcAbilityTask_ReadGlobalTargeting>();
	const int32 ApplyIndex = ImpactState.Tasks.Num();
	ImpactState.AddTask<FArcAbilityTask_ApplyEffectToTargets>();
	ImpactState.AddTransition(EStateTreeTransitionTrigger::OnStateCompleted, EStateTreeTransitionType::Succeeded);

	const bool bBound = EditorData.AddPropertyBinding(
		ImpactState.Tasks[TargetingIndex], TEXT("TargetDataHandle"),
		ImpactState.Tasks[ApplyIndex], TEXT("TargetDataHandle"));
	ensureAlwaysMsgf(bBound, TEXT("Montage hit-window preset failed to bind TargetDataHandle."));
}
