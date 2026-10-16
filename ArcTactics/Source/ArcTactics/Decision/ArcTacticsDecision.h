// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Decision/ArcTacticsInstance.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "Mass/EntityHandle.h"

class UArcAbilityDefinition;
struct FArcActiveAbility;
struct FArcTacticsInstanceSlot;
struct FArcTacticsPackage;
struct FMassEntityManager;

namespace ArcTacticsDecision
{
	ARCTACTICS_API EArcTacticsDecisionStatus BeginDecision(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsTaskContext& Context);

	ARCTACTICS_API EArcTacticsDecisionStatus TickDecision(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsTaskContext& Context);

	ARCTACTICS_API void CancelDecision(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsTaskContext& Context);

	ARCTACTICS_API bool ApplyActiveActionInputs(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsPackage& Package,
		const FArcTQSFunctionContext& FunctionContext);

	ARCTACTICS_API bool PublishActiveActionOutputs(
		FArcTacticsInstanceSlot& Slot,
		FArcTacticsPackage& Package,
		const FArcTQSFunctionContext& FunctionContext);

	ARCTACTICS_API void ComputeSelectionChances(
		TConstArrayView<float> Scores,
		TConstArrayView<int32> Priorities,
		const FArcTacticsSelectionSettings& Settings,
		TArray<float>& OutChances);

	ARCTACTICS_API const FArcActiveAbility* ResolveAbility(
		FMassEntityManager& EntityManager,
		FMassEntityHandle Entity,
		const UArcAbilityDefinition* Definition,
		bool bRequireInactive,
		FName& OutRejectionReason);
}
