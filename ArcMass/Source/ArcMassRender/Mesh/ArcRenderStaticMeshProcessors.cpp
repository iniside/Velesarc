// Copyright Lukasz Baran. All Rights Reserved.

#include "Mesh/ArcRenderStaticMeshProcessors.h"

#include "AI/MassEngineNavigationProcessors.h"
#include "ArcDestroyRenderStateContext.h"
#include "MassEngineTypes.h"
#include "Mass/EntityFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"
#include "ArcISMRenderStateHelper.h"
#include "ArcRenderStateHelper.h"
#include "MassSignalSubsystem.h"
#include "ArcStaticMeshRenderStateHelper.h"
#include "Mesh/ArcRenderMeshFragments.h"
#include "StaticMeshComponentHelper.h"
#include "StaticMeshSceneProxyDesc.h"
#include "Mesh/ArcRenderMeshUtils.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRenderStaticMeshProcessors)

//----------------------------------------------------------------------//
// UArcBaseStaticMeshSetupRenderStateProcessor
//----------------------------------------------------------------------//
UArcBaseStaticMeshSetupRenderStateProcessor::UArcBaseStaticMeshSetupRenderStateProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EntityQuery(*this)
{
	bRequiresGameThreadExecution = true; // @todo revisit if the execution is required on game thread
	// DuringPhysics: render-proxy setup is GT-pinned but physics-independent (Scene proxy
	// work, no Chaos reads), so it overlaps the async solve rather than burdening PrePhysics.
	// Covers UArcStaticMeshSetupRenderStateProcessor and the ISM-instantiate processor too
	// (both inherit this ctor). The SetupRenderState group reconstitutes in DuringPhysics
	// because every member's ProcessingPhase moves with it.
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SetupRenderState;
}

void UArcBaseStaticMeshSetupRenderStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcStaticMeshFragment>();
	EntityQuery.AddConstSharedRequirement<FArcVisualizationMeshFragment>();
	EntityQuery.AddConstSharedRequirement<FArcOverrideMaterialsFragment>(EMassFragmentPresence::Optional);
#if WITH_EDITOR
	EntityQuery.AddConstSharedRequirement<FArcEditorVisualizationMeshFragment>(EMassFragmentPresence::Optional);
#endif // WITH_EDITOR

	EntityQuery.AddRequirement<FArcRenderPrimitiveFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);
}

//----------------------------------------------------------------------//
// UArcStaticMeshSetupRenderStateProcessor
//----------------------------------------------------------------------//
UArcStaticMeshSetupRenderStateProcessor::UArcStaticMeshSetupRenderStateProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UArcStaticMeshSetupRenderStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);
	EntityQuery.AddRequirement<FArcRenderStaticMeshFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcStaticMeshSetupRenderStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext)
{
	FRegisterComponentContext BatchingContext(nullptr);

	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[&EntityManager, &BatchingContext](FMassExecutionContext& Context)
		{
			const FArcStaticMeshFragment& StaticMeshFragment = Context.GetConstSharedFragment<FArcStaticMeshFragment>();
			const FArcVisualizationMeshFragment& MeshFragment = Context.GetConstSharedFragment<FArcVisualizationMeshFragment>();
			const FArcOverrideMaterialsFragment* OverrideMaterialsFragment = Context.GetConstSharedFragmentPtr<FArcOverrideMaterialsFragment>();
#if WITH_EDITOR
			const FArcEditorVisualizationMeshFragment* EditorMeshFragment = Context.GetConstSharedFragmentPtr<FArcEditorVisualizationMeshFragment>();
#endif // WITH_EDITOR
			const TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();

			const TArrayView<FArcRenderPrimitiveFragment> RenderPrimitiveFragments = Context.GetMutableFragmentView<FArcRenderPrimitiveFragment>();
			const TArrayView<FArcRenderStaticMeshFragment> RenderStaticMeshFragments = Context.GetMutableFragmentView<FArcRenderStaticMeshFragment>();
			const TArrayView<FArcRenderStateFragment> RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				// Skip entities whose render state helper was already created (fragments are present-from-build;
				// this processor runs every frame so we gate on helper validity rather than fragment absence).
				if (RenderStateFragments[*EntityIt].HasRenderStateHelper())
				{
					continue;
				}

				FArcRenderPrimitiveFragment& RenderPrimitiveFrag = RenderPrimitiveFragments[*EntityIt];
				RenderPrimitiveFrag.bIsVisible = true;
				RenderPrimitiveFrag.LocalBounds = StaticMeshFragment.Mesh.Get()->GetBounds();
				RenderPrimitiveFrag.WorldBounds = RenderPrimitiveFrag.LocalBounds.TransformBy(TransformFragments[*EntityIt].GetTransform());

				FArcRenderStaticMeshFragment& RenderStaticMeshFrag = RenderStaticMeshFragments[*EntityIt];
				checkf(RenderStaticMeshFrag.StaticMeshSceneProxyDesc, TEXT("Expecting a valid prxy desc here"));

				UE::ArcMassRender::Mesh::InitializeStaticMeshSceneProxyDescFromFragment(StaticMeshFragment, MeshFragment, TransformFragments[*EntityIt], *RenderStaticMeshFrag.StaticMeshSceneProxyDesc);
#if WITH_EDITOR
				if (EditorMeshFragment)
				{
					UE::ArcMassRender::Mesh::InitializePrimitiveSceneProxyDescFromEditorFragment(*EditorMeshFragment, *RenderStaticMeshFrag.StaticMeshSceneProxyDesc);
				}
#endif // WITH_EDITOR

				FArcRenderStateFragment& RenderStateFrag = RenderStateFragments[*EntityIt];
				FArcStaticMeshRenderStateHelper* Helper = RenderStateFrag.CreateRenderStateHelper<FArcStaticMeshRenderStateHelper>(Context.GetEntity(*EntityIt), &EntityManager, RenderPrimitiveFrag, OverrideMaterialsFragment, RenderStaticMeshFrag);

				// Fold in create-render-state (cannot rely on UArcCreateRenderStateObserver: it fires on
				// FArcRenderStateFragment::Add with a null helper and asserts non-null on GetRenderStateHelper).
				if (Helper->ShouldCreateRenderState())
				{
					if (!BatchingContext.IsInitialized())
					{
						BatchingContext.Initialize(Helper->GetWorld());
					}
					Helper->CreateRenderState(&BatchingContext);
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

//----------------------------------------------------------------------//
// UArcStaticMeshUpdateRenderStateProcessor
//----------------------------------------------------------------------//
UArcStaticMeshUpdateRenderStateProcessor::UArcStaticMeshUpdateRenderStateProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bRequiresGameThreadExecution = true; // @todo revisit if the execution is required on game thread
	// DuringPhysics — see UArcBaseStaticMeshSetupRenderStateProcessor; stays grouped with SetupRenderState.
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::SetupRenderState;
	// DISABLED — currently a no-op. Signal processor over FArcRenderStaticMeshFragment, which has zero
	// producers (the live ISM-holder path uses FArcRenderISMFragment instead), so it matches no entity.
	bAutoRegisterWithProcessingPhases = false;
}

void UArcStaticMeshUpdateRenderStateProcessor::InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager)
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
		}
	}
}

void UArcStaticMeshUpdateRenderStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddConstSharedRequirement<FArcStaticMeshFragment>();
	EntityQuery.AddConstSharedRequirement<FArcVisualizationMeshFragment>();
#if WITH_EDITOR
	EntityQuery.AddConstSharedRequirement<FArcEditorVisualizationMeshFragment>(EMassFragmentPresence::Optional);
#endif // WITH_EDITOR
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddRequirement<FArcRenderPrimitiveFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcRenderStaticMeshFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcStaticMeshUpdateRenderStateProcessor::SignalEntities(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& ExecutionContext,
	FMassSignalNameLookup& EntitySignals)
{
	const UObject* EntityManagerOwner = ExecutionContext.GetEntityManagerChecked().GetOwner();
	check(EntityManagerOwner);

	TArray<FName> SignalsForEntity;
	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[&EntitySignals, &SignalsForEntity](FMassExecutionContext& Context)
		{
			const FArcStaticMeshFragment& StaticMeshFragment = Context.GetConstSharedFragment<FArcStaticMeshFragment>();
			const FArcVisualizationMeshFragment& MeshFragment = Context.GetConstSharedFragment<FArcVisualizationMeshFragment>();
#if WITH_EDITOR
			const FArcEditorVisualizationMeshFragment* EditorMeshFragment = Context.GetConstSharedFragmentPtr<FArcEditorVisualizationMeshFragment>();
#endif // WITH_EDITOR
			const TConstArrayView<FTransformFragment> TransformFragments = Context.GetFragmentView<FTransformFragment>();

			const TArrayView<FArcRenderPrimitiveFragment>& RenderPrimitiveFragments = Context.GetMutableFragmentView<FArcRenderPrimitiveFragment>();
			const TArrayView<FArcRenderStaticMeshFragment>& RenderStaticMeshFragments = Context.GetMutableFragmentView<FArcRenderStaticMeshFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				SignalsForEntity.Reset();
				EntitySignals.GetSignalsForEntity(Context.GetEntity(*EntityIt), SignalsForEntity);

				const bool bTransformChanged = SignalsForEntity.Contains(UE::Mass::Signals::TransformChanged);
				const bool bVisualPropertyChanged = SignalsForEntity.Contains(UE::Mass::Signals::MeshVisualPropertyChanged);
				const bool bMeshChanged = SignalsForEntity.Contains(UE::Mass::Signals::MeshChanged);
#if WITH_EDITOR
				const bool bSelectionChanged = SignalsForEntity.Contains(UE::Mass::Signals::SelectionChanged);
				const bool bLevelEditingStateChanged = SignalsForEntity.Contains(UE::Mass::Signals::LevelEditingStateChanged);
#endif // WITH_EDITOR

				if (bTransformChanged)
				{
					RenderPrimitiveFragments[*EntityIt].bIsVisible = true;
					RenderPrimitiveFragments[*EntityIt].LocalBounds = StaticMeshFragment.Mesh.Get()->GetBounds();
					RenderPrimitiveFragments[*EntityIt].WorldBounds = RenderPrimitiveFragments[*EntityIt].LocalBounds.TransformBy(TransformFragments[*EntityIt].GetTransform());
				}

				checkf(RenderStaticMeshFragments[*EntityIt].StaticMeshSceneProxyDesc, TEXT("Expecting a valid prxy desc here"));
				if (bMeshChanged || bVisualPropertyChanged)
				{
					// Full re-init: rewrites StaticMesh ptr, Nanite resources, and all shadow/lighting/visibility flags.
					UE::ArcMassRender::Mesh::InitializeStaticMeshSceneProxyDescFromFragment(StaticMeshFragment, MeshFragment, TransformFragments[*EntityIt], *RenderStaticMeshFragments[*EntityIt].StaticMeshSceneProxyDesc);
				}
				else if (bTransformChanged)
				{
					// Transform-only: flags come from the const-shared MeshFragment and are already written;
					// only update the transform-dependent field(s) to avoid redundant work.
					UE::ArcMassRender::Mesh::UpdateStaticMeshSceneProxyDescTransform(TransformFragments[*EntityIt], *RenderStaticMeshFragments[*EntityIt].StaticMeshSceneProxyDesc);
				}
#if WITH_EDITOR
				if (EditorMeshFragment && (bVisualPropertyChanged || bSelectionChanged || bLevelEditingStateChanged) )
				{
					UE::ArcMassRender::Mesh::InitializePrimitiveSceneProxyDescFromEditorFragment(*EditorMeshFragment, *RenderStaticMeshFragments[*EntityIt].StaticMeshSceneProxyDesc);
				}
#endif // WITH_EDITOR
			}

		});
}

