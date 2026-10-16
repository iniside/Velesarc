// Copyright Lukasz Baran. All Rights Reserved.

#include "Mover/ArcChaosWalkingExecutor.h"

#include "ChaosMover/ChaosMoverSimulation.h"
#include "ChaosMover/ChaosMoverSimulationTypes.h"
#include "MoveLibrary/FloorQueryUtils.h"
#include "MoveLibrary/MoverBlackboard.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcChaosWalkingExecutor)

void UArcChaosWalkingExecutor::ExecuteMove_Async(const FSimulationTickParams& Params, FMoverTickEndData& OutputState)
{
	Super::ExecuteMove_Async(Params, OutputState);

	const FChaosMoverSimulationDefaultInputs* DefaultSimInputs = Simulation ? Simulation->GetLocalSimInput().FindDataByType<FChaosMoverSimulationDefaultInputs>() : nullptr;
	const UMoverBlackboard* SimBlackboard = Simulation ? Simulation->GetBlackboard() : nullptr;
	const FMoverDefaultSyncState* StartingSyncState = Params.StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
	FMoverDefaultSyncState* OutputSyncState = OutputState.SyncState.SyncStateCollection.FindMutableDataByType<FMoverDefaultSyncState>();
	FFloorCheckResult FloorResult;
	if (!DefaultSimInputs || !SimBlackboard || !StartingSyncState || !OutputSyncState
		|| !SimBlackboard->TryGet(CommonBlackboard::LastFloorResult, FloorResult) || !FloorResult.IsWalkableFloor())
	{
		return;
	}

	// Super targets the start-of-step velocity (ChaosCharacterWalkingExecutor.cpp:145) but moves the position target by the proposed move (:164) under this gate (:150).
	const float DeltaSeconds = Params.TimeStep.StepMs * 0.001f;
	const FVector StartVelocity = StartingSyncState->GetVelocity_WorldSpace();
	const FVector GravityProjectedVelocity = StartVelocity + DefaultSimInputs->Gravity * DeltaSeconds - DefaultSimInputs->PhysicsObjectGravity * FVector::UpVector * DeltaSeconds;
	constexpr float ParallelCosThreshold = 0.999f;
	if (!Params.ProposedMove.bHasDirIntent && FVector::Parallel(GravityProjectedVelocity.GetSafeNormal(), DefaultSimInputs->UpDir, ParallelCosThreshold))
	{
		return;
	}

	UE::ChaosMover::FGroundDynamicsInfo GroundDynamicsInfo;
	SimBlackboard->TryGet(UE::ChaosMover::Blackboard::GroundDynamicsInfo, GroundDynamicsInfo);

	const FVector GroundNormal = FloorResult.HitResult.ImpactNormal;
	const FVector ProposedPlaneVelocity = FVector::VectorPlaneProject(Params.ProposedMove.LinearVelocity + GroundDynamicsInfo.LinearVelocity, GroundNormal);
	const FVector StartPlaneVelocity = FVector::VectorPlaneProject(StartVelocity, GroundNormal);

	OutputSyncState->SetTransforms_WorldSpace(
		OutputSyncState->GetLocation_WorldSpace(),
		OutputSyncState->GetOrientation_WorldSpace(),
		OutputSyncState->GetVelocity_WorldSpace() + ProposedPlaneVelocity - StartPlaneVelocity,
		OutputSyncState->GetAngularVelocityDegrees_WorldSpace());
}
