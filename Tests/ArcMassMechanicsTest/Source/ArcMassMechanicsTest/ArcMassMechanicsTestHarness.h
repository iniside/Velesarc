// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMechTestSupport.h"
#include "Mass/EntityHandle.h"

class UArcAttributeHandlerConfig;
class UArcPendingAttributeOpsProcessor;
class UArcResourceConfig;
class UArcResourceProcessor;
class UWorld;
struct FActorTestSpawner;
struct FArcAttributeRef;
struct FArcMechTest_FlowSet;
struct FArcMechTest_ResourceSet;
struct FArcResourceDef;
struct FMassEntityManager;
struct FMassEntityTemplateData;
struct FInstancedStruct;

namespace ArcMassMechanicsTest
{
	struct FEntityOptions
	{
		float ResourceA = 0.f;
		float ResourceB = 0.f;
		float RegenPips = 0.f;
		float DegenPips = 0.f;
		float PerPip = 0.f;
		bool bHandlerConfig = true;
		UArcAttributeHandlerConfig* HandlerConfigOverride = nullptr;
	};

	FArcResourceDef MakeResource(const FArcAttributeRef& Value, const FArcAttributeRef& Delta, float Min, float Max, float Target, float UnitsPerSecond);
	FArcResourceDef MakeResourceA(float Min, float Max, float Target, float UnitsPerSecond);
	FArcResourceDef MakeResourceB(float Min, float Max, float Target, float UnitsPerSecond);
	void SetFixedTarget(UArcResourceConfig& Config, int32 ResourceIndex, float Value);
	// Replaces ResourceA's rate slot with a Pips rate on the test set's pip attributes (RegenPips, DegenPips, PerPip).
	void SetPipsRate(UArcResourceConfig& Config, int32 ResourceIndex);

	void BuildTraitTemplate(UArcResourceConfig* Config, const UWorld& World, FMassEntityTemplateData& OutTemplate);

	class FHarness
	{
	public:
		bool Initialize(FActorTestSpawner& Spawner);

		UArcResourceConfig* MakeConfig(float UpdateInterval = 0.f) const;
		FMassEntityHandle CreateEntity(UArcResourceConfig* Config, const FEntityOptions& Options = FEntityOptions());
		void Tick(float DeltaSeconds = 1.f, int32 Count = 1);

		FArcMechTest_ResourceSet* GetSet(FMassEntityHandle Entity) const;

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;

	private:
		UArcResourceProcessor* Processor = nullptr;
		UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
		TOptional<ArcMechTest::FVerboseLogScope> LogScope;
	};

	struct FFlowEntityOptions
	{
		float Resource = 50.f;
		bool bHandlerConfig = true;
		UArcAttributeHandlerConfig* HandlerConfigOverride = nullptr;
	};

	void AddFlowRule(UArcResourceConfig& Config, const FInstancedStruct& Rate, const FInstancedStruct& Target);
	// A positive rate regenerates toward Max, a negative one drains toward Min at its magnitude (D22).
	void AddConstantFlowRule(UArcResourceConfig& Config, float UnitsPerSecond);

	class FFlowHarness
	{
	public:
		bool Initialize(FActorTestSpawner& Spawner);

		UArcResourceConfig* MakeConfig(float UpdateInterval = 0.f) const;
		FMassEntityHandle CreateEntity(UArcResourceConfig* Config, const FFlowEntityOptions& Options = FFlowEntityOptions());
		void Tick(float DeltaSeconds = 1.f, int32 Count = 1);

		FArcMechTest_FlowSet* GetSet(FMassEntityHandle Entity) const;
		float GetResource(FMassEntityHandle Entity) const;

		UWorld* World = nullptr;
		FMassEntityManager* EntityManager = nullptr;
		UArcAttributeHandlerConfig* HandlerConfig = nullptr;

	private:
		UArcResourceProcessor* Processor = nullptr;
		UArcPendingAttributeOpsProcessor* PendingProcessor = nullptr;
		TOptional<ArcMechTest::FVerboseLogScope> LogScope;
	};
}
