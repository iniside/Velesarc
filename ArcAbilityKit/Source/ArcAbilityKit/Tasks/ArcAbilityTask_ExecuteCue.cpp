// Copyright Lukasz Baran. All Rights Reserved.

#include "Tasks/ArcAbilityTask_ExecuteCue.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "ArcCues/ArcCueFunctions.h"
#include "Cues/ArcCueSourceContext.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Sources/ArcAbilitySource_Item.h"

EStateTreeRunStatus FArcAbilityTask_ExecuteCue::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	if (World == nullptr || InstanceData.Definition == nullptr)
	{
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

	ArcCues::ExecuteCue(World, InstanceData.Definition, Params);

	return EStateTreeRunStatus::Succeeded;
}
