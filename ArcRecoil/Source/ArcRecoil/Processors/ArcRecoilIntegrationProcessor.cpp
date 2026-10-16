// Copyright Lukasz Baran. All Rights Reserved.

#include "Processors/ArcRecoilIntegrationProcessor.h"

#include "ArcRecoilModule.h"
#include "ArcRecoilStep.h"
#include "Fragments/ArcRecoilFragments.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcRecoilIntegrationProcessor)

UArcRecoilIntegrationProcessor::UArcRecoilIntegrationProcessor()
	: EntityQuery{*this}
{
	bAutoRegisterWithProcessingPhases = true;
	bRequiresGameThreadExecution = true;
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionFlags = static_cast<int32>(EProcessorExecutionFlags::AllNetModes);
}

void UArcRecoilIntegrationProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.AddRequirement<FArcRecoilStateFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FArcRecoilProfileFragment>(EMassFragmentAccess::ReadOnly);
}

void UArcRecoilIntegrationProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcRecoilIntegration);

	const float RawDeltaTime = Context.GetDeltaTimeSeconds();
	const float DeltaTime = FMath::IsFinite(RawDeltaTime) ? FMath::Max(RawDeltaTime, 0.0f) : 0.0f;
	const int32 LogEntityIndex = CVarArcRecoilLogEntity.GetValueOnGameThread();

	EntityQuery.ForEachEntityChunk(Context,
		[DeltaTime, LogEntityIndex](FMassExecutionContext& Ctx)
		{
			TArrayView<FArcRecoilStateFragment> States = Ctx.GetMutableFragmentView<FArcRecoilStateFragment>();
			TConstArrayView<FArcRecoilProfileFragment> Profiles = Ctx.GetFragmentView<FArcRecoilProfileFragment>();

			for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
			{
				FArcRecoilStateFragment& State = States[EntityIt];
				const FArcRecoilProfileFragment& Profile = Profiles[EntityIt];

				const ArcRecoil::FArcRecoilStepReport Report = ArcRecoil::AdvanceState(State, Profile, DeltaTime);

				if (Report.bDiverged)
				{
					UE_LOGF(LogArcRecoil, Warning,"Entity %d: recoil spring diverged (Offset=(%f, %f) Vel=(%f, %f) Stiffness=%f Damping=%f); state zeroed",
						Ctx.GetEntity(EntityIt).Index,
						Report.PreZeroOffsetDeg.X, Report.PreZeroOffsetDeg.Y,
						Report.PreZeroVelDegSec.X, Report.PreZeroVelDegSec.Y,
						Profile.Stiffness, Profile.DampingCoeff);
				}

				if (LogEntityIndex >= 0 && Ctx.GetEntity(EntityIt).Index == LogEntityIndex)
				{
					UE_LOGF(LogArcRecoil, Verbose,"Entity %d: Heat=%.3f ShotIndex=%.3f WalkShots=%.3f Offset=(%.3f, %.3f) Vel=(%.3f, %.3f) Clamped=%d",
						LogEntityIndex, State.Heat, State.ShotIndex, State.WalkShots,
						State.AngularOffsetDeg.X, State.AngularOffsetDeg.Y,
						State.OffsetVelDegSec.X, State.OffsetVelDegSec.Y,
						Report.bClamped ? 1 : 0);
				}
			}
		});
}
