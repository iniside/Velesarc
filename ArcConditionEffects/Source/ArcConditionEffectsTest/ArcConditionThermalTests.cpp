// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcConditionEffectsTestHelpers.h"
#include "ArcConditionEffectsSubsystem.h"
#include "ArcConditionFragments.h"
#include "ArcConditionTypes.h"
#include "ArcThermalConditionProcessor.h"
#include "Components/ActorTestSpawner.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassEntityView.h"
#include "MassExecutor.h"
#include "MassProcessingContext.h"
#include "MassSignalSubsystem.h"

// ============================================================================
// Thermal — Fire Application
// ============================================================================

TEST_CLASS(ArcCondition_Thermal_Fire, "ArcConditionEffects.Thermal.Fire")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionEffectsSubsystem* Subsystem = nullptr;
    UArcThermalConditionProcessor* Processor = nullptr;

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

        Subsystem = Spawner.GetWorld().GetSubsystem<UArcConditionEffectsSubsystem>();
        check(Subsystem);

        TSharedRef<FMassEntityManager> SharedEM = EntityManager->AsShared();
        Processor = NewObject<UArcThermalConditionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ApplyAndExecute(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
    {
        Subsystem->ApplyCondition(Entity, Type, Amount);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(SimpleFire_AddsBurning)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Burning, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Burning].bActive));
    }

    TEST_METHOD(Cauterization_ClearsBleeding)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* BleedingFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            BleedingFrag->States[(int32)EArcConditionType::Bleeding].Saturation = 40.f;
            BleedingFrag->States[(int32)EArcConditionType::Bleeding].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 10.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* BleedingFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, BleedingFrag->States[(int32)EArcConditionType::Bleeding].Saturation, 0.001f));
        ASSERT_THAT(IsFalse(BleedingFrag->States[(int32)EArcConditionType::Bleeding].bActive));
    }

    TEST_METHOD(Sterilization_ClearsDisease)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* DiseasedFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            DiseasedFrag->States[(int32)EArcConditionType::Diseased].Saturation = 50.f;
            DiseasedFrag->States[(int32)EArcConditionType::Diseased].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 10.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* DiseasedFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, DiseasedFrag->States[(int32)EArcConditionType::Diseased].Saturation, 0.001f));
    }

    TEST_METHOD(ShatterFrozen_ClearsChilled_GeneratesBlind_HalvesFire)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation = 80.f;
            ChilledFrag->States[(int32)EArcConditionType::Chilled].bActive = true;
            ChilledFrag->States[(int32)EArcConditionType::Chilled].OverloadPhase = EArcConditionOverloadPhase::Overloaded;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 40.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation, 0.001f));

        FArcConditionStatesFragment* BlindedFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsTrue(BlindedFrag->States[(int32)EArcConditionType::Blinded].Saturation > 0.f));

        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(20.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
    }

    TEST_METHOD(EvaporateWet_ConsumesWet_ReducesFire_GeneratesSteam)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            WetFrag->States[(int32)EArcConditionType::Wet].Saturation = 20.f;
            WetFrag->States[(int32)EArcConditionType::Wet].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, WetFrag->States[(int32)EArcConditionType::Wet].Saturation, 0.001f));

        FArcConditionStatesFragment* BlindedFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(10.f, BlindedFrag->States[(int32)EArcConditionType::Blinded].Saturation, 0.5f));

        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
    }

    TEST_METHOD(MeltChilled_CostsDouble_CreatesWet)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation = 20.f;
            ChilledFrag->States[(int32)EArcConditionType::Chilled].bActive = false;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation, 0.001f));

        FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(10.f, WetFrag->States[(int32)EArcConditionType::Wet].Saturation, 0.5f));

        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(40.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
    }

    TEST_METHOD(FuelConversion_OilConsumed_DoubledToFire)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* OiledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            OiledFrag->States[(int32)EArcConditionType::Oiled].Saturation = 10.f;
            OiledFrag->States[(int32)EArcConditionType::Oiled].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* OiledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, OiledFrag->States[(int32)EArcConditionType::Oiled].Saturation, 0.001f));

        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(40.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
    }

    TEST_METHOD(Resistance_ReducesFinalBurning)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            Frag->States[(int32)EArcConditionType::Burning].Resistance = 0.5f;
        }

        ApplyAndExecute(Entity, EArcConditionType::Burning, 40.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(20.f, Frag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));
    }
};

// ============================================================================
// Thermal — Cold Application
// ============================================================================

