// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisUAFBridgeProcessor.h"
#include "ArcMobileVisualization.h"
#include "ArcMobileVisUAFTrait.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "MassRepresentationFragments.h"
#include "Fragments/MassUAFComponentFragment.h"
#include "Component/AnimNextComponent.h"
#include "Variables/AnimNextVariableReference.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMobileVisUAFBridgeProcessor)

UArcMobileVisUAFBridgeProcessor::UArcMobileVisUAFBridgeProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = false;
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcMobileVisUAFBridgeProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisRepFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassUAFComponentWrapperFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisUAFAnimDesc>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FArcMobileVisConfigFragment>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
}

void UArcMobileVisUAFBridgeProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisUAFBridge);

	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcMobileVisRepFragment> RepFragments = Ctx.GetMutableFragmentView<FArcMobileVisRepFragment>();
			TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			TConstArrayView<FMassUAFComponentWrapperFragment> UAFFragments = Ctx.GetFragmentView<FMassUAFComponentWrapperFragment>();
			TConstArrayView<FMassActorFragment> ActorFragments = Ctx.GetFragmentView<FMassActorFragment>();
			const FArcMobileVisUAFAnimDesc& AnimDesc = Ctx.GetConstSharedFragment<FArcMobileVisUAFAnimDesc>();
			const FArcMobileVisConfigFragment& Config = Ctx.GetConstSharedFragment<FArcMobileVisConfigFragment>();

			if (!Config.bUseSkinnedMesh)
			{
				return;
			}

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];
				if (LOD.CurrentLOD != EArcMobileVisLOD::Actor)
				{
					continue;
				}

				UUAFComponent* UAFComp = UAFFragments[EntityIt].Component.Get();
				if (!UAFComp)
				{
					continue;
				}

				FArcMobileVisRepFragment& Rep = RepFragments[EntityIt];

				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				if (LOD.PrevLOD != LOD.CurrentLOD)
				{
					UAFComp->SetVariable(FAnimNextVariableReference(AnimDesc.AnimSequenceIndexVarName), Rep.AnimationIndex);
					UAFComp->SetVariable(FAnimNextVariableReference(AnimDesc.JustSwappedVarName), true);
				}
				else
				{
					UAFComp->GetVariable(FAnimNextVariableReference(AnimDesc.AnimSequenceIndexVarName), Rep.AnimationIndex);
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
		});
}
