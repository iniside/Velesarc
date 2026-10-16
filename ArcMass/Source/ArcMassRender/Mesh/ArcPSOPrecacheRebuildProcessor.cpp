// Copyright Lukasz Baran. All Rights Reserved.

#include "Mesh/ArcPSOPrecacheRebuildProcessor.h"

#include "ArcPrimitiveRenderStateHelper.h"
#include "ArcRenderStateHelper.h"
#include "Mass/ExecutionContext.h"
#include "MassEngineTypes.h"
#include "PSOPrecacheFwd.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcPSOPrecacheRebuildProcessor)

UArcPSOPrecacheRebuildProcessor::UArcPSOPrecacheRebuildProcessor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, EntityQuery(*this)
{
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllWorldModes);
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::CreateRenderState;
}

void UArcPSOPrecacheRebuildProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcRenderStateFragment>(EMassFragmentAccess::ReadWrite);
}

void UArcPSOPrecacheRebuildProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& ExecutionContext)
{
#if !WITH_EDITOR && UE_WITH_PSO_PRECACHING
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcPSOPrecacheRebuild);

	EntityQuery.ForEachEntityChunk(ExecutionContext,
		[](FMassExecutionContext& Context)
		{
			const TArrayView<FArcRenderStateFragment>& RenderStateFragments = Context.GetMutableFragmentView<FArcRenderStateFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Context.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcRenderStateFragment& RenderStateFragment = RenderStateFragments[*EntityIt];
				if (!RenderStateFragment.HasRenderStateHelper())
				{
					continue;
				}

				FArcPrimitiveRenderStateHelper& Helper = RenderStateFragment.GetRenderStateHelper<FArcPrimitiveRenderStateHelper>();
				if (!Helper.ConsumePendingPSOPrecacheRebuild())
				{
					continue;
				}

				// Unbatched destroy: the replacement proxy is added below and shares the helper's single
				// FPrimitiveSceneInfoData, so a deferred removal of the old one would land on the live primitive.
				Helper.DestroyRenderState();

				if (Helper.ShouldCreateRenderState())
				{
					Helper.CreateRenderState(nullptr);
				}
			}
		});
#endif // !WITH_EDITOR && UE_WITH_PSO_PRECACHING
}
