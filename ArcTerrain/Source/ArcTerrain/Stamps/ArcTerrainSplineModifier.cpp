// Copyright Lukasz Baran. All Rights Reserved.

#include "Stamps/ArcTerrainSplineModifier.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainChannelSet.h"
#include "ArcTerrainModule.h"
#include "Async/ParallelFor.h"
#include "Components/SplineComponent.h"
#include "Containers/ArrayView.h"
#include "Containers/StringView.h"
#include "GameFramework/Actor.h"
#include "Hash/Blake3.h"

#if WITH_EDITOR
#include "Misc/TransactionObjectEvent.h"
#include "PrimitiveDrawingUtils.h"
#include "UObject/UObjectGlobals.h"
#endif

namespace ArcTerrainSplineModifier_Local
{
	static constexpr int64 MaxFieldBytes = 64ll * 1024ll * 1024ll;

	// The bed, the union mask and one plane per channel are all rasterised at FieldDims, so the
	// channel count sets how coarse the grid has to be to stay inside the budget.
	int32 MaxFieldSideForPlanes(int32 NumPlanes)
	{
		const int64 MaxTexels =
			MaxFieldBytes / static_cast<int64>((NumPlanes + 2) * sizeof(float));
		int64 Side = 2;
		while ((Side + 1) * (Side + 1) <= MaxTexels)
		{
			++Side;
		}
		return static_cast<int32>(Side);
	}

	template <typename T>
	void HashPod(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	int32 CountSegments(const FInterpCurveVector& Curve)
	{
		const int32 NumPoints = Curve.Points.Num();
		if (NumPoints < 2)
		{
			return 0;
		}
		return Curve.bIsLooped ? NumPoints : NumPoints - 1;
	}

	/** Convex hull of the segment's Bezier control points — a bound the curve cannot leave. */
	FBox SegmentBounds(const FInterpCurveVector& Curve, int32 SegmentIndex)
	{
		const int32 NumPoints = Curve.Points.Num();
		const int32 LastPoint = NumPoints - 1;
		const bool bLoopSegment = Curve.bIsLooped && SegmentIndex == LastPoint;
		const int32 NextIndex = bLoopSegment ? 0 : SegmentIndex + 1;
		const double NextInVal = bLoopSegment
			? static_cast<double>(Curve.Points[LastPoint].InVal) + static_cast<double>(Curve.LoopKeyOffset)
			: static_cast<double>(Curve.Points[NextIndex].InVal);
		const double Diff = NextInVal - static_cast<double>(Curve.Points[SegmentIndex].InVal);

		const FVector Start = Curve.Points[SegmentIndex].OutVal;
		const FVector End = Curve.Points[NextIndex].OutVal;
		const FVector LeaveTangent = Curve.Points[SegmentIndex].LeaveTangent * Diff;
		const FVector ArriveTangent = Curve.Points[NextIndex].ArriveTangent * Diff;

		FBox Bounds(ForceInit);
		Bounds += Start;
		Bounds += End;
		Bounds += Start + LeaveTangent / 3.0;
		Bounds += End - ArriveTangent / 3.0;
		return Bounds;
	}

	double SquaredDistanceToBoxXY(const FBox& Bounds, double X, double Y)
	{
		const double DeltaX = FMath::Max3(Bounds.Min.X - X, 0.0, X - Bounds.Max.X);
		const double DeltaY = FMath::Max3(Bounds.Min.Y - Y, 0.0, Y - Bounds.Max.Y);
		return DeltaX * DeltaX + DeltaY * DeltaY;
	}

	struct FFieldEntry
	{
		FInterpCurveVector Curve;
		FInterpCurveVector CurveFlat;
		FInterpCurveFloat HalfWidthCurve;
		double HalfWidthCm = 0.0;
		double FalloffCm = 0.0;
		int32 PlaneIndex = INDEX_NONE;

		int32 NumSegments = 0;
		TArray<FBox> SegmentAABBs;
		TArray<double> SegmentStartKeys;
		TArray<double> SegmentEndKeys;
		TArray<int32> SegmentNextIndices;
		/** Widest half width reachable anywhere in the segment's key range, never the one at the
		 *  solved key: a key further along the same segment can be wider and still win. */
		TArray<double> SegmentMaxHalfWidths;
		FBox Bounds = FBox(ForceInit);
		double SolveRadiusSq = 0.0;
	};

	struct FEntryHit
	{
		double Objective = -TNumericLimits<double>::Max();
		double Distance = 0.0;
		double HalfWidth = 0.0;
		double Key = 0.0;
	};

	double EvalHalfWidth(const FFieldEntry& Entry, double Key)
	{
		return static_cast<double>(Entry.HalfWidthCurve.Eval(
			static_cast<float>(Key), static_cast<float>(Entry.HalfWidthCm)));
	}

	double MaxHalfWidthOverKeys(const FFieldEntry& Entry, double StartKey, double EndKey)
	{
		if (Entry.HalfWidthCurve.Points.Num() == 0)
		{
			return Entry.HalfWidthCm;
		}

		double Result = FMath::Max(EvalHalfWidth(Entry, StartKey), EvalHalfWidth(Entry, EndKey));
		for (const FInterpCurvePoint<float>& Point : Entry.HalfWidthCurve.Points)
		{
			const double PointKey = static_cast<double>(Point.InVal);
			if (PointKey > StartKey && PointKey < EndKey)
			{
				Result = FMath::Max(Result, static_cast<double>(Point.OutVal));
			}
		}
		return Result;
	}

	void ConsiderKey(const FFieldEntry& Entry, double Key, double PositionX, double PositionY,
		double PatchX, double PatchY, FEntryHit& InOutBest)
	{
		const double Distance = FVector2D(PositionX - PatchX, PositionY - PatchY).Size();
		const double HalfWidth = EvalHalfWidth(Entry, Key);
		const double Objective = HalfWidth - Distance;
		if (Objective > InOutBest.Objective)
		{
			InOutBest.Objective = Objective;
			InOutBest.Distance = Distance;
			InOutBest.HalfWidth = HalfWidth;
			InOutBest.Key = Key;
		}
	}

	void ConsiderSegment(const FFieldEntry& Entry, int32 SegmentIndex, const FVector& Probe,
		double PatchX, double PatchY, FEntryHit& InOutBest)
	{
		float SegmentDistanceSq = 0.0f;
		const float SegmentKey =
			Entry.CurveFlat.FindNearestOnSegment(Probe, SegmentIndex, SegmentDistanceSq);
		const FVector Nearest = Entry.CurveFlat.Eval(SegmentKey, FVector::ZeroVector);
		ConsiderKey(Entry, static_cast<double>(SegmentKey), Nearest.X, Nearest.Y, PatchX, PatchY,
			InOutBest);

		const FVector& Start = Entry.CurveFlat.Points[SegmentIndex].OutVal;
		const FVector& End = Entry.CurveFlat.Points[Entry.SegmentNextIndices[SegmentIndex]].OutVal;
		ConsiderKey(Entry, Entry.SegmentStartKeys[SegmentIndex], Start.X, Start.Y, PatchX, PatchY,
			InOutBest);
		ConsiderKey(Entry, Entry.SegmentEndKeys[SegmentIndex], End.X, End.Y, PatchX, PatchY, InOutBest);
	}

	void BuildFieldEntry(FFieldEntry& Entry, double SpacingX, double SpacingY)
	{
		Entry.NumSegments = CountSegments(Entry.CurveFlat);
		if (Entry.NumSegments <= 0)
		{
			return;
		}

		const int32 LastPoint = Entry.CurveFlat.Points.Num() - 1;

		Entry.SegmentAABBs.Reserve(Entry.NumSegments);
		Entry.SegmentStartKeys.Reserve(Entry.NumSegments);
		Entry.SegmentEndKeys.Reserve(Entry.NumSegments);
		Entry.SegmentNextIndices.Reserve(Entry.NumSegments);
		Entry.SegmentMaxHalfWidths.Reserve(Entry.NumSegments);

		double MaxHalfWidth = Entry.HalfWidthCm;
		if (Entry.HalfWidthCurve.Points.Num() > 0)
		{
			MaxHalfWidth = -TNumericLimits<double>::Max();
			for (const FInterpCurvePoint<float>& Point : Entry.HalfWidthCurve.Points)
			{
				MaxHalfWidth = FMath::Max(MaxHalfWidth, static_cast<double>(Point.OutVal));
			}
		}
		MaxHalfWidth = FMath::Max(MaxHalfWidth, 0.0);

		FBox EntryBounds(ForceInit);
		for (int32 SegmentIndex = 0; SegmentIndex < Entry.NumSegments; ++SegmentIndex)
		{
			const bool bLoopSegment = Entry.CurveFlat.bIsLooped && SegmentIndex == LastPoint;
			const int32 NextIndex = bLoopSegment ? 0 : SegmentIndex + 1;
			const double StartKey = static_cast<double>(Entry.CurveFlat.Points[SegmentIndex].InVal);
			const double EndKey = bLoopSegment
				? static_cast<double>(Entry.CurveFlat.Points[LastPoint].InVal)
					+ static_cast<double>(Entry.CurveFlat.LoopKeyOffset)
				: static_cast<double>(Entry.CurveFlat.Points[NextIndex].InVal);

			const FBox Bounds = SegmentBounds(Entry.CurveFlat, SegmentIndex);
			Entry.SegmentAABBs.Add(Bounds);
			Entry.SegmentStartKeys.Add(StartKey);
			Entry.SegmentEndKeys.Add(EndKey);
			Entry.SegmentNextIndices.Add(NextIndex);
			Entry.SegmentMaxHalfWidths.Add(MaxHalfWidthOverKeys(Entry, StartKey, EndKey));
			EntryBounds += Bounds;
		}

		// The bed is filled beyond the gradient because the height path reconstructs it bicubically: a
		// sample whose mask survives still pulls source texels up to two out, so the solve only stops
		// where no such kernel can reach.
		const double SolveRadius =
			MaxHalfWidth + Entry.FalloffCm + 5.0 * FMath::Max(SpacingX, SpacingY);
		Entry.SolveRadiusSq = SolveRadius * SolveRadius;
		Entry.Bounds = EntryBounds.ExpandBy(FVector(SolveRadius, SolveRadius, 0.0));
	}
}

struct FArcTerrainSplineFieldScratch
{
	TArray<ArcTerrainSplineModifier_Local::FFieldEntry> Entries;
	TArray<FName> PlaneChannels;
	FIntPoint Dims = FIntPoint::ZeroValue;
	FVector2D CoverageXY = FVector2D::ZeroVector;
	FGuid BedContentId;

