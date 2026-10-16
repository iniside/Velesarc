// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcTacticsRequirements.h"

#include "Actions/ArcTacticsAction.h"
#include "Considerations/ArcTacticsConsiderations.h"
#include "Decision/ArcTacticsCompiledData.h"
#include "GlobalTasks/ArcTacticsGlobalTask.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityManager.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassNavigationFragments.h"
#include "Perception/ArcMassSightPerception.h"
#include "SmoothOrientation/MassSmoothOrientationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "Teams/ArcMassTeamFragment.h"

namespace ArcTacticsRequirementsPrivate
{
	void CheckElement(
		const FMassArchetypeCompositionDescriptor& Composition,
		const UScriptStruct* ElementType,
		TArray<FName>& OutMissing)
	{
		if (!Composition.Contains(ElementType))
		{
			OutMissing.AddUnique(ElementType->GetFName());
		}
	}
}

namespace ArcTacticsRequirements
{
	EArcTacticsExecutionRequirement Gather(const FArcTacticsCompiledData& CompiledData)
	{
		EArcTacticsExecutionRequirement Requirements = EArcTacticsExecutionRequirement::None;
		if (!CompiledData.IsExecutable())
		{
			return Requirements;
		}
		for (const FArcTacticsCompiledNode& CompiledNode : CompiledData.Nodes)
		{
			const FArcTacticsNode* Node = CompiledNode.Node.GetPtr<FArcTacticsNode>();
			const int32 DataIndex = CompiledNode.InstanceData.GetIndex();
			const FConstStructView InstanceData =
				CompiledNode.InstanceData.GetKind() == EArcTacticsBindingEndpointKind::Node
				&& CompiledData.DefaultInstanceData.IsValidIndex(DataIndex)
				? CompiledData.DefaultInstanceData[DataIndex]
				: FConstStructView();
			if (const FArcTacticsGlobalTask* Task = Node
				? CompiledNode.Node.GetPtr<FArcTacticsGlobalTask>() : nullptr)
			{
				Requirements |= Task->GetRequirements(InstanceData);
			}
			else if (const FArcTacticsActionNode* Action = Node
				? CompiledNode.Node.GetPtr<FArcTacticsActionNode>() : nullptr)
			{
				Requirements |= Action->GetRequirements(InstanceData);
			}
			else if (const FArcTacticsStepNode* Step = Node
				? CompiledNode.Node.GetPtr<FArcTacticsStepNode>() : nullptr)
			{
				Requirements |= Step->GetRequirements(InstanceData);
			}
			else if (const FArcTacticsActiveStepNode* ActiveStep = Node
				? CompiledNode.Node.GetPtr<FArcTacticsActiveStepNode>() : nullptr)
			{
				Requirements |= ActiveStep->GetRequirements(InstanceData);
			}
			else if (const FArcTacticsConsiderationNode* Consideration = Node
				? CompiledNode.Node.GetPtr<FArcTacticsConsiderationNode>() : nullptr)
			{
				Requirements |= Consideration->GetRequirements(InstanceData);
			}
		}
		return Requirements;
	}

	bool FindMissing(
		const FMassEntityManager& EntityManager,
		const FMassEntityHandle Entity,
		const EArcTacticsExecutionRequirement Requirements,
		TArray<FName>& OutMissing)
	{
		using namespace ArcTacticsRequirementsPrivate;
		OutMissing.Reset();
		const FMassArchetypeHandle Archetype = EntityManager.IsEntityActive(Entity)
			? EntityManager.GetArchetypeForEntity(Entity)
			: FMassArchetypeHandle();
		const FMassArchetypeCompositionDescriptor Composition = Archetype.IsValid()
			? EntityManager.GetArchetypeComposition(Archetype)
			: FMassArchetypeCompositionDescriptor();
		if (EnumHasAnyFlags(Requirements, EArcTacticsExecutionRequirement::Transform))
		{
			CheckElement(Composition, FTransformFragment::StaticStruct(), OutMissing);
		}
		if (EnumHasAnyFlags(Requirements, EArcTacticsExecutionRequirement::Team))
		{
			CheckElement(Composition, FArcMassTeamFragment::StaticStruct(), OutMissing);
		}
		if (EnumHasAnyFlags(Requirements, EArcTacticsExecutionRequirement::Sight))
		{
			CheckElement(Composition, FArcMassSightPerceptionResult::StaticStruct(), OutMissing);
		}
		if (EnumHasAnyFlags(Requirements, EArcTacticsExecutionRequirement::PathFollow))
		{
			CheckElement(Composition, FTransformFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FAgentRadiusFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FAgentHeightFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassDesiredMovementFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassMoveTargetFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassNavMeshShortPathFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassNavMeshCachedPathFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassSteeringFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassStandingSteeringFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassGhostLocationFragment::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassMovementParameters::StaticStruct(), OutMissing);
			CheckElement(Composition, FMassSmoothOrientationParameters::StaticStruct(), OutMissing);
		}
		return !OutMissing.IsEmpty();
	}
}
