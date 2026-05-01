// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcSettlementDiscoveryProcessor.h"
#include "Mass/ArcEconomyFragments.h"
#include "Mass/EntityFragments.h"
#include "ArcSettlementSubsystem.h"
#include "MassExecutionContext.h"

UArcSettlementDiscoveryProcessor::UArcSettlementDiscoveryProcessor()
	: OrphanBuildingQuery{*this}
	, OrphanNPCQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Server | EProcessorExecutionFlags::Standalone);
	ExecutionOrder.ExecuteBefore.Add(TEXT("UArcGovernorProcessor"));
}

void UArcSettlementDiscoveryProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	OrphanBuildingQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadWrite);

	OrphanNPCQuery.AddRequirement<FArcEconomyNPCFragment>(EMassFragmentAccess::ReadWrite);
	OrphanNPCQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcSettlementDiscoveryProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TimeSinceLastScan += Context.GetDeltaTimeSeconds();
	if (TimeSinceLastScan < ScanInterval)
	{
		return;
	}
	TimeSinceLastScan = 0.0f;

	UArcSettlementSubsystem* SettlementSubsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!SettlementSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcSettlementDiscovery);

	// Scan orphan buildings
	OrphanBuildingQuery.ForEachEntityChunk(Context, [&EntityManager, SettlementSubsystem](FMassExecutionContext& Ctx)
	{
		const TArrayView<FArcBuildingFragment> Buildings = Ctx.GetMutableFragmentView<FArcBuildingFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			if (Buildings[Index].SettlementHandle.IsValid())
			{
				continue;
			}

			const FMassEntityHandle SettlementHandle = SettlementSubsystem->FindNearestSettlement(
				EntityManager, Buildings[Index].BuildingLocation);

			if (SettlementHandle.IsValid())
			{
				Buildings[Index].SettlementHandle = SettlementHandle;
				SettlementSubsystem->RegisterBuilding(SettlementHandle, Ctx.GetEntity(Index));
			}
		}
	});

	// Scan orphan NPCs
	OrphanNPCQuery.ForEachEntityChunk(Context, [&EntityManager, SettlementSubsystem](FMassExecutionContext& Ctx)
	{
		const TArrayView<FArcEconomyNPCFragment> NPCs = Ctx.GetMutableFragmentView<FArcEconomyNPCFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			if (NPCs[Index].SettlementHandle.IsValid())
			{
				continue;
			}

			const FVector NPCLocation = Transforms[Index].GetTransform().GetLocation();
			const FMassEntityHandle SettlementHandle = SettlementSubsystem->FindNearestSettlement(
				EntityManager, NPCLocation);

			if (SettlementHandle.IsValid())
			{
				NPCs[Index].SettlementHandle = SettlementHandle;
				SettlementSubsystem->RegisterNPC(SettlementHandle, Ctx.GetEntity(Index));
			}
		}
	});
}
