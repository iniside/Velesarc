// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/ArcEcotopeWeightRasterBuilder.h"

#include "ArcEcotopeModule.h"
#include "ArcTerrainChannelSet.h"
#include "Ecotope/ArcEcotopeModel.h"
#include "Foliage/ArcEcotopeFoliageFieldPayload.h"
#include "Foliage/ArcEcotopeRecordRaster.h"
#include "Internationalization/Text.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcEcotopeVolume.h"
#include "Species/ArcEcotopeSpeciesModel.h"
#include "Species/ArcForestStageBand.h"
#include "Terrain/ArcEcotopeTerrainWeightStamp.h"

namespace ArcEcotopeWeightRasterBuilderPrivate
{
	using UE::ArcEcotope::FFieldRaster;

	bool FailWith(
		FString&& Message,
		TMap<FName, FArcTerrainBakedField>& OutLayers,
		TMap<FName, FName>& OutChannelMap,
		TArray<FName>& OutSuppressedChannels,
		FArcTerrainBakedField& OutUnionMask,
		FText& OutError)
	{
		OutLayers.Reset();
		OutChannelMap.Reset();
		OutSuppressedChannels.Reset();
		OutUnionMask.Reset();
		OutError = FText::FromString(MoveTemp(Message));
		return false;
	}

	uint8 SelectRasterLevel(const FArcEcotopeSiteMetadata& Site)
	{
		for (uint8 Level = 0; Level < FArcEcotopeSiteMetadata::MaxGridLevel; ++Level)
		{
			const FIntPoint LevelDimensions = Site.GetLevelDimensions(Level);
			if (LevelDimensions.X <= ArcEcotopeFoliageFieldLimits::MaxFieldTexels
				&& LevelDimensions.Y <= ArcEcotopeFoliageFieldLimits::MaxFieldTexels)
			{
				return Level;
			}
		}

		return FArcEcotopeSiteMetadata::MaxGridLevel;
	}

	FFieldRaster MakeSiteRaster(
		const FArcEcotopeSiteMetadata& Site,
		const FIntPoint& Dimensions,
		const double CellSizeCm,
		const int32 TexelCount)
	{
		FFieldRaster Raster;
		Raster.Dimensions = Dimensions;
		Raster.TexelSizeCm = CellSizeCm;
		Raster.OriginLocal = FVector2D(Site.LocalBounds.Min.X, Site.LocalBounds.Min.Y);
		Raster.Occupancy.SetNumZeroed(TexelCount);
		return Raster;
	}

	void MergeLayer(
		const FName ChannelName,
		const FIntPoint& Dimensions,
		TArray<float>&& Values,
		TMap<FName, FArcTerrainBakedField>& OutLayers,
		TMap<FName, FName>& OutChannelMap)
	{
		FArcTerrainBakedField* const Existing = OutLayers.Find(ChannelName);
		if (Existing == nullptr)
		{
			FArcTerrainBakedField& Field = OutLayers.Add(ChannelName);
			Field.Values = MoveTemp(Values);
			Field.Dims = Dimensions;
			OutChannelMap.Add(ChannelName, ChannelName);
			return;
		}

		const int32 MergeCount = FMath::Min(Existing->Values.Num(), Values.Num());
		for (int32 Index = 0; Index < MergeCount; ++Index)
		{
			Existing->Values[Index] = FMath::Max(Existing->Values[Index], Values[Index]);
		}
	}

	TArray<float> ThresholdRaster(const FFieldRaster& Raster, const int32 TexelCount)
	{
		TArray<float> Values;
		Values.SetNumUninitialized(TexelCount);
		for (int32 Index = 0; Index < TexelCount; ++Index)
		{
			Values[Index] = Raster.Occupancy[Index] > 0.0f ? 1.0f : 0.0f;
		}

		return Values;
	}

	constexpr double UnreachableSqTexels = 1.0e12;

