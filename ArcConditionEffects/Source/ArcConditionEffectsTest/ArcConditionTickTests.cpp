// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "ArcConditionTickProcessors.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"

// ============================================================================
// Tick Processor — tests decay, overload, burnout through actual processor execution
// ============================================================================

TEST_CLASS(ArcConditionTick_Burning, "ArcConditionEffects.Tick.Burning")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionTickProcessor* TickProcessor = nullptr;

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

        SignalSubsystem = Spawner.GetWorld().GetSubsystem<UMassSignalSubsystem>();
        check(SignalSubsystem);

        TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
        TickProcessor = NewObject<UArcConditionTickProcessor>();
        TickProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ExecuteTick(float DeltaTime)
    {
        FMassProcessingContext ProcessingContext(*EntityManager, DeltaTime);
        UE::Mass::Executor::Run(*TickProcessor, ProcessingContext);
    }

    TEST_METHOD(DecayReducesSaturation)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        Frag->States[(int32)EArcConditionType::Burning].Saturation = 50.f;
        Frag->States[(int32)EArcConditionType::Burning].bActive = true;

        ExecuteTick(1.f);

        FMassEntityView ViewAfter(*EntityManager, Entity);
        FArcConditionStatesFragment* FragAfter = ViewAfter.GetFragmentDataPtr<FArcConditionStatesFragment>();
        // Default burning decay = 3.0/sec, so 50 - 3 = 47
        ASSERT_THAT(IsNear(47.f, FragAfter->States[(int32)EArcConditionType::Burning].Saturation, 0.001f));
    }

    TEST_METHOD(NoDecayAtZeroSaturation)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        Frag->States[(int32)EArcConditionType::Burning].Saturation = 0.f;

        ExecuteTick(1.f);

        FMassEntityView ViewAfter(*EntityManager, Entity);
        FArcConditionStatesFragment* FragAfter = ViewAfter.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, FragAfter->States[(int32)EArcConditionType::Burning].Saturation, 0.001f));
    }

    TEST_METHOD(OverloadTransitionsToBurnout)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        Frag->States[(int32)EArcConditionType::Burning].Saturation = 100.f;
        Frag->States[(int32)EArcConditionType::Burning].bActive = true;
        Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        Frag->States[(int32)EArcConditionType::Burning].OverloadTimeRemaining = 0.5f;

        ExecuteTick(1.f);

        FMassEntityView ViewAfter(*EntityManager, Entity);
        FArcConditionStatesFragment* FragAfter = ViewAfter.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::Burnout, FragAfter->States[(int32)EArcConditionType::Burning].OverloadPhase));
    }

    TEST_METHOD(BurnoutEndsAndReturnsToNone)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        Frag->States[(int32)EArcConditionType::Burning].Saturation = 10.f;
        Frag->States[(int32)EArcConditionType::Burning].bActive = true;
        Frag->States[(int32)EArcConditionType::Burning].OverloadPhase = EArcConditionOverloadPhase::Burnout;
        Frag->States[(int32)EArcConditionType::Burning].OverloadTimeRemaining = 0.1f;

        ExecuteTick(1.f);

        FMassEntityView ViewAfter(*EntityManager, Entity);
        FArcConditionStatesFragment* FragAfter = ViewAfter.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::None, FragAfter->States[(int32)EArcConditionType::Burning].OverloadPhase));
    }

    TEST_METHOD(HysteresisDeactivatesOnlyAtZero)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        Frag->States[(int32)EArcConditionType::Burning].Saturation = 5.f;
        Frag->States[(int32)EArcConditionType::Burning].bActive = true;

        ExecuteTick(1.f);
        FMassEntityView V1(*EntityManager, Entity);
        ASSERT_THAT(IsTrue(V1.GetFragmentDataPtr<FArcConditionStatesFragment>()->States[(int32)EArcConditionType::Burning].bActive));

        ExecuteTick(1.f);
        FMassEntityView V2(*EntityManager, Entity);
        ASSERT_THAT(IsFalse(V2.GetFragmentDataPtr<FArcConditionStatesFragment>()->States[(int32)EArcConditionType::Burning].bActive));
    }
};

// ============================================================================
// Tick Processor — Group B (linear) via Oiled
// ============================================================================

TEST_CLASS(ArcConditionTick_Oiled, "ArcConditionEffects.Tick.Oiled")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UArcConditionTickProcessor* TickProcessor = nullptr;

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

        TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
        TickProcessor = NewObject<UArcConditionTickProcessor>();
        TickProcessor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ExecuteTick(float DeltaTime)
    {
        FMassProcessingContext ProcessingContext(*EntityManager, DeltaTime);
        UE::Mass::Executor::Run(*TickProcessor, ProcessingContext);
    }

    TEST_METHOD(LinearDeactivatesImmediatelyAtZero)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        Frag->States[(int32)EArcConditionType::Oiled].Saturation = 0.5f;
        Frag->States[(int32)EArcConditionType::Oiled].bActive = true;

        ExecuteTick(1.f);

        FMassEntityView ViewAfter(*EntityManager, Entity);
        FArcConditionStatesFragment* FragAfter = ViewAfter.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, FragAfter->States[(int32)EArcConditionType::Oiled].Saturation, 0.001f));
        ASSERT_THAT(IsFalse(FragAfter->States[(int32)EArcConditionType::Oiled].bActive));
    }
};