	bool ProvideBed(TArray<float>& OutValues, FIntPoint& OutDims)
	{
		check(IsInGameThread());
		BuildFields();
		if (Bed.Num() != Dims.X * Dims.Y)
		{
			return false;
		}
		OutValues = Bed;
		OutDims = Dims;
		return true;
	}

	bool ProvideGradient(TArray<float>& OutValues, FIntPoint& OutDims)
	{
		check(IsInGameThread());
		BuildFields();
		if (Gradient.Num() != Dims.X * Dims.Y)
		{
			return false;
		}
		OutValues = Gradient;
		OutDims = Dims;
		return true;
	}

	bool ProvidePlane(int32 PlaneIndex, TArray<float>& OutValues, FIntPoint& OutDims)
	{
		check(IsInGameThread());
		BuildFields();
		if (!Planes.IsValidIndex(PlaneIndex) || Planes[PlaneIndex].Num() != Dims.X * Dims.Y)
		{
			return false;
		}
		OutValues = Planes[PlaneIndex];
		OutDims = Dims;
		return true;
	}

private:
	TArray<float> Bed;
	TArray<float> Gradient;
	TArray<TArray<float>> Planes;
	bool bBuilt = false;

	void BuildFields();
};

void FArcTerrainSplineFieldScratch::BuildFields()
{
	using namespace ArcTerrainSplineModifier_Local;

	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	if (Entries.Num() == 0 || Dims.X < 2 || Dims.Y < 2)
	{
		return;
	}

	const double SpacingX = CoverageXY.X / static_cast<double>(Dims.X - 1);
	const double SpacingY = CoverageXY.Y / static_cast<double>(Dims.Y - 1);

	int32 MaxSegments = 0;
	for (FFieldEntry& Entry : Entries)
	{
		BuildFieldEntry(Entry, SpacingX, SpacingY);
		MaxSegments = FMath::Max(MaxSegments, Entry.NumSegments);
	}
	if (MaxSegments <= 0)
	{
		return;
	}

	Bed.SetNumUninitialized(Dims.X * Dims.Y);
	Gradient.SetNumUninitialized(Dims.X * Dims.Y);
	Planes.SetNum(PlaneChannels.Num());
	for (TArray<float>& Plane : Planes)
	{
		Plane.Reset();
		Plane.SetNumZeroed(Dims.X * Dims.Y);
	}

	TArray<TArray<double>> RowContexts;
	ParallelForWithTaskContext(RowContexts, Dims.Y, [MaxSegments](int32, int32) -> TArray<double>
	{
		TArray<double> Buffer;
		Buffer.SetNumUninitialized(MaxSegments);
		return Buffer;
	},
	[this, SpacingX, SpacingY](TArray<double>& LowerBoundsSq, int32 Row)
	{
		const double PatchY = -CoverageXY.Y * 0.5 + static_cast<double>(Row) * SpacingY;

		for (int32 Col = 0; Col < Dims.X; ++Col)
		{
			const double PatchX = -CoverageXY.X * 0.5 + static_cast<double>(Col) * SpacingX;
			const int32 Index = Row * Dims.X + Col;
			const FVector Probe(PatchX, PatchY, 0.0);

			bool bAnySolve = false;
			double BestMask = 0.0;
			double BestObjective = 0.0;
			double BestZ = 0.0;
			int32 BestEntryIndex = INDEX_NONE;

			for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
			{
				const FFieldEntry& Entry = Entries[EntryIndex];
				if (Entry.NumSegments <= 0
					|| SquaredDistanceToBoxXY(Entry.Bounds, PatchX, PatchY) > 0.0)
				{
					continue;
				}

				double NearestBoundSq = TNumericLimits<double>::Max();
				double SeedUpperBound = -TNumericLimits<double>::Max();
				int32 SeedSegment = INDEX_NONE;
				for (int32 SegmentIndex = 0; SegmentIndex < Entry.NumSegments; ++SegmentIndex)
				{
					const double BoundSq =
						SquaredDistanceToBoxXY(Entry.SegmentAABBs[SegmentIndex], PatchX, PatchY);
					LowerBoundsSq[SegmentIndex] = BoundSq;
					NearestBoundSq = FMath::Min(NearestBoundSq, BoundSq);

					const double UpperBound =
						Entry.SegmentMaxHalfWidths[SegmentIndex] - FMath::Sqrt(BoundSq);
					if (UpperBound > SeedUpperBound)
					{
						SeedUpperBound = UpperBound;
						SeedSegment = SegmentIndex;
					}
				}

				if (SeedSegment == INDEX_NONE || NearestBoundSq > Entry.SolveRadiusSq)
				{
					continue;
				}

				FEntryHit Hit;
				ConsiderSegment(Entry, SeedSegment, Probe, PatchX, PatchY, Hit);
				for (int32 SegmentIndex = 0; SegmentIndex < Entry.NumSegments; ++SegmentIndex)
				{
					if (SegmentIndex == SeedSegment
						|| Entry.SegmentMaxHalfWidths[SegmentIndex]
							- FMath::Sqrt(LowerBoundsSq[SegmentIndex]) <= Hit.Objective)
					{
						continue;
					}
					ConsiderSegment(Entry, SegmentIndex, Probe, PatchX, PatchY, Hit);
				}

				const double Ramp = FMath::Clamp(
					(Hit.HalfWidth + Entry.FalloffCm - Hit.Distance) / FMath::Max(Entry.FalloffCm, 1.0),
					0.0, 1.0);
				const double Mask = Ramp * Ramp * (3.0 - 2.0 * Ramp);
				if (!bAnySolve || Mask > BestMask
					|| (Mask == BestMask && Hit.Objective > BestObjective))
				{
					bAnySolve = true;
					BestMask = Mask;
					BestObjective = Hit.Objective;
					BestZ = Entry.Curve.Eval(static_cast<float>(Hit.Key), FVector::ZeroVector).Z;
					BestEntryIndex = EntryIndex;
				}
			}

			if (!bAnySolve)
			{
				Bed[Index] = 0.0f;
				Gradient[Index] = 0.0f;
				continue;
			}

			// Stamp-local cm: the composite applies this component's Z scale and offset itself, so a
			// terrain-local Z here would count the offset twice.
			Bed[Index] = static_cast<float>(BestZ);
			Gradient[Index] = static_cast<float>(BestMask);

			// Only the winner's plane is written, so the texel's weights sum to at most one and the
			// terrain's max(sum, 1) normalize never attenuates the channels this stamp did not paint.
			const int32 PlaneIndex = Entries[BestEntryIndex].PlaneIndex;
			if (PlaneIndex != INDEX_NONE)
			{
				Planes[PlaneIndex][Index] = static_cast<float>(BestMask);
			}
		}
	});
}

namespace ArcTerrainSplineModifier_Local
{
	/** The fields are one rasterisation but a cache entry each, keyed on ContentId alone, so their
	 *  ids MUST differ: a shared id would hand one field another's texture. */
	FGuid MakeContentId(TConstArrayView<FArcTerrainSplineSnapshot> Snapshots, double TexelCm,
		const FIntPoint& Dims, const FVector2D& CoverageXY, FStringView FieldSalt)
	{
		static const FGuid FormatVersion(0x7C3D9A15, 0x4E6B47F2, 0x8B10D3A6, 0x5F27E0C4);

		FBlake3 Hash;
		HashPod(Hash, FormatVersion.A);
		HashPod(Hash, FormatVersion.B);
		HashPod(Hash, FormatVersion.C);
		HashPod(Hash, FormatVersion.D);

		const int32 NumEntries = Snapshots.Num();
		HashPod(Hash, NumEntries);
		for (const FArcTerrainSplineSnapshot& Snapshot : Snapshots)
		{
			const int32 NumPoints = Snapshot.Curve.Points.Num();
			HashPod(Hash, NumPoints);
			for (const FInterpCurvePoint<FVector>& Point : Snapshot.Curve.Points)
			{
				HashPod(Hash, Point.InVal);
				HashPod(Hash, Point.OutVal.X);
				HashPod(Hash, Point.OutVal.Y);
				HashPod(Hash, Point.OutVal.Z);
				HashPod(Hash, Point.ArriveTangent.X);
				HashPod(Hash, Point.ArriveTangent.Y);
				HashPod(Hash, Point.ArriveTangent.Z);
				HashPod(Hash, Point.LeaveTangent.X);
				HashPod(Hash, Point.LeaveTangent.Y);
				HashPod(Hash, Point.LeaveTangent.Z);
				const uint8 InterpMode = static_cast<uint8>(Point.InterpMode.GetValue());
				HashPod(Hash, InterpMode);
			}
			const uint8 bLooped = Snapshot.Curve.bIsLooped ? 1 : 0;
			HashPod(Hash, bLooped);
			HashPod(Hash, Snapshot.Curve.LoopKeyOffset);

			const int32 NumWidthPoints = Snapshot.HalfWidthCurve.Points.Num();
			HashPod(Hash, NumWidthPoints);
			for (const FInterpCurvePoint<float>& Point : Snapshot.HalfWidthCurve.Points)
			{
				HashPod(Hash, Point.InVal);
				HashPod(Hash, Point.OutVal);
				HashPod(Hash, Point.ArriveTangent);
				HashPod(Hash, Point.LeaveTangent);
				const uint8 InterpMode = static_cast<uint8>(Point.InterpMode.GetValue());
				HashPod(Hash, InterpMode);
			}

			HashPod(Hash, Snapshot.HalfWidthCm);
			HashPod(Hash, Snapshot.FalloffCm);

			const uint32 ChannelHash = ArcTerrain::ChannelNameHash(Snapshot.Channel);
			HashPod(Hash, ChannelHash);
		}

		HashPod(Hash, TexelCm);
		HashPod(Hash, Dims.X);
		HashPod(Hash, Dims.Y);
		HashPod(Hash, CoverageXY.X);
		HashPod(Hash, CoverageXY.Y);
		// The transform against the terrain stays out: the fields are a pure function of the curve in
		// THIS component's frame, and folding it in would re-key the whole footprint on every nudge.
		Hash.Update(FieldSalt.GetData(), FieldSalt.Len() * sizeof(TCHAR));

		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = { 0, 0, 0, 0 };
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}

