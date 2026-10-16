// Copyright Lukasz Baran. All Rights Reserved.

#include "Extraction/ArcRiverGeoreference.h"

#include "Algo/Reverse.h"
#include "ArcRiverModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileAccess.h"
#include "ArcTerrainTypes.h"
#include "Stamps/ArcTerrainTexturePatchModifier.h"
#include "Surface/ArcTerrainSurfaceSample.h"

#define LOCTEXT_NAMESPACE "ArcRiverGeoreference"

namespace ArcRiverGeoreferencePrivate
{
	constexpr double GMaxRelativeTiltDeg = 0.5;
	constexpr double MaxTerrainSampleStepCm = 2500.0; // 25 m

	struct FWorkChain
	{
		TArray<FVector2D> TerrainLocalXY;
		TArray<FVector> WorldPoints;
		TArray<double> GroundZ;
		TArray<float> HalfWidthCm;
		TArray<ArcRiverNetworkExtraction::FArcRiverJunctionRef> Junctions;
	};

	struct FJunctionEnd
	{
		int32 ChainIndex = INDEX_NONE;
		int32 PointIndex = INDEX_NONE;
	};

	struct FPointRef
	{
		int32 ChainIndex = INDEX_NONE;
		int32 PointIndex = INDEX_NONE;
		float FracX = 0.0f;
		float FracY = 0.0f;
	};

	FVector2D TexelToComponentLocalCm(const FVector2f& TexelXY, const FIntPoint Dims, const FVector2D& CoverageXY)
	{
		const double SpanX = static_cast<double>(FMath::Max(Dims.X - 1, 1));
		const double SpanY = static_cast<double>(FMath::Max(Dims.Y - 1, 1));
		const double FootprintU = static_cast<double>(TexelXY.X) / SpanX;
		const double FootprintV = static_cast<double>(TexelXY.Y) / SpanY;
		return FVector2D((FootprintU - 0.5) * CoverageXY.X, (FootprintV - 0.5) * CoverageXY.Y);
	}

	void ReverseChain(FWorkChain& Chain)
	{
		Algo::Reverse(Chain.TerrainLocalXY);
		Algo::Reverse(Chain.WorldPoints);
		Algo::Reverse(Chain.GroundZ);
		Algo::Reverse(Chain.HalfWidthCm);

		const int32 LastPoint = Chain.TerrainLocalXY.Num() - 1;
		for (ArcRiverNetworkExtraction::FArcRiverJunctionRef& Junction : Chain.Junctions)
		{
			Junction.PointIndex = LastPoint - Junction.PointIndex;
		}
	}

	void DensifyWorkChain(FWorkChain& Chain, const double MaxStepCm)
	{
		const int32 NumOriginal = Chain.TerrainLocalXY.Num();
		if (NumOriginal < 2 || Chain.HalfWidthCm.Num() != NumOriginal || !(MaxStepCm > 0.0))
		{
			return;
		}

		TArray<FVector2D> NewXY;
		TArray<float> NewHalfWidthCm;
		TArray<int32> OldToNew;
		NewXY.Reserve(NumOriginal);
		NewHalfWidthCm.Reserve(NumOriginal);
		OldToNew.SetNumUninitialized(NumOriginal);

		NewXY.Add(Chain.TerrainLocalXY[0]);
		NewHalfWidthCm.Add(Chain.HalfWidthCm[0]);
		OldToNew[0] = 0;

		for (int32 PointIndex = 1; PointIndex < NumOriginal; ++PointIndex)
		{
			const FVector2D& Prev = Chain.TerrainLocalXY[PointIndex - 1];
			const FVector2D& Curr = Chain.TerrainLocalXY[PointIndex];
			const double Len = (Curr - Prev).Size();
			if (Len > MaxStepCm)
			{
				const int32 Steps = FMath::Max(1, FMath::CeilToInt32(Len / MaxStepCm));
				const float PrevWidth = Chain.HalfWidthCm[PointIndex - 1];
				const float CurrWidth = Chain.HalfWidthCm[PointIndex];
				for (int32 Step = 1; Step < Steps; ++Step)
				{
					const double Alpha = static_cast<double>(Step) / static_cast<double>(Steps);
					NewXY.Add(FMath::Lerp(Prev, Curr, Alpha));
					NewHalfWidthCm.Add(FMath::Lerp(PrevWidth, CurrWidth, static_cast<float>(Alpha)));
				}
			}

			OldToNew[PointIndex] = NewXY.Num();
			NewXY.Add(Curr);
			NewHalfWidthCm.Add(Chain.HalfWidthCm[PointIndex]);
		}

		for (ArcRiverNetworkExtraction::FArcRiverJunctionRef& Junction : Chain.Junctions)
		{
			if (OldToNew.IsValidIndex(Junction.PointIndex))
			{
				Junction.PointIndex = OldToNew[Junction.PointIndex];
			}
		}

		Chain.TerrainLocalXY = MoveTemp(NewXY);
		Chain.HalfWidthCm = MoveTemp(NewHalfWidthCm);
	}

