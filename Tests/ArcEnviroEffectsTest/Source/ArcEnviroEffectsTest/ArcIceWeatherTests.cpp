// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcEnviroEffectsTestHarness.h"
#include "ArcIceWeatherTestTypes.h"
#include "ArcMassMechanics/ArcMassMechanicsTags.h"
#include "ArcWeather/ArcIceWeatherProcessor.h"
#include "ArcWeather/ArcWeatherSubsystem.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcResourceAttributeHandlers.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectModifier.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityFragments.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassEntityTemplate.h"
#include "MassEntityTemplateRegistry.h"
#include "StructUtils/InstancedStruct.h"

namespace ArcIceWeatherTestsPrivate
{
	constexpr float StartIntegrity = 100.f;

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

	UArcEffectDefinition* MakeSetByCallerEffect(const FArcAttributeRef& Meta, const FGameplayTag& SetByCallerTag)
	{
		FArcEffectModifier Modifier;
		Modifier.Attribute = Meta;
		Modifier.Operation = EArcModifierOp::Add;
		Modifier.Type = EArcModifierType::SetByCaller;
		Modifier.SetByCallerTag = SetByCallerTag;

		FArcEffectExecutionEntry Entry;
		Entry.Modifiers.Add(Modifier);

		UArcEffectDefinition* Effect = NewObject<UArcEffectDefinition>();
		Effect->StackingPolicy.DurationType = EArcEffectDuration::Instant;
		Effect->Executions.Add(Entry);
		Effect->BuildCaptureLayout();
		return Effect;
	}

	void AddMetaHandler(UArcAttributeHandlerConfig& Config, const FArcAttributeRef& Meta, EArcResourceOp Op)
	{
		FArcAttributeHandler_MetaToResource Handler;
		Handler.Resource = FArcIceTest_Set::GetIntegrityAttribute();
		Handler.MinConst = 0.f;
		Handler.MaxConst = 1000.f;
		Handler.Op = Op;
		Config.Handlers.Add(Meta, FInstancedStruct::Make(Handler));
	}
}

