// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMobileVisPhysicsTransformPushProcessor.h"
#include "ArcMobileVisualization.h"
#include "ArcMass/Physics/ArcMassPhysicsBody.h"
#include "ArcMass/Physics/ArcMassPhysicsSimulation.h"
#include "MassCommonFragments.h"
#include "MassExecutionContext.h"
#include "PhysicsEngine/BodyInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMobileVisPhysicsTransformPushProcessor)

UArcMobileVisPhysicsTransformPushProcessor::UArcMobileVisPhysicsTransformPushProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::PrePhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::Client | EProcessorExecutionFlags::Standalone);
}

void UArcMobileVisPhysicsTransformPushProcessor::ConfigureQueries(
	const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMassPhysicsBodyFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FArcMobileVisEntityTag>(EMassFragmentPresence::All);
	EntityQuery.AddSparseRequirement<FArcMassPhysicsSimulatingTag>(EMassFragmentPresence::None);
}

void UArcMobileVisPhysicsTransformPushProcessor::Execute(
	FMassEntityManager& EntityManager,
	FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcMobileVisPhysicsTransformPush);

	EntityQuery.ForEachEntityChunk(Context,
		[](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			TConstArrayView<FArcMassPhysicsBodyFragment> Bodies = Ctx.GetFragmentView<FArcMassPhysicsBodyFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				const FArcMassPhysicsBodyFragment& BodyFrag = Bodies[EntityIt];
				if (!BodyFrag.Body)
				{
					continue;
				}

				const FTransform& EntityTransform = Transforms[EntityIt].GetTransform();
				BodyFrag.Body->SetBodyTransform(EntityTransform, ETeleportType::TeleportPhysics);
			}
		});
}
