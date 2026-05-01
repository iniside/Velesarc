// Copyright Lukasz Baran. All Rights Reserved.

#include "Mass/ArcEconomyObservers.h"
#include "Mass/ArcEconomyFragments.h"
#include "ArcSettlementSubsystem.h"
#include "ArcKnowledgeSubsystem.h"
#include "Mass/EntityFragments.h"
#include "MassExecutionContext.h"

// ============================================================================
// Building Add Observer
// ============================================================================

UArcBuildingAddObserver::UArcBuildingAddObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcBuildingFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcBuildingAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	
	ObserverQuery.AddSubsystemRequirement<UArcSettlementSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcBuildingAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementSubsystem* SettlementSubsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!SettlementSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcBuildingAdd);

	ObserverQuery.ForEachEntityChunk(Context, [&EntityManager, SettlementSubsystem](FMassExecutionContext& Ctx)
	{
		const TArrayView<FArcBuildingFragment> Buildings = Ctx.GetMutableFragmentView<FArcBuildingFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(Index);
			Buildings[Index].BuildingLocation = Transforms[Index].GetTransform().GetLocation();
			const FMassEntityHandle SettlementHandle = SettlementSubsystem->FindNearestSettlement(
				EntityManager, Buildings[Index].BuildingLocation);

			if (SettlementHandle.IsValid())
			{
				Buildings[Index].SettlementHandle = SettlementHandle;
				SettlementSubsystem->RegisterBuilding(SettlementHandle, Entity);
			}
			// else: orphan — UArcSettlementDiscoveryProcessor will pick it up
		}
	});
}

// ============================================================================
// Building Remove Observer
// ============================================================================

UArcBuildingRemoveObserver::UArcBuildingRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcBuildingFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcBuildingRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcBuildingFragment>(EMassFragmentAccess::ReadOnly);
	
	ObserverQuery.AddSubsystemRequirement<UArcSettlementSubsystem>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddSubsystemRequirement<UArcKnowledgeSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcBuildingRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementSubsystem* SettlementSubsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	UArcKnowledgeSubsystem* KnowledgeSubsystem = Context.GetWorld()->GetSubsystem<UArcKnowledgeSubsystem>();

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcBuildingRemove);

	ObserverQuery.ForEachEntityChunk(Context, [SettlementSubsystem, KnowledgeSubsystem](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcBuildingFragment> Buildings = Ctx.GetFragmentView<FArcBuildingFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(Index);
			if (SettlementSubsystem)
			{
				SettlementSubsystem->UnregisterBuilding(Buildings[Index].SettlementHandle, Entity);
			}
			if (KnowledgeSubsystem)
			{
				KnowledgeSubsystem->RemoveKnowledgeBySource(Entity);
			}
		}
	});
}

// ============================================================================
// NPC Add Observer
// ============================================================================

UArcNPCAddObserver::UArcNPCAddObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcEconomyNPCFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcNPCAddObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcEconomyNPCFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	
	ObserverQuery.AddSubsystemRequirement<UArcSettlementSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcNPCAddObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementSubsystem* SettlementSubsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!SettlementSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNPCAdd);

	ObserverQuery.ForEachEntityChunk(Context, [&EntityManager, SettlementSubsystem](FMassExecutionContext& Ctx)
	{
		const TArrayView<FArcEconomyNPCFragment> NPCs = Ctx.GetMutableFragmentView<FArcEconomyNPCFragment>();
		const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			const FMassEntityHandle Entity = Ctx.GetEntity(Index);
			const FVector NPCLocation = Transforms[Index].GetTransform().GetLocation();
			const FMassEntityHandle SettlementHandle = SettlementSubsystem->FindNearestSettlement(
				EntityManager, NPCLocation);

			if (SettlementHandle.IsValid())
			{
				NPCs[Index].SettlementHandle = SettlementHandle;
				SettlementSubsystem->RegisterNPC(SettlementHandle, Entity);
			}
			// else: orphan — UArcSettlementDiscoveryProcessor will pick it up
		}
	});
}

// ============================================================================
// NPC Remove Observer
// ============================================================================

UArcNPCRemoveObserver::UArcNPCRemoveObserver()
	: ObserverQuery{*this}
{
	ObservedTypes.Add(FArcEconomyNPCFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcNPCRemoveObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcEconomyNPCFragment>(EMassFragmentAccess::ReadOnly);
	
	ObserverQuery.AddSubsystemRequirement<UArcSettlementSubsystem>(EMassFragmentAccess::ReadWrite);
}

void UArcNPCRemoveObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UArcSettlementSubsystem* SettlementSubsystem = Context.GetWorld()->GetSubsystem<UArcSettlementSubsystem>();
	if (!SettlementSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcNPCRemove);

	ObserverQuery.ForEachEntityChunk(Context, [SettlementSubsystem](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcEconomyNPCFragment> NPCs = Ctx.GetFragmentView<FArcEconomyNPCFragment>();
		const int32 NumEntities = Ctx.GetNumEntities();
		for (int32 Index = 0; Index < NumEntities; ++Index)
		{
			SettlementSubsystem->UnregisterNPC(NPCs[Index].SettlementHandle, Ctx.GetEntity(Index));
		}
	});
}
