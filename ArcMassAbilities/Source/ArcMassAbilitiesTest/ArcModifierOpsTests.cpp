// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Attributes/ArcAggregator.h"
#include "Effects/ArcEffectTypes.h"
#include "Mass/EntityHandle.h"

// ============================================================================
// ArcModifierOps — EvaluateDefault formula tests for all operation types
// ============================================================================

TEST_CLASS(ArcModifierOps_EvaluateDefault, "ArcMassAbilities.ModifierOps")
{
	float Eval(float BaseValue, EArcModifierOp Op, float Magnitude)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod Mod;
		Mod.Magnitude = Magnitude;
		Mod.bQualified = true;
		Mods[static_cast<uint8>(Op)].Add(Mod);

		return FArcAggregationPolicy::EvaluateDefault(BaseValue, Mods);
	}

	TEST_METHOD(Add_IncreasesBase)
	{
		float Result = Eval(100.f, EArcModifierOp::Add, 25.f);
		// (100 + 25) * 1 / 1 * 1 + 0 = 125
		ASSERT_THAT(IsNear(125.f, Result, 0.001f));
	}

	TEST_METHOD(MultiplyAdditive_ScalesByOnePlusMagnitude)
	{
		float Result = Eval(100.f, EArcModifierOp::MultiplyAdditive, 0.5f);
		// 100 * (1 + 0.5) = 150
		ASSERT_THAT(IsNear(150.f, Result, 0.001f));
	}

	TEST_METHOD(DivideAdditive_DividesByOnePlusMagnitude)
	{
		float Result = Eval(100.f, EArcModifierOp::DivideAdditive, 1.f);
		// 100 / (1 + 1) = 50
		ASSERT_THAT(IsNear(50.f, Result, 0.001f));
	}

	TEST_METHOD(DivideAdditive_ZeroDivisor_Skipped)
	{
		float Result = Eval(100.f, EArcModifierOp::DivideAdditive, -1.f);
		// 1 + (-1) = 0, division skipped, result = 100 * 1 = 100
		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}

	TEST_METHOD(MultiplyCompound_MultipliesDirectly)
	{
		float Result = Eval(100.f, EArcModifierOp::MultiplyCompound, 2.f);
		// 100 * 2 = 200
		ASSERT_THAT(IsNear(200.f, Result, 0.001f));
	}

	TEST_METHOD(AddFinal_AddedAfterAllMultiplications)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod MulMod;
		MulMod.Magnitude = 1.f;
		MulMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(MulMod);

		FArcAggregatorMod FinalMod;
		FinalMod.Magnitude = 10.f;
		FinalMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::AddFinal)].Add(FinalMod);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// 100 * (1 + 1) * 1 + 10 = 210
		ASSERT_THAT(IsNear(210.f, Result, 0.001f));
	}

	TEST_METHOD(Override_ReplacesEverything)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod AddMod;
		AddMod.Magnitude = 50.f;
		AddMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddMod);

		FArcAggregatorMod OverrideMod;
		OverrideMod.Magnitude = 42.f;
		OverrideMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Override)].Add(OverrideMod);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		ASSERT_THAT(IsNear(42.f, Result, 0.001f));
	}

	TEST_METHOD(Override_LastOneWins)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod Ov1;
		Ov1.Magnitude = 10.f;
		Ov1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Override)].Add(Ov1);

		FArcAggregatorMod Ov2;
		Ov2.Magnitude = 77.f;
		Ov2.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Override)].Add(Ov2);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		ASSERT_THAT(IsNear(77.f, Result, 0.001f));
	}

	TEST_METHOD(FullFormula_AllOps)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod AddMod;
		AddMod.Magnitude = 20.f;
		AddMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddMod);

		FArcAggregatorMod MulAddMod;
		MulAddMod.Magnitude = 0.5f;
		MulAddMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(MulAddMod);

		FArcAggregatorMod DivAddMod;
		DivAddMod.Magnitude = 0.5f;
		DivAddMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::DivideAdditive)].Add(DivAddMod);

		FArcAggregatorMod MulCompMod;
		MulCompMod.Magnitude = 2.f;
		MulCompMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)].Add(MulCompMod);

		FArcAggregatorMod FinalMod;
		FinalMod.Magnitude = 5.f;
		FinalMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::AddFinal)].Add(FinalMod);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// (100 + 20) * (1 + 0.5) / (1 + 0.5) * 2 + 5
		// = 120 * 1.5 / 1.5 * 2 + 5
		// = 120 * 2 + 5 = 245
		ASSERT_THAT(IsNear(245.f, Result, 0.001f));
	}

	TEST_METHOD(MultiplyAdditive_TwoMods_Sum)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod Mod1;
		Mod1.Magnitude = 0.3f;
		Mod1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(Mod1);

		FArcAggregatorMod Mod2;
		Mod2.Magnitude = 0.2f;
		Mod2.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(Mod2);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// 100 * (1 + 0.3 + 0.2) = 100 * 1.5 = 150
		ASSERT_THAT(IsNear(150.f, Result, 0.001f));
	}

	TEST_METHOD(MultiplyCompound_TwoMods_Product)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod Mod1;
		Mod1.Magnitude = 2.f;
		Mod1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)].Add(Mod1);

		FArcAggregatorMod Mod2;
		Mod2.Magnitude = 3.f;
		Mod2.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyCompound)].Add(Mod2);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// 100 * 2 * 3 = 600
		ASSERT_THAT(IsNear(600.f, Result, 0.001f));
	}

	TEST_METHOD(UnqualifiedMod_Ignored)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod AddMod;
		AddMod.Magnitude = 50.f;
		AddMod.bQualified = false;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddMod);

		FArcAggregatorMod MulMod;
		MulMod.Magnitude = 0.5f;
		MulMod.bQualified = false;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(MulMod);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}

	TEST_METHOD(NoMods_ReturnsBase)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];
		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		ASSERT_THAT(IsNear(100.f, Result, 0.001f));
	}
};

