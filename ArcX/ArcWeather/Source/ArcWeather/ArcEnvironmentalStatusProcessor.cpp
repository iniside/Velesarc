// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnvironmentalStatusProcessor.h"

#include "ArcWeatherSubsystem.h"
#include "MassCommonFragments.h"
#include "MassCommonTypes.h"
#include "MassEntityTemplateRegistry.h"
#include "MassEntityUtils.h"
#include "MassExecutionContext.h"
#include "MassSignalSubsystem.h"

UArcEnvironmentalStatusProcessor::UArcEnvironmentalStatusProcessor()
	: EntityQuery(*this)
{
	ProcessingPhase = EMassProcessingPhase::DuringPhysics;
	ExecutionOrder.ExecuteBefore.Add(UE::Mass::ProcessorGroupNames::UpdateWorldFromMass);
}

void UArcEnvironmentalStatusProcessor::ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager)
{
	EntityQuery.Initialize(EntityManager);
	EntityQuery.AddRequirement<FArcEnvironmentalStatusFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddConstSharedRequirement<FArcEnvironmentalStatusConfig>();
	EntityQuery.RegisterWithProcessor(*this);
}

void UArcEnvironmentalStatusProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	UWorld* World = EntityManager.GetWorld();
	UArcWeatherSubsystem* WeatherSubsystem = World ? World->GetSubsystem<UArcWeatherSubsystem>() : nullptr;
	UMassSignalSubsystem* SignalSubsystem = World ? World->GetSubsystem<UMassSignalSubsystem>() : nullptr;
	if (!WeatherSubsystem)
	{
		return;
	}

	const float DeltaTime = Context.GetDeltaTimeSeconds();
	SignalTimer += DeltaTime;

	TArray<FMassEntityHandle> EntitiesToSignal;

	EntityQuery.ForEachEntityChunk(Context, [WeatherSubsystem, DeltaTime, &EntitiesToSignal](FMassExecutionContext& Ctx)
	{
		const FArcEnvironmentalStatusConfig& Config = Ctx.GetConstSharedFragment<FArcEnvironmentalStatusConfig>();
		TArrayView<FArcEnvironmentalStatusFragment> StatusFragments = Ctx.GetMutableFragmentView<FArcEnvironmentalStatusFragment>();
		const TConstArrayView<FTransformFragment> TransformFragments = Ctx.GetFragmentView<FTransformFragment>();

		for (FMassExecutionContext::FEntityIterator EntityIt = Ctx.CreateEntityIterator(); EntityIt; ++EntityIt)
		{
			FArcEnvironmentalStatusFragment& Status = StatusFragments[EntityIt];
			const FTransformFragment& Transform = TransformFragments[EntityIt];

			const FArcClimateParams Weather = WeatherSubsystem->GetWeatherAtLocation(
				Transform.GetTransform().GetLocation());

			// --- Wetness ---
			if (Weather.Humidity >= Config.HumidityThreshold && Weather.Temperature > 0.f)
			{
				Status.Wetness += Config.WetRate * DeltaTime;
			}
			else if (Weather.Temperature > 0.f)
			{
				Status.Wetness -= Config.DryRate * DeltaTime;
			}

			// --- Frozen ---
			if (Weather.Temperature < Config.FreezeThreshold && Status.Wetness > 0.f)
			{
				const float WetnessFactor = Status.Wetness / 100.f;
				const float FreezeAmount = Config.FreezeRate * WetnessFactor * DeltaTime;
				Status.Frozen += FreezeAmount;
				// Water turning to ice reduces wetness
				Status.Wetness -= FreezeAmount;
			}
			else if (Weather.Temperature >= Config.FreezeThreshold && Status.Frozen > 0.f)
			{
				const float ThawAmount = Config.ThawRate * DeltaTime;
				// Thawed ice becomes wetness
				const float ActualThaw = FMath::Min(ThawAmount, Status.Frozen);
				Status.Frozen -= ActualThaw;
				Status.Wetness += ActualThaw;
			}

			Status.Wetness = FMath::Clamp(Status.Wetness, 0.f, 100.f);
			Status.Frozen = FMath::Clamp(Status.Frozen, 0.f, 100.f);

			EntitiesToSignal.Add(Ctx.GetEntity(EntityIt));
		}
	});

	if (EntitiesToSignal.Num() > 0 && SignalSubsystem && SignalTimer > 2.f)
	{
		SignalTimer = 0.f;
		SignalSubsystem->SignalEntities(UE::ArcMass::Signals::EnvironmentalStatusChanged, EntitiesToSignal);
	}
}

void UArcEnvironmentalStatusTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	BuildContext.AddFragment<FArcEnvironmentalStatusFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	const FConstSharedStruct ConfigFragment = EntityManager.GetOrCreateConstSharedFragment(StatusConfig);
	BuildContext.AddConstSharedFragment(ConfigFragment);
}
