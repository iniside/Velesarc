// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"
#include "FlexBox/SFlexBox.h"
#include "Layout/Geometry.h"
#include "Layout/ArrangedChildren.h"
#include "Widgets/Layout/SBox.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace SFlexBoxArrangeTestPrivate
{
	/** Build a root FGeometry of the given pixel size at scale 1. */
	static FGeometry MakeRootGeometry(float W, float H)
	{
		return FGeometry::MakeRoot(FVector2f(W, H), FSlateLayoutTransform());
	}

	/**
	 * Run a prepass on Flex (so desired sizes are known) then arrange it into
	 * a W×H allotment and return the arranged children.
	 */
	static FArrangedChildren ArrangeFlexBox(TSharedRef<SFlexBox> Flex, float W, float H)
	{
		Flex->SlatePrepass(1.0f);
		FArrangedChildren ArrangedChildren(EVisibility::All);
		FGeometry RootGeometry = MakeRootGeometry(W, H);
		Flex->OnArrangeChildren(RootGeometry, ArrangedChildren);
		return ArrangedChildren;
	}

	/** Return the top-left position of arranged child i in parent local space. */
	static FVector2f GetPos(const FArrangedChildren& Arr, int32 Index)
	{
		return Arr[Index].Geometry.Position;
	}

	/** Return the local size of arranged child i. */
	static FVector2f GetSize(const FArrangedChildren& Arr, int32 Index)
	{
		return FVector2f(Arr[Index].Geometry.GetLocalSize());
	}

	static bool NearlyEqual(float A, float B, float Tolerance = 0.5f)
	{
		return FMath::Abs(A - B) <= Tolerance;
	}
} // namespace SFlexBoxArrangeTestPrivate

// ---------------------------------------------------------------------------
// Test 1 — Row grow: equal flex-grow distributes extra space evenly
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_RowGrow, "ArcMarkupUI.FlexBox.RowGrow")
{
	TEST_METHOD(ThreeEqualGrowItems_ExtraWidthDistributedEvenly)
	{
		// 3 items of 50px each in a 300px container → 150px extra, 50px per item → each item 100px
		// positions: 0, 100, 200
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(1.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(1.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(1.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 300.f, 100.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		float ExpectedWidth = 100.f; // (300 - 3*50) / 3 + 50
		for (int32 i = 0; i < 3; ++i)
		{
			FVector2f Sz = SFlexBoxArrangeTestPrivate::GetSize(Arr, i);
			FVector2f Pos = SFlexBoxArrangeTestPrivate::GetPos(Arr, i);
			ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz.X, ExpectedWidth)));
			ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos.X, i * ExpectedWidth)));
		}
	}
};

