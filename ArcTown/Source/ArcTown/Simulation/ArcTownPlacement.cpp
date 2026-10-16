// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcTownPlacement.h"

#include "Morphology/ArcTownBlocks.h"
#include "Simulation/ArcTownBootstrap.h"
#include "Simulation/ArcTownDeterministic.h"
#include "Simulation/ArcTownFieldSettings.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownSimulationInput.h"
#include "Simulation/ArcTownSiteFields.h"
#include "Simulation/ArcTownTensorField.h"
#include "Simulation/ArcTownTraffic.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownSiteSnapshot.h"

namespace ArcTownPlacementPrivate
{
namespace
{
	constexpr double ScoreQuanta = 1024.0;

	constexpr double AngleQuanta = 1024.0;

	constexpr double MaxQuantizedMagnitude = 9.0e15;

	constexpr double MaxRunTurnDeg = 15.0;

	constexpr double PlotTouchToleranceCm = 0.5;

	constexpr int32 MaxDiagnosticSamplesPerRun = 20;

	int32 GFrontageOverlapSamplesLogged = 0;

	int32 GFrontageOverlapSurvivesWithoutOverhangCount = 0;

	int32 GFrontageOverlapRemovedByOverhangCount = 0;

	TArray<double> GFrontageOverlapPenetrationCm;

	int32 GPlotGateSamplesLogged = 0;

	int32 GPlacedGateSamplesLogged = 0;

	int32 GBuildingLargerSamplesLogged = 0;

	TArray<double> GBuildingLargerAlongShortfallCm;

	TArray<double> GBuildingLargerAcrossShortfallCm;

	int32 GPlotOverlapSamplesLogged = 0;

	int32 GPlotOverlapSameRunCount = 0;

	TArray<double> GPlotOverlapPenetrationCm;

	bool SetError(FText& OutError, const FString& Message)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	FVector2D ToDouble(const FVector2f& Value)
	{
		return FVector2D(static_cast<double>(Value.X), static_cast<double>(Value.Y));
	}

	FVector2f ToFloat(const FVector2D& Value)
	{
		return FVector2f(static_cast<float>(Value.X), static_cast<float>(Value.Y));
	}

	int64 Quantize(const double Value, const double QuantaPerUnit)
	{
		const double Scaled = FMath::IsFinite(Value) ? Value * QuantaPerUnit : 0.0;
		return static_cast<int64>(
			FMath::RoundToDouble(FMath::Clamp(Scaled, -MaxQuantizedMagnitude, MaxQuantizedMagnitude)));
	}

	struct FFootprintRect
	{
		FVector2D Center = FVector2D::ZeroVector;
		FVector2D AxisX = FVector2D(1.0, 0.0);
		FVector2D AxisY = FVector2D(0.0, 1.0);
		FVector2D HalfExtent = FVector2D::ZeroVector;
	};

	FVector2D DoorForward(const FArcTownFrozenBuildingEntry& Entry)
	{
		const FVector2D Normalized = Entry.MeshLocalDoorForwardXY.GetSafeNormal();
		return Normalized.IsNearlyZero() ? FVector2D(0.0, -1.0) : Normalized;
	}

	double MeshYaw(const FArcTownFrozenBuildingEntry& Entry, const double DoorAngleRad)
	{
		const FVector2D Forward = DoorForward(Entry);
		return FMath::UnwindRadians(DoorAngleRad - FMath::Atan2(Forward.Y, Forward.X));
	}

	FFootprintRect MakeBareFootprintRect(
		const FArcTownFrozenBuildingEntry& Entry,
		const FVector2D& CenterXY,
		const double Yaw)
	{
		FFootprintRect Rect;
		Rect.AxisX = FVector2D(FMath::Cos(Yaw), FMath::Sin(Yaw));
		Rect.AxisY = FVector2D(-Rect.AxisX.Y, Rect.AxisX.X);
		Rect.HalfExtent = Entry.FootprintXY * 0.5;
		Rect.Center = CenterXY;
		return Rect;
	}

	bool OverlapsOnAxis(
		const FFootprintRect& A,
		const FFootprintRect& B,
		const FVector2D& Axis,
		const double ClearanceCm)
	{
		const double Distance = FMath::Abs(FVector2D::DotProduct(B.Center - A.Center, Axis));
		const double RadiusA = FMath::Abs(FVector2D::DotProduct(A.AxisX * A.HalfExtent.X, Axis))
			+ FMath::Abs(FVector2D::DotProduct(A.AxisY * A.HalfExtent.Y, Axis));
		const double RadiusB = FMath::Abs(FVector2D::DotProduct(B.AxisX * B.HalfExtent.X, Axis))
			+ FMath::Abs(FVector2D::DotProduct(B.AxisY * B.HalfExtent.Y, Axis));
		return Distance < RadiusA + RadiusB - ClearanceCm;
	}

	bool Overlaps(const FFootprintRect& A, const FFootprintRect& B, const double ClearanceCm)
	{
		return OverlapsOnAxis(A, B, A.AxisX, ClearanceCm)
			&& OverlapsOnAxis(A, B, A.AxisY, ClearanceCm)
			&& OverlapsOnAxis(A, B, B.AxisX, ClearanceCm)
			&& OverlapsOnAxis(A, B, B.AxisY, ClearanceCm);
	}

	double RectReach(const FArcTownFrozenBuildingEntry& Entry)
	{
		return 0.5 * Entry.FootprintXY.Size();
	}

	FVector2D RectExtentAA(const FFootprintRect& Rect)
	{
		return FVector2D(
			FMath::Abs(Rect.AxisX.X) * Rect.HalfExtent.X + FMath::Abs(Rect.AxisY.X) * Rect.HalfExtent.Y,
			FMath::Abs(Rect.AxisX.Y) * Rect.HalfExtent.X + FMath::Abs(Rect.AxisY.Y) * Rect.HalfExtent.Y);
	}

	double SupportAlong(const FFootprintRect& Rect, const FVector2D& DirectionXY)
	{
		return FMath::Abs(FVector2D::DotProduct(Rect.AxisX, DirectionXY)) * Rect.HalfExtent.X
			+ FMath::Abs(FVector2D::DotProduct(Rect.AxisY, DirectionXY)) * Rect.HalfExtent.Y;
	}

	enum class ESiteGate : uint8
	{
		Accepted,
		OutsideVolumeBounds,
		OutsideSite,
		NotBuildable,
		Count
	};

	int32 GPlotGateCounts[static_cast<int32>(ESiteGate::Count)] = {};

	int32 GPlacedGateCounts[static_cast<int32>(ESiteGate::Count)] = {};

	const TCHAR* SiteGateName(const ESiteGate Gate)
	{
		switch (Gate)
		{
		case ESiteGate::Accepted:
			return TEXT("Accepted");
		case ESiteGate::OutsideVolumeBounds:
			return TEXT("OutsideVolumeBounds");
		case ESiteGate::OutsideSite:
			return TEXT("OutsideSite");
		case ESiteGate::NotBuildable:
			return TEXT("NotBuildable");
		default:
			return TEXT("Unknown");
		}
	}

	ESiteGate TestSiteGates(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const FBox2D& VolumeBoundsTownLocalXY,
		const FVector2D& PointXY,
		int32& OutCellIndex)
	{
		OutCellIndex = INDEX_NONE;

		if (PointXY.X < VolumeBoundsTownLocalXY.Min.X
			|| PointXY.X > VolumeBoundsTownLocalXY.Max.X
			|| PointXY.Y < VolumeBoundsTownLocalXY.Min.Y
			|| PointXY.Y > VolumeBoundsTownLocalXY.Max.Y)
		{
			return ESiteGate::OutsideVolumeBounds;
		}

		FIntPoint Cell = FIntPoint::ZeroValue;
		if (!ArcTownSiteFrame::TownLocalXYToCell(Site, ToFloat(PointXY), Cell))
		{
			return ESiteGate::OutsideSite;
		}

		const int32 CellIndex = Site.SnapshotIndex(Cell.X, Cell.Y);
		if (!Fields.bBuildable.IsValidIndex(CellIndex) || Fields.bBuildable[CellIndex] == 0)
		{
			return ESiteGate::NotBuildable;
		}

		OutCellIndex = CellIndex;
		return ESiteGate::Accepted;
	}

	EArcTownPlacementRejection PlotRejection(const ESiteGate Gate)
	{
		switch (Gate)
		{
		case ESiteGate::OutsideVolumeBounds:
			return EArcTownPlacementRejection::PlotOutsideVolumeBounds;
		case ESiteGate::OutsideSite:
			return EArcTownPlacementRejection::PlotOutsideSite;
		default:
			return EArcTownPlacementRejection::PlotNotBuildable;
		}
	}

	EArcTownPlacementRejection PlacedRejection(const ESiteGate Gate)
	{
		switch (Gate)
		{
		case ESiteGate::OutsideVolumeBounds:
			return EArcTownPlacementRejection::PlacedOutsideVolumeBounds;
		case ESiteGate::OutsideSite:
			return EArcTownPlacementRejection::PlacedOutsideSite;
		default:
			return EArcTownPlacementRejection::PlacedNotBuildable;
		}
	}

	struct FCarriagewayHit
	{
		int32 SnapshotEdgeIndex = INDEX_NONE;
		double PerpendicularDistanceCm = 0.0;
		double MaxHalfWidthCm = 0.0;
		double AlongAxisDistanceCm = 0.0;
		double UnovershotHalfLengthCm = 0.0;
		bool bOverlapsWithoutOverhang = false;
		double MinPenetrationCm = 0.0;
	};

	double DiagnosticAxisPenetrationCm(
		const FFootprintRect& A,
		const FFootprintRect& B,
		const FVector2D& Axis,
		const double ClearanceCm)
	{
		const double Distance = FMath::Abs(FVector2D::DotProduct(B.Center - A.Center, Axis));
		const double RadiusA = FMath::Abs(FVector2D::DotProduct(A.AxisX * A.HalfExtent.X, Axis))
			+ FMath::Abs(FVector2D::DotProduct(A.AxisY * A.HalfExtent.Y, Axis));
		const double RadiusB = FMath::Abs(FVector2D::DotProduct(B.AxisX * B.HalfExtent.X, Axis))
			+ FMath::Abs(FVector2D::DotProduct(B.AxisY * B.HalfExtent.Y, Axis));
		return RadiusA + RadiusB - ClearanceCm - Distance;
	}

	double DiagnosticMinPenetrationCm(const FFootprintRect& A, const FFootprintRect& B, const double ClearanceCm)
	{
		double MinPenetration = DiagnosticAxisPenetrationCm(A, B, A.AxisX, ClearanceCm);
		MinPenetration = FMath::Min(MinPenetration, DiagnosticAxisPenetrationCm(A, B, A.AxisY, ClearanceCm));
		MinPenetration = FMath::Min(MinPenetration, DiagnosticAxisPenetrationCm(A, B, B.AxisX, ClearanceCm));
		MinPenetration = FMath::Min(MinPenetration, DiagnosticAxisPenetrationCm(A, B, B.AxisY, ClearanceCm));
		return MinPenetration;
	}

	double DiagnosticMedianCm(TArray<double> Values)
	{
		if (Values.IsEmpty())
		{
			return 0.0;
		}

		Values.Sort();
		const int32 MidIndex = Values.Num() / 2;
		return (Values.Num() % 2) != 0
			? Values[MidIndex]
			: 0.5 * (Values[MidIndex - 1] + Values[MidIndex]);
	}

	double DiagnosticMaxCm(TConstArrayView<double> Values)
	{
		double MaxValue = 0.0;
		for (const double Value : Values)
		{
			MaxValue = FMath::Max(MaxValue, Value);
		}
		return MaxValue;
	}

