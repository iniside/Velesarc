// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Components/ActorTestSpawner.h"
#include "Effects/ArcEffectSpec.h"
#include "Effects/ArcEffectDefinition.h"
#include "Effects/ArcEffectModifier.h"
#include "Effects/ArcEffectFunctions.h"
#include "Effects/ArcEffectTypes.h"
#include "Abilities/ArcAbilityDefinition.h"
#include "Abilities/ArcAbilityFunctions.h"
#include "Abilities/ArcAbilityHandle.h"
#include "Fragments/ArcAbilityCollectionFragment.h"
#include "Fragments/ArcAbilityInputFragment.h"
#include "Fragments/ArcAbilityCooldownFragment.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "Processors/ArcPendingAttributeOpsProcessor.h"
#include "Signals/ArcAbilitySignals.h"
#include "ArcScalableFloat.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityView.h"
#include "Mass/Executor.h"
#include "Mass/ProcessingContext.h"
#include "MassSignalSubsystem.h"
#include "StructUtils/SharedStruct.h"
#include "NativeGameplayTags.h"

namespace ArcRangeResolutionTestHelpers
{

FArcScalableFloat MakeInlineRange(float Min, float Max)
{
	FArcScalableFloat Sf;
	Sf.Source = EArcScalableFloatSource::InlineRange;
	Sf.MinValue = Min;
	Sf.MaxValue = Max;
	return Sf;
}

} // namespace ArcRangeResolutionTestHelpers

// ============================================================================
// A. ResolveScalableMagnitude — unit tests for the helper function
// ============================================================================

TEST_CLASS(ArcRangeResolution_ResolveScalableMagnitude, "ArcMassAbilities.RangeResolution.ResolveScalableMagnitude")
{
	TEST_METHOD(InlineRange_MinMode_ReturnsMin)
	{
		FArcScalableFloat Sf = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Min);
		ASSERT_THAT(IsNear(Result, 10.f, 0.001f));
	}

	TEST_METHOD(InlineRange_MaxMode_ReturnsMax)
	{
		FArcScalableFloat Sf = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Max);
		ASSERT_THAT(IsNear(Result, 20.f, 0.001f));
	}

	TEST_METHOD(InlineRange_MidMode_ReturnsMidpoint)
	{
		FArcScalableFloat Sf = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Mid);
		ASSERT_THAT(IsNear(Result, 15.f, 0.001f));
	}

	TEST_METHOD(InlineRange_RollMode_ValuesWithinBounds)
	{
		FArcScalableFloat Sf = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		for (int32 i = 0; i < 50; ++i)
		{
			float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Roll);
			ASSERT_THAT(IsTrue(Result >= 10.f && Result <= 20.f));
		}
	}

	TEST_METHOD(StaticFloat_MinMode_ReturnsValue)
	{
		FArcScalableFloat Sf(7.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Min);
		ASSERT_THAT(IsNear(Result, 7.f, 0.001f));
	}

	TEST_METHOD(StaticFloat_MaxMode_ReturnsValue)
	{
		FArcScalableFloat Sf(7.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Max);
		ASSERT_THAT(IsNear(Result, 7.f, 0.001f));
	}

	TEST_METHOD(StaticFloat_MidMode_ReturnsValue)
	{
		FArcScalableFloat Sf(7.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Mid);
		ASSERT_THAT(IsNear(Result, 7.f, 0.001f));
	}

	TEST_METHOD(StaticFloat_RollMode_ReturnsValue)
	{
		FArcScalableFloat Sf(7.f);
		float Result = ArcEffects::ResolveScalableMagnitude(Sf, EArcRangeResolution::Roll);
		ASSERT_THAT(IsNear(Result, 7.f, 0.001f));
	}
};

// ============================================================================
// B. Effect spec bake — RangeResolution honored during MakeSpec
// ============================================================================

TEST_CLASS(ArcRangeResolution_EffectSpecBake, "ArcMassAbilities.RangeResolution.EffectSpecBake")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogUObjectGlobals");
		FAutomationTestBase::SuppressedLogCategories.AddUnique("LogRHI");
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
	}

	TEST_METHOD(SimpleModifier_MinMode_ResolvesToMin)
	{
		UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Type = EArcModifierType::Simple;
		Mod.Magnitude = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		Mod.RangeResolution = EArcRangeResolution::Min;
		Def->Modifiers.Add(Mod);

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));
		ASSERT_THAT(AreEqual(Spec->ResolvedMagnitudes.Num(), 1));
		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[0], 10.f, 0.001f));
	}

	TEST_METHOD(SimpleModifier_MaxMode_ResolvesToMax)
	{
		UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Type = EArcModifierType::Simple;
		Mod.Magnitude = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		Mod.RangeResolution = EArcRangeResolution::Max;
		Def->Modifiers.Add(Mod);

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));
		ASSERT_THAT(AreEqual(Spec->ResolvedMagnitudes.Num(), 1));
		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[0], 20.f, 0.001f));
	}

	TEST_METHOD(SimpleModifier_MidMode_ResolvesToMidpoint)
	{
		UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();
		FArcEffectModifier Mod;
		Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
		Mod.Operation = EArcModifierOp::Add;
		Mod.Type = EArcModifierType::Simple;
		Mod.Magnitude = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);
		Mod.RangeResolution = EArcRangeResolution::Mid;
		Def->Modifiers.Add(Mod);

		FArcEffectContext Ctx;
		Ctx.EntityManager = EntityManager;
		FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
		const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
		ASSERT_THAT(IsNotNull(Spec));
		ASSERT_THAT(AreEqual(Spec->ResolvedMagnitudes.Num(), 1));
		ASSERT_THAT(IsNear(Spec->ResolvedMagnitudes[0], 15.f, 0.001f));
	}

	TEST_METHOD(SimpleModifier_RollMode_ResolvesWithinBounds)
	{
		// Roll re-rolls each MakeSpec call; run 50 specs and verify each resolved magnitude is in [10,20].
		FArcScalableFloat RangeSf = ArcRangeResolutionTestHelpers::MakeInlineRange(10.f, 20.f);

		for (int32 i = 0; i < 50; ++i)
		{
			UArcEffectDefinition* Def = NewObject<UArcEffectDefinition>();
			FArcEffectModifier Mod;
			Mod.Attribute = FArcAttributeSet_TestStats::GetHealthAttribute();
			Mod.Operation = EArcModifierOp::Add;
			Mod.Type = EArcModifierType::Simple;
			Mod.Magnitude = RangeSf;
			Mod.RangeResolution = EArcRangeResolution::Roll;
			Def->Modifiers.Add(Mod);

			FArcEffectContext Ctx;
			Ctx.EntityManager = EntityManager;
			FSharedStruct SharedSpec = ArcEffects::MakeSpec(Def, Ctx);
			const FArcEffectSpec* Spec = SharedSpec.GetPtr<FArcEffectSpec>();
			ASSERT_THAT(IsNotNull(Spec));
			ASSERT_THAT(AreEqual(Spec->ResolvedMagnitudes.Num(), 1));
			ASSERT_THAT(IsTrue(Spec->ResolvedMagnitudes[0] >= 10.f && Spec->ResolvedMagnitudes[0] <= 20.f));
		}
	}
};
