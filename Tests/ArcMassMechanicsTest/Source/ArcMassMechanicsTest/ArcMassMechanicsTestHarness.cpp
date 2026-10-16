// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanicsTestHarness.h"

#include "ArcMassMechanicsTestTypes.h"
#include "ArcMassMechanics/Resources/ArcResourceProcessor.h"
#include "ArcMassMechanics/Resources/ArcResourceTrait.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Components/ActorTestSpawner.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "MassEntitySubsystem.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcMassMechanicsTestHarnessPrivate
{
	struct FTraitBuildContext : public FMassEntityTemplateBuildContext
	{
		explicit FTraitBuildContext(FMassEntityTemplateData& InTemplateData)
			: FMassEntityTemplateBuildContext(InTemplateData)
		{
		}

		void BeginTrait(const UMassEntityTraitBase* Trait)
		{
			SetTraitBeingProcessed(Trait);
		}
	};
}

FArcResourceDef ArcMassMechanicsTest::MakeResource(const FArcAttributeRef& Value, const FArcAttributeRef& Delta, float Min, float Max, float Target, float UnitsPerSecond)
{
	FArcResourceDef Resource;
	Resource.Value = Value;
	Resource.Delta = Delta;
	Resource.Min = Min;
	Resource.Max = Max;

	FArcMechTest_FixedTarget TargetSlot;
	TargetSlot.Value = Target;
	Resource.Target = FInstancedStruct::Make(TargetSlot);

	FArcResourceRate_Constant RateSlot;
	RateSlot.UnitsPerSecond = UnitsPerSecond;
	Resource.Rate = FInstancedStruct::Make(RateSlot);
	return Resource;
}

FArcResourceDef ArcMassMechanicsTest::MakeResourceA(float Min, float Max, float Target, float UnitsPerSecond)
{
	return MakeResource(FArcMechTest_ResourceSet::GetResourceAAttribute(), FArcMechTest_ResourceSet::GetResourceADeltaAttribute(), Min, Max, Target, UnitsPerSecond);
}

FArcResourceDef ArcMassMechanicsTest::MakeResourceB(float Min, float Max, float Target, float UnitsPerSecond)
{
	return MakeResource(FArcMechTest_ResourceSet::GetResourceBAttribute(), FArcMechTest_ResourceSet::GetResourceBDeltaAttribute(), Min, Max, Target, UnitsPerSecond);
}

void ArcMassMechanicsTest::SetFixedTarget(UArcResourceConfig& Config, int32 ResourceIndex, float Value)
{
	FArcMechTest_FixedTarget* Target = Config.Resources[ResourceIndex].Target.GetMutablePtr<FArcMechTest_FixedTarget>();
	check(Target);
	Target->Value = Value;
}

void ArcMassMechanicsTest::SetPipsRate(UArcResourceConfig& Config, int32 ResourceIndex)
{
	FArcResourceRate_Pips Rate;
	Rate.RegenPips = FArcMechTest_ResourceSet::GetRegenPipsAttribute();
	Rate.DegenPips = FArcMechTest_ResourceSet::GetDegenPipsAttribute();
	Rate.PerPip = FArcMechTest_ResourceSet::GetPerPipAttribute();
	Config.Resources[ResourceIndex].Rate = FInstancedStruct::Make(Rate);
}

void ArcMassMechanicsTest::BuildTraitTemplate(UArcResourceConfig* Config, const UWorld& World, FMassEntityTemplateData& OutTemplate)
{
	UArcResourceTrait* Trait = NewObject<UArcResourceTrait>();
	Trait->Config = Config;

	ArcMassMechanicsTestHarnessPrivate::FTraitBuildContext BuildContext(OutTemplate);
	BuildContext.BeginTrait(Trait);
	Trait->BuildTemplate(BuildContext, World);
}

bool ArcMassMechanicsTest::FHarness::Initialize(FActorTestSpawner& Spawner)
{
	LogScope.Emplace();

	Spawner.GetWorld();
	Spawner.InitializeGameSubsystems();
	World = &Spawner.GetWorld();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return false;
	}
	EntityManager = &EntitySubsystem->GetMutableEntityManager();

	HandlerConfig = ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_Resource_Test"));
	if (!HandlerConfig)
	{
		return false;
	}

	Processor = NewObject<UArcResourceProcessor>();
	Processor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
	PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
	PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
	return true;
}

UArcResourceConfig* ArcMassMechanicsTest::FHarness::MakeConfig(float UpdateInterval) const
{
	UArcResourceConfig* Config = NewObject<UArcResourceConfig>();
	Config->UpdateInterval = UpdateInterval;
	return Config;
}

FMassEntityHandle ArcMassMechanicsTest::FHarness::CreateEntity(UArcResourceConfig* Config, const FEntityOptions& Options)
{
	FMassEntityTemplateData TemplateData;
	BuildTraitTemplate(Config, *World, TemplateData);

	FArcMechTest_ResourceSet Set;
	Set.InitResourceA(Options.ResourceA);
	Set.InitResourceB(Options.ResourceB);
	Set.InitRegenPips(Options.RegenPips);
	Set.InitDegenPips(Options.DegenPips);
	Set.InitPerPip(Options.PerPip);

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcMechTest_ResourceSet>(Set)
		.Add<FArcAggregatorFragment>()
		.Add<FArcPendingAttributeOpsFragment>()
		.Add<FArcOwnedTagsFragment>();

	for (const FInstancedStruct& Initial : TemplateData.GetInitialFragmentValues())
	{
		Builder.Add(Initial);
	}
	for (const FConstSharedStruct& Shared : TemplateData.GetSharedFragmentValues().GetConstSharedFragments())
	{
		if (Shared.GetScriptStruct() == FArcResourceConfigSharedFragment::StaticStruct())
		{
			Builder.Add<FArcResourceConfigSharedFragment>(Shared.Get<FArcResourceConfigSharedFragment>());
		}
	}

	if (Options.bHandlerConfig)
	{
		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = Options.HandlerConfigOverride ? Options.HandlerConfigOverride : HandlerConfig;
		Builder.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);
	}

	return Builder.Commit();
}