	struct FCarriagewayGrid
	{
		double CellSizeCm = 1.0;
		// A frontage front line lies on the reservation edge by construction and the arrangement snapped the vertices it is built from within its own tolerance, so only a deeper penetration stands in the carriageway.
		double ClearanceCm = 0.0;
		TArray<FFootprintRect> Carriageways;
		TArray<int32> CarriagewayEdgeIndices;
		TMap<FIntPoint, TArray<int32>> Cells;

		FIntPoint ToCell(const FVector2D& PointXY) const
		{
			return FIntPoint(
				FMath::FloorToInt32(PointXY.X / CellSizeCm),
				FMath::FloorToInt32(PointXY.Y / CellSizeCm));
		}

		void Insert(const FFootprintRect& Carriageway, const int32 SnapshotEdgeIndex)
		{
			const int32 Index = Carriageways.Add(Carriageway);
			CarriagewayEdgeIndices.Add(SnapshotEdgeIndex);
			const FVector2D Extent = RectExtentAA(Carriageway);
			const FIntPoint MinCell = ToCell(Carriageway.Center - Extent);
			const FIntPoint MaxCell = ToCell(Carriageway.Center + Extent);
			for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			{
				for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
				{
					Cells.FindOrAdd(FIntPoint(CellX, CellY)).Add(Index);
				}
			}
		}

		bool OverlapsAny(
			const FFootprintRect& Rect,
			const TConstArrayView<int32> IgnoreSnapshotEdgeIndices,
			FCarriagewayHit& OutHit) const
		{
			OutHit = FCarriagewayHit();
			const FVector2D Extent = RectExtentAA(Rect);
			const FIntPoint MinCell = ToCell(Rect.Center - Extent);
			const FIntPoint MaxCell = ToCell(Rect.Center + Extent);
			for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			{
				for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
				{
					const TArray<int32>* const Bucket = Cells.Find(FIntPoint(CellX, CellY));
					if (Bucket == nullptr)
					{
						continue;
					}

					for (const int32 Index : *Bucket)
					{
						// A run's own steps reserve across each other's frontage, so the whole run is exempt, not just the seated step.
						if (IgnoreSnapshotEdgeIndices.Contains(CarriagewayEdgeIndices[Index]))
						{
							continue;
						}

						if (Overlaps(Rect, Carriageways[Index], ClearanceCm))
						{
							const FFootprintRect& HitCarriageway = Carriageways[Index];
							OutHit.SnapshotEdgeIndex = CarriagewayEdgeIndices[Index];
							OutHit.PerpendicularDistanceCm = FMath::Abs(FVector2D::DotProduct(
								Rect.Center - HitCarriageway.Center, HitCarriageway.AxisY));
							OutHit.MaxHalfWidthCm = HitCarriageway.HalfExtent.Y;
							OutHit.AlongAxisDistanceCm = FMath::Abs(FVector2D::DotProduct(
								Rect.Center - HitCarriageway.Center, HitCarriageway.AxisX));
							OutHit.UnovershotHalfLengthCm = HitCarriageway.HalfExtent.X;
							FFootprintRect UnovershotCarriageway = HitCarriageway;
							UnovershotCarriageway.HalfExtent.X = OutHit.UnovershotHalfLengthCm;
							OutHit.bOverlapsWithoutOverhang = Overlaps(Rect, UnovershotCarriageway, ClearanceCm);
							OutHit.MinPenetrationCm =
								DiagnosticMinPenetrationCm(Rect, HitCarriageway, ClearanceCm);
							return true;
						}
					}
				}
			}
			return false;
		}
	};

	bool TryMakeCarriagewayRect(
		const FArcTownWorldSnapshot& Snapshot,
		const FArcTownTrafficSettings& TrafficSettings,
		const FArcTownRoadEdge& Edge,
		FFootprintRect& OutRect)
	{
		if (!Snapshot.Nodes.IsValidIndex(Edge.NodeA) || !Snapshot.Nodes.IsValidIndex(Edge.NodeB))
		{
			return false;
		}

		const FVector2D Start = ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY);
		const FVector2D Span = ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY) - Start;
		const double LengthCm = Span.Size();
		if (!FMath::IsFinite(LengthCm) || LengthCm <= UE_DOUBLE_SMALL_NUMBER)
		{
			return false;
		}

