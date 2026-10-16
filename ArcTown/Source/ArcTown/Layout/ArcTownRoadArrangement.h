// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/PimplPtr.h"

struct FArcTownWorldSnapshot;

namespace ArcTownRoadArrangement
{
	struct FGraphStorage;

	struct ARCTOWN_API FSourceEdge
	{
		FVector2d Start = FVector2d::ZeroVector;
		FVector2d End = FVector2d::ZeroVector;
		float HalfWidth = 0.0f;
		uint8 Hierarchy = 0;
		int32 SnapshotEdgeIndex = INDEX_NONE;
	};

	struct ARCTOWN_API FSegmentWidth
	{
		float HalfWidth = 0.0f;
		uint8 Hierarchy = 0;
	};

	struct ARCTOWN_API FArrangement
	{
		TArray<FSourceEdge> SourceEdges;

		double SnapToleranceCm = 0.0;

		TPimplPtr<FGraphStorage> Storage;

		bool IsValid() const
		{
			return Storage.IsValid();
		}
	};

	ARCTOWN_API void GatherSourceEdges(
		const FArcTownWorldSnapshot& Snapshot,
		const double MinLengthCm,
		TArray<FSourceEdge>& OutEdges);

	ARCTOWN_API FSegmentWidth FindWidthNear(
		TConstArrayView<FSourceEdge> SourceEdges,
		const FVector2d& Point,
		const double SnapTolerance);

	ARCTOWN_API int32 FindNearestSourceEdge(
		TConstArrayView<FSourceEdge> SourceEdges,
		const FVector2d& Point,
		const double SnapTolerance);

	ARCTOWN_API bool Build(
		const FArcTownWorldSnapshot& Snapshot,
		const double SnapToleranceCm,
		FArrangement& OutArrangement);
}
