// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcNiagaraVisProcessors.h"

#include "ArcNiagaraVisFragments.h"
#include "ArcNiagaraRenderStateHelper.h"
#include "MassEntityFragments.h"
#include "MassExecutionContext.h"
#include "NiagaraSystem.h"

// ---------------------------------------------------------------------------
// UArcNiagaraVisInitObserver
// ---------------------------------------------------------------------------

UArcNiagaraVisInitObserver::UArcNiagaraVisInitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcNiagaraVisTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Add;
	bRequiresGameThreadExecution = true;
}

void UArcNiagaraVisInitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcNiagaraVisFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	ObserverQuery.AddConstSharedRequirement<FArcNiagaraVisConfigFragment>(EMassFragmentPresence::All);
	ObserverQuery.AddTagRequirement<FArcNiagaraVisTag>(EMassFragmentPresence::All);
}

void UArcNiagaraVisInitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	if (!World)
	{
		return;
	}

	ObserverQuery.ForEachEntityChunk(Context,
		[&EntityManager, World](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcNiagaraVisFragment> VisFragments = Ctx.GetMutableFragmentView<FArcNiagaraVisFragment>();
			const FArcNiagaraVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcNiagaraVisConfigFragment>();

			// Resolve soft pointer — must be loaded
			UNiagaraSystem* System = Config.NiagaraSystem.LoadSynchronous();
			if (!System)
			{
				UE_LOG(LogMass, Warning, TEXT("ArcNiagaraVisInitObserver: NiagaraSystem is null or failed to load."));
				return;
			}

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcNiagaraVisFragment& VisFragment = VisFragments[EntityIt];

				// Skip if already initialized (shouldn't happen, but defensive)
				if (VisFragment.RenderStateHelper)
				{
					continue;
				}

				const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

				// Create helper, initialize Niagara system, create render state
				auto* Helper = new FArcNiagaraRenderStateHelper(Entity, &EntityManager);
				Helper->Initialize(*World, *System, Config.bCastShadow);
				Helper->CreateNiagaraRenderState();

				VisFragment.RenderStateHelper = Helper;
			}
		});
}

// ---------------------------------------------------------------------------
// UArcNiagaraVisDeinitObserver
// ---------------------------------------------------------------------------

UArcNiagaraVisDeinitObserver::UArcNiagaraVisDeinitObserver()
	: ObserverQuery{*this}
{
	ObservedType = FArcNiagaraVisTag::StaticStruct();
	ObservedOperations = EMassObservedOperationFlags::Remove;
	bRequiresGameThreadExecution = true;
}

void UArcNiagaraVisDeinitObserver::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	ObserverQuery.AddRequirement<FArcNiagaraVisFragment>(EMassFragmentAccess::ReadWrite);
	ObserverQuery.AddTagRequirement<FArcNiagaraVisTag>(EMassFragmentPresence::All);
}

void UArcNiagaraVisDeinitObserver::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	ObserverQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcNiagaraVisFragment> VisFragments = Ctx.GetMutableFragmentView<FArcNiagaraVisFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcNiagaraVisFragment& VisFragment = VisFragments[EntityIt];

				if (VisFragment.RenderStateHelper)
				{
					// Orphan instead of destroying — lets existing particles fade out.
					// TickOrphanedSystems() in the dynamic data processor handles cleanup.
					VisFragment.RenderStateHelper->Orphan();
					VisFragment.RenderStateHelper = nullptr;
				}
			}
		});
}

// ---------------------------------------------------------------------------
// UArcNiagaraVisDynamicDataProcessor
// ---------------------------------------------------------------------------

UArcNiagaraVisDynamicDataProcessor::UArcNiagaraVisDynamicDataProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcNiagaraVisDynamicDataProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcNiagaraVisFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcNiagaraVisParamsFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddTagRequirement<FArcNiagaraVisTag>(EMassFragmentPresence::All);
}

void UArcNiagaraVisDynamicDataProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcNiagaraVisFragment> VisFragments = Ctx.GetFragmentView<FArcNiagaraVisFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TArrayView<FArcNiagaraVisParamsFragment> ParamsFragments = Ctx.GetMutableFragmentView<FArcNiagaraVisParamsFragment>();
			const bool bHasParams = ParamsFragments.Num() > 0;

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcNiagaraVisFragment& VisFragment = VisFragments[EntityIt];
				const FTransformFragment& Transform = Transforms[EntityIt];
				FArcNiagaraRenderStateHelper* Helper = VisFragment.RenderStateHelper;

				if (!Helper || !Helper->HasSceneProxy())
				{
					continue;
				}

				// Apply parameter overrides before ticking — values take effect on ManualTick
				if (bHasParams)
				{
					FArcNiagaraVisParamsFragment& Params = ParamsFragments[EntityIt];
					if (Params.bDirty)
					{
						Helper->ApplyParameterOverrides(Params);
						Params.bDirty = false;
					}
				}

				Helper->UpdateTransform(Transform.GetTransform());
				Helper->SendDynamicRenderData(Transform.GetTransform());
			}
		});

	// Tick orphaned systems — entities are gone but particles are still fading out.
	// Must be called every frame so ManualTick keeps running on the orphaned instances.
	if (UWorld* World = EntityManager.GetWorld())
	{
		FArcNiagaraRenderStateHelper::TickOrphanedSystems(*World);
	}
}