		OutRect.AxisX = Span / LengthCm;
		OutRect.AxisY = FVector2D(-OutRect.AxisX.Y, OutRect.AxisX.X);
		OutRect.Center = Start + Span * 0.5;
		// A planned edge reserves the width promotion can widen it to, or a building set back from today's axis stands in the carriageway the year that edge widens.
		const double HalfWidthCm =
			ArcTownTraffic::MaxHalfWidthCm(TrafficSettings, static_cast<double>(Edge.HalfWidth));
		// The reservation ends at its own nodes: a half-width of overhang reached into the frontage of every
		// adjoining edge, and the junction wedge it closed is a hole this rectangle cannot cover.
		OutRect.HalfExtent = FVector2D(0.5 * LengthCm, HalfWidthCm);
		return true;
	}

	void BuildCarriagewayGrid(
		const FArcTownWorldSnapshot& Snapshot,
		const FArcTownTrafficSettings& TrafficSettings,
		const double QueryReachCm,
		const double ClearanceCm,
		FCarriagewayGrid& OutGrid)
	{
		TArray<FFootprintRect> Carriageways;
		TArray<int32> EdgeIndices;
		Carriageways.Reserve(Snapshot.Edges.Num());
		EdgeIndices.Reserve(Snapshot.Edges.Num());

		double CellSizeCm = FMath::Max(QueryReachCm, 1.0);
		for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
		{
			FFootprintRect Carriageway;
			if (!TryMakeCarriagewayRect(Snapshot, TrafficSettings, Snapshot.Edges[EdgeIndex], Carriageway))
			{
				continue;
			}

			const FVector2D Extent = RectExtentAA(Carriageway);
			CellSizeCm = FMath::Max(CellSizeCm, 2.0 * FMath::Max(Extent.X, Extent.Y));
			Carriageways.Add(Carriageway);
			EdgeIndices.Add(EdgeIndex);
		}

		OutGrid.CellSizeCm = CellSizeCm;
		OutGrid.ClearanceCm = ClearanceCm;
		OutGrid.Carriageways.Reset();
		OutGrid.Carriageways.Reserve(Carriageways.Num());
		OutGrid.CarriagewayEdgeIndices.Reset();
		OutGrid.CarriagewayEdgeIndices.Reserve(Carriageways.Num());
		OutGrid.Cells.Reset();
		for (int32 Index = 0; Index < Carriageways.Num(); ++Index)
		{
			OutGrid.Insert(Carriageways[Index], EdgeIndices[Index]);
		}
	}

	bool IsEntryUsable(const FArcTownFrozenBuildingEntry& Entry)
	{
		return FMath::IsFinite(Entry.FootprintXY.X)
			&& FMath::IsFinite(Entry.FootprintXY.Y)
			&& Entry.FootprintXY.X > 0.0
			&& Entry.FootprintXY.Y > 0.0
			&& FMath::IsFinite(Entry.InfluenceRadiusCm)
			&& Entry.InfluenceRadiusCm > 0.0;
	}

	double TagMatch(const FGameplayTagContainer& Wanted, const FGameplayTagContainer& Offered)
	{
		if (Wanted.IsEmpty())
		{
			return 0.0;
		}

		int32 Matched = 0;
		for (const FGameplayTag& Tag : Wanted)
		{
			if (Offered.HasTag(Tag))
			{
				++Matched;
			}
		}
		return static_cast<double>(Matched) / static_cast<double>(Wanted.Num());
	}

	struct FPlotSite
	{
		FVector2D FrontMidXY = FVector2D::ZeroVector;
		FVector2D InwardXY = FVector2D(0.0, 1.0);
		double SetbackCm = 0.0;
		double DoorAngleRad = 0.0;
		int32 CellIndex = INDEX_NONE;
		double SiteTerm = 0.0;
	};

	struct FPlot
	{
		FPlotSite Site;
		TArray<FVector2D> Polygon;
		int32 EntryIndex = INDEX_NONE;
		int32 EdgeIndex = INDEX_NONE;
		TArray<int32> ExemptEdgeIndices;
		int32 BlockIndex = INDEX_NONE;
		int32 Ordinal = 0;
	};

	FVector2D PlacedCenter(
		const FArcTownFrozenBuildingEntry& Entry,
		const FPlotSite& Site,
		const double MeshYawRad)
	{
		return Site.FrontMidXY
			+ Site.InwardXY
				* (Site.SetbackCm
					+ SupportAlong(MakeBareFootprintRect(Entry, Site.FrontMidXY, MeshYawRad), Site.InwardXY));
	}

	double Cross2D(const FVector2D& A, const FVector2D& B)
	{
		return A.X * B.Y - A.Y * B.X;
	}

	double PolygonArea(TConstArrayView<FVector2D> Polygon)
	{
		double TwiceArea = 0.0;
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const int32 NextIndex = Index + 1 == Polygon.Num() ? 0 : Index + 1;
			TwiceArea += Cross2D(Polygon[Index], Polygon[NextIndex]);
		}
		return 0.5 * TwiceArea;
	}

	FBox2D PolygonBounds(TConstArrayView<FVector2D> Polygon)
	{
		FBox2D Bounds(ForceInit);
		for (const FVector2D& Vertex : Polygon)
		{
			Bounds += Vertex;
		}
		return Bounds;
	}

	void ClipConvexByHalfPlane(
		TArray<FVector2D>& Polygon,
		const FVector2D& PointOnLine,
		const FVector2D& InwardNormal,
		TArray<FVector2D>& Scratch)
	{
		Scratch.Reset();
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const int32 NextIndex = Index + 1 == Polygon.Num() ? 0 : Index + 1;
			const FVector2D& Current = Polygon[Index];
			const FVector2D& Next = Polygon[NextIndex];
			const double CurrentSide = FVector2D::DotProduct(Current - PointOnLine, InwardNormal);
			const double NextSide = FVector2D::DotProduct(Next - PointOnLine, InwardNormal);

			if (CurrentSide >= 0.0)
			{
				Scratch.Add(Current);
			}

			if ((CurrentSide > 0.0 && NextSide < 0.0) || (CurrentSide < 0.0 && NextSide > 0.0))
			{
				Scratch.Add(Current + (Next - Current) * (CurrentSide / (CurrentSide - NextSide)));
			}
		}

		Polygon = Scratch;
	}

	void ProjectPolygon(
		TConstArrayView<FVector2D> Polygon,
		const FVector2D& Axis,
		double& OutMin,
		double& OutMax)
	{
		OutMin = TNumericLimits<double>::Max();
		OutMax = TNumericLimits<double>::Lowest();
		for (const FVector2D& Vertex : Polygon)
		{
			const double Projection = FVector2D::DotProduct(Vertex, Axis);
			OutMin = FMath::Min(OutMin, Projection);
			OutMax = FMath::Max(OutMax, Projection);
		}
	}

	bool SeparatedByEdgesOf(
		TConstArrayView<FVector2D> Source,
		TConstArrayView<FVector2D> Other)
	{
		for (int32 Index = 0; Index < Source.Num(); ++Index)
		{
			const int32 NextIndex = Index + 1 == Source.Num() ? 0 : Index + 1;
			const FVector2D Edge = Source[NextIndex] - Source[Index];
			const double LengthCm = Edge.Size();
			if (LengthCm <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D Axis(-Edge.Y / LengthCm, Edge.X / LengthCm);
			double SourceMin = 0.0;
			double SourceMax = 0.0;
			double OtherMin = 0.0;
			double OtherMax = 0.0;
			ProjectPolygon(Source, Axis, SourceMin, SourceMax);
			ProjectPolygon(Other, Axis, OtherMin, OtherMax);
			if (SourceMax - OtherMin <= PlotTouchToleranceCm
				|| OtherMax - SourceMin <= PlotTouchToleranceCm)
			{
				return true;
			}
		}
		return false;
	}

	bool ConvexPolygonsOverlap(TConstArrayView<FVector2D> A, TConstArrayView<FVector2D> B)
	{
		return !SeparatedByEdgesOf(A, B) && !SeparatedByEdgesOf(B, A);
	}

	void AccumulatePolygonPenetrationCm(
		TConstArrayView<FVector2D> Source,
		TConstArrayView<FVector2D> Other,
		double& InOutMinPenetrationCm)
	{
		for (int32 Index = 0; Index < Source.Num(); ++Index)
		{
			const int32 NextIndex = Index + 1 == Source.Num() ? 0 : Index + 1;
			const FVector2D Edge = Source[NextIndex] - Source[Index];
			const double LengthCm = Edge.Size();
			if (LengthCm <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}

			const FVector2D Axis(-Edge.Y / LengthCm, Edge.X / LengthCm);
			double SourceMin = 0.0;
			double SourceMax = 0.0;
			double OtherMin = 0.0;
			double OtherMax = 0.0;
			ProjectPolygon(Source, Axis, SourceMin, SourceMax);
			ProjectPolygon(Other, Axis, OtherMin, OtherMax);
			InOutMinPenetrationCm = FMath::Min(
				InOutMinPenetrationCm,
				FMath::Min(SourceMax - OtherMin, OtherMax - SourceMin) - PlotTouchToleranceCm);
		}
	}

	double DiagnosticPolygonPenetrationCm(TConstArrayView<FVector2D> A, TConstArrayView<FVector2D> B)
	{
		double MinPenetrationCm = TNumericLimits<double>::Max();
		AccumulatePolygonPenetrationCm(A, B, MinPenetrationCm);
		AccumulatePolygonPenetrationCm(B, A, MinPenetrationCm);
		return MinPenetrationCm == TNumericLimits<double>::Max() ? 0.0 : MinPenetrationCm;
	}

	void LogSiteGateRejection(
		const ESiteGate Gate,
		const FVector2D& PointXY,
		const TCHAR* Stage,
		const bool bPlaced)
	{
		int32& SamplesLogged = bPlaced ? GPlacedGateSamplesLogged : GPlotGateSamplesLogged;
		int32* const GateCounts = bPlaced ? GPlacedGateCounts : GPlotGateCounts;
		++GateCounts[static_cast<int32>(Gate)];
		UE_CLOGF(
			SamplesLogged < MaxDiagnosticSamplesPerRun,
			LogArcTownGrowth,
			Log,"ArcTown %ls site-gate sample %d/%d: %ls rejected by gate %ls, town-local XY (%.1f, %.1f).",
			bPlaced ? TEXT("placed") : TEXT("plot"),
			SamplesLogged + 1,
			MaxDiagnosticSamplesPerRun,
			Stage,
			SiteGateName(Gate),
			PointXY.X,
			PointXY.Y);
		SamplesLogged = FMath::Min(SamplesLogged + 1, MaxDiagnosticSamplesPerRun);
	}

	void LogBuildingLargerThanPlot(
		const FFootprintRect& Rect,
		TConstArrayView<FVector2D> Polygon,
		const FVector2D& InwardXY)
	{
		const FVector2D AlongXY(InwardXY.Y, -InwardXY.X);
		const double NeededAlongCm = 2.0 * SupportAlong(Rect, AlongXY);
		const double NeededAcrossCm = 2.0 * SupportAlong(Rect, InwardXY);

		double AlongMin = 0.0;
		double AlongMax = 0.0;
		double AcrossMin = 0.0;
		double AcrossMax = 0.0;
		ProjectPolygon(Polygon, AlongXY, AlongMin, AlongMax);
		ProjectPolygon(Polygon, InwardXY, AcrossMin, AcrossMax);
		const double PlotFrontCm = AlongMax - AlongMin;
		const double PlotDepthCm = AcrossMax - AcrossMin;
		const double AlongShortfallCm = NeededAlongCm - PlotFrontCm;
		const double AcrossShortfallCm = NeededAcrossCm - PlotDepthCm;

		UE_CLOGF(
			GBuildingLargerSamplesLogged < MaxDiagnosticSamplesPerRun,
			LogArcTownGrowth,
			Log,"ArcTown building-larger-than-plot sample %d/%d: building needs %.1f cm along the frontage and %.1f cm across it, plot offers front %.1f cm and depth %.1f cm, shortfall along %.1f cm and across %.1f cm.",
			GBuildingLargerSamplesLogged + 1,
			MaxDiagnosticSamplesPerRun,
			NeededAlongCm,
			NeededAcrossCm,
			PlotFrontCm,
			PlotDepthCm,
			AlongShortfallCm,
			AcrossShortfallCm);
		GBuildingLargerSamplesLogged =
			FMath::Min(GBuildingLargerSamplesLogged + 1, MaxDiagnosticSamplesPerRun);

		GBuildingLargerAlongShortfallCm.Add(AlongShortfallCm);
		GBuildingLargerAcrossShortfallCm.Add(AcrossShortfallCm);
	}

	void LogPlotOverlapsPlot(
		const FVector2D& FrontMidXY,
		const int32 RunId,
		const int32 OtherRunId,
		const double PenetrationCm)
	{
		const bool bSameRun = RunId == OtherRunId;
		UE_CLOGF(
			GPlotOverlapSamplesLogged < MaxDiagnosticSamplesPerRun,
			LogArcTownGrowth,
			Log,"ArcTown plot-overlaps-plot sample %d/%d: plot of run %d with front mid town-local XY (%.1f, %.1f) penetrates the plot of run %d by %.1f cm, same run=%d.",
			GPlotOverlapSamplesLogged + 1,
			MaxDiagnosticSamplesPerRun,
			RunId,
			FrontMidXY.X,
			FrontMidXY.Y,
			OtherRunId,
			PenetrationCm,
			bSameRun ? 1 : 0);
		GPlotOverlapSamplesLogged =
			FMath::Min(GPlotOverlapSamplesLogged + 1, MaxDiagnosticSamplesPerRun);

		GPlotOverlapSameRunCount += bSameRun ? 1 : 0;
		GPlotOverlapPenetrationCm.Add(PenetrationCm);
	}

	void LogDiagnosticAggregates()
	{
		UE_CLOGF(
			!GFrontageOverlapPenetrationCm.IsEmpty(),
			LogArcTownGrowth,
			Log,"ArcTown frontage overlap aggregate: %d rejections, %d still overlap with the along-axis overhang removed, %d do not, penetration depth median %.1f cm, max %.1f cm.",
			GFrontageOverlapPenetrationCm.Num(),
			GFrontageOverlapSurvivesWithoutOverhangCount,
			GFrontageOverlapRemovedByOverhangCount,
			DiagnosticMedianCm(GFrontageOverlapPenetrationCm),
			DiagnosticMaxCm(GFrontageOverlapPenetrationCm));

		const int32 PlotGateRejections = GPlotGateCounts[static_cast<int32>(ESiteGate::OutsideVolumeBounds)]
			+ GPlotGateCounts[static_cast<int32>(ESiteGate::OutsideSite)]
			+ GPlotGateCounts[static_cast<int32>(ESiteGate::NotBuildable)];
		UE_CLOGF(
			PlotGateRejections > 0,
			LogArcTownGrowth,
			Log,"ArcTown plot site-gate aggregate: %d rejections, OutsideVolumeBounds=%d, OutsideSite=%d, NotBuildable=%d.",
			PlotGateRejections,
			GPlotGateCounts[static_cast<int32>(ESiteGate::OutsideVolumeBounds)],
			GPlotGateCounts[static_cast<int32>(ESiteGate::OutsideSite)],
			GPlotGateCounts[static_cast<int32>(ESiteGate::NotBuildable)]);

		const int32 PlacedGateRejections = GPlacedGateCounts[static_cast<int32>(ESiteGate::OutsideVolumeBounds)]
			+ GPlacedGateCounts[static_cast<int32>(ESiteGate::OutsideSite)]
			+ GPlacedGateCounts[static_cast<int32>(ESiteGate::NotBuildable)];
		UE_CLOGF(
			PlacedGateRejections > 0,
			LogArcTownGrowth,
			Log,"ArcTown placed site-gate aggregate: %d rejections, OutsideVolumeBounds=%d, OutsideSite=%d, NotBuildable=%d.",
			PlacedGateRejections,
			GPlacedGateCounts[static_cast<int32>(ESiteGate::OutsideVolumeBounds)],
			GPlacedGateCounts[static_cast<int32>(ESiteGate::OutsideSite)],
			GPlacedGateCounts[static_cast<int32>(ESiteGate::NotBuildable)]);

		UE_CLOGF(
			!GBuildingLargerAlongShortfallCm.IsEmpty(),
			LogArcTownGrowth,
			Log,"ArcTown building-larger-than-plot aggregate: %d rejections, median shortfall along the frontage %.1f cm, across it %.1f cm.",
			GBuildingLargerAlongShortfallCm.Num(),
			DiagnosticMedianCm(GBuildingLargerAlongShortfallCm),
			DiagnosticMedianCm(GBuildingLargerAcrossShortfallCm));

		UE_CLOGF(
			!GPlotOverlapPenetrationCm.IsEmpty(),
			LogArcTownGrowth,
			Log,"ArcTown plot-overlaps-plot aggregate: %d rejections, %d against a plot of the same run, penetration depth median %.1f cm, max %.1f cm.",
			GPlotOverlapPenetrationCm.Num(),
			GPlotOverlapSameRunCount,
			DiagnosticMedianCm(GPlotOverlapPenetrationCm),
			DiagnosticMaxCm(GPlotOverlapPenetrationCm));
	}

	bool IsPointInsideConvex(TConstArrayView<FVector2D> Polygon, const FVector2D& PointXY)
	{
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const int32 NextIndex = Index + 1 == Polygon.Num() ? 0 : Index + 1;
			if (Cross2D(Polygon[NextIndex] - Polygon[Index], PointXY - Polygon[Index]) < -PlotTouchToleranceCm)
			{
				return false;
			}
		}
		return true;
	}

	bool RectFitsInsideConvex(const FFootprintRect& Rect, TConstArrayView<FVector2D> Polygon)
	{
		const FVector2D SpanX = Rect.AxisX * Rect.HalfExtent.X;
		const FVector2D SpanY = Rect.AxisY * Rect.HalfExtent.Y;
		const FVector2D Corners[4] = {
			Rect.Center - SpanX - SpanY,
			Rect.Center + SpanX - SpanY,
			Rect.Center + SpanX + SpanY,
			Rect.Center - SpanX + SpanY};

		for (const FVector2D& Corner : Corners)
		{
			if (!IsPointInsideConvex(Polygon, Corner))
			{
				return false;
			}
		}
		return true;
	}

	struct FPlotGrid
	{
		double CellSizeCm = 1.0;
		TArray<TArray<FVector2D>> Polygons;
		TArray<FBox2D> Bounds;
		TArray<int32> RunIds;
		TMap<FIntPoint, TArray<int32>> Cells;

		FIntPoint ToCell(const FVector2D& PointXY) const
		{
			return FIntPoint(
				FMath::FloorToInt32(PointXY.X / CellSizeCm),
				FMath::FloorToInt32(PointXY.Y / CellSizeCm));
		}

		bool OverlapsAny(
			TConstArrayView<FVector2D> Polygon,
			const FBox2D& PolygonBoundsXY,
			int32& OutOtherRunId,
			double& OutPenetrationCm) const
		{
			OutOtherRunId = INDEX_NONE;
			OutPenetrationCm = 0.0;
			const FIntPoint MinCell = ToCell(PolygonBoundsXY.Min);
			const FIntPoint MaxCell = ToCell(PolygonBoundsXY.Max);
			for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			{
				for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
				{
					const TArray<int32>* const Bucket = Cells.Find(FIntPoint(CellX, CellY));
					if (Bucket == nullptr)
					{
						continue;
					}

					for (const int32 Index : *Bucket)
					{
						if (Bounds[Index].Intersect(PolygonBoundsXY)
							&& ConvexPolygonsOverlap(Polygon, Polygons[Index]))
						{
							OutOtherRunId = RunIds[Index];
							OutPenetrationCm = DiagnosticPolygonPenetrationCm(Polygon, Polygons[Index]);
							return true;
						}
					}
				}
			}
			return false;
		}

		void Insert(TConstArrayView<FVector2D> Polygon, const FBox2D& PolygonBoundsXY, const int32 RunId)
		{
			const int32 Index = Polygons.Emplace(Polygon);
			Bounds.Add(PolygonBoundsXY);
			RunIds.Add(RunId);

			const FIntPoint MinCell = ToCell(PolygonBoundsXY.Min);
			const FIntPoint MaxCell = ToCell(PolygonBoundsXY.Max);
			for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
			{
				for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
				{
					Cells.FindOrAdd(FIntPoint(CellX, CellY)).Add(Index);
				}
			}
		}
	};

	struct FRankedPlot
	{
		int32 PlotIndex = INDEX_NONE;
		int64 QuantizedScore = 0;
		double Draw = 0.0;
	};

	struct FPointGrid
	{
		double CellSizeCm = 1.0;
		TMap<FIntPoint, TArray<int32>> Cells;

		FIntPoint ToCell(const FVector2D& PointXY) const
		{
			return FIntPoint(
				FMath::FloorToInt32(PointXY.X / CellSizeCm),
				FMath::FloorToInt32(PointXY.Y / CellSizeCm));
		}

		void Add(const FVector2D& PointXY, const int32 Index)
		{
			Cells.FindOrAdd(ToCell(PointXY)).Add(Index);
		}

		void Gather(const FVector2D& PointXY, TArray<int32>& OutIndices) const
		{
			OutIndices.Reset();
			const FIntPoint Base = ToCell(PointXY);
			for (int32 OffsetY = -1; OffsetY <= 1; ++OffsetY)
			{
				for (int32 OffsetX = -1; OffsetX <= 1; ++OffsetX)
				{
					if (const TArray<int32>* const Bucket = Cells.Find(Base + FIntPoint(OffsetX, OffsetY)))
					{
						OutIndices.Append(*Bucket);
					}
				}
			}
			OutIndices.Sort();
		}
	};

	double DistrictTerm(
		const FArcTownFrozenBuildingEntry& Entry,
		TConstArrayView<FArcTownFrozenAttractor> Attractors,
		const FVector2D& PointXY)
	{
		double Numerator = 0.0;
		double Denominator = 0.0;
		for (const FArcTownFrozenAttractor& Attractor : Attractors)
		{
			const double Strength = static_cast<double>(Attractor.Strength);
			if (!FMath::IsFinite(Strength) || Strength <= 0.0
				|| !FMath::IsFinite(Attractor.InfluenceRadiusCm) || Attractor.InfluenceRadiusCm <= 0.0)
			{
				continue;
			}

			const double DistanceCm = (PointXY - ToDouble(Attractor.CenterTownLocalXY)).Size();
			if (DistanceCm > Attractor.InfluenceRadiusCm)
			{
				continue;
			}

			Denominator += Strength;
			Numerator += Strength
				* ArcTownTensorField::Falloff(DistanceCm, Attractor.InfluenceRadiusCm)
				* TagMatch(Entry.PreferredDistricts, Attractor.DistrictTags);
		}

		// An empty carrier yields zero rather than renormalizing the remaining weights: a town judged on a
		// rescaled score in its first years would grow its core by different rules than its fringe.
		return Denominator > 0.0 ? Numerator / Denominator : 0.0;
	}

	double NeighbourTerm(
		const FGameplayTagContainer& Wanted,
		const FArcTownWorldSnapshot& Snapshot,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		TConstArrayView<int32> Neighbours,
		const FVector2D& PointXY)
	{
		if (Wanted.IsEmpty())
		{
			return 0.0;
		}

		double Numerator = 0.0;
		double Denominator = 0.0;
		for (const int32 BuildingIndex : Neighbours)
		{
			const FArcTownBuildingRecord& Record = Snapshot.Buildings[BuildingIndex];
			const FArcTownFrozenBuildingEntry& Emitter = Catalog[Record.CatalogIndex];
			if (!FMath::IsFinite(Emitter.InfluenceRadiusCm) || Emitter.InfluenceRadiusCm <= 0.0)
			{
				continue;
			}

			const double DistanceCm = (PointXY - ToDouble(Record.LocalXY)).Size();
			if (DistanceCm > Emitter.InfluenceRadiusCm)
			{
				continue;
			}

			const double Falloff = ArcTownTensorField::Falloff(DistanceCm, Emitter.InfluenceRadiusCm);
			Denominator += Falloff;
			Numerator += Falloff * TagMatch(Wanted, Emitter.BuildingTags);
		}

		return Denominator > 0.0 ? Numerator / Denominator : 0.0;
	}

	bool IsDeniedByDistance(
		const FArcTownFrozenBuildingEntry& Entry,
		const FArcTownWorldSnapshot& Snapshot,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		TConstArrayView<int32> Neighbours,
		const FVector2D& PointXY)
	{
		if (Entry.DenyMinDistanceCm <= 0.0 || Entry.DenyBuildings.IsEmpty())
		{
			return false;
		}

		for (const int32 BuildingIndex : Neighbours)
		{
			const FArcTownBuildingRecord& Record = Snapshot.Buildings[BuildingIndex];
			if (!Catalog[Record.CatalogIndex].BuildingTags.HasAny(Entry.DenyBuildings))
			{
				continue;
			}

			if ((PointXY - ToDouble(Record.LocalXY)).Size() < Entry.DenyMinDistanceCm)
			{
				return true;
			}
		}
		return false;
	}

	// DenyTerm only bends the score, so a dense town stays fillable; DenyMinDistance is the separate,
	// authored veto that rejects the position outright.
	bool TryScoreEntry(
		const FArcTownFrozenBuildingEntry& Entry,
		const FArcTownPlacementSettings& PlacementSettings,
		const FArcTownWorldSnapshot& Snapshot,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		TConstArrayView<FArcTownFrozenAttractor> Attractors,
		TConstArrayView<int32> Neighbours,
		const FPlotSite& PlotSite,
		double& OutScore,
		EArcTownPlacementRejection& OutReason)
	{
		OutScore = 0.0;
		const FVector2D UnjitteredCenterXY =
			PlacedCenter(Entry, PlotSite, MeshYaw(Entry, PlotSite.DoorAngleRad));
		if (IsDeniedByDistance(Entry, Snapshot, Catalog, Neighbours, UnjitteredCenterXY))
		{
			OutReason = EArcTownPlacementRejection::EntryDeniedByMinDistance;
			return false;
		}

		OutScore = PlacementSettings.DistrictWeight * DistrictTerm(Entry, Attractors, PlotSite.FrontMidXY)
			+ PlacementSettings.BuildingWeight
				* NeighbourTerm(Entry.PreferredBuildings, Snapshot, Catalog, Neighbours, PlotSite.FrontMidXY)
			- PlacementSettings.DenyWeight
				* NeighbourTerm(Entry.DenyBuildings, Snapshot, Catalog, Neighbours, PlotSite.FrontMidXY)
			+ PlacementSettings.SiteWeight * PlotSite.SiteTerm;
		if (!FMath::IsFinite(OutScore))
		{
			OutReason = EArcTownPlacementRejection::EntryScoreNotFinite;
			return false;
		}
		return true;
	}

	int32 QuantizeCoordinate(const float ValueCm)
	{
		const double Rounded =
			FMath::IsFinite(ValueCm) ? FMath::RoundToDouble(static_cast<double>(ValueCm)) : 0.0;
		return static_cast<int32>(FMath::Clamp(Rounded, -2147483648.0, 2147483647.0));
	}

	int64 PackBlockKey(const FVector2f& LexMinVertexXY)
	{
		const int64 PackedX = static_cast<int64>(QuantizeCoordinate(LexMinVertexXY.X));
		const uint32 PackedY = static_cast<uint32>(QuantizeCoordinate(LexMinVertexXY.Y));
		return (PackedX << 32) | static_cast<int64>(PackedY);
	}

	double PeriodJitterRad(const FArcTownMorphologySettings& MorphologySettings)
	{
		return MorphologySettings.Period == EArcTownPeriod::Default
			? FMath::DegreesToRadians(FMath::Max(MorphologySettings.YawJitterDeg, 0.0))
			: 0.0;
	}

	double PeriodGapCm(const FArcTownMorphologySettings& MorphologySettings)
	{
		return MorphologySettings.Period == EArcTownPeriod::Default
			? FMath::Max(MorphologySettings.DefaultPlotGapCm, 0.0)
			: 0.0;
	}

	// The plot is derived from the footprint, the period setback and the period jitter rather than from a
	// constant gap: a yaw of theta pushes a W by D footprint out to W*cos+D*sin across and D*cos+W*sin deep,
	// and a fixed gap would not pay for that at the 45 degrees YawJitterDeg allows.
	bool TryMakePlotSize(
		const FArcTownFrozenBuildingEntry& Entry,
		const FArcTownMorphologySettings& MorphologySettings,
		const double FrontUnit,
		const double DepthUnit,
		double& OutFrontCm,
		double& OutDepthCm)
	{
		double WidthCm = Entry.FootprintXY.X;
		double DepthCm = Entry.FootprintXY.Y;
		switch (MorphologySettings.Period)
		{
		case EArcTownPeriod::Roman:
			WidthCm = Entry.PlotSizeMaxXY.X;
			DepthCm = Entry.PlotSizeMaxXY.Y;
			break;
		case EArcTownPeriod::Medieval:
			WidthCm = FMath::Lerp(Entry.PlotSizeMinXY.X, Entry.PlotSizeMaxXY.X, FrontUnit);
			DepthCm = FMath::Lerp(Entry.PlotSizeMinXY.Y, Entry.PlotSizeMaxXY.Y, DepthUnit);
			break;
		default:
			break;
		}

		const double JitterRad = PeriodJitterRad(MorphologySettings);
		const double CosJitter = FMath::Cos(JitterRad);
		const double SinJitter = FMath::Sin(JitterRad);

		OutFrontCm = WidthCm * CosJitter + DepthCm * SinJitter + PeriodGapCm(MorphologySettings);
		OutDepthCm = ArcTownPlacement::PeriodSetbackCm(MorphologySettings)
			+ DepthCm * CosJitter + WidthCm * SinJitter;
		return FMath::IsFinite(OutFrontCm) && FMath::IsFinite(OutDepthCm)
			&& OutFrontCm > 0.0 && OutDepthCm > 0.0;
	}

	bool IsEntryEligible(const FArcTownFrozenBuildingEntry& Entry, const int32 Year)
	{
		return Entry.EarliestYear <= Year && Entry.SelectionWeight > 0.0f && IsEntryUsable(Entry);
	}

	struct FFrontageRun
	{
		FVector2D StartXY = FVector2D::ZeroVector;
		FVector2D AlongXY = FVector2D(1.0, 0.0);
		FVector2D InwardXY = FVector2D(0.0, 1.0);
		double LengthCm = 0.0;
		double StartOffsetCm = 0.0;
		double EndOffsetCm = 0.0;
		int32 EdgeIndex = INDEX_NONE;
		TArray<FVector2D> SideStarts;
		TArray<FVector2D> SideInwardNormals;
		TArray<double> SideEndAlongCm;
		TArray<uint8> SideHierarchy;
		TArray<int32> SideEdgeIndices;
		FVector2D StartBisectorNormal = FVector2D::ZeroVector;
		FVector2D EndBisectorNormal = FVector2D::ZeroVector;
	};

	int32 RunSideAtAlongCm(const FFrontageRun& Run, const double AlongCm)
	{
		for (int32 SideIndex = 0; SideIndex < Run.SideEndAlongCm.Num(); ++SideIndex)
		{
			if (Run.SideEndAlongCm[SideIndex] > AlongCm)
			{
				return SideIndex;
			}
		}
		return Run.SideEndAlongCm.Num() - 1;
	}

	double BisectorOffsetCm(
		const FVector2D& Incoming,
		const FVector2D& Outgoing,
		const double DepthCm,
		const double MaxOffsetCm)
	{
		const double Turn = Cross2D(Incoming, Outgoing);
		if (!(Turn > 0.0))
		{
			return 0.0;
		}

		const double Denominator = 1.0 + FVector2D::DotProduct(Incoming, Outgoing);
		if (Denominator <= UE_DOUBLE_SMALL_NUMBER)
		{
			return MaxOffsetCm;
		}

		return FMath::Min(DepthCm * Turn / Denominator, MaxOffsetCm);
	}

	void BuildRuns(
		const FArcTownBlock& Block,
		const double MaxPlotDepthCm,
		TArray<FFrontageRun>& OutRuns,
		FArcTownPlacementYearReport& OutReport)
	{
		OutRuns.Reset();

		const int32 SideCount = Block.Boundary.Num();
		if (SideCount < 2
			|| Block.BoundaryEdgeIndex.Num() != SideCount
			|| Block.BoundaryHalfWidth.Num() != SideCount
			|| Block.BoundaryHierarchy.Num() != SideCount)
		{
			ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::BlockBoundaryUnusable);
			return;
		}

		TArray<FVector2D> Vertices;
		TArray<FVector2D> Directions;
		Vertices.Reserve(SideCount);
		Directions.Reserve(SideCount);
		for (const FVector2f& Vertex : Block.Boundary)
		{
			Vertices.Add(FVector2D(static_cast<double>(Vertex.X), static_cast<double>(Vertex.Y)));
		}

		for (int32 Side = 0; Side < SideCount; ++Side)
		{
			const int32 NextSide = Side + 1 == SideCount ? 0 : Side + 1;
			const FVector2D Span = Vertices[NextSide] - Vertices[Side];
			const double LengthCm = Span.Size();
			if (!FMath::IsFinite(LengthCm) || LengthCm <= UE_DOUBLE_SMALL_NUMBER)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::BlockSideDegenerate);
				return;
			}
			Directions.Add(Span / LengthCm);
		}

		const double MinMergeCos = FMath::Cos(FMath::DegreesToRadians(MaxRunTurnDeg));
		TArray<uint8> bMergesWithPrevious;
		bMergesWithPrevious.SetNumZeroed(SideCount);
		for (int32 Side = 0; Side < SideCount; ++Side)
		{
			const int32 PreviousSide = (Side + SideCount - 1) % SideCount;
			const bool bBothOnStreet = Block.BoundaryEdgeIndex[Side] != INDEX_NONE
				&& Block.BoundaryEdgeIndex[PreviousSide] != INDEX_NONE;
			const bool bContinues =
				FVector2D::DotProduct(Directions[PreviousSide], Directions[Side]) >= MinMergeCos;
			bMergesWithPrevious[Side] = (bBothOnStreet && bContinues) ? 1 : 0;
		}

		int32 FirstSide = 0;
		for (int32 Side = 0; Side < SideCount; ++Side)
		{
			if (bMergesWithPrevious[Side] == 0)
			{
				FirstSide = Side;
				break;
			}
		}

		int32 Consumed = 0;
		while (Consumed < SideCount)
		{
			const int32 RunStart = (FirstSide + Consumed) % SideCount;
			if (Block.BoundaryEdgeIndex[RunStart] == INDEX_NONE)
			{
				++Consumed;
				continue;
			}

			int32 RunLength = 1;
			while (Consumed + RunLength < SideCount
				&& bMergesWithPrevious[(FirstSide + Consumed + RunLength) % SideCount] != 0)
			{
				++RunLength;
			}

			const int32 RunEnd = (RunStart + RunLength - 1) % SideCount;
			const int32 AfterRun = (RunEnd + 1) % SideCount;
			const FVector2D& StartVertex = Vertices[RunStart];
			const FVector2D& EndVertex = Vertices[AfterRun];
			const FVector2D Span = EndVertex - StartVertex;
			const double LengthCm = Span.Size();
			Consumed += RunLength;

			if (!FMath::IsFinite(LengthCm) || LengthCm <= UE_DOUBLE_SMALL_NUMBER)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::BlockRunsCollapsed);
				continue;
			}

			FFrontageRun& Run = OutRuns.AddDefaulted_GetRef();
			Run.StartXY = StartVertex;
			Run.AlongXY = Span / LengthCm;
			Run.InwardXY = FVector2D(-Run.AlongXY.Y, Run.AlongXY.X);
			Run.LengthCm = LengthCm;
			Run.EdgeIndex = Block.BoundaryEdgeIndex[RunStart];

			Run.SideStarts.Reserve(RunLength);
			Run.SideInwardNormals.Reserve(RunLength);
			Run.SideEndAlongCm.Reserve(RunLength);
			Run.SideHierarchy.Reserve(RunLength);
			Run.SideEdgeIndices.Reserve(RunLength);
			double SideEndCm = 0.0;
			for (int32 Offset = 0; Offset < RunLength; ++Offset)
			{
				const int32 Side = (RunStart + Offset) % SideCount;
				const int32 SideEndVertex = (Side + 1) % SideCount;
				Run.SideStarts.Add(Vertices[Side]);
				Run.SideInwardNormals.Add(FVector2D(-Directions[Side].Y, Directions[Side].X));
				SideEndCm = FMath::Max(
					SideEndCm, FVector2D::DotProduct(Vertices[SideEndVertex] - StartVertex, Run.AlongXY));
				Run.SideEndAlongCm.Add(SideEndCm);
				Run.SideHierarchy.Add(Block.BoundaryHierarchy[Side]);
				Run.SideEdgeIndices.Add(Block.BoundaryEdgeIndex[Side]);
			}
			Run.SideEndAlongCm.Last() = FMath::Max(Run.SideEndAlongCm.Last(), LengthCm);

			// Only a corner where another street frontage starts owns half the wedge; a bevel or a strip end
			// has no rival plot to split it with, and bisecting it there would starve the whole run.
			const int32 BeforeRun = (RunStart + SideCount - 1) % SideCount;
			if (Block.BoundaryEdgeIndex[BeforeRun] != INDEX_NONE)
			{
				Run.StartOffsetCm =
					BisectorOffsetCm(Directions[BeforeRun], Run.AlongXY, MaxPlotDepthCm, LengthCm);

				const FVector2D StartBisector = Run.AlongXY - Directions[BeforeRun];
				if (StartBisector.SizeSquared() > UE_DOUBLE_SMALL_NUMBER)
				{
					const FVector2D Normal(-StartBisector.Y, StartBisector.X);
					Run.StartBisectorNormal =
						FVector2D::DotProduct(Normal, Run.AlongXY) >= 0.0 ? Normal : -Normal;
					Run.StartBisectorNormal.Normalize();
				}
			}

			if (Block.BoundaryEdgeIndex[AfterRun] != INDEX_NONE)
			{
				Run.EndOffsetCm =
					BisectorOffsetCm(Run.AlongXY, Directions[AfterRun], MaxPlotDepthCm, LengthCm);

				const FVector2D EndBisector = Directions[AfterRun] - Run.AlongXY;
				if (EndBisector.SizeSquared() > UE_DOUBLE_SMALL_NUMBER)
				{
					const FVector2D Normal(-EndBisector.Y, EndBisector.X);
					Run.EndBisectorNormal =
						FVector2D::DotProduct(Normal, Run.AlongXY) <= 0.0 ? Normal : -Normal;
					Run.EndBisectorNormal.Normalize();
				}
			}
		}
	}

	struct FInterval
	{
		double StartCm = 0.0;
		double EndCm = 0.0;
	};

	void BuildFreeIntervals(
		const FFrontageRun& Run,
		const FArcTownWorldSnapshot& Snapshot,
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
		const double MaxPlotDepthCm,
		const double HalfGapCm,
		TArray<FInterval>& Occupied,
		TArray<FInterval>& OutFree)
	{
		OutFree.Reset();
		Occupied.Reset();

		const double UsableStartCm = FMath::Min(Run.StartOffsetCm, Run.LengthCm);
		const double UsableEndCm = FMath::Max(Run.LengthCm - Run.EndOffsetCm, UsableStartCm);
		if (UsableEndCm - UsableStartCm <= 0.0)
		{
			return;
		}

		for (const FArcTownBuildingRecord& Record : Snapshot.Buildings)
		{
			const FArcTownFrozenBuildingEntry& Entry = Catalog[Record.CatalogIndex];
			const double ReachCm = RectReach(Entry);
			const FVector2D CenterXY = ToDouble(Record.LocalXY);
			const FVector2D Offset = CenterXY - Run.StartXY;
			const double DepthCm = FVector2D::DotProduct(Offset, Run.InwardXY);
			if (!FMath::IsFinite(DepthCm)
				|| DepthCm + ReachCm <= 0.0
				|| DepthCm - ReachCm >= MaxPlotDepthCm)
			{
				continue;
			}

			const double SupportCm = SupportAlong(
				MakeBareFootprintRect(Entry, CenterXY, static_cast<double>(Record.Yaw)), Run.AlongXY);
			const double AlongCm = FVector2D::DotProduct(Offset, Run.AlongXY);
			const double LowCm = AlongCm - SupportCm - HalfGapCm;
			const double HighCm = AlongCm + SupportCm + HalfGapCm;
			if (HighCm <= UsableStartCm || LowCm >= UsableEndCm)
			{
				continue;
			}

			Occupied.Add({LowCm, HighCm});
		}

		Occupied.Sort(
			[](const FInterval& Left, const FInterval& Right) -> bool
			{
				return Left.StartCm < Right.StartCm;
			});

		double CursorCm = UsableStartCm;
		for (const FInterval& Interval : Occupied)
		{
			if (Interval.StartCm > CursorCm)
			{
				OutFree.Add({CursorCm, FMath::Min(Interval.StartCm, UsableEndCm)});
			}

			CursorCm = FMath::Max(CursorCm, Interval.EndCm);
			if (CursorCm >= UsableEndCm)
			{
				break;
			}
		}

		if (CursorCm < UsableEndCm)
		{
			OutFree.Add({CursorCm, UsableEndCm});
		}
	}

	bool TryBuildPlotPolygon(
		const FFrontageRun& Run,
		const double StartAlongCm,
		const double FrontCm,
		const double DepthCm,
		TArray<FVector2D>& OutPolygon,
		TArray<FVector2D>& Scratch)
	{
		const FVector2D Near = Run.StartXY + Run.AlongXY * StartAlongCm;
		const FVector2D Front = Run.AlongXY * FrontCm;
		const FVector2D Deep = Run.InwardXY * DepthCm;

		OutPolygon.Reset();
		OutPolygon.Add(Near);
		OutPolygon.Add(Near + Front);
		OutPolygon.Add(Near + Front + Deep);
		OutPolygon.Add(Near + Deep);

		for (int32 Side = 0; Side < Run.SideStarts.Num(); ++Side)
		{
			ClipConvexByHalfPlane(OutPolygon, Run.SideStarts[Side], Run.SideInwardNormals[Side], Scratch);
			if (OutPolygon.Num() < 3)
			{
				return false;
			}
		}

		if (!Run.StartBisectorNormal.IsZero())
		{
			ClipConvexByHalfPlane(OutPolygon, Run.StartXY, Run.StartBisectorNormal, Scratch);
			if (OutPolygon.Num() < 3)
			{
				return false;
			}
		}

		if (!Run.EndBisectorNormal.IsZero())
		{
			ClipConvexByHalfPlane(
				OutPolygon, Run.StartXY + Run.AlongXY * Run.LengthCm, Run.EndBisectorNormal, Scratch);
			if (OutPolygon.Num() < 3)
			{
				return false;
			}
		}

		return PolygonArea(OutPolygon) > 0.0;
	}

	struct FTilingContext
	{
		const FArcTownSiteSnapshot* Site = nullptr;
		const FArcTownSiteFields* Fields = nullptr;
		const FArcTownFieldSettings* FieldSettings = nullptr;
		const FArcTownPlacementSettings* PlacementSettings = nullptr;
		const FArcTownMorphologySettings* MorphologySettings = nullptr;
		const FArcTownWorldSnapshot* Snapshot = nullptr;
		TConstArrayView<FArcTownFrozenBuildingEntry> Catalog;
		TConstArrayView<FArcTownFrozenAttractor> Attractors;
		TConstArrayView<int32> EligibleEntries;
		FBox2D VolumeBoundsTownLocalXY = FBox2D(ForceInit);
		double MaxPlotDepthCm = 0.0;
		int32 Year = 0;
	};

	void TileRun(
		const FTilingContext& Context,
		const FFrontageRun& Run,
		TConstArrayView<int32> RunEligibleEntries,
		const FPointGrid& NeighbourGrid,
		const int32 BlockIndex,
		const int64 BlockKey,
		const int32 RunId,
		const FInterval& Interval,
		int32& InOutOrdinal,
		FPlotGrid& InOutPlotGrid,
		TArray<FPlot>& OutPlots,
		FArcTownPlacementYearReport& OutReport)
	{
		const FArcTownMorphologySettings& Morphology = *Context.MorphologySettings;
		const double SetbackCm = ArcTownPlacement::PeriodSetbackCm(Morphology);
		const FVector2D OutwardXY = -Run.InwardXY;
		const double DoorAngleRad = FMath::Atan2(OutwardXY.Y, OutwardXY.X);

		TArray<int32> Neighbours;
		TArray<int32> TopEntries;
		TArray<int32> PlotEntries;
		TArray<FVector2D> Polygon;
		TArray<FVector2D> Scratch;

		double CursorCm = Interval.StartCm;
		int32 GatedSideIndex = INDEX_NONE;
		while (CursorCm < Interval.EndCm)
		{
			// Hierarchy is a per-edge promotion and a run merges on collinearity alone, so the admissible
			// subset belongs to the side the plot starts on, not to the run.
			const int32 SideIndex = RunSideAtAlongCm(Run, CursorCm);
			if (SideIndex != GatedSideIndex)
			{
				GatedSideIndex = SideIndex;
				const uint8 SideHierarchy = Run.SideHierarchy[SideIndex];
				PlotEntries.Reset();
				for (const int32 EntryIndex : RunEligibleEntries)
				{
					const FArcTownFrozenBuildingEntry& Entry = Context.Catalog[EntryIndex];
					if (SideHierarchy >= Entry.MinHierarchy && SideHierarchy <= Entry.MaxHierarchy)
					{
						PlotEntries.Add(EntryIndex);
					}
				}
			}

			if (PlotEntries.IsEmpty())
			{
				ArcTownGrowthDiagnostics::Tally(
					OutReport, EArcTownPlacementRejection::PlotHierarchyInadmissible);
				const double SideEndCm = Run.SideEndAlongCm[SideIndex];
				CursorCm = SideEndCm > CursorCm ? SideEndCm : Interval.EndCm;
				continue;
			}

			const int32 Ordinal = InOutOrdinal++;
			const double RemainingCm = Interval.EndCm - CursorCm;

			const double FrontUnit = ArcTownDeterministic::KeyToUnitOpen(ArcTownDeterministic::MakeAnnualKey(
				Context.Snapshot->SimulationId,
				Context.Snapshot->Seed,
				ArcTownDeterministic::EDeterministicDomain::PlotFrontageRoll,
				Context.Year,
				BlockIndex,
				Ordinal,
				BlockKey));
			const double DepthUnit = ArcTownDeterministic::KeyToUnitOpen(ArcTownDeterministic::MakeAnnualKey(
				Context.Snapshot->SimulationId,
				Context.Snapshot->Seed,
				ArcTownDeterministic::EDeterministicDomain::PlotDepthRoll,
				Context.Year,
				BlockIndex,
				Ordinal,
				BlockKey));

			double NarrowestFrontCm = 0.0;
			bool bAnyFront = false;
			for (const int32 EntryIndex : PlotEntries)
			{
				double FrontCm = 0.0;
				double DepthCm = 0.0;
				if (!TryMakePlotSize(
					Context.Catalog[EntryIndex], Morphology, FrontUnit, DepthUnit, FrontCm, DepthCm))
				{
					continue;
				}

				if (!bAnyFront || FrontCm < NarrowestFrontCm)
				{
					NarrowestFrontCm = FrontCm;
					bAnyFront = true;
				}
			}

			if (!bAnyFront)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::PlotWithoutPlotSize);
				return;
			}

			if (NarrowestFrontCm > RemainingCm)
			{
				return;
			}

			// SiteTerm and the site gates are read at the middle of the plot frontage, the point the plot is
			// judged on; leaving SiteTerm zero would drop the term that decides where the town grows.
			const FVector2D ProbeXY = Run.StartXY + Run.AlongXY * (CursorCm + 0.5 * NarrowestFrontCm);
			int32 ProbeCellIndex = INDEX_NONE;
			const ESiteGate ProbeGate = TestSiteGates(
				*Context.Site, *Context.Fields, Context.VolumeBoundsTownLocalXY, ProbeXY, ProbeCellIndex);
			if (ProbeGate != ESiteGate::Accepted)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, PlotRejection(ProbeGate));
				LogSiteGateRejection(ProbeGate, ProbeXY, TEXT("narrowest-front probe"), false);
				CursorCm += NarrowestFrontCm;
				continue;
			}

			FPlotSite ProbeSite;
			ProbeSite.FrontMidXY = ProbeXY;
			ProbeSite.InwardXY = Run.InwardXY;
			ProbeSite.SetbackCm = SetbackCm;
			ProbeSite.DoorAngleRad = DoorAngleRad;
			ProbeSite.CellIndex = ProbeCellIndex;
			ProbeSite.SiteTerm =
				ArcTownBootstrap::SiteTerm(*Context.Fields, *Context.FieldSettings, ProbeCellIndex);

			NeighbourGrid.Gather(ProbeXY, Neighbours);

			int64 BestQuantizedScore = 0;
			bool bAnyScored = false;
			TopEntries.Reset();
			for (const int32 EntryIndex : PlotEntries)
			{
				double FrontCm = 0.0;
				double DepthCm = 0.0;
				if (!TryMakePlotSize(
						Context.Catalog[EntryIndex], Morphology, FrontUnit, DepthUnit, FrontCm, DepthCm)
					|| FrontCm > RemainingCm)
				{
					continue;
				}

				double Score = 0.0;
				EArcTownPlacementRejection Reason = EArcTownPlacementRejection::EntryScoreNotFinite;
				if (!TryScoreEntry(
					Context.Catalog[EntryIndex],
					*Context.PlacementSettings,
					*Context.Snapshot,
					Context.Catalog,
					Context.Attractors,
					Neighbours,
					ProbeSite,
					Score,
					Reason))
				{
					ArcTownGrowthDiagnostics::Tally(OutReport, Reason);
					continue;
				}

				const int64 QuantizedScore = Quantize(Score, ScoreQuanta);
				if (!bAnyScored || QuantizedScore > BestQuantizedScore)
				{
					BestQuantizedScore = QuantizedScore;
					bAnyScored = true;
					TopEntries.Reset();
				}

				if (QuantizedScore == BestQuantizedScore)
				{
					TopEntries.Add(EntryIndex);
				}
			}

			if (TopEntries.IsEmpty())
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::PlotWithoutUsableEntry);
				CursorCm += NarrowestFrontCm;
				continue;
			}

			double TotalWeight = 0.0;
			for (const int32 EntryIndex : TopEntries)
			{
				TotalWeight += static_cast<double>(Context.Catalog[EntryIndex].SelectionWeight);
			}

			const uint64 TypeKey = ArcTownDeterministic::MakeAnnualKey(
				Context.Snapshot->SimulationId,
				Context.Snapshot->Seed,
				ArcTownDeterministic::EDeterministicDomain::BuildingTypePick,
				Context.Year,
				BlockIndex,
				Ordinal,
				BestQuantizedScore);
			double Roll = ArcTownDeterministic::KeyToUnitOpen(TypeKey) * TotalWeight;
			int32 ChosenEntry = TopEntries.Last();
			for (const int32 EntryIndex : TopEntries)
			{
				Roll -= static_cast<double>(Context.Catalog[EntryIndex].SelectionWeight);
				if (Roll < 0.0)
				{
					ChosenEntry = EntryIndex;
					break;
				}
			}

			double FrontCm = 0.0;
			double DepthCm = 0.0;
			if (!TryMakePlotSize(
				Context.Catalog[ChosenEntry], Morphology, FrontUnit, DepthUnit, FrontCm, DepthCm))
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::PlotWithoutUsableEntry);
				CursorCm += NarrowestFrontCm;
				continue;
			}

			const FVector2D FrontMidXY = Run.StartXY + Run.AlongXY * (CursorCm + 0.5 * FrontCm);
			int32 CellIndex = INDEX_NONE;
			const ESiteGate Gate = TestSiteGates(
				*Context.Site, *Context.Fields, Context.VolumeBoundsTownLocalXY, FrontMidXY, CellIndex);
			if (Gate != ESiteGate::Accepted)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, PlotRejection(Gate));
				LogSiteGateRejection(Gate, FrontMidXY, TEXT("chosen-entry front mid"), false);
				CursorCm += FrontCm;
				continue;
			}

			if (!TryBuildPlotPolygon(Run, CursorCm, FrontCm, DepthCm, Polygon, Scratch))
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::PlotClippedAway);
				CursorCm += FrontCm;
				continue;
			}

			const FBox2D PlotBoundsXY = PolygonBounds(Polygon);
			int32 OtherRunId = INDEX_NONE;
			double OverlapPenetrationCm = 0.0;
			if (InOutPlotGrid.OverlapsAny(Polygon, PlotBoundsXY, OtherRunId, OverlapPenetrationCm))
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::PlotOverlapsPlot);
				LogPlotOverlapsPlot(FrontMidXY, RunId, OtherRunId, OverlapPenetrationCm);
				CursorCm += FrontCm;
				continue;
			}

			FPlot& Plot = OutPlots.AddDefaulted_GetRef();
			Plot.Site.FrontMidXY = FrontMidXY;
			Plot.Site.InwardXY = Run.InwardXY;
			Plot.Site.SetbackCm = SetbackCm;
			Plot.Site.DoorAngleRad = DoorAngleRad;
			Plot.Site.CellIndex = CellIndex;
			Plot.Site.SiteTerm = ArcTownBootstrap::SiteTerm(*Context.Fields, *Context.FieldSettings, CellIndex);
			Plot.Polygon = Polygon;
			Plot.EntryIndex = ChosenEntry;
			Plot.EdgeIndex = Run.EdgeIndex;
			Plot.ExemptEdgeIndices.Reserve(Run.SideEdgeIndices.Num());
			for (const int32 SideEdgeIndex : Run.SideEdgeIndices)
			{
				if (SideEdgeIndex != INDEX_NONE)
				{
					Plot.ExemptEdgeIndices.AddUnique(SideEdgeIndex);
				}
			}
			Plot.BlockIndex = BlockIndex;
			Plot.Ordinal = Ordinal;

			InOutPlotGrid.Insert(Polygon, PlotBoundsXY, RunId);
			CursorCm += FrontCm;
		}
	}

	void TileFrontage(
		const FTilingContext& Context,
		TConstArrayView<FArcTownBlock> Frontage,
		const FPointGrid& NeighbourGrid,
		const double PlotGridCellSizeCm,
		TArray<FPlot>& OutPlots,
		FArcTownPlacementYearReport& OutReport)
	{
		OutPlots.Reset();

		const FArcTownMorphologySettings& Morphology = *Context.MorphologySettings;
		const double MinRunCm = FMath::Max(Morphology.MinFrontageRunCm, 0.0);
		const double HalfGapCm = 0.5 * PeriodGapCm(Morphology);

		FPlotGrid PlotGrid;
		PlotGrid.CellSizeCm = FMath::Max(PlotGridCellSizeCm, 1.0);

		TArray<FFrontageRun> Runs;
		TArray<FInterval> Occupied;
		TArray<FInterval> FreeIntervals;
		TArray<int32> RunEligibleEntries;
		int32 NextRunId = 0;
		for (int32 BlockIndex = 0; BlockIndex < Frontage.Num(); ++BlockIndex)
		{
			const FArcTownBlock& Block = Frontage[BlockIndex];
			if (Block.Boundary.IsEmpty())
			{
				continue;
			}

			const int64 BlockKey = PackBlockKey(Block.Boundary[0]);
			BuildRuns(Block, Context.MaxPlotDepthCm, Runs, OutReport);

			int32 Ordinal = 0;
			for (const FFrontageRun& Run : Runs)
			{
				const int32 RunId = NextRunId++;
				if (Run.LengthCm < MinRunCm)
				{
					ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::RunBelowMinFrontage);
					continue;
				}

				RunEligibleEntries.Reset();
				double RunMaxPlotDepthCm = 0.0;
				for (const int32 EntryIndex : Context.EligibleEntries)
				{
					const FArcTownFrozenBuildingEntry& Entry = Context.Catalog[EntryIndex];
					bool bAdmissibleOnAnySide = false;
					for (const uint8 SideHierarchy : Run.SideHierarchy)
					{
						if (SideHierarchy >= Entry.MinHierarchy && SideHierarchy <= Entry.MaxHierarchy)
						{
							bAdmissibleOnAnySide = true;
							break;
						}
					}

					if (!bAdmissibleOnAnySide)
					{
						continue;
					}

					RunEligibleEntries.Add(EntryIndex);

					double FrontCm = 0.0;
					double DepthCm = 0.0;
					if (TryMakePlotSize(Entry, Morphology, 1.0, 1.0, FrontCm, DepthCm))
					{
						RunMaxPlotDepthCm = FMath::Max(RunMaxPlotDepthCm, DepthCm);
					}
				}

				if (RunEligibleEntries.IsEmpty())
				{
					ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::RunHierarchyInadmissible);
					continue;
				}

				BuildFreeIntervals(
					Run,
					*Context.Snapshot,
					Context.Catalog,
					RunMaxPlotDepthCm,
					HalfGapCm,
					Occupied,
					FreeIntervals);

				for (const FInterval& Interval : FreeIntervals)
				{
					TileRun(
						Context,
						Run,
						RunEligibleEntries,
						NeighbourGrid,
						BlockIndex,
						BlockKey,
						RunId,
						Interval,
						Ordinal,
						PlotGrid,
						OutPlots,
						OutReport);
				}
			}
		}
	}
}
}