	// The Z solve runs on the whole network, not per chain: a running minimum taken per chain gives a
	// confluence texel one value on the tributary and another on the main stem, stepping the bed and
	// tearing a waterfall seam between the two Water splines.
	bool SolveDownstreamMinimum(
		TArray<TArray<double>>& Values,
		const TArray<TArray<FJunctionEnd>>& JunctionIncidence,
		const int32 MaxPasses)
	{
		for (int32 Pass = 0; Pass < MaxPasses; ++Pass)
		{
			bool bChanged = false;

			for (TArray<double>& ChainValues : Values)
			{
				for (int32 PointIndex = 1; PointIndex < ChainValues.Num(); ++PointIndex)
				{
					if (ChainValues[PointIndex] > ChainValues[PointIndex - 1])
					{
						ChainValues[PointIndex] = ChainValues[PointIndex - 1];
						bChanged = true;
					}
				}
			}

			for (const TArray<FJunctionEnd>& Ends : JunctionIncidence)
			{
				if (Ends.Num() < 2)
				{
					continue;
				}

				double Lowest = TNumericLimits<double>::Max();
				for (const FJunctionEnd& End : Ends)
				{
					Lowest = FMath::Min(Lowest, Values[End.ChainIndex][End.PointIndex]);
				}
				for (const FJunctionEnd& End : Ends)
				{
					double& Value = Values[End.ChainIndex][End.PointIndex];
					if (Value > Lowest)
					{
						Value = Lowest;
						bChanged = true;
					}
				}
			}

			if (!bChanged)
			{
				return true;
			}
		}

		return false;
	}

