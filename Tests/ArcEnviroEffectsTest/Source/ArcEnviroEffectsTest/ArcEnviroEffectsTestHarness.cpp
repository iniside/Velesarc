// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcEnviroEffectsTestHarness.h"

#include "ArcEnviroEffects/ArcEnviroAttributeHandlers.h"
#include "ArcEnviroEffects/ArcEnviroAttributes.h"
#include "ArcEnviroEffects/ArcEnviroEffectsSubsystem.h"
#include "ArcWeather/ArcWeatherLevelSettings.h"
#include "ArcWeather/ArcWeatherSubsystem.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInstantModifier.h"
#include "Attributes/ArcResourceAttributeHandlers.h"
#include "Components/ActorTestSpawner.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntitySubsystem.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcEnviroHarnessPrivate
{
	void ReadThresholds(float (&Out)[9])
	{
		const UArcWeatherLevelSettings* Settings = GetDefault<UArcWeatherLevelSettings>();
		Out[0] = Settings->ColdLevel1;
		Out[1] = Settings->ColdLevel2;
		Out[2] = Settings->ColdLevel3;
		Out[3] = Settings->HeatLevel1;
		Out[4] = Settings->HeatLevel2;
		Out[5] = Settings->HeatLevel3;
		Out[6] = Settings->RainLevel1;
		Out[7] = Settings->RainLevel2;
		Out[8] = Settings->RainLevel3;
	}

	void WriteThresholds(const float (&In)[9])
	{
		UArcWeatherLevelSettings* Settings = GetMutableDefault<UArcWeatherLevelSettings>();
		Settings->ColdLevel1 = In[0];
		Settings->ColdLevel2 = In[1];
		Settings->ColdLevel3 = In[2];
		Settings->HeatLevel1 = In[3];
		Settings->HeatLevel2 = In[4];
		Settings->HeatLevel3 = In[5];
		Settings->RainLevel1 = In[6];
		Settings->RainLevel2 = In[7];
		Settings->RainLevel3 = In[8];
	}

	void AddProtectionHandler(UArcAttributeHandlerConfig& HandlerConfig, const FArcAttributeRef& Attribute)
	{
		FArcAttributeHandler_ClampRange Clamp;
		Clamp.MinConst = 0.f;
		Clamp.MaxConst = 3.f;
		HandlerConfig.Handlers.Add(Attribute, FInstancedStruct::Make(Clamp));
	}

	void AddConditionHandler(UArcAttributeHandlerConfig& HandlerConfig, const FArcAttributeRef& Meta, const FArcAttributeRef& Resource, float MinValue, float MaxValue,
		const FArcAttributeRef& Level, const FArcAttributeRef& NegativeLevel)
	{
		FArcEnviroAttributeHandler_ConditionDelta Handler;
		Handler.Resource = Resource;
		Handler.MinConst = MinValue;
		Handler.MaxConst = MaxValue;
		Handler.Op = EArcResourceOp::AddSigned;
		Handler.Level = Level;
		Handler.NegativeLevel = NegativeLevel;
		Handler.Bands.Thresholds = {25.f, 50.f, 75.f};
		HandlerConfig.Handlers.Add(Meta, FInstancedStruct::Make(Handler));
	}
}

ArcEnviroTest::FHarness::~FHarness()
{
	if (bThresholdsSaved)
	{
		ArcEnviroHarnessPrivate::WriteThresholds(SavedThresholds);
	}
}

