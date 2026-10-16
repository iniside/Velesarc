// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileFormat.h"

namespace ArcTerrainTileFormatPrivate_7b41
{
	void WriteU32(TArray<uint8>& Out, uint32 Value)
	{
		Out.Add(static_cast<uint8>(Value & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 16) & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 24) & 0xFFu));
	}

	void WriteU16(TArray<uint8>& Out, uint16 Value)
	{
		Out.Add(static_cast<uint8>(Value & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xFFu));
	}

	uint16 ReadU16(const uint8* Bytes)
	{
		return static_cast<uint16>(static_cast<uint16>(Bytes[0]) | (static_cast<uint16>(Bytes[1]) << 8));
	}

	uint32 ReadU32(const uint8* Bytes)
	{
		return static_cast<uint32>(Bytes[0]) | (static_cast<uint32>(Bytes[1]) << 8)
			| (static_cast<uint32>(Bytes[2]) << 16) | (static_cast<uint32>(Bytes[3]) << 24);
	}

	void WriteFloat(TArray<uint8>& Out, float Value)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(uint32));
		WriteU32(Out, Bits);
	}

	float ReadFloat(const uint8* Bytes)
	{
		const uint32 Bits = ReadU32(Bytes);
		float Value = 0.0f;
		FMemory::Memcpy(&Value, &Bits, sizeof(float));
		return Value;
	}

	/** Bytes appended after the plane ids: a float32 min and max per BODY plane, which on the height rail
	 *  is one window under an id array the rail does not have. */
	int64 QuantWindowBytes(EArcTerrainRail Rail, int32 NumIds)
	{
		if (Rail == EArcTerrainRail::Data)
		{
			return static_cast<int64>(NumIds) * 8;
		}
		return Rail == EArcTerrainRail::Height ? ArcTerrainTileFormat::HeightPlaneCount * 8 : 0;
	}

	/** The weight rail's ids are 32-bit name hashes; the data rail's are single-byte mask enums. */
	int64 PlaneIdBytes(EArcTerrainRail Rail, int32 NumPlanes)
	{
		return Rail == EArcTerrainRail::Weight
			? static_cast<int64>(NumPlanes) * 4 : static_cast<int64>(NumPlanes);
	}

	template <typename IdType>
	bool AreIdsUnique(TConstArrayView<IdType> Ids)
	{
		for (int32 Index = 1; Index < Ids.Num(); ++Index)
		{
			for (int32 Other = 0; Other < Index; ++Other)
			{
				if (Ids[Index] == Ids[Other])
				{
					return false;
				}
			}
		}
		return true;
	}

	bool IsKnownRail(uint8 RailValue)
	{
		return RailValue <= static_cast<uint8>(EArcTerrainRail::Data);
	}

	int32 RoundedTexelSizeCm(int32 TileExtentM, int32 TileQuads)
	{
		if (TileQuads <= 0)
		{
			return 0;
		}
		const double TexelCm = static_cast<double>(TileExtentM) * 100.0 / static_cast<double>(TileQuads);
		return FMath::Max(1, FMath::RoundToInt(TexelCm));
	}
}

bool FArcTerrainTileHeader::IsValid() const
{
	return Version == ArcTerrainTileFormat::TileVersion && Resolution > 0
		&& CompressionMethod <= ArcTerrainTileFormat::CompressionOodleKraken;
}

namespace ArcTerrainTileFormat
{
	int32 BytesPerTexel(EArcTerrainRail Rail)
	{
		return Rail == EArcTerrainRail::Height ? 2 : 1;
	}

	int32 NumPlaneIds(const FArcTerrainTileHeader& Header)
	{
		switch (Header.Rail)
		{
		case EArcTerrainRail::Weight:
			return Header.PlaneIds.Num();
		case EArcTerrainRail::Data:
			return Header.DataMapIds.Num();
		default:
			return 0;
		}
	}

	int32 NumBodyPlanes(const FArcTerrainTileHeader& Header)
	{
		return Header.Rail == EArcTerrainRail::Height ? HeightPlaneCount : NumPlaneIds(Header);
	}

	int64 PlaneBytes(const FArcTerrainTileHeader& Header)
	{
		if (Header.Resolution <= 0)
		{
			return 0;
		}
		const int64 Resolution = static_cast<int64>(Header.Resolution);
		return Resolution * Resolution * static_cast<int64>(BytesPerTexel(Header.Rail));
	}

