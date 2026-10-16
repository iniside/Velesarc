// Copyright Lukasz Baran. All Rights Reserved.

#include "Sim/ArcBodyTraits.h"

#include "ArcNeedSignals.h"
#include "Engine/World.h"
#include "Mass/EntityManager.h"
#include "MassCommonFragments.h"
#include "MassEntityTemplateRegistry.h"
#include "MassMovementFragments.h"
#include "MassNavigationFragments.h"
#include "MassNavMeshNavigationFragments.h"
#include "MassSmartObjectFragments.h"
#include "MassSignalSubsystem.h"
#include "Sim/ArcBodyAssignmentFragments.h"
#include "Sim/ArcBodyFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcBodyTraits)

void UArcBodyTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FTransformFragment>();
	BuildContext.RequireFragment<FMassSmartObjectUserFragment>();
	BuildContext.RequireFragment<FMassMoveTargetFragment>();
	BuildContext.RequireFragment<FAgentRadiusFragment>();
	BuildContext.RequireFragment<FAgentHeightFragment>();
	BuildContext.RequireFragment<FMassDesiredMovementFragment>();
	BuildContext.RequireFragment<FMassNavMeshShortPathFragment>();
	BuildContext.RequireFragment<FMassNavMeshCachedPathFragment>();

	BuildContext.AddFragment<FArcBodyFragment>();
	BuildContext.AddFragment<FArcSettlementMembershipFragment>();
	BuildContext.AddFragment<FArcWorkAssignmentFragment>();
	BuildContext.AddFragment<FArcGatherAssignmentFragment>();
	BuildContext.AddFragment<FArcCarrierAssignmentFragment>();
	FArcCarryCapabilityFragment& Capability = BuildContext.AddFragment_GetRef<FArcCarryCapabilityFragment>();
	Capability.Capacity = CarryCapacity;
	BuildContext.AddConstSharedFragment(FConstSharedStruct::Make(UtilityConfig));
}

void UArcBodyTrait::BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const
{
	Out.AddSparseFragment<FArcBodyUtilityIntentFragment>();
}

void UArcBodyTrait::InitializeEntity(
	FMassEntityManager& Mgr,
	const FMassEntityHandle Entity,
	const UWorld& World) const
{
	UMassSignalSubsystem* SignalSubsystem = UWorld::GetSubsystem<UMassSignalSubsystem>(&World);
	if (SignalSubsystem)
	{
		SignalSubsystem->SignalEntity(ArcNeeds::Signals::UtilityEvaluate, Entity);
	}
}
