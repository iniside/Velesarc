// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"
#include "ArcMassAbilitiesTestTypes.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInitialAttributesPreset.h"
#include "Fragments/ArcAggregatorFragment.h"
#include "Fragments/ArcAttributeHandlerSharedFragment.h"
#include "Fragments/ArcInitialAttributesPresetSharedFragment.h"
#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityView.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/SharedStruct.h"

// ---------------------------------------------------------------------------
// Helpers — file-scoped
// ---------------------------------------------------------------------------

namespace ArcInitialAttributesObserverTestLocal
{
	// Build an entity with the attribute fragment(s), FArcAggregatorFragment (the type the
	// observer watches), the const-shared FArcAttributeHandlerSharedFragment (when HandlerConfig
	// is non-null), and the const-shared FArcInitialAttributesPresetSharedFragment (when Preset is
	// non-null). The trait composes fragments with default-constructed values; the observer copies
	// authored BaseValues from the preset onto the entity in Pass 1 and fires Initialize via
	// InitInline in Pass 2.
	//
	// CreateEntity(Archetype, SharedValues) is required (not the FragmentInstanceList overload),
	// because the fragment-list overload constructs the archetype with an empty
	// FMassConstSharedFragmentBitSet, so the observer's AddConstSharedRequirement never matches.
	// The archetype overload calls GetOrCreateSuitableArchetype which folds const-shared bits
	// into the archetype composition.
	//
	// FEntityCreationContext defers the observer dispatch until SetEntityFragmentValues populates
	// fragment memory and the scope exits.
	FMassEntityHandle CreateStatsEntity(
		FMassEntityManager& EntityManager,
		const FArcAttributeSet_TestStats& PresetStats,
		UArcAttributeHandlerConfig* HandlerConfig,
		UArcInitialAttributesPreset*& OutPreset)
	{
		// Sparse attribute fragments (FArcAttributeSet_TestStats etc.) are NOT part of archetype shape;
		// they're added as sparse elements after CreateEntity. Only regular fragments go here.
		TArray<const UScriptStruct*> FragmentTypes;
		FragmentTypes.Add(FArcAggregatorFragment::StaticStruct());
		const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);