TEST_CLASS(ArcCondition_Thermal_Cold, "ArcConditionEffects.Thermal.Cold")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionEffectsSubsystem* Subsystem = nullptr;
    UArcThermalConditionProcessor* Processor = nullptr;

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
        Processor = NewObject<UArcThermalConditionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ApplyAndExecute(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
    {
        Subsystem->ApplyCondition(Entity, Type, Amount);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(SimpleCold_AddsChilled)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Chilled, 40.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(40.f, Frag->States[(int32)EArcConditionType::Chilled].Saturation, 0.5f));
        ASSERT_THAT(IsTrue(Frag->States[(int32)EArcConditionType::Chilled].bActive));
    }

    TEST_METHOD(ExtinguishBurning_DoubleEfficiency)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            BurningFrag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
            BurningFrag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Chilled, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.001f));
    }

    TEST_METHOD(Condensation_ExcessColdBecomesWet_WhenFireExtinguished)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            BurningFrag->States[(int32)EArcConditionType::Burning].Saturation = 10.f;
            BurningFrag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Chilled, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(7.5f, WetFrag->States[(int32)EArcConditionType::Wet].Saturation, 0.5f));

        FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation, 0.001f));
    }

    TEST_METHOD(FlashFreeze_WetConsumed_ColdTripled)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            WetFrag->States[(int32)EArcConditionType::Wet].Saturation = 30.f;
            WetFrag->States[(int32)EArcConditionType::Wet].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Chilled, 10.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, WetFrag->States[(int32)EArcConditionType::Wet].Saturation, 0.001f));

        FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation, 0.5f));
    }

    TEST_METHOD(SteamFromExtinguishing)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            BurningFrag->States[(int32)EArcConditionType::Burning].Saturation = 40.f;
            BurningFrag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Chilled, 50.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* BlindedFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(12.f, BlindedFrag->States[(int32)EArcConditionType::Blinded].Saturation, 0.5f));
    }
};

// ============================================================================
// Thermal — Oil Application
// ============================================================================

TEST_CLASS(ArcCondition_Thermal_Oil, "ArcConditionEffects.Thermal.Oil")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionEffectsSubsystem* Subsystem = nullptr;
    UArcThermalConditionProcessor* Processor = nullptr;

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
        Processor = NewObject<UArcThermalConditionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ApplyAndExecute(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
    {
        Subsystem->ApplyCondition(Entity, Type, Amount);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(SimpleOil_AppliesOiled)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Oiled, 25.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(25.f, Frag->States[(int32)EArcConditionType::Oiled].Saturation, 0.5f));
    }

    TEST_METHOD(Stoking_OilOnBurningTarget_FeedsFire)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            BurningFrag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
            BurningFrag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Oiled, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(60.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));

        FArcConditionStatesFragment* OiledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, OiledFrag->States[(int32)EArcConditionType::Oiled].Saturation, 0.001f));
    }
};

// ============================================================================
// Thermal — Water Application
// ============================================================================

TEST_CLASS(ArcCondition_Thermal_Water, "ArcConditionEffects.Thermal.Water")
{
    FActorTestSpawner Spawner;
    FMassEntityManager* EntityManager = nullptr;
    UMassSignalSubsystem* SignalSubsystem = nullptr;
    UArcConditionEffectsSubsystem* Subsystem = nullptr;
    UArcThermalConditionProcessor* Processor = nullptr;

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
        Processor = NewObject<UArcThermalConditionProcessor>();
        Processor->CallInitialize(EntityManager->GetOwner(), SharedEM);
    }

    void ApplyAndExecute(FMassEntityHandle Entity, EArcConditionType Type, float Amount)
    {
        Subsystem->ApplyCondition(Entity, Type, Amount);
        FMassProcessingContext ProcessingContext(*EntityManager, 0.f);
        UE::Mass::Executor::Run(*Processor, ProcessingContext);
    }

    TEST_METHOD(SimpleWater_AppliesWet)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        ApplyAndExecute(Entity, EArcConditionType::Wet, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* Frag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(30.f, Frag->States[(int32)EArcConditionType::Wet].Saturation, 0.5f));
    }

    TEST_METHOD(Ablative_WaterReducesBurning)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            BurningFrag->States[(int32)EArcConditionType::Burning].Saturation = 30.f;
            BurningFrag->States[(int32)EArcConditionType::Burning].bActive = true;
        }

        ApplyAndExecute(Entity, EArcConditionType::Wet, 20.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* BurningFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(10.f, BurningFrag->States[(int32)EArcConditionType::Burning].Saturation, 0.5f));

        FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, WetFrag->States[(int32)EArcConditionType::Wet].Saturation, 0.001f));
    }

    TEST_METHOD(ThermalMass_WaterOnChilled_BoostsCold)
    {
        FMassEntityHandle Entity = ArcConditionTestHelpers::CreateConditionEntity(*EntityManager);
        {
            FMassEntityView View(*EntityManager, Entity);
            FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
            ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation = 20.f;
            ChilledFrag->States[(int32)EArcConditionType::Chilled].bActive = false;
        }

        ApplyAndExecute(Entity, EArcConditionType::Wet, 30.f);

        FMassEntityView View(*EntityManager, Entity);
        FArcConditionStatesFragment* ChilledFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(35.f, ChilledFrag->States[(int32)EArcConditionType::Chilled].Saturation, 0.5f));

        FArcConditionStatesFragment* WetFrag = View.GetFragmentDataPtr<FArcConditionStatesFragment>();
        ASSERT_THAT(IsNear(0.f, WetFrag->States[(int32)EArcConditionType::Wet].Saturation, 0.001f));
    }
};