double ArcTownPlacement::PeriodSetbackCm(const FArcTownMorphologySettings& MorphologySettings)
{
	return MorphologySettings.Period == EArcTownPeriod::Default
		? FMath::Max(MorphologySettings.SetbackCm, 0.0)
		: 0.0;
}

double ArcTownPlacement::MaxPlotDepthCm(
	const FArcTownMorphologySettings& MorphologySettings,
	TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
	const int32 Year)
{
	using namespace ArcTownPlacementPrivate;

	double MaxDepthCm = 0.0;
	for (const FArcTownFrozenBuildingEntry& Entry : Catalog)
	{
		if (!IsEntryEligible(Entry, Year))
		{
			continue;
		}

		double FrontCm = 0.0;
		double DepthCm = 0.0;
		if (TryMakePlotSize(Entry, MorphologySettings, 1.0, 1.0, FrontCm, DepthCm))
		{
			MaxDepthCm = FMath::Max(MaxDepthCm, DepthCm);
		}
	}
	return MaxDepthCm;
}

bool ArcTownPlacement::PlaceBuildings(
	const FArcTownSiteSnapshot& Site,
	const FArcTownSiteFields& Fields,
	const FArcTownFieldSettings& FieldSettings,
	const FArcTownPlacementSettings& PlacementSettings,
	const FArcTownMorphologySettings& MorphologySettings,
	const FArcTownTrafficSettings& TrafficSettings,
	TConstArrayView<FArcTownFrozenBuildingEntry> Catalog,
	TConstArrayView<FArcTownFrozenAttractor> Attractors,
	TConstArrayView<FArcTownBlock> Frontage,
	const double FrontageSnapToleranceCm,
	const FBox2D& VolumeBoundsTownLocalXY,
	const int32 Year,
	FArcTownWorldSnapshot& Snapshot,
	FArcTownPlacementYearReport& OutReport,
	FText& OutError)
{
	using namespace ArcTownPlacementPrivate;

	// One call to PlaceBuildings simulates one year; scoping the diagnostic sample/aggregate state to it
	// keeps every year's diagnostics printed, not only the first PlaceBuildings call in the editor session.
	GFrontageOverlapSamplesLogged = 0;
	GFrontageOverlapSurvivesWithoutOverhangCount = 0;
	GFrontageOverlapRemovedByOverhangCount = 0;
	GFrontageOverlapPenetrationCm.Reset();
	GPlotGateSamplesLogged = 0;
	GPlacedGateSamplesLogged = 0;
	FMemory::Memzero(GPlotGateCounts, sizeof(GPlotGateCounts));
	FMemory::Memzero(GPlacedGateCounts, sizeof(GPlacedGateCounts));
	GBuildingLargerSamplesLogged = 0;
	GBuildingLargerAlongShortfallCm.Reset();
	GBuildingLargerAcrossShortfallCm.Reset();
	GPlotOverlapSamplesLogged = 0;
	GPlotOverlapSameRunCount = 0;
	GPlotOverlapPenetrationCm.Reset();

	OutError = FText::GetEmpty();
	OutReport = FArcTownPlacementYearReport();

	if (!Site.IsValid())
	{
		return SetError(OutError, TEXT("ArcTown cannot place buildings because the sampled site is not valid; sample the site again."));
	}

	if (!ArcTownBootstrap::AreFieldsUsable(Site, Fields))
	{
		return SetError(OutError, TEXT("ArcTown cannot place buildings because the terrain fields do not match the sampled site; build the fields again."));
	}

	if (!VolumeBoundsTownLocalXY.bIsValid)
	{
		return SetError(OutError, TEXT("ArcTown cannot place buildings without the town volume bounds in the town-local frame."));
	}

	if (Catalog.IsEmpty())
	{
		return SetError(OutError, TEXT("ArcTown cannot place buildings without a building catalog entry; give the town a catalog."));
	}

	// CatalogIndex indexes the frozen catalog, which is parallel to UArcTownBuildingCatalog::Buildings.
	for (int32 BuildingIndex = 0; BuildingIndex < Snapshot.Buildings.Num(); ++BuildingIndex)
	{
		if (!Catalog.IsValidIndex(Snapshot.Buildings[BuildingIndex].CatalogIndex))
		{
			return SetError(OutError, FString::Printf(
				TEXT("Building %d of this town names catalog entry %d, which the frozen catalog of %d entries does not have. Reset Simulation."),
				BuildingIndex,
				Snapshot.Buildings[BuildingIndex].CatalogIndex,
				Catalog.Num()));
		}
	}

	TArray<int32> EligibleEntries;
	for (int32 EntryIndex = 0; EntryIndex < Catalog.Num(); ++EntryIndex)
	{
		const FArcTownFrozenBuildingEntry& Entry = Catalog[EntryIndex];
		if (Entry.EarliestYear > Year)
		{
			ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::CatalogEntryEarliestYear);
		}
		else if (!(Entry.SelectionWeight > 0.0f))
		{
			ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::CatalogEntryZeroWeight);
		}
		else if (!IsEntryUsable(Entry))
		{
			ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::CatalogEntryUnusableFootprint);
		}
		else
		{
			EligibleEntries.Add(EntryIndex);
		}
	}
	if (EligibleEntries.IsEmpty())
	{
		ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::NoEligibleCatalogEntry);
		return true;
	}

	const int32 ExistingCount = Snapshot.Buildings.Num();
	const double ExpectedCount = PlacementSettings.BuildingsPerYear
		+ PlacementSettings.BuildingsPerStandingBuildingPerYear * static_cast<double>(ExistingCount);
	const uint64 CountKey = ArcTownDeterministic::MakeAnnualKey(
		Snapshot.SimulationId,
		Snapshot.Seed,
		ArcTownDeterministic::EDeterministicDomain::BuildingCountRoll,
		Year,
		0,
		0,
		static_cast<int64>(ExistingCount));
	int32 PlannedCount = 0;
	if (!ArcTownDeterministic::TryRoundNonNegative(ExpectedCount, CountKey, PlannedCount))
	{
		return SetError(OutError, FString::Printf(
			TEXT("ArcTown expects %.3f new buildings this year, which is not a count it can roll. Give BuildingsPerYear and BuildingsPerStandingBuildingPerYear finite, non-negative values."),
			ExpectedCount));
	}
	OutReport.BuildingsRolled = PlannedCount;
	if (PlannedCount <= 0)
	{
		ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::ZeroBuildingsRolled);
		return true;
	}

	double GridCellSizeCm = FMath::Max(Site.SampleSpacing, 1.0);
	double FootprintReachCm = 1.0;
	for (const FArcTownFrozenBuildingEntry& Entry : Catalog)
	{
		GridCellSizeCm = FMath::Max(GridCellSizeCm, Entry.InfluenceRadiusCm);
		GridCellSizeCm = FMath::Max(GridCellSizeCm, Entry.DenyMinDistanceCm + RectReach(Entry));
		GridCellSizeCm = FMath::Max(GridCellSizeCm, 2.0 * RectReach(Entry));
		FootprintReachCm = FMath::Max(FootprintReachCm, 2.0 * RectReach(Entry));
	}

	double MaxPlotFrontCm = 0.0;
	double MaxPlotDepthCm = 0.0;
	for (const int32 EntryIndex : EligibleEntries)
	{
		double FrontCm = 0.0;
		double DepthCm = 0.0;
		if (TryMakePlotSize(Catalog[EntryIndex], MorphologySettings, 1.0, 1.0, FrontCm, DepthCm))
		{
			MaxPlotFrontCm = FMath::Max(MaxPlotFrontCm, FrontCm);
			MaxPlotDepthCm = FMath::Max(MaxPlotDepthCm, DepthCm);
		}
	}

	FCarriagewayGrid CarriagewayGrid;
	BuildCarriagewayGrid(
		Snapshot,
		TrafficSettings,
		FootprintReachCm,
		FMath::IsFinite(FrontageSnapToleranceCm) ? FMath::Max(FrontageSnapToleranceCm, 0.0) : 0.0,
		CarriagewayGrid);

	FPointGrid Grid;
	Grid.CellSizeCm = GridCellSizeCm;
	for (int32 BuildingIndex = 0; BuildingIndex < Snapshot.Buildings.Num(); ++BuildingIndex)
	{
		Grid.Add(ToDouble(Snapshot.Buildings[BuildingIndex].LocalXY), BuildingIndex);
	}

	FTilingContext TilingContext;
	TilingContext.Site = &Site;
	TilingContext.Fields = &Fields;
	TilingContext.FieldSettings = &FieldSettings;
	TilingContext.PlacementSettings = &PlacementSettings;
	TilingContext.MorphologySettings = &MorphologySettings;
	TilingContext.Snapshot = &Snapshot;
	TilingContext.Catalog = Catalog;
	TilingContext.Attractors = Attractors;
	TilingContext.EligibleEntries = EligibleEntries;
	TilingContext.VolumeBoundsTownLocalXY = VolumeBoundsTownLocalXY;
	TilingContext.MaxPlotDepthCm = MaxPlotDepthCm;
	TilingContext.Year = Year;

	TArray<FPlot> Plots;
	TileFrontage(
		TilingContext,
		Frontage,
		Grid,
		2.0 * FMath::Sqrt(MaxPlotFrontCm * MaxPlotFrontCm + MaxPlotDepthCm * MaxPlotDepthCm),
		Plots,
		OutReport);

	OutReport.PlotsTiled = Plots.Num();
	if (Plots.IsEmpty())
	{
		ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::NoFreePlot);
		LogDiagnosticAggregates();
		return true;
	}

	FPointGrid PlotGrid;
	PlotGrid.CellSizeCm = GridCellSizeCm;
	for (int32 PlotIndex = 0; PlotIndex < Plots.Num(); ++PlotIndex)
	{
		PlotGrid.Add(Plots[PlotIndex].Site.FrontMidXY, PlotIndex);
	}

	const double JitterRad = PeriodJitterRad(MorphologySettings);

	TArray<uint8> bRejected;
	bRejected.SetNumZeroed(Plots.Num());
	TArray<uint8> bScoreDirty;
	bScoreDirty.Init(1, Plots.Num());
	TArray<uint8> bScoreUsable;
	bScoreUsable.SetNumZeroed(Plots.Num());
	TArray<int64> PlotScores;
	PlotScores.SetNumZeroed(Plots.Num());
	TArray<int32> Neighbours;
	TArray<int32> PlacedNeighbours;
	TArray<int32> DirtyPlots;
	TArray<FRankedPlot> Ranked;

	for (int32 Placed = 0; Placed < PlannedCount; ++Placed)
	{
		for (int32 PlotIndex = 0; PlotIndex < Plots.Num(); ++PlotIndex)
		{
			if (bRejected[PlotIndex] != 0 || bScoreDirty[PlotIndex] == 0)
			{
				continue;
			}

			const FPlot& Plot = Plots[PlotIndex];
			Grid.Gather(Plot.Site.FrontMidXY, Neighbours);

			double Score = 0.0;
			EArcTownPlacementRejection Reason = EArcTownPlacementRejection::EntryScoreNotFinite;
			const bool bScored = TryScoreEntry(
				Catalog[Plot.EntryIndex],
				PlacementSettings,
				Snapshot,
				Catalog,
				Attractors,
				Neighbours,
				Plot.Site,
				Score,
				Reason);
			if (!bScored)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, Reason);
			}

			bScoreDirty[PlotIndex] = 0;
			bScoreUsable[PlotIndex] = bScored ? 1 : 0;
			PlotScores[PlotIndex] = bScored ? Quantize(Score, ScoreQuanta) : 0;
		}

		Ranked.Reset();
		for (int32 PlotIndex = 0; PlotIndex < Plots.Num(); ++PlotIndex)
		{
			if (bRejected[PlotIndex] != 0 || bScoreUsable[PlotIndex] == 0)
			{
				continue;
			}

			FRankedPlot& Rank = Ranked.AddDefaulted_GetRef();
			Rank.PlotIndex = PlotIndex;
			Rank.QuantizedScore = PlotScores[PlotIndex];
			Rank.Draw = ArcTownDeterministic::KeyToUnitOpen(ArcTownDeterministic::MakeAnnualKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::BuildingCandidatePick,
				Year,
				PlotIndex,
				Placed,
				Rank.QuantizedScore));
		}

		Ranked.Sort([](const FRankedPlot& Left, const FRankedPlot& Right) -> bool
		{
			if (Left.QuantizedScore != Right.QuantizedScore)
			{
				return Left.QuantizedScore > Right.QuantizedScore;
			}
			if (Left.Draw != Right.Draw)
			{
				return Left.Draw > Right.Draw;
			}
			return Left.PlotIndex < Right.PlotIndex;
		});

		bool bPlacedThisRound = false;
		for (const FRankedPlot& Winner : Ranked)
		{
			++OutReport.CandidatesEvaluated;

			const FPlot& Plot = Plots[Winner.PlotIndex];
			const FArcTownFrozenBuildingEntry& Entry = Catalog[Plot.EntryIndex];

			const uint64 YawKey = ArcTownDeterministic::MakeAnnualKey(
				Snapshot.SimulationId,
				Snapshot.Seed,
				ArcTownDeterministic::EDeterministicDomain::BuildingYawJitter,
				Year,
				Plot.BlockIndex,
				Plot.Ordinal,
				Quantize(Plot.Site.DoorAngleRad, AngleQuanta));
			const double DoorAngleRad = Plot.Site.DoorAngleRad
				+ (ArcTownDeterministic::KeyToUnitOpen(YawKey) * 2.0 - 1.0) * JitterRad;
			const double MeshYawRad = MeshYaw(Entry, DoorAngleRad);

			const FVector2D PlacedXY = PlacedCenter(Entry, Plot.Site, MeshYawRad);
			const FFootprintRect Rect = MakeBareFootprintRect(Entry, PlacedXY, MeshYawRad);

			if (!RectFitsInsideConvex(Rect, Plot.Polygon))
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::BuildingLargerThanPlot);
				LogBuildingLargerThanPlot(Rect, Plot.Polygon, Plot.Site.InwardXY);
				bRejected[Winner.PlotIndex] = 1;
				continue;
			}

			int32 PlacedCellIndex = INDEX_NONE;
			const ESiteGate PlacedGate =
				TestSiteGates(Site, Fields, VolumeBoundsTownLocalXY, PlacedXY, PlacedCellIndex);
			if (PlacedGate != ESiteGate::Accepted)
			{
				ArcTownGrowthDiagnostics::Tally(OutReport, PlacedRejection(PlacedGate));
				LogSiteGateRejection(PlacedGate, PlacedXY, TEXT("placed centre"), true);
				bRejected[Winner.PlotIndex] = 1;
				continue;
			}

			Grid.Gather(PlacedXY, PlacedNeighbours);

			if (IsDeniedByDistance(Entry, Snapshot, Catalog, PlacedNeighbours, PlacedXY))
			{
				ArcTownGrowthDiagnostics::Tally(
					OutReport, EArcTownPlacementRejection::PlacedDeniedByMinDistance);
				bRejected[Winner.PlotIndex] = 1;
				continue;
			}

			FCarriagewayHit CarriagewayHit;
			if (CarriagewayGrid.OverlapsAny(Rect, Plot.ExemptEdgeIndices, CarriagewayHit))
			{
				const bool bHitFrontedEdge =
					Plot.ExemptEdgeIndices.Contains(CarriagewayHit.SnapshotEdgeIndex);
				const bool bHitBuiltEdge = !bHitFrontedEdge
					&& Snapshot.Edges.IsValidIndex(CarriagewayHit.SnapshotEdgeIndex)
					&& Snapshot.Edges[CarriagewayHit.SnapshotEdgeIndex].bBuilt != 0;
				ArcTownGrowthDiagnostics::Tally(
					OutReport,
					bHitFrontedEdge
						? EArcTownPlacementRejection::FrontageOverlapsRoadFronted
						: bHitBuiltEdge
							? EArcTownPlacementRejection::FrontageOverlapsRoadBuiltEdge
							: EArcTownPlacementRejection::FrontageOverlapsRoadPlannedEdge);

				UE_CLOGF(
					GFrontageOverlapSamplesLogged < MaxDiagnosticSamplesPerRun,
					LogArcTownGrowth,
					Log,"ArcTown frontage overlap sample %d/%d: plot fronted edge %d, hit edge %d (built=%d, hierarchy=%d), perpendicular distance %.1f cm from hit edge axis, hit edge max half-width %.1f cm, along-axis distance %.1f cm vs un-overhung half-length %.1f cm, overlaps without overhang=%d, min penetration %.1f cm.",
					GFrontageOverlapSamplesLogged + 1,
					MaxDiagnosticSamplesPerRun,
					Plot.EdgeIndex,
					CarriagewayHit.SnapshotEdgeIndex,
					Snapshot.Edges.IsValidIndex(CarriagewayHit.SnapshotEdgeIndex)
						? Snapshot.Edges[CarriagewayHit.SnapshotEdgeIndex].bBuilt
						: 0,
					Snapshot.Edges.IsValidIndex(CarriagewayHit.SnapshotEdgeIndex)
						? Snapshot.Edges[CarriagewayHit.SnapshotEdgeIndex].Hierarchy
						: 0,
					CarriagewayHit.PerpendicularDistanceCm,
					CarriagewayHit.MaxHalfWidthCm,
					CarriagewayHit.AlongAxisDistanceCm,
					CarriagewayHit.UnovershotHalfLengthCm,
					CarriagewayHit.bOverlapsWithoutOverhang ? 1 : 0,
					CarriagewayHit.MinPenetrationCm);
				GFrontageOverlapSamplesLogged =
					FMath::Min(GFrontageOverlapSamplesLogged + 1, MaxDiagnosticSamplesPerRun);

				if (CarriagewayHit.bOverlapsWithoutOverhang)
				{
					++GFrontageOverlapSurvivesWithoutOverhangCount;
				}
				else
				{
					++GFrontageOverlapRemovedByOverhangCount;
				}
				GFrontageOverlapPenetrationCm.Add(CarriagewayHit.MinPenetrationCm);

				bRejected[Winner.PlotIndex] = 1;
				continue;
			}

			FArcTownBuildingRecord Record;
			Record.CatalogIndex = Plot.EntryIndex;
			Record.LocalXY = ToFloat(PlacedXY);
			Record.Yaw = static_cast<float>(MeshYawRad);
			Record.BirthYear = Year;
			Record.EdgeIndex = Plot.EdgeIndex;

			const int32 NewBuildingIndex = Snapshot.Buildings.Add(Record);
			Grid.Add(PlacedXY, NewBuildingIndex);
			PlotGrid.Gather(PlacedXY, DirtyPlots);
			for (const int32 DirtyIndex : DirtyPlots)
			{
				bScoreDirty[DirtyIndex] = 1;
			}
			bRejected[Winner.PlotIndex] = 1;
			bPlacedThisRound = true;
			++OutReport.BuildingsPlaced;
			break;
		}

		if (!bPlacedThisRound)
		{
			ArcTownGrowthDiagnostics::Tally(OutReport, EArcTownPlacementRejection::RoundPlacedNothing);
			break;
		}
	}

	LogDiagnosticAggregates();

	return true;
}