	FString MakePlaneSalt(FName Channel)
	{
		return FString::Printf(TEXT("Plane:%s"), *Channel.ToString().ToLower());
	}

	void DeriveChannels(TConstArrayView<FArcTerrainSplineSnapshot> Snapshots,
		TArray<FName>& OutChannels)
	{
		OutChannels.Reset();
		for (const FArcTerrainSplineSnapshot& Snapshot : Snapshots)
		{
			if (!Snapshot.Channel.IsNone())
			{
				OutChannels.AddUnique(Snapshot.Channel);
			}
		}
	}

	double MaxFalloffCm(TConstArrayView<FArcTerrainSplineSnapshot> Snapshots)
	{
		double Result = 0.0;
		for (const FArcTerrainSplineSnapshot& Snapshot : Snapshots)
		{
			Result = FMath::Max(Result, static_cast<double>(Snapshot.FalloffCm));
		}
		return Result;
	}

	double ResolveTerrainTexelCm(const UArcTerrainStampComponent& Stamp)
	{
		const AArcTerrain* Terrain = Stamp.AffectedTerrain.Get();
		return Terrain != nullptr ? static_cast<double>(Terrain->Config.TexelSizeCm()) : 0.0;
	}

	/** The composite writes into terrain texels: a finer field buys no detail, a coarser one loses it. */
	void DeriveRasterGrid(double TerrainTexelCm, const FVector2D& CoverageXY, int32 NumPlanes,
		double& OutTexelCm, FIntPoint& OutDims, bool& bOutCapped)
	{
		const int32 MaxFieldSide = MaxFieldSideForPlanes(NumPlanes);
		double TexelCm = TerrainTexelCm;
		const double LargestSide = FMath::Max(CoverageXY.X, CoverageXY.Y);
		const double CappedTexelCm = LargestSide / static_cast<double>(MaxFieldSide - 1);
		bOutCapped = TexelCm < CappedTexelCm;
		if (bOutCapped)
		{
			TexelCm = CappedTexelCm;
		}

		OutTexelCm = TexelCm;
		OutDims = FIntPoint(
			FMath::Clamp(FMath::FloorToInt32(CoverageXY.X / TexelCm) + 1, 2, MaxFieldSide),
			FMath::Clamp(FMath::FloorToInt32(CoverageXY.Y / TexelCm) + 1, 2, MaxFieldSide));
	}