// ============================================================================
// ArcModifierOps — Channel field tests
// ============================================================================

TEST_CLASS(ArcModifierOps_Channels, "ArcMassAbilities.ModifierOps.Channels")
{
	TEST_METHOD(Channel_StoredOnMod)
	{
		FArcAggregator Agg;
		FMassEntityHandle DummySource;

		FArcModifierHandle Handle = Agg.AddMod(EArcModifierOp::Add, 10.f, DummySource, nullptr, nullptr, 3);

		const TArray<FArcAggregatorMod>& AddMods = Agg.Mods[static_cast<uint8>(EArcModifierOp::Add)];
		ASSERT_THAT(AreEqual(1, AddMods.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(3), AddMods[0].Channel));
		ASSERT_THAT(IsTrue(AddMods[0].Handle == Handle));
	}

	TEST_METHOD(Channel_StoredOnMod_ExistingHandle)
	{
		FArcAggregator Agg;
		FMassEntityHandle DummySource;
		FArcModifierHandle ExistingHandle;
		ExistingHandle.Id = 12345;

		Agg.AddMod(EArcModifierOp::MultiplyAdditive, 0.5f, DummySource, nullptr, nullptr, ExistingHandle, 7);

		const TArray<FArcAggregatorMod>& MulMods = Agg.Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)];
		ASSERT_THAT(AreEqual(1, MulMods.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(7), MulMods[0].Channel));
		ASSERT_THAT(IsTrue(MulMods[0].Handle == ExistingHandle));
	}

	TEST_METHOD(Channel_DefaultsToZero)
	{
		FArcAggregator Agg;
		FMassEntityHandle DummySource;

		Agg.AddMod(EArcModifierOp::Add, 5.f, DummySource, nullptr, nullptr);

		const TArray<FArcAggregatorMod>& AddMods = Agg.Mods[static_cast<uint8>(EArcModifierOp::Add)];
		ASSERT_THAT(AreEqual(1, AddMods.Num()));
		ASSERT_THAT(AreEqual(static_cast<uint8>(0), AddMods[0].Channel));
	}

	TEST_METHOD(DefaultPolicy_IgnoresChannels)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod Mod1;
		Mod1.Magnitude = 10.f;
		Mod1.Channel = 1;
		Mod1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(Mod1);

		FArcAggregatorMod Mod2;
		Mod2.Magnitude = 20.f;
		Mod2.Channel = 2;
		Mod2.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(Mod2);

		FArcAggregatorMod Mod3;
		Mod3.Magnitude = 5.f;
		Mod3.Channel = 0;
		Mod3.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(Mod3);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// (100 + 10 + 20 + 5) = 135 — all channels summed regardless
		ASSERT_THAT(IsNear(135.f, Result, 0.001f));
	}
};