// ---------------------------------------------------------------------------
// Test 2 — Shrink: items shrink but not below their min-content (desired) size;
//           an item with FlexShrink=0 keeps its size.
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_ShrinkToMin, "ArcMarkupUI.FlexBox.ShrinkToMin")
{
	TEST_METHOD(TwoShrinkableOneLocked_ContainerTooSmall)
	{
		// Items: A=80px FlexShrink=1, B=80px FlexShrink=0, C=80px FlexShrink=1
		// Container = 180px. Total basis = 240px, deficit = 60px.
		// B keeps 80px. A and C each absorb 30px → 50px each.
		// Positions: A=0, B=50, C=130
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(1.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(1.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 180.f, 100.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		// B (index 1) must keep its 80px (FlexShrink=0)
		FVector2f SzB = SFlexBoxArrangeTestPrivate::GetSize(Arr, 1);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzB.X, 80.f)));

		// A and C must be 50px each (each absorbs 30px of the 60px deficit)
		FVector2f SzA = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		FVector2f SzC = SFlexBoxArrangeTestPrivate::GetSize(Arr, 2);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzA.X, 50.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzC.X, 50.f)));

		// Positions must accumulate
		FVector2f PosA = SFlexBoxArrangeTestPrivate::GetPos(Arr, 0);
		FVector2f PosB = SFlexBoxArrangeTestPrivate::GetPos(Arr, 1);
		FVector2f PosC = SFlexBoxArrangeTestPrivate::GetPos(Arr, 2);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(PosA.X, 0.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(PosB.X, 50.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(PosC.X, 130.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 3 — justify-content: SpaceBetween
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_JustifySpaceBetween, "ArcMarkupUI.FlexBox.JustifySpaceBetween")
{
	TEST_METHOD(ThreeItems_FirstAtZero_LastFlushRight_EqualGaps)
	{
		// 3 items of 60px in a 300px container.
		// SpaceBetween: gaps = (300 - 180) / 2 = 60px between items.
		// Positions: 0, 120, 240
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::SpaceBetween)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 300.f, 100.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		FVector2f Pos0 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 0);
		FVector2f Pos1 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 1);
		FVector2f Pos2 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 2);

		// First item at 0, last item flush right (240 = 300 - 60)
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos0.X, 0.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos1.X, 120.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos2.X, 240.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 4 — justify-content: SpaceEvenly
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_JustifySpaceEvenly, "ArcMarkupUI.FlexBox.JustifySpaceEvenly")
{
	TEST_METHOD(ThreeItems_EqualSpaceIncludingEnds)
	{
		// 3 items of 60px in a 360px container.
		// SpaceEvenly: (360 - 180) / 4 = 45px each gap (including ends).
		// Positions: 45, 150, 255
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::SpaceEvenly)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 360.f, 100.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		FVector2f Pos0 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 0);
		FVector2f Pos1 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 1);
		FVector2f Pos2 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 2);

		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos0.X, 45.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos1.X, 150.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos2.X, 255.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 5 — Wrap + gap
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_WrapAndGap, "ArcMarkupUI.FlexBox.WrapAndGap")
{
	TEST_METHOD(NoWrap_ItemsDoNotWrap)
	{
		// 3 items of 80px in a 200px container with NoWrap → all on one line
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 200.f, 200.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		// All items on the same cross-axis line (Y == 0)
		for (int32 i = 0; i < 3; ++i)
		{
			FVector2f Pos = SFlexBoxArrangeTestPrivate::GetPos(Arr, i);
			ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos.Y, 0.f)));
		}
	}

	TEST_METHOD(Wrap_ThirdItemBreaksToSecondLine)
	{
		// 3 items of 80px in a 200px container with Wrap → 2 on line 1, 1 on line 2
		// Line height = 40px. With Gap.Y=10px: line 2 Y = 40 + 10 = 50
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::Wrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			.AlignContent(EFlexAlign::FlexStart)
			.Gap(FVector2f(0.f, 10.f))
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(80.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 200.f, 200.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		FVector2f Pos0 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 0);
		FVector2f Pos1 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 1);
		FVector2f Pos2 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 2);

		// Items 0 and 1 on line 1 (Y=0)
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos0.Y, 0.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos1.Y, 0.f)));

		// Item 2 on line 2 (Y = 40 + 10 = 50)
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos2.Y, 50.f)));
	}

	TEST_METHOD(ColumnGap_BetweenItemsOnSameLine)
	{
		// 3 items of 50px with column gap 20px in 300px container → no wrap needed
		// Positions: 0, 70, 140
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			.Gap(FVector2f(20.f, 0.f))
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 300.f, 100.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		FVector2f Pos0 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 0);
		FVector2f Pos1 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 1);
		FVector2f Pos2 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 2);

		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos0.X, 0.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos1.X, 70.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos2.X, 140.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 6 — Single-line AlignContent: a single line fills cross extent
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_AlignContent, "ArcMarkupUI.FlexBox.AlignContent")
{
	TEST_METHOD(SingleLine_AlignItemsStretch_ChildFillsCross)
	{
		// 1 item, no height override → Stretch → fills the 100px cross
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::Stretch)
			.AlignContent(EFlexAlign::Stretch)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 200.f, 100.f);
		ASSERT_THAT(AreEqual(1, Arr.Num()));

		FVector2f Sz = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz.Y, 100.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 7 — align-items: Stretch
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_AlignItemsStretch, "ArcMarkupUI.FlexBox.AlignItemsStretch")
{
	TEST_METHOD(ChildWithNoFixedCrossSize_StretchesToLineCross)
	{
		// Row direction, line height = 80px (from the taller item).
		// First item has no height override → should stretch to 80px.
		// Second item has height 80px → sets the line height.
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::Stretch)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(80.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 300.f, 80.f);
		ASSERT_THAT(AreEqual(2, Arr.Num()));

		FVector2f SzFirst = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		// Container cross == 80 so the single line's cross == 80 (no extra space from a taller container).
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzFirst.Y, 80.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 8 — Order
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_Order, "ArcMarkupUI.FlexBox.Order")
{
	TEST_METHOD(ItemsArrangedByOrderAscending_TiesKeepSourceOrder)
	{
		// Source order: A(Order=2,W=50), B(Order=0,W=60), C(Order=1,W=70)
		// Arrange order: B(0), C(1), A(2)
		// Expected positions: B=0, C=60, A=130
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			.Order(2)
			[
				SNew(SBox).WidthOverride(50.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			.Order(0)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			.Order(1)
			[
				SNew(SBox).WidthOverride(70.f).HeightOverride(40.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 400.f, 100.f);
		ASSERT_THAT(AreEqual(3, Arr.Num()));

		// The widths of the arranged children should be: 60, 70, 50 (in arrangement order)
		FVector2f Sz0 = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		FVector2f Sz1 = SFlexBoxArrangeTestPrivate::GetSize(Arr, 1);
		FVector2f Sz2 = SFlexBoxArrangeTestPrivate::GetSize(Arr, 2);

		// First arranged item should be the B item (W=60)
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz0.X, 60.f)));
		// Second arranged item should be the C item (W=70)
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz1.X, 70.f)));
		// Third arranged item should be the A item (W=50)
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz2.X, 50.f)));

		// Positions accumulate: 0, 60, 130
		FVector2f Pos0 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 0);
		FVector2f Pos1 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 1);
		FVector2f Pos2 = SFlexBoxArrangeTestPrivate::GetPos(Arr, 2);

		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos0.X, 0.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos1.X, 60.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Pos2.X, 130.f)));
	}
};