bool ArcEnviroTest::FHarness::Initialize(FActorTestSpawner& Spawner)
{
	using namespace ArcEnviroHarnessPrivate;

	ReadThresholds(SavedThresholds);
	bThresholdsSaved = true;
	float TestThresholds[9];
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestThresholds[Index] = ColdThresholds[Index];
		TestThresholds[3 + Index] = HeatThresholds[Index];
		TestThresholds[6 + Index] = RainOffsets[Index];
	}
	WriteThresholds(TestThresholds);

	Spawner.GetWorld();
	Spawner.InitializeGameSubsystems();
	World = &Spawner.GetWorld();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	Weather = World->GetSubsystem<UArcWeatherSubsystem>();
	Enviro = World->GetSubsystem<UArcEnviroEffectsSubsystem>();
	if (!EntitySubsystem || !Weather || !Enviro)
	{
		return false;
	}

	EntityManager = &EntitySubsystem->GetMutableEntityManager();
	const TSharedRef<FMassEntityManager> SharedEntityManager = EntityManager->AsShared();

	Weather->DefaultHumidityThreshold = CellHumidityThreshold;
	Weather->DefaultFreezeThreshold = -50.f;

	HandlerConfig = NewObject<UArcAttributeHandlerConfig>();
	AddProtectionHandler(*HandlerConfig, FArcEnviroAttributes::GetColdProtectionAttribute());
	AddProtectionHandler(*HandlerConfig, FArcEnviroAttributes::GetHeatProtectionAttribute());
	AddProtectionHandler(*HandlerConfig, FArcEnviroAttributes::GetRainProtectionAttribute());
	AddConditionHandler(*HandlerConfig, FArcEnviroAttributes::GetWetDeltaAttribute(), FArcEnviroAttributes::GetWetAttribute(), 0.f, 100.f,
		FArcEnviroAttributes::GetWetLevelAttribute(), FArcAttributeRef());
	AddConditionHandler(*HandlerConfig, FArcEnviroAttributes::GetTemperatureDeltaAttribute(), FArcEnviroAttributes::GetTemperatureAttribute(), -100.f, 100.f,
		FArcEnviroAttributes::GetHotLevelAttribute(), FArcEnviroAttributes::GetChilledLevelAttribute());

	PendingOpsProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
	PendingOpsProcessor->CallInitialize(EntityManager->GetOwner(), SharedEntityManager);

	return true;
}

FMassEntityHandle ArcEnviroTest::FHarness::CreateEntity(const FEntitySetup& Setup)
{
	FArcEnviroAttributes Attributes;
	Attributes.InitColdProtection(Setup.ColdProtection);
	Attributes.InitHeatProtection(Setup.HeatProtection);
	Attributes.InitRainProtection(Setup.RainProtection);

	FArcAttributeHandlerSharedFragment HandlerShared;
	HandlerShared.Config = HandlerConfig;

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcEffectStackFragment>()
		.Add<FArcAggregatorFragment>()
		.Add<FArcOwnedTagsFragment>()
		.Add<FArcPendingAttributeOpsFragment>()
		.Add<FArcEnviroAttributes>(Attributes)
		.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);

	return Builder.Commit();
}

bool ArcEnviroTest::FHarness::SetWeatherLevels(int32 Rain, int32 Cold, int32 Heat)
{
	const bool bInRange = Rain >= 0 && Rain <= 3 && Cold >= 0 && Cold <= 3 && Heat >= 0 && Heat <= 3;
	if (!bInRange || (Cold > 0 && Heat > 0))
	{
		return false;
	}

	FArcClimateParams Climate;
	if (Cold > 0)
	{
		Climate.Temperature = ColdThresholds[Cold - 1];
	}
	else if (Heat > 0)
	{
		Climate.Temperature = HeatThresholds[Heat - 1];
	}
	else
	{
		Climate.Temperature = NeutralTemperature;
	}
	Climate.Humidity = Rain > 0 ? CellHumidityThreshold + RainOffsets[Rain - 1] : DryHumidity;
	Weather->DefaultClimate = Climate;

	return Weather->GetRainLevelAtLocation(FVector::ZeroVector) == Rain
		&& Weather->GetColdLevelAtLocation(FVector::ZeroVector) == Cold
		&& Weather->GetHeatLevelAtLocation(FVector::ZeroVector) == Heat;
}

void ArcEnviroTest::FHarness::DrainPendingOps()
{
	FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
	UE::Mass::Executor::Run(*PendingOpsProcessor, ProcessingContext);
}

void ArcEnviroTest::FHarness::ApplyDelta(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Delta)
{
	ArcAttributes::EnqueueInstantModifier(*EntityManager, Entity, Attribute, EArcModifierOp::Add, Delta, EArcAttributeOpType::Execute);
	DrainPendingOps();
}

void ArcEnviroTest::FHarness::OverrideAttribute(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Value)
{
	ArcAttributes::EnqueueInstantModifier(*EntityManager, Entity, Attribute, EArcModifierOp::Override, Value, EArcAttributeOpType::Change);
	DrainPendingOps();
}

FArcEnviroAttributes* ArcEnviroTest::FHarness::GetAttributes(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcEnviroAttributes>();
}
