// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"

#include "NativeGameplayTags.h"
#include "StructUtils/InstancedStruct.h"

#include "ArcCraft/MaterialCraft/ArcMaterialCraftContext.h"
#include "ArcCraft/MaterialCraft/ArcMaterialCraftEvaluator.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyRule.h"
#include "ArcCraft/MaterialCraft/ArcMaterialPropertyTableData.h"
#include "ArcCraft/Recipe/ArcRandomPoolDefinitionData.h"
#include "ArcCraft/Recipe/ArcRandomPoolSelectionMode.h"
#include "ArcCraft/Recipe/ArcRecipeModifierSource.h"
#include "ArcCraft/Shared/ArcCraftModifier.h"
#include "ArcCraft/Shared/ArcCraftRecords.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/Fragments/ArcItemFragment_Tags.h"
#include "ItemFragments/ArcItemFragment_ItemStats.h"
#include "Math/RandomStream.h"

#include "ArcCraftTestRecordFiles.h"
#include "ArcCraftTestRulesFixture.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecIdSrc_Slot, "Test.RecIdSrc.Slot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RecIdSrc_Metal, "Test.RecIdSrc.Metal");

namespace ArcRecipeModifierSourceRecordIdTestsImpl
{
	/** The catalog stat ids are told apart by their fixed ranges, so the value shows which record resolved. */
	FName TableStatId()
	{
		return FName(TEXT("table_stat"));
	}

	FName PoolStatId()
	{
		return FName(TEXT("pool_stat"));
	}

	FInstancedStruct MakeStatModifier(FName StatId)
	{
		FArcCraftModifier_Stats StatMod;
		StatMod.StatId = StatId;
		StatMod.SlotTags.AddTag(TAG_RecIdSrc_Slot);
		return FInstancedStruct::Make(StatMod);
	}

	FInstancedStruct MakeSimpleRandom()
	{
		FArcRandomPoolSelection_SimpleRandom SimpleMode;
		SimpleMode.MaxSelections = 1;
		return FInstancedStruct::Make(SimpleMode);
	}

	FInstancedStruct MakePoolModifier(const FPrimaryAssetId& PoolId)
	{
		FArcCraftModifier_RandomPool PoolMod;
		PoolMod.PoolDefinition = FArcNamedPrimaryAssetId(PoolId);
		PoolMod.SelectionMode = MakeSimpleRandom();
		return FInstancedStruct::Make(PoolMod);
	}

	FArcRandomPoolDefinitionData MakePoolWithModifier(FInstancedStruct Modifier)
	{
		FArcRandomPoolEntry Entry;
		Entry.DisplayName = FText::FromString(TEXT("Entry"));
		Entry.BaseWeight = 1.0f;
		Entry.MinR = 0.0f;
		Entry.Modifiers.Add(MoveTemp(Modifier));

		FArcRandomPoolDefinitionData Pool;
		Pool.Entries.Add(Entry);
		return Pool;
	}

	FArcMaterialPropertyTableData MakeMetalTable(FInstancedStruct Modifier)
	{
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_RecIdSrc_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString(TEXT("Metal"));
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.Modifiers.Add(MoveTemp(Modifier));

		FArcMaterialPropertyTableData Table;
		Table.Rules.Add(Rule);
		return Table;
	}

	FName ThirdStatId()
	{
		return FName(TEXT("third_stat"));
	}

	FArcMaterialPropertyRule MakeMetalRule(const TArray<FInstancedStruct>& Modifiers)
	{
		FGameplayTagContainer MetalTags;
		MetalTags.AddTag(TAG_RecIdSrc_Metal);

		FArcMaterialPropertyRule Rule;
		Rule.RuleName = FText::FromString(TEXT("Metal"));
		Rule.TagQuery = FGameplayTagQuery::MakeQuery_MatchAnyTags(MetalTags);
		Rule.Modifiers = Modifiers;
		return Rule;
	}

