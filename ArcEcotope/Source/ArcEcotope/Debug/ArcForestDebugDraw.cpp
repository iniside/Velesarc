// Copyright Lukasz Baran. All Rights Reserved.

#include "Debug/ArcForestDebugDraw.h"

#if UE_DEBUG_RENDERING_ENABLED

#include "ArcEcotopeModule.h"
#include "ArcTerrainActor.h"
#include "ArcTerrainSubsystem.h"
#include "Climate/ArcClimateVolume.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DataMaps/ArcTerrainDataMapTypes.h"
#include "Debug/ArcForestDebugDrawTypes.h"
#include "DebugDrawLabelPool.h"
#include "Determinism/ArcForestDeterministic.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Instances/ArcEcotopeInstancePartitionActor.h"
#include "Math/QuatRotationTranslationMatrix.h"
#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcForestCrownGeometry.h"
#include "Simulation/ArcForestDisturbanceNoise.h"
#include "Simulation/ArcForestDisturbanceSettings.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeTerrainSelection.h"
#include "Simulation/ArcForestWorldSnapshot.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "UObject/Class.h"

namespace ArcForestDebugDrawPrivate
{
	constexpr int32 TrunkSegments = 6;
	constexpr int32 CrownSegments = 6;
	constexpr int32 CrownLatitudeBands = CrownSegments / 2;
	constexpr int32 CellStandSegments = 8;
	constexpr int32 StandRampBuckets = 24;
	constexpr int32 MaxMeshBatches = 128;
	constexpr uint8 CrownAlpha = 110;
	constexpr uint8 CellStandAlpha = 140;
	constexpr uint8 SiteFieldAlpha = 170;
	constexpr double MinimumTrunkRadiusCm = 2.0;
	constexpr double MinimumCrownRadiusCm = 1.0;
	constexpr double CellStandReferenceCount = 64.0;
	constexpr double SiteFieldMaxSlopeDegrees = 90.0;
	constexpr int32 FallbackMaxAgeYears = 300;
	constexpr double ChunkGridSurfaceLiftCm = 20.0;
	constexpr int32 DisturbanceRingSegments = 64;
	constexpr float DisturbanceThinRingFraction = 0.25f;
	constexpr uint8 DisturbanceLegendRingAlpha = 90;
	constexpr double DisturbanceFalloffStartFraction = 0.6;
	constexpr double DisturbanceProbabilityQuantum = 1000000.0;
	constexpr double DisturbancePreviewSurfaceLiftCm = 20.0;
	constexpr double TerrainDataMapSurfaceLiftCm = 20.0;
	constexpr int32 MaxTerrainDataMapUnreadRetryPlateaus = 10;
	constexpr int32 SheetShadeBuckets = 32;
	constexpr int32 SheetRampBuckets = 127;
	constexpr double MaxDisturbancePreviewStrideCells = 65536.0;
	constexpr int32 MaxSheetLabelCells = 1500;

	static_assert(SheetRampBuckets < MaxMeshBatches, "A sheet's shade buckets must leave a free slot in the mesh batch table.");
	static_assert(SheetShadeBuckets < MaxMeshBatches, "A sheet's shade buckets must leave a free slot in the mesh batch table.");

	template <typename VolumeType, typename EntryType>
	void PruneStaleVolumeEntries(TMap<TWeakObjectPtr<const VolumeType>, EntryType>& Entries)
	{
		TArray<TWeakObjectPtr<const VolumeType>> StaleKeys;
		for (const TPair<TWeakObjectPtr<const VolumeType>, EntryType>& Pair : Entries)
		{
			if (!Pair.Key.IsValid())
			{
				StaleKeys.Add(Pair.Key);
			}
		}
		for (const TWeakObjectPtr<const VolumeType>& StaleKey : StaleKeys)
		{
			Entries.Remove(StaleKey);
		}
	}

#if WITH_EDITOR
	TAutoConsoleVariable<int32> CVarDebugDrawReaderBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.DebugDrawBudgetMiB"),
		64,
		TEXT("Cache budget in MiB for ArcForest debug-overlay site reads; clamped to at least 1 MiB."),
		ECVF_Default);

	// FArcEcotopeSiteMetadata::GetFieldLevel leaves these two at level 0, so their chunk views index base cells
	// directly while every scalar field's view indexes its own mip.
	constexpr uint32 SurfaceFieldMask =
		(1u << uint32(EArcEcotopeSiteField::LocalSurfaceZ)) | (1u << uint32(EArcEcotopeSiteField::ValidSurface));

	bool TryGetSiteFieldReaderEntry(
		const AArcEcotopeVolume& Volume,
		const FArcForestDebugDrawContext& Context,
		FArcForestDebugDrawSiteReaderCache& Cache,
		const FArcForestDebugDrawSiteReaderCache::FEntry*& OutEntry,
		FString& OutError)
	{
		OutEntry = nullptr;

		PruneStaleVolumeEntries(Cache.Entries);

		const uint64 SiteSnapshotHash = Context.Metadata.SiteSnapshotHash;
		FArcForestDebugDrawSiteReaderCache::FEntry& Entry = Cache.Entries.FindOrAdd(&Volume);
		if (Entry.SiteSnapshotHash != SiteSnapshotHash || (!Entry.Reader.IsOpen() && !Entry.bOpenFailed))
		{
			Entry = FArcForestDebugDrawSiteReaderCache::FEntry();
			Entry.SiteSnapshotHash = SiteSnapshotHash;

			FArcEcotopeFrozenSiteSourcePtr FrozenSource;
			FText FreezeError;
			if (!Volume.FreezeSiteChunkSource(FrozenSource, FreezeError))
			{
				Entry.bOpenFailed = true;
				OutError = FreezeError.ToString();
				return false;
			}

			const int64 BudgetBytes = int64(FMath::Max(CVarDebugDrawReaderBudgetMiB.GetValueOnGameThread(), 1))
				* 1024ll * 1024ll;
			FString ReaderError;
			if (!Entry.Reader.OpenEditorBulk(FrozenSource, BudgetBytes, ReaderError))
			{
				Entry.bOpenFailed = true;
				OutError = ReaderError;
				return false;
			}

			Entry.FrozenSource = MoveTemp(FrozenSource);
		}

		if (Entry.bOpenFailed || !Entry.FrozenSource.IsValid() || !Entry.Reader.IsOpen())
		{
			return false;
		}

		OutEntry = &Entry;
		return true;
	}