	bool SampleGroundZ(
		const AArcTerrain& Terrain,
		TArray<FWorkChain>& Chains,
		TArray<TArray<double>>& OutSurfaceZ,
		FText& OutError)
	{
		const FArcTerrainConfig& Config = Terrain.Config;
		const double CellSize = Config.CellSize();
		if (!(CellSize > 0.0))
		{
			OutError = LOCTEXT("BadCellSize", "The terrain config resolves a non-positive cell size.");
			return false;
		}

		const double TexelSize = static_cast<double>(Config.TexelSizeCm());
		if (!(TexelSize > 0.0))
		{
			OutError = LOCTEXT("BadTexelSize", "The terrain config resolves a non-positive texel size.");
			return false;
		}

		UWorld* World = Terrain.GetWorld();
		UArcTerrainSubsystem* TerrainSubsystem =
			World != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
		if (TerrainSubsystem == nullptr)
		{
			OutError = LOCTEXT("NoTerrainSubsystem",
				"The terrain has no live ArcTerrain subsystem, so the river network cannot be placed.");
			return false;
		}

		const FTransform TerrainTransform = Terrain.GetTerrainTransform();

		TArray<FArcTerrainCellKey> CellKeys;
		TArray<TArray<FPointRef>> CellPoints;
		TMap<FArcTerrainCellKey, int32> CellKeyToIndex;

		for (int32 ChainIndex = 0; ChainIndex < Chains.Num(); ++ChainIndex)
		{
			const FWorkChain& Chain = Chains[ChainIndex];
			for (int32 PointIndex = 0; PointIndex < Chain.TerrainLocalXY.Num(); ++PointIndex)
			{
				const FVector2D& LocalXY = Chain.TerrainLocalXY[PointIndex];
				const FArcTerrainCellKey CellKey =
					ArcTerrain::CellKeyFromLocal(FVector(LocalXY.X, LocalXY.Y, 0.0), Config);

				int32* ExistingIndex = CellKeyToIndex.Find(CellKey);
				if (ExistingIndex == nullptr)
				{
					const int32 NewIndex = CellKeys.Add(CellKey);
					CellPoints.AddDefaulted();
					ExistingIndex = &CellKeyToIndex.Add(CellKey, NewIndex);
				}

				FPointRef& Ref = CellPoints[*ExistingIndex].AddDefaulted_GetRef();
				Ref.ChainIndex = ChainIndex;
				Ref.PointIndex = PointIndex;
				Ref.FracX = static_cast<float>(FMath::Clamp(
					(LocalXY.X - static_cast<double>(CellKey.X) * CellSize) / CellSize, 0.0, 1.0));
				Ref.FracY = static_cast<float>(FMath::Clamp(
					(LocalXY.Y - static_cast<double>(CellKey.Y) * CellSize) / CellSize, 0.0, 1.0));
			}
		}

		OutSurfaceZ.Reset();
		OutSurfaceZ.SetNum(Chains.Num());
		for (int32 ChainIndex = 0; ChainIndex < Chains.Num(); ++ChainIndex)
		{
			OutSurfaceZ[ChainIndex].SetNumZeroed(Chains[ChainIndex].TerrainLocalXY.Num());
			Chains[ChainIndex].WorldPoints.SetNumZeroed(Chains[ChainIndex].TerrainLocalXY.Num());
			Chains[ChainIndex].GroundZ.SetNumZeroed(Chains[ChainIndex].TerrainLocalXY.Num());
		}

		TArray<float> Heights;
		bool bAnyBakedFallback = false;
		for (int32 CellIndex = 0; CellIndex < CellKeys.Num(); ++CellIndex)
		{
			const FArcTerrainCellKey CellKey = CellKeys[CellIndex];
			const TArray<FPointRef>& Points = CellPoints[CellIndex];

			FIntPoint MinTexel(TNumericLimits<int32>::Max(), TNumericLimits<int32>::Max());
			FIntPoint MaxTexel(TNumericLimits<int32>::Lowest(), TNumericLimits<int32>::Lowest());
			for (const FPointRef& Ref : Points)
			{
				const FVector2D& LocalXY = Chains[Ref.ChainIndex].TerrainLocalXY[Ref.PointIndex];
				MinTexel.X = FMath::Min(MinTexel.X, FMath::FloorToInt32(LocalXY.X / TexelSize));
				MinTexel.Y = FMath::Min(MinTexel.Y, FMath::FloorToInt32(LocalXY.Y / TexelSize));
				MaxTexel.X = FMath::Max(MaxTexel.X, FMath::CeilToInt32(LocalXY.X / TexelSize));
				MaxTexel.Y = FMath::Max(MaxTexel.Y, FMath::CeilToInt32(LocalXY.Y / TexelSize));
			}
			if (MaxTexel.X <= MinTexel.X)
			{
				MaxTexel.X = MinTexel.X + 1;
			}
			if (MaxTexel.Y <= MinTexel.Y)
			{
				MaxTexel.Y = MinTexel.Y + 1;
			}

			FIntPoint OutDims = FIntPoint::ZeroValue;
			bool bOutComplete = false;
			const bool bLiveOk = TerrainSubsystem->SampleTerrainHeightTexels(
				Terrain, MinTexel, MaxTexel, Heights, OutDims, bOutComplete);

			if (bLiveOk && bOutComplete)
			{
				const int32 MaxCoordX = OutDims.X - 2;
				const int32 MaxCoordY = OutDims.Y - 2;
				const int32 Row = OutDims.X;
				for (const FPointRef& Ref : Points)
				{
					const FVector2D& LocalXY = Chains[Ref.ChainIndex].TerrainLocalXY[Ref.PointIndex];
					const double Tx = LocalXY.X / TexelSize - static_cast<double>(MinTexel.X);
					const double Ty = LocalXY.Y / TexelSize - static_cast<double>(MinTexel.Y);
					const int32 Ix = FMath::Clamp(FMath::FloorToInt32(Tx), 0, MaxCoordX);
					const int32 Iy = FMath::Clamp(FMath::FloorToInt32(Ty), 0, MaxCoordY);
					const float Fx = static_cast<float>(Tx - static_cast<double>(Ix));
					const float Fy = static_cast<float>(Ty - static_cast<double>(Iy));
					const float Z00 = Heights[Iy * Row + Ix];
					const float Z10 = Heights[Iy * Row + Ix + 1];
					const float Z01 = Heights[(Iy + 1) * Row + Ix];
					const float Z11 = Heights[(Iy + 1) * Row + Ix + 1];
					const float GroundLocalZ = FMath::Lerp(
						FMath::Lerp(Z00, Z10, Fx), FMath::Lerp(Z01, Z11, Fx), Fy);

					const FVector GroundWorld = TerrainTransform.TransformPosition(
						FVector(LocalXY.X, LocalXY.Y, static_cast<double>(GroundLocalZ)));

					Chains[Ref.ChainIndex].WorldPoints[Ref.PointIndex] = GroundWorld;
					Chains[Ref.ChainIndex].GroundZ[Ref.PointIndex] = GroundWorld.Z;
					OutSurfaceZ[Ref.ChainIndex][Ref.PointIndex] = GroundWorld.Z;
				}
			}
			else
			{
				bAnyBakedFallback = true;
				int32 CellResolution = 0;
				if (!ArcTerrainTileAccess::LoadCellHeightsSync(Terrain, CellKey, Heights, CellResolution)
					|| CellResolution < 4)
				{
					OutError = FText::Format(
						LOCTEXT("NoBakedHeight",
							"ArcTerrain cell {0} has no readable baked height, so the river network cannot be placed. "
							"Bake the terrain height over the whole river footprint and generate again."),
						FText::FromString(FString::Printf(TEXT("(%d, %d)"), CellKey.X, CellKey.Y)));
					return false;
				}

				// LoadCellHeightsSync yields the apron-inclusive side of a (Quads+3)^2 tile; BilinearApron
				// wants the quad count.
				const int32 CellQuads = CellResolution - 3;

				for (const FPointRef& Ref : Points)
				{
					const float GroundLocalZ =
						ArcTerrainSurface::BilinearApron(Heights, CellQuads, Ref.FracX, Ref.FracY);

					const FVector2D& LocalXY = Chains[Ref.ChainIndex].TerrainLocalXY[Ref.PointIndex];
					const FVector GroundWorld = TerrainTransform.TransformPosition(
						FVector(LocalXY.X, LocalXY.Y, static_cast<double>(GroundLocalZ)));

					Chains[Ref.ChainIndex].WorldPoints[Ref.PointIndex] = GroundWorld;
					Chains[Ref.ChainIndex].GroundZ[Ref.PointIndex] = GroundWorld.Z;
					OutSurfaceZ[Ref.ChainIndex][Ref.PointIndex] = GroundWorld.Z;
				}
			}
		}

		if (bAnyBakedFallback)
		{
			UE_LOGF(LogArcRiver, Warning,"River ground Z used baked height for one or more cells because the live terrain "
					 "composite was incomplete or unavailable.");
		}

		return true;
	}
}

