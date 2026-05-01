// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "Tags/ArcTagCountContainer.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CountTest_Fire, "Test.Count.Fire");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CountTest_Ice, "Test.Count.Ice");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_CountTest_Poison, "Test.Count.Poison");

TEST_CLASS(ArcTagCountContainer, "ArcMassAbilities.Tags.CountContainer")
{
	TEST_METHOD(AddTag_FirstAdd_CountIsOne)
	{
		FArcTagCountContainer Container;
		int32 NewCount = Container.AddTag(TAG_CountTest_Fire);
		ASSERT_THAT(AreEqual(1, NewCount));
		ASSERT_THAT(IsTrue(Container.HasTag(TAG_CountTest_Fire)));
		ASSERT_THAT(AreEqual(1, Container.GetCount(TAG_CountTest_Fire)));
	}

	TEST_METHOD(AddTag_SecondAdd_CountIsTwo)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);
		int32 NewCount = Container.AddTag(TAG_CountTest_Fire);
		ASSERT_THAT(AreEqual(2, NewCount));
		ASSERT_THAT(IsTrue(Container.HasTag(TAG_CountTest_Fire)));
		ASSERT_THAT(AreEqual(2, Container.GetCount(TAG_CountTest_Fire)));
	}

	TEST_METHOD(RemoveTag_FromTwo_CountIsOne_TagStays)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);
		Container.AddTag(TAG_CountTest_Fire);
		int32 NewCount = Container.RemoveTag(TAG_CountTest_Fire);
		ASSERT_THAT(AreEqual(1, NewCount));
		ASSERT_THAT(IsTrue(Container.HasTag(TAG_CountTest_Fire)));
	}

	TEST_METHOD(RemoveTag_FromOne_CountIsZero_TagGone)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);
		int32 NewCount = Container.RemoveTag(TAG_CountTest_Fire);
		ASSERT_THAT(AreEqual(0, NewCount));
		ASSERT_THAT(IsFalse(Container.HasTag(TAG_CountTest_Fire)));
	}

	TEST_METHOD(RemoveTag_FromZero_ClampsAtZero)
	{
		FArcTagCountContainer Container;
		int32 NewCount = Container.RemoveTag(TAG_CountTest_Fire);
		ASSERT_THAT(AreEqual(0, NewCount));
		ASSERT_THAT(IsFalse(Container.HasTag(TAG_CountTest_Fire)));
	}

	TEST_METHOD(GetCount_AbsentTag_ReturnsZero)
	{
		FArcTagCountContainer Container;
		ASSERT_THAT(AreEqual(0, Container.GetCount(TAG_CountTest_Fire)));
	}

	TEST_METHOD(AddTags_MultipleTags_EachCountedOnce)
	{
		FArcTagCountContainer Container;
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_CountTest_Fire);
		Tags.AddTag(TAG_CountTest_Ice);
		Container.AddTags(Tags);

		ASSERT_THAT(AreEqual(1, Container.GetCount(TAG_CountTest_Fire)));
		ASSERT_THAT(AreEqual(1, Container.GetCount(TAG_CountTest_Ice)));
		ASSERT_THAT(IsTrue(Container.HasTag(TAG_CountTest_Fire)));
		ASSERT_THAT(IsTrue(Container.HasTag(TAG_CountTest_Ice)));
	}

	TEST_METHOD(RemoveTags_MultipleTags_AllRemoved)
	{
		FArcTagCountContainer Container;
		FGameplayTagContainer Tags;
		Tags.AddTag(TAG_CountTest_Fire);
		Tags.AddTag(TAG_CountTest_Ice);
		Container.AddTags(Tags);
		Container.RemoveTags(Tags);

		ASSERT_THAT(AreEqual(0, Container.GetCount(TAG_CountTest_Fire)));
		ASSERT_THAT(AreEqual(0, Container.GetCount(TAG_CountTest_Ice)));
		ASSERT_THAT(IsFalse(Container.HasTag(TAG_CountTest_Fire)));
		ASSERT_THAT(IsFalse(Container.HasTag(TAG_CountTest_Ice)));
	}

	TEST_METHOD(HasAll_ReturnsTrue_WhenAllPresent)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);
		Container.AddTag(TAG_CountTest_Ice);

		FGameplayTagContainer Query;
		Query.AddTag(TAG_CountTest_Fire);
		Query.AddTag(TAG_CountTest_Ice);
		ASSERT_THAT(IsTrue(Container.HasAll(Query)));
	}

	TEST_METHOD(HasAll_ReturnsFalse_WhenOneMissing)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);

		FGameplayTagContainer Query;
		Query.AddTag(TAG_CountTest_Fire);
		Query.AddTag(TAG_CountTest_Ice);
		ASSERT_THAT(IsFalse(Container.HasAll(Query)));
	}

	TEST_METHOD(HasAny_ReturnsTrue_WhenOnePresent)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);

		FGameplayTagContainer Query;
		Query.AddTag(TAG_CountTest_Fire);
		Query.AddTag(TAG_CountTest_Ice);
		ASSERT_THAT(IsTrue(Container.HasAny(Query)));
	}

	TEST_METHOD(HasAny_ReturnsFalse_WhenNonePresent)
	{
		FArcTagCountContainer Container;

		FGameplayTagContainer Query;
		Query.AddTag(TAG_CountTest_Fire);
		ASSERT_THAT(IsFalse(Container.HasAny(Query)));
	}

	TEST_METHOD(GetTagContainer_ReturnsInnerContainer)
	{
		FArcTagCountContainer Container;
		Container.AddTag(TAG_CountTest_Fire);

		const FGameplayTagContainer& Inner = Container.GetTagContainer();
		ASSERT_THAT(IsTrue(Inner.HasTag(TAG_CountTest_Fire)));
	}

	TEST_METHOD(IsEmpty_TrueWhenEmpty_FalseWhenNot)
	{
		FArcTagCountContainer Container;
		ASSERT_THAT(IsTrue(Container.IsEmpty()));

		Container.AddTag(TAG_CountTest_Fire);
		ASSERT_THAT(IsFalse(Container.IsEmpty()));
	}
};
