// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "ArcConditionReactionProcessor.h"
#include "ArcConditionSaturationAttributes.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"

TEST_CLASS(ArcCondition_SaturationAttributeSync, "ArcConditionEffects.Integration.SaturationAttributes")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionReactionProcessor* Processor = nullptr;

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
        Processor = NewObject<UArcConditionReactionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void SignalAndExecute(FMassEntityHandle Entity, FName SignalName)
    {
        SignalSubsystem->SignalEntity(SignalName, Entity);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(BurningSaturation_SyncsToAttribute)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Saturation = 42.5f;
            Frag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionSaturationAttributes* SatAttrs = View.GetFragmentDataPtr<FArcConditionSaturationAttributes>();
        ASSERT_THAT(IsNear(42.5f, SatAttrs->BurningSaturation.BaseValue, 0.001f));
    }

    TEST_METHOD(MultipleSaturations_SyncIndependently)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();

            Frag->States[(int32)EArcConditionType::Burning].Saturation = 10.f;
            Frag->States[(int32)EArcConditionType::Burning].bActive = true;

            Frag->States[(int32)EArcConditionType::Bleeding].Saturation = 60.f;
            Frag->States[(int32)EArcConditionType::Bleeding].bActive = true;

            Frag->States[(int32)EArcConditionType::Chilled].Saturation = 0.f;
            Frag->States[(int32)EArcConditionType::Chilled].bActive = false;
        }

        SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionSaturationAttributes* SatAttrs = View.GetFragmentDataPtr<FArcConditionSaturationAttributes>();
        ASSERT_THAT(IsNear(10.f, SatAttrs->BurningSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(60.f, SatAttrs->BleedingSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->ChilledSaturation.BaseValue, 0.001f));
    }

    TEST_METHOD(ZeroSaturation_SyncsZero)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

        // Fresh entity — all saturations default to 0; just signal and verify all attributes are 0
        SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionSaturationAttributes* SatAttrs = View.GetFragmentDataPtr<FArcConditionSaturationAttributes>();
        ASSERT_THAT(IsNear(0.f, SatAttrs->BurningSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->BleedingSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->ChilledSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->ShockedSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->PoisonedSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->DiseasedSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->WeakenedSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->OiledSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->WetSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->CorrodedSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->BlindedSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->SuffocatingSaturation.BaseValue, 0.001f));
        ASSERT_THAT(IsNear(0.f, SatAttrs->ExhaustedSaturation.BaseValue, 0.001f));
    }

    TEST_METHOD(SaturationUpdatesOnSubsequentRuns)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntityWithEffects(*EntityManager);

        // First run: burning saturation = 50
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Saturation = 50.f;
            Frag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionSaturationAttributes* SatAttrs = View.GetFragmentDataPtr<FArcConditionSaturationAttributes>();
            ASSERT_THAT(IsNear(50.f, SatAttrs->BurningSaturation.BaseValue, 0.001f));
        }

        // Second run: burning saturation updated to 30
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
        }

        SignalAndExecute(Entity, UE::ArcConditionEffects::Signals::ConditionStateChanged);

        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionSaturationAttributes* SatAttrs = View.GetFragmentDataPtr<FArcConditionSaturationAttributes>();
            ASSERT_THAT(IsNear(30.f, SatAttrs->BurningSaturation.BaseValue, 0.001f));
        }
    }
};