namespace ArcRiverGeoreference
{
	double ComputeCmPerTexel(const UArcTerrainTexturePatchModifier& Modifier, const FIntPoint MaskDims)
	{
		if (MaskDims.X <= 1 || MaskDims.Y <= 1)
		{
			return 0.0;
		}

		// The texel-to-world path pushes the footprint through the component transform, so the scale
		// that widens the river's positions has to widen the texel it measures its width in too.
		const FVector WorldScale = Modifier.GetComponentTransform().GetScale3D();
		const FVector2D FootprintCm(
			Modifier.CoverageXY.X * FMath::Abs(WorldScale.X),
			Modifier.CoverageXY.Y * FMath::Abs(WorldScale.Y));

		return FMath::Min(
			FootprintCm.X / static_cast<double>(MaskDims.X - 1),
			FootprintCm.Y / static_cast<double>(MaskDims.Y - 1));
	}

	bool BuildPolylines(
		TConstArrayView<ArcRiverNetworkExtraction::FArcRiverChain> Chains,
		TConstArrayView<FIntPoint> JunctionTexels,
		const FGeoreferenceInput& Input,
		TArray<FArcRiverPolyline>& OutPolylines,
		FText& OutError)
	{
		using namespace ArcRiverGeoreferencePrivate;

		OutPolylines.Reset();

		if (Input.Modifier == nullptr || Input.Terrain == nullptr)
		{
			OutError = LOCTEXT("MissingInputs",
				"River georeferencing needs both the source texture patch modifier and the terrain.");
			return false;
		}
		if (Input.MaskDims.X <= 1 || Input.MaskDims.Y <= 1)
		{
			OutError = LOCTEXT("BadMaskDims", "The river mask must be at least 2 texels on each side.");
			return false;
		}
		const double CmPerTexel = ComputeCmPerTexel(*Input.Modifier, Input.MaskDims);
		if (!(CmPerTexel > 0.0) || !(Input.WidthToDepthRatio > 0.0f))
		{
			OutError = LOCTEXT("BadScale",
				"River georeferencing needs a positive texel size and width-to-depth ratio.");
			return false;
		}

		const FTransform ModifierTransform = Input.Modifier->GetComponentTransform();
		const FRotator RelativeRotation =
			ModifierTransform.GetRelativeTransform(Input.Terrain->GetTerrainTransform()).Rotator();
		if (FMath::Abs(RelativeRotation.Pitch) > GMaxRelativeTiltDeg
			|| FMath::Abs(RelativeRotation.Roll) > GMaxRelativeTiltDeg)
		{
			OutError = FText::Format(
				LOCTEXT("TiltedModifier",
					"The Rivers mask modifier is pitched {0} and rolled {1} degrees relative to the terrain, past the "
					"{2} degrees allowed. The terrain compositor drops Z when it projects the mask, so a tilted patch "
					"carves the channel where the mask never marked one. Level the modifier and generate again."),
				FText::AsNumber(RelativeRotation.Pitch),
				FText::AsNumber(RelativeRotation.Roll),
				FText::AsNumber(GMaxRelativeTiltDeg));
			return false;
		}
		const FVector2D CoverageXY = Input.Modifier->CoverageXY;

		TArray<FWorkChain> WorkChains;
		WorkChains.Reserve(Chains.Num());

		for (const ArcRiverNetworkExtraction::FArcRiverChain& Chain : Chains)
		{
			const int32 NumPoints = Chain.TexelXY.Num();
			if (NumPoints < 2 || Chain.HalfWidthTexels.Num() != NumPoints)
			{
				OutError = LOCTEXT("MalformedChain",
					"The extracted river network holds a chain with mismatched point or width arrays.");
				return false;
			}

			FWorkChain Work;
			Work.TerrainLocalXY.Reserve(NumPoints);
			Work.HalfWidthCm.Reserve(NumPoints);
			Work.Junctions = Chain.Junctions;

			for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
			{
				const FVector2D ComponentLocalCm =
					TexelToComponentLocalCm(Chain.TexelXY[PointIndex], Input.MaskDims, CoverageXY);
				const FVector WorldFlat =
					ModifierTransform.TransformPosition(FVector(ComponentLocalCm.X, ComponentLocalCm.Y, 0.0));
				const FVector TerrainLocal = Input.Terrain->WorldToTerrainLocal(WorldFlat);
				Work.TerrainLocalXY.Emplace(TerrainLocal.X, TerrainLocal.Y);

				const float HalfWidthCm =
					Chain.HalfWidthTexels[PointIndex] * static_cast<float>(CmPerTexel);
				Work.HalfWidthCm.Add(HalfWidthCm);
			}

			WorkChains.Add(MoveTemp(Work));
		}

		for (FWorkChain& WorkChain : WorkChains)
		{
			DensifyWorkChain(WorkChain, MaxTerrainSampleStepCm);
		}

		TArray<TArray<double>> SurfaceZ;
		if (!SampleGroundZ(*Input.Terrain, WorkChains, SurfaceZ, OutError))
		{
			return false;
		}

		int32 UnorientableChains = 0;
		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			TArray<double>& ChainZ = SurfaceZ[ChainIndex];
			const double Drop = ChainZ[0] - ChainZ.Last();
			if (Drop < 0.0)
			{
				ReverseChain(WorkChains[ChainIndex]);
				Algo::Reverse(ChainZ);
			}
			else if (Drop == 0.0)
			{
				++UnorientableChains;
			}
		}
		UE_CLOGF(UnorientableChains > 0, LogArcRiver, Log,"%d river chain(s) end at exactly their starting height; keeping their scan order.",
			UnorientableChains);

