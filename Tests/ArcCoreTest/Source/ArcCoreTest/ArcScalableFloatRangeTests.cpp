/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "CQTest.h"
#include "ArcScalableFloat.h"
#include "Engine/DataTable.h"
#include "Math/RandomStream.h"
#include "UObject/StrongObjectPtr.h"

// ===================================================================
// 1. InlineRange: results stay within [Min,Max] and are not frozen
// ===================================================================

TEST_CLASS(ArcScalableFloat_InlineRange, "ArcCore.ScalableFloat.InlineRange")
{
	TEST_METHOD(GetValue_ReturnsWithinBounds_ManyTimes)
	{
		FArcScalableFloat Float;
		Float.Source    = EArcScalableFloatSource::InlineRange;
		Float.MinValue  = 10.f;
		Float.MaxValue  = 20.f;

		bool bAnyOutOfBounds = false;
		for (int32 Idx = 0; Idx < 200; ++Idx)
		{
			float Result = Float.GetValue();
			if (Result < 10.f || Result > 20.f)
			{
				bAnyOutOfBounds = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bAnyOutOfBounds, TEXT("Every roll must be in [10,20]")));
	}

	TEST_METHOD(GetValue_ProducesDistinctResults_OverManyRolls)
	{
		// Wide range (0..1000) makes a frozen-value implementation fail this
		// with astronomical certainty.
		FArcScalableFloat Float;
		Float.Source    = EArcScalableFloatSource::InlineRange;
		Float.MinValue  = 0.f;
		Float.MaxValue  = 1000.f;

		float First = Float.GetValue();
		bool bFoundDifferent = false;
		for (int32 Idx = 0; Idx < 200; ++Idx)
		{
			if (Float.GetValue() != First)
			{
				bFoundDifferent = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundDifferent, TEXT("GetValue() must roll fresh each call, not return a frozen value")));
	}
};

// ===================================================================
// 2. Seeded determinism via FArcScalableFloatContext::RandomStream
// ===================================================================

TEST_CLASS(ArcScalableFloat_SeededDeterminism, "ArcCore.ScalableFloat.SeededDeterminism")
{
	TEST_METHOD(GetScaledValue_SameSeed_ProducesIdenticalSequence)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 0.f;
		Float.MaxValue = 100.f;

		const int32 SeedA = 42;
		FRandomStream StreamA(SeedA);
		FRandomStream StreamB(SeedA);

		FArcScalableFloatContext CtxA;
		CtxA.RandomStream = &StreamA;

		FArcScalableFloatContext CtxB;
		CtxB.RandomStream = &StreamB;

		TArray<float> SeqA;
		TArray<float> SeqB;

		for (int32 Idx = 0; Idx < 50; ++Idx)
		{
			SeqA.Add(Float.GetScaledValue(0, CtxA));
			SeqB.Add(Float.GetScaledValue(0, CtxB));
		}

		bool bAllEqual = true;
		for (int32 Idx = 0; Idx < SeqA.Num(); ++Idx)
		{
			if (SeqA[Idx] != SeqB[Idx])
			{
				bAllEqual = false;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bAllEqual, TEXT("Identical seed must produce identical sequence")));
	}

	TEST_METHOD(GetScaledValue_DifferentSeeds_ProduceDifferentSequences)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 0.f;
		Float.MaxValue = 1000.f;

		FRandomStream StreamX(1);
		FRandomStream StreamY(9999);

		FArcScalableFloatContext CtxX;
		CtxX.RandomStream = &StreamX;

		FArcScalableFloatContext CtxY;
		CtxY.RandomStream = &StreamY;

		bool bFoundDifference = false;
		for (int32 Idx = 0; Idx < 50; ++Idx)
		{
			float X = Float.GetScaledValue(0, CtxX);
			float Y = Float.GetScaledValue(0, CtxY);
			if (X != Y)
			{
				bFoundDifference = true;
				break;
			}
		}
		ASSERT_THAT(IsTrue(bFoundDifference, TEXT("Different seeds must produce different sequences")));
	}
};

// ===================================================================
// 3. Zero-width range: Min==Max → always returns that value
// ===================================================================

TEST_CLASS(ArcScalableFloat_ZeroWidthRange, "ArcCore.ScalableFloat.ZeroWidthRange")
{
	TEST_METHOD(GetValue_ZeroWidth_AlwaysReturnsSingleValue)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 5.f;
		Float.MaxValue = 5.f;

		bool bWrongValue = false;
		for (int32 Idx = 0; Idx < 50; ++Idx)
		{
			if (Float.GetValue() != 5.f)
			{
				bWrongValue = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bWrongValue, TEXT("Min==Max==5 must always return 5")));
	}

	TEST_METHOD(IsValid_ZeroWidth_ReturnsTrue)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 5.f;
		Float.MaxValue = 5.f;

		ASSERT_THAT(IsTrue(Float.IsValid(), TEXT("Zero-width InlineRange is valid")));
	}
};

// ===================================================================
// 4. IsValid: inverted range returns false; zero-zero returns true
// ===================================================================

