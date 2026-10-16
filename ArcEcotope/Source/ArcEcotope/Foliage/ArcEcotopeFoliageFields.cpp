// Copyright Lukasz Baran. All Rights Reserved.

#include "Foliage/ArcEcotopeFoliageFields.h"

#include "ArcEcotopeModule.h"
#include "Foliage/ArcEcotopeFoliageFieldCache.h"
#include "Foliage/ArcEcotopeFoliageFieldPayload.h"
#include "Foliage/ArcEcotopeRecordRaster.h"
#include "Hash/Blake3.h"
#include "Memory/SharedBuffer.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#include "Determinism/ArcForestDeterministic.h"
#include "HAL/IConsoleManager.h"
#include "Math/Float16Color.h"
#include "Misc/LargeWorldRenderPosition.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "Simulation/ArcForestSimulationState.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcForestWorldSnapshot.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "SystemTextures.h"

namespace ArcEcotopeFoliageFieldsPrivate
{
	constexpr double TargetTexelSizeCm = ArcEcotopeFoliageFieldLimits::TargetTexelSizeCm;
	constexpr int32 MaxFieldTexels = ArcEcotopeFoliageFieldLimits::MaxFieldTexels;
	constexpr float MaxOccupancy = ArcEcotopeFoliageFieldLimits::MaxOccupancy;

// Everything from here to the LWC split reads the baked site and the committed stand. Only the
// editor-side derivation does that, so a cooked build carries none of it.
#if WITH_EDITOR
	using UE::ArcEcotope::FFieldRaster;
	using UE::ArcEcotope::FSpeciesCrown;
	using UE::ArcEcotope::TexelCentreLocal;

	TAutoConsoleVariable<int32> CVarFoliageReaderBudgetMiB(
		TEXT("arcecotope.SiteChunkReader.FoliageBudgetMiB"),
		64,
		TEXT("Cache budget in MiB for ArcEcotope foliage-field site reads; clamped to at least 1 MiB."),
		ECVF_Default);