#endif

	enum class EArcForestDebugDrawWarning : uint8
	{
		NonUniformScale,
		MissingMetadata,
		SiteFieldMetadata,
		SiteFieldValues,
		SiteFieldOwnedByClimate,
		SiteFieldStrideClamped,
		SiteFieldLabelsClamped,
		ClimateFieldSelection,
		ClimateFieldMetadata,
		ClimateFieldValues,
		ClimateFieldStrideClamped,
		ClimateFieldLabelsClamped,
		TerrainDataMapMetadata,
		TerrainDataMapNoTerrain,
		TerrainDataMapTerrain,
		TerrainDataMapStrideClamped,
		TerrainDataMapValues,
		TerrainDataMapLabelsClamped,
		DisturbancePreviewMetadata,
		DisturbancePreviewStrideClamped,
		DisturbancePreviewUnresolvable,
		DisturbancePreviewLabelsClamped,
		DisturbancePreviewFoundYear,
		DisturbancePreviewNoEventInWindow,
		ChunkLayout,
		SurfaceHeight,
	};

	enum class EArcForestDebugDrawSurfaceLayer : uint64
	{
		Cohorts,
		Patches,
		ChunkGrid,
	};

	struct FArcForestDebugDrawWarningKey
	{
		EArcForestDebugDrawWarning Kind = EArcForestDebugDrawWarning::NonUniformScale;
		FGuid VolumeId;
		uint64 Payload = 0;

		bool operator==(const FArcForestDebugDrawWarningKey& Other) const
		{
			return Kind == Other.Kind && VolumeId == Other.VolumeId && Payload == Other.Payload;
		}

		friend uint32 GetTypeHash(const FArcForestDebugDrawWarningKey& Key)
		{
			return HashCombine(
				HashCombine(GetTypeHash(uint8(Key.Kind)), GetTypeHash(Key.VolumeId)),
				GetTypeHash(Key.Payload));
		}
	};

	TSet<FArcForestDebugDrawWarningKey>& GetLatchedWarnings()
	{
		static TSet<FArcForestDebugDrawWarningKey> LatchedWarnings;
		return LatchedWarnings;
	}

	bool ShouldLogOnce(const EArcForestDebugDrawWarning Kind, const FGuid& VolumeId, const uint64 Payload = 0)
	{
		bool bAlreadyLatched = false;
		GetLatchedWarnings().Add(FArcForestDebugDrawWarningKey{Kind, VolumeId, Payload}, &bAlreadyLatched);
		return !bAlreadyLatched;
	}

	void ClearLogLatch(const EArcForestDebugDrawWarning Kind, const FGuid& VolumeId, const uint64 Payload = 0)
	{
		GetLatchedWarnings().Remove(FArcForestDebugDrawWarningKey{Kind, VolumeId, Payload});
	}

	bool TryBuildContext(const AArcEcotopeVolume& Volume, FArcForestDebugDrawContext& OutContext)
	{
		const FTransform VolumeTransform = Volume.GetActorTransform();
		const FVector Scale = VolumeTransform.GetScale3D();
		if (!(Scale.X > 0.0) || !FMath::IsNearlyEqual(Scale.X, Scale.Y) || !FMath::IsNearlyEqual(Scale.Y, Scale.Z))
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::NonUniformScale, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls does not have a positive uniform scale; the overlay does not support it and draws nothing.",
					*Volume.GetName());
			}
			return false;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::NonUniformScale, Volume.VolumeId);

		OutContext.Volume = &Volume;
		OutContext.VolumeTransform = VolumeTransform;
		OutContext.UniformScale = Scale.GetAbsMax();
		OutContext.Metadata = Volume.GetActiveSiteMetadata();
		OutContext.bMetadataValid = OutContext.Metadata.GetCellCount() > 0 && OutContext.Metadata.LocalBounds.IsValid;
		return true;
	}

	FVector GetWorldUp(const FArcForestDebugDrawContext& Context)
	{
		return Context.VolumeTransform.TransformVectorNoScale(FVector::UpVector);
	}

	struct FStandSpeciesFilter
	{
		bool bUseAllowList = false;
		TSet<FGuid> AllowedSpeciesIds;
		TSet<FGuid> GroundCoverSpeciesIds;

		bool Allows(const FGuid& SpeciesId) const
		{
			if (bUseAllowList && !AllowedSpeciesIds.Contains(SpeciesId))
			{
				return false;
			}

			return !GroundCoverSpeciesIds.Contains(SpeciesId);
		}
	};

	FStandSpeciesFilter MakeStandSpeciesFilter(const AArcEcotopeVolume& Volume)
	{
		FStandSpeciesFilter Filter;
		Filter.bUseAllowList = !Volume.DebugSpeciesFilter.IsEmpty();
		for (const TObjectPtr<UArcEcotopeSpeciesModel>& Species : Volume.DebugSpeciesFilter)
		{
			if (Species != nullptr)
			{
				Filter.AllowedSpeciesIds.Add(Species->SpeciesId);
			}
		}

		if (!Volume.bDrawGroundCoverSpecies)
		{
			for (const FArcEcotopeSpeciesSetup& Setup : Volume.GetEcotopeOrDefault().SpeciesSetups)
			{
				const UArcEcotopeSpeciesModel* SpeciesModel = Setup.Species;
				if (SpeciesModel != nullptr && !SpeciesModel->Individualizes())
				{
					Filter.GroundCoverSpeciesIds.Add(SpeciesModel->SpeciesId);
				}
			}
		}

		return Filter;
	}

	uint32 MakeStandSpeciesFilterHash(const AArcEcotopeVolume& Volume)
	{
		uint32 Hash = GetTypeHash(Volume.DebugSpeciesFilter.Num());
		for (const TObjectPtr<UArcEcotopeSpeciesModel>& Species : Volume.DebugSpeciesFilter)
		{
			Hash = HashCombine(Hash, Species != nullptr ? GetTypeHash(Species->SpeciesId) : 0u);
		}

		return Hash;
	}

	template <typename RecordType, typename IsDrawableType>
	void GatherFilteredRecords(
		const TArray<RecordType>& Records,
		const int32 MaxDrawn,
		const IsDrawableType& IsDrawable,
		TArray<int32>& OutIndices)
	{
		OutIndices.Reset();
		for (int32 RecordIndex = 0; RecordIndex < Records.Num(); ++RecordIndex)
		{
			if (IsDrawable(Records[RecordIndex]))
			{
				OutIndices.Add(RecordIndex);
				if (MaxDrawn > 0 && OutIndices.Num() >= MaxDrawn)
				{
					break;
				}
			}
		}
	}

	class FSurfaceHeightSampler
	{
	public:
		FSurfaceHeightSampler(
			const FArcForestDebugDrawContext& InContext,
			const AArcEcotopeVolume& InVolume,
			FArcForestDebugDrawSiteReaderCache& InReaderCache)
			: Context(InContext)
			, Volume(InVolume)
			, ReaderCache(InReaderCache)
			, FloorZCm(InContext.Metadata.LocalBounds.Min.Z)
		{
		}

		void BeginLayer()
		{
			LayerSampleCount = 0;
			LayerResolvedCount = 0;
		}

		bool HasResolvedThisLayer() const { return LayerResolvedCount > 0; }
		bool HasFlooredEverySampleThisLayer() const { return LayerSampleCount > 0 && LayerResolvedCount == 0; }
		double GetFloorZCm() const { return FloorZCm; }

		bool TryGetLocalSurfaceZCm(const FVector2D& LocalXY, double& OutZCm)
		{
			++LayerSampleCount;
			if (!TryReadLocalSurfaceZCm(LocalXY, OutZCm))
			{
				return false;
			}

			++LayerResolvedCount;
			return true;
		}

		double GetLocalSurfaceZCmOrFloor(const FVector2D& LocalXY)
		{
			double SurfaceZCm = FloorZCm;
			TryGetLocalSurfaceZCm(LocalXY, SurfaceZCm);
			return SurfaceZCm;
		}

	private:
#if WITH_EDITOR
		struct FChunkSurface
		{
			FIntRect BaseCoreRect;
			FArcEcotopeSiteChunkLease Lease;
			bool bUsable = false;
		};
#endif

		bool TryReadLocalSurfaceZCm(const FVector2D& LocalXY, double& OutZCm)
		{
			int32 BaseCellIndex = INDEX_NONE;
			if (!Context.Metadata.TryGetCellIndexForLocalPosition(LocalXY, BaseCellIndex))
			{
				return false;
			}

#if WITH_EDITOR
			FIntPoint BaseCoordinates = FIntPoint::ZeroValue;
			if (Context.Metadata.TryGetCellCoordinates(BaseCellIndex, BaseCoordinates))
			{
				return TryReadChunkSurfaceZCm(BaseCoordinates, OutZCm);
			}
#endif

			return false;
		}

#if WITH_EDITOR
		bool TryReadChunkSurfaceZCm(const FIntPoint& BaseCoordinates, double& OutZCm)
		{
			const FChunkSurface* Surface = FindOrAcquireChunkSurface(BaseCoordinates);
			if (Surface == nullptr || !Surface->bUsable)
			{
				return false;
			}

			const FArcEcotopeSiteChunkView& View = Surface->Lease.GetView();
			const uint8* ValidValue = View.ValidSurface.TryGet(BaseCoordinates.X, BaseCoordinates.Y);
			const float* SurfaceZValue = View.LocalSurfaceZ.TryGet(BaseCoordinates.X, BaseCoordinates.Y);
			if (ValidValue == nullptr || *ValidValue == 0 || SurfaceZValue == nullptr)
			{
				return false;
			}

			OutZCm = double(*SurfaceZValue);
			return true;
		}

		const FChunkSurface* FindOrAcquireChunkSurface(const FIntPoint& BaseCoordinates)
		{
			if (ChunkSurfaces.IsValidIndex(LastChunkSurfaceIndex)
				&& ChunkSurfaces[LastChunkSurfaceIndex].BaseCoreRect.Contains(BaseCoordinates))
			{
				return &ChunkSurfaces[LastChunkSurfaceIndex];
			}

			for (int32 SurfaceIndex = 0; SurfaceIndex < ChunkSurfaces.Num(); ++SurfaceIndex)
			{
				if (ChunkSurfaces[SurfaceIndex].BaseCoreRect.Contains(BaseCoordinates))
				{
					LastChunkSurfaceIndex = SurfaceIndex;
					return &ChunkSurfaces[SurfaceIndex];
				}
			}

			EnsureReaderEntry();
			if (ReaderEntry == nullptr)
			{
				return nullptr;
			}

			for (const FArcEcotopeChunkRect& Chunk : ReaderEntry->FrozenSource->Layout.GetChunks())
			{
				if (!Chunk.BaseCoreRect.Contains(BaseCoordinates))
				{
					continue;
				}

				FChunkSurface Surface;
				Surface.BaseCoreRect = Chunk.BaseCoreRect;

				FString AcquireError;
				Surface.bUsable =
					ReaderEntry->Reader.AcquireFields(Chunk.Id, 0, SurfaceFieldMask, Surface.Lease, AcquireError)
					&& Surface.Lease.GetView().LocalSurfaceZ.IsValid()
					&& Surface.Lease.GetView().ValidSurface.IsValid();

				LastChunkSurfaceIndex = ChunkSurfaces.Add(MoveTemp(Surface));
				return &ChunkSurfaces[LastChunkSurfaceIndex];
			}

			return nullptr;
		}

		void EnsureReaderEntry()
		{
			if (bReaderEntryResolved)
			{
				return;
			}
			bReaderEntryResolved = true;

			const FArcForestDebugDrawSiteReaderCache::FEntry* Entry = nullptr;
			FString ReaderError;
			if (!TryGetSiteFieldReaderEntry(Volume, Context, ReaderCache, Entry, ReaderError))
			{
				return;
			}

			if (Entry->FrozenSource->Layout.GetBaseDimensions() != Context.Metadata.GridDimensions)
			{
				return;
			}

			ReaderEntry = Entry;
		}
#endif // WITH_EDITOR

		const FArcForestDebugDrawContext& Context;
		const AArcEcotopeVolume& Volume;
		FArcForestDebugDrawSiteReaderCache& ReaderCache;
		double FloorZCm = 0.0;
		int32 LayerSampleCount = 0;
		int32 LayerResolvedCount = 0;
#if WITH_EDITOR
		TArray<FChunkSurface> ChunkSurfaces;
		const FArcForestDebugDrawSiteReaderCache::FEntry* ReaderEntry = nullptr;
		int32 LastChunkSurfaceIndex = INDEX_NONE;
		bool bReaderEntryResolved = false;
#endif
	};

	void ReportSurfaceSamples(
		const AArcEcotopeVolume& Volume,
		const FSurfaceHeightSampler& Sampler,
		const EArcForestDebugDrawSurfaceLayer Layer,
		const TCHAR* LayerName)
	{
		const uint64 Payload = uint64(Layer);
		if (Sampler.HasResolvedThisLayer())
		{
			ClearLogLatch(EArcForestDebugDrawWarning::SurfaceHeight, Volume.VolumeId, Payload);
			return;
		}

		if (Sampler.HasFlooredEverySampleThisLayer()
			&& ShouldLogOnce(EArcForestDebugDrawWarning::SurfaceHeight, Volume.VolumeId, Payload))
		{
			UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls read no surface height from the baked site chunks, so the %ls layer floored every sample to the volume bounds bottom at local Z %.0f cm.",
				*Volume.GetName(), LayerName, Sampler.GetFloorZCm());
		}
	}

	bool TryGetRecordCellAnchorLocal(
		const FArcForestDebugDrawContext& Context,
		FSurfaceHeightSampler& Sampler,
		const int32 CellIndex,
		FVector& OutLocalAnchor)
	{
		FVector2D MinLocal = FVector2D::ZeroVector;
		FVector2D MaxLocal = FVector2D::ZeroVector;
		if (!Context.Metadata.TryGetLevelCellRect(CellIndex, Context.Metadata.RecordGridLevel, MinLocal, MaxLocal))
		{
			return false;
		}

		const FVector2D CenterLocal = (MinLocal + MaxLocal) * 0.5;
		OutLocalAnchor = FVector(CenterLocal.X, CenterLocal.Y, Sampler.GetLocalSurfaceZCmOrFloor(CenterLocal));
		return true;
	}

	TMap<FGuid, int32> BuildSpeciesMaxAgeYearsCache(const AArcEcotopeVolume& Volume)
	{
		TMap<FGuid, int32> Cache;
		Cache.Reserve(Volume.GetEcotopeOrDefault().SpeciesSetups.Num());
		for (const FArcEcotopeSpeciesSetup& Setup : Volume.GetEcotopeOrDefault().SpeciesSetups)
		{
			const UArcEcotopeSpeciesModel* SpeciesModel = Setup.Species;
			if (SpeciesModel == nullptr)
			{
				continue;
			}

			ArcForestSpeciesModel::FArcForestDerivedEcology Derived;
			FText DerivationError;
			const int32 MaxAgeYears = ArcForestSpeciesModel::DeriveEcology(*SpeciesModel, Derived, DerivationError)
				? FMath::Max(Derived.MaxAgeYears, 1)
				: FallbackMaxAgeYears;
			Cache.Add(SpeciesModel->SpeciesId, MaxAgeYears);
		}

		return Cache;
	}

	int32 ResolveMaxAgeYears(const TMap<FGuid, int32>& SpeciesMaxAgeYearsCache, const FGuid& SpeciesId)
	{
		if (const int32* MaxAgeYears = SpeciesMaxAgeYearsCache.Find(SpeciesId))
		{
			return *MaxAgeYears;
		}

		return FallbackMaxAgeYears;
	}

	// DebugDraw renders label text as Latin-1 and turns anything else into '?', so every label string stays ASCII.
	FString MakeSpeciesShortId(const FGuid& SpeciesId)
	{
		return SpeciesId.ToString(EGuidFormats::Digits).Left(8);
	}

	/** Scene key every submission carries, so the shapes stay in the volume's own world. */
	const FSceneInterface* ActorScene(const AActor& Actor)
	{
		return UE::DebugDraw::GetSceneKey(Actor.GetWorld());
	}

	void EmitLabel(const FSceneInterface* Scene, const FVector& WorldLocation, const FColor& Color, const FString& Description)
	{
		FColor OpaqueColor = Color;
		OpaqueColor.A = 255;
		UE::DebugDraw::EnqueueLabel(Scene, WorldLocation, OpaqueColor, Description, UE::DebugDraw::EFrustumCullPolicy::Apply);
	}

	FColor MakeSpeciesColor(const FGuid& SpeciesId, const uint8 Alpha)
	{
		const uint32 SpeciesHash = GetTypeHash(SpeciesId);
		FColor Color = FLinearColor::MakeFromHSV8(static_cast<uint8>(SpeciesHash & 0xFFu), 190, 235).ToFColor(true);
		Color.A = Alpha;
		return Color;
	}

	double QuantizeStandRamp01(const double Value01)
	{
		const int32 Bucket = FMath::Clamp(
			int32(FMath::Clamp(Value01, 0.0, 1.0) * double(StandRampBuckets)), 0, StandRampBuckets - 1);
		return (double(Bucket) + 0.5) / double(StandRampBuckets);
	}

	FColor MakeStandColor(
		const TMap<FGuid, int32>& SpeciesMaxAgeYearsCache,
		const EArcForestDebugStandColor Mode,
		const FGuid& SpeciesId,
		const float Vigor,
		const int32 AgeYears,
		const uint8 Alpha)
	{
		switch (Mode)
		{
		case EArcForestDebugStandColor::LifeStatus:
			return FColor(70, 190, 80, Alpha);

		case EArcForestDebugStandColor::Vigor:
		{
			const float ClampedVigor = float(QuantizeStandRamp01(double(Vigor)));
			FColor Color = FLinearColor(1.0f - ClampedVigor, ClampedVigor, 0.05f).ToFColor(true);
			Color.A = Alpha;
			return Color;
		}

		case EArcForestDebugStandColor::Age:
		{
			const double AgeFraction = QuantizeStandRamp01(
				double(AgeYears) / double(ResolveMaxAgeYears(SpeciesMaxAgeYearsCache, SpeciesId)));
			FColor Color = FMath::Lerp(
				FLinearColor(0.20f, 0.85f, 0.35f), FLinearColor(0.85f, 0.30f, 0.10f), float(AgeFraction)).ToFColor(true);
			Color.A = Alpha;
			return Color;
		}

		case EArcForestDebugStandColor::Species:
		default:
			return MakeSpeciesColor(SpeciesId, Alpha);
		}
	}

	FLinearColor MakeDeadwoodBaseColor(const EArcForestDeadwoodType Type)
	{
		switch (Type)
		{
		case EArcForestDeadwoodType::Snag:
			return FLinearColor(0.78f, 0.64f, 0.22f);
		case EArcForestDeadwoodType::Log:
			return FLinearColor(0.42f, 0.24f, 0.10f);
		case EArcForestDeadwoodType::Stump:
			return FLinearColor(0.78f, 0.16f, 0.28f);
		case EArcForestDeadwoodType::Uprooted:
			return FLinearColor(0.46f, 0.22f, 0.72f);
		case EArcForestDeadwoodType::Branch:
			return FLinearColor(0.16f, 0.62f, 0.42f);
		case EArcForestDeadwoodType::MAX:
			break;
		}
		return FLinearColor::White;
	}

	FColor MakeDeadwoodColor(const EArcForestDeadwoodType Type, const uint8 DecayClass)
	{
		const float DecayFactor = FMath::Clamp(1.0f - 0.15f * float(DecayClass), 0.35f, 1.0f);
		FColor Color = (MakeDeadwoodBaseColor(Type) * DecayFactor).ToFColor(true);
		Color.A = 255;
		return Color;
	}

	void DrawVolumeBounds(const AArcEcotopeVolume& Volume, const double UniformScale)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		const UBoxComponent* Bounds = Volume.VolumeBounds;
		if (Bounds == nullptr)
		{
			return;
		}

		UE::DebugDraw::Box(Scene, Bounds->GetComponentLocation(), Bounds->GetScaledBoxExtent(), Bounds->GetComponentQuat(),
			FColor::White, float(double(Volume.DebugLineThicknessCm) * UniformScale),
			UE::DebugDraw::EDepthTest::WorldDepth);
	}

	FArcForestDebugDrawGeometryKey MakeStandGeometryKey(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		const int32 MaxDrawn)
	{
		FArcForestDebugDrawGeometryKey Key;
		Key.VolumeId = Volume.VolumeId;
		Key.VolumeTransform = Context.VolumeTransform;
		Key.MaxDrawn = MaxDrawn;
		Key.bDrawLabels = Volume.bDrawLabels;
		Key.StateChecksum = Snapshot.StateChecksum;
		Key.SpeciesFilterHash = MakeStandSpeciesFilterHash(Volume);
		Key.StandColorMode = uint8(Volume.StandColorMode);
		Key.bDrawIndividualCrowns = Volume.bDrawIndividualCrowns;
		Key.bDrawGroundCoverSpecies = Volume.bDrawGroundCoverSpecies;
		return Key;
	}

	uint64 MakeMeshBatchKey(const FColor& Color, const UE::DebugDraw::EDepthTest Depth)
	{
		return uint64(Color.ToPackedARGB()) | (uint64(Depth) << 32);
	}

	template <typename EntryType>
	int32 FindNearestMeshBatch(
		const EntryType& Entry,
		const FColor& Color,
		const UE::DebugDraw::EDepthTest Depth)
	{
		int32 NearestSameAlpha = INDEX_NONE;
		int32 BestSameAlphaDistance = MAX_int32;
		int32 NearestAnyAlpha = INDEX_NONE;
		int32 BestAnyAlphaDistance = MAX_int32;

		for (int32 BatchIndex = 0; BatchIndex < Entry.Batches.Num(); ++BatchIndex)
		{
			const FArcForestDebugDrawMeshBatch& Batch = Entry.Batches[BatchIndex];
			if (Batch.Depth != Depth)
			{
				continue;
			}

			const int32 Distance = FMath::Square(int32(Batch.Color.R) - int32(Color.R))
				+ FMath::Square(int32(Batch.Color.G) - int32(Color.G))
				+ FMath::Square(int32(Batch.Color.B) - int32(Color.B));
			if (Batch.Color.A == Color.A && Distance < BestSameAlphaDistance)
			{
				BestSameAlphaDistance = Distance;
				NearestSameAlpha = BatchIndex;
			}
			if (Distance < BestAnyAlphaDistance)
			{
				BestAnyAlphaDistance = Distance;
				NearestAnyAlpha = BatchIndex;
			}
		}

		if (NearestSameAlpha != INDEX_NONE)
		{
			return NearestSameAlpha;
		}
		return NearestAnyAlpha != INDEX_NONE ? NearestAnyAlpha : 0;
	}

	template <typename EntryType>
	int32 FindOrAddMeshBatch(
		EntryType& Entry,
		const FColor& Color,
		const UE::DebugDraw::EDepthTest Depth,
		const int32 NewBatchVertexHint = 0)
	{
		const uint64 BatchKey = MakeMeshBatchKey(Color, Depth);
		if (const int32* ExistingIndex = Entry.BatchIndexByKey.Find(BatchKey))
		{
			return *ExistingIndex;
		}

		if (Entry.Batches.Num() >= MaxMeshBatches)
		{
			const int32 FoldedIndex = FindNearestMeshBatch(Entry, Color, Depth);
			Entry.BatchIndexByKey.Add(BatchKey, FoldedIndex);
			return FoldedIndex;
		}

		const int32 NewIndex = Entry.Batches.Num();
		FArcForestDebugDrawMeshBatch& Batch = Entry.Batches.AddDefaulted_GetRef();
		Batch.Color = Color;
		Batch.Depth = Depth;
		if (NewBatchVertexHint > 0)
		{
			Batch.Vertices.Reserve(NewBatchVertexHint);
			Batch.Indices.Reserve(NewBatchVertexHint + NewBatchVertexHint / 2);
		}
		Entry.BatchIndexByKey.Add(BatchKey, NewIndex);
		return NewIndex;
	}

	void BuildUnitRing(const int32 Segments, TArray<FVector2D>& OutRing)
	{
		OutRing.SetNumUninitialized(Segments + 1);
		const double Step = UE_DOUBLE_TWO_PI / double(Segments);
		for (int32 Index = 0; Index < Segments; ++Index)
		{
			double SinValue = 0.0;
			double CosValue = 0.0;
			FMath::SinCos(&SinValue, &CosValue, Step * double(Index));
			OutRing[Index] = FVector2D(CosValue, SinValue);
		}
		OutRing[Segments] = OutRing[0];
	}

	void AppendCylinderMesh(
		FArcForestDebugDrawMeshBatch& Batch,
		const FVector& Start,
		const FVector& End,
		const double Radius,
		const TConstArrayView<FVector2D> Ring)
	{
		const int32 Segments = Ring.Num() - 1;
		if (Segments < 3)
		{
			return;
		}

		FVector Axis = (End - Start).GetSafeNormal();
		if (Axis.IsZero())
		{
			Axis = FVector(0.0, 0.0, 1.0);
		}
		FVector Perp1 = FVector::ZeroVector;
		FVector Perp2 = FVector::ZeroVector;
		Axis.FindBestAxisVectors(Perp1, Perp2);

		const FVector Perp1Scaled = Perp1 * Radius;
		const FVector Perp2Scaled = Perp2 * Radius;

		const int32 BaseVertex = Batch.Vertices.Num();
		Batch.Vertices.Add(Start);
		Batch.Vertices.Add(End);
		for (int32 SegmentIndex = 0; SegmentIndex < Segments; ++SegmentIndex)
		{
			Batch.Vertices.Add(Start + Perp1Scaled * Ring[SegmentIndex].X + Perp2Scaled * Ring[SegmentIndex].Y);
		}
		for (int32 SegmentIndex = 0; SegmentIndex < Segments; ++SegmentIndex)
		{
			Batch.Vertices.Add(End + Perp1Scaled * Ring[SegmentIndex].X + Perp2Scaled * Ring[SegmentIndex].Y);
		}

		const int32 StartVertex = BaseVertex;
		const int32 EndVertex = BaseVertex + 1;
		const int32 StartRimVertex = BaseVertex + 2;
		const int32 EndRimVertex = StartRimVertex + Segments;
		for (int32 SegmentIndex = 0; SegmentIndex < Segments; ++SegmentIndex)
		{
			const int32 NextSegment = (SegmentIndex + 1) % Segments;
			const int32 S1 = StartRimVertex + SegmentIndex;
			const int32 S2 = StartRimVertex + NextSegment;
			const int32 E1 = EndRimVertex + SegmentIndex;
			const int32 E2 = EndRimVertex + NextSegment;

			Batch.Indices.Add(S1);
			Batch.Indices.Add(E2);
			Batch.Indices.Add(E1);

			Batch.Indices.Add(S1);
			Batch.Indices.Add(S2);
			Batch.Indices.Add(E2);

			Batch.Indices.Add(EndVertex);
			Batch.Indices.Add(E1);
			Batch.Indices.Add(E2);

			Batch.Indices.Add(StartVertex);
			Batch.Indices.Add(S2);
			Batch.Indices.Add(S1);
		}
	}

	void AppendSphereMesh(
		FArcForestDebugDrawMeshBatch& Batch,
		const FVector& Center,
		const double Radius,
		const int32 NumLon,
		const int32 NumLat,
		const TConstArrayView<FVector2D> LongitudeRing,
		const TConstArrayView<FVector2D> LatitudeRing)
	{
		if (NumLon < 3 || NumLat < 1 || LongitudeRing.Num() < NumLon || LatitudeRing.Num() <= NumLat)
		{
			return;
		}

		const int32 BaseVertex = Batch.Vertices.Num();
		for (int32 LatIndex = 0; LatIndex <= NumLat; ++LatIndex)
		{
			const double CosTheta = LatitudeRing[LatIndex].X;
			const double SinTheta = LatitudeRing[LatIndex].Y;
			for (int32 LonIndex = 0; LonIndex < NumLon; ++LonIndex)
			{
				const double CosPhi = LongitudeRing[LonIndex].X;
				const double SinPhi = LongitudeRing[LonIndex].Y;
				Batch.Vertices.Add(
					Center + Radius * FVector(SinTheta * CosPhi, SinTheta * SinPhi, CosTheta));
			}
		}

		for (int32 LatIndex = 0; LatIndex < NumLat; ++LatIndex)
		{
			for (int32 LonIndex = 0; LonIndex < NumLon; ++LonIndex)
			{
				const int32 NextLon = (LonIndex + 1) % NumLon;
				const int32 V00 = BaseVertex + LatIndex * NumLon + LonIndex;
				const int32 V01 = BaseVertex + LatIndex * NumLon + NextLon;
				const int32 V10 = BaseVertex + (LatIndex + 1) * NumLon + LonIndex;
				const int32 V11 = BaseVertex + (LatIndex + 1) * NumLon + NextLon;

				Batch.Indices.Add(V00);
				Batch.Indices.Add(V01);
				Batch.Indices.Add(V11);

				Batch.Indices.Add(V00);
				Batch.Indices.Add(V11);
				Batch.Indices.Add(V10);
			}
		}
	}

	void AppendBoxMesh(
		FArcForestDebugDrawMeshBatch& Batch,
		const FVector& Center,
		const FVector& Extent,
		const FQuat& Rotation)
	{
		const FVector LocalCorners[8] = {
			FVector(-Extent.X, -Extent.Y, -Extent.Z),
			FVector(Extent.X, -Extent.Y, -Extent.Z),
			FVector(Extent.X, Extent.Y, -Extent.Z),
			FVector(-Extent.X, Extent.Y, -Extent.Z),
			FVector(-Extent.X, -Extent.Y, Extent.Z),
			FVector(Extent.X, -Extent.Y, Extent.Z),
			FVector(Extent.X, Extent.Y, Extent.Z),
			FVector(-Extent.X, Extent.Y, Extent.Z),
		};
		static constexpr int32 TriIndices[12][3] = {
			{0, 1, 2}, {0, 2, 3},
			{4, 6, 5}, {4, 7, 6},
			{0, 5, 1}, {0, 4, 5},
			{2, 7, 3}, {2, 6, 7},
			{1, 6, 2}, {1, 5, 6},
			{3, 4, 0}, {3, 7, 4},
		};

		const FMatrix LocalToWorld = FQuatRotationTranslationMatrix(Rotation, Center);
		const int32 BaseVertex = Batch.Vertices.Num();
		for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
		{
			Batch.Vertices.Add(LocalToWorld.TransformPosition(LocalCorners[CornerIndex]));
		}

		for (int32 TriangleIndex = 0; TriangleIndex < 12; ++TriangleIndex)
		{
			Batch.Indices.Add(BaseVertex + TriIndices[TriangleIndex][0]);
			Batch.Indices.Add(BaseVertex + TriIndices[TriangleIndex][1]);
			Batch.Indices.Add(BaseVertex + TriIndices[TriangleIndex][2]);
		}
	}

	// Tessellation is baked into the cache, not re-derived per frame: the Solid* helpers re-run the trig and
	// re-emit 24-36 triangles plus their opaque outline per record on every submission, which is what a
	// 50,000-stem stand cannot afford.
	void ReplayStandEntry(const FSceneInterface* Scene, const FArcForestDebugDrawGeometryCache::FStandEntry& Entry)
	{
		for (const FArcForestDebugDrawMeshBatch& Batch : Entry.Batches)
		{
			UE::DebugDraw::Mesh(Scene, Batch.Vertices, Batch.Indices, Batch.Color, Batch.Depth);
		}

		for (const FArcForestDebugDrawCachedLabel& CachedLabel : Entry.Labels)
		{
			EmitLabel(Scene, CachedLabel.WorldLocation, CachedLabel.Color, CachedLabel.Text);
		}
	}

	template <typename BuildType>
	void DrawCachedStandLayer(
		TMap<TWeakObjectPtr<const AArcEcotopeVolume>, FArcForestDebugDrawGeometryCache::FStandEntry>& Entries,
		const AArcEcotopeVolume& Volume,
		const FArcForestDebugDrawGeometryKey& Key,
		const BuildType& Build)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		FArcForestDebugDrawGeometryCache::FStandEntry* Entry = Entries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(Entries);
			Entry = &Entries.FindOrAdd(&Volume);
			Entry->Key = Key;
			Entry->Batches.Reset();
			Entry->BatchIndexByKey.Reset();
			Entry->Labels.Reset();
			Build(*Entry);
		}

		ReplayStandEntry(Scene, *Entry);
	}

	void BuildIndividualGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FArcForestDebugDrawGeometryCache::FStandEntry& Entry)
	{
		const FStandSpeciesFilter SpeciesFilter = MakeStandSpeciesFilter(Volume);
		TArray<int32> DrawnIndices;
		GatherFilteredRecords(Snapshot.Individuals, Volume.MaxDrawnStems,
			[&SpeciesFilter](const FArcForestIndividualRecord& Record) -> bool
			{
				return (Record.LifeStatus == EArcForestLifeStatus::Alive
						|| Record.LifeStatus == EArcForestLifeStatus::DeadStanding)
					&& SpeciesFilter.Allows(Record.SpeciesId);
			},
			DrawnIndices);

		const FVector WorldUp = GetWorldUp(Context);
		const double UniformScale = Context.UniformScale;

		const TMap<FGuid, int32> SpeciesMaxAgeYearsCache = Volume.StandColorMode == EArcForestDebugStandColor::Age
			? BuildSpeciesMaxAgeYearsCache(Volume)
			: TMap<FGuid, int32>();

		TArray<FVector2D> TrunkRing;
		BuildUnitRing(TrunkSegments, TrunkRing);
		TArray<FVector2D> CrownRing;
		BuildUnitRing(CrownSegments, CrownRing);
		TArray<FVector2D> CrownLatitudeRing;
		BuildUnitRing(2 * CrownLatitudeBands, CrownLatitudeRing);

		for (const int32 RecordIndex : DrawnIndices)
		{
			const FArcForestIndividualRecord& Record = Snapshot.Individuals[RecordIndex];
			const FVector BaseWorld = Context.VolumeTransform.TransformPosition(Record.LocalPosition);
			const bool bAlive = Record.LifeStatus == EArcForestLifeStatus::Alive;
			const double HeightCm = FMath::Max(double(Record.HeightCm), 0.0);
			const double TrunkRadiusCm = FMath::Max(double(Record.DbhCm) * 0.5, MinimumTrunkRadiusCm);
			const FVector TopWorld = BaseWorld + WorldUp * (HeightCm * UniformScale);
			const FColor TrunkColor = bAlive
				? MakeStandColor(SpeciesMaxAgeYearsCache, Volume.StandColorMode, Record.SpeciesId, Record.Vigor, Record.AgeYears, 255)
				: FColor(155, 155, 155, 255);

			const int32 TrunkBatchIndex = FindOrAddMeshBatch(Entry, TrunkColor, UE::DebugDraw::EDepthTest::WorldDepth);
			AppendCylinderMesh(Entry.Batches[TrunkBatchIndex], BaseWorld, TopWorld,
				TrunkRadiusCm * UniformScale, TrunkRing);

			if (Volume.bDrawLabels)
			{
				Entry.Labels.Add(FArcForestDebugDrawCachedLabel{TopWorld, TrunkColor,
					FString::Printf(TEXT("%s %.0fcm %dy v%.2f"), *MakeSpeciesShortId(Record.SpeciesId),
						HeightCm, Record.AgeYears, Record.Vigor)});
			}

			if (!bAlive || !Volume.bDrawIndividualCrowns)
			{
				continue;
			}

			// A recruit carries a zero crown radius, and a degenerate crown still costs a full sphere of triangles.
			const double CrownRadiusCm = double(Record.CrownRadiusCm);
			if (CrownRadiusCm < MinimumCrownRadiusCm)
			{
				continue;
			}

			const double CrownDepthCm = UE::ArcForest::Crown::CalculateCrownDepthCm(HeightCm, CrownRadiusCm);
			FColor CrownColor = TrunkColor;
			CrownColor.A = CrownAlpha;

			const int32 CrownBatchIndex = FindOrAddMeshBatch(Entry, CrownColor, UE::DebugDraw::EDepthTest::WorldDepth);

			// CalculateCrownDepthCm clamps 2r to the stem height, so an unclamped crown is exactly a sphere of radius r.
			if (2.0 * CrownRadiusCm <= HeightCm)
			{
				AppendSphereMesh(Entry.Batches[CrownBatchIndex],
					BaseWorld + WorldUp * ((HeightCm - CrownDepthCm * 0.5) * UniformScale),
					CrownRadiusCm * UniformScale, CrownSegments, CrownLatitudeBands, CrownRing, CrownLatitudeRing);
			}
			else
			{
				AppendCylinderMesh(Entry.Batches[CrownBatchIndex],
					BaseWorld + WorldUp * ((HeightCm - CrownDepthCm) * UniformScale),
					TopWorld, CrownRadiusCm * UniformScale, CrownRing);
			}
		}
	}

	void BuildDeadwoodGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FArcForestDebugDrawGeometryCache::FStandEntry& Entry)
	{
		const FStandSpeciesFilter SpeciesFilter = MakeStandSpeciesFilter(Volume);
		TArray<int32> DrawnIndices;
		GatherFilteredRecords(Snapshot.Deadwood, Volume.MaxDrawnDeadwood,
			[&SpeciesFilter](const FArcForestDeadwoodRecord& Record) -> bool
			{
				return Record.DimensionsCm.GetAbsMax() > 0.0 && SpeciesFilter.Allows(Record.SpeciesId);
			},
			DrawnIndices);

		const double UniformScale = Context.UniformScale;
		const FQuat VolumeRotation = Context.VolumeTransform.GetRotation();

		for (const int32 RecordIndex : DrawnIndices)
		{
			const FArcForestDeadwoodRecord& Record = Snapshot.Deadwood[RecordIndex];
			const FVector BaseWorld = Context.VolumeTransform.TransformPosition(Record.LocalTransform.GetLocation());
			const FQuat Rotation = VolumeRotation * Record.LocalTransform.GetRotation();
			const FVector CenterWorld = BaseWorld
				+ Rotation.RotateVector(FVector(0.0, 0.0, Record.DimensionsCm.Z * 0.5 * UniformScale));

			const FColor DeadwoodColor = MakeDeadwoodColor(
				Record.Type, UE::ArcEcotope::Deadwood::CalculateDecayClass(Record, Snapshot.CurrentYear));
			const int32 BatchIndex = FindOrAddMeshBatch(Entry, DeadwoodColor, UE::DebugDraw::EDepthTest::WorldDepth);
			AppendBoxMesh(Entry.Batches[BatchIndex], CenterWorld, Record.DimensionsCm * 0.5 * UniformScale, Rotation);
		}
	}

	bool AppendCellStand(
		const FArcForestDebugDrawContext& Context,
		const FVector& GroundWorld,
		const FVector& WorldUp,
		const double CellSizeCm,
		const double Count,
		const double MeanHeightCm,
		const FColor& Color,
		const TConstArrayView<FVector2D> Ring,
		FArcForestDebugDrawGeometryCache::FStandEntry& Entry)
	{
		if (!(MeanHeightCm > 0.0) || !(Count > 0.0))
		{
			return false;
		}

		const double CountFraction = FMath::Clamp(
			FMath::Sqrt(Count) / FMath::Sqrt(CellStandReferenceCount), 0.0, 1.0);
		const double RadiusCm = 0.45 * CellSizeCm * CountFraction;
		if (!(RadiusCm > 0.0))
		{
			return false;
		}

		const int32 BatchIndex = FindOrAddMeshBatch(Entry, Color, UE::DebugDraw::EDepthTest::WorldDepth);
		AppendCylinderMesh(Entry.Batches[BatchIndex], GroundWorld,
			GroundWorld + WorldUp * (MeanHeightCm * Context.UniformScale),
			RadiusCm * Context.UniformScale, Ring);
		return true;
	}

	void BuildCohortGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FSurfaceHeightSampler& Sampler,
		FArcForestDebugDrawGeometryCache::FStandEntry& Entry)
	{
		const FStandSpeciesFilter SpeciesFilter = MakeStandSpeciesFilter(Volume);
		TArray<int32> DrawnIndices;
		GatherFilteredRecords(Snapshot.Cohorts, Volume.MaxDrawnCellRecords,
			[&SpeciesFilter](const FArcForestCohortRecord& Record) -> bool
			{
				return SpeciesFilter.Allows(Record.SpeciesId);
			},
			DrawnIndices);

		const FVector WorldUp = GetWorldUp(Context);
		const double CellSizeCm = Context.Metadata.GetLevelCellSizeCm(Context.Metadata.RecordGridLevel);

		TArray<FVector2D> CellStandRing;
		BuildUnitRing(CellStandSegments, CellStandRing);

		Sampler.BeginLayer();
		for (const int32 RecordIndex : DrawnIndices)
		{
			const FArcForestCohortRecord& Record = Snapshot.Cohorts[RecordIndex];
			FVector LocalAnchor = FVector::ZeroVector;
			if (!TryGetRecordCellAnchorLocal(Context, Sampler, Record.CellIndex, LocalAnchor))
			{
				continue;
			}

			const FVector GroundWorld = Context.VolumeTransform.TransformPosition(LocalAnchor);
			const FColor CohortColor = MakeSpeciesColor(Record.SpeciesId, CellStandAlpha);
			if (!AppendCellStand(Context, GroundWorld, WorldUp, CellSizeCm, double(Record.StemCount),
					double(Record.MeanHeightCm), CohortColor, CellStandRing, Entry))
			{
				continue;
			}

			if (Volume.bDrawLabels)
			{
				Entry.Labels.Add(FArcForestDebugDrawCachedLabel{
					GroundWorld + WorldUp * (double(Record.MeanHeightCm) * Context.UniformScale), CohortColor,
					FString::Printf(TEXT("%lld stems %.0fcm"),
						static_cast<long long>(Record.StemCount), Record.MeanHeightCm)});
			}
		}
		ReportSurfaceSamples(Volume, Sampler, EArcForestDebugDrawSurfaceLayer::Cohorts, TEXT("cohort"));
	}

	void BuildPatchGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestWorldSnapshot& Snapshot,
		FSurfaceHeightSampler& Sampler,
		FArcForestDebugDrawGeometryCache::FStandEntry& Entry)
	{
		const FStandSpeciesFilter SpeciesFilter = MakeStandSpeciesFilter(Volume);
		TArray<int32> DrawnIndices;
		GatherFilteredRecords(Snapshot.Patches, Volume.MaxDrawnCellRecords,
			[&SpeciesFilter](const FArcForestPatchRecord& Record) -> bool
			{
				return SpeciesFilter.Allows(Record.SpeciesId);
			},
			DrawnIndices);

		const FVector WorldUp = GetWorldUp(Context);
		const double CellSizeCm = Context.Metadata.GetLevelCellSizeCm(Context.Metadata.RecordGridLevel);
		const FColor PatchColor = FColor(40, 180, 170, CellStandAlpha);

		TArray<FVector2D> CellStandRing;
		BuildUnitRing(CellStandSegments, CellStandRing);

		Sampler.BeginLayer();
		for (const int32 RecordIndex : DrawnIndices)
		{
			const FArcForestPatchRecord& Record = Snapshot.Patches[RecordIndex];
			FVector LocalAnchor = FVector::ZeroVector;
			if (!TryGetRecordCellAnchorLocal(Context, Sampler, Record.CellIndex, LocalAnchor))
			{
				continue;
			}

			const FVector GroundWorld = Context.VolumeTransform.TransformPosition(LocalAnchor);
			if (!AppendCellStand(Context, GroundWorld, WorldUp, CellSizeCm, double(Record.StemOrCoverEquivalent),
					double(Record.MeanHeightCm), PatchColor, CellStandRing, Entry))
			{
				continue;
			}

			if (Volume.bDrawLabels)
			{
				Entry.Labels.Add(FArcForestDebugDrawCachedLabel{
					GroundWorld + WorldUp * (double(Record.MeanHeightCm) * Context.UniformScale), PatchColor,
					FString::Printf(TEXT("%.1f cover %.0fcm"), Record.StemOrCoverEquivalent, Record.MeanHeightCm)});
			}
		}
		ReportSurfaceSamples(Volume, Sampler, EArcForestDebugDrawSurfaceLayer::Patches, TEXT("patch"));
	}

	FColor MakeFieldColor(const double Normalised01)
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
		Color.A = SiteFieldAlpha;
		return Color;
	}

	FColor MakeGreyscaleFieldColor(const double Normalised01)
	{
		const uint8 Luminance = uint8(FMath::Clamp(Normalised01, 0.0, 1.0) * 255.0 + 0.5);
		return FColor(Luminance, Luminance, Luminance, SiteFieldAlpha);
	}

	// FGpuTriangle packs one colour for all three verts (DebugDrawInternal.h:40-49), so quads only share a Mesh()
	// call once they share a shade.
	int32 MakeSheetShadeBucket(const double Shade, const int32 BucketCount)
	{
		return FMath::Clamp(
			int32(FMath::Clamp(Shade, 0.0, 1.0) * double(BucketCount)),
			0,
			BucketCount - 1);
	}

	double MakeSheetBucketShade(const int32 Bucket, const int32 BucketCount)
	{
		return (double(Bucket) + 0.5) / double(BucketCount);
	}

	bool IsBucketedSiteField(const EArcEcotopeSiteField Field)
	{
		return Field != EArcEcotopeSiteField::ValidSurface
			&& Field != EArcEcotopeSiteField::RecordValid;
	}

	int32 GetSiteFieldShadeBucketCount(const bool bGreyscale)
	{
		return bGreyscale ? SheetShadeBuckets : SheetRampBuckets;
	}

	FColor MakeSiteFieldShadeColor(const EArcEcotopeSiteField Field, const bool bGreyscale, const double Shade)
	{
		if (bGreyscale)
		{
			return MakeGreyscaleFieldColor(Shade);
		}

		if (Field == EArcEcotopeSiteField::Aspect)
		{
			// Aspect wraps: it is an azimuth in [0,1), so a hue wheel keeps north continuous where a linear ramp seams.
			FColor Color = FLinearColor::MakeFromHSV8(
				uint8(FMath::Clamp(Shade, 0.0, 1.0) * 255.0), 200, 235).ToFColor(true);
			Color.A = SiteFieldAlpha;
			return Color;
		}

		return MakeFieldColor(Shade);
	}

	bool TryMakeSiteFieldColor(
		const EArcEcotopeSiteField Field,
		const FArcEcotopeSiteMetadata& Metadata,
		const double RawValue,
		const bool bGreyscale,
		FColor& OutColor)
	{
		double Shade = RawValue;
		switch (Field)
		{
		case EArcEcotopeSiteField::ValidSurface:
		case EArcEcotopeSiteField::RecordValid:
			if (RawValue != 0.0)
			{
				return false;
			}
			// Greyscale cannot separate an invalid cell from a reading of 0, so the sentinel stays magenta in both modes.
			OutColor = FColor(255, 0, 255, 255);
			return true;

		case EArcEcotopeSiteField::Slope:
			Shade = RawValue / SiteFieldMaxSlopeDegrees;
			break;

		case EArcEcotopeSiteField::LocalSurfaceZ:
		{
			const double ExtentZCm = Metadata.LocalBounds.Max.Z - Metadata.LocalBounds.Min.Z;
			Shade = ExtentZCm > 0.0
				? (RawValue - Metadata.LocalBounds.Min.Z) / ExtentZCm
				: 0.0;
			break;
		}

		default:
			break;
		}

		const int32 BucketCount = GetSiteFieldShadeBucketCount(bGreyscale);
		OutColor = MakeSiteFieldShadeColor(
			Field, bGreyscale, MakeSheetBucketShade(MakeSheetShadeBucket(Shade, BucketCount), BucketCount));
		return true;
	}

	void GatherStridedSiteCells(
		const FArcEcotopeSiteMetadata& Metadata,
		const uint8 Level,
		const int32 AuthoredStride,
		const int32 MaxDrawnSiteCells,
		TArray<int32>& OutCellIndices,
		int32& OutStride,
		bool& bOutClampedToBudget)
	{
		OutCellIndices.Reset();
		OutStride = FMath::Max(1, AuthoredStride);
		bOutClampedToBudget = false;

		const FIntPoint LevelDimensions = Metadata.GetLevelDimensions(Level);
		const int32 LevelCellCount = Metadata.GetLevelCellCount(Level);
		if (LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || LevelCellCount <= 0)
		{
			return;
		}

		int32 Stride = FMath::Max(1, AuthoredStride);
		if (MaxDrawnSiteCells > 0)
		{
			const int32 AutoStride = FMath::Max(1, FMath::CeilToInt(
				FMath::Sqrt(double(LevelCellCount) / double(MaxDrawnSiteCells))));

			Stride = AuthoredStride > 0 ? AuthoredStride : AutoStride;
			const int64 SampledCellCount = int64(FMath::DivideAndRoundUp(LevelDimensions.X, Stride))
				* int64(FMath::DivideAndRoundUp(LevelDimensions.Y, Stride));
			if (AuthoredStride > 0 && SampledCellCount > int64(MaxDrawnSiteCells))
			{
				Stride = FMath::Max(Stride, AutoStride);
				bOutClampedToBudget = true;
			}
		}

		OutStride = Stride;
		OutCellIndices.Reserve(
			FMath::DivideAndRoundUp(LevelDimensions.X, Stride) * FMath::DivideAndRoundUp(LevelDimensions.Y, Stride));
		for (int32 CellY = 0; CellY < LevelDimensions.Y; CellY += Stride)
		{
			for (int32 CellX = 0; CellX < LevelDimensions.X; CellX += Stride)
			{
				OutCellIndices.Add(CellX + CellY * LevelDimensions.X);
			}
		}
	}

	// A single index stride over row-major cells only ever selects RowWidth / gcd of the columns, so labels lattice.
	struct FSheetLabelLattice
	{
		int32 RowWidth = 1;
		int32 ColumnStride = 1;
		int32 RowStride = 1;

		bool IsClamped() const
		{
			return ColumnStride > 1 || RowStride > 1;
		}

		bool ShouldLabel(const int32 CellArrayIndex) const
		{
			return ((CellArrayIndex % RowWidth) % ColumnStride) == 0
				&& ((CellArrayIndex / RowWidth) % RowStride) == 0;
		}

		int32 CountLabels(const int32 CellCount) const
		{
			return FMath::DivideAndRoundUp(RowWidth, ColumnStride)
				* FMath::DivideAndRoundUp(FMath::DivideAndRoundUp(CellCount, RowWidth), RowStride);
		}
	};

	FSheetLabelLattice MakeSheetLabelLattice(
		const FArcEcotopeSiteMetadata& Metadata,
		const uint8 Level,
		const int32 Stride,
		const int32 CellCount)
	{
		FSheetLabelLattice Lattice;
		Lattice.RowWidth = FMath::Max(1,
			FMath::DivideAndRoundUp(Metadata.GetLevelDimensions(Level).X, FMath::Max(1, Stride)));

		const int32 CellStride = FMath::Max(1,
			FMath::CeilToInt(double(CellCount) / double(MaxSheetLabelCells)));
		Lattice.ColumnStride = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(double(CellStride))));
		Lattice.RowStride = Lattice.ColumnStride;

		const int32 RowCount = FMath::Max(1, FMath::DivideAndRoundUp(CellCount, Lattice.RowWidth));
		while (Lattice.CountLabels(CellCount) > MaxSheetLabelCells)
		{
			const int32 ColumnLabels = FMath::DivideAndRoundUp(Lattice.RowWidth, Lattice.ColumnStride);
			const int32 RowLabels = FMath::DivideAndRoundUp(RowCount, Lattice.RowStride);
			if (ColumnLabels >= RowLabels)
			{
				++Lattice.ColumnStride;
			}
			else
			{
				++Lattice.RowStride;
			}
		}
		return Lattice;
	}

	bool TryGetStridedCellFootprintLocal(
		const FArcEcotopeSiteMetadata& Metadata,
		const int32 LevelCellIndex,
		const uint8 Level,
		const int32 Stride,
		const float CellScale,
		FVector2D& OutMinLocal,
		FVector2D& OutMaxLocal)
	{
		const FIntPoint LevelDimensions = Metadata.GetLevelDimensions(Level);
		const double LevelCellSizeCm = Metadata.GetLevelCellSizeCm(Level);
		if (LevelDimensions.X <= 0 || LevelDimensions.Y <= 0 || LevelCellSizeCm <= 0.0 || !Metadata.LocalBounds.IsValid)
		{
			return false;
		}

		const int32 LevelCellX = LevelCellIndex % LevelDimensions.X;
		const int32 LevelCellY = LevelCellIndex / LevelDimensions.X;
		const double FootprintCells = double(FMath::Max(1, Stride));
		const double FootprintMinX = Metadata.LocalBounds.Min.X + double(LevelCellX) * LevelCellSizeCm;
		const double FootprintMinY = Metadata.LocalBounds.Min.Y + double(LevelCellY) * LevelCellSizeCm;
		const double FootprintMaxX = FMath::Min(
			FootprintMinX + FootprintCells * LevelCellSizeCm, double(Metadata.LocalBounds.Max.X));
		const double FootprintMaxY = FMath::Min(
			FootprintMinY + FootprintCells * LevelCellSizeCm, double(Metadata.LocalBounds.Max.Y));

		FVector2D FootprintMin(FootprintMinX, FootprintMinY);
		FVector2D FootprintMax(FootprintMaxX, FootprintMaxY);
		const float ClampedScale = FMath::Clamp(CellScale, 0.05f, 1.0f);
		if (ClampedScale < 1.0f)
		{
			const FVector2D Center = (FootprintMin + FootprintMax) * 0.5;
			const FVector2D HalfExtent = (FootprintMax - FootprintMin) * 0.5 * double(ClampedScale);
			FootprintMin = Center - HalfExtent;
			FootprintMax = Center + HalfExtent;
		}

		OutMinLocal = FootprintMin;
		OutMaxLocal = FootprintMax;
		return true;
	}

	struct FSiteFieldCellQuad
	{
		FVector Corners[4] = {FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector};
		FVector Center = FVector::ZeroVector;
	};

	bool TryGetSiteFieldCellQuad(
		const FArcForestDebugDrawContext& Context,
		const int32 LevelCellIndex,
		const uint8 Level,
		const int32 Stride,
		const float CellScale,
		const double LocalZCm,
		FSiteFieldCellQuad& OutQuad)
	{
		FVector2D MinLocal = FVector2D::ZeroVector;
		FVector2D MaxLocal = FVector2D::ZeroVector;
		if (!TryGetStridedCellFootprintLocal(Context.Metadata, LevelCellIndex, Level, Stride, CellScale, MinLocal, MaxLocal))
		{
			return false;
		}

		// Both WorldDepth tiers cull CW, so the corner order matches SolidBox's top face (DebugDrawModule.cpp:2002).
		OutQuad.Corners[0] = Context.VolumeTransform.TransformPosition(FVector(MinLocal.X, MinLocal.Y, LocalZCm));
		OutQuad.Corners[1] = Context.VolumeTransform.TransformPosition(FVector(MinLocal.X, MaxLocal.Y, LocalZCm));
		OutQuad.Corners[2] = Context.VolumeTransform.TransformPosition(FVector(MaxLocal.X, MaxLocal.Y, LocalZCm));
		OutQuad.Corners[3] = Context.VolumeTransform.TransformPosition(FVector(MaxLocal.X, MinLocal.Y, LocalZCm));

		const FVector2D CenterLocal = (MinLocal + MaxLocal) * 0.5;
		OutQuad.Center = Context.VolumeTransform.TransformPosition(FVector(CenterLocal.X, CenterLocal.Y, LocalZCm));
		return true;
	}

	void EmitSiteFieldCellToBatch(
		const FArcForestDebugDrawContext& Context,
		const int32 LevelCellIndex,
		const uint8 Level,
		const int32 Stride,
		const float CellScale,
		const double LocalZCm,
		const bool bDrawLabel,
		const double RawValue,
		const int32 LabelPrecision,
		FArcForestDebugDrawMeshBatch& Batch,
		TArray<FArcForestDebugDrawCachedLabel>& Labels)
	{
		FSiteFieldCellQuad Quad;
		if (!TryGetSiteFieldCellQuad(Context, LevelCellIndex, Level, Stride, CellScale, LocalZCm, Quad))
		{
			return;
		}

		const int32 BaseVertex = Batch.Vertices.Num();
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			Batch.Vertices.Add(Quad.Corners[CornerIndex]);
		}

		Batch.Indices.Add(BaseVertex + 0);
		Batch.Indices.Add(BaseVertex + 1);
		Batch.Indices.Add(BaseVertex + 2);

		Batch.Indices.Add(BaseVertex + 0);
		Batch.Indices.Add(BaseVertex + 2);
		Batch.Indices.Add(BaseVertex + 3);

		if (bDrawLabel)
		{
			Labels.Add(FArcForestDebugDrawCachedLabel{
				Quad.Center,
				FColor::White,
				FString::Printf(TEXT("%.*f"), LabelPrecision, RawValue)});
		}
	}

