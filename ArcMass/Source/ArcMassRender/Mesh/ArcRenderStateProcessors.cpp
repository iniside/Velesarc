// Copyright Lukasz Baran. All Rights Reserved.

#include "Mesh/ArcRenderStateProcessors.h"

#include "AI/MassEngineNavigationProcessors.h"
#include "ArcDestroyRenderStateContext.h"
#include "MassEngineTypes.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "ArcISMRenderStateHelper.h"
#include "ArcRenderStateHelper.h"
#include "MassSignalSubsystem.h"
#include "ArcStaticMeshRenderStateHelper.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "StaticMeshComponentHelper.h"
#include "StaticMeshSceneProxyDesc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRenderStateProcessors)

// ----------------------------------------------------------------------//
// UArcRenderStateDirtyUpdateProcessor
//----------------------------------------------------------------------//
UArcRenderStateDirtyUpdateProcessor::UArcRenderStateDirtyUpdateProcessor(const FObjectInitializer & ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRequiresGameThreadExecution = true; // @todo revisit if the execution is required on game thread
	// DuringPhysics — transform/selection updates are GT render enqueues; overlaps async Chaos solve.
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::CreateRenderState;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::SetupRenderState);
	// DISABLED — the non-ISM one-off path is served by the repurposed
	// UArcStaticMeshDestroyRenderStateProcessor (Step-4) and the create-render-state fold-in inside
	// UArcStaticMeshSetupRenderStateProcessor (Step-3). This generic FArcRenderStateFragment signal
	// processor cannot discriminate ISM holders from non-ISM one-offs, so re-enabling it would
	// double-update holder helpers that ArcMassEntityVisualization already drives directly.
	bAutoRegisterWithProcessingPhases = false;
}

void UArcRenderStateDirtyUpdateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::InitializeInternal(Owner, EntityManager);

	if (const UWorld* World = GetWorld())
	{
		if (UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>())
		{
			SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::TransformChanged);
			SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::MeshVisualPropertyChanged);
			SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::MeshChanged);
#if WITH_EDITOR
			SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::SelectionChanged);
			SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::LevelEditingStateChanged);
#endif // WITH_EDITOR
			SubscribeToSignal(*SignalSubsystem, UE::Mass::Signals::RenderStateDirty);
		}
	}
}

void UArcRenderStateDirtyUpdateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcRenderStateDirtyUpdateProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& ExecutionContext,
	FMassSignalNameLookup& EntitySignals)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcRenderStateDirtyUpdate);

	const UObject* EntityManagerOwner = ExecutionContext.GetEntityManagerChecked().GetOwner();
	check(EntityManagerOwner);

	FRegisterComponentContext BatchingContext(nullptr);

	TArray<FName> SignalsForEntity;
	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[&EntitySignals, &SignalsForEntity, &BatchingContext](FMassExecutionContext& Context)
		{
			const TArrayView<FArcRenderStateFragment>& RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcPrimitiveRenderStateHelper& Helper = RenderStateFragments[*EntityIt].GetRenderStateHelper<FArcPrimitiveRenderStateHelper>();
				SignalsForEntity.Reset();
				EntitySignals.GetSignalsForEntity(Context.GetEntity(*EntityIt), SignalsForEntity);

				bool bHandled = false;
				if (SignalsForEntity.Num() == 1)
				{
					if (SignalsForEntity[0] == UE::Mass::Signals::TransformChanged)
					{
						Helper.UpdateTransform();
						bHandled = true;
					}
#if WITH_EDITOR
					else if (SignalsForEntity[0] == UE::Mass::Signals::SelectionChanged)
					{
						Helper.UpdateSelection();
						bHandled = true;
					}
					else if (SignalsForEntity[0] == UE::Mass::Signals::LevelEditingStateChanged)
					{
						Helper.UpdateLevelInstanceEditingState();
						bHandled = true;
					}
#endif // WITH_EDITOR
				}

				if (!bHandled)
				{
					Helper.DestroyRenderState();
					if (Helper.ShouldCreateRenderState())
					{
						if (!BatchingContext.IsInitialized())
						{
							BatchingContext.Initialize(Helper.GetWorld());
						}
						Helper.CreateRenderState(&BatchingContext);
					}
				}
			}
		});

	if (BatchingContext.IsInitialized())
	{
		BatchingContext.Process();
#if WITH_EDITOR
		// Make sure any new created hit proxies are taken into account by the ProxyMap
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(/*bInvalidateProxies*/true);
		}