	constexpr uint32 FoliageFieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::Moisture))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::Fertility))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::BareRock))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::ChannelExclusion))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::EstablishmentWear))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::SubstrateMobility));

	/** Scalar-only site description used by canopy geometry and raster ownership. */
	struct FFoliageSiteMetadata
	{
		FBox LocalBounds = FBox(EForceInit::ForceInit);
		float CellSizeCm = 0.0f;
		FIntPoint GridDimensions = FIntPoint::ZeroValue;
		uint8 RecordGridLevel = 0;
		uint8 MoistureLevel = 0;
		uint8 FertilityLevel = 0;
		uint8 BareRockLevel = 0;
		uint8 ChannelExclusionLevel = 0;
		uint8 EstablishmentWearLevel = 0;
		uint8 SubstrateMobilityLevel = 0;
		uint64 SiteSnapshotHash = 0;

		static FFoliageSiteMetadata FromFrozen(const FArcEcotopeSiteMetadata& Metadata)
		{
			FFoliageSiteMetadata Result;
			Result.LocalBounds = Metadata.LocalBounds;
			Result.CellSizeCm = Metadata.CellSizeCm;
			Result.GridDimensions = Metadata.GridDimensions;
			Result.RecordGridLevel = Metadata.RecordGridLevel;
			Result.MoistureLevel = Metadata.MoistureLevel;
			Result.FertilityLevel = Metadata.FertilityLevel;
			Result.BareRockLevel = Metadata.BareRockLevel;
			Result.ChannelExclusionLevel = Metadata.ChannelExclusionLevel;
			Result.EstablishmentWearLevel = Metadata.EstablishmentWearLevel;
			Result.SubstrateMobilityLevel = Metadata.SubstrateMobilityLevel;
			Result.SiteSnapshotHash = Metadata.SiteSnapshotHash;
			return Result;
		}

		FIntPoint GetLevelDimensions(const uint8 Level) const
		{
			if (Level > FArcEcotopeSiteMetadata::MaxGridLevel
				|| GridDimensions.X <= 0 || GridDimensions.Y <= 0)
			{
				return FIntPoint::ZeroValue;
			}
			const int32 Divisor = static_cast<int32>(1) << Level;
			return FIntPoint(
				FMath::DivideAndRoundUp(GridDimensions.X, Divisor),
				FMath::DivideAndRoundUp(GridDimensions.Y, Divisor));
		}

		int32 GetLevelCellCount(const uint8 Level) const
		{
			const FIntPoint Dimensions = GetLevelDimensions(Level);
			const int64 Count = static_cast<int64>(Dimensions.X) * Dimensions.Y;
			return Dimensions.X > 0 && Dimensions.Y > 0 && Count <= MAX_int32
				? static_cast<int32>(Count)
				: 0;
		}

		double GetLevelCellSizeCm(const uint8 Level) const
		{
			return Level <= FArcEcotopeSiteMetadata::MaxGridLevel
				&& FMath::IsFinite(CellSizeCm) && CellSizeCm > 0.0f
				? static_cast<double>(CellSizeCm) * static_cast<double>(static_cast<int32>(1) << Level)
				: 0.0;
		}

		bool TryGetBaseCellIndex(
			const int32 CellX,
			const int32 CellY,
			int32& OutCellIndex) const
		{
			OutCellIndex = INDEX_NONE;
			if (CellX < 0 || CellY < 0 || GridDimensions.X <= 0 || GridDimensions.Y <= 0
				|| CellX >= GridDimensions.X || CellY >= GridDimensions.Y)
			{
				return false;
			}
			const int64 CellCount = static_cast<int64>(GridDimensions.X) * GridDimensions.Y;
			const int64 CellIndex = static_cast<int64>(CellX) + static_cast<int64>(CellY) * GridDimensions.X;
			if (CellCount <= 0 || CellCount > MAX_int32 || CellIndex < 0 || CellIndex >= CellCount)
			{
				return false;
			}
			OutCellIndex = static_cast<int32>(CellIndex);
			return true;
		}

		bool TryGetBaseCellIndexForLocalPosition(
			const FVector2D& LocalPosition,
			int32& OutCellIndex) const
		{
			OutCellIndex = INDEX_NONE;
			if (!FMath::IsFinite(LocalPosition.X) || !FMath::IsFinite(LocalPosition.Y)
				|| !FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f || !LocalBounds.IsValid
				|| LocalPosition.X < LocalBounds.Min.X || LocalPosition.X > LocalBounds.Max.X
				|| LocalPosition.Y < LocalBounds.Min.Y || LocalPosition.Y > LocalBounds.Max.Y)
			{
				return false;
			}
			const int32 CellX = FMath::Clamp(
				FMath::FloorToInt((LocalPosition.X - LocalBounds.Min.X) / CellSizeCm), 0, GridDimensions.X - 1);
			const int32 CellY = FMath::Clamp(
				FMath::FloorToInt((LocalPosition.Y - LocalBounds.Min.Y) / CellSizeCm), 0, GridDimensions.Y - 1);
			return TryGetBaseCellIndex(CellX, CellY, OutCellIndex);
		}

		bool TryGetLevelCellRect(
			const int32 CellIndex,
			const uint8 Level,
			FVector2D& OutMinLocal,
			FVector2D& OutMaxLocal) const
		{
			OutMinLocal = FVector2D::ZeroVector;
			OutMaxLocal = FVector2D::ZeroVector;
			const FIntPoint Dimensions = GetLevelDimensions(Level);
			const int32 CellCount = GetLevelCellCount(Level);
			const double LevelCellSizeCm = GetLevelCellSizeCm(Level);
			if (CellCount <= 0 || LevelCellSizeCm <= 0.0 || !LocalBounds.IsValid
				|| CellIndex < 0 || CellIndex >= CellCount)
			{
				return false;
			}
			const int32 CellX = CellIndex % Dimensions.X;
			const int32 CellY = CellIndex / Dimensions.X;
			const double CellMinX = LocalBounds.Min.X + static_cast<double>(CellX) * LevelCellSizeCm;
			const double CellMinY = LocalBounds.Min.Y + static_cast<double>(CellY) * LevelCellSizeCm;
			OutMinLocal = FVector2D(CellMinX, CellMinY);
			OutMaxLocal = FVector2D(
				FMath::Min(CellMinX + LevelCellSizeCm, static_cast<double>(LocalBounds.Max.X)),
				FMath::Min(CellMinY + LevelCellSizeCm, static_cast<double>(LocalBounds.Max.Y)));
			return true;
		}
	};

	struct FFoliageSiteContext
	{
		FFoliageSiteMetadata Metadata;
		FArcEcotopeChunkLayout Layout;
		FArcEcotopeSiteChunkReader Reader;
		FArcEcotopeFrozenSiteSourcePtr FrozenSource;
		int64 ReaderBudgetBytes = 0;
	};

	bool OpenFoliageSiteContext(
		AArcEcotopeVolume& Volume,
		FFoliageSiteContext& OutContext,
		FText& OutError)
	{
		OutContext = FFoliageSiteContext();
		OutContext.ReaderBudgetBytes = static_cast<int64>(FMath::Max(
			CVarFoliageReaderBudgetMiB.GetValueOnGameThread(), 1)) * 1024ll * 1024ll;
		if (!Volume.FreezeSiteChunkSource(OutContext.FrozenSource, OutError))
		{
			return false;
		}
		OutContext.Metadata = FFoliageSiteMetadata::FromFrozen(OutContext.FrozenSource->Metadata);
		OutContext.Layout = OutContext.FrozenSource->Layout;
		if (OutContext.Metadata.SiteSnapshotHash == 0
			|| !OutContext.Metadata.LocalBounds.IsValid
			|| !FMath::IsFinite(OutContext.Metadata.CellSizeCm)
			|| OutContext.Metadata.CellSizeCm <= 0.0f
			|| OutContext.Metadata.GridDimensions.X <= 0
			|| OutContext.Metadata.GridDimensions.Y <= 0
			|| OutContext.Metadata.MoistureLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| OutContext.Metadata.FertilityLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| OutContext.Metadata.BareRockLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| OutContext.Metadata.ChannelExclusionLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| OutContext.Metadata.EstablishmentWearLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| OutContext.Metadata.SubstrateMobilityLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| OutContext.Layout.GetBaseDimensions() != OutContext.Metadata.GridDimensions
			|| OutContext.Layout.GetChunkCount() <= 0)
		{
			OutError = FText::FromString(
				TEXT("ArcEcotope foliage site metadata and chunk layout are incompatible."));
			return false;
		}

		FString ReaderError;
		if (!OutContext.Reader.OpenEditorBulk(
				OutContext.FrozenSource, OutContext.ReaderBudgetBytes, ReaderError))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope foliage could not open its site reader: %s"), *ReaderError));
			return false;
		}
		return true;
	}

	bool TryGetChunkLocalRect(
		const FFoliageSiteMetadata& Metadata,
		const FIntRect& BaseCoreRect,
		FVector2D& OutMinLocal,
		FVector2D& OutMaxLocal)
	{
		OutMinLocal = FVector2D::ZeroVector;
		OutMaxLocal = FVector2D::ZeroVector;
		if (!Metadata.LocalBounds.IsValid || !FMath::IsFinite(Metadata.CellSizeCm)
			|| Metadata.CellSizeCm <= 0.0f
			|| BaseCoreRect.Min.X < 0 || BaseCoreRect.Min.Y < 0
			|| BaseCoreRect.Max.X > Metadata.GridDimensions.X
			|| BaseCoreRect.Max.Y > Metadata.GridDimensions.Y
			|| BaseCoreRect.Width() <= 0 || BaseCoreRect.Height() <= 0)
		{
			return false;
		}

		const double MinX = Metadata.LocalBounds.Min.X
			+ static_cast<double>(BaseCoreRect.Min.X) * Metadata.CellSizeCm;
		const double MinY = Metadata.LocalBounds.Min.Y
			+ static_cast<double>(BaseCoreRect.Min.Y) * Metadata.CellSizeCm;
		const double MaxX = FMath::Min(
			Metadata.LocalBounds.Min.X + static_cast<double>(BaseCoreRect.Max.X) * Metadata.CellSizeCm,
			static_cast<double>(Metadata.LocalBounds.Max.X));
		const double MaxY = FMath::Min(
			Metadata.LocalBounds.Min.Y + static_cast<double>(BaseCoreRect.Max.Y) * Metadata.CellSizeCm,
			static_cast<double>(Metadata.LocalBounds.Max.Y));
		if (!FMath::IsFinite(MinX) || !FMath::IsFinite(MinY)
			|| !FMath::IsFinite(MaxX) || !FMath::IsFinite(MaxY)
			|| MaxX < MinX || MaxY < MinY)
		{
			return false;
		}

		OutMinLocal = FVector2D(MinX, MinY);
		OutMaxLocal = FVector2D(MaxX, MaxY);
		return true;
	}

	bool TryMakeRasterCandidateRect(
		const FFieldRaster& Raster,
		const FVector2D& MinLocal,
		const FVector2D& MaxLocal,
		FIntRect& OutRect)
	{
		OutRect = FIntRect();
		if (Raster.Dimensions.X <= 0 || Raster.Dimensions.Y <= 0
			|| !FMath::IsFinite(Raster.TexelSizeCm) || Raster.TexelSizeCm <= 0.0
			|| !FMath::IsFinite(MinLocal.X) || !FMath::IsFinite(MinLocal.Y)
			|| !FMath::IsFinite(MaxLocal.X) || !FMath::IsFinite(MaxLocal.Y)
			|| MaxLocal.X < MinLocal.X || MaxLocal.Y < MinLocal.Y)
		{
			return false;
		}

		auto MakeAxisRange = [](const double MinValue, const double MaxValue, const double Origin,
			const double TexelSize, const int32 Dimension, int32& OutMin, int32& OutMax)
		{
			const double MinTexel = (MinValue - Origin) / TexelSize - 0.5;
			const double MaxTexel = (MaxValue - Origin) / TexelSize - 0.5;
			if (!FMath::IsFinite(MinTexel) || !FMath::IsFinite(MaxTexel))
			{
				return false;
			}
			const double ClampedMin = FMath::Clamp(FMath::CeilToDouble(MinTexel), 0.0, static_cast<double>(Dimension));
			const double ClampedMax = FMath::Clamp(
				FMath::FloorToDouble(MaxTexel) + 1.0, 0.0, static_cast<double>(Dimension));
			OutMin = static_cast<int32>(ClampedMin);
			OutMax = static_cast<int32>(ClampedMax);
			return OutMax >= OutMin;
		};

		return MakeAxisRange(
				MinLocal.X, MaxLocal.X, Raster.OriginLocal.X, Raster.TexelSizeCm,
				Raster.Dimensions.X, OutRect.Min.X, OutRect.Max.X)
			&& MakeAxisRange(
				MinLocal.Y, MaxLocal.Y, Raster.OriginLocal.Y, Raster.TexelSizeCm,
				Raster.Dimensions.Y, OutRect.Min.Y, OutRect.Max.Y);
	}

	bool IsExpectedFieldView(
		const FArcEcotopeSiteFloatFieldView& View,
		const FFoliageSiteMetadata& Metadata,
		const uint8 ExpectedLevel)
	{
		return View.IsValid()
			&& View.Level == ExpectedLevel
			&& View.GlobalDimensions == Metadata.GetLevelDimensions(ExpectedLevel);
	}

	bool ValidateFoliageChunkView(
		const FArcEcotopeSiteChunkView& View,
		const FArcEcotopeChunkRect& Chunk,
		const FFoliageSiteMetadata& Metadata)
	{
		return (View.FieldMask & FoliageFieldMask) == FoliageFieldMask
			&& View.Id == Chunk.Id
			&& View.BaseCoreRect == Chunk.BaseCoreRect
			&& View.BaseReadRect == Chunk.BaseCoreRect
			&& IsExpectedFieldView(View.Moisture, Metadata, Metadata.MoistureLevel)
			&& IsExpectedFieldView(View.Fertility, Metadata, Metadata.FertilityLevel)
			&& IsExpectedFieldView(View.BareRock, Metadata, Metadata.BareRockLevel)
			&& IsExpectedFieldView(View.ChannelExclusion, Metadata, Metadata.ChannelExclusionLevel)
			&& IsExpectedFieldView(View.EstablishmentWear, Metadata, Metadata.EstablishmentWearLevel)
			&& IsExpectedFieldView(View.SubstrateMobility, Metadata, Metadata.SubstrateMobilityLevel);
	}

	bool TrySampleFoliageFields(
		const FArcEcotopeSiteChunkView& View,
		const FFoliageSiteMetadata& Metadata,
		const int32 BaseCellIndex,
		float& OutMoisture,
		float& OutFertility,
		float& OutBareRock,
		float& OutChannelExclusion,
		float& OutEstablishmentWear,
		float& OutSubstrateMobility)
	{
		const int64 BaseCellCount = static_cast<int64>(Metadata.GridDimensions.X) * Metadata.GridDimensions.Y;
		if (Metadata.GridDimensions.X <= 0 || BaseCellIndex < 0 || BaseCellIndex >= BaseCellCount)
		{
			return false;
		}
		const int32 BaseX = BaseCellIndex % Metadata.GridDimensions.X;
		const int32 BaseY = BaseCellIndex / Metadata.GridDimensions.X;
		const float* Moisture = View.Moisture.TryGet(
			BaseX >> View.Moisture.Level, BaseY >> View.Moisture.Level);
		const float* Fertility = View.Fertility.TryGet(
			BaseX >> View.Fertility.Level, BaseY >> View.Fertility.Level);
		const float* BareRock = View.BareRock.TryGet(
			BaseX >> View.BareRock.Level, BaseY >> View.BareRock.Level);
		const float* ChannelExclusion = View.ChannelExclusion.TryGet(
			BaseX >> View.ChannelExclusion.Level, BaseY >> View.ChannelExclusion.Level);
		const float* EstablishmentWear = View.EstablishmentWear.TryGet(
			BaseX >> View.EstablishmentWear.Level, BaseY >> View.EstablishmentWear.Level);
		const float* SubstrateMobility = View.SubstrateMobility.TryGet(
			BaseX >> View.SubstrateMobility.Level, BaseY >> View.SubstrateMobility.Level);
		if (Moisture == nullptr || Fertility == nullptr || BareRock == nullptr
			|| ChannelExclusion == nullptr || EstablishmentWear == nullptr || SubstrateMobility == nullptr)
		{
			return false;
		}
		OutMoisture = *Moisture;
		OutFertility = *Fertility;
		OutBareRock = *BareRock;
		OutChannelExclusion = *ChannelExclusion;
		OutEstablishmentWear = *EstablishmentWear;
		OutSubstrateMobility = *SubstrateMobility;
		return true;
	}

