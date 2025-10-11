// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMovableActorsVisualizationProcessor.h"

#include "MassStationaryISMSwitcherProcessor.h"

UArcMovableActorsVisualizationProcessor::UArcMovableActorsVisualizationProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
	// This processor needs to be executed before UMassStationaryISMSwitcherProcessors since that's the processor
	// responsible for executing what UInstancedActorsVisualizationProcessor calculates.
	// Missing this dependency would result in client-side one-frame representation absence when switching
	// from actor representation back to ISM.
	ExecutionOrder.ExecuteBefore.Add(UMassStationaryISMSwitcherProcessor::StaticClass()->GetFName());

	UpdateParams.bTestCollisionAvailibilityForActorVisualization = false;
}

void UArcMovableActorsVisualizationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) 
{
	Super::ConfigureQueries(EntityManager);

	EntityQuery.ClearTagRequirements(FMassTagBitSet(*FMassVisualizationProcessorTag::StaticStruct()));
	EntityQuery.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
}

UArcMovableActorsVisualizationLODProcessor::UArcMovableActorsVisualizationLODProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
}

void UArcMovableActorsVisualizationLODProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	
	CloseEntityQuery.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	CloseEntityAdjustDistanceQuery.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	FarEntityQuery.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	DebugEntityQuery.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	
}

UArcMovableActorsLODCollectorProcessor::UArcMovableActorsLODCollectorProcessor()
{
	bAutoRegisterWithProcessingPhases = true;
}

void UArcMovableActorsLODCollectorProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	EntityQuery_VisibleRangeAndOnLOD.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	EntityQuery_VisibleRangeOnly.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	EntityQuery_OnLODOnly.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
	EntityQuery_NotVisibleRangeAndOffLOD.AddTagRequirement<FArcMovableActorsVisualizationProcessorTag>(EMassFragmentPresence::All);
}