#if WITH_EDITOR
	template <typename ValueType>
	bool IsExpectedFieldView(
		const TArcEcotopeSiteFieldView<ValueType>* View,
		const FArcEcotopeSiteMetadata& Metadata,
		const uint8 ExpectedLevel)
	{
		return View != nullptr
			&& View->IsValid()
			&& View->Level == ExpectedLevel
			&& View->GlobalDimensions == Metadata.GetLevelDimensions(ExpectedLevel);
	}

	void BuildSiteFieldCellsFromChunks(
		const FArcForestDebugDrawContext& Context,
		const FArcForestDebugDrawSiteReaderCache::FEntry& ReaderEntry,
		const EArcEcotopeSiteField Field,
		const uint8 Level,
		const int32 Stride,
		const float CellScale,
		const double LocalZCm,
		const bool bDrawLabels,
		const FSheetLabelLattice& LabelLattice,
		const bool bGreyscale,
		const TArray<int32>& CellIndices,
		FArcForestDebugDrawGeometryCache::FSiteFieldEntry& Entry,
		int32& OutValueCount,
		FString& OutError)
	{
		OutValueCount = 0;
		OutError.Reset();

		const FArcEcotopeChunkLayout& Layout = ReaderEntry.FrozenSource->Layout;
		const FIntPoint LevelDimensions = Context.Metadata.GetLevelDimensions(Level);
		if (Layout.GetBaseDimensions() != Context.Metadata.GridDimensions || LevelDimensions.X <= 0)
		{
			OutError = TEXT("the baked chunk layout does not match the runtime site grid");
			return;
		}

		const uint32 FieldMask = 1u << uint32(Field);
		const int32 BatchCeiling = IsBucketedSiteField(Field)
			? GetSiteFieldShadeBucketCount(bGreyscale)
			: MaxMeshBatches;
		const int32 NewBatchVertexHint = FMath::Max(4, (CellIndices.Num() * 4) / BatchCeiling);
		FArcEcotopeChunkId FailedChunkId;
		for (const FArcEcotopeChunkRect& Chunk : Layout.GetChunks())
		{
			FIntRect OwnedRect;
			if (!FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
					Chunk.BaseCoreRect, Context.Metadata.GridDimensions, Level, OwnedRect)
				|| OwnedRect.Width() <= 0 || OwnedRect.Height() <= 0)
			{
				continue;
			}

			FArcEcotopeSiteChunkLease Lease;
			FString AcquireError;
			if (!ReaderEntry.Reader.AcquireFields(Chunk.Id, 0, FieldMask, Lease, AcquireError))
			{
				FailedChunkId = Chunk.Id;
				continue;
			}

			const FArcEcotopeSiteChunkView& View = Lease.GetView();
			const FArcEcotopeSiteFloatFieldView* FloatField = View.FindFloatField(Field);
			const FArcEcotopeSiteByteFieldView* ByteField = View.FindByteField(Field);
			const bool bFloatFieldUsable = IsExpectedFieldView(FloatField, Context.Metadata, Level);
			const bool bByteFieldUsable = IsExpectedFieldView(ByteField, Context.Metadata, Level);
			if (!bFloatFieldUsable && !bByteFieldUsable)
			{
				FailedChunkId = Chunk.Id;
				continue;
			}

			int32 OwnedCellCount = 0;
			int32 ChunkValueCount = 0;
			for (int32 CellArrayIndex = 0; CellArrayIndex < CellIndices.Num(); ++CellArrayIndex)
			{
				const int32 CellIndex = CellIndices[CellArrayIndex];
				const FIntPoint CellCoordinates(CellIndex % LevelDimensions.X, CellIndex / LevelDimensions.X);
				if (!OwnedRect.Contains(CellCoordinates))
				{
					continue;
				}
				++OwnedCellCount;

				double RawValue = 0.0;
				if (bFloatFieldUsable)
				{
					const float* Value = FloatField->TryGet(CellCoordinates.X, CellCoordinates.Y);
					if (Value == nullptr)
					{
						continue;
					}
					RawValue = double(*Value);
				}
				else
				{
					const uint8* Value = ByteField->TryGet(CellCoordinates.X, CellCoordinates.Y);
					if (Value == nullptr)
					{
						continue;
					}
					RawValue = double(*Value);
				}
				++ChunkValueCount;

				FColor Color = FColor::Black;
				if (TryMakeSiteFieldColor(Field, Context.Metadata, RawValue, bGreyscale, Color))
				{
					const int32 BatchIndex = FindOrAddMeshBatch(Entry, Color,
						UE::DebugDraw::EDepthTest::WorldDepth, NewBatchVertexHint);
					EmitSiteFieldCellToBatch(Context, CellIndex, Level, Stride, CellScale, LocalZCm,
						bDrawLabels && LabelLattice.ShouldLabel(CellArrayIndex), RawValue, 2,
						Entry.Batches[BatchIndex], Entry.Labels);
				}
			}

			if (OwnedCellCount > 0 && ChunkValueCount == 0)
			{
				FailedChunkId = Chunk.Id;
				continue;
			}
			OutValueCount += ChunkValueCount;
		}

		if (OutValueCount == 0 && FailedChunkId.IsValid())
		{
			OutError = FString::Printf(
				TEXT("the baked chunk (%d, %d) carries no readable values for this field"),
				FailedChunkId.X, FailedChunkId.Y);
		}
		else if (OutValueCount == 0)
		{
			OutError = TEXT("no baked chunk owns the sampled site cells");
		}
	}