	FArcItemSpec MakeMetalIngredient(FArcItemTestRecordFiles& ItemRecords)
	{
		FArcItemDefinitionData Def;

		FArcItemFragment_Tags TagsFragment;
		TagsFragment.ItemTags.AddTag(TAG_RecIdSrc_Metal);
		TagsFragment.AssetTags.AddTag(TAG_RecIdSrc_Metal);
		ArcItemTestRecords::AddFragment(Def, TagsFragment);

		FArcItemSpec Spec;
		Spec.SetItemDefinition(ItemRecords.Write(Def));
		return Spec;
	}

	FPrimaryAssetId MakeUnindexedId(const TCHAR* TypeName)
	{
		return FPrimaryAssetId(FPrimaryAssetType(TypeName), FName(*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	}
}

TEST_CLASS(RecipeModifierSourceRecordIds, "ArcCraft.Source.RecordIds")
{
	FArcItemTestRecordFiles ItemRecords;
	FArcCraftTestRecordFiles Records;
	FArcCraftTestRulesFixture RulesFixture;

	TEST_METHOD(MaterialPropertiesSource_IndexedTableId_EmitsRuleModifier)
	{
		ASSERT_THAT(IsTrue(RulesFixture.Install(Records, {
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::TableStatId(), 7.0f, 7.0f),
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::PoolStatId(), 11.0f, 11.0f) }),
			TEXT("Stat catalog and craft rules records must be written and installed")));

		const FPrimaryAssetId TableId = Records.Write(
			ArcRecipeModifierSourceRecordIdTestsImpl::MakeMetalTable(ArcRecipeModifierSourceRecordIdTestsImpl::MakeStatModifier(ArcRecipeModifierSourceRecordIdTestsImpl::TableStatId())));
		ASSERT_THAT(IsTrue(TableId.IsValid(), TEXT("Material property table record must be written and indexed")));

		FArcRecipeModifierSource_MaterialProperties Source;
		Source.PropertyTable = FArcNamedPrimaryAssetId(TableId);

		TArray<FArcItemSpec> Ingredients = { ArcRecipeModifierSourceRecordIdTestsImpl::MakeMetalIngredient(ItemRecords) };
		ASSERT_THAT(IsTrue(Ingredients[0].GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		TArray<float> SlotRaritySteps = { 1.0f };

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Indexed table id must resolve and its matching rule must emit one modifier")));
		ASSERT_THAT(IsTrue(Results[0].Result.Type == EArcCraftModifierResultType::AttributeModifier, TEXT("Emitted modifier must be the rule's wearer stat modifier")));
		ASSERT_THAT(AreEqual(1, Results[0].Result.AttributeModifiers.Num(), TEXT("The rule's stat converts to exactly one wearer modifier")));
		ASSERT_THAT(IsNear(7.0f, Results[0].Result.AttributeModifiers[0].Magnitude, 0.001f, TEXT("Stat value must come from the stat the indexed table record names")));
		ASSERT_THAT(IsTrue(Results[0].SlotTags.HasTagExact(TAG_RecIdSrc_Slot.GetTag()), TEXT("Pending modifier must carry the modifier slot tag")));
	}

	TEST_METHOD(MaterialPropertiesSource_TwoMatchedRules_ConvertsEveryModifierOneGroupPerRule)
	{
		ASSERT_THAT(IsTrue(RulesFixture.Install(Records, {
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::TableStatId(), 7.0f, 7.0f),
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::PoolStatId(), 11.0f, 11.0f),
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::ThirdStatId(), 13.0f, 13.0f) }),
			TEXT("Stat catalog and craft rules records must be written and installed")));

		FArcMaterialPropertyTableData TableData;
		TableData.Rules.Add(ArcRecipeModifierSourceRecordIdTestsImpl::MakeMetalRule({
			ArcRecipeModifierSourceRecordIdTestsImpl::MakeStatModifier(ArcRecipeModifierSourceRecordIdTestsImpl::TableStatId()),
			ArcRecipeModifierSourceRecordIdTestsImpl::MakeStatModifier(ArcRecipeModifierSourceRecordIdTestsImpl::PoolStatId()) }));
		TableData.Rules.Add(ArcRecipeModifierSourceRecordIdTestsImpl::MakeMetalRule({
			ArcRecipeModifierSourceRecordIdTestsImpl::MakeStatModifier(ArcRecipeModifierSourceRecordIdTestsImpl::ThirdStatId()) }));

