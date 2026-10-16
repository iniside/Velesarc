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

	TEST_METHOD(DefaultPolicy_LayersChannels)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod AddCh0;
		AddCh0.Magnitude = 20.f;
		AddCh0.Channel = 0;
		AddCh0.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddCh0);

		FArcAggregatorMod MulCh1;
		MulCh1.Magnitude = 0.5f;
		MulCh1.Channel = 1;
		MulCh1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(MulCh1);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// Ch0: 100 + 20 = 120; Ch1: 120 * (1 + 0.5) = 180 — channel output feeds the next channel
		ASSERT_THAT(IsNear(180.f, Result, 0.001f));
	}

	TEST_METHOD(Override_OnChannel0_FeedsChannel1)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod OverrideCh0;
		OverrideCh0.Magnitude = 50.f;
		OverrideCh0.Channel = 0;
		OverrideCh0.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Override)].Add(OverrideCh0);

		FArcAggregatorMod AddCh1;
		AddCh1.Magnitude = 10.f;
		AddCh1.Channel = 1;
		AddCh1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddCh1);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// Ch0: override -> 50; Ch1: 50 + 10 = 60 — override wins its channel only
		ASSERT_THAT(IsNear(60.f, Result, 0.001f));
	}

	TEST_METHOD(AddFinal_AppliedPerChannel)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod FinalCh0;
		FinalCh0.Magnitude = 5.f;
		FinalCh0.Channel = 0;
		FinalCh0.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::AddFinal)].Add(FinalCh0);

		FArcAggregatorMod MulCh1;
		MulCh1.Magnitude = 2.f;
		MulCh1.Channel = 1;
		MulCh1.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::MultiplyAdditive)].Add(MulCh1);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// Ch0: 100 + AddFinal 5 = 105; Ch1: 105 * (1 + 2) = 315 — Ch0's AddFinal is inside Ch1's base
		ASSERT_THAT(IsNear(315.f, Result, 0.001f));
	}

	TEST_METHOD(Override_LastQualifyingWins_WithinChannel)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod First;
		First.Magnitude = 11.f;
		First.Channel = 0;
		First.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Override)].Add(First);

		FArcAggregatorMod Second;
		Second.Magnitude = 42.f;
		Second.Channel = 0;
		Second.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Override)].Add(Second);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// Arc semantics: LAST qualifying override wins (divergence from GAS first-wins)
		ASSERT_THAT(IsNear(42.f, Result, 0.001f));
	}

	TEST_METHOD(DivisorZero_TreatedAsOne)
	{
		TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];

		FArcAggregatorMod DivMod;
		DivMod.Magnitude = -1.f;
		DivMod.Channel = 0;
		DivMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::DivideAdditive)].Add(DivMod);

		FArcAggregatorMod AddMod;
		AddMod.Magnitude = 25.f;
		AddMod.Channel = 0;
		AddMod.bQualified = true;
		Mods[static_cast<uint8>(EArcModifierOp::Add)].Add(AddMod);

		float Result = FArcAggregationPolicy::EvaluateDefault(100.f, Mods);
		// Divisor = 1 + (-1) = 0 -> guarded to 1: (100 + 25) / 1 = 125
		ASSERT_THAT(IsNear(125.f, Result, 0.001f));
	}
};
