#include "CQTest.h"
#include "ArcItemTestRecordFiles.h"
#include "ArcScalableFloat.h"
#include "Items/ArcItemDefinitionData.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemStackMethod.h"

namespace ArcItemStackMethodTestHelpers
{
	FPrimaryAssetId CreateStackableItemDef(FArcItemTestRecordFiles& Records, float InMaxStacks)
	{
		FArcItemDefinitionData Def;
		FArcItemStackMethod_StackByType StackByType;
		StackByType.MaxStacks = FArcScalableFloat(InMaxStacks);
		ArcItemTestRecords::SetStackMethod(Def, StackByType);
		return Records.Write(Def);
	}

	FPrimaryAssetId CreateUniqueItemDef(FArcItemTestRecordFiles& Records)
	{
		FArcItemDefinitionData Def;
		ArcItemTestRecords::SetStackMethod(Def, FArcItemStackMethod_CanNotStackUnique());
		return Records.Write(Def);
	}

	FPrimaryAssetId CreateNonStackableItemDef(FArcItemTestRecordFiles& Records)
	{
		FArcItemDefinitionData Def;
		ArcItemTestRecords::SetStackMethod(Def, FArcItemStackMethod_CanNotStack());
		return Records.Write(Def);
	}
}

// ===================================================================
// FArcItemStackMethod_CanNotStack tests
// ===================================================================

TEST_CLASS(TryStackSpec_CanNotStack, "ArcCore.Items.StackMethod.CanNotStack")
{
	FArcItemTestRecordFiles Records;

	TEST_METHOD(TryStackSpec_ReturnsFalse_AlwaysAppends)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateNonStackableItemDef(Records);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 5));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 3);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsFalse(bHandled, TEXT("CanNotStack should return false")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Array should be unchanged")));
	}
};

// ===================================================================
// FArcItemStackMethod_CanNotStackUnique tests
// ===================================================================

TEST_CLASS(TryStackSpec_CanNotStackUnique, "ArcCore.Items.StackMethod.CanNotStackUnique")
{
	FArcItemTestRecordFiles Records;

	TEST_METHOD(TryStackSpec_RejectsDuplicate)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateUniqueItemDef(Records);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 1));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 1);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should reject duplicate")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Array should still have one entry")));
	}

	TEST_METHOD(TryStackSpec_AllowsNewDefinition)
	{
		const FPrimaryAssetId DefAId = ArcItemStackMethodTestHelpers::CreateUniqueItemDef(Records);
		ASSERT_THAT(IsTrue(DefAId.IsValid()));
		const FPrimaryAssetId DefBId = ArcItemStackMethodTestHelpers::CreateUniqueItemDef(Records);
		const FArcItemDefinitionData* DefB = FArcItemDefinitionData::Find(DefBId);
		ASSERT_THAT(IsNotNull(DefB));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefAId, 1, 1));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefBId, 1, 1);
		const FArcItemStackMethod* StackMethod = DefB->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsFalse(bHandled, TEXT("Different definition should not be handled")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Array unchanged — caller appends")));
	}
};

// ===================================================================
// FArcItemStackMethod_StackByType tests
// ===================================================================

TEST_CLASS(TryStackSpec_StackByType, "ArcCore.Items.StackMethod.StackByType")
{
	FArcItemTestRecordFiles Records;

	TEST_METHOD(TryStackSpec_MergesWithinMax)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 100.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 40));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 25);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should merge")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Should still be one entry")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(65), Specs[0].Amount, TEXT("Amount should be 40+25=65")));
	}

	TEST_METHOD(TryStackSpec_OverflowAppendsRemainder)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 100.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 90));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 20);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should handle with overflow")));
		ASSERT_THAT(AreEqual(2, Specs.Num(), TEXT("Should have original + remainder")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(100), Specs[0].Amount, TEXT("First entry should be at max")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[1].Amount, TEXT("Remainder should be 10")));
	}

	TEST_METHOD(TryStackSpec_NoMatchReturnsFalse)
	{
		const FPrimaryAssetId DefAId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 100.0f);
		ASSERT_THAT(IsTrue(DefAId.IsValid()));
		const FPrimaryAssetId DefBId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 100.0f);
		const FArcItemDefinitionData* DefB = FArcItemDefinitionData::Find(DefBId);
		ASSERT_THAT(IsNotNull(DefB));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefAId, 1, 50));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefBId, 1, 10);
		const FArcItemStackMethod* StackMethod = DefB->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsFalse(bHandled, TEXT("Different definition should not match")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Array unchanged")));
	}

	TEST_METHOD(TryStackSpec_ExactMaxMerges)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 100.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 80));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 20);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should merge exactly to max")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("No overflow — still one entry")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(100), Specs[0].Amount, TEXT("Amount should be exactly 100")));
	}

	TEST_METHOD(TryStackSpec_EmptyArrayReturnsFalse)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 100.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 10);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsFalse(bHandled, TEXT("Empty array has nothing to merge into")));
		ASSERT_THAT(AreEqual(0, Specs.Num(), TEXT("Array still empty")));
	}

	TEST_METHOD(TryStackSpec_MergesIntoSecondPartialStack)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 10.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 3));  // partial

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 1);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should merge into second partial")));
		ASSERT_THAT(AreEqual(2, Specs.Num(), TEXT("Should still be two entries")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[0].Amount, TEXT("First stack unchanged at max")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(4), Specs[1].Amount, TEXT("Second stack should be 3+1=4")));
	}

	TEST_METHOD(TryStackSpec_CascadesAcrossMultiplePartials)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 10.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 8));  // 2 space
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 7));  // 3 space

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 5);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should cascade across partials")));
		ASSERT_THAT(AreEqual(3, Specs.Num(), TEXT("No new entries needed")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[0].Amount, TEXT("First stays at 10")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[1].Amount, TEXT("Second filled to 10")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[2].Amount, TEXT("Third filled to 10")));
	}

	TEST_METHOD(TryStackSpec_AllFullAppendsNewEntry)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 10.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 10)); // full

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 3);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should handle — match found even though all full")));
		ASSERT_THAT(AreEqual(3, Specs.Num(), TEXT("New entry appended")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[0].Amount, TEXT("First unchanged")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[1].Amount, TEXT("Second unchanged")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(3), Specs[2].Amount, TEXT("Remainder appended")));
	}

	TEST_METHOD(TryStackSpec_OverflowFromPartialCascades)
	{
		const FPrimaryAssetId DefId = ArcItemStackMethodTestHelpers::CreateStackableItemDef(Records, 10.0f);
		const FArcItemDefinitionData* Def = FArcItemDefinitionData::Find(DefId);
		ASSERT_THAT(IsNotNull(Def));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(DefId, 1, 8));  // 2 space

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefId, 1, 7);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should handle with overflow")));
		ASSERT_THAT(AreEqual(3, Specs.Num(), TEXT("Remainder creates new entry")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[0].Amount, TEXT("First unchanged")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[1].Amount, TEXT("Second filled to max")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(5), Specs[2].Amount, TEXT("Remainder is 7-2=5")));
	}
};