TEST_CLASS(ArcScalableFloat_IsValid, "ArcCore.ScalableFloat.IsValid")
{
	TEST_METHOD(IsValid_InvertedRange_ReturnsFalse)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 20.f;
		Float.MaxValue = 10.f;

		ASSERT_THAT(IsFalse(Float.IsValid(), TEXT("Max < Min must be invalid")));
	}

	TEST_METHOD(IsValid_ZeroZeroRange_ReturnsTrue)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 0.f;
		Float.MaxValue = 0.f;

		ASSERT_THAT(IsTrue(Float.IsValid(), TEXT("Min=0,Max=0 InlineRange is valid")));
	}

	TEST_METHOD(IsValid_DoesNotRoll_InvertedRangeStaysInvalid)
	{
		// Call IsValid many times — must never roll and must always stay false
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 50.f;
		Float.MaxValue = 1.f;

		bool bEverTrue = false;
		for (int32 Idx = 0; Idx < 20; ++Idx)
		{
			if (Float.IsValid())
			{
				bEverTrue = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bEverTrue, TEXT("IsValid must never return true for inverted range")));
	}
};

// ===================================================================
// 5. DataTableRange with in-memory UDataTable fixture
// ===================================================================

TEST_CLASS(ArcScalableFloat_DataTableRange, "ArcCore.ScalableFloat.DataTableRange")
{
	TEST_METHOD(GetValue_DataTableRange_ReturnsWithinTableRowBounds)
	{
		// Build an in-memory DataTable using FArcScalableFloatRange as the row struct.
		TStrongObjectPtr<UDataTable> TablePtr(
			NewObject<UDataTable>(GetTransientPackage(), TEXT("TestRangeTable_ArcScalable"), RF_Transient));
		UDataTable* Table = TablePtr.Get();
		Table->RowStruct = FArcScalableFloatRange::StaticStruct();

		FArcScalableFloatRange Row;
		Row.Min = 3.f;
		Row.Max = 7.f;
		Table->AddRow(FName("R"), Row);

		FArcScalableFloat Float;
		Float.Source              = EArcScalableFloatSource::DataTableRange;
		Float.DataTable.DataTable = Table;
		Float.DataTable.RowName   = FName("R");

		bool bAnyOutOfBounds = false;
		for (int32 Idx = 0; Idx < 200; ++Idx)
		{
			float Result = Float.GetValue();
			if (Result < 3.f || Result > 7.f)
			{
				bAnyOutOfBounds = true;
				break;
			}
		}
		ASSERT_THAT(IsFalse(bAnyOutOfBounds, TEXT("DataTableRange rolls must stay within [3,7]")));
	}

	TEST_METHOD(IsValid_DataTableRange_ValidTableAndRow_ReturnsTrue)
	{
		TStrongObjectPtr<UDataTable> TablePtr(
			NewObject<UDataTable>(GetTransientPackage(), TEXT("TestRangeTable_IsValid"), RF_Transient));
		UDataTable* Table = TablePtr.Get();
		Table->RowStruct = FArcScalableFloatRange::StaticStruct();

		FArcScalableFloatRange Row;
		Row.Min = 1.f;
		Row.Max = 10.f;
		Table->AddRow(FName("V"), Row);

		FArcScalableFloat Float;
		Float.Source              = EArcScalableFloatSource::DataTableRange;
		Float.DataTable.DataTable = Table;
		Float.DataTable.RowName   = FName("V");

		ASSERT_THAT(IsTrue(Float.IsValid(), TEXT("DataTableRange with valid table+row must be valid")));
	}
};

// ===================================================================
// 6. IsRangeSource / GetValueRange / GetRangeMin/Max/Mid
// ===================================================================