void ArcMassMechanicsTest::FHarness::Tick(float DeltaSeconds, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
		FMassProcessingContext PendingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, PendingContext);
	}
}

FArcMechTest_ResourceSet* ArcMassMechanicsTest::FHarness::GetSet(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcMechTest_ResourceSet>();
}

void ArcMassMechanicsTest::AddFlowRule(UArcResourceConfig& Config, const FInstancedStruct& Rate, const FInstancedStruct& Target)
{
	FArcResourceDef Resource;
	Resource.Value = FArcMechTest_FlowSet::GetResourceAttribute();
	Resource.Delta = FArcMechTest_FlowSet::GetGainMetaAttribute();
	Resource.Rate = Rate;
	Resource.Target = Target;
	Config.Resources.Add(Resource);
}

void ArcMassMechanicsTest::AddConstantFlowRule(UArcResourceConfig& Config, float UnitsPerSecond)
{
	FArcResourceRate_Constant Rate;
	Rate.UnitsPerSecond = FMath::Abs(UnitsPerSecond);
	const FInstancedStruct Target = UnitsPerSecond < 0.f
		? FInstancedStruct::Make(FArcResourceTarget_Min())
		: FInstancedStruct::Make(FArcResourceTarget_Max());
	AddFlowRule(Config, FInstancedStruct::Make(Rate), Target);
}

bool ArcMassMechanicsTest::FFlowHarness::Initialize(FActorTestSpawner& Spawner)
{
	LogScope.Emplace();
	Spawner.GetWorld();
	Spawner.InitializeGameSubsystems();
	World = &Spawner.GetWorld();

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return false;
	}
	EntityManager = &EntitySubsystem->GetMutableEntityManager();

	HandlerConfig = ArcMechTest::LoadTestAsset<UArcAttributeHandlerConfig>(TEXT("Handlers/AHC_Flow_Test"));
	if (!HandlerConfig)
	{
		return false;
	}

	Processor = NewObject<UArcResourceProcessor>();
	Processor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
	PendingProcessor = NewObject<UArcPendingAttributeOpsProcessor>();
	PendingProcessor->CallInitialize(EntityManager->GetOwner(), EntityManager->AsShared());
	return true;
}

UArcResourceConfig* ArcMassMechanicsTest::FFlowHarness::MakeConfig(float UpdateInterval) const
{
	UArcResourceConfig* Config = NewObject<UArcResourceConfig>();
	Config->UpdateInterval = UpdateInterval;
	return Config;
}

FMassEntityHandle ArcMassMechanicsTest::FFlowHarness::CreateEntity(UArcResourceConfig* Config, const FFlowEntityOptions& Options)
{
	FMassEntityTemplateData TemplateData;
	BuildTraitTemplate(Config, *World, TemplateData);

	FArcMechTest_FlowSet Set;
	Set.InitResource(Options.Resource);
	Set.InitGainMeta(0.f);
	Set.InitUnhandledMeta(0.f);

	UE::Mass::FEntityBuilder Builder(*EntityManager);
	Builder.Add<FArcMechTest_FlowSet>(Set)
		.Add<FArcAggregatorFragment>()
		.Add<FArcPendingAttributeOpsFragment>();

	for (const FInstancedStruct& Initial : TemplateData.GetInitialFragmentValues())
	{
		Builder.Add(Initial);
	}
	for (const FConstSharedStruct& Shared : TemplateData.GetSharedFragmentValues().GetConstSharedFragments())
	{
		if (Shared.GetScriptStruct() == FArcResourceConfigSharedFragment::StaticStruct())
		{
			Builder.Add<FArcResourceConfigSharedFragment>(Shared.Get<FArcResourceConfigSharedFragment>());
		}
	}

	if (Options.bHandlerConfig)
	{
		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = Options.HandlerConfigOverride ? Options.HandlerConfigOverride : HandlerConfig;
		Builder.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);
	}

	return Builder.Commit();
}

void ArcMassMechanicsTest::FFlowHarness::Tick(float DeltaSeconds, int32 Count)
{
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FMassProcessingContext ProcessingContext(*EntityManager, DeltaSeconds);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
		FMassProcessingContext PendingContext(*EntityManager, 0.f);
		UE::Mass::Executor::Run(*PendingProcessor, PendingContext);
	}
}

FArcMechTest_FlowSet* ArcMassMechanicsTest::FFlowHarness::GetSet(FMassEntityHandle Entity) const
{
	FMassEntityView EntityView(*EntityManager, Entity);
	return EntityView.GetSparseFragmentDataPtr<FArcMechTest_FlowSet>();
}

float ArcMassMechanicsTest::FFlowHarness::GetResource(FMassEntityHandle Entity) const
{
	const FArcMechTest_FlowSet* Set = GetSet(Entity);
	return Set ? Set->GetResource() : -1.f;
}
