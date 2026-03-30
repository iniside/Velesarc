// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassStateTreeTickProcessor.h"

#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeFragments.h"
#include "Tasks/ArcMassDrawDebugSphereTask.h"

UArcMassStateTreeTickProcessor::UArcMassStateTreeTickProcessor()
	: EntityQuery_Conditional(*this)
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Tasks;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::Avoidance);
	
	QueryBasedPruning = EMassQueryBasedPruning::Never;
}

void UArcMassStateTreeTickProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery_Conditional.AddRequirement<FMassStateTreeInstanceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.AddConstSharedRequirement<FMassStateTreeSharedFragment>();
	
	EntityQuery_Conditional.AddSparseRequirement<FArcMassTickStateTreeTag>(EMassFragmentPresence::All);
	EntityQuery_Conditional.AddSubsystemRequirement<UMassStateTreeSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery_Conditional.RegisterWithProcessor(*this);
}

void UArcMassStateTreeTickProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& MassEntityManager)
{
	Super::InitializeInternal(Owner, MassEntityManager);
	
	MassStateTreeSubsystem = UWorld::GetSubsystem<UMassStateTreeSubsystem>(Owner.GetWorld());
}

void UArcMassStateTreeTickProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	if (!MassStateTreeSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMassStateTreeTick);

	EntityQuery_Conditional.ForEachEntityChunk(Context, [this](FMassExecutionContext& MyContext)
	{
		const TArrayView<FMassStateTreeInstanceFragment> StateTreeInstanceList = MyContext.GetMutableFragmentView<FMassStateTreeInstanceFragment>();
		const FMassStateTreeSharedFragment& SharedStateTree = MyContext.GetConstSharedFragment<FMassStateTreeSharedFragment>();
			
		const UStateTree* StateTree = SharedStateTree.StateTree;
			
		const float WorldDeltaTime = MyContext.GetDeltaTimeSeconds();
			
		for (FMassExecutionContext::FEntityIterator EntityIt = MyContext.CreateSparseEntityIterator(); EntityIt; ++EntityIt)
		{
			const FMassEntityHandle Entity = MyContext.GetEntity(EntityIt);
			
			FMassStateTreeInstanceFragment& StateTreeFragment = StateTreeInstanceList[EntityIt];
			FStateTreeInstanceData* InstanceData = MassStateTreeSubsystem->GetInstanceData(StateTreeFragment.InstanceHandle);
			
			FMassStateTreeExecutionContext StateTreeContext(*MassStateTreeSubsystem, *StateTree, *InstanceData, MyContext);
			StateTreeContext.SetEntity(Entity);
			StateTreeContext.SetOuterTraceId(Entity.AsNumber());
			
			StateTreeContext.Tick(WorldDeltaTime);
		}
	});
}