	void SquaredDistanceTransform1D(
		double* const Values,
		const int32 Count,
		TArray<double>& Source,
		TArray<int32>& Hulls,
		TArray<double>& Breaks)
	{
		Source.SetNumUninitialized(Count);
		FMemory::Memcpy(Source.GetData(), Values, static_cast<uint64>(Count) * sizeof(double));
		Hulls.SetNumUninitialized(Count);
		Breaks.SetNumUninitialized(Count + 1);

		int32 Top = 0;
		Hulls[0] = 0;
		Breaks[0] = -UnreachableSqTexels;
		Breaks[1] = UnreachableSqTexels;
		for (int32 Q = 1; Q < Count; ++Q)
		{
			double Intersection = 0.0;
			while (true)
			{
				const int32 P = Hulls[Top];
				Intersection =
					((Source[Q] + static_cast<double>(Q) * static_cast<double>(Q))
						- (Source[P] + static_cast<double>(P) * static_cast<double>(P)))
					/ (2.0 * static_cast<double>(Q - P));
				if (Intersection > Breaks[Top])
				{
					break;
				}
				--Top;
			}

			++Top;
			Hulls[Top] = Q;
			Breaks[Top] = Intersection;
			Breaks[Top + 1] = UnreachableSqTexels;
		}

		int32 Cursor = 0;
		for (int32 Q = 0; Q < Count; ++Q)
		{
			while (Breaks[Cursor + 1] < static_cast<double>(Q))
			{
				++Cursor;
			}
			const double Offset = static_cast<double>(Q - Hulls[Cursor]);
			Values[Q] = Offset * Offset + Source[Hulls[Cursor]];
		}
	}

	void BuildSquaredDistanceTexels(
		const TArray<float>& UnionValues,
		const FIntPoint& Dimensions,
		const bool bSeedOnPainted,
		TArray<double>& OutSquaredTexels)
	{
		const int32 Width = Dimensions.X;
		const int32 Height = Dimensions.Y;
		const int32 TexelCount = Width * Height;

		OutSquaredTexels.SetNumUninitialized(TexelCount);
		for (int32 Index = 0; Index < TexelCount; ++Index)
		{
			OutSquaredTexels[Index] =
				(UnionValues[Index] > 0.0f) == bSeedOnPainted ? 0.0 : UnreachableSqTexels;
		}

		TArray<double> Column;
		TArray<double> Source;
		TArray<int32> Hulls;
		TArray<double> Breaks;
		Column.SetNumUninitialized(Height);

		for (int32 X = 0; X < Width; ++X)
		{
			for (int32 Y = 0; Y < Height; ++Y)
			{
				Column[Y] = OutSquaredTexels[Y * Width + X];
			}
			SquaredDistanceTransform1D(Column.GetData(), Height, Source, Hulls, Breaks);
			for (int32 Y = 0; Y < Height; ++Y)
			{
				OutSquaredTexels[Y * Width + X] = Column[Y];
			}
		}

		for (int32 Y = 0; Y < Height; ++Y)
		{
			SquaredDistanceTransform1D(OutSquaredTexels.GetData() + Y * Width, Width, Source, Hulls, Breaks);
		}
	}

	void ApplyMaskFalloff(
		TArray<float>& InOutUnionValues,
		const FIntPoint& Dimensions,
		const double CellSizeCm,
		const float ExpandCm,
		const float FalloffCm,
		const float FalloffExponent)
	{
		const bool bHardEdge = !(FalloffCm > 0.0f);
		if (bHardEdge && ExpandCm == 0.0f)
		{
			return;
		}

		const int32 TexelCount = InOutUnionValues.Num();

		// Negative inside the painted set, so a negative expand erodes it.
		TArray<float> SignedDistanceCm;
		SignedDistanceCm.SetNumUninitialized(TexelCount);

		TArray<double> SquaredTexels;
		BuildSquaredDistanceTexels(InOutUnionValues, Dimensions, true, SquaredTexels);
		for (int32 Index = 0; Index < TexelCount; ++Index)
		{
			SignedDistanceCm[Index] = InOutUnionValues[Index] > 0.0f
				? 0.0f
				: static_cast<float>(FMath::Sqrt(SquaredTexels[Index]) * CellSizeCm);
		}

		BuildSquaredDistanceTexels(InOutUnionValues, Dimensions, false, SquaredTexels);
		for (int32 Index = 0; Index < TexelCount; ++Index)
		{
			if (InOutUnionValues[Index] > 0.0f)
			{
				SignedDistanceCm[Index] =
					-static_cast<float>(FMath::Sqrt(SquaredTexels[Index]) * CellSizeCm);
			}
		}
		SquaredTexels.Empty();

		const float Exponent = FMath::Clamp(FalloffExponent, 0.1f, 8.0f);
		const bool bLinearRamp = FMath::IsNearlyEqual(Exponent, 1.0f);
		const float InvFalloffCm = bHardEdge ? 0.0f : 1.0f / FalloffCm;
		for (int32 Index = 0; Index < TexelCount; ++Index)
		{
			const float DistanceCm = SignedDistanceCm[Index];
			if (bHardEdge)
			{
				InOutUnionValues[Index] = DistanceCm <= ExpandCm ? 1.0f : 0.0f;
				continue;
			}

			const float Ramp = FMath::Clamp(1.0f - (DistanceCm - ExpandCm) * InvFalloffCm, 0.0f, 1.0f);
			InOutUnionValues[Index] = bLinearRamp ? Ramp : FMath::Pow(Ramp, Exponent);
		}
	}