#endif // WITH_EDITOR

	uint64 MakeSiteFieldValuePayload(const uint64 SiteSnapshotHash, const EArcEcotopeSiteField Field)
	{
		return (SiteSnapshotHash << 8) ^ uint64(Field);
	}

	FString GetSiteFieldName(const EArcEcotopeSiteField Field)
	{
		const UEnum* FieldEnum = StaticEnum<EArcEcotopeSiteField>();
		return FieldEnum != nullptr
			? FieldEnum->GetNameStringByValue(int64(Field))
			: FString::FromInt(int32(Field));
	}

	bool IsClimateOwnedSiteField(const EArcEcotopeSiteField Field)
	{
		switch (Field)
		{
		case EArcEcotopeSiteField::Temperature:
		case EArcEcotopeSiteField::SunLoad:
		case EArcEcotopeSiteField::WindExposure:
		case EArcEcotopeSiteField::SkyViewFactor:
			return true;
		default:
			return false;
		}
	}

	void BuildSiteFieldGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		const EArcEcotopeSiteField Field,
		const uint8 Level,
		const bool bGreyscale,
		FArcForestDebugDrawGeometryCache::FSiteFieldEntry& Entry)
	{
		Entry.Batches.Reset();
		Entry.BatchIndexByKey.Reset();
		Entry.Labels.Reset();

		TArray<int32> CellIndices;
		int32 Stride = 1;
		bool bClampedToBudget = false;
		GatherStridedSiteCells(Context.Metadata, Level, Volume.SiteFieldStride, Volume.MaxDrawnSiteCells,
			CellIndices, Stride, bClampedToBudget);

		if (bClampedToBudget)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::SiteFieldStrideClamped, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls authored Site Field Stride %d would exceed Max Drawn Site Cells (%d), so the overlay raised it to %d.",
					*Volume.GetName(), Volume.SiteFieldStride, Volume.MaxDrawnSiteCells, Stride);
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::SiteFieldStrideClamped, Volume.VolumeId);
		}

		const FSheetLabelLattice LabelLattice = MakeSheetLabelLattice(
			Context.Metadata, Level, Stride, CellIndices.Num());
		if (Volume.bDrawLabels && LabelLattice.IsClamped())
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::SiteFieldLabelsClamped, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls would emit %d site field labels; capped to every %d columns and %d rows (%d labels).",
					*Volume.GetName(), CellIndices.Num(), LabelLattice.ColumnStride, LabelLattice.RowStride,
					LabelLattice.CountLabels(CellIndices.Num()));
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::SiteFieldLabelsClamped, Volume.VolumeId);
		}

		int32 ValueCount = 0;
		FString SourceError;
#if WITH_EDITOR
		const float CellScale = Volume.SiteFieldCellScale;
		const double LocalZCm = Context.Metadata.LocalBounds.Max.Z;
		const FArcForestDebugDrawSiteReaderCache::FEntry* ReaderEntry = nullptr;
		FString ReadError;
		if (TryGetSiteFieldReaderEntry(Volume, Context, SiteReaderCache, ReaderEntry, ReadError))
		{
			BuildSiteFieldCellsFromChunks(Context, *ReaderEntry, Field, Level, Stride, CellScale, LocalZCm,
				Volume.bDrawLabels, LabelLattice, bGreyscale, CellIndices, Entry, ValueCount, ReadError);
		}
		else if (ReadError.IsEmpty())
		{
			ReadError = TEXT("the baked chunk source could not be opened");
		}
		SourceError = ReadError;
