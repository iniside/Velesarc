// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWeatherWetProcessor.h"

#include "ArcConditionEffects/ArcConditionEffectsSubsystem.h"
#include "ArcConditionEffects/ArcConditionFragments.h"
#include "ArcConditionEffects/ArcConditionTypes.h"
#include "ArcWeatherSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassExecutionContext.h"

UArcWeatherWetProcessor::UArcWeatherWetProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcWeatherWetProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcWetConditionFragment>(EMassFragmentAccess::ReadOnly);
	
	EntityQuery.AddSubsystemRequirement<UArcConditionEffectsSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddSubsystemRequirement<UArcWeatherSubsystem>(EMassFragmentAccess::ReadWrite);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcWeatherWetProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
	UArcConditionEffectsSubsystem* ConditionSubsystem = World ? World->GetSubsystem<UArcConditionEffectsSubsystem>() : nullptr;
	if (!WeatherSubsystem || !ConditionSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcWeatherWet);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [WeatherSubsystem, ConditionSubsystem, DeltaTime](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransformFragment& Transform = TransformFragments[EntityIt];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);
			const FVector Location = Transform.GetTransform().GetLocation();

			const float RainIntensity = WeatherSubsystem->GetRainIntensityAtLocation(Location);

			if (RainIntensity > 0.f)
			{
				ConditionSubsystem->ApplyCondition(Entity, EArcConditionType::Wet, RainIntensity * DeltaTime);
			}
			else
			{
				const FArcWeatherCell* Cell = WeatherSubsystem->GetGrid().Find(
					WeatherSubsystem->WorldToCell(Location));
				const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(Location);
				const float FrzThreshold = Cell ? Cell->FreezeThreshold : WeatherSubsystem->DefaultFreezeThreshold;

				if (Weather.Temperature > FrzThreshold)
				{
					const float DryRate = (Weather.Temperature - FrzThreshold) / WeatherSubsystem->ColdNormalizationDelta;
					ConditionSubsystem->ApplyCondition(Entity, EArcConditionType::Wet, -DryRate * DeltaTime);
				}
			}
		}
	});
}