TEST_CLASS(ArcIceWeather, "ArcWeather.Ice")
{
	FActorTestSpawner Spawner;
	ArcEnviroTest::FHarness Harness;
	UArcIceWeatherProcessor* Processor = nullptr;
	UArcAttributeHandlerConfig* IceHandlers = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		const bool bInitialized = Harness.Initialize(Spawner);
		check(bInitialized);

		Processor = NewObject<UArcIceWeatherProcessor>();
		Processor->CallInitialize(Harness.EntityManager->GetOwner(), Harness.EntityManager->AsShared());

		IceHandlers = NewObject<UArcAttributeHandlerConfig>();
		ArcIceWeatherTestsPrivate::AddMetaHandler(*IceHandlers, FArcIceTest_Set::GetDamageMetaAttribute(), EArcResourceOp::Subtract);
		ArcIceWeatherTestsPrivate::AddMetaHandler(*IceHandlers, FArcIceTest_Set::GetHealMetaAttribute(), EArcResourceOp::Add);
	}

	FMassEntityHandle CreateIce(float TemperatureResistance, float ThermalMass, bool bHealEffect = true)
	{
		using namespace ArcIceWeatherTestsPrivate;

		UArcIceTrait* Trait = NewObject<UArcIceTrait>();
		Trait->IceConfig.TemperatureResistance = TemperatureResistance;
		Trait->IceConfig.ThermalMass = ThermalMass;
		Trait->DamageEffect = MakeSetByCallerEffect(FArcIceTest_Set::GetDamageMetaAttribute(), ArcMassMechanicsTags::SBC_TrueDamage);
		Trait->HealEffect = bHealEffect ? MakeSetByCallerEffect(FArcIceTest_Set::GetHealMetaAttribute(), ArcMassMechanicsTags::SBC_TrueHeal) : nullptr;

		FMassEntityTemplateData TemplateData;
		FTraitBuildContext BuildContext(TemplateData);
		BuildContext.BeginTrait(Trait);
		Trait->BuildTemplate(BuildContext, *Harness.World);

		FArcIceTest_Set Set;
		Set.InitIntegrity(StartIntegrity);
		Set.InitDamageMeta(0.f);
		Set.InitHealMeta(0.f);

		FArcAttributeHandlerSharedFragment HandlerShared;
		HandlerShared.Config = IceHandlers;

		UE::Mass::FEntityBuilder Builder(*Harness.EntityManager);
		Builder.Add<FArcIceTest_Set>(Set)
			.Add<FTransformFragment>()
			.Add<FArcEffectStackFragment>()
			.Add<FArcAggregatorFragment>()
			.Add<FArcOwnedTagsFragment>()
			.Add<FArcPendingAttributeOpsFragment>()
			.Add<FArcAttributeHandlerSharedFragment>(HandlerShared);

		for (const FInstancedStruct& Initial : TemplateData.GetInitialFragmentValues())
		{
			Builder.Add(Initial);
		}
		for (const FConstSharedStruct& Shared : TemplateData.GetSharedFragmentValues().GetConstSharedFragments())
		{
			if (Shared.GetScriptStruct() == FArcIceEffectsSharedFragment::StaticStruct())
			{
				Builder.Add<FArcIceEffectsSharedFragment>(Shared.Get<FArcIceEffectsSharedFragment>());
			}
		}
		return Builder.Commit();
	}

	void RunIce(float DeltaSeconds)
	{
		FMassProcessingContext ProcessingContext(*Harness.EntityManager, DeltaSeconds);
		UE::Mass::Executor::Run(*Processor, ProcessingContext);
	}

	const FArcIceTest_Set* GetSet(FMassEntityHandle Entity) const
	{
		FMassEntityView View(*Harness.EntityManager, Entity);
		return View.GetSparseFragmentDataPtr<FArcIceTest_Set>();
	}

	float WorldTemperature() const
	{
		return Harness.Weather->GetWeatherAtLocation(FVector::ZeroVector).Temperature;
	}

	TEST_METHOD(Melting_AppliesTheDamageEffectAsTrueDamage_ThroughTheMeta)
	{
		Harness.Weather->DefaultClimate.Temperature = 12.f;
		const float Expected = (WorldTemperature() - 2.f) / 4.f * 1.f;
		ASSERT_THAT(IsTrue(Expected > 0.f));

		const FMassEntityHandle Ice = CreateIce(2.f, 4.f);
		RunIce(1.f);

		ASSERT_THAT(IsNear(ArcIceWeatherTestsPrivate::StartIntegrity, GetSet(Ice)->GetIntegrity(), 0.0001f));

		Harness.DrainPendingOps();

		const FArcIceTest_Set* Set = GetSet(Ice);
		ASSERT_THAT(IsNear(ArcIceWeatherTestsPrivate::StartIntegrity - Expected, Set->GetIntegrity(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->DamageMeta.BaseValue, 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->HealMeta.BaseValue, 0.001f));
	}

	TEST_METHOD(Freezing_AppliesTheHealEffectAsTrueHeal_ThroughTheMeta)
	{
		Harness.Weather->DefaultClimate.Temperature = -10.f;
		const float Expected = (2.f - WorldTemperature()) / 4.f * 2.f;
		ASSERT_THAT(IsTrue(Expected > 0.f));

		const FMassEntityHandle Ice = CreateIce(2.f, 4.f);
		RunIce(2.f);

		ASSERT_THAT(IsNear(ArcIceWeatherTestsPrivate::StartIntegrity, GetSet(Ice)->GetIntegrity(), 0.0001f));

		Harness.DrainPendingOps();

		const FArcIceTest_Set* Set = GetSet(Ice);
		ASSERT_THAT(IsNear(ArcIceWeatherTestsPrivate::StartIntegrity + Expected, Set->GetIntegrity(), 0.001f));
		ASSERT_THAT(IsNear(0.f, Set->HealMeta.BaseValue, 0.001f));
	}

	TEST_METHOD(Freezing_WithoutAHealEffect_ChangesNothing)
	{
		Harness.Weather->DefaultClimate.Temperature = -10.f;

		const FMassEntityHandle Ice = CreateIce(2.f, 4.f, false);
		RunIce(1.f);
		Harness.DrainPendingOps();

		ASSERT_THAT(IsNear(ArcIceWeatherTestsPrivate::StartIntegrity, GetSet(Ice)->GetIntegrity(), 0.0001f));
	}

	TEST_METHOD(AtTheResistanceTemperature_ChangesNothing)
	{
		Harness.Weather->DefaultClimate.Temperature = 2.f;

		const FMassEntityHandle Ice = CreateIce(WorldTemperature(), 4.f);
		RunIce(1.f);
		Harness.DrainPendingOps();

		ASSERT_THAT(IsNear(ArcIceWeatherTestsPrivate::StartIntegrity, GetSet(Ice)->GetIntegrity(), 0.0001f));
	}
};
