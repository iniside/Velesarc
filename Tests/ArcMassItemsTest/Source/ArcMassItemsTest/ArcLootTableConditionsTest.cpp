// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Components/ActorTestSpawner.h"

#include "Abilities/ArcAbilityFunctions.h"
#include "Fragments/ArcOwnedTagsFragment.h"
#include "Items/Loot/ArcLootTable.h"
#include "Items/Loot/ArcLootDropSubsystem.h"
#include "Items/Loot/ArcLootWorldConditionSchema.h"
#include "Items/Loot/ArcLootWorldConditions.h"

#include "Mass/EntityManager.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityHandle.h"
#include "Mass/EntityFragments.h"
#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/StructView.h"
#include "WorldConditionQuery.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Loot_Rare, "Test.Loot.Rare");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_Test_Loot_Junk, "Test.Loot.Junk");

namespace ArcLootTableConditionsTest
{
	FArcNamedPrimaryAssetId MakeTestItemId(const TCHAR* Name)
	{
		FArcNamedPrimaryAssetId Id;
		Id.AssetId = FPrimaryAssetId(FPrimaryAssetType(TEXT("ArcItemDefinition")), FName(Name));
		return Id;
	}

	FArcLootTableEntry_Weighted MakeWeightedEntry(const TCHAR* Name, const float Weight)
	{
		FArcLootTableEntry_Weighted Entry;
		Entry.ItemDefinitionId = MakeTestItemId(Name);
		Entry.MinAmount = 1;
		Entry.MaxAmount = 1;
		Entry.Weight = Weight;
		Entry.DropProbability = 1.f;
		return Entry;
	}
}

TEST_CLASS(ArcLootTable_ConditionFiltering, "ArcMassItems.Loot.Conditions")
{
	FActorTestSpawner Spawner;
	FMassEntityManager* EntityManager = nullptr;
	UArcLootDropSubsystem* LootSubsystem = nullptr;

	FMassEntityHandle SpawnTaggedEntity(const FGameplayTagContainer& InitialTags)
	{
		TArray<FInstancedStruct> Fragments;
		Fragments.Add(FInstancedStruct::Make(FTransformFragment{}));
		Fragments.Add(FInstancedStruct::Make(FArcOwnedTagsFragment{}));
		FMassEntityHandle Entity = EntityManager->CreateEntity(Fragments);

		ArcAbilities::AddOwnedTags(*EntityManager, Entity, InitialTags);
		return Entity;
	}

	BEFORE_EACH()
	{
		FAutomationTestBase::bSuppressLogWarnings = true;
		Spawner.GetWorld();
		Spawner.InitializeGameSubsystems();
		UMassEntitySubsystem* MES = Spawner.GetWorld().GetSubsystem<UMassEntitySubsystem>();
		check(MES);
		EntityManager = &MES->GetMutableEntityManager();
		LootSubsystem = Spawner.GetWorld().GetSubsystem<UArcLootDropSubsystem>();
		check(LootSubsystem);
	}

	TEST_METHOD(EmptyConditions_EntryRolls)
	{
		// Loot table with one weighted entry, no conditions → must roll.
		UArcLootTable* Table = NewObject<UArcLootTable>();
		Table->Entries.Add(FInstancedStruct::Make(ArcLootTableConditionsTest::MakeWeightedEntry(TEXT("Item.A"), 1.f)));
		FArcLootRollStrategy_WeightedSelection Strategy;
		Strategy.MaxDrops = 1;
		Table->RollStrategy = FInstancedStruct::Make(Strategy);

		FArcLootContext Ctx;
		Ctx.SelfEntity        = SpawnTaggedEntity(FGameplayTagContainer{});
		Ctx.InstigatorEntity  = SpawnTaggedEntity(FGameplayTagContainer{});

		const FInstancedStruct CtxStruct = FInstancedStruct::Make(Ctx);
		const TArray<FArcLootDropResult> Drops = LootSubsystem->RollLootTable(Table, FConstStructView(CtxStruct));

		ASSERT_THAT(AreEqual(1, Drops.Num()));
	}

	TEST_METHOD(SelfHasTag_DropsWhenTagPresent)
	{
		// Build a loot table where the single entry requires Self.HasTag(Test.Loot.Rare).
		UArcLootTable* Table = NewObject<UArcLootTable>();
		FArcLootTableEntry_Weighted Entry = ArcLootTableConditionsTest::MakeWeightedEntry(TEXT("Item.Rare"), 1.f);

#if WITH_EDITORONLY_DATA
		FArcLootWorldCondition_EntityHasTag Cond;
		Cond.Entity = EArcLootContextEntity::Self;
		Cond.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Loot_Rare));

		TArray<FWorldConditionEditable> EditableConds;
		EditableConds.Emplace(/*ExpressionDepth*/0, EWorldConditionOperator::And, FConstStructView::Make(Cond));

		const bool bInit = Entry.Conditions.Initialize(Table, UArcLootWorldConditionSchema::StaticClass(), EditableConds);
		ASSERT_THAT(IsTrue(bInit));
