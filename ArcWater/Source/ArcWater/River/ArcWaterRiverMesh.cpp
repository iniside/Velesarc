// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverMesh.h"

#include "River/ArcWaterRiverChannel.h"
#include "River/ArcWaterRiverComponent.h"

namespace ArcWaterRiverMeshLocal_4e07
{
	const double MinHorizontalTangentSin = FMath::Sin(FMath::DegreesToRadians(3.0));

	struct FStationPlan
	{
		int32 NumStations = 0;
		float Step = 0.0f;
		float Length = 0.0f;
	};

	bool MakeStationPlan(const UArcWaterRiverComponent& River, FStationPlan& OutPlan)
	{
		const float Length = River.GetSplineLength();
		if (!(Length > UE_KINDA_SMALL_NUMBER))
		{
			return false;
		}

		const double MaxStep = static_cast<double>(FMath::Max(River.MaxStepCm, 1.0f));
		const double RawIntervals = FMath::CeilToDouble(static_cast<double>(Length) / MaxStep);
		if (!FMath::IsFinite(RawIntervals) || RawIntervals > static_cast<double>(MAX_int32 - 1))
		{
			return false;
		}
		const int32 Intervals = FMath::Max(static_cast<int32>(RawIntervals), 1);
		const bool bClosed = River.IsClosedLoop();

		OutPlan.Length = Length;
		OutPlan.NumStations = bClosed ? FMath::Max(Intervals, 3) : Intervals + 1;
		OutPlan.Step = Length / static_cast<float>(
			bClosed ? OutPlan.NumStations : OutPlan.NumStations - 1);
		return true;
	}

	struct FStation
	{
		FVector Center = FVector::ZeroVector;
		FVector Tangent = FVector::ForwardVector;
		FVector Right = FVector::RightVector;
		float Distance = 0.0f;
		float Key = 0.0f;
		float HalfWidth = 0.0f;
	};

	template <typename FStationVisitor>
	void WalkStations(const UArcWaterRiverComponent& River, const FStationPlan& Plan,
		ESplineCoordinateSpace::Type Space, FStationVisitor&& Visitor)
	{
		FStation Station;
		FVector PreviousTangent = FVector::ZeroVector;
		bool bPreviousResolved = false;

		for (int32 StationIndex = 0; StationIndex < Plan.NumStations; ++StationIndex)
		{
			Station.Distance = FMath::Min(Plan.Step * static_cast<float>(StationIndex), Plan.Length);

			FVector Center = FVector::ZeroVector;
			FVector Tangent = FVector::ForwardVector;
			FVector Right = FVector::RightVector;
			const bool bResolved = ArcWaterRiverMesh::ResolveFrameAt(
				River, Station.Distance, Space, Center, Tangent, Right);

			Station.Center = Center;
			if (bResolved)
			{
				Station.Tangent = Tangent;
				Station.Right = Right;
			}

			if (bResolved && bPreviousResolved)
			{
				const double DirectionChange = (Station.Tangent - PreviousTangent).Size();
				if (DirectionChange > UE_DOUBLE_SMALL_NUMBER)
				{
					const float Radius = Plan.Step / static_cast<float>(DirectionChange);
					River.CachedMinCurvatureRadiusCm = FMath::Min(River.CachedMinCurvatureRadiusCm, Radius);
				}
			}
			PreviousTangent = Station.Tangent;
			bPreviousResolved = bResolved;

			Station.Key = River.GetInputKeyValueAtDistanceAlongSpline(Station.Distance);
			Station.HalfWidth = FMath::Max(River.GetHalfWidthAtKey(Station.Key), 0.0f);

			Visitor(Station);
		}
	}

	bool BuildManualGeneratedData(const UArcWaterRiverComponent& River,
		FArcWaterRiverGeneratedData& OutData)
	{
		FStationPlan Plan;
		if (!MakeStationPlan(River, Plan))
		{
			return false;
		}

		FArcWaterRiverGeneratedData Data;
		FArcWaterRiverGeneratedReach& Reach = Data.Reaches.AddDefaulted_GetRef();
		Reach.LengthCm = Plan.Length;
		Reach.TileCm = FMath::Max(River.TileCm, 1.0f);
		Reach.bClosed = River.IsClosedLoop();
		Reach.SplinePoints.Reserve(Plan.NumStations);
		Reach.Sections.Reserve(Plan.NumStations);

		WalkStations(River, Plan, ESplineCoordinateSpace::Local,
			[&Reach, &River](const FStation& Station)
			{
				FArcWaterRiverChannelSection& Section = Reach.Sections.AddDefaulted_GetRef();
				Section.Position = Station.Center;
				Section.Lateral = FVector2D(Station.Right.X, Station.Right.Y);
				if (!Section.Lateral.Normalize())
				{
					Section.Lateral = FVector2D(0.0, 1.0);
				}
				Section.DistanceAlongCm = Station.Distance;
				Section.HalfWidthCm = Station.HalfWidth;
				Section.FlowSpeedCmS = River.GetFlowSpeedAtKey(Station.Key);
				Section.ReachIndex = 0;
				Section.SectionIndex = Reach.Sections.Num() - 1;
				Section.WidthSegments = FMath::Max(River.WidthSegments, 1);
				Reach.SplinePoints.Add(Station.Center);
			});

		if (Reach.Sections.Num() < 2)
		{
			return false;
		}
		if (Reach.bClosed)
		{
			Reach.Sections.Last().bClosesReach = true;
		}

		OutData = MoveTemp(Data);
		return true;
	}

