// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcTownDebugDraw.h"

#if UE_DEBUG_RENDERING_ENABLED

#include "Catalog/ArcTownBuildingCatalog.h"
#include "Catalog/ArcTownBuildingDefinition.h"
#include "Components/BoxComponent.h"
#include "DebugDraw.h"
#include "DebugDrawLabelPool.h"
#include "Layout/ArcTownRoadArrangement.h"
#include "Morphology/ArcTownBlocks.h"
#include "Simulation/ArcTownBootstrap.h"
#include "Simulation/ArcTownGrowthDiagnostics.h"
#include "Simulation/ArcTownPlacement.h"
#include "Simulation/ArcTownSimulationState.h"
#include "Simulation/ArcTownSiteFields.h"
#include "Simulation/ArcTownTensorField.h"
#include "Simulation/ArcTownWorldSnapshot.h"
#include "Town/ArcTownAttractorVolume.h"
#include "Town/ArcTownVolume.h"
#include "UObject/Class.h"
#include "UObject/ObjectKey.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcTownDebugDraw, Log, All);

namespace ArcTownDebugDrawPrivate
{
	constexpr int32 SheetShadeBuckets = 32;
	constexpr int32 MaxMeshBatches = 128;
	constexpr int32 RectEdgeSegments = 32;
	constexpr int32 CoreRingSegments = 48;
	constexpr int32 MinDrapedRingSegments = 16;
	constexpr int32 MaxDrapedRingSegments = 360;
	constexpr int32 HierarchyColorCount = 5;
	constexpr double TensorSegmentFraction = 0.4;
	constexpr double LabelStackSpacingCm = 400.0;
	constexpr double PlannedDashFraction = 0.55;
	constexpr float NodePointSizePx = 10.0f;
	constexpr int32 RoadRibbonTrianglesPerSegment = 2;

	/** The fill carries the colour mode, so the edge only separates one box from the next. */
	constexpr FColor BuildingEdgeColor(20, 20, 20, 255);

	constexpr double SingleStoreyWorldHeightCm = 300.0;
	constexpr double TwoStoreyWorldHeightCm = 600.0;

	constexpr double ArrangementSnapToleranceInSamples = 0.1;

	constexpr FColor BlockBoundaryColor(150, 150, 220, 255);
	constexpr FColor FrontagePolygonColor(235, 225, 90, 255);
	constexpr FColor FrontageAttributedColor(90, 235, 140, 255);

	static_assert(SheetShadeBuckets < MaxMeshBatches, "A sheet's shade buckets must leave a free slot in the mesh batch table.");

	enum class EArcTownDebugDrawWarning : uint8
	{
		TerrainScale,
		FreezeFailed,
		FieldsFailed,
		CoreFailed,
		NoSiteCells,
		SiteFieldStrideClamped,
		TensorStrideClamped,
		TensorFieldFullyDegenerate,
		RoadDrapeClamped,
		BuildingCatalogIndexUnknown,
		BuildingBudgetClamped,
		NoAttractors,
		BlockBudgetClamped,
	};

	struct FArcTownDebugDrawWarningKey
	{
		EArcTownDebugDrawWarning Kind = EArcTownDebugDrawWarning::TerrainScale;

		/** Two placements of one Level Instance share a TownId, so the latch keys on the actor itself. */
		FObjectKey Volume;

		uint64 Payload = 0;

		bool operator==(const FArcTownDebugDrawWarningKey& Other) const
		{
			return Kind == Other.Kind && Volume == Other.Volume && Payload == Other.Payload;
		}

		friend uint32 GetTypeHash(const FArcTownDebugDrawWarningKey& Key)
		{
			return HashCombine(
				HashCombine(GetTypeHash(uint8(Key.Kind)), GetTypeHash(Key.Volume)),
				GetTypeHash(Key.Payload));
		}
	};

	TSet<FArcTownDebugDrawWarningKey>& GetLatchedWarnings()
	{
		static TSet<FArcTownDebugDrawWarningKey> LatchedWarnings;
		return LatchedWarnings;
	}

	bool ShouldLogOnce(const EArcTownDebugDrawWarning Kind, const AArcTownVolume& Volume, const uint64 Payload = 0)
	{
		bool bAlreadyLatched = false;
		GetLatchedWarnings().Add(FArcTownDebugDrawWarningKey{Kind, FObjectKey(&Volume), Payload}, &bAlreadyLatched);
		return !bAlreadyLatched;
	}

	void ClearLogLatch(const EArcTownDebugDrawWarning Kind, const AArcTownVolume& Volume, const uint64 Payload = 0)
	{
		GetLatchedWarnings().Remove(FArcTownDebugDrawWarningKey{Kind, FObjectKey(&Volume), Payload});
	}

	/** The good path cannot name the payload that latched, so it clears every payload of that kind for this volume. */
	void ClearLogLatches(const EArcTownDebugDrawWarning Kind, const AArcTownVolume& Volume)
	{
		const FObjectKey VolumeKey(&Volume);
		TSet<FArcTownDebugDrawWarningKey>& LatchedWarnings = GetLatchedWarnings();

		TArray<FArcTownDebugDrawWarningKey> MatchingKeys;
		for (const FArcTownDebugDrawWarningKey& LatchedKey : LatchedWarnings)
		{
			if (LatchedKey.Kind == Kind && LatchedKey.Volume == VolumeKey)
			{
				MatchingKeys.Add(LatchedKey);
			}
		}

		for (const FArcTownDebugDrawWarningKey& MatchingKey : MatchingKeys)
		{
			LatchedWarnings.Remove(MatchingKey);
		}
	}

	template <typename EntryType>
	void PruneStaleVolumeEntries(TMap<TWeakObjectPtr<const AArcTownVolume>, EntryType>& Entries)
	{
		TArray<TWeakObjectPtr<const AArcTownVolume>> StaleKeys;
		for (const TPair<TWeakObjectPtr<const AArcTownVolume>, EntryType>& Pair : Entries)
		{
			if (!Pair.Key.IsValid())
			{
				StaleKeys.Add(Pair.Key);
			}
		}
		for (const TWeakObjectPtr<const AArcTownVolume>& StaleKey : StaleKeys)
		{
			Entries.Remove(StaleKey);
		}
	}