	bool DerivePatchCurve(const USplineComponent* SplineComponent,
		const FTransform& PatchToWorld, FInterpCurveVector& OutCurve)
	{
		OutCurve.Reset();
		OutCurve.bIsLooped = false;
		OutCurve.LoopKeyOffset = 0.0f;

		if (SplineComponent == nullptr)
		{
			return false;
		}

		const FSplineCurves Curves = SplineComponent->GetSplineCurves();
		if (Curves.Position.Points.Num() < 2)
		{
			return false;
		}

		const FTransform SplineToPatch =
			SplineComponent->GetComponentTransform().GetRelativeTransform(PatchToWorld);

		OutCurve.bIsLooped = Curves.Position.bIsLooped;
		OutCurve.LoopKeyOffset = Curves.Position.LoopKeyOffset;
		OutCurve.Points.Reserve(Curves.Position.Points.Num());
		for (const FInterpCurvePoint<FVector>& Point : Curves.Position.Points)
		{
			FInterpCurvePoint<FVector>& PatchPoint = OutCurve.Points.Add_GetRef(Point);
			PatchPoint.OutVal = SplineToPatch.TransformPosition(Point.OutVal);
			PatchPoint.ArriveTangent = SplineToPatch.TransformVector(Point.ArriveTangent);
			PatchPoint.LeaveTangent = SplineToPatch.TransformVector(Point.LeaveTangent);

			if (!FMath::IsFinite(PatchPoint.InVal) || PatchPoint.OutVal.ContainsNaN()
				|| PatchPoint.ArriveTangent.ContainsNaN() || PatchPoint.LeaveTangent.ContainsNaN())
			{
				OutCurve.Reset();
				OutCurve.bIsLooped = false;
				OutCurve.LoopKeyOffset = 0.0f;
				return false;
			}
		}
		return true;
	}