// ---------------------------------------------------------------------------
// Test 9 — RTL parity: RowReverse mirrors Row
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_RTL, "ArcMarkupUI.FlexBox.RTL")
{
	TEST_METHOD(RowReverse_MirrorsRowPositions)
	{
		// Row: 3 items of 60px in 300px container, ArrangedChildren in source order:
		// positions 0, 60, 120 (FlexStart packs left).
		// RowReverse + FlexStart (CSS): the reversed main axis flips the main-start edge, so
		// flex-start packs items to the RIGHT. Free space (300 - 180 = 120px) sits on the LEFT.
		// ArrangedChildren are in visual (left→right) order: [0]=source2 @120, [1]=source1 @180,
		// [2]=source0 @240.

		TSharedRef<SFlexBox> FlexRow = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			];

		TSharedRef<SFlexBox> FlexRowReverse = SNew(SFlexBox)
			.Direction(EFlexDirection::RowReverse)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::FlexStart)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(60.f).HeightOverride(40.f)
			];

		FArrangedChildren ArrRow = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(FlexRow, 300.f, 100.f);
		FArrangedChildren ArrRev = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(FlexRowReverse, 300.f, 100.f);

		ASSERT_THAT(AreEqual(3, ArrRow.Num()));
		ASSERT_THAT(AreEqual(3, ArrRev.Num()));

		// Row: item 0 at X=0, item 2 at X=120
		FVector2f RowPos0 = SFlexBoxArrangeTestPrivate::GetPos(ArrRow, 0);
		FVector2f RowPos2 = SFlexBoxArrangeTestPrivate::GetPos(ArrRow, 2);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(RowPos0.X, 0.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(RowPos2.X, 120.f)));

		// ArrangedChildren are in visual (laid-out) order (left-to-right, regardless of direction).
		// Under RowReverse+FlexStart the main-start edge flips to the right, so items pack to the
		// far end: the 120px of free space sits on the LEFT. Visual left→right is source2 @120,
		// source1 @180, source0 @240.
		FVector2f RevPos0 = SFlexBoxArrangeTestPrivate::GetPos(ArrRev, 0);
		FVector2f RevPos1 = SFlexBoxArrangeTestPrivate::GetPos(ArrRev, 1);
		FVector2f RevPos2 = SFlexBoxArrangeTestPrivate::GetPos(ArrRev, 2);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(RevPos0.X, 120.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(RevPos1.X, 180.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(RevPos2.X, 240.f)));

		// All items same size in both directions
		for (int32 i = 0; i < 3; ++i)
		{
			FVector2f SzRow = SFlexBoxArrangeTestPrivate::GetSize(ArrRow, i);
			FVector2f SzRev = SFlexBoxArrangeTestPrivate::GetSize(ArrRev, i);
			ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzRow.X, 60.f)));
			ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzRev.X, 60.f)));
		}
	}
};

