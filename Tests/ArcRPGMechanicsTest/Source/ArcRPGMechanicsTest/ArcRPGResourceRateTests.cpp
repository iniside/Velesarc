// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcRPGMechTestSupport.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "Resources/ArcRPGResourceRate_AttributeModifier.h"
#include "Attributes/ArcAttributeSet_RPGCoreStats.h"
#include "Attributes/ArcAttributeSet_RPGEntropy.h"
#include "Components/ActorTestSpawner.h"
#include "Mass/EntityBuilder.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"

namespace ArcRPGResourceRateTestsPrivate
{
	FArcAttributeSet_RPGEntropy MakeEntropy(float Entropy)
	{
		FArcAttributeSet_RPGEntropy Set;
		Set.InitEntropy(Entropy);
		Set.InitMaxEntropy(1000.f);
		return Set;
	}

	FArcAttributeSet_RPGCoreStats MakeCoreStats(float Willpower)
	{
		FArcAttributeSet_RPGCoreStats Set;
		Set.InitWillpower(Willpower);
		return Set;
	}
}

TEST_CLASS(ArcRPGEntropyRegenRate, "ArcRPGMechanics.Resources.EntropyRegen")
{
	TOptional<ArcRPGMechTest::FVerboseLogScope> LogScope;
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcResourceConfig* Config = nullptr;
	const FArcRPGResourceRate_AttributeModifier* EntropyRegen = nullptr;

	BEFORE_EACH()
	{
		LogScope.Emplace();
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");

		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* EntitySubsystem = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(EntitySubsystem);
		EntityManager = &EntitySubsystem->GetMutableEntityManager();
		Config = ArcRPGMechTest::LoadTestAsset<UArcResourceConfig>(TEXT("Resources/DA_EntropyResources_Test"));
		ASSERT_THAT(IsNotNull(Config));
		ASSERT_THAT(AreEqual(1, Config->Resources.Num()));
		EntropyRegen = Config->Resources[0].Rate.GetPtr<FArcRPGResourceRate_AttributeModifier>();
		ASSERT_THAT(IsNotNull(EntropyRegen));
	}

	FMassEntityHandle CreateEntity(float Entropy, float Willpower)
	{
		UE::Mass::FEntityBuilder Builder(*EntityManager);
		Builder.Add<FArcAttributeSet_RPGEntropy>(ArcRPGResourceRateTestsPrivate::MakeEntropy(Entropy))
			.Add<FArcAttributeSet_RPGCoreStats>(ArcRPGResourceRateTestsPrivate::MakeCoreStats(Willpower));
		return Builder.Commit();
	}

	float Evaluate(const FArcRPGResourceRate_AttributeModifier& Rate, FMassEntityHandle Entity)
	{
		FArcResourceContext Context(*EntityManager, Config->Resources[0]);
		Context.Entity = Entity;
		return Rate.GetRate(Context);
	}

	TEST_METHOD(Rate_IsBaseTimesOnePlusScaleTimesWillpowerModifier)
	{
		const FArcRPGResourceRate_AttributeModifier& Defaults = *EntropyRegen;

		ASSERT_THAT(IsNear(3.f, Evaluate(Defaults, CreateEntity(50.f, 10.f)), 0.0001f));
		ASSERT_THAT(IsNear(3.9f, Evaluate(Defaults, CreateEntity(50.f, 16.f)), 0.0001f));
		ASSERT_THAT(IsNear(2.1f, Evaluate(Defaults, CreateEntity(50.f, 5.f)), 0.0001f));

		FArcRPGResourceRate_AttributeModifier Tuned = *EntropyRegen;
		Tuned.BasePerSecond = 2.f;
		Tuned.ModifierScale = 0.5f;
		ASSERT_THAT(IsNear(4.f, Evaluate(Tuned, CreateEntity(50.f, 14.f)), 0.0001f));
	}

	TEST_METHOD(Rate_IsZeroWhenTheAttributeIsMissingOrUnset)
	{
		UE::Mass::FEntityBuilder EntropyOnly(*EntityManager);
		EntropyOnly.Add<FArcAttributeSet_RPGEntropy>(ArcRPGResourceRateTestsPrivate::MakeEntropy(50.f));
		ASSERT_THAT(IsTrue(Evaluate(*EntropyRegen, EntropyOnly.Commit()) == 0.f));

		const FArcRPGResourceRate_AttributeModifier Unset;
		ASSERT_THAT(IsTrue(Evaluate(Unset, CreateEntity(50.f, 16.f)) == 0.f));
	}
};
