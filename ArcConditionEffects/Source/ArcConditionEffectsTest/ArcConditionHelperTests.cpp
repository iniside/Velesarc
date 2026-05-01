// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionTypes.h"

// ============================================================================
// SetSaturationClamped
// ============================================================================

TEST_CLASS(ArcConditionHelpers_SetSaturationClamped, "ArcConditionEffects.Helpers.SetSaturationClamped")
{
    FArcConditionState State;
    FArcConditionConfig Config;

    BEFORE_EACH()
    {
        State = FArcConditionState();
        Config = FArcConditionConfig();
        Config.Group = EArcConditionGroup::GroupA_Hysteresis;
        Config.OverloadDuration = 6.f;
    }

    TEST_METHOD(ClampsToZero_WhenNegative)
    {
        ArcConditionHelpers::SetSaturationClamped(State, Config, -10.f);
        ASSERT_THAT(IsNear(0.f, State.Saturation, 0.001f));
    }

    TEST_METHOD(ClampsTo100_WhenOver)
    {
        ArcConditionHelpers::SetSaturationClamped(State, Config, 150.f);
        ASSERT_THAT(IsNear(100.f, State.Saturation, 0.001f));
    }

    TEST_METHOD(SetsExactValue_WhenInRange)
    {
        ArcConditionHelpers::SetSaturationClamped(State, Config, 42.5f);
        ASSERT_THAT(IsNear(42.5f, State.Saturation, 0.001f));
    }

    TEST_METHOD(TriggersOverload_AtHundred)
    {
        ArcConditionHelpers::SetSaturationClamped(State, Config, 100.f);
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::Overloaded, State.OverloadPhase));
        ASSERT_THAT(IsNear(6.f, State.OverloadTimeRemaining, 0.001f));
    }

    TEST_METHOD(NoOverload_WhenAlreadyOverloaded)
    {
        State.OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        State.OverloadTimeRemaining = 3.f;
        ArcConditionHelpers::SetSaturationClamped(State, Config, 100.f);
        ASSERT_THAT(IsNear(3.f, State.OverloadTimeRemaining, 0.001f));
    }

    TEST_METHOD(NoOverload_WhenDurationIsZero)
    {
        Config.OverloadDuration = 0.f;
        ArcConditionHelpers::SetSaturationClamped(State, Config, 100.f);
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::None, State.OverloadPhase));
    }
};

// ============================================================================
// UpdateActiveFlag
// ============================================================================

TEST_CLASS(ArcConditionHelpers_UpdateActiveFlag, "ArcConditionEffects.Helpers.UpdateActiveFlag")
{
    FArcConditionState State;
    FArcConditionConfig Config;

    BEFORE_EACH()
    {
        State = FArcConditionState();
        Config = FArcConditionConfig();
    }

    TEST_METHOD(GroupA_ActivatesAtThreshold)
    {
        Config.Group = EArcConditionGroup::GroupA_Hysteresis;
        Config.ActivationThreshold = 20.f;
        State.Saturation = 20.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsTrue(State.bActive));
    }

    TEST_METHOD(GroupA_StaysActiveAboveZero)
    {
        Config.Group = EArcConditionGroup::GroupA_Hysteresis;
        Config.ActivationThreshold = 20.f;
        State.bActive = true;
        State.Saturation = 5.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsTrue(State.bActive));
    }

    TEST_METHOD(GroupA_DeactivatesAtZero)
    {
        Config.Group = EArcConditionGroup::GroupA_Hysteresis;
        Config.ActivationThreshold = 20.f;
        State.bActive = true;
        State.Saturation = 0.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsFalse(State.bActive));
    }

    TEST_METHOD(GroupA_DoesNotActivateBelowThreshold)
    {
        Config.Group = EArcConditionGroup::GroupA_Hysteresis;
        Config.ActivationThreshold = 20.f;
        State.Saturation = 15.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsFalse(State.bActive));
    }

    TEST_METHOD(GroupB_ActiveWhenAboveZero)
    {
        Config.Group = EArcConditionGroup::GroupB_Linear;
        State.Saturation = 0.01f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsTrue(State.bActive));
    }

    TEST_METHOD(GroupB_InactiveAtZero)
    {
        Config.Group = EArcConditionGroup::GroupB_Linear;
        State.Saturation = 0.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsFalse(State.bActive));
    }

    TEST_METHOD(GroupC_ActiveWhenAboveZero)
    {
        Config.Group = EArcConditionGroup::GroupC_Environmental;
        State.Saturation = 1.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsTrue(State.bActive));
    }

    TEST_METHOD(GroupC_InactiveAtZero)
    {
        Config.Group = EArcConditionGroup::GroupC_Environmental;
        State.Saturation = 0.f;
        ArcConditionHelpers::UpdateActiveFlag(State, Config);
        ASSERT_THAT(IsFalse(State.bActive));
    }
};

