#include "CQTest.h"
#include "Items/ArcItemSpec.h"
#include "Items/ArcItemDefinition.h"
#include "Items/ArcItemStackMethod.h"

namespace ArcItemStackMethodTestHelpers
{
	FInstancedStruct* GetStackMethodProperty(UArcItemDefinition* Def)
	{
		FProperty* StackProp = Def->GetClass()->FindPropertyByName(TEXT("StackMethod"));
		check(StackProp);
		return StackProp->ContainerPtrToValuePtr<FInstancedStruct>(Def);
	}

	UArcItemDefinition* CreateTransientItemDef(const FName& Name)
	{
		UArcItemDefinition* Def = NewObject<UArcItemDefinition>(
			GetTransientPackage(), Name, RF_Transient);
		Def->RegenerateItemId();
		return Def;
	}

	UArcItemDefinition* CreateStackableItemDef(const FName& Name, float InMaxStacks)
	{
		UArcItemDefinition* Def = CreateTransientItemDef(Name);
		FInstancedStruct* StackMethod = GetStackMethodProperty(Def);
		StackMethod->InitializeAs<FArcItemStackMethod_StackByType>();
		FArcItemStackMethod_StackByType* StackByType = StackMethod->GetMutablePtr<FArcItemStackMethod_StackByType>();
		StackByType->MaxStacks = FScalableFloat(InMaxStacks);
		return Def;
	}

	UArcItemDefinition* CreateUniqueItemDef(const FName& Name)
	{
		UArcItemDefinition* Def = CreateTransientItemDef(Name);
		FInstancedStruct* StackMethod = GetStackMethodProperty(Def);
		StackMethod->InitializeAs<FArcItemStackMethod_CanNotStackUnique>();
		return Def;
	}

	UArcItemDefinition* CreateNonStackableItemDef(const FName& Name)
	{
		UArcItemDefinition* Def = CreateTransientItemDef(Name);
		FInstancedStruct* StackMethod = GetStackMethodProperty(Def);
		StackMethod->InitializeAs<FArcItemStackMethod_CanNotStack>();
		return Def;
	}
}

// ===================================================================
// FArcItemStackMethod_CanNotStack tests
// ===================================================================

TEST_CLASS(TryStackSpec_CanNotStack, "ArcCore.Items.StackMethod.CanNotStack")
{
	TEST_METHOD(TryStackSpec_ReturnsFalse_AlwaysAppends)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateNonStackableItemDef(TEXT("TestDef_NoStack"));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 5));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 3);
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
	TEST_METHOD(TryStackSpec_RejectsDuplicate)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateUniqueItemDef(TEXT("TestDef_Unique"));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 1));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 1);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should reject duplicate")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Array should still have one entry")));
	}

	TEST_METHOD(TryStackSpec_AllowsNewDefinition)
	{
		UArcItemDefinition* DefA = ArcItemStackMethodTestHelpers::CreateUniqueItemDef(TEXT("TestDef_UniqueA"));
		UArcItemDefinition* DefB = ArcItemStackMethodTestHelpers::CreateUniqueItemDef(TEXT("TestDef_UniqueB"));

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefA, 1, 1));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefB, 1, 1);
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
	TEST_METHOD(TryStackSpec_MergesWithinMax)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_Stackable"), 100.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 40));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 25);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should merge")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Should still be one entry")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(65), Specs[0].Amount, TEXT("Amount should be 40+25=65")));
	}

	TEST_METHOD(TryStackSpec_OverflowAppendsRemainder)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_Overflow"), 100.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 90));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 20);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should handle with overflow")));
		ASSERT_THAT(AreEqual(2, Specs.Num(), TEXT("Should have original + remainder")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(100), Specs[0].Amount, TEXT("First entry should be at max")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[1].Amount, TEXT("Remainder should be 10")));
	}

	TEST_METHOD(TryStackSpec_NoMatchReturnsFalse)
	{
		UArcItemDefinition* DefA = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_StackA"), 100.0f);
		UArcItemDefinition* DefB = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_StackB"), 100.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(DefA, 1, 50));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(DefB, 1, 10);
		const FArcItemStackMethod* StackMethod = DefB->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsFalse(bHandled, TEXT("Different definition should not match")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("Array unchanged")));
	}

	TEST_METHOD(TryStackSpec_ExactMaxMerges)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_ExactMax"), 100.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 80));

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 20);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should merge exactly to max")));
		ASSERT_THAT(AreEqual(1, Specs.Num(), TEXT("No overflow — still one entry")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(100), Specs[0].Amount, TEXT("Amount should be exactly 100")));
	}

	TEST_METHOD(TryStackSpec_EmptyArrayReturnsFalse)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_Empty"), 100.0f);

		TArray<FArcItemSpec> Specs;

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 10);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsFalse(bHandled, TEXT("Empty array has nothing to merge into")));
		ASSERT_THAT(AreEqual(0, Specs.Num(), TEXT("Array still empty")));
	}

	TEST_METHOD(TryStackSpec_MergesIntoSecondPartialStack)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_SecondPartial"), 10.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 3));  // partial

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 1);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should merge into second partial")));
		ASSERT_THAT(AreEqual(2, Specs.Num(), TEXT("Should still be two entries")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[0].Amount, TEXT("First stack unchanged at max")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(4), Specs[1].Amount, TEXT("Second stack should be 3+1=4")));
	}

	TEST_METHOD(TryStackSpec_CascadesAcrossMultiplePartials)
	{
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_Cascade"), 10.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 8));  // 2 space
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 7));  // 3 space

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 5);
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
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_AllFull"), 10.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 10)); // full

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 3);
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
		UArcItemDefinition* Def = ArcItemStackMethodTestHelpers::CreateStackableItemDef(TEXT("TestDef_PartialOverflow"), 10.0f);

		TArray<FArcItemSpec> Specs;
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 10)); // full
		Specs.Add(FArcItemSpec::NewItem(Def, 1, 8));  // 2 space

		FArcItemSpec NewSpec = FArcItemSpec::NewItem(Def, 1, 7);
		const FArcItemStackMethod* StackMethod = Def->GetStackMethod<FArcItemStackMethod>();

		bool bHandled = StackMethod->TryStackSpec(Specs, MoveTemp(NewSpec));

		ASSERT_THAT(IsTrue(bHandled, TEXT("Should handle with overflow")));
		ASSERT_THAT(AreEqual(3, Specs.Num(), TEXT("Remainder creates new entry")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[0].Amount, TEXT("First unchanged")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(10), Specs[1].Amount, TEXT("Second filled to max")));
		ASSERT_THAT(AreEqual(static_cast<uint16>(5), Specs[2].Amount, TEXT("Remainder is 7-2=5")));
	}
};