		FMassArchetypeSharedFragmentValues SharedValues;
		if (HandlerConfig)
		{
			FArcAttributeHandlerSharedFragment SharedHandler;
			SharedHandler.Config = HandlerConfig;
			SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(SharedHandler));
		}

		UArcInitialAttributesPreset* Preset = NewObject<UArcInitialAttributesPreset>();
		Preset->AttributeFragments.Add(FInstancedStruct::Make(PresetStats));
		OutPreset = Preset;

		FArcInitialAttributesPresetSharedFragment PresetShared;
		PresetShared.Preset = Preset;
		SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(PresetShared));

		// FMassArchetypeData::AddEntityInternal asserts SharedFragmentValues.IsSorted() — Sort
		// must be called once all shared fragments have been added.
		SharedValues.Sort();

		const TSharedRef<FMassEntityManager::FEntityCreationContext> CreationCtx =
			EntityManager.GetOrMakeCreationContext();

		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype, SharedValues);

		// FArcAttributeSet_TestStats is sparse (inherits FArcMassAttributesBase -> FMassSparseFragment),
		// so it cannot live in the archetype. Seed it as a sparse element while the creation
		// context is still alive so the observer sees the entity in its post-Add dispatch.
		EntityManager.AddSparseElementToEntity<FArcAttributeSet_TestStats>(Entity);

		// Entity fragments start at default-constructed values. The observer overwrites BaseValues
		// from the preset and fires PostChange.
		TArray<FInstancedStruct> FragmentData;
		FragmentData.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		EntityManager.SetEntityFragmentValues(Entity, FragmentData);

		return Entity;
	}

	// Overload for tests that don't need the preset reference after creation.
	FMassEntityHandle CreateStatsEntity(
		FMassEntityManager& EntityManager,
		const FArcAttributeSet_TestStats& PresetStats,
		UArcAttributeHandlerConfig* HandlerConfig)
	{
		UArcInitialAttributesPreset* Unused = nullptr;
		return CreateStatsEntity(EntityManager, PresetStats, HandlerConfig, Unused);
	}

	// Variant that omits the preset entirely — used by the "missing shared fragment" test to
	// verify the observer skips the entity rather than crashing when required shared fragments
	// are missing.
	FMassEntityHandle CreateStatsEntityWithoutPreset(
		FMassEntityManager& EntityManager,
		UArcAttributeHandlerConfig* HandlerConfig)
	{
		// Sparse attribute fragments are added post-CreateEntity.
		TArray<const UScriptStruct*> FragmentTypes;
		FragmentTypes.Add(FArcAggregatorFragment::StaticStruct());
		const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);

		FMassArchetypeSharedFragmentValues SharedValues;
		if (HandlerConfig)
		{
			FArcAttributeHandlerSharedFragment SharedHandler;
			SharedHandler.Config = HandlerConfig;
			SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(SharedHandler));
		}

		const TSharedRef<FMassEntityManager::FEntityCreationContext> CreationCtx =
			EntityManager.GetOrMakeCreationContext();

		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype, SharedValues);

		EntityManager.AddSparseElementToEntity<FArcAttributeSet_TestStats>(Entity);

		TArray<FInstancedStruct> FragmentData;
		FragmentData.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		EntityManager.SetEntityFragmentValues(Entity, FragmentData);

		return Entity;
	}

	FMassEntityHandle CreateStatsDerivedEntity(
		FMassEntityManager& EntityManager,
		const FArcAttributeSet_TestStats& PresetStats,
		const FArcAttributeSet_TestDerivedStats& PresetDerived,
		UArcAttributeHandlerConfig* HandlerConfig,
		UArcInitialAttributesPreset*& OutPreset)
	{
		// Sparse attribute fragments are added post-CreateEntity.
		TArray<const UScriptStruct*> FragmentTypes;
		FragmentTypes.Add(FArcAggregatorFragment::StaticStruct());
		const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);

		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		FMassArchetypeSharedFragmentValues SharedValues;
		SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(SharedHandler));

		UArcInitialAttributesPreset* Preset = NewObject<UArcInitialAttributesPreset>();
		Preset->AttributeFragments.Add(FInstancedStruct::Make(PresetStats));
		Preset->AttributeFragments.Add(FInstancedStruct::Make(PresetDerived));
		OutPreset = Preset;

		FArcInitialAttributesPresetSharedFragment PresetShared;
		PresetShared.Preset = Preset;
		SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(PresetShared));

		SharedValues.Sort();

		const TSharedRef<FMassEntityManager::FEntityCreationContext> CreationCtx =
			EntityManager.GetOrMakeCreationContext();

		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype, SharedValues);

		EntityManager.AddSparseElementToEntity<FArcAttributeSet_TestStats>(Entity);
		EntityManager.AddSparseElementToEntity<FArcAttributeSet_TestDerivedStats>(Entity);

		TArray<FInstancedStruct> FragmentData;
		FragmentData.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		EntityManager.SetEntityFragmentValues(Entity, FragmentData);

		return Entity;
	}

	FMassEntityHandle CreateStatsAndResourceEntity(
		FMassEntityManager& EntityManager,
		const FArcAttributeSet_TestStats& PresetStats,
		const FArcAttributeSet_TestResource& PresetResource,
		UArcAttributeHandlerConfig* HandlerConfig,
		UArcInitialAttributesPreset*& OutPreset)
	{
		// Sparse attribute fragments are added post-CreateEntity.
		TArray<const UScriptStruct*> FragmentTypes;
		FragmentTypes.Add(FArcAggregatorFragment::StaticStruct());
		const FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(FragmentTypes);

		FArcAttributeHandlerSharedFragment SharedHandler;
		SharedHandler.Config = HandlerConfig;
		FMassArchetypeSharedFragmentValues SharedValues;
		SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(SharedHandler));

		UArcInitialAttributesPreset* Preset = NewObject<UArcInitialAttributesPreset>();
		Preset->AttributeFragments.Add(FInstancedStruct::Make(PresetStats));
		Preset->AttributeFragments.Add(FInstancedStruct::Make(PresetResource));
		OutPreset = Preset;

		FArcInitialAttributesPresetSharedFragment PresetShared;
		PresetShared.Preset = Preset;
		SharedValues.Add(EntityManager.GetOrCreateConstSharedFragment(PresetShared));

		SharedValues.Sort();

		const TSharedRef<FMassEntityManager::FEntityCreationContext> CreationCtx =
			EntityManager.GetOrMakeCreationContext();

		const FMassEntityHandle Entity = EntityManager.CreateEntity(Archetype, SharedValues);

		EntityManager.AddSparseElementToEntity<FArcAttributeSet_TestStats>(Entity);
		EntityManager.AddSparseElementToEntity<FArcAttributeSet_TestResource>(Entity);

		TArray<FInstancedStruct> FragmentData;
		FragmentData.Add(FInstancedStruct::Make(FArcAggregatorFragment()));
		EntityManager.SetEntityFragmentValues(Entity, FragmentData);

		return Entity;
	}

} // namespace ArcInitialAttributesObserverTestLocal

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

