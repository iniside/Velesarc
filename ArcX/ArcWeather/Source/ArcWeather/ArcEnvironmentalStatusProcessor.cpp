// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnvironmentalStatusProcessor.h"

#include "ArcConditionEffects/ArcConditionEffectsSubsystem.h"
#include "ArcConditionEffects/ArcConditionTypes.h"
#include "ArcWeatherSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"

UArcEnvironmentalStatusProcessor::UArcEnvironmentalStatusProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcEnvironmentalStatusProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcEnvironmentalStatusConfig>();
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcEnvironmentalStatusProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
	UArcConditionEffectsSubsystem* ConditionSubsystem = World ? World->GetSubsystem<UArcConditionEffectsSubsystem>() : nullptr;
	if (!WeatherSubsystem || !ConditionSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [WeatherSubsystem, ConditionSubsystem, DeltaTime](FMassExecutionContext& Ctx)
	{
		const FArcEnvironmentalStatusConfig& Config = Ctx.GetConstSharedFragment<FArcEnvironmentalStatusConfig>();
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FTransformFragment& Transform = TransformFragments[EntityIt];
			const FMassEntityHandle Entity = Ctx.GetEntity(EntityIt);

			const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(
				Transform.GetTransform().GetLocation());

			// Rain: high humidity above freezing -> apply Wet
			if (Weather.Humidity >= Config.HumidityThreshold && Weather.Temperature > Config.FreezeThreshold)
			{
				ConditionSubsystem->ApplyCondition(Entity, EArcConditionType::Wet, Config.WetRate * DeltaTime);
			}

			// Cold: below freeze threshold -> apply Chilled
			if (Weather.Temperature < Config.FreezeThreshold)
			{
				ConditionSubsystem->ApplyCondition(Entity, EArcConditionType::Chilled, Config.ColdRate * DeltaTime);
			}
		}
	});
}

void UArcEnvironmentalStatusTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.RequireFragment<FTransformFragment>();

	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(StatusConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