	bool BuildEntrySnapshot(const FArcTerrainSplineEntry& Entry, AActor* Owner,
		const FTransform& PatchToWorld, FArcTerrainSplineSnapshot& OutSnapshot)
	{
		OutSnapshot = FArcTerrainSplineSnapshot();

		if (!DerivePatchCurve(Cast<USplineComponent>(Entry.Spline.GetComponent(Owner)), PatchToWorld,
			OutSnapshot.Curve))
		{
			return false;
		}

		OutSnapshot.HalfWidthCm =
			FMath::IsFinite(Entry.HalfWidthCm) ? FMath::Max(Entry.HalfWidthCm, 0.0f) : 0.0f;
		OutSnapshot.FalloffCm =
			FMath::IsFinite(Entry.FalloffCm) ? FMath::Max(Entry.FalloffCm, 0.0f) : 0.0f;
		OutSnapshot.Channel = Entry.SurfaceChannel;

		OutSnapshot.HalfWidthCurve.Points.Reserve(Entry.HalfWidthCurve.Points.Num());
		for (const FInterpCurvePoint<float>& Point : Entry.HalfWidthCurve.Points)
		{
			if (!FMath::IsFinite(Point.InVal) || !FMath::IsFinite(Point.OutVal))
			{
				continue;
			}
			FInterpCurvePoint<float>& WidthPoint =
				OutSnapshot.HalfWidthCurve.Points.Add_GetRef(Point);
			WidthPoint.OutVal = FMath::Max(WidthPoint.OutVal, 0.0f);
			WidthPoint.InterpMode = CIM_Linear;
		}
		OutSnapshot.HalfWidthCurve.bIsLooped = false;
		OutSnapshot.HalfWidthCurve.LoopKeyOffset = 0.0f;

		if (OutSnapshot.Curve.bIsLooped && OutSnapshot.HalfWidthCurve.Points.Num() > 0)
		{
			const float ClosingKey =
				OutSnapshot.Curve.Points.Last().InVal + OutSnapshot.Curve.LoopKeyOffset;
			if (ClosingKey > OutSnapshot.HalfWidthCurve.Points.Last().InVal)
			{
				FInterpCurvePoint<float> ClosingPoint = OutSnapshot.HalfWidthCurve.Points[0];
				ClosingPoint.InVal = ClosingKey;
				ClosingPoint.InterpMode = CIM_Linear;
				OutSnapshot.HalfWidthCurve.Points.Add(ClosingPoint);
			}
		}
		return true;
	}

	void BuildSnapshots(TConstArrayView<FArcTerrainSplineEntry> Entries, AActor* Owner,
		const FTransform& PatchToWorld, TArray<FArcTerrainSplineSnapshot>& OutSnapshots)
	{
		OutSnapshots.Reset();
		OutSnapshots.Reserve(Entries.Num());
		for (const FArcTerrainSplineEntry& Entry : Entries)
		{
			FArcTerrainSplineSnapshot Snapshot;
			if (BuildEntrySnapshot(Entry, Owner, PatchToWorld, Snapshot))
			{
				OutSnapshots.Add(MoveTemp(Snapshot));
			}
		}
	}

#if WITH_EDITOR
	class FSplineDelegateBinding
	{
	public:
		FSplineDelegateBinding(UObject* InOwner, TFunction<void()> InCallback)
			: Owner(InOwner)
			, Callback(MoveTemp(InCallback))
		{
		}

		~FSplineDelegateBinding()
		{
			Unbind();
		}

		void Bind(TConstArrayView<USplineComponent*> InSplines)
		{
			Unbind();

			for (USplineComponent* const SplineComponent : InSplines)
			{
				if (SplineComponent == nullptr)
				{
					continue;
				}
				BoundSplines.Add(SplineComponent);

				PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
				OnSplineChangedHandles.Add(SplineComponent->GetOnSplineChanged().AddWeakLambda(Owner,
					[this]()
					{
						Callback();
					}));
				PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS

				OnTransformUpdatedHandles.Add(SplineComponent->TransformUpdated.AddWeakLambda(Owner,
					[this](USceneComponent*, EUpdateTransformFlags, ETeleportType)
					{
						Callback();
					}));
			}

			if (BoundSplines.Num() == 0)
			{
				return;
			}

			// Undo/redo restores the serialized curve without running UpdateSpline, so neither
			// OnSplineChanged nor TransformUpdated fires for it.
			OnTransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddWeakLambda(Owner,
				[this](UObject* Object, const FTransactionObjectEvent& Transaction)
				{
					if (Transaction.GetEventType() != ETransactionObjectEventType::UndoRedo)
					{
						return;
					}
					for (const TWeakObjectPtr<USplineComponent>& Bound : BoundSplines)
					{
						if (Bound.Get() == Object)
						{
							Callback();
							return;
						}
					}
				});
		}

		void Unbind()
		{
			if (OnTransactedHandle.IsValid())
			{
				FCoreUObjectDelegates::OnObjectTransacted.Remove(OnTransactedHandle);
				OnTransactedHandle.Reset();
			}

			for (int32 BoundIndex = 0; BoundIndex < BoundSplines.Num(); ++BoundIndex)
			{
				USplineComponent* const SplineComponent = BoundSplines[BoundIndex].Get();
				if (SplineComponent == nullptr)
				{
					continue;
				}
				if (OnSplineChangedHandles.IsValidIndex(BoundIndex)
					&& OnSplineChangedHandles[BoundIndex].IsValid())
				{
					PRAGMA_DISABLE_EXPERIMENTAL_WARNINGS
					SplineComponent->GetOnSplineChanged().Remove(OnSplineChangedHandles[BoundIndex]);
					PRAGMA_ENABLE_EXPERIMENTAL_WARNINGS
				}
				if (OnTransformUpdatedHandles.IsValidIndex(BoundIndex)
					&& OnTransformUpdatedHandles[BoundIndex].IsValid())
				{
					SplineComponent->TransformUpdated.Remove(OnTransformUpdatedHandles[BoundIndex]);
				}
			}

			OnSplineChangedHandles.Reset();
			OnTransformUpdatedHandles.Reset();
			BoundSplines.Reset();
		}

	private:
		UObject* Owner = nullptr;
		TFunction<void()> Callback;
		FDelegateHandle OnTransactedHandle;
		TArray<FDelegateHandle> OnSplineChangedHandles;
		TArray<FDelegateHandle> OnTransformUpdatedHandles;
		TArray<TWeakObjectPtr<USplineComponent>> BoundSplines;
	};

	bool IsSplineProperty(FName PropertyName, FName MemberName)
	{
		static const FName SplinesName = GET_MEMBER_NAME_CHECKED(UArcTerrainSplineModifier, Splines);
		static const FName EntrySplineName = GET_MEMBER_NAME_CHECKED(FArcTerrainSplineEntry, Spline);
		return PropertyName == SplinesName || MemberName == SplinesName
			|| PropertyName == EntrySplineName || MemberName == EntrySplineName;
	}

