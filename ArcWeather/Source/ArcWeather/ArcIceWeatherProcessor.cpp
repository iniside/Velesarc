// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIceWeatherProcessor.h"

#include "ArcWeatherSubsystem.h"
#include "ArcMassDamageSystem/ArcMassDamageTypes.h"
#include "ArcMassDamageSystem/ArcMassHealthStatsFragment.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "Modifiers/ArcModifierFunctions.h"

UArcIceWeatherProcessor::UArcIceWeatherProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
	bRequiresGameThreadExecution = true;
}

void UArcIceWeatherProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FArcIceFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);

	EntityQuery.AddSubsystemRequirement<UArcWeatherSubsystem>(EMassFragmentAccess::ReadWrite);

	EntityQuery.RegisterWithProcessor(*this);
}

void UArcIceWeatherProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
	if (!WeatherSubsystem)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(ArcIceWeather);

	const float DeltaTime = Context.GetDeltaTimeSeconds();

	EntityQuery.ForEachEntityChunk(Context, [&EntityManager, WeatherSubsystem, DeltaTime](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcIceFragment> IceFragments = Ctx.GetFragmentView<FArcIceFragment>();
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcIceFragment& Ice = IceFragments[EntityIt];
			const FTransformFragment& Transform = TransformFragments[EntityIt];

			const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(
				Transform.GetTransform().GetLocation());

			const float EffectiveTemp = Weather.Temperature - Ice.TemperatureResistance;
			const float Rate = EffectiveTemp / Ice.ThermalMass;
			const float DamageThisTick = Rate * DeltaTime;

			if (!FMath::IsNearlyZero(DamageThisTick))
			{
				ArcModifiers::QueueExecute(EntityManager, Ctx.GetEntity(EntityIt),
					FArcMassDamageStatsFragment::GetHealthDamageAttribute(),
					EArcModifierOp::Add, DamageThisTick);
			}
		}
	});
}

void UArcIceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment(FConstStructView::Make(IceConfig));

	BuildContext.RequireFragment<FArcMassHealthStatsFragment>();
	BuildContext.RequireFragment<FArcMassDamageStatsFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
}
