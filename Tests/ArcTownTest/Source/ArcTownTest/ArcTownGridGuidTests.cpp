// Copyright Lukasz Baran. All Rights Reserved.

#include "CQTest.h"

#if WITH_EDITOR

#include "Build/ArcTownGridKeys.h"

namespace ArcTownGridGuidTestHelpers
{
	FGuid BaselineTownId()
	{
		return FGuid(0x11111111, 0x22222222, 0x33333333, 0x44444444);
	}
}

// UActorPartitionSubsystem caches on (cell, class + grid guid) with no grid size in the key
// (ActorPartitionSubsystem.h:203), and FName compares case-insensitively, so both are required inputs to the
// fold, not just plausible ones: a missing GridSize fold aliases two differently-sized grids onto one actor
// family, and a missing ToLower splits one family into two by display casing alone.
TEST_CLASS(ArcTownGridGuid, "ArcTown.GridKeys.MakeTownGridGuid")
{
	TEST_METHOD(MakeTownGridGuid_SameArguments_ProducesSameGuidTwice)
	{
		const FGuid TownId = ArcTownGridGuidTestHelpers::BaselineTownId();
		const FGuid First = ArcTownGridKeys::MakeTownGridGuid(TownId, 25600, TEXT("MainGrid"));
		const FGuid Second = ArcTownGridKeys::MakeTownGridGuid(TownId, 25600, TEXT("MainGrid"));

		ASSERT_THAT(AreEqual(First, Second,
			TEXT("Two independent folds of identical arguments must land on the same guid")));
	}

	TEST_METHOD(MakeTownGridGuid_ChangingTownId_ChangesGuid)
	{
		const FGuid Baseline = ArcTownGridKeys::MakeTownGridGuid(
			ArcTownGridGuidTestHelpers::BaselineTownId(), 25600, TEXT("MainGrid"));
		const FGuid Changed = ArcTownGridKeys::MakeTownGridGuid(
			FGuid(0x55555555, 0x66666666, 0x77777777, 0x88888888), 25600, TEXT("MainGrid"));

		ASSERT_THAT(AreNotEqual(Baseline, Changed,
			TEXT("TownId dropped from the fold: two towns' partition-actor families would alias")));
	}

	TEST_METHOD(MakeTownGridGuid_SameTownDifferentGridSize_ProducesDifferentGuid)
	{
		const FGuid TownId = ArcTownGridGuidTestHelpers::BaselineTownId();
		const FGuid SmallGrid = ArcTownGridKeys::MakeTownGridGuid(TownId, 25600, TEXT("MainGrid"));
		const FGuid LargeGrid = ArcTownGridKeys::MakeTownGridGuid(TownId, 51200, TEXT("MainGrid"));

		ASSERT_THAT(AreNotEqual(SmallGrid, LargeGrid,
			TEXT("GridSize dropped from the fold: the subsystem's cache key has no grid size of its own, "
				"so two differently-sized grids for the same town would alias onto one actor family")));
	}

	TEST_METHOD(MakeTownGridGuid_SameTownRuntimeGridNameDifferingOnlyByCase_ProducesSameGuid)
	{
		const FGuid TownId = ArcTownGridGuidTestHelpers::BaselineTownId();
		const FGuid Upper = ArcTownGridKeys::MakeTownGridGuid(TownId, 25600, TEXT("MainGrid"));
		const FGuid Lower = ArcTownGridKeys::MakeTownGridGuid(TownId, 25600, TEXT("maingrid"));

		ASSERT_THAT(AreEqual(Upper, Lower,
			TEXT("FName compares case-insensitively; a display-casing-sensitive fold would split one grid into two families")));
	}
};

#endif