	bool IsSnapshotProperty(FName PropertyName, FName MemberName)
	{
		static const FName SnapshotNames[] = {
			GET_MEMBER_NAME_CHECKED(UArcTerrainSplineModifier, Splines),
			GET_MEMBER_NAME_CHECKED(FArcTerrainSplineEntry, Spline),
			GET_MEMBER_NAME_CHECKED(FArcTerrainSplineEntry, HalfWidthCm),
			GET_MEMBER_NAME_CHECKED(FArcTerrainSplineEntry, FalloffCm),
			GET_MEMBER_NAME_CHECKED(FArcTerrainSplineEntry, HalfWidthCurve),
			GET_MEMBER_NAME_CHECKED(FArcTerrainSplineEntry, SurfaceChannel),
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, CoverageXY),
			GET_MEMBER_NAME_CHECKED(UArcTerrainStampComponent, AffectedTerrain)
		};

		for (const FName& SnapshotName : SnapshotNames)
		{
			if (PropertyName == SnapshotName || MemberName == SnapshotName)
			{
				return true;
			}
		}
		return false;
	}

	void SampleCurvePolyline(const FInterpCurveVector& Curve, TArray<FVector>& OutPositions,
		TArray<FVector>& OutTangents, TArray<float>& OutKeys)
	{
		const int32 NumSegments = CountSegments(Curve);
		if (NumSegments <= 0)
		{
			return;
		}

		const int32 LastPoint = Curve.Points.Num() - 1;
		const float FirstKey = Curve.Points[0].InVal;
		const float LastKey = Curve.bIsLooped
			? Curve.Points[LastPoint].InVal + Curve.LoopKeyOffset
			: Curve.Points[LastPoint].InVal;
		const int32 NumSamples = FMath::Clamp(NumSegments * 12, 32, 1024);

		OutPositions.Reserve(NumSamples + 1);
		OutTangents.Reserve(NumSamples + 1);
		OutKeys.Reserve(NumSamples + 1);
		for (int32 SampleIndex = 0; SampleIndex <= NumSamples; ++SampleIndex)
		{
			const float Alpha = static_cast<float>(SampleIndex) / static_cast<float>(NumSamples);
			const float Key = FMath::Lerp(FirstKey, LastKey, Alpha);
			OutPositions.Add(Curve.Eval(Key, FVector::ZeroVector));
			OutTangents.Add(Curve.EvalDerivative(Key, FVector::ForwardVector));
			OutKeys.Add(Key);
		}
	}

	void DrawOffsetPolyline(FPrimitiveDrawInterface* PDI, const FMatrix& LocalToWorld,
		const TArray<FVector>& Positions, const TArray<FVector>& Tangents,
		TConstArrayView<double> Offsets, const FLinearColor& Color)
	{
		FVector PreviousLeft = FVector::ZeroVector;
		FVector PreviousRight = FVector::ZeroVector;
		for (int32 SampleIndex = 0; SampleIndex < Positions.Num(); ++SampleIndex)
		{
			FVector2D Normal(-Tangents[SampleIndex].Y, Tangents[SampleIndex].X);
			if (!Normal.Normalize())
			{
				Normal = FVector2D(0.0, 1.0);
			}
			const double Offset = Offsets[SampleIndex];
			const FVector Lateral(Normal.X * Offset, Normal.Y * Offset, 0.0);
			const FVector Left = LocalToWorld.TransformPosition(Positions[SampleIndex] + Lateral);
			const FVector Right = LocalToWorld.TransformPosition(Positions[SampleIndex] - Lateral);
			if (SampleIndex > 0)
			{
				PDI->DrawLine(PreviousLeft, Left, Color, SDPG_World);
				PDI->DrawLine(PreviousRight, Right, Color, SDPG_World);
			}
			PreviousLeft = Left;
			PreviousRight = Right;
		}
	}
#endif // WITH_EDITOR
}

UArcTerrainSplineModifier::UArcTerrainSplineModifier()
{
	BlendMode = EArcTerrainBlendMode::AlphaBlend;
}

void UArcTerrainSplineModifier::ResolveSplineComponents(TArray<USplineComponent*>& OutSplines) const
{
	OutSplines.Reset();
	OutSplines.Reserve(Splines.Num());

	AActor* const Owner = GetOwner();
	for (const FArcTerrainSplineEntry& Entry : Splines)
	{
		USplineComponent* const SplineComponent =
			Cast<USplineComponent>(Entry.Spline.GetComponent(Owner));
		if (SplineComponent != nullptr)
		{
			OutSplines.Add(SplineComponent);
		}
	}
}

void UArcTerrainSplineModifier::OnRegister()
{
	ConditionalUpdateComponentToWorld();

	TArray<USplineComponent*> ResolvedSplines;
	ResolveSplineComponents(ResolvedSplines);
	for (USplineComponent* const SplineComponent : ResolvedSplines)
	{
		SplineComponent->ConditionalUpdateComponentToWorld();
	}

	if (!IsSnapshotCurrent())
	{
		RebuildSnapshot();
	}

	Super::OnRegister();

#if WITH_EDITOR
	BindSplineDelegates();
#endif
}

void UArcTerrainSplineModifier::OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags,
	ETeleportType Teleport)
{
	if (!IsSnapshotCurrent())
	{
		RebuildSnapshot();
	}

	Super::OnUpdateTransform(UpdateTransformFlags, Teleport);
}

void UArcTerrainSplineModifier::OnAffectedTerrainConfigChanged()
{
	if (!IsSnapshotCurrent())
	{
		RebuildSnapshot();
	}
}

void UArcTerrainSplineModifier::OnUnregister()
{
#if WITH_EDITOR
	UnbindSplineDelegates();
#endif
	FieldScratch.Reset();

	Super::OnUnregister();
}

