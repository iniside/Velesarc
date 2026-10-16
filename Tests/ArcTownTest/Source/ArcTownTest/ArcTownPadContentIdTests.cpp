// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"

#include "Terrain/ArcTownStampKeys.h"

namespace ArcTownPadContentIdTestHelpers
{
	FGuid BaselineTownId()
	{
		return FGuid(0x11111111, 0x22222222, 0x33333333, 0x44444444);
	}
}

// MakePadContentId feeds the terrain collision/Nanite cache key directly (ArcTerrainCollisionBuilder.cpp:425-426);
// a field the fold forgets is a field a rebuild can never invalidate, and a non-deterministic fold invalidates
// every cell on every rebuild. Each test below fails if ArcTownStampKeys.cpp regresses to FGuid::NewGuid() or
// drops one of the four arguments from the hash.
TEST_CLASS(ArcTownPadContentId, "ArcTown.StampKeys.PadContentId")
{
	TEST_METHOD(MakePadContentId_SameArguments_ProducesSameGuidTwice)
	{
		const FGuid TownId = ArcTownPadContentIdTestHelpers::BaselineTownId();
		const FGuid First = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 12.5);
		const FGuid Second = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 12.5);

		ASSERT_THAT(AreEqual(First, Second,
			TEXT("Two independent folds of identical arguments must land on the same guid, "
				"or the DDC key it feeds is unstable across sessions")));
	}

	TEST_METHOD(MakePadContentId_ChangingTownId_ChangesGuid)
	{
		const FGuid Baseline = ArcTownStampKeys::MakePadContentId(
			ArcTownPadContentIdTestHelpers::BaselineTownId(), 3, FVector2D(400.0, 400.0), 12.5);
		const FGuid Changed = ArcTownStampKeys::MakePadContentId(
			FGuid(0x55555555, 0x66666666, 0x77777777, 0x88888888), 3, FVector2D(400.0, 400.0), 12.5);

		ASSERT_THAT(AreNotEqual(Baseline, Changed,
			TEXT("TownId dropped from the fold: a stamp from one town would alias another's cached raster")));
	}

	TEST_METHOD(MakePadContentId_ChangingBuildingIndex_ChangesGuid)
	{
		const FGuid TownId = ArcTownPadContentIdTestHelpers::BaselineTownId();
		const FGuid Baseline = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 12.5);
		const FGuid Changed = ArcTownStampKeys::MakePadContentId(TownId, 4, FVector2D(400.0, 400.0), 12.5);

		ASSERT_THAT(AreNotEqual(Baseline, Changed,
			TEXT("BuildingIndex dropped from the fold: two buildings' pads would share one cache entry")));
	}

	TEST_METHOD(MakePadContentId_ChangingFootprintX_ChangesGuid)
	{
		const FGuid TownId = ArcTownPadContentIdTestHelpers::BaselineTownId();
		const FGuid Baseline = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 12.5);
		const FGuid Changed = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(500.0, 400.0), 12.5);

		ASSERT_THAT(AreNotEqual(Baseline, Changed,
			TEXT("FootprintXY.X dropped from the fold: a resized pad would keep serving the stale raster")));
	}

	TEST_METHOD(MakePadContentId_ChangingFootprintY_ChangesGuid)
	{
		const FGuid TownId = ArcTownPadContentIdTestHelpers::BaselineTownId();
		const FGuid Baseline = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 12.5);
		const FGuid Changed = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 500.0), 12.5);

		ASSERT_THAT(AreNotEqual(Baseline, Changed,
			TEXT("FootprintXY.Y dropped from the fold: a resized pad would keep serving the stale raster")));
	}

	TEST_METHOD(MakePadContentId_ChangingFloorHeight_ChangesGuid)
	{
		const FGuid TownId = ArcTownPadContentIdTestHelpers::BaselineTownId();
		const FGuid Baseline = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 12.5);
		const FGuid Changed = ArcTownStampKeys::MakePadContentId(TownId, 3, FVector2D(400.0, 400.0), 99.0);

		ASSERT_THAT(AreNotEqual(Baseline, Changed,
			TEXT("FloorHeight dropped from the fold: re-levelling a pad would keep serving the stale flat")));
	}
};