#endif // WITH_EDITOR

	/** The volume transform, split so the shader never multiplies a world position by a matrix carrying
	 *  a multi-kilometre translation. Mirrors SplitTerrainTransformLWC in ArcEcotopeFoliageAtlas.cpp. */
	void SplitVolumeTransformLWC(const FTransform& VolumeTransform, FVector3f& OutLWCTile,
		FMatrix44f& OutWorldToLocal)
	{
		FTransform TileRelative = VolumeTransform;
		const FLargeWorldRenderPosition Origin(VolumeTransform.GetLocation());

		OutLWCTile = Origin.GetTile();
		TileRelative.SetLocation(FVector(Origin.GetOffset()));

		OutWorldToLocal = FMatrix44f(TileRelative.ToMatrixWithScale().Inverse());
	}
}

namespace ArcEcotopeFoliageFields
{

#if WITH_EDITOR
static bool RasterizeFieldPayload(
	AArcEcotopeVolume& Volume,
	FArcEcotopeFoliageFieldPayload& OutPayload,
	FText& OutError)
{
	using namespace ArcEcotopeFoliageFieldsPrivate;

	check(IsInGameThread());
	OutPayload.Reset();
	OutError = FText::GetEmpty();

	const UArcForestSimulationState* State = Volume.SimulationState;
	if (State == nullptr || State->Status != EArcForestSimulationStateStatus::Valid)
	{
		OutError = FText::FromString(TEXT("ArcEcotope foliage fields require a valid committed simulation snapshot."));
		return false;
	}

	FFoliageSiteContext SiteContext;
	if (!OpenFoliageSiteContext(Volume, SiteContext, OutError))
	{
		return false;
	}
	const FFoliageSiteMetadata& Site = SiteContext.Metadata;
	const FBox& LocalBounds = Site.LocalBounds;

	const double SizeXCm = LocalBounds.Max.X - LocalBounds.Min.X;
	const double SizeYCm = LocalBounds.Max.Y - LocalBounds.Min.Y;
	if (!(SizeXCm > 0.0) || !(SizeYCm > 0.0) || !FMath::IsFinite(SizeXCm) || !FMath::IsFinite(SizeYCm))
	{
		OutError = FText::FromString(TEXT("ArcEcotope foliage site bounds have no finite positive XY extent."));
		return false;
	}

	FFieldRaster Raster;
	Raster.OriginLocal = FVector2D(LocalBounds.Min.X, LocalBounds.Min.Y);
	Raster.TexelSizeCm = TargetTexelSizeCm;
	while (SizeXCm / Raster.TexelSizeCm > MaxFieldTexels
		|| SizeYCm / Raster.TexelSizeCm > MaxFieldTexels)
	{
		Raster.TexelSizeCm *= 2.0;
		if (!FMath::IsFinite(Raster.TexelSizeCm))
		{
			OutError = FText::FromString(TEXT("ArcEcotope foliage raster texel size overflowed."));
			return false;
		}
	}
	Raster.Dimensions = FIntPoint(
		FMath::CeilToInt32(SizeXCm / Raster.TexelSizeCm),
		FMath::CeilToInt32(SizeYCm / Raster.TexelSizeCm));

	const int64 TexelCount = static_cast<int64>(Raster.Dimensions.X) * Raster.Dimensions.Y;
	if (Raster.Dimensions.X <= 0 || Raster.Dimensions.Y <= 0
		|| Raster.Dimensions.X > MaxFieldTexels || Raster.Dimensions.Y > MaxFieldTexels
		|| TexelCount <= 0 || TexelCount > MAX_int32)
	{
		OutError = FText::FromString(TEXT("ArcEcotope foliage raster dimensions are not representable."));
		return false;
	}

	if (Raster.TexelSizeCm > TargetTexelSizeCm)
	{
		UE_LOGF(LogArcEcotope, Log,"[Foliage] '%ls' spans %.0f x %.0f cm, so its forest field uses a %.0f cm texel instead of %.0f cm.",
			*Volume.GetName(), SizeXCm, SizeYCm, Raster.TexelSizeCm, TargetTexelSizeCm);
	}

	Raster.Occupancy.SetNumZeroed(static_cast<int32>(TexelCount));

	const TMap<FGuid, FSpeciesCrown> SpeciesById =
		UE::ArcEcotope::BuildSpeciesCrowns(Volume.GetEcotopeOrDefault());

	UE::ArcEcotope::RasterizeRecordOccupancy(State->Snapshot, SiteContext.FrozenSource->Metadata,
		SpeciesById, TOptional<FGuid>(), Raster);

	TArray<FFloat16Color> Texels;
	Texels.SetNumUninitialized(Raster.Occupancy.Num());
	int64 ExpectedSampledTexels = 0;
	for (int32 Y = 0; Y < Raster.Dimensions.Y; ++Y)
	{
		for (int32 X = 0; X < Raster.Dimensions.X; ++X)
		{
			const int32 TexelIndex = X + Y * Raster.Dimensions.X;
			int32 BaseCellIndex = INDEX_NONE;
			if (Site.TryGetBaseCellIndexForLocalPosition(TexelCentreLocal(Raster, X, Y), BaseCellIndex))
			{
				++ExpectedSampledTexels;
			}
			Texels[TexelIndex] = FFloat16Color(FLinearColor(
				FMath::Clamp(Raster.Occupancy[TexelIndex], 0.0f, MaxOccupancy), 1.0f, 1.0f, 0.0f));
		}
	}

	int32 VisitedChunks = 0;
	int64 PrimaryAcquisitions = 0;
	int64 FailedPrimaryAcquisitions = 0;
	int64 SampledTexels = 0;
	for (const FArcEcotopeChunkRect& Chunk : SiteContext.Layout.GetChunks())
	{
		FVector2D ChunkMinLocal = FVector2D::ZeroVector;
		FVector2D ChunkMaxLocal = FVector2D::ZeroVector;
		FIntRect CandidateRect;
		if (!TryGetChunkLocalRect(Site, Chunk.BaseCoreRect, ChunkMinLocal, ChunkMaxLocal)
			|| !TryMakeRasterCandidateRect(Raster, ChunkMinLocal, ChunkMaxLocal, CandidateRect))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope foliage could not map chunk (%d,%d) into its raster."), Chunk.Id.X, Chunk.Id.Y));
			return false;
		}
		if (CandidateRect.Width() <= 0 || CandidateRect.Height() <= 0)
		{
			continue;
		}

		++VisitedChunks;
		++PrimaryAcquisitions;
		FArcEcotopeSiteChunkLease Lease;
		FString AcquireError;
		if (!SiteContext.Reader.AcquireFields(
				Chunk.Id, 0, FoliageFieldMask, Lease, AcquireError))
		{
			++FailedPrimaryAcquisitions;
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope foliage failed site acquire for chunk (%d,%d) after %lld/%d visits: %s"),
				Chunk.Id.X, Chunk.Id.Y, PrimaryAcquisitions, VisitedChunks, *AcquireError));
			return false;
		}
		const FArcEcotopeSiteChunkView& View = Lease.GetView();
		if (!ValidateFoliageChunkView(View, Chunk, Site))
		{
			OutError = FText::FromString(FString::Printf(
				TEXT("ArcEcotope foliage received an invalid field window for chunk (%d,%d)."), Chunk.Id.X, Chunk.Id.Y));
			return false;
		}

		for (int32 Y = CandidateRect.Min.Y; Y < CandidateRect.Max.Y; ++Y)
		{
			for (int32 X = CandidateRect.Min.X; X < CandidateRect.Max.X; ++X)
			{
				int32 BaseCellIndex = INDEX_NONE;
				if (!Site.TryGetBaseCellIndexForLocalPosition(
						TexelCentreLocal(Raster, X, Y), BaseCellIndex))
				{
					continue;
				}
				const int32 BaseX = BaseCellIndex % Site.GridDimensions.X;
				const int32 BaseY = BaseCellIndex / Site.GridDimensions.X;
				if (BaseX < Chunk.BaseCoreRect.Min.X || BaseX >= Chunk.BaseCoreRect.Max.X
					|| BaseY < Chunk.BaseCoreRect.Min.Y || BaseY >= Chunk.BaseCoreRect.Max.Y)
				{
					continue;
				}

				float Moisture = 0.0f;
				float Fertility = 0.0f;
				float BareRock = 0.0f;
				float ChannelExclusion = 0.0f;
				float EstablishmentWear = 0.0f;
				float SubstrateMobility = 0.0f;
				if (!TrySampleFoliageFields(
						View, Site, BaseCellIndex, Moisture, Fertility,
						BareRock, ChannelExclusion, EstablishmentWear, SubstrateMobility))
				{
					OutError = FText::FromString(FString::Printf(
						TEXT("ArcEcotope foliage field window does not cover base cell (%d,%d) in chunk (%d,%d)."),
						BaseX, BaseY, Chunk.Id.X, Chunk.Id.Y));
					return false;
				}

				const float GroundCoverSuppression = FMath::Max(
					FMath::Max(BareRock, ChannelExclusion),
					FMath::Max(EstablishmentWear, SubstrateMobility));

				const int32 TexelIndex = X + Y * Raster.Dimensions.X;
				Texels[TexelIndex] = FFloat16Color(FLinearColor(
					FMath::Clamp(Raster.Occupancy[TexelIndex], 0.0f, MaxOccupancy),
					Moisture, Fertility, GroundCoverSuppression));
				++SampledTexels;
			}
		}
	}

	if (PrimaryAcquisitions != VisitedChunks || SampledTexels != ExpectedSampledTexels)
	{
		OutError = FText::FromString(FString::Printf(
			TEXT("ArcEcotope foliage ownership invariant failed: chunks=%d primary=%lld sampled=%lld expected=%lld."),
			VisitedChunks, PrimaryAcquisitions, SampledTexels, ExpectedSampledTexels));
		return false;
	}

	const int64 DefaultTexels = TexelCount - SampledTexels;