	uint32 MakeSettingsHash(const FArcTownGrowthSettings& Settings)
	{
		uint32 Hash = GetTypeHash(Settings.Fields.ElevationQuantumM);
		Hash = HashCombine(Hash, GetTypeHash(Settings.Fields.MaxBuildSlopeDeg));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Fields.MinWaterClearanceCm));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Fields.MinHeightAboveChannelM));
		Hash = HashCombine(Hash, GetTypeHash(Settings.TensorField.RadialWeight));
		Hash = HashCombine(Hash, GetTypeHash(Settings.TensorField.GridWeight));
		Hash = HashCombine(Hash, GetTypeHash(Settings.TensorField.BoundaryWeight));
		Hash = HashCombine(Hash, GetTypeHash(Settings.TensorField.TerrainHalfWeightSlopeDeg));
		Hash = HashCombine(Hash, GetTypeHash(Settings.TensorField.GridAngleDeg));
		Hash = HashCombine(Hash, GetTypeHash(Settings.TensorField.CoreFalloffCm));
		Hash = HashCombine(Hash, GetTypeHash(uint8(Settings.Morphology.Period)));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Morphology.MinFrontageRunCm));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Morphology.SetbackCm));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Morphology.YawJitterDeg));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Morphology.DefaultPlotGapCm));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Traffic.RoadLaneWidthCm));
		Hash = HashCombine(Hash, GetTypeHash(Settings.Traffic.HierarchyLaneCounts.Num()));
		for (const double LaneCount : Settings.Traffic.HierarchyLaneCounts)
		{
			Hash = HashCombine(Hash, GetTypeHash(LaneCount));
		}
		return Hash;
	}

	uint32 MakeAttractorsHash(TConstArrayView<TWeakObjectPtr<const AArcTownAttractorVolume>> CachedAttractors)
	{
		uint32 Combined = 0;
		int32 Count = 0;
		for (const TWeakObjectPtr<const AArcTownAttractorVolume>& AttractorPtr : CachedAttractors)
		{
			const AArcTownAttractorVolume* const Attractor = AttractorPtr.Get();
			if (Attractor == nullptr || Attractor->VolumeBounds == nullptr)
			{
				continue;
			}

			// The freeze admits an attractor by its box reaching the town, so extent and rotation are inputs too.
			const UBoxComponent& Bounds = *Attractor->VolumeBounds;
			const FVector Center = Bounds.GetComponentLocation();
			const FVector Extent = Bounds.GetScaledBoxExtent();
			const FQuat Rotation = Bounds.GetComponentQuat();

			uint32 Hash = GetTypeHash(Center);
			Hash = HashCombine(Hash, GetTypeHash(Extent));
			Hash = HashCombine(Hash, GetTypeHash(FVector(Rotation.X, Rotation.Y, Rotation.Z)));
			Hash = HashCombine(Hash, GetTypeHash(Rotation.W));
			Hash = HashCombine(Hash, GetTypeHash(Attractor->Strength));
			Hash = HashCombine(Hash, GetTypeHash(Attractor->InfluenceRadius));
			Hash = HashCombine(Hash, GetTypeHash(Attractor->GetFName()));
			Hash = HashCombine(Hash, GetTypeHash(Attractor->DistrictTags.ToStringSimple()));

			// Order-independent because actor iteration order is not stable, and additive because two equal
			// per-attractor hashes cancel out of an XOR.
			Combined += Hash;
			++Count;
		}
		return HashCombine(Combined, GetTypeHash(Count));
	}

	uint32 MakeCatalogHash(const UArcTownBuildingCatalog* const Catalog)
	{
		if (Catalog == nullptr)
		{
			return 0u;
		}

		uint32 Hash = HashCombine(PointerHash(Catalog), GetTypeHash(Catalog->Buildings.Num()));
		for (int32 Index = 0; Index < Catalog->Buildings.Num(); ++Index)
		{
			const UArcTownBuildingDefinition* const Entry = Catalog->Buildings[Index];
			if (Entry == nullptr)
			{
				Hash = HashCombine(Hash, GetTypeHash(Index));
				continue;
			}

			// The footprint sizes the drawn box, and an unloadable mesh fails the freeze outright.
			Hash = HashCombine(Hash, GetTypeHash(Entry->FootprintXY.X));
			Hash = HashCombine(Hash, GetTypeHash(Entry->FootprintXY.Y));
			const FArcTownBuildingVisual* const Visual = Entry->Visual.GetPtr();
			Hash = HashCombine(Hash, GetTypeHash(Visual != nullptr ? Visual->GetAssetPath() : FSoftObjectPath()));
			Hash = HashCombine(Hash, GetTypeHash(Entry->PlotSizeMinXY.X));
			Hash = HashCombine(Hash, GetTypeHash(Entry->PlotSizeMinXY.Y));
			Hash = HashCombine(Hash, GetTypeHash(Entry->PlotSizeMaxXY.X));
			Hash = HashCombine(Hash, GetTypeHash(Entry->PlotSizeMaxXY.Y));
			Hash = HashCombine(Hash, GetTypeHash(Entry->InfluenceRadius));
			Hash = HashCombine(Hash, GetTypeHash(Entry->SelectionWeight));
			Hash = HashCombine(Hash, GetTypeHash(Entry->EarliestYear));
		}
		return Hash;
	}

	FArcTownDebugDrawInputKey MakeInputKey(
		const AArcTownVolume& Volume,
		TConstArrayView<TWeakObjectPtr<const AArcTownAttractorVolume>> CachedAttractors)
	{
		FArcTownDebugDrawInputKey Key;
		Key.TownId = Volume.TownId;
		Key.BoundsTransform = Volume.VolumeBounds != nullptr
			? Volume.VolumeBounds->GetComponentTransform()
			: Volume.GetActorTransform();
		Key.SiteSnapshotHash = Volume.SiteSnapshot.SnapshotHash;
		Key.StateChecksum = Volume.SimulationState != nullptr ? Volume.SimulationState->Snapshot.StateChecksum : 0;
		Key.SettingsHash = MakeSettingsHash(Volume.GrowthSettings);
		Key.AttractorsHash = MakeAttractorsHash(CachedAttractors);
		Key.CatalogHash = MakeCatalogHash(Volume.Catalog);
		Key.Seed = Volume.Seed;
		return Key;
	}

	bool HasUsableTerrainTransform(const FArcTownSiteSnapshot& Site)
	{
		const FVector Scale = Site.TerrainToWorld.GetScale3D();
		return Scale.X > 0.0
			&& FMath::IsNearlyEqual(Scale.X, Scale.Y)
			&& FMath::IsNearlyEqual(Scale.Y, Scale.Z);
	}

	bool HasUsableSamples(const FArcTownSiteSnapshot& Site)
	{
		const int64 SampleCount = int64(Site.GridDims.X) * int64(Site.GridDims.Y);
		return Site.GridDims.X > 0
			&& Site.GridDims.Y > 0
			&& Site.SampleSpacing > 0.0
			&& int64(Site.Heights.Num()) == SampleCount;
	}

	FVector2D SiteCellToTerrainLocalXY(const FArcTownSiteSnapshot& Site, const double SiteX, const double SiteY)
	{
		const FVector2D SiteOrigin = Site.GetSiteOriginLocal();
		return FVector2D(SiteOrigin.X + SiteX * Site.SampleSpacing, SiteOrigin.Y + SiteY * Site.SampleSpacing);
	}

	double SampleTerrainLocalHeightCm(const FArcTownSiteSnapshot& Site, const FVector2D& TerrainLocalXY)
	{
		const int32 GridX = FMath::Clamp(
			FMath::RoundToInt32((TerrainLocalXY.X - Site.OriginLocal.X) / Site.SampleSpacing), 0, Site.GridDims.X - 1);
		const int32 GridY = FMath::Clamp(
			FMath::RoundToInt32((TerrainLocalXY.Y - Site.OriginLocal.Y) / Site.SampleSpacing), 0, Site.GridDims.Y - 1);
		const int32 Index = GridX + GridY * Site.GridDims.X;
		return Site.Heights.IsValidIndex(Index) ? double(Site.Heights[Index]) : 0.0;
	}

	double GetTerrainScale(const FArcTownSiteSnapshot& Site)
	{
		const double Scale = Site.TerrainToWorld.GetScale3D().X;
		return Scale > 0.0 ? Scale : 1.0;
	}

	/** LiftCm is WORLD centimetres; TerrainToWorld scales the sampled height, so the lift is divided out first. */
	FVector TerrainLocalToWorld(const FArcTownSiteSnapshot& Site, const FVector2D& TerrainLocalXY, const double LiftCm)
	{
		const double HeightCm = SampleTerrainLocalHeightCm(Site, TerrainLocalXY) + LiftCm / GetTerrainScale(Site);
		return Site.TerrainToWorld.TransformPosition(FVector(TerrainLocalXY.X, TerrainLocalXY.Y, HeightCm));
	}

	FVector TownLocalToWorld(const FArcTownSiteSnapshot& Site, const FVector2D& TownLocalXY, const double LiftCm)
	{
		return TerrainLocalToWorld(Site, ArcTownSiteFrame::TownLocalToTerrainLocalXY(Site, TownLocalXY), LiftCm);
	}

	/** Draped layers stack in whole multiples of the one authored lift, so a filled layer never shares a plane with the layer above it. */
	enum class EDrapeLiftTier : uint8
	{
		Sheet = 1,
		Ribbon = 2,
		Overlay = 3,
	};

	double MakeDrapedLiftCm(const AArcTownVolume& Volume, const EDrapeLiftTier Tier)
	{
		return double(uint8(Tier)) * Volume.DebugSurfaceLiftCm;
	}

	void AppendDrapedRect(
		const FArcTownSiteSnapshot& Site,
		const FVector2D& MinTerrainLocalXY,
		const FVector2D& MaxTerrainLocalXY,
		const double LiftCm,
		const FColor& Color,
		const float Thickness,
		TArray<UE::DebugDraw::FLineSubmission>& OutLines)
	{
		const FVector2D Corners[4] = {
			MinTerrainLocalXY,
			FVector2D(MinTerrainLocalXY.X, MaxTerrainLocalXY.Y),
			MaxTerrainLocalXY,
			FVector2D(MaxTerrainLocalXY.X, MinTerrainLocalXY.Y)};

		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			const FVector2D EdgeStart = Corners[CornerIndex];
			const FVector2D EdgeEnd = Corners[(CornerIndex + 1) % 4];

			FVector PreviousWorld = TerrainLocalToWorld(Site, EdgeStart, LiftCm);
			for (int32 SegmentIndex = 1; SegmentIndex <= RectEdgeSegments; ++SegmentIndex)
			{
				const double Alpha = double(SegmentIndex) / double(RectEdgeSegments);
				const FVector NextWorld = TerrainLocalToWorld(
					Site, FMath::Lerp(EdgeStart, EdgeEnd, Alpha), LiftCm);
				OutLines.Add(UE::DebugDraw::FLineSubmission{
					PreviousWorld, NextWorld, Color, Thickness, UE::DebugDraw::EDepthTest::WorldDepth});
				PreviousWorld = NextWorld;
			}
		}
	}

	FVector2D ToDouble(const FVector2f& Value)
	{
		return FVector2D(double(Value.X), double(Value.Y));
	}

	int32 ResolveDrapeSegmentCount(const double LengthCm, const double SpacingCm)
	{
		if (!FMath::IsFinite(LengthCm) || LengthCm <= 0.0 || SpacingCm <= 0.0)
		{
			return 1;
		}
		return FMath::Max(1, FMath::CeilToInt32(LengthCm / SpacingCm));
	}

	/** Start and End are TOWN-local; the polyline takes its height from the sampled ground under each sample. */
	void AppendDrapedTownLocalLine(
		const FArcTownSiteSnapshot& Site,
		const FVector2D& StartTownLocalXY,
		const FVector2D& EndTownLocalXY,
		const double LiftCm,
		const FColor& Color,
		const float Thickness,
		TArray<UE::DebugDraw::FLineSubmission>& OutLines)
	{
		const int32 SegmentCount = ResolveDrapeSegmentCount(
			(EndTownLocalXY - StartTownLocalXY).Size(), Site.SampleSpacing);

		FVector PreviousWorld = TownLocalToWorld(Site, StartTownLocalXY, LiftCm);
		for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
		{
			const double Alpha = double(SegmentIndex) / double(SegmentCount);
			const FVector NextWorld = TownLocalToWorld(
				Site, FMath::Lerp(StartTownLocalXY, EndTownLocalXY, Alpha), LiftCm);
			OutLines.Add(UE::DebugDraw::FLineSubmission{
				PreviousWorld, NextWorld, Color, Thickness, UE::DebugDraw::EDepthTest::WorldDepth});
			PreviousWorld = NextWorld;
		}
	}

	void AppendDrapedRing(
		const FArcTownSiteSnapshot& Site,
		const FVector2D& CenterTownLocalXY,
		const double RadiusCm,
		const double LiftCm,
		const FColor& Color,
		const float Thickness,
		TArray<UE::DebugDraw::FLineSubmission>& OutLines)
	{
		const int32 SegmentCount = FMath::Clamp(
			ResolveDrapeSegmentCount(UE_DOUBLE_TWO_PI * RadiusCm, Site.SampleSpacing),
			MinDrapedRingSegments,
			MaxDrapedRingSegments);

		FVector2D PreviousTownLocalXY = CenterTownLocalXY + FVector2D(RadiusCm, 0.0);
		for (int32 SegmentIndex = 1; SegmentIndex <= SegmentCount; ++SegmentIndex)
		{
			const double Angle = UE_DOUBLE_TWO_PI * double(SegmentIndex) / double(SegmentCount);
			const FVector2D NextTownLocalXY = CenterTownLocalXY
				+ FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * RadiusCm;
			OutLines.Add(UE::DebugDraw::FLineSubmission{
				TownLocalToWorld(Site, PreviousTownLocalXY, LiftCm),
				TownLocalToWorld(Site, NextTownLocalXY, LiftCm),
				Color,
				Thickness,
				UE::DebugDraw::EDepthTest::WorldDepth});
			PreviousTownLocalXY = NextTownLocalXY;
		}
	}

	void ResolveStride(
		const FIntPoint& SiteDims,
		const int32 AuthoredStride,
		const int32 MaxDrawnSamples,
		int32& OutStride,
		bool& bOutClampedToBudget)
	{
		OutStride = FMath::Max(1, AuthoredStride);
		bOutClampedToBudget = false;

		if (MaxDrawnSamples <= 0)
		{
			return;
		}

		const double CellCount = double(SiteDims.X) * double(SiteDims.Y);
		const int32 AutoStride = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(CellCount / double(MaxDrawnSamples))));
		if (AuthoredStride <= 0)
		{
			OutStride = AutoStride;
			return;
		}

		const int64 SampledCellCount = int64(FMath::DivideAndRoundUp(SiteDims.X, OutStride))
			* int64(FMath::DivideAndRoundUp(SiteDims.Y, OutStride));
		if (SampledCellCount > int64(MaxDrawnSamples))
		{
			OutStride = FMath::Max(OutStride, AutoStride);
			bOutClampedToBudget = true;
		}
	}

	/** Anything below 255 routes the submission to DebugDraw's depth-testing, non-depth-writing tier. */
	uint8 ResolveFillAlpha(const AArcTownVolume& Volume)
	{
		const float Percent = FMath::Clamp(Volume.DebugOverlayOpacity, 1.0f, 100.0f);
		return uint8(FMath::Clamp(FMath::RoundToInt32(Percent * 2.55f), 1, 255));
	}

	FColor MakeRampColor(const double Normalised01, const uint8 Alpha)
	{
		static const FLinearColor Stops[5] = {
			FLinearColor(0.05f, 0.03f, 0.28f),
			FLinearColor(0.13f, 0.32f, 0.55f),
			FLinearColor(0.13f, 0.57f, 0.55f),
			FLinearColor(0.36f, 0.78f, 0.38f),
			FLinearColor(0.99f, 0.91f, 0.15f),
		};

		const double Scaled = FMath::Clamp(Normalised01, 0.0, 1.0) * 4.0;
		const int32 LowIndex = FMath::Min(int32(Scaled), 3);
		FColor Color = FMath::Lerp(Stops[LowIndex], Stops[LowIndex + 1], float(Scaled - double(LowIndex))).ToFColor(true);
		Color.A = Alpha;
		return Color;
	}

	FColor MakeBinaryColor(const bool bSet, const uint8 Alpha)
	{
		return bSet ? FColor(70, 200, 90, Alpha) : FColor(150, 40, 45, Alpha);
	}

	FColor MakeSentinelColor(const uint8 Alpha)
	{
		return FColor(255, 0, 255, Alpha);
	}

	FColor MakeHierarchyColor(const uint8 Hierarchy, const uint8 Alpha)
	{
		static const FColor Steps[HierarchyColorCount] = {
			FColor(120, 130, 150, 255),
			FColor(90, 190, 235, 255),
			FColor(90, 235, 140, 255),
			FColor(245, 205, 70, 255),
			FColor(245, 105, 60, 255),
		};
		FColor Color = Steps[FMath::Min(int32(Hierarchy), HierarchyColorCount - 1)];
		Color.A = Alpha;
		return Color;
	}

	FColor MakeIndexColor(const int32 Index, const uint8 Alpha)
	{
		const uint32 Scrambled = uint32(GetTypeHash(Index)) * 2654435761u;
		FColor Color = FLinearColor::MakeFromHSV8(uint8(Scrambled >> 24), 190, 245).ToFColor(true);
		Color.A = Alpha;
		return Color;
	}

	FColor MakeNormalisedColor(const double Value, const double MinValue, const double MaxValue, const uint8 Alpha)
	{
		const double Span = MaxValue - MinValue;
		return MakeRampColor(Span > 0.0 ? (Value - MinValue) / Span : 0.5, Alpha);
	}

	/** Scene key every submission for this volume carries, so the shapes stay in the volume's own world. */
	const FSceneInterface* VolumeScene(const AArcTownVolume& Volume)
	{
		return UE::DebugDraw::GetSceneKey(Volume.GetWorld());
	}

	void EmitLabel(const FSceneInterface* Scene, const FVector& WorldLocation, const FColor& Color, const FString& Text)
	{
		FColor OpaqueColor = Color;
		OpaqueColor.A = 255;
		UE::DebugDraw::EnqueueLabel(Scene, WorldLocation, OpaqueColor, Text, UE::DebugDraw::EFrustumCullPolicy::Apply);
	}

	enum class ELabelPass : uint8
	{
		Offer,
		Enqueue,
	};

	/** Two passes: the first measures anchors, the second formats only the survivors, so a dropped label costs no Printf. */
	struct FLabelBudget
	{
		int32 Limit = 0;
		int32 Requested = 0;
		int32 Enqueued = 0;

		/** Ordering keys off the primary view the label pool's own top-N keep-nearest uses; empty view set falls back to array order. */
		FVector ViewOrigin = FVector::ZeroVector;
		bool bHasView = false;

		TArray<double> OfferedDistancesSquared;
		double EnqueueDistanceSquared = TNumericLimits<double>::Max();

		/** A record with a non-finite anchor sorts last rather than poisoning the cut. */
		double MakeSortableDistanceSquared(const FVector& WorldLocation) const
		{
			const double DistanceSquared = FVector::DistSquared(WorldLocation, ViewOrigin);
			return FMath::IsFinite(DistanceSquared) ? DistanceSquared : TNumericLimits<double>::Max();
		}

		void Offer(const FVector& WorldLocation)
		{
			++Requested;
			if (bHasView)
			{
				OfferedDistancesSquared.Add(MakeSortableDistanceSquared(WorldLocation));
			}
		}

		void ResolveDistanceCut()
		{
			EnqueueDistanceSquared = TNumericLimits<double>::Max();
			if (bHasView && Limit > 0 && OfferedDistancesSquared.Num() > Limit)
			{
				OfferedDistancesSquared.Sort();
				EnqueueDistanceSquared = OfferedDistancesSquared[Limit - 1];
			}
		}

		bool TryEnqueue(const FVector& WorldLocation)
		{
			if (Limit > 0 && Enqueued >= Limit)
			{
				return false;
			}
			if (bHasView && MakeSortableDistanceSquared(WorldLocation) > EnqueueDistanceSquared)
			{
				return false;
			}
			++Enqueued;
			return true;
		}
	};

	double QuantizeShade(const double Shade)
	{
		const int32 Bucket = FMath::Clamp(
			int32(FMath::Clamp(Shade, 0.0, 1.0) * double(SheetShadeBuckets)), 0, SheetShadeBuckets - 1);
		return (double(Bucket) + 0.5) / double(SheetShadeBuckets);
	}

	bool IsBinarySiteField(const EArcTownDebugSiteField Field)
	{
		return Field == EArcTownDebugSiteField::Buildable || Field == EArcTownDebugSiteField::ChannelMask;
	}

	FString GetSiteFieldName(const EArcTownDebugSiteField Field)
	{
		const UEnum* const FieldEnum = StaticEnum<EArcTownDebugSiteField>();
		return FieldEnum != nullptr
			? FieldEnum->GetNameStringByValue(int64(Field))
			: FString::FromInt(int32(Field));
	}

	FString GetSiteFieldUnit(const EArcTownDebugSiteField Field)
	{
		switch (Field)
		{
		case EArcTownDebugSiteField::DistanceToWater:
			return TEXT("cm");

		case EArcTownDebugSiteField::HeightAboveChannel:
			return TEXT("m");

		default:
			return FString();
		}
	}

	struct FSiteFieldSample
	{
		double Value = 0.0;
		bool bValid = false;
	};

	FSiteFieldSample GetSiteFieldValue(
		const FArcTownSiteSnapshot& Site,
		const FArcTownSiteFields& Fields,
		const EArcTownDebugSiteField Field,
		const int32 CellIndex)
	{
		FSiteFieldSample Sample;
		switch (Field)
		{
		case EArcTownDebugSiteField::Slope:
			if (Fields.Slope.IsValidIndex(CellIndex))
			{
				Sample.Value = double(Fields.Slope[CellIndex]);
			}
			break;

		case EArcTownDebugSiteField::DistanceToWater:
			if (Fields.DistanceToWaterCm.IsValidIndex(CellIndex))
			{
				Sample.Value = double(Fields.DistanceToWaterCm[CellIndex]);
			}
			break;

		case EArcTownDebugSiteField::HeightAboveChannel:
			if (Fields.HeightAboveChannel.IsValidIndex(CellIndex))
			{
				Sample.Value = double(Fields.HeightAboveChannel[CellIndex]);
			}
			break;

		case EArcTownDebugSiteField::Buildable:
			if (Fields.bBuildable.IsValidIndex(CellIndex))
			{
				Sample.Value = Fields.bBuildable[CellIndex] != 0 ? 1.0 : 0.0;
			}
			break;

		case EArcTownDebugSiteField::ChannelMask:
			if (Site.ChannelMask.IsValidIndex(CellIndex))
			{
				Sample.Value = Site.ChannelMask[CellIndex] != 0 ? 1.0 : 0.0;
			}
			break;
		}

		Sample.bValid = FMath::IsFinite(Sample.Value) && Sample.Value < double(TNumericLimits<float>::Max());
		return Sample;
	}

	uint64 MakeMeshBatchKey(const FColor& Color, const UE::DebugDraw::EDepthTest Depth)
	{
		return uint64(Color.ToPackedARGB()) | (uint64(Depth) << 32);
	}

	int32 FindOrAddMeshBatch(
		TArray<FArcTownDebugDrawMeshBatch>& Batches,
		TMap<uint64, int32>& BatchIndexByKey,
		const FColor& Color,
		const UE::DebugDraw::EDepthTest Depth,
		const int32 NewBatchVertexHint)
	{
		const uint64 BatchKey = MakeMeshBatchKey(Color, Depth);
		if (const int32* const ExistingIndex = BatchIndexByKey.Find(BatchKey))
		{
			return *ExistingIndex;
		}

		if (Batches.Num() >= MaxMeshBatches)
		{
			BatchIndexByKey.Add(BatchKey, 0);
			return 0;
		}

		const int32 NewIndex = Batches.Num();
		FArcTownDebugDrawMeshBatch& Batch = Batches.AddDefaulted_GetRef();
		Batch.Color = Color;
		Batch.Depth = Depth;
		if (NewBatchVertexHint > 0)
		{
			Batch.Vertices.Reserve(NewBatchVertexHint);
			Batch.Indices.Reserve(NewBatchVertexHint + NewBatchVertexHint / 2);
		}
		BatchIndexByKey.Add(BatchKey, NewIndex);
		return NewIndex;
	}

	void EmitSheetQuad(
		const FArcTownSiteSnapshot& Site,
		const FVector2D& MinSiteCell,
		const FVector2D& MaxSiteCell,
		const double LiftCm,
		FArcTownDebugDrawMeshBatch& Batch)
	{
		const FVector2D CornerCells[4] = {
			MinSiteCell,
			FVector2D(MinSiteCell.X, MaxSiteCell.Y),
			MaxSiteCell,
			FVector2D(MaxSiteCell.X, MinSiteCell.Y)};

		const int32 BaseVertex = Batch.Vertices.Num();
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			const FVector2D TerrainLocalXY = SiteCellToTerrainLocalXY(
				Site, CornerCells[CornerIndex].X, CornerCells[CornerIndex].Y);
			Batch.Vertices.Add(TerrainLocalToWorld(Site, TerrainLocalXY, LiftCm));
		}

		Batch.Indices.Add(BaseVertex + 0);
		Batch.Indices.Add(BaseVertex + 1);
		Batch.Indices.Add(BaseVertex + 2);

		Batch.Indices.Add(BaseVertex + 0);
		Batch.Indices.Add(BaseVertex + 2);
		Batch.Indices.Add(BaseVertex + 3);
	}

	void DrawVolumeBounds(const AArcTownVolume& Volume)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		const UBoxComponent* const Bounds = Volume.VolumeBounds;
		if (Bounds == nullptr)
		{
			return;
		}

		UE::DebugDraw::Box(Scene, Bounds->GetComponentLocation(), Bounds->GetScaledBoxExtent(), Bounds->GetComponentQuat(),
			FColor::White, Volume.DebugLineThicknessCm, UE::DebugDraw::EDepthTest::WorldDepth);
	}

	void DrawSiteGrid(const AArcTownVolume& Volume)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		const FArcTownSiteSnapshot& Site = Volume.SiteSnapshot;
		const FIntPoint SiteDims = Site.GetSiteDims();
		if (SiteDims.X <= 1 || SiteDims.Y <= 1)
		{
			return;
		}

		const FVector2D SiteMin = Site.GetSiteOriginLocal();
		const FVector2D SiteMax = SiteMin
			+ FVector2D(double(SiteDims.X - 1), double(SiteDims.Y - 1)) * Site.SampleSpacing;
		const FVector2D GridMin = Site.OriginLocal;
		const FVector2D GridMax = GridMin
			+ FVector2D(double(Site.GridDims.X - 1), double(Site.GridDims.Y - 1)) * Site.SampleSpacing;

		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);

		TArray<UE::DebugDraw::FLineSubmission> Lines;
		Lines.Reserve(8 * RectEdgeSegments);
		AppendDrapedRect(Site, SiteMin, SiteMax, LiftCm, FColor(90, 230, 120, 255),
			Volume.DebugLineThicknessCm, Lines);
		AppendDrapedRect(Site, GridMin, GridMax, LiftCm, FColor(235, 150, 45, 255),
			Volume.DebugLineThicknessCm, Lines);
		UE::DebugDraw::Lines(Scene, Lines);
	}

	bool TryFreezeEntry(
		const AArcTownVolume& Volume,
		FArcTownDebugDrawFrozenCache::FEntry& Entry)
	{
		FText Error;
		if (!Volume.FreezeSimulationInput(1, Entry.Input, Error))
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::FreezeFailed, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls cannot freeze its simulation input, so the site and tensor layers draw nothing: %ls",
					*Volume.GetName(), *Error.ToString());
			}
			return false;
		}
		ClearLogLatch(EArcTownDebugDrawWarning::FreezeFailed, Volume);
		Entry.bInputFrozen = true;

		if (!ArcTownSiteFields::BuildSiteFields(Entry.Input.Site, Entry.Input.Settings.Fields, Entry.Fields, Error))
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::FieldsFailed, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls cannot build its site fields, so the site and tensor layers draw nothing: %ls",
					*Volume.GetName(), *Error.ToString());
			}
			return true;
		}
		ClearLogLatch(EArcTownDebugDrawWarning::FieldsFailed, Volume);
		Entry.bFieldsBuilt = true;

		FArcTownBootstrapReport BootstrapReport;
		if (!ArcTownBootstrap::ChooseCore(Entry.Input.Site, Entry.Fields, Entry.Input.Settings.Fields,
			Entry.Input.Attractors, Entry.Input.SimulationId, Entry.Input.Seed, Entry.CoreTownLocalXY,
			BootstrapReport, Error))
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::CoreFailed, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls has no town core, so the tensor field layer draws nothing: %ls",
					*Volume.GetName(), *Error.ToString());
			}
			return true;
		}
		ClearLogLatch(EArcTownDebugDrawWarning::CoreFailed, Volume);
		Entry.bCoreChosen = true;
		return true;
	}

	const FArcTownDebugDrawFrozenCache::FEntry* GetFrozenEntry(
		const AArcTownVolume& Volume,
		TConstArrayView<TWeakObjectPtr<const AArcTownAttractorVolume>> CachedAttractors,
		FArcTownDebugDrawFrozenCache& FrozenCache)
	{
		const FArcTownDebugDrawInputKey Key = MakeInputKey(Volume, CachedAttractors);

		FArcTownDebugDrawFrozenCache::FEntry* Entry = FrozenCache.Entries.Find(&Volume);
		if (Entry != nullptr && Entry->bAttempted)
		{
			if (Entry->Key.Matches(Key))
			{
				Entry->bBoundsMovePending = false;
				return Entry->bInputFrozen ? Entry : nullptr;
			}

			// A moving volume redraws the last freeze; the first tick that repeats the transform refreezes.
			const bool bBoundsSettled = Entry->bBoundsMovePending
				&& Entry->PendingBoundsTransform.Equals(Key.BoundsTransform);
			if (Entry->Key.MatchesIgnoringBounds(Key) && !bBoundsSettled)
			{
				Entry->bBoundsMovePending = true;
				Entry->PendingBoundsTransform = Key.BoundsTransform;
				return Entry->bInputFrozen ? Entry : nullptr;
			}
		}

		PruneStaleVolumeEntries(FrozenCache.Entries);
		Entry = &FrozenCache.Entries.FindOrAdd(&Volume);
		*Entry = FArcTownDebugDrawFrozenCache::FEntry();
		Entry->Key = Key;
		Entry->bAttempted = true;
		TryFreezeEntry(Volume, *Entry);

		return Entry->bInputFrozen ? Entry : nullptr;
	}

	void BuildSiteFieldGeometry(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache::FSiteFieldEntry& Entry)
	{
		Entry.Batches.Reset();
		Entry.BatchIndexByKey.Reset();
		Entry.MinValue = 0.0;
		Entry.MaxValue = 0.0;
		Entry.DrawnCellCount = 0;

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const FIntPoint SiteDims = Site.GetSiteDims();
		if (SiteDims.X <= 0 || SiteDims.Y <= 0)
		{
			return;
		}

		int32 Stride = 1;
		bool bClampedToBudget = false;
		ResolveStride(SiteDims, Volume.SiteFieldStride, Volume.MaxDrawnSiteCells, Stride, bClampedToBudget);
		if (bClampedToBudget)
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::SiteFieldStrideClamped, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls authored Site Field Stride %d would exceed Max Drawn Site Cells (%d), so the overlay raised it to %d.",
					*Volume.GetName(), Volume.SiteFieldStride, Volume.MaxDrawnSiteCells, Stride);
			}
		}
		else
		{
			ClearLogLatch(EArcTownDebugDrawWarning::SiteFieldStrideClamped, Volume);
		}

		const EArcTownDebugSiteField Field = Volume.SiteField;
		const bool bBinaryField = IsBinarySiteField(Field);

		double MinValue = TNumericLimits<double>::Max();
		double MaxValue = -TNumericLimits<double>::Max();
		if (!bBinaryField)
		{
			for (int32 SiteY = 0; SiteY < SiteDims.Y; SiteY += Stride)
			{
				for (int32 SiteX = 0; SiteX < SiteDims.X; SiteX += Stride)
				{
					const FSiteFieldSample Sample = GetSiteFieldValue(
						Site, Frozen.Fields, Field, Site.SnapshotIndex(SiteX, SiteY));
					if (Sample.bValid)
					{
						MinValue = FMath::Min(MinValue, Sample.Value);
						MaxValue = FMath::Max(MaxValue, Sample.Value);
					}
				}
			}
		}

		const double ValueSpan = MaxValue - MinValue;
		const int32 SampledCellCount = FMath::DivideAndRoundUp(SiteDims.X, Stride)
			* FMath::DivideAndRoundUp(SiteDims.Y, Stride);
		const int32 NewBatchVertexHint = FMath::Max(4, (SampledCellCount * 4) / SheetShadeBuckets);
		const double MinCellBound = -0.5;
		const double MaxCellBoundX = double(SiteDims.X - 1) + 0.5;
		const double MaxCellBoundY = double(SiteDims.Y - 1) + 0.5;
		const float CellScale = FMath::Clamp(Volume.SiteFieldCellScale, 0.05f, 1.0f);
		const uint8 FillAlpha = ResolveFillAlpha(Volume);

		for (int32 SiteY = 0; SiteY < SiteDims.Y; SiteY += Stride)
		{
			for (int32 SiteX = 0; SiteX < SiteDims.X; SiteX += Stride)
			{
				const FSiteFieldSample Sample = GetSiteFieldValue(
					Site, Frozen.Fields, Field, Site.SnapshotIndex(SiteX, SiteY));

				FColor Color = MakeSentinelColor(FillAlpha);
				if (Sample.bValid)
				{
					if (bBinaryField)
					{
						Color = MakeBinaryColor(Sample.Value != 0.0, FillAlpha);
					}
					else
					{
						const double Shade = ValueSpan > 0.0 ? (Sample.Value - MinValue) / ValueSpan : 0.5;
						Color = MakeRampColor(QuantizeShade(Shade), FillAlpha);
					}
				}

				FVector2D MinCell(
					FMath::Max(double(SiteX) - 0.5, MinCellBound),
					FMath::Max(double(SiteY) - 0.5, MinCellBound));
				FVector2D MaxCell(
					FMath::Min(double(SiteX) + double(Stride) - 0.5, MaxCellBoundX),
					FMath::Min(double(SiteY) + double(Stride) - 0.5, MaxCellBoundY));
				if (MaxCell.X <= MinCell.X || MaxCell.Y <= MinCell.Y)
				{
					continue;
				}

				if (CellScale < 1.0f)
				{
					const FVector2D Center = (MinCell + MaxCell) * 0.5;
					const FVector2D HalfExtent = (MaxCell - MinCell) * 0.5 * double(CellScale);
					MinCell = Center - HalfExtent;
					MaxCell = Center + HalfExtent;
				}

				const int32 BatchIndex = FindOrAddMeshBatch(
					Entry.Batches, Entry.BatchIndexByKey, Color, UE::DebugDraw::EDepthTest::WorldDepth,
					NewBatchVertexHint);
				EmitSheetQuad(Site, MinCell, MaxCell, MakeDrapedLiftCm(Volume, EDrapeLiftTier::Sheet),
					Entry.Batches[BatchIndex]);
				++Entry.DrawnCellCount;
			}
		}

		if (!bBinaryField && MaxValue >= MinValue)
		{
			Entry.MinValue = MinValue;
			Entry.MaxValue = MaxValue;
		}
	}

	void DrawSiteField(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache& GeometryCache,
		const FArcTownDebugDrawGeometryCache::FSiteFieldEntry*& OutEntry)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		OutEntry = nullptr;
		if (!Frozen.bFieldsBuilt)
		{
			return;
		}

		FArcTownDebugDrawGeometryKey Key;
		Key.Input = Frozen.Key;
		Key.Field = uint8(Volume.SiteField);
		Key.Stride = Volume.SiteFieldStride;
		Key.MaxDrawn = Volume.MaxDrawnSiteCells;
		Key.CellScale = Volume.SiteFieldCellScale;
		Key.SurfaceLiftCm = Volume.DebugSurfaceLiftCm;
		Key.OverlayOpacityPercent = Volume.DebugOverlayOpacity;

		FArcTownDebugDrawGeometryCache::FSiteFieldEntry* Entry = GeometryCache.SiteFieldEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.SiteFieldEntries);
			Entry = &GeometryCache.SiteFieldEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildSiteFieldGeometry(Volume, Frozen, *Entry);
		}

		if (Entry->DrawnCellCount == 0)
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::NoSiteCells, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls drew no site field cell; the sampled site has no cells outside its margin.",
					*Volume.GetName());
			}
		}
		else
		{
			ClearLogLatch(EArcTownDebugDrawWarning::NoSiteCells, Volume);
		}

		for (const FArcTownDebugDrawMeshBatch& Batch : Entry->Batches)
		{
			if (Batch.Indices.Num() == 0)
			{
				continue;
			}
			UE::DebugDraw::Mesh(Scene, Batch.Vertices, Batch.Indices, Batch.Color, Batch.Depth);
		}

		OutEntry = Entry;
	}

	void BuildTensorFieldGeometry(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache::FTensorFieldEntry& Entry)
	{
		Entry.Lines.Reset();
		Entry.CoreWorld = FVector::ZeroVector;
		Entry.CoreRadiusCm = 0.0;
		Entry.DrawnSampleCount = 0;

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const FIntPoint SiteDims = Site.GetSiteDims();
		if (SiteDims.X <= 0 || SiteDims.Y <= 0)
		{
			return;
		}

		int32 Stride = 1;
		bool bClampedToBudget = false;
		ResolveStride(SiteDims, Volume.TensorFieldStride, Volume.MaxDrawnTensorSamples, Stride, bClampedToBudget);
		if (bClampedToBudget)
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::TensorStrideClamped, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls authored Tensor Field Stride %d would exceed Max Drawn Tensor Samples (%d), so the overlay raised it to %d.",
					*Volume.GetName(), Volume.TensorFieldStride, Volume.MaxDrawnTensorSamples, Stride);
			}
		}
		else
		{
			ClearLogLatch(EArcTownDebugDrawWarning::TensorStrideClamped, Volume);
		}

		FArcTownTensorField Field;
		Field.Site = &Site;
		Field.Fields = &Frozen.Fields;
		Field.Attractors = Frozen.Input.Attractors;
		Field.Settings = Frozen.Input.Settings.TensorField;
		Field.CoreTownLocalXY = Frozen.CoreTownLocalXY;

		const double HalfLengthCm = TensorSegmentFraction * double(Stride) * Site.SampleSpacing;
		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);
		const FColor LineColor(90, 200, 255, 255);
		Entry.Lines.Reserve(
			FMath::DivideAndRoundUp(SiteDims.X, Stride) * FMath::DivideAndRoundUp(SiteDims.Y, Stride));

		int32 VisitedSampleCount = 0;
		for (int32 SiteY = 0; SiteY < SiteDims.Y; SiteY += Stride)
		{
			for (int32 SiteX = 0; SiteX < SiteDims.X; SiteX += Stride)
			{
				++VisitedSampleCount;

				const FVector2f TownLocalXY = ArcTownSiteFrame::CellToTownLocalXY(Site, SiteX, SiteY);
				bool bDegenerate = false;
				const FVector2f Direction =
					ArcTownTensorField::SampleMajorEigenvector(Field, TownLocalXY, bDegenerate);
				if (bDegenerate)
				{
					continue;
				}

				const FVector2D Center(double(TownLocalXY.X), double(TownLocalXY.Y));
				const FVector2D Offset = FVector2D(double(Direction.X), double(Direction.Y)) * HalfLengthCm;

				Entry.Lines.Add(UE::DebugDraw::FLineSubmission{
					TownLocalToWorld(Site, Center - Offset, LiftCm),
					TownLocalToWorld(Site, Center + Offset, LiftCm),
					LineColor,
					Volume.DebugLineThicknessCm,
					UE::DebugDraw::EDepthTest::WorldDepth});
				++Entry.DrawnSampleCount;
			}
		}

		if (VisitedSampleCount > 0 && Entry.DrawnSampleCount == 0)
		{
			if (ShouldLogOnce(EArcTownDebugDrawWarning::TensorFieldFullyDegenerate, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls sampled %d tensor field points and every one was degenerate, so the tensor field overlay draws nothing.",
					*Volume.GetName(), VisitedSampleCount);
			}
		}
		else
		{
			ClearLogLatch(EArcTownDebugDrawWarning::TensorFieldFullyDegenerate, Volume);
		}

		Entry.CoreWorld = TownLocalToWorld(
			Site,
			FVector2D(double(Frozen.CoreTownLocalXY.X), double(Frozen.CoreTownLocalXY.Y)),
			LiftCm);
		Entry.CoreRadiusCm = Field.Settings.CoreFalloffCm * Site.TerrainToWorld.GetScale3D().X;
	}

	void DrawTensorField(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache& GeometryCache)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		if (!Frozen.bCoreChosen)
		{
			return;
		}

		FArcTownDebugDrawGeometryKey Key;
		Key.Input = Frozen.Key;
		Key.Stride = Volume.TensorFieldStride;
		Key.MaxDrawn = Volume.MaxDrawnTensorSamples;
		Key.SurfaceLiftCm = Volume.DebugSurfaceLiftCm;
		Key.LineThicknessCm = Volume.DebugLineThicknessCm;

		FArcTownDebugDrawGeometryCache::FTensorFieldEntry* Entry = GeometryCache.TensorFieldEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.TensorFieldEntries);
			Entry = &GeometryCache.TensorFieldEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildTensorFieldGeometry(Volume, Frozen, *Entry);
		}

		UE::DebugDraw::Lines(Scene, Entry->Lines);

		if (Entry->CoreRadiusCm > 0.0)
		{
			const FVector TerrainUp = Frozen.Input.Site.TerrainToWorld.TransformVectorNoScale(FVector::UpVector);
			UE::DebugDraw::Circle(Scene, Entry->CoreWorld, TerrainUp, float(Entry->CoreRadiusCm), CoreRingSegments,
				FColor(255, 210, 80, 255), Volume.DebugLineThicknessCm, UE::DebugDraw::EDepthTest::WorldDepth);
		}
	}

	bool IsDrawableEdge(const FArcTownWorldSnapshot& Snapshot, const FArcTownRoadEdge& Edge, double& OutLengthCm)
	{
		OutLengthCm = 0.0;
		if (!Snapshot.Nodes.IsValidIndex(Edge.NodeA) || !Snapshot.Nodes.IsValidIndex(Edge.NodeB))
		{
			return false;
		}

		OutLengthCm = (ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY) - ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY)).Size();
		return FMath::IsFinite(OutLengthCm);
	}

	struct FRoadDrapeEstimate
	{
		double SpacingCm = 0.0;
		int32 SegmentCount = 0;
		bool bClampedToBudget = false;
	};

	FRoadDrapeEstimate ResolveRoadDrape(
		const FArcTownSiteSnapshot& Site,
		const FArcTownWorldSnapshot& Snapshot,
		const int32 MaxDrawnSegments)
	{
		FRoadDrapeEstimate Estimate;
		Estimate.SpacingCm = FMath::Max(Site.SampleSpacing, 1.0);

		double TotalLengthCm = 0.0;
		int32 EdgeCount = 0;
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			double LengthCm = 0.0;
			if (IsDrawableEdge(Snapshot, Edge, LengthCm))
			{
				TotalLengthCm += LengthCm;
				++EdgeCount;
			}
		}

		if (EdgeCount == 0)
		{
			return Estimate;
		}

		const double EstimatedSegments = TotalLengthCm / Estimate.SpacingCm + double(EdgeCount);
		if (MaxDrawnSegments > 0 && EstimatedSegments > double(MaxDrawnSegments))
		{
			Estimate.SpacingCm *= EstimatedSegments / double(MaxDrawnSegments);
			Estimate.SegmentCount = MaxDrawnSegments;
			Estimate.bClampedToBudget = true;
			return Estimate;
		}

		Estimate.SegmentCount = FMath::CeilToInt32(EstimatedSegments);
		return Estimate;
	}

	int32 GetRoadColorBucketCount(const EArcTownDebugRoadColor Mode)
	{
		switch (Mode)
		{
		case EArcTownDebugRoadColor::Traffic:
			return SheetShadeBuckets;

		case EArcTownDebugRoadColor::Hierarchy:
			return HierarchyColorCount;

		default:
			return 2;
		}
	}

	FColor MakeRoadEdgeColor(
		const EArcTownDebugRoadColor Mode,
		const FArcTownRoadEdge& Edge,
		const double MinTraffic,
		const double MaxTraffic,
		const uint8 Alpha)
	{
		switch (Mode)
		{
		case EArcTownDebugRoadColor::Traffic:
		{
			// One mesh batch per distinct colour, so the ramp is bucketed like the sheet's rather than continuous.
			const double Span = MaxTraffic - MinTraffic;
			return MakeRampColor(
				QuantizeShade(Span > 0.0 ? (double(Edge.Traffic) - MinTraffic) / Span : 0.5), Alpha);
		}

		case EArcTownDebugRoadColor::Hierarchy:
			return MakeHierarchyColor(Edge.Hierarchy, Alpha);

		default:
			return Edge.bBuilt != 0 ? FColor(240, 240, 240, Alpha) : FColor(130, 135, 175, Alpha);
		}
	}

	/**
	 * Start and End are TOWN-local. The ribbon is a flat quad strip of the street's real width: every corner takes
	 * its own height from the sampled ground, so it follows the terrain across the width as well as along the length.
	 */
	void AppendRoadRibbon(
		const FArcTownSiteSnapshot& Site,
		const FVector2D& StartTownLocalXY,
		const FVector2D& EndTownLocalXY,
		const double HalfWidthCm,
		const int32 SegmentCount,
		const double DashFraction,
		const double LiftCm,
		FArcTownDebugDrawMeshBatch& Batch)
	{
		const FVector2D Direction = (EndTownLocalXY - StartTownLocalXY).GetSafeNormal();
		if (Direction.IsNearlyZero() || HalfWidthCm <= 0.0)
		{
			return;
		}

		const FVector2D HalfWidthOffset = FVector2D(-Direction.Y, Direction.X) * HalfWidthCm;
		const double Segments = double(FMath::Max(SegmentCount, 1));
		const double DrawnFraction = FMath::Clamp(DashFraction, 0.0, 1.0);

		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			const double AlphaStart = double(SegmentIndex) / Segments;
			const double AlphaEnd = AlphaStart + DrawnFraction / Segments;
			const FVector2D StationStart = FMath::Lerp(StartTownLocalXY, EndTownLocalXY, AlphaStart);
			const FVector2D StationEnd = FMath::Lerp(StartTownLocalXY, EndTownLocalXY, AlphaEnd);

			const int32 BaseVertex = Batch.Vertices.Num();
			Batch.Vertices.Add(TownLocalToWorld(Site, StationStart - HalfWidthOffset, LiftCm));
			Batch.Vertices.Add(TownLocalToWorld(Site, StationStart + HalfWidthOffset, LiftCm));
			Batch.Vertices.Add(TownLocalToWorld(Site, StationEnd + HalfWidthOffset, LiftCm));
			Batch.Vertices.Add(TownLocalToWorld(Site, StationEnd - HalfWidthOffset, LiftCm));

			Batch.Indices.Add(BaseVertex + 0);
			Batch.Indices.Add(BaseVertex + 1);
			Batch.Indices.Add(BaseVertex + 2);

			Batch.Indices.Add(BaseVertex + 0);
			Batch.Indices.Add(BaseVertex + 2);
			Batch.Indices.Add(BaseVertex + 3);
		}
	}

	void BuildRoadGeometry(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache::FRoadEntry& Entry)
	{
		Entry.Batches.Reset();
		Entry.BatchIndexByKey.Reset();
		Entry.Nodes.Reset();
		Entry.MinTraffic = 0.0;
		Entry.MaxTraffic = 0.0;
		Entry.DrapeSpacingCm = 0.0;
		Entry.DrawnEdgeCount = 0;
		Entry.BuiltEdgeCount = 0;
		Entry.SkippedEdgeCount = 0;
		Entry.DroppedEdgeCount = 0;
		Entry.DrawnTriangleCount = 0;
		Entry.FinishedNodeCount = 0;

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const FArcTownWorldSnapshot& Snapshot = Frozen.Input.SourceSnapshot;

		const int32 TriangleBudget = FMath::Max(Volume.MaxDrawnRoadTriangles, 0);
		const int32 SegmentBudget = TriangleBudget > 0
			? FMath::Max(1, TriangleBudget / RoadRibbonTrianglesPerSegment)
			: 0;

		const FRoadDrapeEstimate Drape = ResolveRoadDrape(Site, Snapshot, SegmentBudget);
		const double SpacingCm = Drape.SpacingCm;
		Entry.DrapeSpacingCm = SpacingCm;

		UE_CLOGF(Drape.bClampedToBudget && ShouldLogOnce(EArcTownDebugDrawWarning::RoadDrapeClamped, Volume),
			LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls would drape its roads over more than Max Drawn Road Triangles (%d), so the overlay stretched the drape spacing from %.0f cm to %.0f cm.",
			*Volume.GetName(), Volume.MaxDrawnRoadTriangles, Site.SampleSpacing, SpacingCm);
		if (!Drape.bClampedToBudget)
		{
			ClearLogLatch(EArcTownDebugDrawWarning::RoadDrapeClamped, Volume);
		}

		const int32 NewBatchVertexHint = FMath::Max(
			4, (Drape.SegmentCount * 4) / GetRoadColorBucketCount(Volume.RoadColor));
		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Ribbon);
		const uint8 FillAlpha = ResolveFillAlpha(Volume);

		double MinTraffic = TNumericLimits<double>::Max();
		double MaxTraffic = -TNumericLimits<double>::Max();
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			double LengthCm = 0.0;
			if (IsDrawableEdge(Snapshot, Edge, LengthCm) && FMath::IsFinite(Edge.Traffic))
			{
				MinTraffic = FMath::Min(MinTraffic, double(Edge.Traffic));
				MaxTraffic = FMath::Max(MaxTraffic, double(Edge.Traffic));
			}
		}
		if (MaxTraffic >= MinTraffic)
		{
			Entry.MinTraffic = MinTraffic;
			Entry.MaxTraffic = MaxTraffic;
		}

		int32 UsedSegmentCount = 0;
		for (const FArcTownRoadEdge& Edge : Snapshot.Edges)
		{
			double LengthCm = 0.0;
			if (!IsDrawableEdge(Snapshot, Edge, LengthCm) || LengthCm <= UE_DOUBLE_SMALL_NUMBER)
			{
				++Entry.SkippedEdgeCount;
				continue;
			}

			// Stretched spacing still floors at one segment per edge, so the budget only binds by dropping edges.
			const int32 SegmentCount = ResolveDrapeSegmentCount(LengthCm, SpacingCm);
			if (SegmentBudget > 0 && UsedSegmentCount + SegmentCount > SegmentBudget)
			{
				++Entry.DroppedEdgeCount;
				continue;
			}
			UsedSegmentCount += SegmentCount;

			const bool bBuilt = Edge.bBuilt != 0;
			const FColor Color = MakeRoadEdgeColor(
				Volume.RoadColor, Edge, Entry.MinTraffic, Entry.MaxTraffic, FillAlpha);
			const int32 BatchIndex = FindOrAddMeshBatch(
				Entry.Batches, Entry.BatchIndexByKey, Color, UE::DebugDraw::EDepthTest::WorldDepth,
				NewBatchVertexHint);

			AppendRoadRibbon(
				Site,
				ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY),
				ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY),
				FMath::Max(double(Edge.HalfWidth), 0.0),
				SegmentCount,
				bBuilt ? 1.0 : PlannedDashFraction,
				LiftCm,
				Entry.Batches[BatchIndex]);

			++Entry.DrawnEdgeCount;
			Entry.BuiltEdgeCount += bBuilt ? 1 : 0;
		}

		for (const FArcTownDebugDrawMeshBatch& Batch : Entry.Batches)
		{
			Entry.DrawnTriangleCount += Batch.Indices.Num() / 3;
		}

		const double NodeLiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);
		Entry.Nodes.Reserve(Snapshot.Nodes.Num());
		for (const FArcTownRoadNode& Node : Snapshot.Nodes)
		{
			const bool bFinished = Node.bFinished != 0;

			FArcTownDebugDrawPoint& Point = Entry.Nodes.AddDefaulted_GetRef();
			Point.Location = TownLocalToWorld(Site, ToDouble(Node.LocalXY), NodeLiftCm);
			Point.Color = bFinished ? FColor(255, 105, 90, 255) : FColor(105, 255, 150, 255);
			Point.ScreenSizePx = NodePointSizePx;
			Entry.FinishedNodeCount += bFinished ? 1 : 0;
		}
	}

	void DrawRoads(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache& GeometryCache,
		const FArcTownDebugDrawGeometryCache::FRoadEntry*& OutEntry)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		FArcTownDebugDrawGeometryKey Key;
		Key.Input = Frozen.Key;
		Key.ColorMode = uint8(Volume.RoadColor);
		Key.MaxDrawn = Volume.MaxDrawnRoadTriangles;
		Key.SurfaceLiftCm = Volume.DebugSurfaceLiftCm;
		Key.OverlayOpacityPercent = Volume.DebugOverlayOpacity;

		FArcTownDebugDrawGeometryCache::FRoadEntry* Entry = GeometryCache.RoadEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.RoadEntries);
			Entry = &GeometryCache.RoadEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildRoadGeometry(Volume, Frozen, *Entry);
		}

		for (const FArcTownDebugDrawMeshBatch& Batch : Entry->Batches)
		{
			if (Batch.Indices.Num() == 0)
			{
				continue;
			}
			UE::DebugDraw::Mesh(Scene, Batch.Vertices, Batch.Indices, Batch.Color, Batch.Depth);
		}

		for (const FArcTownDebugDrawPoint& Point : Entry->Nodes)
		{
			UE::DebugDraw::Point(Scene,
				Point.Location, Point.ScreenSizePx, Point.Color, UE::DebugDraw::EDepthTest::WorldDepth);
		}

		OutEntry = Entry;
	}

	void ProcessRoadLabels(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		const ELabelPass Pass,
		FLabelBudget& LabelBudget)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const FArcTownWorldSnapshot& Snapshot = Frozen.Input.SourceSnapshot;
		for (int32 EdgeIndex = 0; EdgeIndex < Snapshot.Edges.Num(); ++EdgeIndex)
		{
			const FArcTownRoadEdge& Edge = Snapshot.Edges[EdgeIndex];
			double LengthCm = 0.0;
			if (!IsDrawableEdge(Snapshot, Edge, LengthCm))
			{
				continue;
			}

			const FVector2D MidTownLocalXY = 0.5
				* (ToDouble(Snapshot.Nodes[Edge.NodeA].LocalXY) + ToDouble(Snapshot.Nodes[Edge.NodeB].LocalXY));
			const FVector WorldLocation = TownLocalToWorld(
				Site, MidTownLocalXY, MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay));
			if (Pass == ELabelPass::Offer)
			{
				LabelBudget.Offer(WorldLocation);
				continue;
			}

			if (LabelBudget.TryEnqueue(WorldLocation))
			{
				EmitLabel(Scene, WorldLocation, FColor::White,
					FString::Printf(TEXT("e%d n%d-n%d h%d traffic %.2f"),
						EdgeIndex, Edge.NodeA, Edge.NodeB, int32(Edge.Hierarchy), Edge.Traffic));
			}
		}
	}

	void AppendUnknownCatalogMarker(
		const FArcTownSiteSnapshot& Site,
		const FVector2D& CenterTownLocalXY,
		const double LiftCm,
		const float Thickness,
		TArray<UE::DebugDraw::FLineSubmission>& OutLines)
	{
		const FColor Color = MakeSentinelColor(255);

		const double ArmCm = FMath::Max(Site.SampleSpacing, 1.0);
		AppendDrapedTownLocalLine(Site, CenterTownLocalXY - FVector2D(ArmCm, ArmCm),
			CenterTownLocalXY + FVector2D(ArmCm, ArmCm), LiftCm, Color, Thickness, OutLines);
		AppendDrapedTownLocalLine(Site, CenterTownLocalXY - FVector2D(ArmCm, -ArmCm),
			CenterTownLocalXY + FVector2D(ArmCm, -ArmCm), LiftCm, Color, Thickness, OutLines);
		AppendDrapedRing(Site, CenterTownLocalXY, ArmCm, LiftCm, Color, Thickness, OutLines);
	}

	/** The drawn set is the leading run of the record array, so the label pass can bound itself the same way the geometry pass does. */
	int32 ResolveDrawnBuildingLimit(const AArcTownVolume& Volume, const int32 RecordCount)
	{
		return Volume.MaxDrawnBuildings > 0
			? FMath::Min(RecordCount, Volume.MaxDrawnBuildings)
			: RecordCount;
	}

	void BuildBuildingGeometry(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache::FBuildingEntry& Entry)
	{
		Entry.Boxes.Reset();
		Entry.Lines.Reset();
		Entry.MinBirthYear = 0;
		Entry.MaxBirthYear = 0;
		Entry.DrawnBuildingCount = 0;
		Entry.UnknownCatalogCount = 0;
		Entry.FirstUnknownCatalogIndex = INDEX_NONE;
		Entry.OrphanedFrontageCount = 0;

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const FArcTownWorldSnapshot& Snapshot = Frozen.Input.SourceSnapshot;
		const TArray<FArcTownFrozenBuildingEntry>& Catalog = Frozen.Input.Catalog;
		const float Thickness = Volume.DebugLineThicknessCm;
		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);
		const int32 DrawnLimit = ResolveDrawnBuildingLimit(Volume, Snapshot.Buildings.Num());
		const uint8 FillAlpha = ResolveFillAlpha(Volume);
		Entry.DroppedBuildingCount = Snapshot.Buildings.Num() - DrawnLimit;

		// Footprints are town-local centimetres, so the box has to carry the terrain scale into world extents.
		const double TerrainScale = GetTerrainScale(Site);
		const FQuat TerrainRotation = Site.TerrainToWorld.GetRotation();
		const FVector TerrainUp = Site.TerrainToWorld.TransformVectorNoScale(FVector::UpVector);

		int32 MinBirthYear = MAX_int32;
		int32 MaxBirthYear = MIN_int32;
		for (const FArcTownBuildingRecord& Record : Snapshot.Buildings)
		{
			MinBirthYear = FMath::Min(MinBirthYear, Record.BirthYear);
			MaxBirthYear = FMath::Max(MaxBirthYear, Record.BirthYear);
		}
		if (MaxBirthYear >= MinBirthYear)
		{
			Entry.MinBirthYear = MinBirthYear;
			Entry.MaxBirthYear = MaxBirthYear;
		}

		Entry.Boxes.Reserve(DrawnLimit);

		for (int32 RecordIndex = 0; RecordIndex < DrawnLimit; ++RecordIndex)
		{
			const FArcTownBuildingRecord& Record = Snapshot.Buildings[RecordIndex];
			const FVector2D CenterTownLocalXY = ToDouble(Record.LocalXY);
			if (!Catalog.IsValidIndex(Record.CatalogIndex))
			{
				AppendUnknownCatalogMarker(Site, CenterTownLocalXY, LiftCm, Thickness, Entry.Lines);
				++Entry.UnknownCatalogCount;
				if (Entry.FirstUnknownCatalogIndex == INDEX_NONE)
				{
					Entry.FirstUnknownCatalogIndex = Record.CatalogIndex;
				}
				continue;
			}

			const FArcTownFrozenBuildingEntry& CatalogEntry = Catalog[Record.CatalogIndex];

			const bool bFrontageBuilt = Snapshot.Edges.IsValidIndex(Record.EdgeIndex)
				&& Snapshot.Edges[Record.EdgeIndex].bBuilt != 0;
			Entry.OrphanedFrontageCount += bFrontageBuilt ? 0 : 1;

			FColor Color = FColor::White;
			switch (Volume.BuildingColor)
			{
			case EArcTownDebugBuildingColor::CatalogEntry:
				Color = MakeIndexColor(Record.CatalogIndex, FillAlpha);
				break;

			case EArcTownDebugBuildingColor::Frontage:
				Color = bFrontageBuilt ? FColor(80, 220, 110, FillAlpha) : FColor(230, 70, 70, FillAlpha);
				break;

			default:
				Color = MakeNormalisedColor(
					double(Record.BirthYear), double(Entry.MinBirthYear), double(Entry.MaxBirthYear), FillAlpha);
				break;
			}

			// The catalog authors no height, so the shorter footprint axis stands in for one within the storey band.
			const double WorldHeightCm = FMath::Clamp(
				FMath::Min(CatalogEntry.FootprintXY.X, CatalogEntry.FootprintXY.Y) * TerrainScale,
				SingleStoreyWorldHeightCm,
				TwoStoreyWorldHeightCm);

			FArcTownDebugDrawBox& Box = Entry.Boxes.AddDefaulted_GetRef();
			Box.Center = TownLocalToWorld(Site, CenterTownLocalXY, LiftCm)
				+ TerrainUp * (0.5 * WorldHeightCm);
			Box.Extent = FVector(
				0.5 * CatalogEntry.FootprintXY.X * TerrainScale,
				0.5 * CatalogEntry.FootprintXY.Y * TerrainScale,
				0.5 * WorldHeightCm);
			Box.Rotation = TerrainRotation * FQuat(FVector::UpVector, double(Record.Yaw));
			Box.Color = Color;

			++Entry.DrawnBuildingCount;
		}
	}

	void DrawBuildings(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache& GeometryCache,
		const FArcTownDebugDrawGeometryCache::FBuildingEntry*& OutEntry)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		FArcTownDebugDrawGeometryKey Key;
		Key.Input = Frozen.Key;
		Key.ColorMode = uint8(Volume.BuildingColor);
		Key.MaxDrawn = Volume.MaxDrawnBuildings;
		Key.SurfaceLiftCm = Volume.DebugSurfaceLiftCm;
		Key.LineThicknessCm = Volume.DebugLineThicknessCm;
		Key.OverlayOpacityPercent = Volume.DebugOverlayOpacity;

		FArcTownDebugDrawGeometryCache::FBuildingEntry* Entry = GeometryCache.BuildingEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.BuildingEntries);
			Entry = &GeometryCache.BuildingEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildBuildingGeometry(Volume, Frozen, *Entry);
		}

		UE_CLOGF(Entry->UnknownCatalogCount > 0
				&& ShouldLogOnce(EArcTownDebugDrawWarning::BuildingCatalogIndexUnknown, Volume,
					uint64(uint32(Entry->FirstUnknownCatalogIndex))),
			LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls holds %d buildings naming a catalog entry the frozen catalog of %d entries does not have, first %d; they draw as magenta crosses. Reset Simulation.",
			*Volume.GetName(), Entry->UnknownCatalogCount, Frozen.Input.Catalog.Num(),
			Entry->FirstUnknownCatalogIndex);
		if (Entry->UnknownCatalogCount == 0)
		{
			ClearLogLatches(EArcTownDebugDrawWarning::BuildingCatalogIndexUnknown, Volume);
		}

		UE_CLOGF(Entry->DroppedBuildingCount > 0
				&& ShouldLogOnce(EArcTownDebugDrawWarning::BuildingBudgetClamped, Volume),
			LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls holds more buildings than Max Drawn Buildings (%d), so the overlay dropped %d of them.",
			*Volume.GetName(), Volume.MaxDrawnBuildings, Entry->DroppedBuildingCount);
		if (Entry->DroppedBuildingCount == 0)
		{
			ClearLogLatch(EArcTownDebugDrawWarning::BuildingBudgetClamped, Volume);
		}

		for (const FArcTownDebugDrawBox& Box : Entry->Boxes)
		{
			UE::DebugDraw::SolidBox(Scene, Box.Center, Box.Extent, Box.Rotation, Box.Color,
				UE::DebugDraw::EDepthTest::WorldDepth);
			UE::DebugDraw::Box(Scene, Box.Center, Box.Extent, Box.Rotation, BuildingEdgeColor,
				0.0f, UE::DebugDraw::EDepthTest::WorldDepth);
		}

		UE::DebugDraw::Lines(Scene, Entry->Lines);

		OutEntry = Entry;
	}

	void ProcessBuildingLabels(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		const ELabelPass Pass,
		FLabelBudget& LabelBudget)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const TArray<FArcTownBuildingRecord>& Records = Frozen.Input.SourceSnapshot.Buildings;
		const int32 DrawnLimit = ResolveDrawnBuildingLimit(Volume, Records.Num());
		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);
		for (int32 RecordIndex = 0; RecordIndex < DrawnLimit; ++RecordIndex)
		{
			const FArcTownBuildingRecord& Record = Records[RecordIndex];
			const FVector WorldLocation = TownLocalToWorld(Site, ToDouble(Record.LocalXY), LiftCm);
			if (Pass == ELabelPass::Offer)
			{
				LabelBudget.Offer(WorldLocation);
				continue;
			}

			if (LabelBudget.TryEnqueue(WorldLocation))
			{
				EmitLabel(Scene, WorldLocation, FColor::White,
					FString::Printf(TEXT("c%d year %d"), Record.CatalogIndex, Record.BirthYear));
			}
		}
	}

	/** One sample spacing of ground per unit of strength, expressed in world centimetres like the lift. */
	double MakeAttractorStemWorldCm(const FArcTownSiteSnapshot& Site, const float Strength)
	{
		return FMath::Max(double(Strength), 0.0) * FMath::Max(Site.SampleSpacing, 1.0) * GetTerrainScale(Site);
	}

	void BuildAttractorGeometry(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache::FAttractorEntry& Entry)
	{
		Entry.Lines.Reset();
		Entry.DrawnAttractorCount = 0;
		Entry.SkippedAttractorCount = 0;

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		const FColor Color(255, 190, 70, 255);
		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);

		for (const FArcTownFrozenAttractor& Attractor : Frozen.Input.Attractors)
		{
			const FVector2D CenterTownLocalXY = ToDouble(Attractor.CenterTownLocalXY);
			if (!FMath::IsFinite(Attractor.InfluenceRadiusCm) || Attractor.InfluenceRadiusCm <= 0.0
				|| !FMath::IsFinite(Attractor.Strength))
			{
				++Entry.SkippedAttractorCount;
				continue;
			}

			AppendDrapedRing(Site, CenterTownLocalXY, Attractor.InfluenceRadiusCm, LiftCm,
				Color, Volume.DebugLineThicknessCm, Entry.Lines);

			const double StemCm = MakeAttractorStemWorldCm(Site, Attractor.Strength);
			Entry.Lines.Add(UE::DebugDraw::FLineSubmission{
				TownLocalToWorld(Site, CenterTownLocalXY, LiftCm),
				TownLocalToWorld(Site, CenterTownLocalXY, LiftCm + StemCm),
				Color,
				Volume.DebugLineThicknessCm,
				UE::DebugDraw::EDepthTest::WorldDepth});

			++Entry.DrawnAttractorCount;
		}
	}

	FString MakeAttractorLabel(const FArcTownFrozenAttractor& Attractor)
	{
		FString Name = Attractor.PathName;
		FString Trailing;
		if (Attractor.PathName.Split(TEXT("."), nullptr, &Trailing, ESearchCase::CaseSensitive, ESearchDir::FromEnd))
		{
			Name = Trailing;
		}

		return FString::Printf(TEXT("%s strength %.2f %s"),
			*Name, Attractor.Strength,
			Attractor.DistrictTags.IsEmpty() ? TEXT("(no district tag)") : *Attractor.DistrictTags.ToStringSimple());
	}

	void DrawAttractors(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache& GeometryCache,
		const FArcTownDebugDrawGeometryCache::FAttractorEntry*& OutEntry)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		FArcTownDebugDrawGeometryKey Key;
		Key.Input = Frozen.Key;
		Key.SurfaceLiftCm = Volume.DebugSurfaceLiftCm;
		Key.LineThicknessCm = Volume.DebugLineThicknessCm;

		FArcTownDebugDrawGeometryCache::FAttractorEntry* Entry = GeometryCache.AttractorEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.AttractorEntries);
			Entry = &GeometryCache.AttractorEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildAttractorGeometry(Volume, Frozen, *Entry);
		}

		UE_CLOGF(Entry->DrawnAttractorCount == 0
				&& ShouldLogOnce(EArcTownDebugDrawWarning::NoAttractors, Volume),
			LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls froze no usable attractor, so the attractor layer draws nothing; an attractor needs a positive influence radius and has to reach this town's bounds.",
			*Volume.GetName());
		if (Entry->DrawnAttractorCount > 0)
		{
			ClearLogLatch(EArcTownDebugDrawWarning::NoAttractors, Volume);
		}

		UE::DebugDraw::Lines(Scene, Entry->Lines);

		OutEntry = Entry;
	}

	void ProcessAttractorLabels(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		const ELabelPass Pass,
		FLabelBudget& LabelBudget)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		for (const FArcTownFrozenAttractor& Attractor : Frozen.Input.Attractors)
		{
			const double StemCm = MakeAttractorStemWorldCm(Site, Attractor.Strength);
			const FVector WorldLocation = TownLocalToWorld(
				Site, ToDouble(Attractor.CenterTownLocalXY),
				MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay) + StemCm);
			if (Pass == ELabelPass::Offer)
			{
				LabelBudget.Offer(WorldLocation);
				continue;
			}

			if (LabelBudget.TryEnqueue(WorldLocation))
			{
				EmitLabel(Scene, WorldLocation, FColor(255, 190, 70, 255), MakeAttractorLabel(Attractor));
			}
		}
	}

	int32 CountDrapedBoundaryLines(const FArcTownSiteSnapshot& Site, const FArcTownBlock& Block)
	{
		const int32 SideCount = Block.Boundary.Num();
		int32 LineCount = 0;
		for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
		{
			const FVector2D StartTownLocalXY = ToDouble(Block.Boundary[SideIndex]);
			const FVector2D EndTownLocalXY = ToDouble(Block.Boundary[(SideIndex + 1) % SideCount]);
			LineCount += ResolveDrapeSegmentCount((EndTownLocalXY - StartTownLocalXY).Size(), Site.SampleSpacing);
		}
		return LineCount;
	}

	/** Plots are not drawn because they depend on the scores and the placement order of a year that has
	 * not run yet. */
	void BuildBlockGeometry(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache::FBlockEntry& Entry)
	{
		Entry.Lines.Reset();
		Entry.DrawnBlockCount = 0;
		Entry.DrawnFrontageCount = 0;
		Entry.DroppedBlockCount = 0;
		Entry.DroppedFrontageCount = 0;

		const FArcTownSiteSnapshot& Site = Frozen.Input.Site;
		if (Site.SampleSpacing <= 0.0)
		{
			return;
		}

		const double MaxPlotDepthCm = ArcTownPlacement::MaxPlotDepthCm(
			Frozen.Input.Settings.Morphology, Frozen.Input.Catalog, Frozen.Input.SourceSnapshot.Year + 1);

		ArcTownRoadArrangement::FArrangement Arrangement;
		if (!ArcTownRoadArrangement::Build(
			Frozen.Input.SourceSnapshot, ArrangementSnapToleranceInSamples * Site.SampleSpacing, Arrangement))
		{
			return;
		}

		const double MinAreaCm2 =
			FMath::Max(Frozen.Input.Settings.Morphology.MinFrontageRunCm, 0.0) * MaxPlotDepthCm;

		TArray<FArcTownBlock> Blocks;
		FArcTownBlockRejections BlockRejections;
		ArcTownBlocks::ExtractBlocks(Arrangement, MinAreaCm2, Blocks, BlockRejections);

		TArray<FArcTownBlock> Frontage;
		FArcTownBlockRejections FrontageRejections;
		ArcTownBlocks::BuildFrontage(
			Arrangement, Frozen.Input.Settings.Traffic, MaxPlotDepthCm, MinAreaCm2, Frontage, FrontageRejections);

		const double LiftCm = MakeDrapedLiftCm(Volume, EDrapeLiftTier::Overlay);
		const float Thickness = Volume.DebugLineThicknessCm;

		const int32 LineBudget = FMath::Max(Volume.MaxDrawnBlockLines, 0);

		int32 UsedBlockLineCount = 0;
		int32 BlockIndex = 0;
		for (; BlockIndex < Blocks.Num(); ++BlockIndex)
		{
			const FArcTownBlock& Block = Blocks[BlockIndex];
			const int32 BlockLineCount = CountDrapedBoundaryLines(Site, Block);
			if (LineBudget > 0 && UsedBlockLineCount + BlockLineCount > LineBudget)
			{
				break;
			}
			UsedBlockLineCount += BlockLineCount;

			const int32 SideCount = Block.Boundary.Num();
			for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
			{
				AppendDrapedTownLocalLine(
					Site,
					ToDouble(Block.Boundary[SideIndex]),
					ToDouble(Block.Boundary[(SideIndex + 1) % SideCount]),
					LiftCm,
					BlockBoundaryColor,
					Thickness,
					Entry.Lines);
			}
			++Entry.DrawnBlockCount;
		}
		Entry.DroppedBlockCount = Blocks.Num() - BlockIndex;

		int32 UsedFrontageLineCount = 0;
		int32 FrontageIndex = 0;
		for (; FrontageIndex < Frontage.Num(); ++FrontageIndex)
		{
			const FArcTownBlock& FrontagePolygon = Frontage[FrontageIndex];
			const int32 FrontageLineCount = CountDrapedBoundaryLines(Site, FrontagePolygon);
			if (LineBudget > 0 && UsedFrontageLineCount + FrontageLineCount > LineBudget)
			{
				break;
			}
			UsedFrontageLineCount += FrontageLineCount;

			const int32 SideCount = FrontagePolygon.Boundary.Num();
			for (int32 SideIndex = 0; SideIndex < SideCount; ++SideIndex)
			{
				const bool bAttributed = FrontagePolygon.BoundaryEdgeIndex.IsValidIndex(SideIndex)
					&& FrontagePolygon.BoundaryEdgeIndex[SideIndex] != INDEX_NONE;
				AppendDrapedTownLocalLine(
					Site,
					ToDouble(FrontagePolygon.Boundary[SideIndex]),
					ToDouble(FrontagePolygon.Boundary[(SideIndex + 1) % SideCount]),
					LiftCm,
					bAttributed ? FrontageAttributedColor : FrontagePolygonColor,
					Thickness,
					Entry.Lines);
			}
			++Entry.DrawnFrontageCount;
		}
		Entry.DroppedFrontageCount = Frontage.Num() - FrontageIndex;
	}

	void DrawBlocks(
		const AArcTownVolume& Volume,
		const FArcTownDebugDrawFrozenCache::FEntry& Frozen,
		FArcTownDebugDrawGeometryCache& GeometryCache,
		const FArcTownDebugDrawGeometryCache::FBlockEntry*& OutEntry)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		FArcTownDebugDrawGeometryKey Key;
		Key.Input = Frozen.Key;
		Key.MaxDrawn = Volume.MaxDrawnBlockLines;
		Key.SurfaceLiftCm = Volume.DebugSurfaceLiftCm;
		Key.LineThicknessCm = Volume.DebugLineThicknessCm;

		FArcTownDebugDrawGeometryCache::FBlockEntry* Entry = GeometryCache.BlockEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.BlockEntries);
			Entry = &GeometryCache.BlockEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildBlockGeometry(Volume, Frozen, *Entry);
		}

		const bool bDropped = Entry->DroppedBlockCount > 0 || Entry->DroppedFrontageCount > 0;
		UE_CLOGF(bDropped && ShouldLogOnce(EArcTownDebugDrawWarning::BlockBudgetClamped, Volume),
			LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls draws more block and frontage-polygon lines than Max Drawn Block Lines (%d), so the overlay dropped %d blocks and %d frontage polygons.",
			*Volume.GetName(), Volume.MaxDrawnBlockLines, Entry->DroppedBlockCount, Entry->DroppedFrontageCount);
		if (!bDropped)
		{
			ClearLogLatch(EArcTownDebugDrawWarning::BlockBudgetClamped, Volume);
		}

		UE::DebugDraw::Lines(Scene, Entry->Lines);

		OutEntry = Entry;
	}

	struct FDrawnLayers
	{
		const FArcTownDebugDrawGeometryCache::FSiteFieldEntry* SiteField = nullptr;
		const FArcTownDebugDrawGeometryCache::FRoadEntry* Roads = nullptr;
		const FArcTownDebugDrawGeometryCache::FBuildingEntry* Buildings = nullptr;
		const FArcTownDebugDrawGeometryCache::FAttractorEntry* Attractors = nullptr;
		const FArcTownDebugDrawGeometryCache::FBlockEntry* Blocks = nullptr;
	};

	void DrawHeaderLabels(
		const AArcTownVolume& Volume,
		const FDrawnLayers& Layers,
		const FLabelBudget& LabelBudget)
	{
		const FSceneInterface* const Scene = VolumeScene(Volume);

		const FArcTownDebugDrawGeometryCache::FSiteFieldEntry* const SiteFieldEntry = Layers.SiteField;
		const UBoxComponent* const Bounds = Volume.VolumeBounds;
		if (Bounds == nullptr)
		{
			return;
		}

		const FVector Up = Bounds->GetComponentQuat().GetUpVector();
		FVector LabelLocation = Bounds->GetComponentLocation() + Up * Bounds->GetScaledBoxExtent().Z;

		const FArcTownWorldSnapshot* const Snapshot = Volume.SimulationState != nullptr
			? &Volume.SimulationState->Snapshot
			: nullptr;

		EmitLabel(Scene, LabelLocation, FColor::White, Snapshot != nullptr
			? FString::Printf(TEXT("%s: year %d, %d nodes, %d edges, %d buildings"),
				*Volume.GetName(), Snapshot->Year, Snapshot->Nodes.Num(), Snapshot->Edges.Num(),
				Snapshot->Buildings.Num())
			: FString::Printf(TEXT("%s: not simulated"), *Volume.GetName()));

		if (SiteFieldEntry != nullptr && SiteFieldEntry->DrawnCellCount > 0)
		{
			LabelLocation += Up * LabelStackSpacingCm;
			const FString Unit = GetSiteFieldUnit(Volume.SiteField);
			EmitLabel(Scene, LabelLocation, FColor::White, IsBinarySiteField(Volume.SiteField)
				? FString::Printf(TEXT("%s over %d cells"),
					*GetSiteFieldName(Volume.SiteField), SiteFieldEntry->DrawnCellCount)
				: FString::Printf(TEXT("%s %.3f to %.3f %s over %d cells"),
					*GetSiteFieldName(Volume.SiteField), SiteFieldEntry->MinValue, SiteFieldEntry->MaxValue,
					*Unit, SiteFieldEntry->DrawnCellCount));
		}

		if (Layers.Roads != nullptr)
		{
			LabelLocation += Up * LabelStackSpacingCm;
			FString RoadText = FString::Printf(
				TEXT("roads: %d edges, %d built, %d nodes, %d finished, drape %.0f cm, %d triangles"),
				Layers.Roads->DrawnEdgeCount, Layers.Roads->BuiltEdgeCount, Layers.Roads->Nodes.Num(),
				Layers.Roads->FinishedNodeCount, Layers.Roads->DrapeSpacingCm,
				Layers.Roads->DrawnTriangleCount);
			if (Volume.RoadColor == EArcTownDebugRoadColor::Traffic)
			{
				RoadText += FString::Printf(TEXT(", traffic %.2f to %.2f"),
					Layers.Roads->MinTraffic, Layers.Roads->MaxTraffic);
			}
			if (Layers.Roads->SkippedEdgeCount > 0)
			{
				RoadText += FString::Printf(TEXT(", %d unusable"), Layers.Roads->SkippedEdgeCount);
			}
			if (Layers.Roads->DroppedEdgeCount > 0)
			{
				RoadText += FString::Printf(TEXT(", %d dropped over Max Drawn Road Triangles (%d)"),
					Layers.Roads->DroppedEdgeCount, Volume.MaxDrawnRoadTriangles);
			}
			EmitLabel(Scene, LabelLocation, FColor::White, RoadText);
		}

		if (Layers.Buildings != nullptr)
		{
			LabelLocation += Up * LabelStackSpacingCm;
			FString BuildingText = FString::Printf(TEXT("buildings: %d drawn, years %d to %d, %d off a built street"),
				Layers.Buildings->DrawnBuildingCount, Layers.Buildings->MinBirthYear, Layers.Buildings->MaxBirthYear,
				Layers.Buildings->OrphanedFrontageCount);
			if (Layers.Buildings->UnknownCatalogCount > 0)
			{
				BuildingText += FString::Printf(TEXT(", %d naming an unknown catalog entry (first %d)"),
					Layers.Buildings->UnknownCatalogCount, Layers.Buildings->FirstUnknownCatalogIndex);
			}
			if (Layers.Buildings->DroppedBuildingCount > 0)
			{
				BuildingText += FString::Printf(TEXT(", %d dropped over Max Drawn Buildings (%d)"),
					Layers.Buildings->DroppedBuildingCount, Volume.MaxDrawnBuildings);
			}
			EmitLabel(Scene, LabelLocation, FColor::White, BuildingText);
		}

		if (Layers.Attractors != nullptr)
		{
			LabelLocation += Up * LabelStackSpacingCm;
			FString AttractorText = FString::Printf(TEXT("attractors: %d drawn"),
				Layers.Attractors->DrawnAttractorCount);
			if (Layers.Attractors->SkippedAttractorCount > 0)
			{
				AttractorText += FString::Printf(TEXT(", %d unusable"), Layers.Attractors->SkippedAttractorCount);
			}
			EmitLabel(Scene, LabelLocation, FColor::White, AttractorText);
		}

		if (Layers.Blocks != nullptr)
		{
			LabelLocation += Up * LabelStackSpacingCm;
			FString BlockText = FString::Printf(TEXT("blocks: %d drawn, %d frontage polygons drawn"),
				Layers.Blocks->DrawnBlockCount, Layers.Blocks->DrawnFrontageCount);
			if (Layers.Blocks->DroppedBlockCount > 0 || Layers.Blocks->DroppedFrontageCount > 0)
			{
				BlockText += FString::Printf(
					TEXT(", %d blocks and %d frontage polygons dropped over Max Drawn Block Lines (%d)"),
					Layers.Blocks->DroppedBlockCount, Layers.Blocks->DroppedFrontageCount, Volume.MaxDrawnBlockLines);
			}
			EmitLabel(Scene, LabelLocation, FColor::White, BlockText);
		}

		if (LabelBudget.Enqueued < LabelBudget.Requested)
		{
			LabelLocation += Up * LabelStackSpacingCm;
			EmitLabel(Scene, LabelLocation, FColor(255, 190, 70, 255), FString::Printf(
				TEXT("labels: %d of %d enqueued, capped by Max Drawn Labels (%d)"),
				LabelBudget.Enqueued, LabelBudget.Requested, Volume.MaxDrawnLabels));
		}
	}
}