#else
		SourceError = TEXT("the baked chunk source could not be opened");
#endif

		const uint64 Payload = MakeSiteFieldValuePayload(Context.Metadata.SiteSnapshotHash, Field);
		if (ValueCount > 0)
		{
			ClearLogLatch(EArcForestDebugDrawWarning::SiteFieldValues, Volume.VolumeId, Payload);
			return;
		}

		if (ShouldLogOnce(EArcForestDebugDrawWarning::SiteFieldValues, Volume.VolumeId, Payload))
		{
			UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls read no value for site field %ls over %d sampled cells and drew nothing: %ls.",
				*Volume.GetName(), *GetSiteFieldName(Field), CellIndices.Num(),
				SourceError.IsEmpty() ? TEXT("the site grid has no cells at this field's level") : *SourceError);
		}
	}

	void DrawSiteField(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		const EArcEcotopeSiteField Field = Volume.SiteField;
		if (uint32(Field) >= uint32(EArcEcotopeSiteField::Count))
		{
			return;
		}
		if (IsClimateOwnedSiteField(Field))
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::SiteFieldOwnedByClimate, Volume.VolumeId, uint64(Field)))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls selected climate-owned site field %ls; draw that field on the covering ClimateVolume.",
					*Volume.GetName(), *GetSiteFieldName(Field));
			}
			return;
		}

		if (!Context.bMetadataValid)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::SiteFieldMetadata, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls has no usable site metadata at runtime; the site field layer is not drawn.",
					*Volume.GetName());
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::SiteFieldMetadata, Volume.VolumeId);

		FArcForestDebugDrawGeometryKey Key;
		Key.VolumeId = Volume.VolumeId;
		Key.VolumeTransform = Context.VolumeTransform;
		Key.SiteSnapshotHash = Context.Metadata.SiteSnapshotHash;
		Key.MaxDrawn = Volume.MaxDrawnSiteCells;
		Key.Field = uint8(Field);
		Key.bDrawLabels = Volume.bDrawLabels;
		Key.Stride = Volume.SiteFieldStride;
		Key.CellScale = Volume.SiteFieldCellScale;
		Key.bSiteFieldGreyscale = Volume.bSiteFieldGreyscale;

		// The quads answer what is baked, so they are rebuilt only when this key moves and re-submitted every frame.
		FArcForestDebugDrawGeometryCache::FSiteFieldEntry* Entry = GeometryCache.SiteFieldEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.SiteFieldEntries);
			Entry = &GeometryCache.SiteFieldEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildSiteFieldGeometry(
				Context, Volume, SiteReaderCache, Field, Context.Metadata.GetFieldLevel(Field),
				Volume.bSiteFieldGreyscale, *Entry);
		}

		for (const FArcForestDebugDrawMeshBatch& Batch : Entry->Batches)
		{
			if (Batch.Indices.Num() == 0)
			{
				continue;
			}
			UE::DebugDraw::Mesh(Scene, Batch.Vertices, Batch.Indices, Batch.Color, Batch.Depth);
		}

		for (const FArcForestDebugDrawCachedLabel& CachedLabel : Entry->Labels)
		{
			EmitLabel(Scene, CachedLabel.WorldLocation, CachedLabel.Color, CachedLabel.Text);
		}
	}

#if WITH_EDITOR
	FString GetClimateFieldName(const EArcClimateField Field)
	{
		const UEnum* FieldEnum = StaticEnum<EArcClimateField>();
		return FieldEnum != nullptr
			? FieldEnum->GetNameStringByValue(int64(Field))
			: FString::FromInt(int32(Field));
	}

	void GatherStridedClimateCells(
		const FArcClimateProductMetadata& Metadata,
		const uint8 Level,
		const int32 AuthoredStride,
		const int32 MaxDrawnCells,
		TArray<int32>& OutCellIndices,
		int32& OutStride,
		bool& bOutClampedToBudget)
	{
		OutCellIndices.Reset();
		OutStride = FMath::Max(1, AuthoredStride);
		bOutClampedToBudget = false;

		const FIntPoint Dimensions = Metadata.GetLevelDimensions(Level);
		const int64 CellCount = int64(Dimensions.X) * int64(Dimensions.Y);
		if (Dimensions.X <= 0 || Dimensions.Y <= 0 || CellCount <= 0 || CellCount > MAX_int32)
		{
			return;
		}

		int32 Stride = FMath::Max(1, AuthoredStride);
		if (MaxDrawnCells > 0)
		{
			const int32 AutoStride = FMath::Max(1, FMath::CeilToInt(
				FMath::Sqrt(double(CellCount) / double(MaxDrawnCells))));
			Stride = AuthoredStride > 0 ? AuthoredStride : AutoStride;
			const int64 SampledCellCount = int64(FMath::DivideAndRoundUp(Dimensions.X, Stride))
				* int64(FMath::DivideAndRoundUp(Dimensions.Y, Stride));
			if (AuthoredStride > 0 && SampledCellCount > int64(MaxDrawnCells))
			{
				Stride = FMath::Max(Stride, AutoStride);
				bOutClampedToBudget = true;
			}
		}

		OutStride = Stride;
		OutCellIndices.Reserve(
			FMath::DivideAndRoundUp(Dimensions.X, Stride) * FMath::DivideAndRoundUp(Dimensions.Y, Stride));
		for (int32 CellY = 0; CellY < Dimensions.Y; CellY += Stride)
		{
			for (int32 CellX = 0; CellX < Dimensions.X; CellX += Stride)
			{
				OutCellIndices.Add(CellX + CellY * Dimensions.X);
			}
		}
	}

	FSheetLabelLattice MakeClimateLabelLattice(
		const FArcClimateProductMetadata& Metadata,
		const uint8 Level,
		const int32 Stride,
		const int32 CellCount)
	{
		FSheetLabelLattice Lattice;
		Lattice.RowWidth = FMath::Max(1,
			FMath::DivideAndRoundUp(Metadata.GetLevelDimensions(Level).X, FMath::Max(1, Stride)));

		const int32 CellStride = FMath::Max(1,
			FMath::CeilToInt(double(CellCount) / double(MaxSheetLabelCells)));
		Lattice.ColumnStride = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(double(CellStride))));
		Lattice.RowStride = Lattice.ColumnStride;

		const int32 RowCount = FMath::Max(1, FMath::DivideAndRoundUp(CellCount, Lattice.RowWidth));
		while (Lattice.CountLabels(CellCount) > MaxSheetLabelCells)
		{
			const int32 ColumnLabels = FMath::DivideAndRoundUp(Lattice.RowWidth, Lattice.ColumnStride);
			const int32 RowLabels = FMath::DivideAndRoundUp(RowCount, Lattice.RowStride);
			if (ColumnLabels >= RowLabels)
			{
				++Lattice.ColumnStride;
			}
			else
			{
				++Lattice.RowStride;
			}
		}
		return Lattice;
	}

	bool TryGetClimateFieldCellQuad(
		const FArcClimateProductMetadata& Metadata,
		const int32 LevelCellIndex,
		const uint8 Level,
		const int32 Stride,
		const float CellScale,
		FSiteFieldCellQuad& OutQuad)
	{
		const FIntPoint Dimensions = Metadata.GetLevelDimensions(Level);
		const double CellSizeCm = double(Metadata.CellSizeCm) * double(1 << Level);
		if (Dimensions.X <= 0 || Dimensions.Y <= 0 || CellSizeCm <= 0.0 || !Metadata.LocalBounds.IsValid
			|| LevelCellIndex < 0 || LevelCellIndex >= Dimensions.X * Dimensions.Y)
		{
			return false;
		}

		const int32 CellX = LevelCellIndex % Dimensions.X;
		const int32 CellY = LevelCellIndex / Dimensions.X;
		const double FootprintCells = double(FMath::Max(1, Stride));
		FVector2D FootprintMin(
			Metadata.LocalBounds.Min.X + double(CellX) * CellSizeCm,
			Metadata.LocalBounds.Min.Y + double(CellY) * CellSizeCm);
		FVector2D FootprintMax(
			FMath::Min(FootprintMin.X + FootprintCells * CellSizeCm, double(Metadata.LocalBounds.Max.X)),
			FMath::Min(FootprintMin.Y + FootprintCells * CellSizeCm, double(Metadata.LocalBounds.Max.Y)));
		const float ClampedScale = FMath::Clamp(CellScale, 0.05f, 1.0f);
		if (ClampedScale < 1.0f)
		{
			const FVector2D Center = (FootprintMin + FootprintMax) * 0.5;
			const FVector2D HalfExtent = (FootprintMax - FootprintMin) * 0.5 * double(ClampedScale);
			FootprintMin = Center - HalfExtent;
			FootprintMax = Center + HalfExtent;
		}

		const double LocalZCm = Metadata.LocalBounds.Max.Z;
		OutQuad.Corners[0] = Metadata.BakeTransform.TransformPosition(FVector(FootprintMin.X, FootprintMin.Y, LocalZCm));
		OutQuad.Corners[1] = Metadata.BakeTransform.TransformPosition(FVector(FootprintMin.X, FootprintMax.Y, LocalZCm));
		OutQuad.Corners[2] = Metadata.BakeTransform.TransformPosition(FVector(FootprintMax.X, FootprintMax.Y, LocalZCm));
		OutQuad.Corners[3] = Metadata.BakeTransform.TransformPosition(FVector(FootprintMax.X, FootprintMin.Y, LocalZCm));
		const FVector2D CenterLocal = (FootprintMin + FootprintMax) * 0.5;
		OutQuad.Center = Metadata.BakeTransform.TransformPosition(FVector(CenterLocal.X, CenterLocal.Y, LocalZCm));
		return true;
	}

	void EmitClimateFieldCellToBatch(
		const FSiteFieldCellQuad& Quad,
		const bool bDrawLabel,
		const double RawValue,
		FArcForestDebugDrawMeshBatch& Batch,
		TArray<FArcForestDebugDrawCachedLabel>& Labels)
	{
		const int32 BaseVertex = Batch.Vertices.Num();
		for (int32 CornerIndex = 0; CornerIndex < 4; ++CornerIndex)
		{
			Batch.Vertices.Add(Quad.Corners[CornerIndex]);
		}
		Batch.Indices.Add(BaseVertex + 0);
		Batch.Indices.Add(BaseVertex + 1);
		Batch.Indices.Add(BaseVertex + 2);
		Batch.Indices.Add(BaseVertex + 0);
		Batch.Indices.Add(BaseVertex + 2);
		Batch.Indices.Add(BaseVertex + 3);

		if (bDrawLabel)
		{
			Labels.Add(FArcForestDebugDrawCachedLabel{
				Quad.Center, FColor::White, FString::Printf(TEXT("%.2f"), RawValue)});
		}
	}

	bool BuildClimateFieldGeometry(
		const AArcClimateVolume& Volume,
		const EArcClimateField Field,
		FArcForestDebugDrawGeometryCache::FSiteFieldEntry& Entry,
		FString& OutError)
	{
		Entry.Batches.Reset();
		Entry.BatchIndexByKey.Reset();
		Entry.Labels.Reset();
		OutError.Reset();

		FArcClimateFrozenSourcePtr FrozenSource;
		FText FreezeError;
		if (!Volume.FreezeClimateSource(FrozenSource, FreezeError))
		{
			OutError = FreezeError.ToString();
			return false;
		}

		FArcClimateProductReader Reader;
		if (!Reader.Open(FrozenSource, OutError))
		{
			return false;
		}

		const FArcClimateProductMetadata& Metadata = FrozenSource->Metadata;
		const uint8 Level = Metadata.GetFieldLevel(Field);
		TArray<int32> CellIndices;
		int32 Stride = 1;
		bool bClampedToBudget = false;
		GatherStridedClimateCells(Metadata, Level, Volume.ClimateFieldStride, Volume.MaxDrawnClimateCells,
			CellIndices, Stride, bClampedToBudget);
		if (CellIndices.IsEmpty())
		{
			OutError = TEXT("the climate grid has no cells at the selected field level");
			return false;
		}

		if (bClampedToBudget)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::ClimateFieldStrideClamped, Volume.ClimateVolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls authored Climate Field Stride %d would exceed Max Drawn Climate Cells (%d), so the overlay raised it to %d.",
					*Volume.GetName(), Volume.ClimateFieldStride, Volume.MaxDrawnClimateCells, Stride);
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::ClimateFieldStrideClamped, Volume.ClimateVolumeId);
		}

		const FSheetLabelLattice LabelLattice = MakeClimateLabelLattice(
			Metadata, Level, Stride, CellIndices.Num());
		if (Volume.bDrawClimateLabels && LabelLattice.IsClamped())
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::ClimateFieldLabelsClamped, Volume.ClimateVolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls would emit %d climate field labels; capped to every %d columns and %d rows (%d labels).",
					*Volume.GetName(), CellIndices.Num(), LabelLattice.ColumnStride, LabelLattice.RowStride,
					LabelLattice.CountLabels(CellIndices.Num()));
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::ClimateFieldLabelsClamped, Volume.ClimateVolumeId);
		}

		const int32 BucketCount = GetSiteFieldShadeBucketCount(Volume.bClimateFieldGreyscale);
		const int32 NewBatchVertexHint = FMath::Max(4, (CellIndices.Num() * 4) / BucketCount);
		for (int32 CellArrayIndex = 0; CellArrayIndex < CellIndices.Num(); ++CellArrayIndex)
		{
			FSiteFieldCellQuad Quad;
			if (!TryGetClimateFieldCellQuad(Metadata, CellIndices[CellArrayIndex], Level, Stride,
					Volume.ClimateFieldCellScale, Quad))
			{
				OutError = TEXT("a sampled climate cell has no valid baked footprint");
				Entry.Batches.Reset();
				Entry.BatchIndexByKey.Reset();
				Entry.Labels.Reset();
				return false;
			}

			float Value = 0.0f;
			bool bFieldValid = false;
			if (!Reader.SampleAtWorldPosition(Quad.Center, Field, Value, bFieldValid, OutError)
				|| !bFieldValid || !FMath::IsFinite(Value))
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("the selected baked climate field is invalid or non-finite");
				}
				Entry.Batches.Reset();
				Entry.BatchIndexByKey.Reset();
				Entry.Labels.Reset();
				return false;
			}

			const int32 Bucket = MakeSheetShadeBucket(double(Value), BucketCount);
			const double BucketShade = MakeSheetBucketShade(Bucket, BucketCount);
			const FColor Color = Volume.bClimateFieldGreyscale
				? MakeGreyscaleFieldColor(BucketShade)
				: MakeFieldColor(BucketShade);
			const int32 BatchIndex = FindOrAddMeshBatch(
				Entry, Color, UE::DebugDraw::EDepthTest::WorldDepth, NewBatchVertexHint);
			EmitClimateFieldCellToBatch(Quad,
				Volume.bDrawClimateLabels && LabelLattice.ShouldLabel(CellArrayIndex), double(Value),
				Entry.Batches[BatchIndex], Entry.Labels);
		}
		return true;
	}

	void DrawClimateField(
		const AArcClimateVolume& Volume,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		const EArcClimateField Field = Volume.ClimateField;
		if (uint32(Field) >= uint32(EArcClimateField::Count))
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::ClimateFieldSelection,
					Volume.ClimateVolumeId, uint64(Field)))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls selected an invalid climate field and drew nothing.",
					*Volume.GetName());
			}
			return;
		}

		FString MetadataError;
		if (!Volume.ClimateProductMetadata.IsValid(MetadataError)
			|| !Volume.ClimateProductMetadata.IsFieldValid(Field)
			|| Volume.ClimateChunkManifest.IsEmpty())
		{
			if (MetadataError.IsEmpty())
			{
				MetadataError = TEXT("the selected field is invalid or the product manifest is empty");
			}
			if (ShouldLogOnce(EArcForestDebugDrawWarning::ClimateFieldMetadata, Volume.ClimateVolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls has no usable baked climate product and drew nothing: %ls.",
					*Volume.GetName(), *MetadataError);
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::ClimateFieldMetadata, Volume.ClimateVolumeId);

		const FArcClimateProductMetadata& Metadata = Volume.ClimateProductMetadata;
		FArcForestDebugDrawGeometryKey Key;
		Key.VolumeId = Volume.ClimateVolumeId;
		Key.VolumeTransform = Metadata.BakeTransform;
		Key.SiteSnapshotHash = Volume.ClimateChunkManifest.ManifestHash;
		Key.MaxDrawn = Volume.MaxDrawnClimateCells;
		Key.Field = uint8(Field);
		Key.RecordGridLevel = Metadata.GetFieldLevel(Field);
		Key.bDrawLabels = Volume.bDrawClimateLabels;
		Key.Stride = Volume.ClimateFieldStride;
		Key.CellScale = Volume.ClimateFieldCellScale;
		Key.SheetLocalZCm = Metadata.LocalBounds.Max.Z;
		Key.bSiteFieldGreyscale = Volume.bClimateFieldGreyscale;
		Key.SheetLocalBounds = Metadata.LocalBounds;
		Key.SheetCellSizeCm = Metadata.CellSizeCm;
		Key.SheetGridDimensions = Metadata.GridDimensions;

		FArcForestDebugDrawGeometryCache::FSiteFieldEntry* Entry =
			GeometryCache.ClimateFieldEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.ClimateFieldEntries);
			Entry = &GeometryCache.ClimateFieldEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			FString BuildError;
			const bool bBuilt = BuildClimateFieldGeometry(Volume, Field, *Entry, BuildError);
			const uint64 WarningPayload = (Volume.ClimateChunkManifest.ManifestHash << 8) ^ uint64(Field);
			if (bBuilt)
			{
				ClearLogLatch(EArcForestDebugDrawWarning::ClimateFieldValues,
					Volume.ClimateVolumeId, WarningPayload);
			}
			else if (ShouldLogOnce(EArcForestDebugDrawWarning::ClimateFieldValues,
					Volume.ClimateVolumeId, WarningPayload))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls could not read baked climate field %ls and drew nothing: %ls.",
					*Volume.GetName(), *GetClimateFieldName(Field), *BuildError);
			}
		}

		for (const FArcForestDebugDrawMeshBatch& Batch : Entry->Batches)
		{
			if (Batch.Indices.Num() > 0)
			{
				UE::DebugDraw::Mesh(Scene, Batch.Vertices, Batch.Indices, Batch.Color, Batch.Depth);
			}
		}
		for (const FArcForestDebugDrawCachedLabel& CachedLabel : Entry->Labels)
		{
			EmitLabel(Scene, CachedLabel.WorldLocation, CachedLabel.Color, CachedLabel.Text);
		}
	}