//----------------------------------------------------------------------//
// UArcBaseStaticMeshDestroyRenderStateProcessor
//----------------------------------------------------------------------//
UArcBaseStaticMeshDestroyRenderStateProcessor::UArcBaseStaticMeshDestroyRenderStateProcessor()
	: EntityQuery(*this)
{
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	// Observe FArcRenderStaticMeshFragment::Remove only — this is the ISM-safe discriminator.
	// Only non-ISM one-off entities carry FArcRenderStaticMeshFragment (ISM holders carry
	// FArcRenderISMFragment), so this Remove observer is structurally ISM-safe with no query gate.
	ObservedTypes.Add(FArcRenderStaticMeshFragment::StaticStruct());
	ObservedOperations = EMassObservedOperationFlags::Remove;
}

void UArcBaseStaticMeshDestroyRenderStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	// @Todo, this would prevent the observer to be called more than once when the fragments are removed in multiple calls,
	// but we need to sort out the Observer manager LockCount batcher before. Otherwise we get called after the fragments are
	// removed and the query will never run.
	//EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	//EntityQuery.AddConstSharedRequirement<FArcStaticMeshFragment>(EMassFragmentPresence::All);
	//EntityQuery.AddConstSharedRequirement<FArcVisualizationMeshFragment>(EMassFragmentPresence::All);
}

//----------------------------------------------------------------------//
// UArcStaticMeshDestroyRenderStateProcessor
//----------------------------------------------------------------------//
UArcStaticMeshDestroyRenderStateProcessor::UArcStaticMeshDestroyRenderStateProcessor()
{
}

void UArcStaticMeshDestroyRenderStateProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	Super::ConfigureQueries(EntityManager);

	// Require FArcRenderStateFragment RW so we can read the helper and call teardown.
	// Do NOT gate on FArcRenderPrimitiveFragment or FArcRenderStaticMeshFragment here — the in-code
	// comment on the base ConfigureQueries explains that gated Remove-observer queries can "never run"
	// when the fragments are removed in the same operation as the observed type.
	EntityQuery.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcStaticMeshDestroyRenderStateProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext)
{
	FArcDestroyRenderStateContext BatchingContext(nullptr);

	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[&BatchingContext](FMassExecutionContext& Context)
		{
			const TArrayView<FArcRenderStateFragment> RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				// An entity spawned and destroyed before the Setup processor's DuringPhysics pass never
				// got its helper created; GetRenderStateHelper asserts non-null, so skip those.
				if (!RenderStateFragments[*EntityIt].HasRenderStateHelper())
				{
					continue;
				}

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