	int64 HeaderBytes(const FArcTerrainTileHeader& Header)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		const int32 NumIds = NumPlaneIds(Header);
		return static_cast<int64>(FixedHeaderBytes) + PlaneIdBytes(Header.Rail, NumIds)
			+ QuantWindowBytes(Header.Rail, NumIds);
	}

	int64 PlaneBodyOffset(const FArcTerrainTileHeader& Header, int32 PlaneIndex)
	{
		if (PlaneIndex < 0 || PlaneIndex >= NumBodyPlanes(Header))
		{
			return INDEX_NONE;
		}
		return static_cast<int64>(PlaneIndex) * PlaneBytes(Header);
	}

	int64 ExpectedFileBytes(const FArcTerrainTileHeader& Header)
	{
		return HeaderBytes(Header) + static_cast<int64>(Header.CompressedBodyBytes);
	}

	void SerializeHeader(const FArcTerrainTileHeader& Header, TArray<uint8>& OutBytes)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		const int32 NumIds = NumPlaneIds(Header);

		// NumPlanes is one byte on disk: a wider plane set would wrap to a count the reader rejects,
		// producing a file that writes cleanly and can never be read back.
		checkf(NumIds <= MaxPlaneCount,
			TEXT("Tile header carries %d planes, over the %d the format can encode"),
			NumIds, MaxPlaneCount);

		OutBytes.Reserve(OutBytes.Num() + static_cast<int32>(HeaderBytes(Header)));
		WriteU32(OutBytes, TileMagic);
		WriteU32(OutBytes, Header.Version);
		OutBytes.Add(static_cast<uint8>(Header.Rail));
		OutBytes.Add(static_cast<uint8>(NumIds));
		OutBytes.Add(Header.CompressionMethod);
		OutBytes.Add(0);
		WriteU32(OutBytes, Header.TileExtentM);
		WriteU32(OutBytes, Header.TexelSizeCm);
		WriteU32(OutBytes, static_cast<uint32>(Header.Resolution));
		WriteU32(OutBytes, Header.CompressedBodyBytes);
		WriteU32(OutBytes, Header.UncompressedBodyBytes);

		if (Header.Rail == EArcTerrainRail::Weight)
		{
			for (uint32 PlaneId : Header.PlaneIds)
			{
				WriteU32(OutBytes, PlaneId);
			}
		}
		else
		{
			OutBytes.Append(Header.DataMapIds);
		}

		if (Header.Rail != EArcTerrainRail::Weight)
		{
			// Body planes, not ids: the height rail has one window and no ids at all.
			const int32 NumWindows = NumBodyPlanes(Header);
			checkf(Header.PlaneQuantMin.Num() == NumWindows && Header.PlaneQuantMax.Num() == NumWindows,
				TEXT("Tile header carries %d body planes but %d/%d quantization windows"),
				NumWindows, Header.PlaneQuantMin.Num(), Header.PlaneQuantMax.Num());
			for (int32 PlaneIndex = 0; PlaneIndex < NumWindows; ++PlaneIndex)
			{
				WriteFloat(OutBytes, Header.PlaneQuantMin[PlaneIndex]);
				WriteFloat(OutBytes, Header.PlaneQuantMax[PlaneIndex]);
			}
		}
	}

	bool PeekTileVersion(TConstArrayView<uint8> LeadingBytes, uint32& OutVersion)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		if (LeadingBytes.Num() < VersionPeekBytes)
		{
			return false;
		}

		const uint8* Bytes = LeadingBytes.GetData();
		if (ReadU32(Bytes) != TileMagic)
		{
			return false;
		}

		OutVersion = ReadU32(Bytes + 4);
		return true;
	}

	bool ParseHeader(TConstArrayView<uint8> FileBytes, FArcTerrainTileHeader& OutHeader)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		if (FileBytes.Num() < FixedHeaderBytes)
		{
			return false;
		}

		const uint8* Bytes = FileBytes.GetData();
		if (ReadU32(Bytes) != TileMagic || ReadU32(Bytes + 4) != TileVersion)
		{
			return false;
		}

		const uint8 RailValue = Bytes[8];
		const int32 NumPlanes = static_cast<int32>(Bytes[9]);
		const uint8 CompressionMethod = Bytes[10];
		if (!IsKnownRail(RailValue) || CompressionMethod > CompressionOodleKraken || Bytes[11] != 0)
		{
			return false;
		}

		const EArcTerrainRail Rail = static_cast<EArcTerrainRail>(RailValue);
		if (Rail == EArcTerrainRail::Height ? NumPlanes != 0 : NumPlanes == 0)
		{
			return false;
		}

		const int64 Resolution = static_cast<int64>(ReadU32(Bytes + 20));
		if (Resolution <= 0 || Resolution > static_cast<int64>(ArcTerrain::MaxTileQuads) + 3)
		{
			return false;
		}

		const int64 CompressedBodyBytes = static_cast<int64>(ReadU32(Bytes + 24));
		const int64 UncompressedBodyBytes = static_cast<int64>(ReadU32(Bytes + 28));
		if (CompressionMethod == CompressionNone && CompressedBodyBytes != UncompressedBodyBytes)
		{
			return false;
		}

		const int64 BodyPlanes =
			Rail == EArcTerrainRail::Height ? HeightPlaneCount : static_cast<int64>(NumPlanes);
		const int64 PlaneTexelBytes =
			Resolution * Resolution * static_cast<int64>(BytesPerTexel(Rail));
		if (UncompressedBodyBytes != BodyPlanes * PlaneTexelBytes)
		{
			return false;
		}

		const int64 HeadBytes = static_cast<int64>(FixedHeaderBytes) + PlaneIdBytes(Rail, NumPlanes)
			+ QuantWindowBytes(Rail, NumPlanes);
		if (static_cast<int64>(FileBytes.Num()) < HeadBytes)
		{
			return false;
		}

		FArcTerrainTileHeader Header;
		Header.Version = TileVersion;
		Header.Rail = Rail;
		Header.TileExtentM = ReadU32(Bytes + 12);
		Header.TexelSizeCm = ReadU32(Bytes + 16);
		Header.Resolution = static_cast<int32>(Resolution);
		Header.CompressionMethod = CompressionMethod;
		Header.CompressedBodyBytes = static_cast<uint32>(CompressedBodyBytes);
		Header.UncompressedBodyBytes = static_cast<uint32>(UncompressedBodyBytes);
		if (Rail == EArcTerrainRail::Weight)
		{
			Header.PlaneIds.Reserve(NumPlanes);
			for (int32 PlaneIndex = 0; PlaneIndex < NumPlanes; ++PlaneIndex)
			{
				Header.PlaneIds.Add(ReadU32(Bytes + FixedHeaderBytes + PlaneIndex * 4));
			}
			if (!AreIdsUnique(TConstArrayView<uint32>(Header.PlaneIds)))
			{
				return false;
			}
		}
		else
		{
			Header.DataMapIds.Append(Bytes + FixedHeaderBytes, NumPlanes);
			if (!AreIdsUnique(TConstArrayView<uint8>(Header.DataMapIds)))
			{
				return false;
			}
		}

		if (Rail != EArcTerrainRail::Weight)
		{
			// NumPlanes is the id byte count, zero on the height rail, so this base serves both.
			const uint8* WindowBytes = Bytes + FixedHeaderBytes + NumPlanes;
			const int32 NumWindows = static_cast<int32>(BodyPlanes);
			Header.PlaneQuantMin.Reserve(NumWindows);
			Header.PlaneQuantMax.Reserve(NumWindows);
			for (int32 PlaneIndex = 0; PlaneIndex < NumWindows; ++PlaneIndex)
			{
				const float QuantMin = ReadFloat(WindowBytes + PlaneIndex * 8);
				const float QuantMax = ReadFloat(WindowBytes + PlaneIndex * 8 + 4);
				if (!FMath::IsFinite(QuantMin) || !FMath::IsFinite(QuantMax) || QuantMax < QuantMin)
				{
					return false;
				}
				Header.PlaneQuantMin.Add(QuantMin);
				Header.PlaneQuantMax.Add(QuantMax);
			}
		}

		if (ExpectedFileBytes(Header) != static_cast<int64>(FileBytes.Num()))
		{
			return false;
		}

		OutHeader = MoveTemp(Header);
		return true;
	}

	bool IsCompatible(const FArcTerrainTileHeader& Header, EArcTerrainRail ExpectedRail,
		const FArcTerrainTileGrid& ExpectedGrid)
	{
		return ExpectedGrid.IsValid() && Header.Rail == ExpectedRail
			&& Header.Resolution == ExpectedGrid.Resolution()
			&& Header.TileExtentM == static_cast<uint32>(ExpectedGrid.TileExtentM)
			&& Header.TexelSizeCm == static_cast<uint32>(ExpectedGrid.TexelSizeCm);
	}

	FArcTerrainTileHeader MakeWeightHeader(const FArcTerrainTileGrid& Grid, TConstArrayView<uint32> PlaneIds)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		FArcTerrainTileHeader Header;
		if (!Grid.IsValid() || PlaneIds.Num() <= 0 || PlaneIds.Num() > MaxPlaneCount
			|| !AreIdsUnique(PlaneIds))
		{
			return Header;
		}

		Header.Version = TileVersion;
		Header.Rail = EArcTerrainRail::Weight;
		Header.TileExtentM = static_cast<uint32>(Grid.TileExtentM);
		Header.TexelSizeCm = static_cast<uint32>(Grid.TexelSizeCm);
		Header.Resolution = Grid.Resolution();
		Header.PlaneIds.Append(PlaneIds.GetData(), PlaneIds.Num());
		return Header;
	}

	FArcTerrainTileHeader MakeHeightHeader(const FArcTerrainTileGrid& Grid, float QuantMin, float QuantMax)
	{
		FArcTerrainTileHeader Header;
		if (!Grid.IsValid() || !FMath::IsFinite(QuantMin) || !FMath::IsFinite(QuantMax)
			|| QuantMax < QuantMin)
		{
			return Header;
		}

		Header.Version = TileVersion;
		Header.Rail = EArcTerrainRail::Height;
		Header.TileExtentM = static_cast<uint32>(Grid.TileExtentM);
		Header.TexelSizeCm = static_cast<uint32>(Grid.TexelSizeCm);
		Header.Resolution = Grid.Resolution();
		Header.PlaneQuantMin.Add(QuantMin);
		Header.PlaneQuantMax.Add(QuantMax);
		return Header;
	}

	void EncodeHeightTexels(TConstArrayView<float> Heights, float QuantMin, float QuantMax,
		TArray<uint8>& OutBody)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		const double Span = static_cast<double>(QuantMax) - static_cast<double>(QuantMin);
		OutBody.Reserve(OutBody.Num() + Heights.Num() * 2);
		for (float Height : Heights)
		{
			// A flat tile has no span to divide by, and every texel of it is the window's floor.
			const double Ratio = Span > 0.0
				? (static_cast<double>(Height) - static_cast<double>(QuantMin)) / Span : 0.0;
			WriteU16(OutBody, static_cast<uint16>(
				FMath::Clamp(FMath::RoundToDouble(Ratio * 65535.0), 0.0, 65535.0)));
		}
	}

	void DecodeHeightTexels(TConstArrayView<uint8> Body, float QuantMin, float QuantMax,
		TArrayView<float> OutHeights)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		check(Body.Num() >= OutHeights.Num() * 2);

		const double Span = static_cast<double>(QuantMax) - static_cast<double>(QuantMin);
		const uint8* const Texels = Body.GetData();
		for (int32 Index = 0; Index < OutHeights.Num(); ++Index)
		{
			const uint16 Quantized = ReadU16(Texels + static_cast<int64>(Index) * 2);
			OutHeights[Index] = static_cast<float>(static_cast<double>(QuantMin)
				+ static_cast<double>(Quantized) / 65535.0 * Span);
		}
	}

	FArcTerrainTileHeader MakeDataHeader(const FArcTerrainTileGrid& Grid, TConstArrayView<uint8> DataMapIds,
		TConstArrayView<float> QuantMin, TConstArrayView<float> QuantMax)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		if (!Grid.IsValid() || DataMapIds.Num() <= 0 || DataMapIds.Num() > MaxPlaneCount
			|| !AreIdsUnique(DataMapIds) || QuantMin.Num() != DataMapIds.Num()
			|| QuantMax.Num() != DataMapIds.Num())
		{
			return FArcTerrainTileHeader();
		}

		for (int32 PlaneIndex = 0; PlaneIndex < DataMapIds.Num(); ++PlaneIndex)
		{
			if (!FMath::IsFinite(QuantMin[PlaneIndex]) || !FMath::IsFinite(QuantMax[PlaneIndex])
				|| QuantMax[PlaneIndex] < QuantMin[PlaneIndex])
			{
				return FArcTerrainTileHeader();
			}
		}

		FArcTerrainTileHeader Header;
		Header.Version = TileVersion;
		Header.Rail = EArcTerrainRail::Data;
		Header.TileExtentM = static_cast<uint32>(Grid.TileExtentM);
		Header.TexelSizeCm = static_cast<uint32>(Grid.TexelSizeCm);
		Header.Resolution = Grid.Resolution();
		Header.DataMapIds.Append(DataMapIds.GetData(), DataMapIds.Num());
		Header.PlaneQuantMin.Append(QuantMin.GetData(), QuantMin.Num());
		Header.PlaneQuantMax.Append(QuantMax.GetData(), QuantMax.Num());
		return Header;
	}

	EArcTerrainExtent RailTileExtent(const FArcTerrainConfig& Config, EArcTerrainRail Rail)
	{
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
			return Config.WeightTileExtent;
		case EArcTerrainRail::Data:
			return Config.DataTileExtent;
		default:
			return Config.HeightTileExtent;
		}
	}

	FArcTerrainTileGrid HeightTileGrid(const FArcTerrainConfig& Config, int32 CumulativeShift)
	{
		FArcTerrainTileGrid Grid;
		if (CumulativeShift < 0)
		{
			return Grid;
		}

		// Built past Config::TileQuads / CellsPerTileEdge / RailTileExtent on purpose: those are typed on
		// EArcTerrainExtent, which has no enumerator for the shifted edge a coarse tier covers.
		Grid.TileQuads = Config.HeightTileQuads();
		Grid.CellsPerTileEdge = Config.CellsPerTileEdge(Config.HeightTileExtent) << CumulativeShift;
		Grid.TileExtentM =
			static_cast<int32>(static_cast<uint32>(Config.HeightTileExtent)) << CumulativeShift;
		Grid.TexelSizeCm = Config.TexelSizeCm() << CumulativeShift;
		return Grid;
	}

	FArcTerrainTileGrid DataTileGrid(const FArcTerrainConfig& Config)
	{
		FArcTerrainTileGrid Grid;
		Grid.TileQuads = Config.DataTileQuads();
		Grid.CellsPerTileEdge = Config.CellsPerTileEdge(Config.DataTileExtent);
		Grid.TileExtentM = static_cast<int32>(static_cast<uint32>(Config.DataTileExtent));
		Grid.TexelSizeCm = Config.TexelSizeCm();
		return Grid;
	}

	FArcTerrainTileGrid WeightTileGrid(const FArcTerrainConfig& Config, int32 ResolvedCellWeightQuads,
		int32 CumulativeShift)
	{
		using namespace ArcTerrainTileFormatPrivate_7b41;

		FArcTerrainTileGrid Grid;
		if (CumulativeShift < 0)
		{
			return Grid;
		}

		Grid.CellsPerTileEdge = Config.CellsPerTileEdge(Config.WeightTileExtent) << CumulativeShift;
		Grid.TileExtentM =
			static_cast<int32>(static_cast<uint32>(Config.WeightTileExtent)) << CumulativeShift;
		if (ResolvedCellWeightQuads <= 0 || Grid.CellsPerTileEdge <= 0)
		{
			return Grid;
		}

		Grid.TileQuads = ResolvedCellWeightQuads * Grid.CellsPerTileEdge;
		Grid.TexelSizeCm = RoundedTexelSizeCm(Grid.TileExtentM, Grid.TileQuads);
		return Grid;
	}

	bool TileKeyFromLocal(const FArcTerrainTileGrid& Grid, FVector2D Local, FIntPoint& OutKey)
	{
		if (!Grid.IsValid())
		{
			return false;
		}
		const double TileSizeCm = static_cast<double>(Grid.TileExtentM) * 100.0;

		const double KeyX = FMath::FloorToDouble(Local.X / TileSizeCm);
		const double KeyY = FMath::FloorToDouble(Local.Y / TileSizeCm);
		if (KeyX < static_cast<double>(MIN_int32) || KeyX > static_cast<double>(MAX_int32)
			|| KeyY < static_cast<double>(MIN_int32) || KeyY > static_cast<double>(MAX_int32))
		{
			return false;
		}

		OutKey = FIntPoint(static_cast<int32>(KeyX), static_cast<int32>(KeyY));
		return true;
	}
}