#endif

	FString GetTerrainDataMapName(const EArcTerrainDataMap Map)
	{
		const UEnum* MapEnum = StaticEnum<EArcTerrainDataMap>();
		return MapEnum != nullptr
			? MapEnum->GetNameStringByValue(int64(Map))
			: FString::FromInt(int32(Map));
	}

	FString DescribeTerrainDataMapUse(
		const EArcTerrainDataMap Map,
		const FArcEcotopeTerrainDataSettings& Settings)
	{
		switch (Map)
		{
		case EArcTerrainDataMap::Deposits:
			return FString::Printf(
				TEXT("soil depth, via DepositsAtZeroDepthM=%.3f, DepositsAtMaxDepthM=%.3f, MissingDepositsDepthM=%.3f"),
				Settings.DepositsAtZeroDepthM, Settings.DepositsAtMaxDepthM, Settings.MissingDepositsDepthM);

		case EArcTerrainDataMap::Flow:
			return TEXT("the Moisture wetness index, configured by the covering ClimateVolume");

		case EArcTerrainDataMap::RockMap:
			return FString::Printf(
				TEXT("BareRock, via RockWeight=%.3f"), Settings.RockWeight);

		case EArcTerrainDataMap::Rivers:
			return FString::Printf(
				TEXT("ChannelExclusion and ChannelBuffer, via bUseTreeRiverBuffer=%s, TreeBufferHardDistanceCm=%.1f, TreeBufferFalloffDistanceCm=%.1f"),
				Settings.bUseTreeRiverBuffer ? TEXT("true") : TEXT("false"),
				Settings.TreeBufferHardDistanceCm, Settings.TreeBufferFalloffDistanceCm);

		case EArcTerrainDataMap::Wear:
			return FString::Printf(
				TEXT("EstablishmentWear, via WearWeight=%.3f"), Settings.WearWeight);

		case EArcTerrainDataMap::Scree:
			return FString::Printf(
				TEXT("SubstrateMobility, via ScreeWeight=%.3f"), Settings.ScreeWeight);

		case EArcTerrainDataMap::FlowMapX:
		case EArcTerrainDataMap::FlowMapY:
			return FString::Printf(
				TEXT("ground-cover yaw, via FlowAlignStrength=%.3f"), Settings.FlowAlignStrength);

		default:
			return TEXT("nothing in the bake");
		}
	}

	double MakeTerrainDataMapShade(const EArcTerrainDataMap Map, const double Value, const double Gain)
	{
		return Map == EArcTerrainDataMap::FlowMapX || Map == EArcTerrainDataMap::FlowMapY
			? 0.5 + 0.5 * Value * Gain
			: Value * Gain;
	}

	void BuildTerrainDataMapGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const UArcTerrainSubsystem& TerrainSubsystem,
		AArcTerrain& Terrain,
		FArcForestDebugDrawGeometryCache::FTerrainDataMapEntry& Entry)
	{
		Entry.Batches.Reset();
		Entry.Labels.Reset();
		Entry.UnreadCellCount = 0;

		const FIntPoint Dimensions = Context.Metadata.GetLevelDimensions(0);
		if (Dimensions.X <= 0 || Dimensions.Y <= 0)
		{
			return;
		}

		TArray<int32> CellIndices;
		int32 Stride = 1;
		bool bClampedToBudget = false;
		GatherStridedSiteCells(Context.Metadata, 0, Volume.SiteFieldStride, Volume.MaxDrawnSiteCells,
			CellIndices, Stride, bClampedToBudget);

		if (bClampedToBudget)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::TerrainDataMapStrideClamped, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls authored Site Field Stride %d would exceed Max Drawn Site Cells (%d), so the terrain data map sheet raised it to %d.",
					*Volume.GetName(), Volume.SiteFieldStride, Volume.MaxDrawnSiteCells, Stride);
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::TerrainDataMapStrideClamped, Volume.VolumeId);
		}

		const FSheetLabelLattice LabelLattice = MakeSheetLabelLattice(
			Context.Metadata, 0, Stride, CellIndices.Num());
		if (Volume.bDrawLabels && LabelLattice.IsClamped())
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::TerrainDataMapLabelsClamped, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls would emit %d terrain data map labels; capped to every %d columns and %d rows (%d labels).",
					*Volume.GetName(), CellIndices.Num(), LabelLattice.ColumnStride, LabelLattice.RowStride,
					LabelLattice.CountLabels(CellIndices.Num()));
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::TerrainDataMapLabelsClamped, Volume.VolumeId);
		}

		const EArcTerrainDataMap Map = Volume.TerrainDataMap;
		const double Gain = double(Volume.TerrainDataMapGain);
		const double LocalZCm = Context.Metadata.LocalBounds.Max.Z + TerrainDataMapSurfaceLiftCm;

		double MinReading = 0.0;
		double MaxReading = 0.0;
		double SumReading = 0.0;
		int32 ReadCount = 0;

		const int32 BucketVertexHint = FMath::Max(4, (CellIndices.Num() * 4) / SheetShadeBuckets);
		TArray<FArcForestDebugDrawMeshBatch> ShadeBuckets;
		ShadeBuckets.SetNum(SheetShadeBuckets);
		for (int32 BucketIndex = 0; BucketIndex < SheetShadeBuckets; ++BucketIndex)
		{
			ShadeBuckets[BucketIndex].Color = MakeGreyscaleFieldColor(
				MakeSheetBucketShade(BucketIndex, SheetShadeBuckets));
			ShadeBuckets[BucketIndex].Depth = UE::DebugDraw::EDepthTest::WorldDepth;
			ShadeBuckets[BucketIndex].Vertices.Reserve(BucketVertexHint);
			ShadeBuckets[BucketIndex].Indices.Reserve(BucketVertexHint + BucketVertexHint / 2);
		}

		for (int32 CellArrayIndex = 0; CellArrayIndex < CellIndices.Num(); ++CellArrayIndex)
		{
			const int32 CellIndex = CellIndices[CellArrayIndex];
			const int32 CellX = CellIndex % Dimensions.X;
			const int32 CellY = CellIndex / Dimensions.X;
			const FVector WorldPosition = Context.VolumeTransform.TransformPosition(
				Context.Metadata.GetCellCenterLocal(CellX, CellY));

			float Reading = 0.0f;
			if (!TerrainSubsystem.SampleDataMap(Terrain, Map, WorldPosition, Reading) || !FMath::IsFinite(Reading))
			{
				++Entry.UnreadCellCount;
				continue;
			}

			const double Value = double(Reading);
			MinReading = ReadCount == 0 ? Value : FMath::Min(MinReading, Value);
			MaxReading = ReadCount == 0 ? Value : FMath::Max(MaxReading, Value);
			SumReading += Value;
			++ReadCount;

			const int32 Bucket = MakeSheetShadeBucket(
				MakeTerrainDataMapShade(Map, Value, Gain), SheetShadeBuckets);
			EmitSiteFieldCellToBatch(Context, CellIndex, 0, Stride, Volume.SiteFieldCellScale, LocalZCm,
				Volume.bDrawLabels && LabelLattice.ShouldLabel(CellArrayIndex), Value, 5,
				ShadeBuckets[Bucket], Entry.Labels);
		}

		for (FArcForestDebugDrawMeshBatch& ShadeBucket : ShadeBuckets)
		{
			if (ShadeBucket.Indices.Num() > 0)
			{
				Entry.Batches.Add(MoveTemp(ShadeBucket));
			}
		}

		const uint32 ReadingHash = ReadCount > 0
			? HashCombine(
				HashCombine(FMath::AsUInt(float(MinReading)), FMath::AsUInt(float(MaxReading))),
				FMath::AsUInt(float(SumReading / double(ReadCount))))
			: 0u;
		const uint64 Payload = (uint64(Map) << 32)
			^ uint64(HashCombine(
				HashCombine(uint32(CellIndices.Num()), uint32(Entry.UnreadCellCount)),
				ReadingHash));
		if (!ShouldLogOnce(EArcForestDebugDrawWarning::TerrainDataMapValues, Volume.VolumeId, Payload))
		{
			return;
		}

		const FString RangeText = ReadCount > 0
			? FString::Printf(TEXT("min %.5f max %.5f mean %.5f"),
				MinReading, MaxReading, SumReading / double(ReadCount))
			: FString(TEXT("min n/a max n/a mean n/a"));
		UE_LOGF(LogArcEcotope, Log,"ArcForestDebugDraw: %ls terrain data map %ls over %d sampled cells, %d unread, %ls; the bake reads it for %ls.",
			*Volume.GetName(), *GetTerrainDataMapName(Map), CellIndices.Num(), Entry.UnreadCellCount,
			*RangeText, *DescribeTerrainDataMapUse(Map, Volume.TerrainDataSettings));
	}

	void DrawTerrainDataMap(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		if (!Context.bMetadataValid)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::TerrainDataMapMetadata, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls has no usable site metadata at runtime; the terrain data map layer is not drawn.",
					*Volume.GetName());
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::TerrainDataMapMetadata, Volume.VolumeId);

		if (uint32(Volume.TerrainDataMap) >= uint32(EArcTerrainDataMap::Count))
		{
			return;
		}

		UWorld* World = Volume.GetWorld();
		if (World == nullptr)
		{
			return;
		}

		const UArcTerrainSubsystem* TerrainSubsystem = World->GetSubsystem<UArcTerrainSubsystem>();
		if (TerrainSubsystem == nullptr)
		{
			return;
		}

		const FBox VolumeWorldBounds = Context.Metadata.LocalBounds.TransformBy(Context.VolumeTransform);
		const UE::ArcEcotope::FArcEcotopeTerrainChoice Choice = UE::ArcEcotope::SelectTerrainForVolume(
			*World, VolumeWorldBounds, VolumeWorldBounds, Context.VolumeTransform);
		AArcTerrain* Terrain = Choice.Terrain;
		if (Terrain == nullptr)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::TerrainDataMapNoTerrain, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls resolves to no ArcTerrain; the terrain data map layer draws nothing.",
					*Volume.GetName());
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::TerrainDataMapNoTerrain, Volume.VolumeId);

		const uint64 TerrainPayload = uint64(GetTypeHash(Terrain->GetFName()));
		if (ShouldLogOnce(EArcForestDebugDrawWarning::TerrainDataMapTerrain, Volume.VolumeId, TerrainPayload))
		{
			UE_LOGF(LogArcEcotope, Log,"ArcForestDebugDraw: %ls reads its terrain data map overlay from %ls.",
				*Volume.GetName(), *Terrain->GetName());
		}

		FArcForestDebugDrawGeometryKey Key;
		Key.VolumeId = Volume.VolumeId;
		Key.VolumeTransform = Context.VolumeTransform;
		Key.SiteSnapshotHash = Context.Metadata.SiteSnapshotHash;
		Key.MaxDrawn = Volume.MaxDrawnSiteCells;
		Key.bDrawLabels = Volume.bDrawLabels;
		Key.Stride = Volume.SiteFieldStride;
		Key.CellScale = Volume.SiteFieldCellScale;
		Key.TerrainDataMap = uint8(Volume.TerrainDataMap);
		Key.TerrainDataMapGain = Volume.TerrainDataMapGain;
		Key.Terrain = Terrain;
		Key.TerrainTransform = Terrain->GetTerrainTransform();

		FArcForestDebugDrawGeometryCache::FTerrainDataMapEntry* Entry =
			GeometryCache.TerrainDataMapEntries.Find(&Volume);
		const double NowSeconds = FPlatformTime::Seconds();
		const bool bKeyRebuild = Entry == nullptr || !Entry->Key.Matches(Key);
		// Data tiles stream in after the sheet is first built, and nothing in the key moves when they arrive.
		const bool bRetryUnreadCells = !bKeyRebuild
			&& Entry->UnreadCellCount > 0
			&& !Entry->bUnreadRetryStalled
			&& NowSeconds - Entry->LastBuildSeconds > 1.0;
		if (bKeyRebuild || bRetryUnreadCells)
		{
			const int32 PreviousUnreadCellCount = Entry != nullptr ? Entry->UnreadCellCount : 0;
			PruneStaleVolumeEntries(GeometryCache.TerrainDataMapEntries);
			Entry = &GeometryCache.TerrainDataMapEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			Entry->LastBuildSeconds = NowSeconds;
			BuildTerrainDataMapGeometry(Context, Volume, *TerrainSubsystem, *Terrain, *Entry);
			// Tiles land in bursts, so one retry without progress is not evidence that no more will arrive.
			if (bRetryUnreadCells)
			{
				Entry->UnreadRetryPlateauCount = Entry->UnreadCellCount >= PreviousUnreadCellCount
					? Entry->UnreadRetryPlateauCount + 1
					: 0;
			}
			else
			{
				Entry->UnreadRetryPlateauCount = 0;
			}
			Entry->bUnreadRetryStalled =
				Entry->UnreadRetryPlateauCount >= MaxTerrainDataMapUnreadRetryPlateaus;
		}

		for (const FArcForestDebugDrawMeshBatch& Batch : Entry->Batches)
		{
			UE::DebugDraw::Mesh(Scene, Batch.Vertices, Batch.Indices, Batch.Color, Batch.Depth);
		}

		for (const FArcForestDebugDrawCachedLabel& CachedLabel : Entry->Labels)
		{
			EmitLabel(Scene, CachedLabel.WorldLocation, CachedLabel.Color, CachedLabel.Text);
		}
	}

	uint32 MakeDisturbancePreviewNoiseSeed(const AArcEcotopeVolume& Volume)
	{
		// Mirrors the run's own composition in ApplyDisturbance; nothing cross-checks the two, so a drift here
		// silently previews a field the simulation never produces.
		return HashCombine(
			HashCombine(static_cast<uint32>(Volume.Seed), static_cast<uint32>(Volume.PreviewDisturbanceYear)),
			static_cast<uint32>(Volume.PreviewDisturbanceKind));
	}

	uint32 MakeDisturbanceEventSettingsHash(const FArcForestDisturbanceSettings& Settings)
	{
		return HashCombine(
			HashCombine(FMath::AsUInt(Settings.FireReturnIntervalYears),
				FMath::AsUInt(Settings.WindReturnIntervalYears)),
			HashCombine(FMath::AsUInt(Settings.MinEventRadiusCm), FMath::AsUInt(Settings.MaxEventRadiusCm)));
	}

	// Local copies of the growth engine's file-private quantizers: the branch input has to be bit-identical to the
	// run's or every key below indexes a different stream.
	int64 QuantizeDisturbanceProbability(const double Probability)
	{
		return FMath::Clamp<int64>(
			FMath::RoundToInt64(FMath::Clamp(Probability, 0.0, 1.0) * DisturbanceProbabilityQuantum),
			0,
			static_cast<int64>(DisturbanceProbabilityQuantum));
	}

	double DequantizeDisturbanceProbability(const int64 QuantizedProbability)
	{
		return static_cast<double>(QuantizedProbability) / DisturbanceProbabilityQuantum;
	}

	struct FArcForestDisturbancePreviewEvent
	{
		FArcForestDisturbanceEvent Event;
		bool bFired = false;
	};

	EArcForestDisturbanceKind ToDisturbanceKind(const EArcForestDisturbancePreviewKind PreviewKind)
	{
		return PreviewKind == EArcForestDisturbancePreviewKind::Wind
			? EArcForestDisturbanceKind::Wind
			: EArcForestDisturbanceKind::Fire;
	}

	const TCHAR* GetDisturbanceKindName(const EArcForestDisturbanceKind Kind)
	{
		return Kind == EArcForestDisturbanceKind::Fire ? TEXT("fire") : TEXT("wind");
	}

	FColor GetDisturbanceKindColor(const EArcForestDisturbanceKind Kind)
	{
		return Kind == EArcForestDisturbanceKind::Fire
			? FColor(255, 110, 20, 255)
			: FColor(60, 220, 255, 255);
	}

	FArcForestDisturbancePreviewEvent ResolveDisturbancePreviewEventForYear(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const int32 Year)
	{
		FArcForestDisturbancePreviewEvent Preview;
		const FArcForestDisturbanceSettings& Settings = Volume.GetEcotopeOrDefault().DisturbanceSettings;
		Preview.Event.Kind = ToDisturbanceKind(Volume.PreviewDisturbanceKind);

		const int32 KindIndex = int32(Preview.Event.Kind);
		const FGuid SimulationId = UE::ArcForest::MakeSimulationId(Volume.VolumeId, Volume.Seed);
		const double IntervalYears = static_cast<double>(
			Preview.Event.Kind == EArcForestDisturbanceKind::Fire
				? Settings.FireReturnIntervalYears
				: Settings.WindReturnIntervalYears);
		const int64 Quantized =
			QuantizeDisturbanceProbability(IntervalYears > 0.0 ? 1.0 / IntervalYears : 0.0);
		const uint64 OccurrenceKey = UE::ArcForest::MakeAnnualKey(
			SimulationId,
			Volume.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DisturbanceOccurrence,
			FGuid(),
			INDEX_NONE,
			0,
			KindIndex,
			Quantized);
		if (!(UE::ArcForest::KeyToUnitOpen(OccurrenceKey) < DequantizeDisturbanceProbability(Quantized)))
		{
			return Preview;
		}

		const uint64 CentreXKey = UE::ArcForest::MakeAnnualKey(
			SimulationId,
			Volume.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DisturbanceCentre,
			FGuid(),
			INDEX_NONE,
			0,
			KindIndex * 2,
			Quantized);
		const uint64 CentreYKey = UE::ArcForest::MakeAnnualKey(
			SimulationId,
			Volume.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DisturbanceCentre,
			FGuid(),
			INDEX_NONE,
			0,
			KindIndex * 2 + 1,
			Quantized);
		const uint64 RadiusKey = UE::ArcForest::MakeAnnualKey(
			SimulationId,
			Volume.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DisturbanceRadius,
			FGuid(),
			INDEX_NONE,
			0,
			KindIndex,
			Quantized);
		const uint64 WindBearingKey = UE::ArcForest::MakeAnnualKey(
			SimulationId,
			Volume.Seed,
			Year,
			UE::ArcForest::EDeterministicDomain::DisturbanceWindBearing,
			FGuid(),
			INDEX_NONE,
			0,
			KindIndex,
			Quantized);
		const double MinRadiusCm = static_cast<double>(Settings.MinEventRadiusCm);
		const double MaxRadiusCm = FMath::Max(MinRadiusCm, static_cast<double>(Settings.MaxEventRadiusCm));
		const double RadiusUnit = UE::ArcForest::KeyToUnitOpen(RadiusKey);
		Preview.Event.CentreLocal = FVector2D(
			FMath::Lerp(
				Context.Metadata.LocalBounds.Min.X,
				Context.Metadata.LocalBounds.Max.X,
				UE::ArcForest::KeyToUnitOpen(CentreXKey)),
			FMath::Lerp(
				Context.Metadata.LocalBounds.Min.Y,
				Context.Metadata.LocalBounds.Max.Y,
				UE::ArcForest::KeyToUnitOpen(CentreYKey)));
		Preview.Event.RadiusCm = MinRadiusCm + (MaxRadiusCm - MinRadiusCm) * RadiusUnit * RadiusUnit;
		Preview.Event.WindBearingRadians = UE::ArcForest::KeyToUnitOpen(WindBearingKey) * UE_DOUBLE_TWO_PI;
		Preview.bFired = FMath::IsFinite(Preview.Event.CentreLocal.X)
			&& FMath::IsFinite(Preview.Event.CentreLocal.Y)
			&& FMath::IsFinite(Preview.Event.RadiusCm)
			&& Preview.Event.RadiusCm > 0.0
			&& FMath::IsFinite(Preview.Event.WindBearingRadians);
		return Preview;
	}

	struct FArcForestDisturbancePreviewSearchResult
	{
		FArcForestDisturbancePreviewEvent Preview;
		int32 PreviewedYear = 0;
	};

	// The occurrence roll is a pure hash of (SimulationId, Seed, Year, Kind), so scanning it forward is cheap;
	// it must still run once per cache build, not per frame.
	FArcForestDisturbancePreviewSearchResult ResolveDisturbancePreviewEvent(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume)
	{
		const int32 StartYear = Volume.PreviewDisturbanceYear;
		const int32 SearchYears = FMath::Max(Volume.PreviewDisturbanceSearchYears, 0);
		const EArcForestDisturbanceKind Kind = ToDisturbanceKind(Volume.PreviewDisturbanceKind);
		const uint64 SearchPayload = HashCombine(
			HashCombine(uint32(StartYear), uint32(SearchYears)), uint32(Kind));

		FArcForestDisturbancePreviewSearchResult Result;
		for (int32 Offset = 0; Offset <= SearchYears; ++Offset)
		{
			const int32 Year = StartYear + Offset;
			Result.Preview = ResolveDisturbancePreviewEventForYear(Context, Volume, Year);
			Result.PreviewedYear = Year;
			if (Result.Preview.bFired)
			{
				if (ShouldLogOnce(EArcForestDebugDrawWarning::DisturbancePreviewFoundYear, Volume.VolumeId, SearchPayload))
				{
					UE_LOGF(LogArcEcotope, Log,"ArcForestDebugDraw: %ls previews the first %ls event at or after year %d in year %d.",
						*Volume.GetName(), GetDisturbanceKindName(Kind), StartYear, Year);
				}
				return Result;
			}
		}

		if (ShouldLogOnce(EArcForestDebugDrawWarning::DisturbancePreviewNoEventInWindow, Volume.VolumeId, SearchPayload))
		{
			const double IntervalYears = static_cast<double>(
				Kind == EArcForestDisturbanceKind::Fire
					? Volume.GetEcotopeOrDefault().DisturbanceSettings.FireReturnIntervalYears
					: Volume.GetEcotopeOrDefault().DisturbanceSettings.WindReturnIntervalYears);
			const double PerYearProbability = IntervalYears > 0.0 ? 1.0 / IntervalYears : 0.0;
			UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls found no %ls event between years %d and %d (per-year probability %.4f%%); the preview shows nothing fired.",
				*Volume.GetName(), GetDisturbanceKindName(Kind), StartYear, Result.PreviewedYear,
				PerYearProbability * 100.0);
		}
		return Result;
	}

	enum class EArcForestDisturbanceNyquistState : uint8
	{
		Unconstrained,
		Unresolvable,
		Constrained,
	};

	double ComputeDisturbanceTopOctaveFrequencyPerCm()
	{
		return double(UE::ArcForest::DisturbanceNoiseFrequencyPerCm)
			* FMath::Pow(double(UE::ArcForest::DisturbanceNoiseLacunarity), double(UE::ArcForest::DisturbanceNoiseOctaves - 1));
	}

	EArcForestDisturbanceNyquistState ComputeDisturbanceNyquistStrideCells(
		const double FrequencyPerCm,
		const double LevelCellSizeCm,
		int32& OutStrideCells)
	{
		OutStrideCells = 0;
		if (!FMath::IsFinite(FrequencyPerCm) || FrequencyPerCm <= 0.0
			|| !FMath::IsFinite(LevelCellSizeCm) || LevelCellSizeCm <= 0.0)
		{
			return EArcForestDisturbanceNyquistState::Unconstrained;
		}

		const double SpacingCells = 0.5 / (FrequencyPerCm * LevelCellSizeCm);
		if (!FMath::IsFinite(SpacingCells))
		{
			return EArcForestDisturbanceNyquistState::Unconstrained;
		}

		if (SpacingCells < 1.0)
		{
			return EArcForestDisturbanceNyquistState::Unresolvable;
		}

		OutStrideCells = int32(FMath::FloorToDouble(FMath::Min(SpacingCells, MaxDisturbancePreviewStrideCells)));
		return EArcForestDisturbanceNyquistState::Constrained;
	}

	double GetVolumeTopLocalZCm(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume)
	{
		const UBoxComponent* Bounds = Volume.VolumeBounds;
		if (Bounds != nullptr)
		{
			const double BoxTopZCm = Bounds->GetUnscaledBoxExtent().Z;
			if (FMath::IsFinite(BoxTopZCm) && BoxTopZCm > 0.0)
			{
				return BoxTopZCm;
			}
		}

		return Context.Metadata.LocalBounds.Max.Z;
	}

	void DrawDisturbanceScaleLegend(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const double SheetLocalZCm)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		const FVector BoundsCentre = Context.Metadata.LocalBounds.GetCenter();
		const FVector2D CentreLocal(BoundsCentre.X, BoundsCentre.Y);
		const FVector CentreWorld = Context.VolumeTransform.TransformPosition(
			FVector(CentreLocal.X, CentreLocal.Y, SheetLocalZCm));
		const FVector WorldUp = GetWorldUp(Context);
		const float Thickness = float(
			double(Volume.DebugLineThicknessCm) * DisturbanceThinRingFraction * Context.UniformScale);

		const double MinRadiusCm = double(Volume.GetEcotopeOrDefault().DisturbanceSettings.MinEventRadiusCm);
		const double MaxRadiusCm = double(Volume.GetEcotopeOrDefault().DisturbanceSettings.MaxEventRadiusCm);
		const FColor MinRingColor(160, 100, 60, DisturbanceLegendRingAlpha);
		const FColor MaxRingColor(70, 130, 150, DisturbanceLegendRingAlpha);

		UE::DebugDraw::Circle(Scene, CentreWorld, WorldUp, float(MinRadiusCm * Context.UniformScale),
			DisturbanceRingSegments, MinRingColor, Thickness, UE::DebugDraw::EDepthTest::WorldDepth);
		UE::DebugDraw::Circle(Scene, CentreWorld, WorldUp, float(MaxRadiusCm * Context.UniformScale),
			DisturbanceRingSegments, MaxRingColor, Thickness, UE::DebugDraw::EDepthTest::WorldDepth);

		if (!Volume.bDrawLabels)
		{
			return;
		}

		EmitLabel(Scene,
			Context.VolumeTransform.TransformPosition(
				FVector(CentreLocal.X + MinRadiusCm, CentreLocal.Y, SheetLocalZCm)),
			MinRingColor, FString::Printf(TEXT("scale legend: min r %.0fm"), MinRadiusCm * 0.01));
		EmitLabel(Scene,
			Context.VolumeTransform.TransformPosition(
				FVector(CentreLocal.X + MaxRadiusCm, CentreLocal.Y, SheetLocalZCm)),
			MaxRingColor, FString::Printf(TEXT("scale legend: max r %.0fm"), MaxRadiusCm * 0.01));
	}

	void DrawDisturbanceEventOverlay(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestDisturbancePreviewEvent& Preview,
		const int32 PreviewedYear,
		const double SheetLocalZCm)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		const TCHAR* KindName = GetDisturbanceKindName(Preview.Event.Kind);
		if (!Preview.bFired)
		{
			const FVector BoundsCentre = Context.Metadata.LocalBounds.GetCenter();
			const double IntervalYears = static_cast<double>(
				Preview.Event.Kind == EArcForestDisturbanceKind::Fire
					? Volume.GetEcotopeOrDefault().DisturbanceSettings.FireReturnIntervalYears
					: Volume.GetEcotopeOrDefault().DisturbanceSettings.WindReturnIntervalYears);
			EmitLabel(Scene,
				Context.VolumeTransform.TransformPosition(
					FVector(BoundsCentre.X, BoundsCentre.Y, SheetLocalZCm)),
				FColor(180, 180, 180, 255),
				IntervalYears > 0.0
					? FString::Printf(TEXT("no %s event in years %d-%d"),
						KindName, Volume.PreviewDisturbanceYear, PreviewedYear)
					: FString::Printf(TEXT("disturbance disabled: no %s event in years %d-%d"),
						KindName, Volume.PreviewDisturbanceYear, PreviewedYear));
			return;
		}

		const FVector CentreWorld = Context.VolumeTransform.TransformPosition(
			FVector(Preview.Event.CentreLocal.X, Preview.Event.CentreLocal.Y, SheetLocalZCm));
		const FVector WorldUp = GetWorldUp(Context);
		const FColor RingColor = GetDisturbanceKindColor(Preview.Event.Kind);
		const FColor FalloffColor(RingColor.R, RingColor.G, RingColor.B, 140);
		const float ThinThickness = float(
			double(Volume.DebugLineThicknessCm) * DisturbanceThinRingFraction * Context.UniformScale);

		UE::DebugDraw::Circle(Scene, CentreWorld, WorldUp,
			float(Preview.Event.RadiusCm * Context.UniformScale), DisturbanceRingSegments, RingColor,
			float(double(Volume.DebugLineThicknessCm) * Context.UniformScale),
			UE::DebugDraw::EDepthTest::AlwaysOnTop);
		UE::DebugDraw::Circle(Scene, CentreWorld, WorldUp,
			float(Preview.Event.RadiusCm * DisturbanceFalloffStartFraction * Context.UniformScale),
			DisturbanceRingSegments, FalloffColor, ThinThickness,
			UE::DebugDraw::EDepthTest::AlwaysOnTop);

		EmitLabel(Scene, CentreWorld, RingColor,
			Preview.Event.Kind == EArcForestDisturbanceKind::Wind
				? FString::Printf(TEXT("%s year %d: epicentre, r %.0fm, bearing %.0fdeg"),
					KindName, PreviewedYear, Preview.Event.RadiusCm * 0.01,
					FMath::RadiansToDegrees(Preview.Event.WindBearingRadians))
				: FString::Printf(TEXT("%s year %d: epicentre, r %.0fm"),
					KindName, PreviewedYear, Preview.Event.RadiusCm * 0.01));
	}

	void BuildDisturbancePreviewGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		const FArcForestDisturbancePreviewEvent& Preview,
		const double SheetLocalZCm,
		FArcForestDebugDrawGeometryCache::FDisturbancePreviewEntry& Entry)
	{
		Entry.Triangles.Reset();
		Entry.Labels.Reset();

		const uint8 Level = Context.Metadata.RecordGridLevel;
		const double LevelCellSizeCm = Context.Metadata.GetLevelCellSizeCm(Level);
		const double TopOctaveFrequencyPerCm = ComputeDisturbanceTopOctaveFrequencyPerCm();
		int32 NyquistStride = 0;
		const EArcForestDisturbanceNyquistState NyquistState = ComputeDisturbanceNyquistStrideCells(
			TopOctaveFrequencyPerCm, LevelCellSizeCm, NyquistStride);

		TArray<int32> CellIndices;
		int32 Stride = 1;
		bool bClampedToBudget = false;
		GatherStridedSiteCells(Context.Metadata, Level, 0, Volume.MaxDrawnDisturbancePreviewCells,
			CellIndices, Stride, bClampedToBudget);

		if (NyquistState == EArcForestDisturbanceNyquistState::Unresolvable)
		{
			ClearLogLatch(EArcForestDebugDrawWarning::DisturbancePreviewStrideClamped, Volume.VolumeId);
			if (ShouldLogOnce(EArcForestDebugDrawWarning::DisturbancePreviewUnresolvable, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls samples the noise on record grid level %d, whose %.0f cm cells are coarser than the half wavelength %.0f cm of its top octave (%d octaves at lacunarity %.2f), so no stride resolves the field and the sheet's pattern is not the field the run produces.",
					*Volume.GetName(), int32(Level), LevelCellSizeCm, 0.5 / TopOctaveFrequencyPerCm,
					UE::ArcForest::DisturbanceNoiseOctaves, UE::ArcForest::DisturbanceNoiseLacunarity);
			}
		}
		else if (NyquistState == EArcForestDisturbanceNyquistState::Constrained && Stride > NyquistStride)
		{
			ClearLogLatch(EArcForestDebugDrawWarning::DisturbancePreviewUnresolvable, Volume.VolumeId);
			if (ShouldLogOnce(EArcForestDebugDrawWarning::DisturbancePreviewStrideClamped, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls resolves the noise top octave at every %d record cells, but Max Drawn Disturbance Preview Cells (%d) only affords every %d, so the sheet is undersampled and its pattern is not the field the run produces.",
					*Volume.GetName(), NyquistStride, Volume.MaxDrawnDisturbancePreviewCells, Stride);
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::DisturbancePreviewStrideClamped, Volume.VolumeId);
			ClearLogLatch(EArcForestDebugDrawWarning::DisturbancePreviewUnresolvable, Volume.VolumeId);
		}

		const FSheetLabelLattice LabelLattice = MakeSheetLabelLattice(
			Context.Metadata, Level, Stride, CellIndices.Num());
		if (Volume.bDrawLabels && LabelLattice.IsClamped())
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::DisturbancePreviewLabelsClamped, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls would emit %d disturbance preview labels; capped to every %d columns and %d rows (%d labels) to stay near the site field's label budget.",
					*Volume.GetName(), CellIndices.Num(), LabelLattice.ColumnStride, LabelLattice.RowStride,
					LabelLattice.CountLabels(CellIndices.Num()));
			}
		}
		else
		{
			ClearLogLatch(EArcForestDebugDrawWarning::DisturbancePreviewLabelsClamped, Volume.VolumeId);
		}

		const uint32 NoiseSeed = MakeDisturbancePreviewNoiseSeed(Volume);
		Entry.Triangles.Reserve(CellIndices.Num() * 2);
		for (int32 CellArrayIndex = 0; CellArrayIndex < CellIndices.Num(); ++CellArrayIndex)
		{
			const int32 CellIndex = CellIndices[CellArrayIndex];
			FVector2D MinLocal = FVector2D::ZeroVector;
			FVector2D MaxLocal = FVector2D::ZeroVector;
			if (!TryGetStridedCellFootprintLocal(
					Context.Metadata, CellIndex, Level, Stride, 1.0f, MinLocal, MaxLocal))
			{
				continue;
			}

			const FVector2D CenterLocal = (MinLocal + MaxLocal) * 0.5;
			const float Noise01 = UE::ArcForest::SampleDisturbanceNoise01(
				FVector2f(float(CenterLocal.X), float(CenterLocal.Y)), NoiseSeed);
			double Shade = double(Noise01);
			if (Preview.bFired)
			{
				const double Severity = UE::ArcForest::CalculateDisturbanceSeverity(
					Volume.GetEcotopeOrDefault().DisturbanceSettings, Preview.Event, NoiseSeed, CenterLocal);
				if (Severity > 0.0)
				{
					Shade = Severity;
				}
			}
			const FColor Color = MakeGreyscaleFieldColor(Shade);

			const FVector CornerMinMin = Context.VolumeTransform.TransformPosition(
				FVector(MinLocal.X, MinLocal.Y, SheetLocalZCm));
			const FVector CornerMaxMin = Context.VolumeTransform.TransformPosition(
				FVector(MaxLocal.X, MinLocal.Y, SheetLocalZCm));
			const FVector CornerMaxMax = Context.VolumeTransform.TransformPosition(
				FVector(MaxLocal.X, MaxLocal.Y, SheetLocalZCm));
			const FVector CornerMinMax = Context.VolumeTransform.TransformPosition(
				FVector(MinLocal.X, MaxLocal.Y, SheetLocalZCm));

			Entry.Triangles.Add(FArcForestDebugDrawCachedTriangle{CornerMinMin, CornerMinMax, CornerMaxMax, Color});
			Entry.Triangles.Add(FArcForestDebugDrawCachedTriangle{CornerMinMin, CornerMaxMax, CornerMaxMin, Color});

			if (Volume.bDrawLabels && LabelLattice.ShouldLabel(CellArrayIndex))
			{
				Entry.Labels.Add(FArcForestDebugDrawCachedLabel{
					Context.VolumeTransform.TransformPosition(
						FVector(CenterLocal.X, CenterLocal.Y, SheetLocalZCm)),
					FColor::White,
					FString::Printf(TEXT("%.2f"), Shade)});
			}
		}
	}

	void DrawDisturbancePreview(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		if (!Context.bMetadataValid)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::DisturbancePreviewMetadata, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls has no usable site metadata at runtime; the disturbance preview layer is not drawn.",
					*Volume.GetName());
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::DisturbancePreviewMetadata, Volume.VolumeId);

		const double SheetLocalZCm = GetVolumeTopLocalZCm(Context, Volume) + DisturbancePreviewSurfaceLiftCm;
		const FArcForestDisturbancePreviewSearchResult SearchResult = ResolveDisturbancePreviewEvent(Context, Volume);
		DrawDisturbanceScaleLegend(Context, Volume, SheetLocalZCm);
		DrawDisturbanceEventOverlay(Context, Volume, SearchResult.Preview, SearchResult.PreviewedYear, SheetLocalZCm);

		FArcForestDebugDrawGeometryKey Key;
		Key.VolumeId = Volume.VolumeId;
		Key.VolumeTransform = Context.VolumeTransform;
		Key.SiteSnapshotHash = Context.Metadata.SiteSnapshotHash;
		Key.MaxDrawn = Volume.MaxDrawnDisturbancePreviewCells;
		Key.RecordGridLevel = Context.Metadata.RecordGridLevel;
		Key.bDrawLabels = Volume.bDrawLabels;
		Key.SheetLocalZCm = SheetLocalZCm;
		Key.Seed = Volume.Seed;
		Key.PreviewYear = Volume.PreviewDisturbanceYear;
		Key.PreviewSearchYears = Volume.PreviewDisturbanceSearchYears;
		Key.PreviewKind = uint8(Volume.PreviewDisturbanceKind);
		Key.DisturbanceSettingsHash = MakeDisturbanceEventSettingsHash(Volume.GetEcotopeOrDefault().DisturbanceSettings);

		FArcForestDebugDrawGeometryCache::FDisturbancePreviewEntry* Entry =
			GeometryCache.DisturbancePreviewEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.DisturbancePreviewEntries);
			Entry = &GeometryCache.DisturbancePreviewEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildDisturbancePreviewGeometry(Context, Volume, SearchResult.Preview, SheetLocalZCm, *Entry);
		}

		for (const FArcForestDebugDrawCachedTriangle& CachedTriangle : Entry->Triangles)
		{
			UE::DebugDraw::Triangle(Scene, CachedTriangle.V0, CachedTriangle.V1, CachedTriangle.V2, CachedTriangle.Color,
				UE::DebugDraw::EDepthTest::WorldDepth);
		}

		for (const FArcForestDebugDrawCachedLabel& CachedLabel : Entry->Labels)
		{
			EmitLabel(Scene, CachedLabel.WorldLocation, CachedLabel.Color, CachedLabel.Text);
		}
	}

	void GetChunkRectLocal(
		const FArcEcotopeSiteMetadata& Metadata,
		const FIntRect& BaseCoreRect,
		FVector2D& OutMinLocal,
		FVector2D& OutMaxLocal)
	{
		const double CellSizeCm = double(Metadata.CellSizeCm);
		OutMinLocal.X = Metadata.LocalBounds.Min.X + double(BaseCoreRect.Min.X) * CellSizeCm;
		OutMinLocal.Y = Metadata.LocalBounds.Min.Y + double(BaseCoreRect.Min.Y) * CellSizeCm;
		OutMaxLocal.X = FMath::Min(
			Metadata.LocalBounds.Min.X + double(BaseCoreRect.Max.X) * CellSizeCm, Metadata.LocalBounds.Max.X);
		OutMaxLocal.Y = FMath::Min(
			Metadata.LocalBounds.Min.Y + double(BaseCoreRect.Max.Y) * CellSizeCm, Metadata.LocalBounds.Max.Y);
	}

	bool TryGetChunkLayoutForDraw(
		const AArcEcotopeVolume& Volume,
		const FArcForestDebugDrawContext& Context,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		FArcEcotopeChunkLayout& ScratchLayout,
		const FArcEcotopeChunkLayout*& OutLayout)
	{
		OutLayout = nullptr;

#if WITH_EDITOR
		const FArcForestDebugDrawSiteReaderCache::FEntry* Entry = nullptr;
		FString ReaderError;
		if (TryGetSiteFieldReaderEntry(Volume, Context, SiteReaderCache, Entry, ReaderError)
			&& Entry->FrozenSource.IsValid())
		{
			OutLayout = &Entry->FrozenSource->Layout;
		}
		return OutLayout != nullptr;
#else
		FString BuildError;
		if (FArcEcotopeChunkLayout::Build(Context.Metadata, Volume.ChunkBaseCellSpan, 0, ScratchLayout, BuildError))
		{
			OutLayout = &ScratchLayout;
		}
		return OutLayout != nullptr;
#endif
	}

	void BuildChunkGridGeometry(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		FSurfaceHeightSampler& Sampler,
		FArcForestDebugDrawGeometryCache::FChunkGridEntry& Entry)
	{
		Entry.Lines.Reset();
		Entry.Labels.Reset();

		FArcEcotopeChunkLayout ScratchLayout;
		const FArcEcotopeChunkLayout* Layout = nullptr;
		if (!TryGetChunkLayoutForDraw(Volume, Context, SiteReaderCache, ScratchLayout, Layout))
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::ChunkLayout, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls could not resolve a chunk layout; the chunk grid layer is not drawn.",
					*Volume.GetName());
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::ChunkLayout, Volume.VolumeId);

		const double FloorZLocal = Context.Metadata.LocalBounds.Min.Z;
		const float Thickness = float(double(Volume.DebugLineThicknessCm) * Context.UniformScale);
		const TConstArrayView<FArcEcotopeChunkRect> Chunks = Layout->GetChunks();
		const int32 DrawnChunkCount = Volume.MaxDrawnChunks > 0
			? FMath::Min(Chunks.Num(), Volume.MaxDrawnChunks)
			: Chunks.Num();
		if (DrawnChunkCount <= 0)
		{
			return;
		}

		constexpr int32 EdgeSegments = 4;
		constexpr int32 PerimeterVertexCount = EdgeSegments * 4;

		Entry.Lines.Reserve(DrawnChunkCount * PerimeterVertexCount);

		TArray<FVector> PerimeterWorld;
		PerimeterWorld.SetNum(PerimeterVertexCount);

		Sampler.BeginLayer();
		for (int32 ChunkIndex = 0; ChunkIndex < DrawnChunkCount; ++ChunkIndex)
		{
			const FArcEcotopeChunkRect& Chunk = Chunks[ChunkIndex];
			FVector2D MinLocal = FVector2D::ZeroVector;
			FVector2D MaxLocal = FVector2D::ZeroVector;
			GetChunkRectLocal(Context.Metadata, Chunk.BaseCoreRect, MinLocal, MaxLocal);

			const FVector2D SideCorners[4] = {
				FVector2D(MinLocal.X, MinLocal.Y),
				FVector2D(MaxLocal.X, MinLocal.Y),
				FVector2D(MaxLocal.X, MaxLocal.Y),
				FVector2D(MinLocal.X, MaxLocal.Y)};

			int32 VertexIndex = 0;
			for (int32 SideIndex = 0; SideIndex < 4; ++SideIndex)
			{
				const FVector2D SideStart = SideCorners[SideIndex];
				const FVector2D SideEnd = SideCorners[(SideIndex + 1) % 4];
				for (int32 SegmentIndex = 0; SegmentIndex < EdgeSegments; ++SegmentIndex)
				{
					const double Alpha = double(SegmentIndex) / double(EdgeSegments);
					const FVector2D VertexLocalXY = FMath::Lerp(SideStart, SideEnd, Alpha);

					double SurfaceZCm = FloorZLocal;
					if (Sampler.TryGetLocalSurfaceZCm(VertexLocalXY, SurfaceZCm))
					{
						SurfaceZCm += ChunkGridSurfaceLiftCm;
					}

					PerimeterWorld[VertexIndex] = Context.VolumeTransform.TransformPosition(
						FVector(VertexLocalXY.X, VertexLocalXY.Y, SurfaceZCm));
					++VertexIndex;
				}
			}

			const bool bOddChunk = ((Chunk.Id.X + Chunk.Id.Y) & 1) != 0;
			const FColor Color = bOddChunk ? FColor(255, 210, 60, 255) : FColor(60, 170, 255, 255);

			for (int32 Index = 0; Index < PerimeterVertexCount; ++Index)
			{
				const FVector& SegmentStart = PerimeterWorld[Index];
				const FVector& SegmentEnd = PerimeterWorld[(Index + 1) % PerimeterVertexCount];
				Entry.Lines.Add(UE::DebugDraw::FLineSubmission{SegmentStart, SegmentEnd, Color, Thickness, UE::DebugDraw::EDepthTest::WorldDepth});
			}

			if (Volume.bDrawLabels)
			{
				const FVector2D CenterLocal = (MinLocal + MaxLocal) * 0.5;
				Entry.Labels.Add(FArcForestDebugDrawCachedLabel{
					Context.VolumeTransform.TransformPosition(FVector(CenterLocal.X, CenterLocal.Y, FloorZLocal)),
					Color,
					FString::Printf(TEXT("chunk %d,%d"), Chunk.Id.X, Chunk.Id.Y)});
			}
		}

		ReportSurfaceSamples(Volume, Sampler, EArcForestDebugDrawSurfaceLayer::ChunkGrid, TEXT("chunk grid"));
	}

	void DrawChunkGrid(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		FSurfaceHeightSampler& Sampler,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		if (!Context.bMetadataValid)
		{
			return;
		}

		FArcForestDebugDrawGeometryKey Key;
		Key.VolumeId = Volume.VolumeId;
		Key.VolumeTransform = Context.VolumeTransform;
		Key.SiteSnapshotHash = Context.Metadata.SiteSnapshotHash;
		Key.MaxDrawn = Volume.MaxDrawnChunks;
		Key.bDrawLabels = Volume.bDrawLabels;
		Key.DebugLineThicknessCm = Volume.DebugLineThicknessCm;

		FArcForestDebugDrawGeometryCache::FChunkGridEntry* Entry = GeometryCache.ChunkGridEntries.Find(&Volume);
		if (Entry == nullptr || !Entry->Key.Matches(Key))
		{
			PruneStaleVolumeEntries(GeometryCache.ChunkGridEntries);
			Entry = &GeometryCache.ChunkGridEntries.FindOrAdd(&Volume);
			Entry->Key = Key;
			BuildChunkGridGeometry(Context, Volume, SiteReaderCache, Sampler, *Entry);
		}

		UE::DebugDraw::Lines(Scene, Entry->Lines);

		for (const FArcForestDebugDrawCachedLabel& CachedLabel : Entry->Labels)
		{
			EmitLabel(Scene, CachedLabel.WorldLocation, CachedLabel.Color, CachedLabel.Text);
		}
	}

	int32 ComputeInstancePartitionInstanceCount(const AArcEcotopeInstancePartitionActor& Actor)
	{
		int32 Count = 0;
		for (const TObjectPtr<UInstancedStaticMeshComponent>& Component : Actor.StaticMeshInstances)
		{
			if (Component != nullptr)
			{
				Count += Component->GetInstanceCount();
			}
		}
		for (const TObjectPtr<UInstancedSkinnedMeshComponent>& Component : Actor.SkinnedMeshInstances)
		{
			if (Component != nullptr)
			{
				Count += Component->GetInstanceCount();
			}
		}
		return Count;
	}

	void DrawInstancePartitions(
		const AArcEcotopeVolume& Volume,
		const double UniformScale,
		TConstArrayView<TWeakObjectPtr<AArcEcotopeInstancePartitionActor>> CachedInstancePartitionActors)
	{
		const FSceneInterface* const Scene = ActorScene(Volume);

		const FColor VioletColor(140, 60, 220, 255);

		for (const TWeakObjectPtr<AArcEcotopeInstancePartitionActor>& ActorPtr : CachedInstancePartitionActors)
		{
			const AArcEcotopeInstancePartitionActor* Actor = ActorPtr.Get();
			// OwnerGuid is readable without resolving the OwningVolume soft pointer every tick.
			if (Actor == nullptr || Actor->OwnerGuid != Volume.VolumeId)
			{
				continue;
			}

			const FBox Bounds = Actor->GetComponentsBoundingBox(/*bNonColliding*/ true);
			if (!Bounds.IsValid || Bounds.GetExtent().IsNearlyZero())
			{
				continue;
			}

			UE::DebugDraw::Box(Scene, Bounds.GetCenter(), Bounds.GetExtent(), FQuat::Identity, VioletColor,
				float(double(Volume.DebugLineThicknessCm) * UniformScale), UE::DebugDraw::EDepthTest::WorldDepth);

			if (Volume.bDrawLabels)
			{
				EmitLabel(Scene, Bounds.GetCenter(), VioletColor,
					FString::Printf(TEXT("%d instances"), ComputeInstancePartitionInstanceCount(*Actor)));
			}
		}
	}

	void DrawStandLayers(
		const FArcForestDebugDrawContext& Context,
		const AArcEcotopeVolume& Volume,
		FSurfaceHeightSampler& Sampler,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
		const UArcForestSimulationState* SimulationState = Volume.SimulationState;
		if (SimulationState == nullptr)
		{
			return;
		}

		const FArcForestWorldSnapshot& Snapshot = SimulationState->Snapshot;

		if (Volume.bDrawIndividuals)
		{
			DrawCachedStandLayer(GeometryCache.IndividualEntries, Volume,
				MakeStandGeometryKey(Context, Volume, Snapshot, Volume.MaxDrawnStems),
				[&Context, &Volume, &Snapshot](FArcForestDebugDrawGeometryCache::FStandEntry& Entry) -> void
				{
					BuildIndividualGeometry(Context, Volume, Snapshot, Entry);
				});
		}

		if (Volume.bDrawDeadwood)
		{
			DrawCachedStandLayer(GeometryCache.DeadwoodEntries, Volume,
				MakeStandGeometryKey(Context, Volume, Snapshot, Volume.MaxDrawnDeadwood),
				[&Context, &Volume, &Snapshot](FArcForestDebugDrawGeometryCache::FStandEntry& Entry) -> void
				{
					BuildDeadwoodGeometry(Context, Volume, Snapshot, Entry);
				});
		}

		if (!Volume.bDrawCohorts && !Volume.bDrawPatches)
		{
			return;
		}

		if (!Context.bMetadataValid)
		{
			if (ShouldLogOnce(EArcForestDebugDrawWarning::MissingMetadata, Volume.VolumeId))
			{
				UE_LOGF(LogArcEcotope, Warning,"ArcForestDebugDraw: %ls has no usable site metadata at runtime; cohorts and patches carry only a cell index and are not drawn.",
					*Volume.GetName());
			}
			return;
		}
		ClearLogLatch(EArcForestDebugDrawWarning::MissingMetadata, Volume.VolumeId);

		if (Volume.bDrawCohorts)
		{
			DrawCachedStandLayer(GeometryCache.CohortEntries, Volume,
				MakeStandGeometryKey(Context, Volume, Snapshot, Volume.MaxDrawnCellRecords),
				[&Context, &Volume, &Snapshot, &Sampler](FArcForestDebugDrawGeometryCache::FStandEntry& Entry) -> void
				{
					BuildCohortGeometry(Context, Volume, Snapshot, Sampler, Entry);
				});
		}

		if (Volume.bDrawPatches)
		{
			DrawCachedStandLayer(GeometryCache.PatchEntries, Volume,
				MakeStandGeometryKey(Context, Volume, Snapshot, Volume.MaxDrawnCellRecords),
				[&Context, &Volume, &Snapshot, &Sampler](FArcForestDebugDrawGeometryCache::FStandEntry& Entry) -> void
				{
					BuildPatchGeometry(Context, Volume, Snapshot, Sampler, Entry);
				});
		}
	}
}