void UArcTerrainSplineModifier::OnComponentDestroyed(bool bDestroyingHierarchy)
{
#if WITH_EDITOR
	UnbindSplineDelegates();
#endif
	FieldScratch.Reset();

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UArcTerrainSplineModifier::RebuildSnapshot()
{
	using namespace ArcTerrainSplineModifier_Local;

	const double TerrainTexelCm = ResolveTerrainTexelCm(*this);
	if (TerrainTexelCm <= 0.0 || CoverageXY.X <= 0.0 || CoverageXY.Y <= 0.0)
	{
		return;
	}

	Modify();

	FieldScratch.Reset();
	SplineSnapshots.Reset();
	FieldDims = FIntPoint::ZeroValue;
	BedContentId.Invalidate();
	MaskContentId.Invalidate();
	PlaneChannels.Reset();
	PlaneContentIds.Reset();

	BuildSnapshots(Splines, GetOwner(), GetComponentTransform(), SplineSnapshots);
	if (SplineSnapshots.Num() == 0)
	{
		return;
	}

	DeriveChannels(SplineSnapshots, PlaneChannels);

	double DerivedTexelCm = 0.0;
	FIntPoint DerivedDims = FIntPoint::ZeroValue;
	bool bCapped = false;
	DeriveRasterGrid(TerrainTexelCm, CoverageXY, PlaneChannels.Num(), DerivedTexelCm, DerivedDims,
		bCapped);

	UE_CLOGF(bCapped, LogArcTerrain, Warning,
		"ArcTerrain: spline modifier '%ls' footprint %.0fx%.0f cm at the terrain's %.1f cm texel "
		"exceeds the %lld MB budget for its %d field planes; texel coarsened to %.1f cm "
		"(field %dx%d).",
		*GetPathName(), CoverageXY.X, CoverageXY.Y, TerrainTexelCm, MaxFieldBytes / (1024ll * 1024ll),
		PlaneChannels.Num() + 2, DerivedTexelCm, DerivedDims.X, DerivedDims.Y);

	FieldDims = DerivedDims;
	BedContentId = MakeContentId(SplineSnapshots, DerivedTexelCm, FieldDims, CoverageXY, TEXTVIEW("Bed"));
	MaskContentId = MakeContentId(SplineSnapshots, DerivedTexelCm, FieldDims, CoverageXY, TEXTVIEW("Mask"));

	PlaneContentIds.Reserve(PlaneChannels.Num());
	for (const FName& Channel : PlaneChannels)
	{
		const FString PlaneSalt = MakePlaneSalt(Channel);
		PlaneContentIds.Add(
			MakeContentId(SplineSnapshots, DerivedTexelCm, FieldDims, CoverageXY, PlaneSalt));
	}
}

void UArcTerrainSplineModifier::RefreshSplineSnapshot()
{
	if (!IsSnapshotCurrent())
	{
		RebuildSnapshot();
		NotifyStampChanged();
	}
}

bool UArcTerrainSplineModifier::IsSnapshotCurrent() const
{
	using namespace ArcTerrainSplineModifier_Local;

	if (SplineSnapshots.Num() == 0 || FieldDims.X < 2 || FieldDims.Y < 2
		|| !BedContentId.IsValid() || !MaskContentId.IsValid()
		|| PlaneContentIds.Num() != PlaneChannels.Num())
	{
		return false;
	}

	for (const FGuid& PlaneContentId : PlaneContentIds)
	{
		if (!PlaneContentId.IsValid())
		{
			return false;
		}
	}

	const double TerrainTexelCm = ResolveTerrainTexelCm(*this);
	if (TerrainTexelCm <= 0.0 || CoverageXY.X <= 0.0 || CoverageXY.Y <= 0.0)
	{
		return false;
	}

	TArray<FArcTerrainSplineSnapshot> DerivedSnapshots;
	BuildSnapshots(Splines, GetOwner(), GetComponentTransform(), DerivedSnapshots);
	if (DerivedSnapshots.Num() == 0)
	{
		return false;
	}

	TArray<FName> DerivedChannels;
	DeriveChannels(DerivedSnapshots, DerivedChannels);
	if (DerivedChannels != PlaneChannels)
	{
		return false;
	}

	double DerivedTexelCm = 0.0;
	FIntPoint DerivedDims = FIntPoint::ZeroValue;
	bool bCapped = false;
	DeriveRasterGrid(TerrainTexelCm, CoverageXY, DerivedChannels.Num(), DerivedTexelCm, DerivedDims,
		bCapped);
	if (DerivedDims != FieldDims)
	{
		return false;
	}

	return BedContentId
		== MakeContentId(DerivedSnapshots, DerivedTexelCm, FieldDims, CoverageXY, TEXTVIEW("Bed"));
}

TSharedRef<FArcTerrainSplineFieldScratch> UArcTerrainSplineModifier::AcquireScratch() const
{
	check(IsInGameThread());

	if (FieldScratch.IsValid() && FieldScratch->BedContentId == BedContentId)
	{
		return FieldScratch.ToSharedRef();
	}

	TSharedRef<FArcTerrainSplineFieldScratch> Scratch = MakeShared<FArcTerrainSplineFieldScratch>();
	Scratch->Entries.Reserve(SplineSnapshots.Num());
	for (const FArcTerrainSplineSnapshot& Snapshot : SplineSnapshots)
	{
		ArcTerrainSplineModifier_Local::FFieldEntry& Entry = Scratch->Entries.AddDefaulted_GetRef();
		Entry.Curve = Snapshot.Curve;
		Entry.CurveFlat = Snapshot.Curve;
		for (FInterpCurvePoint<FVector>& Point : Entry.CurveFlat.Points)
		{
			Point.OutVal.Z = 0.0;
			Point.ArriveTangent.Z = 0.0;
			Point.LeaveTangent.Z = 0.0;
		}
		Entry.HalfWidthCurve = Snapshot.HalfWidthCurve;
		Entry.HalfWidthCm = static_cast<double>(Snapshot.HalfWidthCm);
		Entry.FalloffCm = static_cast<double>(Snapshot.FalloffCm);
		Entry.PlaneIndex = Snapshot.Channel.IsNone()
			? INDEX_NONE
			: PlaneChannels.IndexOfByKey(Snapshot.Channel);
	}
	Scratch->PlaneChannels = PlaneChannels;
	Scratch->Dims = FieldDims;
	Scratch->CoverageXY = CoverageXY;
	Scratch->BedContentId = BedContentId;
	FieldScratch = Scratch;
	return Scratch;
}

bool UArcTerrainSplineModifier::GetSourceDescriptor(FArcStampSourceDesc& OutDesc) const
{
	if (SplineSnapshots.Num() == 0 || FieldDims.X < 2 || FieldDims.Y < 2
		|| !BedContentId.IsValid() || !MaskContentId.IsValid()
		|| PlaneContentIds.Num() != PlaneChannels.Num())
	{
		return false;
	}

	const TSharedRef<FArcTerrainSplineFieldScratch> Scratch = AcquireScratch();

	OutDesc.Kind = EArcStampSourceKind::BakedField;
	OutDesc.ContentId = BedContentId;
	OutDesc.SourceDims = FieldDims;
	OutDesc.SourceChannel = EArcTerrainSourceChannel::R;
	OutDesc.EncodingScale = 1.0f;
	OutDesc.ZeroInEncoding = 0.0f;
	OutDesc.bDeriveZeroFromBorder = false;
	OutDesc.BakedProvider = [Scratch](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
	{
		return Scratch->ProvideBed(OutValues, OutDims);
	};

	OutDesc.MaskKind = EArcStampSourceKind::BakedField;
	OutDesc.MaskContentId = MaskContentId;
	OutDesc.MaskSourceDims = FieldDims;
	OutDesc.MaskSourceChannel = EArcTerrainSourceChannel::R;
	OutDesc.MaskBakedProvider = [Scratch](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
	{
		return Scratch->ProvideGradient(OutValues, OutDims);
	};

	for (int32 PlaneIndex = 0; PlaneIndex < PlaneChannels.Num(); ++PlaneIndex)
	{
		FArcStampLayerDesc Layer;
		Layer.Kind = EArcStampSourceKind::BakedField;
		Layer.ContentId = PlaneContentIds[PlaneIndex];
		Layer.SourceChannel = EArcTerrainSourceChannel::R;
		Layer.EncodingScale = 1.0f;
		Layer.ZeroInEncoding = 0.0f;
		Layer.bContributeToHeight = false;
		Layer.BakedProvider = [Scratch, PlaneIndex](TArray<float>& OutValues, FIntPoint& OutDims) -> bool
			{ return Scratch->ProvidePlane(PlaneIndex, OutValues, OutDims); };
		// SourceDims stays zero: a non-zero value feeds Max3 in ResolveStampMaxWeightSourceDim
		// and would raise the shared weight-tile resolution of the whole terrain.
		OutDesc.Layers.Emplace(PlaneChannels[PlaneIndex], MoveTemp(Layer));
	}

	OutDesc.EdgeFalloffBand = static_cast<float>(
		ArcTerrainSplineModifier_Local::MaxFalloffCm(SplineSnapshots));
	OutDesc.EdgeSharpness = 1.0f;
	OutDesc.bCircleMask = false;
	return true;
}

#if WITH_EDITOR
void UArcTerrainSplineModifier::BindSplineDelegates()
{
	if (!SplineDelegates.IsValid())
	{
		SplineDelegates = MakePimpl<ArcTerrainSplineModifier_Local::FSplineDelegateBinding>(this,
			[this]()
			{
				HandleSplineEdited();
			});
	}

	TArray<USplineComponent*> ResolvedSplines;
	ResolveSplineComponents(ResolvedSplines);
	SplineDelegates->Bind(ResolvedSplines);
}

void UArcTerrainSplineModifier::UnbindSplineDelegates()
{
	if (SplineDelegates.IsValid())
	{
		SplineDelegates->Unbind();
	}
}

void UArcTerrainSplineModifier::HandleSplineEdited()
{
	RefreshSplineSnapshot();
}

void UArcTerrainSplineModifier::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property != nullptr
		&& PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		const FName PropertyName = PropertyChangedEvent.GetPropertyName();
		const FName MemberName = PropertyChangedEvent.GetMemberPropertyName();

		if (ArcTerrainSplineModifier_Local::IsSnapshotProperty(PropertyName, MemberName))
		{
			RebuildSnapshot();
		}

		if (ArcTerrainSplineModifier_Local::IsSplineProperty(PropertyName, MemberName))
		{
			BindSplineDelegates();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UArcTerrainSplineModifier::PostEditUndo()
{
	Super::PostEditUndo();

	if (IsRegistered() && IsValidChecked(this))
	{
		BindSplineDelegates();
	}
	else
	{
		UnbindSplineDelegates();
	}
}

void UArcTerrainSplineModifier::PostEditComponentMove(bool bFinished)
{
	if (!IsSnapshotCurrent())
	{
		RebuildSnapshot();
	}

	Super::PostEditComponentMove(bFinished);
}

void UArcTerrainSplineModifier::DrawStampVisualization(const FSceneView* View,
	FPrimitiveDrawInterface* PDI) const
{
	using namespace ArcTerrainSplineModifier_Local;

	const AArcTerrain* Terrain = AffectedTerrain.Get();
	if (Terrain == nullptr)
	{
		return;
	}

	const FBox TerrainLocalBounds = ComputeBounds();
	if (TerrainLocalBounds.IsValid)
	{
		const FMatrix TerrainToWorld = Terrain->GetTerrainTransform().ToMatrixWithScale();
		DrawWireBox(PDI, TerrainToWorld, TerrainLocalBounds, FLinearColor::Green, SDPG_World);
	}

	const FMatrix LocalToWorld = GetComponentTransform().ToMatrixWithScale();
	for (const FArcTerrainSplineSnapshot& Snapshot : SplineSnapshots)
	{
		TArray<FVector> Positions;
		TArray<FVector> Tangents;
		TArray<float> Keys;
		SampleCurvePolyline(Snapshot.Curve, Positions, Tangents, Keys);
		if (Positions.Num() < 2)
		{
			continue;
		}

		FVector PreviousCenter = LocalToWorld.TransformPosition(Positions[0]);
		for (int32 SampleIndex = 1; SampleIndex < Positions.Num(); ++SampleIndex)
		{
			const FVector Center = LocalToWorld.TransformPosition(Positions[SampleIndex]);
			PDI->DrawLine(PreviousCenter, Center, FLinearColor::White, SDPG_World);
			PreviousCenter = Center;
		}

		TArray<double> CoreOffsets;
		TArray<double> BandOffsets;
		CoreOffsets.Reserve(Keys.Num());
		BandOffsets.Reserve(Keys.Num());
		for (const float Key : Keys)
		{
			const double HalfWidth =
				static_cast<double>(Snapshot.HalfWidthCurve.Eval(Key, Snapshot.HalfWidthCm));
			CoreOffsets.Add(HalfWidth);
			BandOffsets.Add(HalfWidth + static_cast<double>(Snapshot.FalloffCm));
		}

		DrawOffsetPolyline(PDI, LocalToWorld, Positions, Tangents, CoreOffsets, FLinearColor::Yellow);
		DrawOffsetPolyline(PDI, LocalToWorld, Positions, Tangents, BandOffsets,
			FLinearColor(1.0f, 0.5f, 0.0f));
	}
}
#endif // WITH_EDITOR