#if WITH_EDITORONLY_DATA
	{
		const FArcEcotopeSiteBulkReaderStats Stats = SiteContext.Reader.GetBulkStats();
		UE_LOGF(LogArcEcotope, Log,"[FoliageFields] volume='%ls' source=editor-bulk chunks=%d/%d primary=%lld failed=%lld sampled=%lld default=%lld reader_peak=%.2f/%.2fMiB resident_peak=%.2fMiB pinned_peak=%.2fMiB scratch_peak=%.2fMiB evictions=%llu source_loaded=%.2fMiB",
			*Volume.GetName(), VisitedChunks, SiteContext.Layout.GetChunkCount(),
			PrimaryAcquisitions, FailedPrimaryAcquisitions,
			SampledTexels, DefaultTexels,
			static_cast<double>(Stats.PeakTotalBytes) / (1024.0 * 1024.0),
			static_cast<double>(Stats.BudgetBytes) / (1024.0 * 1024.0),
			static_cast<double>(Stats.PeakResidentBytes) / (1024.0 * 1024.0),
			static_cast<double>(Stats.PeakPinnedBytes) / (1024.0 * 1024.0),
			static_cast<double>(Stats.PeakScratchBytes) / (1024.0 * 1024.0),
			static_cast<unsigned long long>(Stats.EvictionCount),
			static_cast<double>(Stats.SourceLoadedBytes) / (1024.0 * 1024.0));
	}