	bool ValidateChannel(
		const AArcEcotopeVolume& Volume,
		const UArcTerrainChannelSet* ChannelSet,
		const FName Channel,
		const FString& Role,
		TMap<FName, FArcTerrainBakedField>& OutLayers,
		TMap<FName, FName>& OutChannelMap,
		TArray<FName>& OutSuppressedChannels,
		FArcTerrainBakedField& OutUnionMask,
		FText& OutError)
	{
		if (Channel.IsNone())
		{
			return true;
		}

		if (ChannelSet == nullptr)
		{
			return FailWith(FString::Printf(
				TEXT("ArcEcotope volume '%s' paints %s into terrain weight channel '%s', but the terrain has no channel set."),
				*Volume.GetName(), *Role, *Channel.ToString()),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
		}

		const int32 ChannelIndex = ChannelSet->FindChannelIndex(Channel);
		if (ChannelIndex == INDEX_NONE)
		{
			return FailWith(FString::Printf(
				TEXT("ArcEcotope volume '%s' paints %s into terrain weight channel '%s', which channel set '%s' does not carry; the composite would drop it without a word."),
				*Volume.GetName(), *Role, *Channel.ToString(), *ChannelSet->GetName()),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
		}

		if (ChannelIndex == ArcTerrain::ResolveBaseChannelIndex(ChannelSet->Channels.Num()))
		{
			return FailWith(FString::Printf(
				TEXT("ArcEcotope volume '%s' paints %s into terrain weight channel '%s', which is the base channel; normalization overwrites that lane with the remainder."),
				*Volume.GetName(), *Role, *Channel.ToString()),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
		}

		return true;
	}
}

namespace ArcEcotopeWeightRaster
{
	bool BuildWeightRasters(
		const AArcEcotopeVolume& Volume,
		const FArcEcotopeSiteMetadata& Site,
		const UArcTerrainChannelSet* ChannelSet,
		const FName CanopyChannel,
		const FName LitterChannel,
		const float MaskExpandCm,
		const float MaskFalloffCm,
		const float MaskFalloffExponent,
		const TConstArrayView<FArcEcotopeInstanceFootprint> Footprints,
		TMap<FName, FArcTerrainBakedField>& OutLayers,
		TMap<FName, FName>& OutChannelMap,
		TArray<FName>& OutSuppressedChannels,
		FArcTerrainBakedField& OutUnionMask,
		FText& OutError)
	{
		using namespace ArcEcotopeWeightRasterBuilderPrivate;

		OutLayers.Reset();
		OutChannelMap.Reset();
		OutSuppressedChannels.Reset();
		OutUnionMask.Reset();
		OutError = FText::GetEmpty();

		const uint8 RasterLevel = SelectRasterLevel(Site);
		const FIntPoint Dimensions = Site.GetLevelDimensions(RasterLevel);
		if (Dimensions.X < 2 || Dimensions.Y < 2)
		{
			return FailWith(FString::Printf(
				TEXT("ArcEcotope volume '%s' rasterizes to %d x %d texels at grid level %d; a terrain weight layer needs at least 2 x 2."),
				*Volume.GetName(), Dimensions.X, Dimensions.Y, static_cast<int32>(RasterLevel)),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
		}

		const double CellSizeCm = Site.GetLevelCellSizeCm(RasterLevel);
		const int32 TexelCount = Site.GetLevelCellCount(RasterLevel);
		if (TexelCount <= 0 || !(CellSizeCm > 0.0) || !FMath::IsFinite(CellSizeCm) || !Site.LocalBounds.IsValid)
		{
			return FailWith(FString::Printf(
				TEXT("ArcEcotope volume '%s' has no usable site frame at grid level %d."),
				*Volume.GetName(), static_cast<int32>(RasterLevel)),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
		}

		const UArcEcotopeModel& Ecotope = Volume.GetEcotopeOrDefault();

		if (!ValidateChannel(
				Volume, ChannelSet, CanopyChannel, TEXT("canopy closure"),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError)
			|| !ValidateChannel(
				Volume, ChannelSet, LitterChannel, TEXT("litter"),
				OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError))
		{
			return false;
		}
		for (const FArcEcotopeSpeciesSetup& Setup : Ecotope.SpeciesSetups)
		{
			if (!Setup.bEnabled || Setup.Species == nullptr)
			{
				continue;
			}
			if (!ValidateChannel(
					Volume, ChannelSet, Setup.Species->TerrainCoverChannel,
					FString::Printf(TEXT("species '%s' cover"), *Setup.Species->GetPathName()),
					OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError))
			{
				return false;
			}
		}

		if (!CanopyChannel.IsNone())
		{
			FArcEcotopeInstanceRasterRequest Request;
			Request.KindFilter = static_cast<uint8>(EArcForestVisualKind::Alive);

			FFieldRaster Raster = MakeSiteRaster(Site, Dimensions, CellSizeCm, TexelCount);
			UE::ArcEcotope::RasterizeInstanceOccupancy(Footprints, Request, Raster);

			MergeLayer(
				CanopyChannel, Dimensions, ThresholdRaster(Raster, TexelCount), OutLayers, OutChannelMap);
		}

		if (!LitterChannel.IsNone())
		{
			static const EArcForestVisualKind LitterKinds[] = {
				EArcForestVisualKind::Log,
				EArcForestVisualKind::Uprooted,
				EArcForestVisualKind::Branch,
			};

			FFieldRaster Raster = MakeSiteRaster(Site, Dimensions, CellSizeCm, TexelCount);
			for (const EArcForestVisualKind LitterKind : LitterKinds)
			{
				FArcEcotopeInstanceRasterRequest Request;
				Request.KindFilter = static_cast<uint8>(LitterKind);
				UE::ArcEcotope::RasterizeInstanceOccupancy(Footprints, Request, Raster);
			}

			MergeLayer(
				LitterChannel, Dimensions, ThresholdRaster(Raster, TexelCount), OutLayers, OutChannelMap);
		}

		TSet<FGuid> SpeciesWithFootprints;
		SpeciesWithFootprints.Reserve(Footprints.Num());
		for (const FArcEcotopeInstanceFootprint& Footprint : Footprints)
		{
			SpeciesWithFootprints.Add(Footprint.SpeciesId);
		}

		TMap<FGuid, FName> AssignedSpeciesChannels;
		for (const FArcEcotopeSpeciesSetup& Setup : Ecotope.SpeciesSetups)
		{
			if (!Setup.bEnabled || Setup.Species == nullptr || Setup.Species->TerrainCoverChannel.IsNone())
			{
				continue;
			}

			if (!Setup.Species->SpeciesId.IsValid())
			{
				return FailWith(FString::Printf(
					TEXT("ArcEcotope volume '%s' paints species '%s' into terrain weight channel '%s', but that species carries no valid SpeciesId."),
					*Volume.GetName(), *Setup.Species->GetPathName(),
					*Setup.Species->TerrainCoverChannel.ToString()),
					OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
			}

			if (const FName* AssignedChannel = AssignedSpeciesChannels.Find(Setup.Species->SpeciesId))
			{
				if (*AssignedChannel == Setup.Species->TerrainCoverChannel)
				{
					continue;
				}

				return FailWith(FString::Printf(
					TEXT("ArcEcotope volume '%s' paints species '%s' into terrain weight channel '%s', but another setup already paints the same SpeciesId into '%s'."),
					*Volume.GetName(), *Setup.Species->GetPathName(),
					*Setup.Species->TerrainCoverChannel.ToString(), *AssignedChannel->ToString()),
					OutLayers, OutChannelMap, OutSuppressedChannels, OutUnionMask, OutError);
			}
			AssignedSpeciesChannels.Add(Setup.Species->SpeciesId, Setup.Species->TerrainCoverChannel);

			// An all-zero cover raster counts as painted and would erase the channel under the mask.
			if (!SpeciesWithFootprints.Contains(Setup.Species->SpeciesId))
			{
				continue;
			}

			FArcEcotopeInstanceRasterRequest Request;
			Request.SpeciesFilter = Setup.Species->SpeciesId;
			Request.KindFilter = static_cast<uint8>(EArcForestVisualKind::Alive);

			FFieldRaster Raster = MakeSiteRaster(Site, Dimensions, CellSizeCm, TexelCount);
			UE::ArcEcotope::RasterizeInstanceOccupancy(Footprints, Request, Raster);

			MergeLayer(
				Setup.Species->TerrainCoverChannel, Dimensions, ThresholdRaster(Raster, TexelCount),
				OutLayers, OutChannelMap);
		}

		if (OutLayers.IsEmpty())
		{
			return true;
		}

		// The composite normalizes weights across planes, so writing 1.0 only buys a share of the texel.
		// The union of everything this bake paints becomes the stamp mask, which zeroes the destination
		// under it before the value blends in — that is what makes the paint an override.
		TArray<float> UnionValues;
		UnionValues.Init(0.0f, TexelCount);
		for (const TPair<FName, FArcTerrainBakedField>& LayerPair : OutLayers)
		{
			const TArray<float>& LayerValues = LayerPair.Value.Values;
			for (int32 Index = 0; Index < TexelCount; ++Index)
			{
				UnionValues[Index] = LayerValues[Index] > 0.0f ? 1.0f : UnionValues[Index];
			}
		}

		ApplyMaskFalloff(
			UnionValues, Dimensions, CellSizeCm, MaskExpandCm, MaskFalloffCm, MaskFalloffExponent);

		if (ChannelSet != nullptr)
		{
			OutSuppressedChannels.Reserve(ChannelSet->Channels.Num());
			for (int32 ChannelIndex = 0; ChannelIndex < ChannelSet->Channels.Num(); ++ChannelIndex)
			{
				// Skipped: the composite overwrites the base lane with 1 - sum(others), so a zero written
				// there says nothing.
				if (ChannelIndex == ArcTerrain::GBaseChannelIndex)
				{
					continue;
				}

				const FName ChannelName = ChannelSet->Channels[ChannelIndex];
				if (ChannelName.IsNone() || OutLayers.Contains(ChannelName))
				{
					continue;
				}

				OutSuppressedChannels.AddUnique(ChannelName);
			}
			OutSuppressedChannels.Sort(FNameLexicalLess());
		}

		OutUnionMask.Values = MoveTemp(UnionValues);
		OutUnionMask.Dims = Dimensions;

		for (TPair<FName, FArcTerrainBakedField>& LayerPair : OutLayers)
		{
			LayerPair.Value.ContentId = ArcEcotopeTerrainWeight::ComputeFieldContentId(
				LayerPair.Value.Values, LayerPair.Value.Dims);
		}
		OutUnionMask.ContentId =
			ArcEcotopeTerrainWeight::ComputeFieldContentId(OutUnionMask.Values, OutUnionMask.Dims);

		for (const TPair<FName, FArcTerrainBakedField>& LayerPair : OutLayers)
		{
			const FName* MappedChannel = OutChannelMap.Find(LayerPair.Key);
			int32 LitTexels = 0;
			for (const float Value : LayerPair.Value.Values)
			{
				LitTexels += Value > 0.0f ? 1 : 0;
			}
			const int32 ChannelIndex = MappedChannel != nullptr && ChannelSet != nullptr
				? ChannelSet->FindChannelIndex(*MappedChannel)
				: INDEX_NONE;
			UE_LOGF(LogArcEcotope, Log,"ArcEcotope weight layer '%ls' -> channel '%ls' (index %d): %d x %d texels at %.0f cm, %d lit, from %d footprints.",
				*LayerPair.Key.ToString(),
				MappedChannel != nullptr ? *MappedChannel->ToString() : TEXT("<unmapped>"),
				ChannelIndex,
				LayerPair.Value.Dims.X, LayerPair.Value.Dims.Y, CellSizeCm, LitTexels, Footprints.Num());
			UE_CLOGF(ChannelIndex == ArcTerrain::GBaseChannelIndex, LogArcEcotope, Warning,"ArcEcotope weight layer '%ls' paints the terrain's BASE channel '%ls' (index %d), which the composite overwrites with the remainder of every other channel. Pick a different channel.",
				*LayerPair.Key.ToString(), *MappedChannel->ToString(), ChannelIndex);
		}

		int32 MaskedTexels = 0;
		for (const float Value : OutUnionMask.Values)
		{
			MaskedTexels += Value > 0.0f ? 1 : 0;
		}
		UE_LOGF(LogArcEcotope, Log,"ArcEcotope union mask: %d x %d texels at %.0f cm, %d masked (expand %.0f cm, falloff %.0f cm, exponent %.2f), %d suppressed channels.",
			OutUnionMask.Dims.X, OutUnionMask.Dims.Y, CellSizeCm, MaskedTexels,
			MaskExpandCm, MaskFalloffCm, MaskFalloffExponent, OutSuppressedChannels.Num());

		return true;
	}
}