namespace ArcTownDebugDraw
{
	void DrawVolume(
		const AArcTownVolume& Volume,
		TConstArrayView<TWeakObjectPtr<const AArcTownAttractorVolume>> CachedAttractors,
		FArcTownDebugDrawFrozenCache& FrozenCache,
		FArcTownDebugDrawGeometryCache& GeometryCache)
	{
		using namespace ArcTownDebugDrawPrivate;

		if (Volume.bDrawVolumeBounds)
		{
			DrawVolumeBounds(Volume);
		}

		const FArcTownSiteSnapshot& Site = Volume.SiteSnapshot;
		const bool bSiteUsable = HasUsableSamples(Site) && HasUsableTerrainTransform(Site);
		if (!bSiteUsable)
		{
			if (!Site.IsEmpty() && ShouldLogOnce(EArcTownDebugDrawWarning::TerrainScale, Volume))
			{
				UE_LOGF(LogArcTownDebugDraw, Warning,"ArcTownDebugDraw: %ls carries a sampled site the overlay cannot draw; it needs a full sample lattice and a positive uniform terrain scale.",
					*Volume.GetName());
			}

			if (Volume.bDrawLabels)
			{
				DrawHeaderLabels(Volume, FDrawnLayers(), FLabelBudget());
			}
			return;
		}
		ClearLogLatch(EArcTownDebugDrawWarning::TerrainScale, Volume);

		if (Volume.bDrawSiteGrid)
		{
			DrawSiteGrid(Volume);
		}

		FDrawnLayers Layers;
		FLabelBudget LabelBudget;
		LabelBudget.Limit = FMath::Max(1, Volume.MaxDrawnLabels);
		if (Volume.bDrawLabels)
		{
			const UE::DebugDraw::FFrameContext FrameContext = UE::DebugDraw::CaptureFrameContext();
			LabelBudget.bHasView = FrameContext.NumViews > 0;
			LabelBudget.ViewOrigin = FVector(FrameContext.ViewOrigin);
		}

		if (Volume.bDrawSiteField || Volume.bDrawTensorField || Volume.bDrawRoads || Volume.bDrawBuildings
			|| Volume.bDrawAttractors || Volume.bDrawBlocks)
		{
			const FArcTownDebugDrawFrozenCache::FEntry* const Frozen =
				GetFrozenEntry(Volume, CachedAttractors, FrozenCache);
			if (Frozen != nullptr)
			{
				if (Volume.bDrawSiteField)
				{
					DrawSiteField(Volume, *Frozen, GeometryCache, Layers.SiteField);
				}

				if (Volume.bDrawTensorField)
				{
					DrawTensorField(Volume, *Frozen, GeometryCache);
				}

				if (Volume.bDrawRoads)
				{
					DrawRoads(Volume, *Frozen, GeometryCache, Layers.Roads);
				}

				if (Volume.bDrawBuildings)
				{
					DrawBuildings(Volume, *Frozen, GeometryCache, Layers.Buildings);
				}

				if (Volume.bDrawAttractors)
				{
					DrawAttractors(Volume, *Frozen, GeometryCache, Layers.Attractors);
				}

				if (Volume.bDrawBlocks)
				{
					DrawBlocks(Volume, *Frozen, GeometryCache, Layers.Blocks);
				}

				if (Volume.bDrawLabels)
				{
					for (const ELabelPass Pass : {ELabelPass::Offer, ELabelPass::Enqueue})
					{
						if (Volume.bDrawRoads)
						{
							ProcessRoadLabels(Volume, *Frozen, Pass, LabelBudget);
						}

						if (Volume.bDrawBuildings)
						{
							ProcessBuildingLabels(Volume, *Frozen, Pass, LabelBudget);
						}

						if (Volume.bDrawAttractors)
						{
							ProcessAttractorLabels(Volume, *Frozen, Pass, LabelBudget);
						}

						if (Pass == ELabelPass::Offer)
						{
							LabelBudget.ResolveDistanceCut();
						}
					}
				}
			}
		}

		if (Volume.bDrawLabels)
		{
			DrawHeaderLabels(Volume, Layers, LabelBudget);
		}
	}

	void ResetLogLatches()
	{
		ArcTownDebugDrawPrivate::GetLatchedWarnings().Reset();
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