#endif

	OutPayload.Dimensions = Raster.Dimensions;
	OutPayload.TexelSizeCm = Raster.TexelSizeCm;
	OutPayload.OriginLocal = Raster.OriginLocal;
	OutPayload.SourceSiteSnapshotHash = Site.SiteSnapshotHash;
	OutPayload.SourceStateChecksum = State->Snapshot.StateChecksum;
	OutPayload.PayloadRev = ArcEcotopeFoliageFieldLimits::FieldPayloadRev;

	OutPayload.Texels.SetNumUninitialized(Texels.Num() * sizeof(FFloat16Color));
	FMemory::Memcpy(OutPayload.Texels.GetData(), Texels.GetData(), OutPayload.Texels.Num());

	return true;
}
#endif // WITH_EDITOR

#if WITH_EDITOR
static FIoHash ComputeFieldCacheKey(
	const AArcEcotopeVolume& Volume,
	const UArcForestSimulationState& State)
{
	FBlake3 Hash;

	// Moving this GUID is the only lever that invalidates a poisoned blob.
	static const FGuid FormatVersionKey(TEXT("A6E31D07-59B4-4C82-9F15-C7B0D8E24A63"));
	Hash.Update(&FormatVersionKey, sizeof(FormatVersionKey));

	const int32 PayloadRev = ArcEcotopeFoliageFieldLimits::FieldPayloadRev;
	Hash.Update(&PayloadRev, sizeof(PayloadRev));

	const uint64 SiteSnapshotHash = Volume.GetActiveSiteMetadata().SiteSnapshotHash;
	Hash.Update(&SiteSnapshotHash, sizeof(SiteSnapshotHash));

	const uint64 StateChecksum = State.Snapshot.StateChecksum;
	Hash.Update(&StateChecksum, sizeof(StateChecksum));

	const int32 ChunkBaseCellSpan = Volume.ChunkBaseCellSpan;
	Hash.Update(&ChunkBaseCellSpan, sizeof(ChunkBaseCellSpan));

	const double TargetTexelSizeCm = ArcEcotopeFoliageFieldLimits::TargetTexelSizeCm;
	Hash.Update(&TargetTexelSizeCm, sizeof(TargetTexelSizeCm));

	const int32 MaxFieldTexels = ArcEcotopeFoliageFieldLimits::MaxFieldTexels;
	Hash.Update(&MaxFieldTexels, sizeof(MaxFieldTexels));

	// The crown terms RasterizeRecordOccupancy reads live on the species assets, so an edit there has to move
	// the key even though the committed snapshot is untouched.
	TArray<TPair<FGuid, uint64>> SpeciesEcology;
	SpeciesEcology.Reserve(Volume.GetEcotopeOrDefault().SpeciesSetups.Num());
	for (const FArcEcotopeSpeciesSetup& Setup : Volume.GetEcotopeOrDefault().SpeciesSetups)
	{
		if (Setup.Species != nullptr)
		{
			SpeciesEcology.Emplace(Setup.Species->SpeciesId, Setup.Species->ComputeEcologyHash());
		}
	}
	SpeciesEcology.Sort([](const TPair<FGuid, uint64>& Left, const TPair<FGuid, uint64>& Right)
	{
		return Left.Key < Right.Key;
	});

	const int32 SpeciesCount = SpeciesEcology.Num();
	Hash.Update(&SpeciesCount, sizeof(SpeciesCount));
	for (const TPair<FGuid, uint64>& Entry : SpeciesEcology)
	{
		Hash.Update(&Entry.Key, sizeof(Entry.Key));
		Hash.Update(&Entry.Value, sizeof(Entry.Value));
	}

	return FIoHash(Hash.Finalize());
}