// ============================================================================
// ClearCondition
// ============================================================================

TEST_CLASS(ArcConditionHelpers_ClearCondition, "ArcConditionEffects.Helpers.ClearCondition")
{
    TEST_METHOD(ResetsAllState)
    {
        FArcConditionState State;
        State.Saturation = 75.f;
        State.bActive = true;
        State.OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        State.OverloadTimeRemaining = 3.f;
        State.Resistance = 0.5f;

        ArcConditionHelpers::ClearCondition(State);

        ASSERT_THAT(IsNear(0.f, State.Saturation, 0.001f));
        ASSERT_THAT(IsFalse(State.bActive));
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::None, State.OverloadPhase));
        ASSERT_THAT(IsNear(0.f, State.OverloadTimeRemaining, 0.001f));
    }

    TEST_METHOD(PreservesResistance)
    {
        FArcConditionState State;
        State.Resistance = 0.75f;
        State.Saturation = 50.f;
        ArcConditionHelpers::ClearCondition(State);
        ASSERT_THAT(IsNear(0.75f, State.Resistance, 0.001f));
    }
};

// ============================================================================
// ApplyResistance
// ============================================================================

TEST_CLASS(ArcConditionHelpers_ApplyResistance, "ArcConditionEffects.Helpers.ApplyResistance")
{
    TEST_METHOD(ReducesPositiveAmount)
    {
        float Result = ArcConditionHelpers::ApplyResistance(100.f, 0.5f);
        ASSERT_THAT(IsNear(50.f, Result, 0.001f));
    }

    TEST_METHOD(FullResistance_ZerosAmount)
    {
        float Result = ArcConditionHelpers::ApplyResistance(100.f, 1.0f);
        ASSERT_THAT(IsNear(0.f, Result, 0.001f));
    }

    TEST_METHOD(ZeroResistance_PassesThrough)
    {
        float Result = ArcConditionHelpers::ApplyResistance(100.f, 0.f);
        ASSERT_THAT(IsNear(100.f, Result, 0.001f));
    }

    TEST_METHOD(NegativeAmount_BypassesResistance)
    {
        float Result = ArcConditionHelpers::ApplyResistance(-30.f, 0.5f);
        ASSERT_THAT(IsNear(-30.f, Result, 0.001f));
    }

    TEST_METHOD(ClampsResistanceAboveOne)
    {
        float Result = ArcConditionHelpers::ApplyResistance(100.f, 1.5f);
        ASSERT_THAT(IsNear(0.f, Result, 0.001f));
    }
};

// ============================================================================
// TickCondition
// ============================================================================

