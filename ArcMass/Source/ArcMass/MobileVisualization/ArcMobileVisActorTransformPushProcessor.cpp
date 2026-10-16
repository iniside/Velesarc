// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisActorTransformPushProcessor.h"
#include "ArcMobileVisualization.h"
#include "GameFramework/Actor.h"
#include "MassActorSubsystem.h"
#include "MassCommonFragments.h"
#include "Mass/ExecutionContext.h"
#include "MoverMassTranslators.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMobileVisActorTransformPushProcessor)

UArcMobileVisActorTransformPushProcessor::UArcMobileVisActorTransformPushProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	// PostPhysics on the game thread: AActor movement must not race the physics step.
	ProcessingPhase = EMassProcessingPhase::PostPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcMobileVisActorTransformPushProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcMobileVisLODFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassActorFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddTagRequirement<FMassNavMoverCopyToMassTag>(EMassFragmentPresence::None);
}

void UArcMobileVisActorTransformPushProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisActorTransformPush);

	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			const TConstArrayView<FArcMobileVisLODFragment> LODFragments = Ctx.GetFragmentView<FArcMobileVisLODFragment>();
			const TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const TArrayView<FMassActorFragment> ActorFragments = Ctx.GetMutableFragmentView<FMassActorFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMobileVisLODFragment& LOD = LODFragments[EntityIt];
				if (LOD.CurrentLOD != EArcMobileVisLOD::Actor)
				{
					continue;
				}

				AActor* Actor = ActorFragments[EntityIt].GetMutable();
				if (!Actor)
				{
					continue;
				}

				Actor->SetActorTransform(Transforms[EntityIt].GetTransform(), /*bSweep*/ false, nullptr, ETeleportType::TeleportPhysics);
			}
		});
}
