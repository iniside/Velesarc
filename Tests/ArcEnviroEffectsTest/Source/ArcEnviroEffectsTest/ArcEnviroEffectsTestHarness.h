// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

class UArcAttributeHandlerConfig;
class UArcEnviroEffectsSubsystem;
class UArcPendingAttributeOpsProcessor;
class UArcWeatherSubsystem;
class UWorld;
struct FActorTestSpawner;
struct FArcAttributeRef;
struct FArcEnviroAttributes;
struct FMassEntityManager;

namespace ArcEnviroTest
{
	constexpr float ColdThresholds[3] = {15.f, 5.f, -5.f};
	constexpr float HeatThresholds[3] = {30.f, 38.f, 46.f};
	constexpr float RainOffsets[3] = {5.f, 15.f, 25.f};
	constexpr float NeutralTemperature = 22.f;
	constexpr float DryHumidity = 40.f;
	constexpr float CellHumidityThreshold = 70.f;

	struct FEntitySetup
	{
		float ColdProtection = 0.f;
		float HeatProtection = 0.f;
		float RainProtection = 0.f;
	};

	class FHarness
	{
	public:
		FHarness() = default;
		FHarness(const FHarness&) = delete;
		FHarness& operator=(const FHarness&) = delete;
		~FHarness();

		bool Initialize(FActorTestSpawner& Spawner);

		FMassEntityHandle CreateEntity(const FEntitySetup& Setup = FEntitySetup());
		bool SetWeatherLevels(int32 Rain, int32 Cold, int32 Heat);
		void DrainPendingOps();
		void ApplyDelta(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Delta);
		void OverrideAttribute(FMassEntityHandle Entity, const FArcAttributeRef& Attribute, float Value);

		FArcEnviroAttributes* GetAttributes(FMassEntityHandle Entity) const;

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UArcWeatherSubsystem* Weather = nullptr;
		UArcEnviroEffectsSubsystem* Enviro = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;

	private:
		UArcPendingAttributeOpsProcessor* PendingOpsProcessor = nullptr;
		float SavedThresholds[9] = {};
		bool bThresholdsSaved = false;
	};
}