// ---------------------------------------------------------------------------
// Test 10 — Oversized child clamps to container cross (single-line stretch)
// ---------------------------------------------------------------------------

TEST_CLASS(SFlexBoxArrange_OversizedChildClamp, "ArcMarkupUI.FlexBox.OversizedChildClamp")
{
	TEST_METHOD(Row_OversizedStretchChild_ClampsToContainerCross)
	{
		// Row, cross axis = Y, container 200x80, AlignItems=Stretch.
		// Child A: 40px wide, no height override → stretches.
		// Child B: 40px wide, 120px tall (taller than the container).
		// Both arranged children must clamp to the container cross (80), not balloon to 120.
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::Stretch)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(40.f)
			]
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(40.f).HeightOverride(120.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 200.f, 80.f);
		ASSERT_THAT(AreEqual(2, Arr.Num()));

		FVector2f SzA = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		FVector2f SzB = SFlexBoxArrangeTestPrivate::GetSize(Arr, 1);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzA.Y, 80.f)));
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(SzB.Y, 80.f)));
	}

	TEST_METHOD(Column_OversizedStretchChild_ClampsToContainerCross)
	{
		// Column, cross axis = X, container 80x200, AlignItems=Stretch.
		// One child 120px wide (wider than the container) → clamps to 80.
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Column)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::Stretch)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(120.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 80.f, 200.f);
		ASSERT_THAT(AreEqual(1, Arr.Num()));

		FVector2f Sz = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz.X, 80.f)));
	}

	TEST_METHOD(Row_RoomyStretchChild_StillFillsContainerCross)
	{
		// Control: Row, container 200x80, AlignItems=Stretch, child 40x30 (smaller than cross).
		// Stretch must still fill to 80 — locks the non-oversized path.
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::Stretch)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(40.f).HeightOverride(30.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 200.f, 80.f);
		ASSERT_THAT(AreEqual(1, Arr.Num()));

		FVector2f Sz = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz.Y, 80.f)));
	}

	TEST_METHOD(Row_ZeroCrossContainer_FallsBackToNaturalSize)
	{
		// 0-cross fallback: Row, container 200x0, AlignItems=Stretch, child 40x50.
		// The UE_KINDA_SMALL_NUMBER guard keeps the natural 50 (not zero).
		TSharedRef<SFlexBox> Flex = SNew(SFlexBox)
			.Direction(EFlexDirection::Row)
			.Wrap(EFlexWrap::NoWrap)
			.JustifyContent(EFlexJustify::FlexStart)
			.AlignItems(EFlexAlign::Stretch)
			+ SFlexBox::Slot()
			.FlexGrow(0.f)
			.FlexShrink(0.f)
			[
				SNew(SBox).WidthOverride(40.f).HeightOverride(50.f)
			];

		FArrangedChildren Arr = SFlexBoxArrangeTestPrivate::ArrangeFlexBox(Flex, 200.f, 0.f);
		ASSERT_THAT(AreEqual(1, Arr.Num()));

		FVector2f Sz = SFlexBoxArrangeTestPrivate::GetSize(Arr, 0);
		ASSERT_THAT(IsTrue(SFlexBoxArrangeTestPrivate::NearlyEqual(Sz.Y, 50.f)));
	}
};
