// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "ArcBiologicalConditionProcessor.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"

TEST_CLASS(ArcCondition_Biological, "ArcConditionEffects.Biological")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionEffectsSubsystem* Subsystem = nullptr;
    UArcBiologicalConditionProcessor* Processor = nullptr;

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
        Processor = NewObject<UArcBiologicalConditionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ApplyAndExecute(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
    {
        Subsystem->ApplyCondition(Entity, Type, Amount);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(Bleeding_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Bleeding, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Bleeding].bActive));
    }

    TEST_METHOD(Bleeding_BlockedByActiveBurning)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
            Frag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Bleeding, 50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.001f));
    }

    TEST_METHOD(Bleeding_BlockedByFrozen)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Chilled].Saturation = 100.f;
            Frag->States[(int32)EArcConditionType::Chilled].bActive = true;
            Frag->States[(int32)EArcConditionType::Chilled].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        }

        ApplyAndExecute(Entity, EArcConditionType::Bleeding, 50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.001f));
    }

    TEST_METHOD(Bleeding_NotBlockedByInactiveBurning)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Saturation = 10.f;
            Frag->States[(int32)EArcConditionType::Burning].bActive = false;
        }

        ApplyAndExecute(Entity, EArcConditionType::Bleeding, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.5f));
    }

    TEST_METHOD(Poison_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Poisoned, 25.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(25.f, Frag->States[(int32)EArcConditionType::Poisoned].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Poisoned].bActive));
    }

    TEST_METHOD(Poison_BoostedByActiveBleeding)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 30.f;
            Frag->States[(int32)EArcConditionType::Bleeding].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Poisoned, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Poisoned].Saturation, 0.5f));
    }

    TEST_METHOD(Poison_NotBoostedByInactiveBleeding)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 10.f;
            Frag->States[(int32)EArcConditionType::Bleeding].bActive = false;
        }

        ApplyAndExecute(Entity, EArcConditionType::Poisoned, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(20.f, Frag->States[(int32)EArcConditionType::Poisoned].Saturation, 0.5f));
    }

    TEST_METHOD(Disease_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Diseased, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Diseased].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Diseased].bActive));
    }

    TEST_METHOD(Weakened_SimpleApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Weakened, 25.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(25.f, Frag->States[(int32)EArcConditionType::Weakened].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Weakened].bActive));
    }

    TEST_METHOD(NegativeAmount_BypassesInteractions_ReducesSaturation)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Saturation = 50.f;
            Frag->States[(int32)EArcConditionType::Burning].bActive = true;
            Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 40.f;
            Frag->States[(int32)EArcConditionType::Bleeding].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Bleeding, -20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(20.f, Frag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.5f));
    }

    TEST_METHOD(Resistance_ReducesApplication)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Weakened].Resistance = 0.5f;
        }

        ApplyAndExecute(Entity, EArcConditionType::Weakened, 40.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(20.f, Frag->States[(int32)EArcConditionType::Weakened].Saturation, 0.5f));
    }
};