namespace ArcForestDebugDraw
{
	void DrawVolume(
		const AArcClimateVolume& Volume,
		FArcForestDebugDrawGeometryCache& GeometryCache)
	{
#if WITH_EDITOR
		using namespace ArcForestDebugDrawPrivate;
		if (Volume.bDrawClimateField)
		{
			DrawClimateField(Volume, GeometryCache);
		}
#endif
	}

	void DrawVolume(
		const AArcEcotopeVolume& Volume,
		FArcForestDebugDrawSiteReaderCache& SiteReaderCache,
		FArcForestDebugDrawGeometryCache& GeometryCache,
		TConstArrayView<TWeakObjectPtr<AArcEcotopeInstancePartitionActor>> CachedInstancePartitionActors)
	{
		using namespace ArcForestDebugDrawPrivate;

		FArcForestDebugDrawContext Context;
		if (!TryBuildContext(Volume, Context))
		{
			return;
		}

		FSurfaceHeightSampler Sampler(Context, Volume, SiteReaderCache);

		if (Volume.bDrawVolumeBounds)
		{
			DrawVolumeBounds(Volume, Context.UniformScale);
		}

		if (Volume.bDrawSiteField)
		{
			DrawSiteField(Context, Volume, SiteReaderCache, GeometryCache);
		}

		if (Volume.bDrawTerrainDataMap)
		{
			DrawTerrainDataMap(Context, Volume, GeometryCache);
		}

		if (Volume.bDrawDisturbancePreview)
		{
			DrawDisturbancePreview(Context, Volume, GeometryCache);
		}

		if (Volume.bDrawChunkGrid)
		{
			DrawChunkGrid(Context, Volume, SiteReaderCache, Sampler, GeometryCache);
		}

		if (Volume.bDrawInstancePartitions)
		{
			DrawInstancePartitions(Volume, Context.UniformScale, CachedInstancePartitionActors);
		}

		DrawStandLayers(Context, Volume, Sampler, GeometryCache);
	}
}

#endif // UE_DEBUG_RENDERING_ENABLED
