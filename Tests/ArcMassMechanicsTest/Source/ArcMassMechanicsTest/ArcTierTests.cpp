// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassMechanicsTestTypes.h"
#include "ArcMechTestSupport.h"
#include "ArcMassMechanics/Tiers/ArcTierTypes.h"
#include "Effects/ArcEffectDefinition.h"

namespace ArcTierTestsPrivate
{
	struct FTierCase
	{
		float Magnitude;
		int32 Tier;
	};

	FArcTierBands MakeBands(std::initializer_list<float> Thresholds)
	{
		FArcTierBands Bands;
		Bands.Thresholds = TArray<float>(Thresholds);
		return Bands;
	}

	UArcTierEffectTable* LoadSignedTable()
	{
		return ArcMechTest::LoadTestAsset<UArcTierEffectTable>(TEXT("Tiers/DA_TierTable_Signed"));
	}
}

TEST_CLASS(ArcTierBands, "ArcMassMechanics.Tiers.Bands")
{
	TOptional<ArcMechTest::FVerboseLogScope> LogScope;

	BEFORE_EACH()
	{
		LogScope.Emplace();
	}

	TEST_METHOD(GetTier_IsTheOneBasedIndexOfTheHighestReachedThreshold)
	{
		const FArcTierBands Bands = ArcTierTestsPrivate::MakeBands({25.f, 50.f, 75.f});
		const ArcTierTestsPrivate::FTierCase Cases[] = {
			{-10.f, 0}, {0.f, 0}, {24.99f, 0}, {25.f, 1}, {49.99f, 1}, {50.f, 2}, {74.99f, 2}, {75.f, 3}, {1000.f, 3}
		};
		for (const ArcTierTestsPrivate::FTierCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(Case.Tier, Bands.GetTier(Case.Magnitude)));
		}
	}

	TEST_METHOD(GetTier_WithNegativeThresholds_CountsFromTheFirst)
	{
		const FArcTierBands Bands = ArcTierTestsPrivate::MakeBands({-10.f, 0.f, 10.f});
		const ArcTierTestsPrivate::FTierCase Cases[] = {
			{-20.f, 0}, {-10.f, 1}, {-0.01f, 1}, {0.f, 2}, {9.99f, 2}, {10.f, 3}
		};
		for (const ArcTierTestsPrivate::FTierCase& Case : Cases)
		{
			ASSERT_THAT(AreEqual(Case.Tier, Bands.GetTier(Case.Magnitude)));
		}
	}

	TEST_METHOD(GetTier_WithNoThresholds_IsAlwaysZero)
	{
		const FArcTierBands Bands;
		ASSERT_THAT(AreEqual(0, Bands.GetTier(-1000.f)));
		ASSERT_THAT(AreEqual(0, Bands.GetTier(0.f)));
		ASSERT_THAT(AreEqual(0, Bands.GetTier(1000.f)));
	}

	TEST_METHOD(FindEffect_MatchesKeyAndSignedTier)
	{
		UArcTierEffectTable* Table = ArcTierTestsPrivate::LoadSignedTable();
		ASSERT_THAT(IsNotNull(Table));
		const FGameplayTag KeyA = ArcMassMechanicsTestTags::Key_A.GetTag();

		UArcEffectDefinition* Pos2 = Table->FindEffect(KeyA, 2);
		UArcEffectDefinition* Neg2 = Table->FindEffect(KeyA, -2);
		ASSERT_THAT(IsNotNull(Pos2));
		ASSERT_THAT(IsNotNull(Neg2));
		ASSERT_THAT(IsTrue(Pos2 != Neg2));
		ASSERT_THAT(IsTrue(Pos2 == Table->Entries[1].Effect));
		ASSERT_THAT(IsTrue(Neg2 == Table->Entries[4].Effect));

		ASSERT_THAT(IsNull(Table->FindEffect(KeyA, 0)));
		ASSERT_THAT(IsNull(Table->FindEffect(KeyA, 4)));
		ASSERT_THAT(IsNull(Table->FindEffect(ArcMassMechanicsTestTags::Key_B.GetTag(), 2)));
		ASSERT_THAT(IsNull(Table->FindEffect(FGameplayTag(), 2)));
	}

	TEST_METHOD(FindEffect_DistinguishesKeysAtTheSameTier)
	{
		UArcTierEffectTable* Table = ArcTierTestsPrivate::LoadSignedTable();
		ASSERT_THAT(IsNotNull(Table));

		ASSERT_THAT(AreEqual(7, Table->Entries.Num()));
		ASSERT_THAT(IsNotNull(Table->FindEffect(ArcMassMechanicsTestTags::Key_B.GetTag(), 1)));
		ASSERT_THAT(IsTrue(Table->FindEffect(ArcMassMechanicsTestTags::Key_B.GetTag(), 1) == Table->Entries[6].Effect));
		ASSERT_THAT(IsTrue(Table->FindEffect(ArcMassMechanicsTestTags::Key_A.GetTag(), 1) == Table->Entries[0].Effect));
	}
};