TEST_CLASS(ArcScalableFloat_RangeAPI, "ArcCore.ScalableFloat.RangeAPI")
{
	TEST_METHOD(IsRangeSource_InlineRange_ReturnsTrue)
	{
		FArcScalableFloat Float;
		Float.Source = EArcScalableFloatSource::InlineRange;
		ASSERT_THAT(IsTrue(Float.IsRangeSource(), TEXT("InlineRange must be a range source")));
	}

	TEST_METHOD(IsRangeSource_DataTableRange_ReturnsTrue)
	{
		FArcScalableFloat Float;
		Float.Source = EArcScalableFloatSource::DataTableRange;
		ASSERT_THAT(IsTrue(Float.IsRangeSource(), TEXT("DataTableRange must be a range source")));
	}

	TEST_METHOD(IsRangeSource_DataRegistryRange_ReturnsTrue)
	{
		FArcScalableFloat Float;
		Float.Source = EArcScalableFloatSource::DataRegistryRange;
		ASSERT_THAT(IsTrue(Float.IsRangeSource(), TEXT("DataRegistryRange must be a range source")));
	}

	TEST_METHOD(IsRangeSource_None_ReturnsFalse)
	{
		FArcScalableFloat Float;
		Float.Source = EArcScalableFloatSource::None;
		ASSERT_THAT(IsFalse(Float.IsRangeSource(), TEXT("None must not be a range source")));
	}

	TEST_METHOD(IsRangeSource_CurveTableRow_ReturnsFalse)
	{
		FArcScalableFloat Float;
		Float.Source = EArcScalableFloatSource::CurveTableRow;
		ASSERT_THAT(IsFalse(Float.IsRangeSource(), TEXT("CurveTableRow must not be a range source")));
	}

	TEST_METHOD(GetValueRange_InlineRange_ReturnsTrueAndCorrectBounds)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 10.f;
		Float.MaxValue = 20.f;

		float OutMin = 0.f;
		float OutMax = 0.f;
		bool bResult = Float.GetValueRange(OutMin, OutMax);

		ASSERT_THAT(IsTrue(bResult, TEXT("GetValueRange on InlineRange must return true")));
		ASSERT_THAT(IsTrue(OutMin == 10.f, TEXT("OutMin must be 10")));
		ASSERT_THAT(IsTrue(OutMax == 20.f, TEXT("OutMax must be 20")));
	}

	TEST_METHOD(GetValueRange_NoneSource_ReturnsFalseAndStaticValue)
	{
		FArcScalableFloat Float;
		Float.Source = EArcScalableFloatSource::None;
		Float.Value  = 7.f;

		float OutMin = 0.f;
		float OutMax = 0.f;
		bool bResult = Float.GetValueRange(OutMin, OutMax);

		ASSERT_THAT(IsFalse(bResult, TEXT("GetValueRange on None source must return false")));
		ASSERT_THAT(IsTrue(OutMin == 7.f, TEXT("OutMin must equal static value 7")));
		ASSERT_THAT(IsTrue(OutMax == 7.f, TEXT("OutMax must equal static value 7")));
	}

	TEST_METHOD(GetRangeMin_InlineRange_ReturnsMin)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 10.f;
		Float.MaxValue = 20.f;

		ASSERT_THAT(IsTrue(Float.GetRangeMin() == 10.f, TEXT("GetRangeMin must return 10")));
	}

	TEST_METHOD(GetRangeMax_InlineRange_ReturnsMax)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 10.f;
		Float.MaxValue = 20.f;

		ASSERT_THAT(IsTrue(Float.GetRangeMax() == 20.f, TEXT("GetRangeMax must return 20")));
	}

	TEST_METHOD(GetRangeMid_InlineRange_ReturnsMidpoint)
	{
		FArcScalableFloat Float;
		Float.Source   = EArcScalableFloatSource::InlineRange;
		Float.MinValue = 10.f;
		Float.MaxValue = 20.f;

		ASSERT_THAT(IsTrue(Float.GetRangeMid() == 15.f, TEXT("GetRangeMid must return 15")));
	}
};

// ===================================================================
// 7. operator==: field-level equality comparisons
// ===================================================================

TEST_CLASS(ArcScalableFloat_Equality, "ArcCore.ScalableFloat.Equality")
{
	TEST_METHOD(Equals_IdenticalInlineRange_ReturnsTrue)
	{
		FArcScalableFloat A;
		A.Source   = EArcScalableFloatSource::InlineRange;
		A.MinValue = 1.f;
		A.MaxValue = 9.f;

		FArcScalableFloat B;
		B.Source   = EArcScalableFloatSource::InlineRange;
		B.MinValue = 1.f;
		B.MaxValue = 9.f;

		ASSERT_THAT(IsTrue(A == B, TEXT("Identical InlineRange floats must compare equal")));
	}

	TEST_METHOD(Equals_DifferentMaxValue_ReturnsFalse)
	{
		FArcScalableFloat A;
		A.Source   = EArcScalableFloatSource::InlineRange;
		A.MinValue = 1.f;
		A.MaxValue = 9.f;

		FArcScalableFloat B;
		B.Source   = EArcScalableFloatSource::InlineRange;
		B.MinValue = 1.f;
		B.MaxValue = 99.f;

		ASSERT_THAT(IsFalse(A == B, TEXT("Different MaxValue must compare unequal")));
	}

	TEST_METHOD(Equals_RangeVsNone_ReturnsFalse)
	{
		FArcScalableFloat Range;
		Range.Source   = EArcScalableFloatSource::InlineRange;
		Range.MinValue = 1.f;
		Range.MaxValue = 9.f;

		FArcScalableFloat None;
		None.Source = EArcScalableFloatSource::None;
		None.Value  = 5.f;

		ASSERT_THAT(IsFalse(Range == None, TEXT("InlineRange must not equal a None float")));
	}

	TEST_METHOD(NotEquals_DifferentMinValue_ReturnsTrue)
	{
		FArcScalableFloat A;
		A.Source   = EArcScalableFloatSource::InlineRange;
		A.MinValue = 0.f;
		A.MaxValue = 5.f;

		FArcScalableFloat B;
		B.Source   = EArcScalableFloatSource::InlineRange;
		B.MinValue = 2.f;
		B.MaxValue = 5.f;

		ASSERT_THAT(IsTrue(A != B, TEXT("Different MinValue must make floats unequal")));
	}
};
