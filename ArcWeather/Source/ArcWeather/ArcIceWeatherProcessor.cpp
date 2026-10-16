// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcIceWeatherProcessor.h"

#include "ArcWeatherSubsystem.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcMassMechanics/Combat/ArcCombatFunctions.h"
#include "Effects/ArcEffectDefinition.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassWorldUtils.h"
#include "Mass/ExecutionContext.h"
#include "MassSubsystemAccess.h"

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
	EntityQuery.AddRequirement<FArcEffectStackFragment>(EMassFragmentAccess::ReadWrite, EMassFragmentPresence::Optional);
	EntityQuery.AddConstSharedRequirement<FArcIceEffectsSharedFragment>();

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
		const FArcIceEffectsSharedFragment& Effects = Ctx.GetConstSharedFragment<FArcIceEffectsSharedFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			const FArcIceFragment& Ice = IceFragments[EntityIt];
			const FTransformFragment& Transform = TransformFragments[EntityIt];

			const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(
				Transform.GetTransform().GetLocation());

			const float EffectiveTemp = Weather.Temperature - Ice.TemperatureResistance;
			const float Rate = EffectiveTemp / Ice.ThermalMass;
			const float DamageThisTick = Rate * DeltaTime;

			if (FMath::IsNearlyZero(DamageThisTick))
			{
				continue;
			}

			if (DamageThisTick > 0.f)
			{
				ArcMechCombat::ApplyEffectWithSetByCaller(EntityManager, Ctx.GetEntity(EntityIt), FMassEntityHandle(),
					Effects.DamageEffect, ArcMassMechanicsTags::SBC_TrueDamage, DamageThisTick);
			}
			else
			{
				ArcMechCombat::ApplyEffectWithSetByCaller(EntityManager, Ctx.GetEntity(EntityIt), FMassEntityHandle(),
					Effects.HealEffect, ArcMassMechanicsTags::SBC_TrueHeal, -DamageThisTick);
			}
		}
	});
}

void UArcIceTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment(FConstStructView::Make(IceConfig));

	FArcIceEffectsSharedFragment SharedEffects;
	SharedEffects.DamageEffect = DamageEffect;
	SharedEffects.HealEffect = HealEffect;
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);
	BuildContext.AddConstSharedFragment(EntityManager.GetOrCreateConstSharedFragment(SharedEffects));

	BuildContext.RequireFragment<FTransformFragment>();
}