		TArray<TArray<FJunctionEnd>> JunctionIncidence;
		JunctionIncidence.SetNum(JunctionTexels.Num());
		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			const FWorkChain& Chain = WorkChains[ChainIndex];
			for (const ArcRiverNetworkExtraction::FArcRiverJunctionRef& Junction : Chain.Junctions)
			{
				if (!JunctionIncidence.IsValidIndex(Junction.JunctionId))
				{
					OutError = LOCTEXT("BadJunctionId",
						"The extracted river network names a junction that is not in the junction table.");
					return false;
				}
				if (!Chain.TerrainLocalXY.IsValidIndex(Junction.PointIndex))
				{
					OutError = LOCTEXT("BadJunctionPoint",
						"The extracted river network places a junction outside the points of its own chain.");
					return false;
				}

				FJunctionEnd& End = JunctionIncidence[Junction.JunctionId].AddDefaulted_GetRef();
				End.ChainIndex = ChainIndex;
				End.PointIndex = Junction.PointIndex;
			}
		}

		const int32 MaxPasses = WorkChains.Num() + JunctionTexels.Num() + 1;
		if (!SolveDownstreamMinimum(SurfaceZ, JunctionIncidence, MaxPasses))
		{
			OutError = LOCTEXT("SurfaceNotConverged",
				"The river surface height did not settle; the extracted junction table is inconsistent.");
			return false;
		}

		TArray<TArray<double>> BedZ;
		BedZ.SetNum(WorkChains.Num());
		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			const FWorkChain& Chain = WorkChains[ChainIndex];
			BedZ[ChainIndex].SetNumUninitialized(Chain.HalfWidthCm.Num());
			for (int32 PointIndex = 0; PointIndex < Chain.HalfWidthCm.Num(); ++PointIndex)
			{
				const double SeedDepth =
					2.0 * static_cast<double>(Chain.HalfWidthCm[PointIndex]) / static_cast<double>(Input.WidthToDepthRatio);
				BedZ[ChainIndex][PointIndex] = SurfaceZ[ChainIndex][PointIndex] - SeedDepth;
			}
		}

		// A monotone surface does not make the bed monotone: a downstream narrowing shrinks the seed
		// depth and lifts the bed, damming the channel, so the bed gets its own minimum. Lowering the
		// bed never raises the surface, so the surface solve above still stands.
		if (!SolveDownstreamMinimum(BedZ, JunctionIncidence, MaxPasses))
		{
			OutError = LOCTEXT("BedNotConverged",
				"The river bed height did not settle; the extracted junction table is inconsistent.");
			return false;
		}

		OutPolylines.Reserve(WorkChains.Num());
		for (int32 ChainIndex = 0; ChainIndex < WorkChains.Num(); ++ChainIndex)
		{
			FWorkChain& Chain = WorkChains[ChainIndex];
			const int32 NumPoints = Chain.WorldPoints.Num();

			FArcRiverPolyline& Polyline = OutPolylines.AddDefaulted_GetRef();
			Polyline.WorldPoints = MoveTemp(Chain.WorldPoints);
			Polyline.HalfWidthCm = MoveTemp(Chain.HalfWidthCm);
			Polyline.DepthCm.SetNumUninitialized(NumPoints);
			Polyline.BedZ.SetNumUninitialized(NumPoints);
			Polyline.GroundZ.SetNumUninitialized(NumPoints);

			for (int32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
			{
				Polyline.WorldPoints[PointIndex].Z = SurfaceZ[ChainIndex][PointIndex];
				Polyline.GroundZ[PointIndex] = static_cast<float>(Chain.GroundZ[PointIndex]);
				Polyline.BedZ[PointIndex] = static_cast<float>(BedZ[ChainIndex][PointIndex]);
				// The water body measures Depth downward from the spline, which sits at the solved
				// surface, so it is re-derived after the bed fixpoint rather than kept from the seed.
				Polyline.DepthCm[PointIndex] =
					static_cast<float>(SurfaceZ[ChainIndex][PointIndex] - BedZ[ChainIndex][PointIndex]);
			}
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
