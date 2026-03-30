// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWeatherChilledProcessor.h"

#include "ArcConditionEffects/ArcConditionEffectsSubsystem.h"
#include "ArcConditionEffects/ArcConditionFragments.h"
#include "ArcConditionEffects/ArcConditionTypes.h"
#include "ArcWeatherSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

UArcWeatherChilledProcessor::UArcWeatherChilledProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcWeatherChilledProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcChilledConditionFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcWeatherChilledProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
	UArcConditionEffectsSubsystem* ConditionSubsystem = World ? World->GetSubsystem<UArcConditionEffectsSubsystem>() : nullptr;
	if (!WeatherSubsystem || !ConditionSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcWeatherChilled);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [WeatherSubsystem, ConditionSubsystem, DeltaTime](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FArcChilledConditionFragment> ChilledFragments = Ctx.GetFragmentView<FArcChilledConditionFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransformFragment& Transform = TransformFragments[EntityIt];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FVector Location = Transform.GetTransform().GetLocation();

			const float ColdIntensity = WeatherSubsystem->GetColdIntensityAtLocation(Location);

			if (ColdIntensity > 0.f)
			{
				ConditionSubsystem->ApplyCondition(Entity, EArcConditionType::Chilled, ColdIntensity * DeltaTime);
			}
			else if (ChilledFragments[EntityIt].State.Saturation > 0.f)
			{
				const FArcWeatherCell* Cell = WeatherSubsystem->GetGrid().Find(
					WeatherSubsystem->WorldToCell(Location));
				const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(Location);
				const float FrzThreshold = Cell ? Cell->FreezeThreshold : WeatherSubsystem->DefaultFreezeThreshold;
				const float Warmth = (Weather.Temperature - FrzThreshold) / WeatherSubsystem->ColdNormalizationDelta;
				ConditionSubsystem->ApplyCondition(Entity, EArcConditionType::Chilled, -Warmth * DeltaTime);
			}
		}
	});
}