TEST_CLASS(ArcInitialAttributesObserver, "ArcMassAbilities.InitialAttributes")
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

	// -----------------------------------------------------------------------
	// Test 1: Initialize fires for every attribute on a populated fragment.
	//         Preset authors Health=10, Armor=12; observer fires InitInline
	//         for each, both registered trackers see InitializeCount >= 1.
	// -----------------------------------------------------------------------
	TEST_METHOD(Initialize_FiresForEveryAttribute)
	{
		FArcAttributeHandler_Test_PostExecuteTracker HealthTracker;
		FArcAttributeHandler_Test_PostExecuteTracker ArmorTracker;

		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(HealthTracker));
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetArmorAttribute(),  FInstancedStruct::Make(ArmorTracker));

		FArcAttributeSet_TestStats PresetStats;
		PresetStats.Health.BaseValue    = 10.f;
		PresetStats.Health.CurrentValue = 10.f;
		PresetStats.Armor.BaseValue     = 12.f;
		PresetStats.Armor.CurrentValue  = 12.f;

		ArcInitialAttributesObserverTestLocal::CreateStatsEntity(*EntityManager, PresetStats, Config);

		const FInstancedStruct* LiveHealthStruct = Config->Handlers.Find(FArcAttributeSet_TestStats::GetHealthAttribute());
		const FInstancedStruct* LiveArmorStruct  = Config->Handlers.Find(FArcAttributeSet_TestStats::GetArmorAttribute());
		ASSERT_THAT(IsNotNull(LiveHealthStruct));
		ASSERT_THAT(IsNotNull(LiveArmorStruct));

		const FArcAttributeHandler_Test_PostExecuteTracker* LiveHealth = LiveHealthStruct->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();
		const FArcAttributeHandler_Test_PostExecuteTracker* LiveArmor  = LiveArmorStruct->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();
		ASSERT_THAT(IsNotNull(LiveHealth));
		ASSERT_THAT(IsNotNull(LiveArmor));

		ASSERT_THAT(IsTrue(LiveHealth->InitializeCount >= 1));
		ASSERT_THAT(IsTrue(LiveArmor->InitializeCount >= 1));
	}

	// -----------------------------------------------------------------------
	// Test 2: Entity BaseValue and CurrentValue both end up matching the
	//         preset's authored BaseValue after the observer fires.
	// -----------------------------------------------------------------------
	TEST_METHOD(BaseAndCurrentValue_BothSet)
	{
		FArcAttributeHandler_Test_PostExecuteTracker HealthTracker;
		FArcAttributeHandler_Test_PostExecuteTracker ArmorTracker;

		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(HealthTracker));
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetArmorAttribute(),  FInstancedStruct::Make(ArmorTracker));

		FArcAttributeSet_TestStats PresetStats;
		PresetStats.Health.BaseValue    = 10.f;
		PresetStats.Health.CurrentValue = 10.f;
		PresetStats.Armor.BaseValue     = 12.f;
		PresetStats.Armor.CurrentValue  = 12.f;

		const FMassEntityHandle Entity =
			ArcInitialAttributesObserverTestLocal::CreateStatsEntity(*EntityManager, PresetStats, Config);

		FMassEntityView EntityView(*EntityManager, Entity);
		const FArcAttributeSet_TestStats* LiveStats = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestStats>();
		ASSERT_THAT(IsNotNull(LiveStats));

		ASSERT_THAT(IsNear(10.f, LiveStats->Health.BaseValue,    0.001f));
		ASSERT_THAT(IsNear(10.f, LiveStats->Health.CurrentValue, 0.001f));
		ASSERT_THAT(IsNear(12.f, LiveStats->Armor.BaseValue,     0.001f));
		ASSERT_THAT(IsNear(12.f, LiveStats->Armor.CurrentValue,  0.001f));
	}

	// -----------------------------------------------------------------------
	// Test 3: Initialize fires even when the preset's BaseValue is 0.
	//         InitInline must NOT short-circuit on Old == New.
	// -----------------------------------------------------------------------
	TEST_METHOD(Initialize_FiresForZeroBaseValue)
	{
		FArcAttributeHandler_Test_PostExecuteTracker HealthTracker;
		FArcAttributeHandler_Test_PostExecuteTracker ArmorTracker;

		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(HealthTracker));
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetArmorAttribute(),  FInstancedStruct::Make(ArmorTracker));

		// All preset values explicitly at 0.
		FArcAttributeSet_TestStats PresetStats;
		PresetStats.Health.BaseValue    = 0.f;
		PresetStats.Health.CurrentValue = 0.f;
		PresetStats.Armor.BaseValue     = 0.f;
		PresetStats.Armor.CurrentValue  = 0.f;

		ArcInitialAttributesObserverTestLocal::CreateStatsEntity(*EntityManager, PresetStats, Config);

		const FInstancedStruct* LiveHealthStruct = Config->Handlers.Find(FArcAttributeSet_TestStats::GetHealthAttribute());
		const FInstancedStruct* LiveArmorStruct  = Config->Handlers.Find(FArcAttributeSet_TestStats::GetArmorAttribute());
		ASSERT_THAT(IsNotNull(LiveHealthStruct));
		ASSERT_THAT(IsNotNull(LiveArmorStruct));

		const FArcAttributeHandler_Test_PostExecuteTracker* LiveHealth = LiveHealthStruct->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();
		const FArcAttributeHandler_Test_PostExecuteTracker* LiveArmor  = LiveArmorStruct->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();
		ASSERT_THAT(IsNotNull(LiveHealth));
		ASSERT_THAT(IsNotNull(LiveArmor));

		ASSERT_THAT(IsTrue(LiveHealth->InitializeCount >= 1));
		ASSERT_THAT(IsTrue(LiveArmor->InitializeCount >= 1));
	}

	// -----------------------------------------------------------------------
	// Test 4: Missing required shared fragments → no Initialize, no crash.
	//         Observer requires both FArcAttributeHandlerSharedFragment and
	//         FArcInitialAttributesPresetSharedFragment via EMassFragmentPresence::All;
	//         neither is attached here, so the entity is skipped entirely.
	// -----------------------------------------------------------------------
	TEST_METHOD(MissingHandlerSharedFragment_NoCrash_NoInitialize)
	{
		FArcAttributeHandler_Test_PostExecuteTracker HealthTracker;

		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetHealthAttribute(), FInstancedStruct::Make(HealthTracker));

		ArcInitialAttributesObserverTestLocal::CreateStatsEntityWithoutPreset(*EntityManager, nullptr);

		const FInstancedStruct* LiveHealthStruct = Config->Handlers.Find(FArcAttributeSet_TestStats::GetHealthAttribute());
		ASSERT_THAT(IsNotNull(LiveHealthStruct));

		const FArcAttributeHandler_Test_PostExecuteTracker* LiveHealth = LiveHealthStruct->GetPtr<FArcAttributeHandler_Test_PostExecuteTracker>();
		ASSERT_THAT(IsNotNull(LiveHealth));

		ASSERT_THAT(AreEqual(0, LiveHealth->InitializeCount));
	}

	// -----------------------------------------------------------------------
	// Test 5: Two-pass ordering correctness.
	//
	//         Preset authors FArcAttributeSet_TestStats (Intelligence.BaseValue = 12)
	//                      + FArcAttributeSet_TestDerivedStats (MaxEnergy = 0).
	//         Handler: FArcAttributeHandler_Test_EnergyFromIntelligence on Intelligence sets
	//                  Derived.MaxEnergy.BaseValue = NewValue * 2.
	//
	//         Observer Pass 1 copies preset values onto the entity. Pass 2
	//         InitInline on Intelligence fires Initialize, which writes
	//         MaxEnergy.BaseValue = 24 into entity memory. Pass 2's subsequent
	//         InitInline on MaxEnergy reads from entity memory (post-cascade),
	//         so the 24 survives.
	// -----------------------------------------------------------------------
	TEST_METHOD(TwoPassOrdering_DerivedAttributeSeeded)
	{
		FArcAttributeHandler_Test_EnergyFromIntelligence IntelHandler;

		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetIntelligenceAttribute(), FInstancedStruct::Make(IntelHandler));

		FArcAttributeSet_TestStats PresetStats;
		PresetStats.Intelligence.BaseValue    = 12.f;
		PresetStats.Intelligence.CurrentValue = 12.f;

		FArcAttributeSet_TestDerivedStats PresetDerived;

		UArcInitialAttributesPreset* Preset = nullptr;
		const FMassEntityHandle Entity = ArcInitialAttributesObserverTestLocal::CreateStatsDerivedEntity(
			*EntityManager, PresetStats, PresetDerived, Config, Preset);

		FMassEntityView EntityView(*EntityManager, Entity);
		const FArcAttributeSet_TestDerivedStats* LiveDerived = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestDerivedStats>();
		ASSERT_THAT(IsNotNull(LiveDerived));

		ASSERT_THAT(IsNear(24.f, LiveDerived->MaxEnergy.BaseValue, 0.001f));
	}

	// -----------------------------------------------------------------------
	// Test 6: Initialize fills resource to max at seed time.
	//
	//         Preset authors FArcAttributeSet_TestStats (Intelligence.BaseValue = 5)
	//                      + FArcAttributeSet_TestResource (Energy = 0, MaxEnergy = 0).
	//         Handler: FArcAttributeHandler_Test_ResourceMaxFiller on Intelligence.
	//
	//         Initialize sets MaxEnergy = 10 (5 * 2) and Energy = 10 (filled,
	//         not clamped). PostChange would have left Energy at 0 (Min(0, 10)).
	//         This test proves the fill-vs-clamp split is seed-time correct.
	// -----------------------------------------------------------------------
	TEST_METHOD(Initialize_FillsResourceToMax)
	{
		FArcAttributeHandler_Test_ResourceMaxFiller FillerHandler;

		UArcAttributeHandlerConfig* Config = NewObject<UArcAttributeHandlerConfig>();
		Config->Handlers.Add(FArcAttributeSet_TestStats::GetIntelligenceAttribute(), FInstancedStruct::Make(FillerHandler));

		FArcAttributeSet_TestStats PresetStats;
		PresetStats.Intelligence.BaseValue    = 5.f;
		PresetStats.Intelligence.CurrentValue = 5.f;

		FArcAttributeSet_TestResource PresetResource;
		PresetResource.Energy.BaseValue     = 0.f;
		PresetResource.Energy.CurrentValue  = 0.f;
		PresetResource.MaxEnergy.BaseValue  = 0.f;
		PresetResource.MaxEnergy.CurrentValue = 0.f;

		UArcInitialAttributesPreset* Preset = nullptr;
		const FMassEntityHandle Entity = ArcInitialAttributesObserverTestLocal::CreateStatsAndResourceEntity(
			*EntityManager, PresetStats, PresetResource, Config, Preset);

		FMassEntityView EntityView(*EntityManager, Entity);
		const FArcAttributeSet_TestResource* LiveResource = EntityView.GetSparseFragmentDataPtr<FArcAttributeSet_TestResource>();
		ASSERT_THAT(IsNotNull(LiveResource));

		ASSERT_THAT(IsNear(10.f, LiveResource->MaxEnergy.BaseValue,   0.001f));
		ASSERT_THAT(IsNear(10.f, LiveResource->Energy.CurrentValue,   0.001f));
	}
};