		const FPrimaryAssetId TableId = Records.Write(TableData);
		ASSERT_THAT(IsTrue(TableId.IsValid(), TEXT("Material property table record must be written and indexed")));

		FArcRecipeModifierSource_MaterialProperties Source;
		Source.PropertyTable = FArcNamedPrimaryAssetId(TableId);

		TArray<FArcItemSpec> Ingredients = { ArcRecipeModifierSourceRecordIdTestsImpl::MakeMetalIngredient(ItemRecords) };
		ASSERT_THAT(IsTrue(Ingredients[0].GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		TArray<float> SlotRaritySteps = { 1.0f };

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(3, Results.Num(), TEXT("Both matched rules must convert every eligible modifier: 2 from rule 0 plus 1 from rule 1")));
		for (const FArcCraftPendingModifier& Pending : Results)
		{
			ASSERT_THAT(AreEqual(1, Pending.Result.AttributeModifiers.Num(), TEXT("Each stat converts to exactly one wearer modifier")));
		}

		ASSERT_THAT(IsNear(7.0f, Results[0].Result.AttributeModifiers[0].Magnitude, 0.001f, TEXT("Table order: rule 0 first modifier first")));
		ASSERT_THAT(IsNear(11.0f, Results[1].Result.AttributeModifiers[0].Magnitude, 0.001f, TEXT("Table order: rule 0 second modifier second")));
		ASSERT_THAT(IsNear(13.0f, Results[2].Result.AttributeModifiers[0].Magnitude, 0.001f, TEXT("Table order: rule 1 modifier last")));

		ASSERT_THAT(AreEqual(0, Results[0].GroupIndex, TEXT("Rule 0 first modifier must carry rule 0 as its group")));
		ASSERT_THAT(AreEqual(0, Results[1].GroupIndex, TEXT("Rule 0 second modifier must share rule 0's group")));
		ASSERT_THAT(AreEqual(1, Results[2].GroupIndex, TEXT("Rule 1 modifier must carry a distinct group index")));
	}

	TEST_METHOD(MaterialPropertiesSource_UnresolvedTableId_ReturnsEmpty)
	{
		FArcRecipeModifierSource_MaterialProperties Source;
		Source.PropertyTable = FArcNamedPrimaryAssetId(ArcRecipeModifierSourceRecordIdTestsImpl::MakeUnindexedId(TEXT("ArcMaterialPropertyTable")));

		TArray<FArcItemSpec> Ingredients = { ArcRecipeModifierSourceRecordIdTestsImpl::MakeMetalIngredient(ItemRecords) };
		ASSERT_THAT(IsTrue(Ingredients[0].GetItemDefinitionId().IsValid(), TEXT("Item record must be written and indexed")));
		TArray<float> SlotRaritySteps = { 1.0f };

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Unresolved table id must yield no modifiers and must not crash")));
	}

	TEST_METHOD(RandomPoolSource_UnresolvedPoolId_ReturnsEmpty)
	{
		FArcRecipeModifierSource_RandomPool Source;
		Source.PoolDefinition = FArcNamedPrimaryAssetId(ArcRecipeModifierSourceRecordIdTestsImpl::MakeUnindexedId(TEXT("ArcRandomPool")));
		Source.SelectionMode = ArcRecipeModifierSourceRecordIdTestsImpl::MakeSimpleRandom();

		const TArray<FArcItemSpec> Ingredients;
		const TArray<float> SlotRaritySteps;

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Unresolved pool id must yield no modifiers and must not crash")));
	}

	TEST_METHOD(StaticSource_UnresolvedModifierSetId_ReturnsEmpty)
	{
		FArcRecipeModifierSource_Static Source;
		Source.ModifierSet = FArcNamedPrimaryAssetId(ArcRecipeModifierSourceRecordIdTestsImpl::MakeUnindexedId(TEXT("ArcStaticModifierSet")));

		const TArray<FArcItemSpec> Ingredients;
		const TArray<float> SlotRaritySteps;

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Unresolved modifier set id must yield no modifiers and must not crash")));
	}