#endif

		Table->Entries.Add(FInstancedStruct::Make(Entry));
		FArcLootRollStrategy_WeightedSelection Strategy;
		Strategy.MaxDrops = 1;
		Table->RollStrategy = FInstancedStruct::Make(Strategy);

		// Pass case: self entity owns the required tag.
		FArcLootContext PassCtx;
		PassCtx.SelfEntity       = SpawnTaggedEntity(FGameplayTagContainer(TAG_Test_Loot_Rare));
		PassCtx.InstigatorEntity = SpawnTaggedEntity(FGameplayTagContainer{});
		const FInstancedStruct PassStruct = FInstancedStruct::Make(PassCtx);
		const TArray<FArcLootDropResult> PassDrops = LootSubsystem->RollLootTable(Table, FConstStructView(PassStruct));
		ASSERT_THAT(AreEqual(1, PassDrops.Num()));
	}

	TEST_METHOD(SelfHasTag_NoDropWhenTagAbsent)
	{
		UArcLootTable* Table = NewObject<UArcLootTable>();
		FArcLootTableEntry_Weighted Entry = ArcLootTableConditionsTest::MakeWeightedEntry(TEXT("Item.Rare"), 1.f);

#if WITH_EDITORONLY_DATA
		FArcLootWorldCondition_EntityHasTag Cond;
		Cond.Entity = EArcLootContextEntity::Self;
		Cond.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Loot_Rare));

		TArray<FWorldConditionEditable> EditableConds;
		EditableConds.Emplace(/*ExpressionDepth*/0, EWorldConditionOperator::And, FConstStructView::Make(Cond));

		const bool bInit = Entry.Conditions.Initialize(Table, UArcLootWorldConditionSchema::StaticClass(), EditableConds);
		ASSERT_THAT(IsTrue(bInit));
#endif

		Table->Entries.Add(FInstancedStruct::Make(Entry));
		FArcLootRollStrategy_WeightedSelection Strategy;
		Strategy.MaxDrops = 1;
		Table->RollStrategy = FInstancedStruct::Make(Strategy);

		FArcLootContext FailCtx;
		FailCtx.SelfEntity       = SpawnTaggedEntity(FGameplayTagContainer(TAG_Test_Loot_Junk)); // wrong tag
		FailCtx.InstigatorEntity = SpawnTaggedEntity(FGameplayTagContainer{});
		const FInstancedStruct FailStruct = FInstancedStruct::Make(FailCtx);
		const TArray<FArcLootDropResult> FailDrops = LootSubsystem->RollLootTable(Table, FConstStructView(FailStruct));
		ASSERT_THAT(AreEqual(0, FailDrops.Num()));
	}

	TEST_METHOD(InstigatorHasTag_DropsWhenTagPresent)
	{
		UArcLootTable* Table = NewObject<UArcLootTable>();
		FArcLootTableEntry_Weighted Entry = ArcLootTableConditionsTest::MakeWeightedEntry(TEXT("Item.Reward"), 1.f);

#if WITH_EDITORONLY_DATA
		FArcLootWorldCondition_EntityHasTag Cond;
		Cond.Entity = EArcLootContextEntity::Instigator;
		Cond.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Loot_Rare));

		TArray<FWorldConditionEditable> EditableConds;
		EditableConds.Emplace(/*ExpressionDepth*/0, EWorldConditionOperator::And, FConstStructView::Make(Cond));

		const bool bInit = Entry.Conditions.Initialize(Table, UArcLootWorldConditionSchema::StaticClass(), EditableConds);
		ASSERT_THAT(IsTrue(bInit));
