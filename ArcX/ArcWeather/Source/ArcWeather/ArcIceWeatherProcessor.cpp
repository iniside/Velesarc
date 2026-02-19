// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIceWeatherProcessor.h"

#include "ArcWeatherSubsystem.h"
#include "ArcMass/ArcMassFragments.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"
#include "ArcMass/ArcMassHealthSignalProcessor.h"

UArcIceWeatherProcessor::UArcIceWeatherProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcIceWeatherProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FArcIceFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FArcMassHealthFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcIceWeatherProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
	UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
	if (!WeatherSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	CurrentSignalTime += DeltaTime;
	
	TArray<FMassEntityHandle> EntitiesToSignal;
	EntityQuery.ForEachEntityChunk(Context, [WeatherSubsystem, DeltaTime, &EntitiesToSignal](FMassExecutionContext& Ctx)
	{
		const TConstArrayView<FArcIceFragment> IceFragments = Ctx.GetFragmentView<FArcIceFragment>();
		TArrayView<FArcMassHealthFragment> HealthFragments = Ctx.GetMutableFragmentView<FArcMassHealthFragment>();
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcIceFragment& Ice = IceFragments[EntityIt];
			FArcMassHealthFragment& Health = HealthFragments[EntityIt];
			const FTransformFragment& Transform = TransformFragments[EntityIt];

			const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(
				Transform.GetTransform().GetLocation());

			// Effective temperature delta: how far above/below the ice's resistance
			const float EffectiveTemp = Weather.Temperature - Ice.TemperatureResistance;

			// Rate = temperature_delta / thermal_mass
			// Positive EffectiveTemp -> melting (health loss)
			// Negative EffectiveTemp -> freezing/strengthening (health gain)
			const float Rate = EffectiveTemp / Ice.ThermalMass;
			Health.CurrentHealth -= Rate * DeltaTime;
			Health.CurrentHealth = FMath::Clamp(Health.CurrentHealth, 0.f, Health.MaxHealth);
			EntitiesToSignal.Add(Ctx.GetEntity(EntityIt));
		}
	});
	
	if (EntitiesToSignal.Num() > 0 && SignalSubsystem && CurrentSignalTime > 2.f)
	{
		CurrentSignalTime = 0;
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::HealthChanged, EntitiesToSignal);
	}
}

void UArcIceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment(FConstStructView::Make(IceConfig));
	
	BuildContext.RequireFragment<FArcMassHealthFragment>();
	BuildContext.RequireFragment<FTransformFragment>();
}
