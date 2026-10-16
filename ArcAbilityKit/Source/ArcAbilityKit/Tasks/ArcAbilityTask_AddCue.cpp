// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_AddCue.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcCues/ArcCueFunctions.h"
#include "ArcCues/ArcCuesModule.h"
#include "Cues/ArcCueSourceContext.h"
#include "Engine/World.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Sources/ArcAbilitySource_Item.h"

EStateTreeRunStatus FArcAbilityTask_AddCue::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	InstanceData.Handle = FArcCueHandle::Invalid();
	if (World == nullptr || InstanceData.Definition == nullptr)
	{
		UE_LOGF(LogArcCues, Warning, "[CueLifecycle] AddCue task rejected: World=%ls DefinitionSet=%d", *GetNameSafe(World), InstanceData.Definition != nullptr);
		return EStateTreeRunStatus::Failed;
	}

	FArcCueParameters Params;
	Params.OwnerEntity = InstanceData.OwnerEntity;
	Params.TargetEntity = InstanceData.TargetEntity;
	Params.SourceEntity = AbilityContext.GetEntity();

	FArcCueSourceContext SourceContext;
	const FArcActiveAbility* ActiveAbility = ArcAbilities::FindAbility(
		AbilityContext.GetEntityManager(),
		AbilityContext.GetEntity(),
		AbilityContext.GetAbilityHandle());
	SourceContext.SourceAbilityDefinition = ActiveAbility != nullptr ? ActiveAbility->Definition : nullptr;
	const FArcAbilitySource_Item* ItemSource = ActiveAbility != nullptr
		? ActiveAbility->CustomData.Find<FArcAbilitySource_Item>()
		: nullptr;
	SourceContext.SourceItemDefinition = ItemSource != nullptr ? ItemSource->ItemDefinition : FPrimaryAssetId();
	SourceContext.SourceAbilityHandle = AbilityContext.GetAbilityHandle();
	SourceContext.SourceAbilityOwnerEntity = AbilityContext.GetEntity();
	Params.SourceContext = FInstancedStruct::Make<FArcCueSourceContext>(SourceContext);

	InstanceData.Handle = ArcCues::AddCue(World, InstanceData.Definition, Params);
	UE_LOGF(LogArcCues, Log, "[CueLifecycle] AddCue task output: Handle=%u World=%ls AbilityEntity=%ls Owner=%ls Target=%ls",
		InstanceData.Handle.GetId(), *GetNameSafe(World), *AbilityContext.GetEntity().DebugGetDescription(),
		*InstanceData.OwnerEntity.DebugGetDescription(), *InstanceData.TargetEntity.DebugGetDescription());

	return InstanceData.Handle.IsValid() ? EStateTreeRunStatus::Running : EStateTreeRunStatus::Failed;
}