#endif // WITH_EDITOR
	}
}

// ----------------------------------------------------------------------//
// UArcCreateRenderStateObserver
//----------------------------------------------------------------------//
UArcCreateRenderStateObserver::UArcCreateRenderStateObserver()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	ObservedTypes.Add(FArcRenderStateFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
	// DISABLED — cannot be used for non-ISM one-offs even though they now carry FArcRenderStateFragment
	// from build time. This observer fires on FArcRenderStateFragment::Add; at that moment the helper
	// is still null (CreateRenderStateHelper has not run yet), so GetRenderStateHelper<>() would assert
	// non-null → crash. Create-render-state is folded directly into UArcStaticMeshSetupRenderStateProcessor
	// instead. Keep this observer out of the registry.
	bAutoRegisterWithObserverRegistry = false;
}

void UArcCreateRenderStateObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcRenderStaticMeshFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::All);
}

void UArcCreateRenderStateObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcCreateRenderStateObserver);

	FRegisterComponentContext BatchingContext(nullptr);

	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[&BatchingContext](FMassExecutionContext& Context)
		{
			const TArrayView<FArcRenderStateFragment>& RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcPrimitiveRenderStateHelper& Helper = RenderStateFragments[*EntityIt].GetRenderStateHelper<FArcPrimitiveRenderStateHelper>();
				if (!Helper.IsRenderStateCreated() && Helper.ShouldCreateRenderState())
				{
					if (!BatchingContext.IsInitialized())
					{
						BatchingContext.Initialize(Helper.GetWorld());
					}
					Helper.CreateRenderState(&BatchingContext);
				}
			}
		});

	if (BatchingContext.IsInitialized())
	{
		BatchingContext.Process();
#if WITH_EDITOR
		// Make sure any new created hit proxies are taken into account by the ProxyMap
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(/*bInvalidateProxies*/true);
		}
#endif // WITH_EDITOR
	}
}

// ----------------------------------------------------------------------//
// UArcDestroyRenderStateProcessor
//----------------------------------------------------------------------//
UArcDestroyRenderStateProcessor::UArcDestroyRenderStateProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	ObservedTypes.Add(FArcRenderStateFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true; // @todo revisit if the execution is required on game thread
	// DISABLED — observes FArcRenderStateFragment::Remove, which cannot discriminate ISM holders from
	// non-ISM one-offs (both carry FArcRenderStateFragment). Re-enabling it would collide with
	// ArcMassEntityVisualization's own holder teardown. Non-ISM teardown is handled by the repurposed
	// UArcStaticMeshDestroyRenderStateProcessor, which observes FArcRenderStaticMeshFragment::Remove —
	// a fragment that only non-ISM one-offs carry. Keep this generic observer out of the registry.
	bAutoRegisterWithObserverRegistry = false;
}

void UArcDestroyRenderStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcDestroyRenderStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcDestroyRenderState);

	FArcDestroyRenderStateContext BatchingContext(nullptr);

	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[&BatchingContext](FMassExecutionContext& Context)
		{
			const TArrayView<FArcRenderStateFragment>& RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcPrimitiveRenderStateHelper& Helper = RenderStateFragments[*EntityIt].GetRenderStateHelper<FArcPrimitiveRenderStateHelper>();
				if (!BatchingContext.IsInitialized())
				{
					BatchingContext.Initialize(Helper.GetScene());
				}
				Helper.DestroyRenderState(&BatchingContext);
				RenderStateFragments[*EntityIt].DestroyRenderStateHelper();
			}
		});

	if (BatchingContext.IsInitialized())
	{
		BatchingContext.Process();
	}
}