#endif

		Table->Entries.Add(FInstancedStruct::Make(Entry));
		FArcLootRollStrategy_WeightedSelection Strategy;
		Strategy.MaxDrops = 1;
		Table->RollStrategy = FInstancedStruct::Make(Strategy);

		FArcLootContext Ctx;
		Ctx.SelfEntity        = SpawnTaggedEntity(FGameplayTagContainer{});
		Ctx.InstigatorEntity  = SpawnTaggedEntity(FGameplayTagContainer(TAG_Test_Loot_Rare));
		const FInstancedStruct CtxStruct = FInstancedStruct::Make(Ctx);
		const TArray<FArcLootDropResult> Drops = LootSubsystem->RollLootTable(Table, FConstStructView(CtxStruct));
		ASSERT_THAT(AreEqual(1, Drops.Num()));
	}

	TEST_METHOD(WeightedSelection_FailingEntryDoesNotConsumeSlot)
	{
		// A condition-failing entry with overwhelming weight must be excluded
		// BEFORE weighted selection — it must not eat the single MaxDrops slot
		// nor dilute the eligible entry's odds.
		UArcLootTable* Table = NewObject<UArcLootTable>();
		FArcLootTableEntry_Weighted GatedEntry = ArcLootTableConditionsTest::MakeWeightedEntry(TEXT("Item.Gated"), 1000000.f);

#if WITH_EDITORONLY_DATA
		FArcLootWorldCondition_EntityHasTag Cond;
		Cond.Entity = EArcLootContextEntity::Self;
		Cond.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(FGameplayTagContainer(TAG_Test_Loot_Rare));

		TArray<FWorldConditionEditable> EditableConds;
		EditableConds.Emplace(/*ExpressionDepth*/0, EWorldConditionOperator::And, FConstStructView::Make(Cond));

		const bool bInit = GatedEntry.Conditions.Initialize(Table, UArcLootWorldConditionSchema::StaticClass(), EditableConds);
		ASSERT_THAT(IsTrue(bInit));
#endif

		Table->Entries.Add(FInstancedStruct::Make(GatedEntry));
		Table->Entries.Add(FInstancedStruct::Make(ArcLootTableConditionsTest::MakeWeightedEntry(TEXT("Item.Open"), 1.f)));
		FArcLootRollStrategy_WeightedSelection Strategy;
		Strategy.MaxDrops = 1;
		Table->RollStrategy = FInstancedStruct::Make(Strategy);

		// Self entity lacks the required tag → gated entry is ineligible.
		FArcLootContext Ctx;
		Ctx.SelfEntity       = SpawnTaggedEntity(FGameplayTagContainer{});
		Ctx.InstigatorEntity = SpawnTaggedEntity(FGameplayTagContainer{});
		const FInstancedStruct CtxStruct = FInstancedStruct::Make(Ctx);

		const FPrimaryAssetId ExpectedId = ArcLootTableConditionsTest::MakeTestItemId(TEXT("Item.Open")).AssetId;
		for (int32 RollIdx = 0; RollIdx < 20; ++RollIdx)
		{
			const TArray<FArcLootDropResult> Drops = LootSubsystem->RollLootTable(Table, FConstStructView(CtxStruct));
			ASSERT_THAT(AreEqual(1, Drops.Num()));
			ASSERT_THAT(IsTrue(ExpectedId == Drops[0].ItemSpec.GetItemDefinitionId()));
		}
	}

	TEST_METHOD(LootTable_StampsSchema_OnEntriesAfterEdit)
	{
		UArcLootTable* Table = NewObject<UArcLootTable>();

		// Push a default-constructed base entry — ItemDefinitionId intentionally unset.
		Table->Entries.Add(FInstancedStruct::Make(FArcLootTableEntry{}));
		ASSERT_THAT(AreEqual(1, Table->Entries.Num()));

		// Sanity: freshly constructed entry has no schema stamped yet.
		{
			const FArcLootTableEntry* RawEntry = Table->Entries[0].GetPtr<FArcLootTableEntry>();
			check(RawEntry);
			const TSubclassOf<UWorldConditionSchema> SchemaBeforeStamp = RawEntry->Conditions.GetSchemaClass();
			ASSERT_THAT(IsTrue(SchemaBeforeStamp != UArcLootWorldConditionSchema::StaticClass()));
		}

		// Call the file-scoped helper directly — this is the unit under test.
		ArcLootTableSchemaInternal::StampEntrySchemas(*Table);

		// Re-fetch pointer after stamp (FInstancedStruct storage may have moved).
		const FArcLootTableEntry* Entry = Table->Entries[0].GetPtr<FArcLootTableEntry>();
		ASSERT_THAT(IsTrue(Entry != nullptr));
		const TSubclassOf<UWorldConditionSchema> SchemaAfterStamp = Entry->Conditions.GetSchemaClass();
		ASSERT_THAT(AreEqual(UArcLootWorldConditionSchema::StaticClass(), SchemaAfterStamp.Get()));
	}
};
