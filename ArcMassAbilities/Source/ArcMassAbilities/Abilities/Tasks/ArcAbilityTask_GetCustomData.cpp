// Copyright Lukasz Baran. All Rights Reserved.

#include "Abilities/Tasks/ArcAbilityTask_GetCustomData.h"
#include "Abilities/Schema/ArcAbilityExecutionContext.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityCustomData.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassAbilitiesLogs.h"

EStateTreeRunStatus FArcAbilityTask_GetCustomData::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FArcAbilityExecutionContext& AbilityContext = static_cast<FArcAbilityExecutionContext&>(Context);
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	const FMassEntityHandle Entity = AbilityContext.GetEntity();
	UWorld* World = AbilityContext.GetMassContext().GetWorld();

	const UScriptStruct* RequestedType = InstanceData.CustomData.GetScriptStruct();

	ARCMA_VLOG(World, Log,
		TEXT("[Ability] GetCustomData enter entity=[%d:%d] requested_type=%s"),
		Entity.Index, Entity.SerialNumber,
		RequestedType ? *RequestedType->GetName() : TEXT("none"));

	if (RequestedType == nullptr)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] GetCustomData FAILED: no RequestedType"));
		return EStateTreeRunStatus::Failed;
	}

	FMassEntityManager& EntityManager = AbilityContext.GetEntityManager();
	const FArcAbilityHandle AbilityHandle = AbilityContext.GetAbilityHandle();
	const FArcActiveAbility* ActiveAbility =
		ArcAbilities::FindAbility(EntityManager, Entity, AbilityHandle);
	if (ActiveAbility == nullptr)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] GetCustomData FAILED: ability not found entity=[%d:%d] handle=%s"),
			Entity.Index, Entity.SerialNumber, *AbilityHandle.ToString());
		return EStateTreeRunStatus::Failed;
	}

	const FInstancedStruct* Hit = ActiveAbility->CustomData.FindByScriptStruct(RequestedType);
	if (Hit == nullptr)
	{
		ARCMA_VLOG(World, Warning,
			TEXT("[Ability] GetCustomData FAILED: no matching entry type=%s entity=[%d:%d]"),
			*RequestedType->GetName(), Entity.Index, Entity.SerialNumber);
		return EStateTreeRunStatus::Failed;
	}

	ARCMA_VLOG(World, Log,
		TEXT("[Ability] GetCustomData: matched type=%s entity=[%d:%d]"),
		*RequestedType->GetName(), Entity.Index, Entity.SerialNumber);

	InstanceData.CustomData = *Hit;
	return EStateTreeRunStatus::Succeeded;
}
