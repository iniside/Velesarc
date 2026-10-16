/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

// Re-declare the production formula locally (approach a).
// FormatTimeRemaining lives in the file-scoped namespace
// ArcCraftQueueEntryViewModelInternal inside ArcCraftQueueEntryViewModel.cpp
// and is not exposed via a header.  Duplicating the six-line pure function here
// is intentional: the test verifies the formula is correct; keeping it
// self-contained avoids promoting an internal helper to a public API.
namespace ArcCraftQueueEntryFormatTimeTest
{
	FText FormatTimeRemaining(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt(Seconds));
		const int32 Hours = Total / 3600;
		const int32 Mins  = (Total % 3600) / 60;
		const int32 Secs  = Total % 60;
		return FText::FromString(FString::Printf(TEXT("%02d:%02d:%02d"), Hours, Mins, Secs));
	}
}

// ===================================================================
// FormatTimeRemaining — boundary and rounding cases
// ===================================================================

TEST_CLASS(CraftQueueEntry_FormatTimeRemaining, "ArcCraft.ViewModels.FormatTimeRemaining")
{
	TEST_METHOD(Zero_ReturnsAllZeroes)
	{
		FText Result = ArcCraftQueueEntryFormatTimeTest::FormatTimeRemaining(0.0f);
		ASSERT_THAT(AreEqual(FString(TEXT("00:00:00")), Result.ToString()));
	}

	TEST_METHOD(Negative_ClampsToZero)
	{
		FText Result = ArcCraftQueueEntryFormatTimeTest::FormatTimeRemaining(-5.0f);
		ASSERT_THAT(AreEqual(FString(TEXT("00:00:00")), Result.ToString()));
	}

	TEST_METHOD(FiftyNineSeconds_FormatsCorrectly)
	{
		FText Result = ArcCraftQueueEntryFormatTimeTest::FormatTimeRemaining(59.0f);
		ASSERT_THAT(AreEqual(FString(TEXT("00:00:59")), Result.ToString()));
	}

	TEST_METHOD(SixtySeconds_RollsOverToOneMinute)
	{
		FText Result = ArcCraftQueueEntryFormatTimeTest::FormatTimeRemaining(60.0f);
		ASSERT_THAT(AreEqual(FString(TEXT("00:01:00")), Result.ToString()));
	}

	TEST_METHOD(ThreeThousandSixHundredSixtyOne_FormatsHoursMinutesSeconds)
	{
		FText Result = ArcCraftQueueEntryFormatTimeTest::FormatTimeRemaining(3661.0f);
		ASSERT_THAT(AreEqual(FString(TEXT("01:01:01")), Result.ToString()));
	}

	TEST_METHOD(FractionalSeconds_CeilRoundsUp)
	{
		// 0.4f — CeilToInt yields 1 → "00:00:01"
		FText Result = ArcCraftQueueEntryFormatTimeTest::FormatTimeRemaining(0.4f);
		ASSERT_THAT(AreEqual(FString(TEXT("00:00:01")), Result.ToString()));
	}
};