	void UpdateGeneratedCurvature(const UArcWaterRiverComponent& River,
		const FArcWaterRiverGeneratedReach& Reach)
	{
		River.CachedMinCurvatureRadiusCm = TNumericLimits<float>::Max();
		const int32 SegmentCount = Reach.bClosed ? Reach.Sections.Num() : Reach.Sections.Num() - 1;
		for (int32 SectionIndex = 0; SectionIndex < SegmentCount; ++SectionIndex)
		{
			const int32 NextIndex = SectionIndex + 1 < Reach.Sections.Num() ? SectionIndex + 1 : 0;
			const FArcWaterRiverChannelSection& Section = Reach.Sections[SectionIndex];
			const FArcWaterRiverChannelSection& NextSection = Reach.Sections[NextIndex];
			const FVector2D Tangent(Section.Lateral.Y, -Section.Lateral.X);
			const FVector2D NextTangent(NextSection.Lateral.Y, -NextSection.Lateral.X);
			const double DirectionChange = (NextTangent - Tangent).Size();
			if (DirectionChange <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			const float Distance = NextIndex == 0
				? Reach.LengthCm - Section.DistanceAlongCm
				: NextSection.DistanceAlongCm - Section.DistanceAlongCm;
			if (Distance > 0.0f)
			{
				River.CachedMinCurvatureRadiusCm = FMath::Min(
					River.CachedMinCurvatureRadiusCm,
					Distance / static_cast<float>(DirectionChange));
			}
		}
	}
}

namespace ArcWaterRiverMesh
{
	bool ResolveFrameAt(const UArcWaterRiverComponent& River, float DistanceAlong,
		ESplineCoordinateSpace::Type Space, FVector& OutCenter, FVector& OutTangent, FVector& OutRight)
	{
		OutCenter = River.GetLocationAtDistanceAlongSpline(DistanceAlong, Space);

		FVector UnitTangent = River.GetTangentAtDistanceAlongSpline(DistanceAlong, Space);
		const bool bHasTangent = UnitTangent.Normalize();

		const FVector Right = bHasTangent
			? FVector::CrossProduct(FVector::UpVector, UnitTangent)
			: FVector::ZeroVector;

		if (Right.Size() < ArcWaterRiverMeshLocal_4e07::MinHorizontalTangentSin)
		{
			FVector Horizontal(UnitTangent.X, UnitTangent.Y, 0.0);
			if (!bHasTangent || !Horizontal.Normalize())
			{
				Horizontal = FVector::ForwardVector;
			}

			OutTangent = Horizontal;
			OutRight = FVector::CrossProduct(FVector::UpVector, Horizontal);
			return false;
		}

		OutTangent = UnitTangent;
		OutRight = Right.GetUnsafeNormal();
		return true;
	}

	void BuildRibbon(const UArcWaterRiverComponent& River,
		TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices)
	{
		OutVertices.Reset();
		OutIndices.Reset();
		const FArcWaterRiverGeneratedData* Data = River.GetGeneratedChannelData();
		int32 ReachIndex = River.GetGeneratedReachIndex();
		FArcWaterRiverGeneratedData ManualData;
		if (Data == nullptr)
		{
			if (!ArcWaterRiverMeshLocal_4e07::BuildManualGeneratedData(River, ManualData))
			{
				return;
			}
			Data = &ManualData;
			ReachIndex = 0;
		}
		if (!Data->Reaches.IsValidIndex(ReachIndex))
		{
			return;
		}
		ArcWaterRiverMeshLocal_4e07::UpdateGeneratedCurvature(
			River, Data->Reaches[ReachIndex]);
		ArcWaterRiverChannel::BuildWaterMesh(*Data, ReachIndex, OutVertices, OutIndices);
	}

	FBox ComputeLocalBounds(const UArcWaterRiverComponent& River)
	{
		FBox LocalBounds(ForceInit);
		TArray<FDynamicMeshVertex> Vertices;
		TArray<uint32> Indices;
		BuildRibbon(River, Vertices, Indices);
		for (const FDynamicMeshVertex& Vertex : Vertices)
		{
			LocalBounds += FVector(Vertex.Position);
		}

		return LocalBounds;
	}
}