TEST_CLASS(ArcConditionHelpers_TickCondition, "ArcConditionEffects.Helpers.TickCondition")
{
    FArcConditionState State;
    FArcConditionConfig Config;
    bool bStateChanged = false;
    bool bOverloadChanged = false;

    BEFORE_EACH()
    {
        State = FArcConditionState();
        Config = FArcConditionConfig();
        Config.Group = EArcConditionGroup::GroupA_Hysteresis;
        Config.ActivationThreshold = 20.f;
        Config.DecayRate = 10.f;
        Config.OverloadDuration = 5.f;
        Config.BurnoutDuration = 2.f;
        Config.BurnoutDecayMultiplier = 5.f;
        Config.BurnoutTargetSaturation = 0.f;
        bStateChanged = false;
        bOverloadChanged = false;
    }

    TEST_METHOD(NormalDecay_ReducesSaturation)
    {
        State.Saturation = 50.f;
        State.bActive = true;
        ArcConditionHelpers::TickCondition(State, Config, 1.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(IsNear(40.f, State.Saturation, 0.001f));
        ASSERT_THAT(IsFalse(bStateChanged));
    }

    TEST_METHOD(NormalDecay_ClampsToZero)
    {
        State.Saturation = 5.f;
        State.bActive = true;
        ArcConditionHelpers::TickCondition(State, Config, 1.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(IsNear(0.f, State.Saturation, 0.001f));
    }

    TEST_METHOD(NormalDecay_DeactivatesAtZero_SignalsStateChange)
    {
        State.Saturation = 5.f;
        State.bActive = true;
        ArcConditionHelpers::TickCondition(State, Config, 1.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(IsFalse(State.bActive));
        ASSERT_THAT(IsTrue(bStateChanged));
    }

    TEST_METHOD(NoTick_WhenSaturationIsZero)
    {
        State.Saturation = 0.f;
        State.bActive = false;
        ArcConditionHelpers::TickCondition(State, Config, 1.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(IsNear(0.f, State.Saturation, 0.001f));
        ASSERT_THAT(IsFalse(bStateChanged));
    }

    TEST_METHOD(Overloaded_TimerCountsDown)
    {
        State.Saturation = 100.f;
        State.bActive = true;
        State.OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        State.OverloadTimeRemaining = 5.f;
        ArcConditionHelpers::TickCondition(State, Config, 2.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::Overloaded, State.OverloadPhase));
        ASSERT_THAT(IsNear(3.f, State.OverloadTimeRemaining, 0.001f));
        ASSERT_THAT(IsNear(100.f, State.Saturation, 0.001f));
    }

    TEST_METHOD(Overloaded_TransitionsToBurnout)
    {
        State.Saturation = 100.f;
        State.bActive = true;
        State.OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        State.OverloadTimeRemaining = 1.f;
        ArcConditionHelpers::TickCondition(State, Config, 2.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::Burnout, State.OverloadPhase));
        ASSERT_THAT(IsNear(Config.BurnoutDuration, State.OverloadTimeRemaining, 0.001f));
        ASSERT_THAT(IsTrue(bOverloadChanged));
    }

    TEST_METHOD(Burnout_DecaysAtMultipliedRate)
    {
        State.Saturation = 100.f;
        State.bActive = true;
        State.OverloadPhase = EArcConditionOverloadPhase::Burnout;
        State.OverloadTimeRemaining = 2.f;
        ArcConditionHelpers::TickCondition(State, Config, 0.5f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(IsNear(75.f, State.Saturation, 0.001f));
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::Burnout, State.OverloadPhase));
    }

    TEST_METHOD(Burnout_EndsWhenTimerExpires)
    {
        State.Saturation = 80.f;
        State.bActive = true;
        State.OverloadPhase = EArcConditionOverloadPhase::Burnout;
        State.OverloadTimeRemaining = 0.5f;
        Config.BurnoutTargetSaturation = 10.f;
        ArcConditionHelpers::TickCondition(State, Config, 1.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::None, State.OverloadPhase));
        ASSERT_THAT(IsNear(10.f, State.Saturation, 0.001f));
        ASSERT_THAT(IsTrue(bOverloadChanged));
    }

    TEST_METHOD(Burnout_EndsWhenReachesTargetSaturation)
    {
        State.Saturation = 15.f;
        State.bActive = true;
        State.OverloadPhase = EArcConditionOverloadPhase::Burnout;
        State.OverloadTimeRemaining = 10.f;
        Config.BurnoutTargetSaturation = 10.f;
        ArcConditionHelpers::TickCondition(State, Config, 1.f, bStateChanged, bOverloadChanged);
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::None, State.OverloadPhase));
        ASSERT_THAT(IsNear(10.f, State.Saturation, 0.001f));
    }
};