static FSharedBuffer WritePayloadBlob(const FArcEcotopeFoliageFieldPayload& Payload)
{
	TArray<uint8> Blob;
	FMemoryWriter Writer(Blob);

	int32 BlobRev = ArcEcotopeFoliageFieldLimits::FieldPayloadRev;
	FIntPoint Dimensions = Payload.Dimensions;
	double TexelSizeCm = Payload.TexelSizeCm;
	FVector2D OriginLocal = Payload.OriginLocal;
	uint64 SourceSiteSnapshotHash = Payload.SourceSiteSnapshotHash;
	uint64 SourceStateChecksum = Payload.SourceStateChecksum;
	int32 TexelBytes = Payload.Texels.Num();

	Writer << BlobRev;
	Writer << Dimensions;
	Writer << TexelSizeCm;
	Writer << OriginLocal;
	Writer << SourceSiteSnapshotHash;
	Writer << SourceStateChecksum;
	Writer << TexelBytes;
	Writer.Serialize(const_cast<uint8*>(Payload.Texels.GetData()), TexelBytes);

	return MakeSharedBufferFromArray(MoveTemp(Blob));
}

static bool TryReadPayloadBlob(const FSharedBuffer& Blob, FArcEcotopeFoliageFieldPayload& OutPayload)
{
	OutPayload.Reset();
	if (Blob.IsNull() || Blob.GetSize() > MAX_int32)
	{
		return false;
	}

	FMemoryReaderView Reader(
		MakeArrayView(static_cast<const uint8*>(Blob.GetData()), static_cast<int32>(Blob.GetSize())));

	int32 BlobRev = 0;
	Reader << BlobRev;
	if (BlobRev != ArcEcotopeFoliageFieldLimits::FieldPayloadRev)
	{
		return false;
	}

	int32 TexelBytes = 0;
	Reader << OutPayload.Dimensions;
	Reader << OutPayload.TexelSizeCm;
	Reader << OutPayload.OriginLocal;
	Reader << OutPayload.SourceSiteSnapshotHash;
	Reader << OutPayload.SourceStateChecksum;
	Reader << TexelBytes;
	if (Reader.IsError() || TexelBytes <= 0 || Reader.Tell() + TexelBytes > Reader.TotalSize())
	{
		return false;
	}

	OutPayload.PayloadRev = BlobRev;
	OutPayload.Texels.SetNumUninitialized(TexelBytes);
	Reader.Serialize(OutPayload.Texels.GetData(), TexelBytes);
	return !Reader.IsError();
}

