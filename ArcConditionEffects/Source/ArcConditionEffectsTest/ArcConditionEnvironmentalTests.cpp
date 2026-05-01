// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "ArcEnvironmentalConditionProcessor.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"

TEST_CLASS(ArcCondition_Environmental, "ArcConditionEffects.Environmental")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionEffectsSubsystem* Subsystem = nullptr;
    UArcEnvironmentalConditionProcessor* Processor = nullptr;

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
        Subsystem = Spawner.GetWorld().GetSubsystem<UArcConditionEffectsSubsystem>();

        TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
        Processor = NewObject<UArcEnvironmentalConditionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ApplyAndExecute(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
    {
        Subsystem->ApplyCondition(Entity, Type, Amount);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(Blinded_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Blinded, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Blinded].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Blinded].bActive));
    }

    TEST_METHOD(Suffocating_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Suffocating, 40.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(40.f, Frag->States[(int32)EArcConditionType::Suffocating].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Suffocating].bActive));
    }

    TEST_METHOD(Exhausted_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Exhausted, 25.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(25.f, Frag->States[(int32)EArcConditionType::Exhausted].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Exhausted].bActive));
    }

    TEST_METHOD(Corroded_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Corroded, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(20.f, Frag->States[(int32)EArcConditionType::Corroded].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Corroded].bActive));
    }

    TEST_METHOD(Shocked_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Shocked, 35.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(35.f, Frag->States[(int32)EArcConditionType::Shocked].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Shocked].bActive));
    }

    TEST_METHOD(Removal_ReducesSaturation)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Blinded].Saturation = 50.f;
            Frag->States[(int32)EArcConditionType::Blinded].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Blinded, -20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Blinded].Saturation, 0.5f));
    }

    TEST_METHOD(Removal_ClampsToZero)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Corroded].Saturation = 10.f;
            Frag->States[(int32)EArcConditionType::Corroded].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Corroded, -50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, Frag->States[(int32)EArcConditionType::Corroded].Saturation, 0.001f));
    }

    TEST_METHOD(Resistance_ReducesApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Shocked].Resistance = 0.4f;
        }

        ApplyAndExecute(Entity, EArcConditionType::Shocked, 50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Shocked].Saturation, 0.5f));
    }

    TEST_METHOD(Blinded_OverloadsAtHundred)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Blinded, 100.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(100.f, Frag->States[(int32)EArcConditionType::Blinded].Saturation, 0.001f));
        ASSERT_THAT(AreEqual(EArcConditionOverloadPhase::Overloaded, Frag->States[(int32)EArcConditionType::Blinded].OverloadPhase));
    }
};