	TEST_METHOD(RandomPoolSource_PoolEntryHoldsPoolModifierId_EmitsInnerPoolStat)
	{
		ASSERT_THAT(IsTrue(RulesFixture.Install(Records, {
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::TableStatId(), 7.0f, 7.0f),
			FArcCraftTestRulesFixture::MakeEntry(ArcRecipeModifierSourceRecordIdTestsImpl::PoolStatId(), 11.0f, 11.0f) }),
			TEXT("Stat catalog and craft rules records must be written and installed")));

		const FPrimaryAssetId InnerPoolId = Records.Write(
			ArcRecipeModifierSourceRecordIdTestsImpl::MakePoolWithModifier(ArcRecipeModifierSourceRecordIdTestsImpl::MakeStatModifier(ArcRecipeModifierSourceRecordIdTestsImpl::PoolStatId())));
		ASSERT_THAT(IsTrue(InnerPoolId.IsValid(), TEXT("Inner pool record must be written and indexed")));

		const FPrimaryAssetId OuterPoolId = Records.Write(
			ArcRecipeModifierSourceRecordIdTestsImpl::MakePoolWithModifier(ArcRecipeModifierSourceRecordIdTestsImpl::MakePoolModifier(InnerPoolId)));
		ASSERT_THAT(IsTrue(OuterPoolId.IsValid(), TEXT("Outer pool record must be written and indexed")));

		FArcRecipeModifierSource_RandomPool Source;
		Source.PoolDefinition = FArcNamedPrimaryAssetId(OuterPoolId);
		Source.SelectionMode = ArcRecipeModifierSourceRecordIdTestsImpl::MakeSimpleRandom();

		const TArray<FArcItemSpec> Ingredients;
		const TArray<float> SlotRaritySteps;

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(1, Results.Num(), TEXT("Nested pool id must resolve to the inner pool and emit its one stat")));
		ASSERT_THAT(IsTrue(Results[0].Result.Type == EArcCraftModifierResultType::AttributeModifier, TEXT("The inner pool's stat must convert to a wearer modifier")));
		ASSERT_THAT(AreEqual(1, Results[0].Result.AttributeModifiers.Num(), TEXT("The inner pool's stat converts to exactly one wearer modifier")));
		ASSERT_THAT(IsNear(11.0f, Results[0].Result.AttributeModifiers[0].Magnitude, 0.001f, TEXT("Stat value must come from the stat the inner pool record names")));
	}

	TEST_METHOD(RandomPoolSource_PoolEntryHoldsUnresolvedPoolModifierId_ReturnsEmpty)
	{
		const FPrimaryAssetId OuterPoolId = Records.Write(
			ArcRecipeModifierSourceRecordIdTestsImpl::MakePoolWithModifier(
				ArcRecipeModifierSourceRecordIdTestsImpl::MakePoolModifier(ArcRecipeModifierSourceRecordIdTestsImpl::MakeUnindexedId(TEXT("ArcRandomPool")))));
		ASSERT_THAT(IsTrue(OuterPoolId.IsValid(), TEXT("Outer pool record must be written and indexed")));

		FArcRecipeModifierSource_RandomPool Source;
		Source.PoolDefinition = FArcNamedPrimaryAssetId(OuterPoolId);
		Source.SelectionMode = ArcRecipeModifierSourceRecordIdTestsImpl::MakeSimpleRandom();

		const TArray<FArcItemSpec> Ingredients;
		const TArray<float> SlotRaritySteps;

		const TArray<FArcCraftPendingModifier> Results = Source.Evaluate(Ingredients, FArcCraftContext{SlotRaritySteps, 1.0f});

		ASSERT_THAT(AreEqual(0, Results.Num(), TEXT("Unresolved nested pool id must yield no modifiers and must not crash")));
	}
};