bool DeriveFieldPayload(
	AArcEcotopeVolume& Volume,
	FArcEcotopeFoliageFieldPayload& OutPayload,
	FText& OutError)
{
	check(IsInGameThread());
	OutPayload.Reset();
	OutError = FText::GetEmpty();

	const UArcForestSimulationState* State = Volume.SimulationState;
	if (State == nullptr || State->Status != EArcForestSimulationStateStatus::Valid)
	{
		OutError = FText::FromString(
			TEXT("ArcEcotope foliage fields require a valid committed simulation snapshot."));
		return false;
	}

	const FIoHash CacheKey = ComputeFieldCacheKey(Volume, *State);
	const FString DebugName = Volume.GetPathName();

	if (TryReadPayloadBlob(ArcEcotopeFoliageFieldCache::TryGet(CacheKey, *DebugName), OutPayload)
		&& OutPayload.IsValid())
	{
		UE_LOGF(LogArcEcotope, Log,"[FoliageFields] volume='%ls' field taken from the cache: %dx%d texels at %.0f cm.",
			*Volume.GetName(), OutPayload.Dimensions.X, OutPayload.Dimensions.Y, OutPayload.TexelSizeCm);
		return true;
	}

	OutPayload.Reset();
	if (!RasterizeFieldPayload(Volume, OutPayload, OutError))
	{
		OutPayload.Reset();
		return false;
	}

	ArcEcotopeFoliageFieldCache::Put(CacheKey, WritePayloadBlob(OutPayload), *DebugName);
	return true;
}
#endif // WITH_EDITOR

FArcEcotopeFoliageFieldRef MakeFieldFromPayload(
	const FArcEcotopeFoliageFieldPayload& Payload,
	const FTransform& VolumeTransform,
	const uint32 Revision)
{
	check(IsInGameThread());

	if (!Payload.IsValid())
	{
		return FArcEcotopeFoliageFieldRef();
	}

	FArcEcotopeFoliageFieldRef Field = MakeShared<FArcEcotopeFoliageField, ESPMode::ThreadSafe>();
	Field->Dimensions = Payload.Dimensions;
	ArcEcotopeFoliageFieldsPrivate::SplitVolumeTransformLWC(
		VolumeTransform, Field->LWCTile, Field->WorldToVolumeLocal);
	Field->OriginLocal = FVector2f(
		static_cast<float>(Payload.OriginLocal.X), static_cast<float>(Payload.OriginLocal.Y));
	Field->InvSizeLocal = FVector2f(
		static_cast<float>(1.0 / (Payload.Dimensions.X * Payload.TexelSizeCm)),
		static_cast<float>(1.0 / (Payload.Dimensions.Y * Payload.TexelSizeCm)));
	Field->Revision = Revision;

	const FIntPoint Dimensions = Payload.Dimensions;
	ENQUEUE_RENDER_COMMAND(ArcEcotopeFoliageFieldUpload)(
		[Field, UploadTexels = Payload.Texels, Dimensions](FRHICommandListImmediate& RHICmdList)
		{
			const FRHITextureCreateDesc Desc =
				FRHITextureCreateDesc::Create2D(TEXT("ArcEcotopeFoliageField"), Dimensions, PF_FloatRGBA)
					.SetFlags(ETextureCreateFlags::ShaderResource)
					.SetInitialState(ERHIAccess::SRVMask);

			FTextureRHIRef Texture = RHICmdList.CreateTexture(Desc);

			const FUpdateTextureRegion2D Region(0, 0, 0, 0, Dimensions.X, Dimensions.Y);
			RHICmdList.UpdateTexture2D(Texture, /*MipIndex=*/0, Region,
				static_cast<uint32>(Dimensions.X * sizeof(FFloat16Color)),
				UploadTexels.GetData());

			Field->TextureRHI = Texture;
		});

	return Field;
}

FArcEcotopeFoliageFieldRef MakeFieldWithTransform(
	const FArcEcotopeFoliageFieldRef& Source,
	const FTransform& VolumeTransform,
	const uint32 Revision)
{
	check(IsInGameThread());

	if (!Source.IsValid())
	{
		return FArcEcotopeFoliageFieldRef();
	}

	FArcEcotopeFoliageFieldRef Field = MakeShared<FArcEcotopeFoliageField, ESPMode::ThreadSafe>();
	Field->Dimensions = Source->Dimensions;
	Field->OriginLocal = Source->OriginLocal;
	Field->InvSizeLocal = Source->InvSizeLocal;
	Field->Revision = Revision;
	ArcEcotopeFoliageFieldsPrivate::SplitVolumeTransformLWC(
		VolumeTransform, Field->LWCTile, Field->WorldToVolumeLocal);

	ENQUEUE_RENDER_COMMAND(ArcEcotopeFoliageFieldAdopt)(
		[Field, Source](FRHICommandListImmediate&)
		{
			Field->TextureRHI = Source->TextureRHI;
		});

	return Field;
}

void BindField(FRDGBuilder& GraphBuilder, const FArcEcotopeFoliageFieldRef& Field,
	FArcEcotopeFoliageFieldParameters& OutParameters)
{
	FRDGTextureSRVRef FieldSRV = nullptr;

	OutParameters.ArcEcotopeFieldValid = 0;
	OutParameters.ArcEcotopeFieldWorldToLocal = FMatrix44f::Identity;
	OutParameters.ArcEcotopeFieldLWCTile = FVector3f::ZeroVector;
	OutParameters.ArcEcotopeFieldOriginLocal = FVector2f::ZeroVector;
	OutParameters.ArcEcotopeFieldInvSizeLocal = FVector2f::ZeroVector;

	// The upload command was enqueued before the dispatch that reaches here, and render commands run in
	// order, so a field built this tick already carries its texture.
	if (Field.IsValid() && Field->TextureRHI.IsValid())
	{
		FRDGTextureRef FieldTexture =
			RegisterExternalTexture(GraphBuilder, Field->TextureRHI, TEXT("ArcEcotopeFoliageField"));
		FieldSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(FieldTexture));

		OutParameters.ArcEcotopeFieldValid = 1;
		OutParameters.ArcEcotopeFieldWorldToLocal = Field->WorldToVolumeLocal;
		OutParameters.ArcEcotopeFieldLWCTile = Field->LWCTile;
		OutParameters.ArcEcotopeFieldOriginLocal = Field->OriginLocal;
		OutParameters.ArcEcotopeFieldInvSizeLocal = Field->InvSizeLocal;
	}

	// A bound-but-dead resource, never a null binding.
	if (FieldSRV == nullptr)
	{
		FieldSRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc(GSystemTextures.GetDefaultTexture2D(
			GraphBuilder, PF_FloatRGBA, FVector4f(0.0f, 1.0f, 1.0f, 0.0f))));
	}

	OutParameters.ArcEcotopeFieldTexture = FieldSRV;
	OutParameters.ArcEcotopeFieldSampler =
		TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
}

void ReleaseField(FArcEcotopeFoliageFieldRef&& Field)
{
	check(IsInGameThread());

	if (!Field.IsValid())
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(ReleaseArcEcotopeFoliageField)(
		[Released = MoveTemp(Field)](FRHICommandListImmediate&)
		{
		});
}

} // namespace ArcEcotopeFoliageFields
