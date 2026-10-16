// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSiteChunkStorage.h"

#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"

#include <type_traits>

struct FArcEcotopeSiteChunkBuildWriterState
{
	FArcEcotopeSiteMetadata Metadata;
	FArcEcotopeChunkLayout Layout;
	FArcEcotopeSiteChunkManifest Manifest;
	TArray<FUniqueBuffer> MutablePayloads;
	TArray<TArray<uint8>> WrittenFields;
	bool bFinalized = false;
};

namespace ArcEcotopeSiteChunkStoragePrivate
{
	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

	struct FFieldInfo
	{
		EArcEcotopeSiteField Field;
		EArcEcotopeSiteValueType ValueType;
		uint8 Level;
		const TCHAR* Name;
	};

	void HashByte(uint64& Hash, const uint8 Value)
	{
		Hash ^= Value;
		Hash *= FnvPrime;
	}

	void HashBytes(uint64& Hash, const void* Data, const uint64 Size)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		for (uint64 Index = 0; Index < Size; ++Index)
		{
			HashByte(Hash, Bytes[Index]);
		}
	}

	template <typename IntegerType>
	void HashInteger(uint64& Hash, const IntegerType Value)
	{
		using UnsignedType = std::make_unsigned_t<IntegerType>;
		const UnsignedType UnsignedValue = static_cast<UnsignedType>(Value);
		for (uint32 ByteIndex = 0; ByteIndex < sizeof(IntegerType); ++ByteIndex)
		{
			HashByte(Hash, static_cast<uint8>(UnsignedValue >> (ByteIndex * 8)));
		}
	}

	void HashFloat(uint64& Hash, float Value)
	{
		if (Value == 0.0f)
		{
			Value = 0.0f;
		}
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &Value, sizeof(Bits));
		HashInteger(Hash, Bits);
	}

	bool TryAdd(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Left > MAX_uint64 - Right)
		{
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

	bool TryMultiply(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Left > 0 && Right > MAX_uint64 / Left)
		{
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool TryAlign4(const uint64 Value, uint64& OutValue)
	{
		if (!TryAdd(Value, 3, OutValue))
		{
			return false;
		}
		OutValue &= ~3ull;
		return true;
	}

	TArray<FFieldInfo> MakeFieldInfos(const FArcEcotopeSiteMetadata& Metadata)
	{
		return {
			{EArcEcotopeSiteField::LocalSurfaceZ, EArcEcotopeSiteValueType::Float32, 0, TEXT("LocalSurfaceZ")},
			{EArcEcotopeSiteField::ValidSurface, EArcEcotopeSiteValueType::UInt8, 0, TEXT("ValidSurface")},
			{EArcEcotopeSiteField::Slope, EArcEcotopeSiteValueType::Float32, 0, TEXT("Slope")},
			{EArcEcotopeSiteField::Aspect, EArcEcotopeSiteValueType::Float32, 0, TEXT("Aspect")},
			{EArcEcotopeSiteField::Moisture, EArcEcotopeSiteValueType::Float32, Metadata.MoistureLevel, TEXT("Moisture")},
			{EArcEcotopeSiteField::Fertility, EArcEcotopeSiteValueType::Float32, Metadata.FertilityLevel, TEXT("Fertility")},
			{EArcEcotopeSiteField::SunLoad, EArcEcotopeSiteValueType::Float32, Metadata.SunLoadLevel, TEXT("SunLoad")},
			{EArcEcotopeSiteField::WindExposure, EArcEcotopeSiteValueType::Float32, Metadata.WindExposureLevel, TEXT("WindExposure")},
			{EArcEcotopeSiteField::SkyViewFactor, EArcEcotopeSiteValueType::Float32, Metadata.SkyViewFactorLevel, TEXT("SkyViewFactor")},
			{EArcEcotopeSiteField::Temperature, EArcEcotopeSiteValueType::Float32, Metadata.TemperatureLevel, TEXT("Temperature")},
			{EArcEcotopeSiteField::ModificationCut, EArcEcotopeSiteValueType::Float32, Metadata.ModificationCutLevel, TEXT("ModificationCut")},
			{EArcEcotopeSiteField::RecordValid, EArcEcotopeSiteValueType::UInt8, Metadata.RecordGridLevel, TEXT("RecordValid")},
			{EArcEcotopeSiteField::BareRock, EArcEcotopeSiteValueType::Float32, Metadata.BareRockLevel, TEXT("BareRock")},
			{EArcEcotopeSiteField::ChannelExclusion, EArcEcotopeSiteValueType::Float32, Metadata.ChannelExclusionLevel, TEXT("ChannelExclusion")},
			{EArcEcotopeSiteField::ChannelBuffer, EArcEcotopeSiteValueType::Float32, 0, TEXT("ChannelBuffer")},
			{EArcEcotopeSiteField::EstablishmentWear, EArcEcotopeSiteValueType::Float32, Metadata.EstablishmentWearLevel, TEXT("EstablishmentWear")},
			{EArcEcotopeSiteField::SubstrateMobility, EArcEcotopeSiteValueType::Float32, Metadata.SubstrateMobilityLevel, TEXT("SubstrateMobility")},
		};
	}

	uint64 CalculateDescriptorHash(const FArcEcotopeSiteChunkManifestEntry& Entry)
	{
		uint64 Hash = FnvOffsetBasis;
		HashInteger(Hash, Entry.ChunkId.X);
		HashInteger(Hash, Entry.ChunkId.Y);
		HashInteger(Hash, Entry.FieldMask);
		HashInteger(Hash, Entry.Fields.Num());
		for (const FArcEcotopeSiteFieldBlockDescriptor& Field : Entry.Fields)
		{
			HashByte(Hash, static_cast<uint8>(Field.Field));
			HashByte(Hash, static_cast<uint8>(Field.ValueType));
			HashByte(Hash, Field.Level);
			HashInteger(Hash, Field.RectMin.X);
			HashInteger(Hash, Field.RectMin.Y);
			HashInteger(Hash, Field.RectMax.X);
			HashInteger(Hash, Field.RectMax.Y);
			HashInteger(Hash, Field.ByteOffset);
			HashInteger(Hash, Field.ByteSize);
			HashInteger(Hash, Field.DataHash);
		}
		return Hash;
	}

	void WriteUInt16(uint8* Data, const uint64 Offset, const uint16 Value)
	{
		Data[Offset] = static_cast<uint8>(Value);
		Data[Offset + 1] = static_cast<uint8>(Value >> 8);
	}

	void WriteUInt32(uint8* Data, const uint64 Offset, const uint32 Value)
	{
		for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
		{
			Data[Offset + ByteIndex] = static_cast<uint8>(Value >> (ByteIndex * 8));
		}
	}

	void WriteUInt64(uint8* Data, const uint64 Offset, const uint64 Value)
	{
		for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			Data[Offset + ByteIndex] = static_cast<uint8>(Value >> (ByteIndex * 8));
		}
	}

	uint16 ReadUInt16(const uint8* Data, const uint64 Offset)
	{
		return static_cast<uint16>(Data[Offset]) | static_cast<uint16>(Data[Offset + 1] << 8);
	}

	uint32 ReadUInt32(const uint8* Data, const uint64 Offset)
	{
		uint32 Value = 0;
		for (uint32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
		{
			Value |= static_cast<uint32>(Data[Offset + ByteIndex]) << (ByteIndex * 8);
		}
		return Value;
	}

	uint64 ReadUInt64(const uint8* Data, const uint64 Offset)
	{
		uint64 Value = 0;
		for (uint32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			Value |= static_cast<uint64>(Data[Offset + ByteIndex]) << (ByteIndex * 8);
		}
		return Value;
	}

	bool MakeExpectedEntry(
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcEcotopeChunkRect& Chunk,
		const int32 PayloadIndex,
		FArcEcotopeSiteChunkManifestEntry& OutEntry,
		FString& OutError)
	{
		OutEntry = FArcEcotopeSiteChunkManifestEntry();
		OutEntry.ChunkId = FIntPoint(Chunk.Id.X, Chunk.Id.Y);
		OutEntry.BaseCoreMin = Chunk.BaseCoreRect.Min;
		OutEntry.BaseCoreMax = Chunk.BaseCoreRect.Max;
		OutEntry.PayloadIndex = PayloadIndex;
		uint64 NextOffset = FArcEcotopeSiteChunkStorage::PayloadHeaderBytes;

		for (const FFieldInfo& Field : MakeFieldInfos(Metadata))
		{
			FIntRect OwnedRect;
			if (!FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
					Chunk.BaseCoreRect, Metadata.GridDimensions, Field.Level, OwnedRect))
			{
				OutError = FString::Printf(TEXT("ArcEcotope site chunk could not map field %s ownership."), Field.Name);
				return false;
			}
			if (OwnedRect.Width() == 0 || OwnedRect.Height() == 0)
			{
				continue;
			}

			if (!TryAlign4(NextOffset, NextOffset))
			{
				OutError = TEXT("ArcEcotope site chunk field offset overflowed.");
				return false;
			}
			uint64 CellCount = 0;
			uint64 ByteSize = 0;
			const uint64 ValueSize = Field.ValueType == EArcEcotopeSiteValueType::Float32 ? sizeof(float) : sizeof(uint8);
			if (!TryMultiply(OwnedRect.Width(), OwnedRect.Height(), CellCount)
				|| !TryMultiply(CellCount, ValueSize, ByteSize))
			{
				OutError = TEXT("ArcEcotope site chunk field size overflowed.");
				return false;
			}

			FArcEcotopeSiteFieldBlockDescriptor& Descriptor = OutEntry.Fields.AddDefaulted_GetRef();
			Descriptor.Field = Field.Field;
			Descriptor.ValueType = Field.ValueType;
			Descriptor.Level = Field.Level;
			Descriptor.RectMin = OwnedRect.Min;
			Descriptor.RectMax = OwnedRect.Max;
			Descriptor.ByteOffset = NextOffset;
			Descriptor.ByteSize = ByteSize;
			Descriptor.DataHash = 0;
			OutEntry.FieldMask |= 1u << static_cast<uint8>(Field.Field);
			if (!TryAdd(NextOffset, ByteSize, NextOffset))
			{
				OutError = TEXT("ArcEcotope site chunk payload size overflowed.");
				return false;
			}
		}
		OutEntry.PayloadSize = NextOffset;
		return true;
	}

	bool DescriptorsEqual(
		const FArcEcotopeSiteFieldBlockDescriptor& Left,
		const FArcEcotopeSiteFieldBlockDescriptor& Right,
		const bool bCompareDataHash)
	{
		return Left.Field == Right.Field && Left.ValueType == Right.ValueType && Left.Level == Right.Level
			&& Left.RectMin == Right.RectMin && Left.RectMax == Right.RectMax
			&& Left.ByteOffset == Right.ByteOffset && Left.ByteSize == Right.ByteSize
			&& (!bCompareDataHash || Left.DataHash == Right.DataHash);
	}
}

namespace ArcEcotopeSiteChunkStoragePrivate
{
	bool FindFieldInfo(
		const FArcEcotopeSiteMetadata& Metadata,
		const EArcEcotopeSiteField Field,
		FFieldInfo& OutInfo)
	{
		for (const FFieldInfo& Candidate : MakeFieldInfos(Metadata))
		{
			if (Candidate.Field == Field)
			{
				OutInfo = Candidate;
				return true;
			}
		}
		return false;
	}

	bool FindDescriptorIndex(
		const FArcEcotopeSiteChunkManifestEntry& Entry,
		const EArcEcotopeSiteField Field,
		int32& OutDescriptorIndex)
	{
		OutDescriptorIndex = INDEX_NONE;
		for (int32 DescriptorIndex = 0; DescriptorIndex < Entry.Fields.Num(); ++DescriptorIndex)
		{
			if (Entry.Fields[DescriptorIndex].Field == Field)
			{
				OutDescriptorIndex = DescriptorIndex;
				return true;
			}
		}
		return false;
	}

	bool TryGetWriterChunkIndex(
		const FArcEcotopeChunkLayout& Layout,
		const FArcEcotopeChunkId& Id,
		int32& OutChunkIndex)
	{
		OutChunkIndex = INDEX_NONE;
		const FIntPoint ChunkGrid = Layout.GetChunkGridDimensions();
		if (Id.X < 0 || Id.Y < 0 || Id.X >= ChunkGrid.X || Id.Y >= ChunkGrid.Y)
		{
			return false;
		}
		const int64 ChunkIndex = static_cast<int64>(Id.X) + static_cast<int64>(Id.Y) * ChunkGrid.X;
		if (ChunkIndex < 0 || ChunkIndex >= Layout.GetChunkCount() || ChunkIndex > MAX_int32)
		{
			return false;
		}
		OutChunkIndex = static_cast<int32>(ChunkIndex);
		return true;
	}

	bool ValidateWriterMetadataAndLayout(
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcEcotopeChunkLayout& Layout,
		FString& OutError)
	{
		OutError.Reset();
		const auto IsFiniteVector = [](const FVector& Value)
		{
			return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
		};
		if (Metadata.SiteSchemaVersion != FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion()
			|| !Metadata.LocalBounds.IsValid
			|| !IsFiniteVector(Metadata.LocalBounds.Min)
			|| !IsFiniteVector(Metadata.LocalBounds.Max)
			|| Metadata.LocalBounds.Min.X >= Metadata.LocalBounds.Max.X
			|| Metadata.LocalBounds.Min.Y >= Metadata.LocalBounds.Max.Y
			|| !FMath::IsFinite(Metadata.CellSizeCm) || Metadata.CellSizeCm <= 0.0f
			|| Metadata.GridDimensions.X <= 0 || Metadata.GridDimensions.Y <= 0
			|| Metadata.RecordGridLevel > FArcEcotopeSiteMetadata::MaxGridLevel
			|| Layout.GetBaseDimensions() != Metadata.GridDimensions
			|| Layout.GetRecordDimensions() != Metadata.GetLevelDimensions(Metadata.RecordGridLevel)
			|| Layout.GetChunkGridDimensions().X <= 0 || Layout.GetChunkGridDimensions().Y <= 0
			|| Layout.GetChunkCount() <= 0 || Layout.GetRecordHaloCells() != 0
			|| Layout.GetCoreBaseCellSpan() <= 0)
		{
			OutError = TEXT("ArcEcotope chunk writer received invalid metadata or a non-final chunk layout.");
			return false;
		}

		for (const FFieldInfo& Field : MakeFieldInfos(Metadata))
		{
			if (Field.Level > FArcEcotopeSiteMetadata::MaxGridLevel)
			{
				OutError = FString::Printf(TEXT("ArcEcotope chunk writer field %s has an unsupported level."), Field.Name);
				return false;
			}
			const int32 Divisor = 1 << Field.Level;
			if (Divisor <= 0 || Layout.GetCoreBaseCellSpan() % Divisor != 0)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope chunk writer core span %d must be divisible by %s level divisor %d."),
					Layout.GetCoreBaseCellSpan(), Field.Name, Divisor);
				return false;
			}
		}
		return true;
	}

	bool WriteOwnedField(
		FArcEcotopeSiteChunkBuildWriterState& WriterState,
		const FArcEcotopeChunkId& ChunkId,
		const EArcEcotopeSiteField Field,
		const EArcEcotopeSiteValueType ValueType,
		const uint8 Level,
		const FIntRect& OwnedRect,
		const void* Values,
		const int32 ValueCount,
		FString& OutError)
	{
		OutError.Reset();
		FFieldInfo ExpectedField;
		int32 ChunkIndex = INDEX_NONE;
		if (!FindFieldInfo(WriterState.Metadata, Field, ExpectedField)
			|| ExpectedField.ValueType != ValueType
			|| ExpectedField.Level != Level
			|| !TryGetWriterChunkIndex(WriterState.Layout, ChunkId, ChunkIndex)
			|| !WriterState.Manifest.Chunks.IsValidIndex(ChunkIndex)
			|| !WriterState.MutablePayloads.IsValidIndex(ChunkIndex)
			|| !WriterState.WrittenFields.IsValidIndex(ChunkIndex))
		{
			OutError = TEXT("ArcEcotope chunk writer received an unknown field, type, level, or chunk.");
			return false;
		}

		FArcEcotopeChunkRect Chunk;
		FIntRect ExpectedRect;
		int32 DescriptorIndex = INDEX_NONE;
		FArcEcotopeSiteChunkManifestEntry& Entry = WriterState.Manifest.Chunks[ChunkIndex];
		if (!WriterState.Layout.GetChunk(ChunkId, Chunk)
			|| !FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
				Chunk.BaseCoreRect, WriterState.Metadata.GridDimensions, Level, ExpectedRect)
			|| !FindDescriptorIndex(Entry, Field, DescriptorIndex)
			|| !Entry.Fields.IsValidIndex(DescriptorIndex)
			|| !WriterState.WrittenFields[ChunkIndex].IsValidIndex(DescriptorIndex)
			|| OwnedRect != ExpectedRect)
		{
			OutError = TEXT("ArcEcotope chunk writer field rectangle does not equal the chunk's owned rectangle.");
			return false;
		}

		FArcEcotopeSiteFieldBlockDescriptor& Descriptor = Entry.Fields[DescriptorIndex];
		if (WriterState.WrittenFields[ChunkIndex][DescriptorIndex] != 0)
		{
			OutError = TEXT("ArcEcotope chunk writer received a duplicate chunk-field write.");
			return false;
		}
		const uint64 ValueSize = ValueType == EArcEcotopeSiteValueType::Float32 ? sizeof(float) : sizeof(uint8);
		uint64 ExpectedValueCount = 0;
		uint64 ExpectedByteSize = 0;
		uint64 EndOffset = 0;
		if (OwnedRect.Width() <= 0 || OwnedRect.Height() <= 0 || Values == nullptr
			|| !TryMultiply(static_cast<uint64>(OwnedRect.Width()), static_cast<uint64>(OwnedRect.Height()), ExpectedValueCount)
			|| !TryMultiply(ExpectedValueCount, ValueSize, ExpectedByteSize)
			|| ExpectedValueCount != static_cast<uint64>(ValueCount)
			|| Descriptor.ValueType != ValueType || Descriptor.Level != Level
			|| Descriptor.RectMin != OwnedRect.Min || Descriptor.RectMax != OwnedRect.Max
			|| Descriptor.ByteSize != ExpectedByteSize
			|| !TryAdd(Descriptor.ByteOffset, Descriptor.ByteSize, EndOffset)
			|| Descriptor.ByteOffset < FArcEcotopeSiteChunkStorage::PayloadHeaderBytes
			|| EndOffset > WriterState.MutablePayloads[ChunkIndex].GetSize())
		{
			OutError = TEXT("ArcEcotope chunk writer field byte count or payload offset is invalid.");
			return false;
		}
		if (ValueType == EArcEcotopeSiteValueType::UInt8)
		{
			const uint8* ByteValues = static_cast<const uint8*>(Values);
			for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
			{
				if (ByteValues[ValueIndex] > 1)
				{
					OutError = FString::Printf(
						TEXT("ArcEcotope chunk writer %s value %d is not a binary mask value."),
						ExpectedField.Name, ValueIndex);
					return false;
				}
			}
		}
		else
		{
			const float* FloatValues = static_cast<const float*>(Values);
			for (int32 ValueIndex = 0; ValueIndex < ValueCount; ++ValueIndex)
			{
				const float Value = FloatValues[ValueIndex];
				const bool bValid = Field == EArcEcotopeSiteField::LocalSurfaceZ
					? FMath::IsFinite(Value)
					: Field == EArcEcotopeSiteField::Slope
						? FMath::IsFinite(Value) && Value >= 0.0f && Value <= 180.0f
						: Field == EArcEcotopeSiteField::Aspect
							? FMath::IsFinite(Value) && Value >= 0.0f && Value < 1.0f
							: FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
				if (!bValid)
				{
					OutError = FString::Printf(
						TEXT("ArcEcotope chunk writer %s value %d is outside its valid domain."),
						ExpectedField.Name, ValueIndex);
					return false;
				}
			}
		}

		uint8* PayloadData = static_cast<uint8*>(WriterState.MutablePayloads[ChunkIndex].GetData());
		FMemory::Memcpy(PayloadData + Descriptor.ByteOffset, Values, Descriptor.ByteSize);
		Descriptor.DataHash = FnvOffsetBasis;
		HashBytes(Descriptor.DataHash, Values, Descriptor.ByteSize);
		WriterState.WrittenFields[ChunkIndex][DescriptorIndex] = 1;
		return true;
	}

	bool ReadByteFieldValue(
		const FArcEcotopeSiteFieldBlockDescriptor& Descriptor,
		const FUniqueBuffer& Payload,
		const int32 X,
		const int32 Y,
		uint8& OutValue,
		FString& OutError)
	{
		OutValue = 0;
		if (Descriptor.ValueType != EArcEcotopeSiteValueType::UInt8
			|| X < Descriptor.RectMin.X || X >= Descriptor.RectMax.X
			|| Y < Descriptor.RectMin.Y || Y >= Descriptor.RectMax.Y)
		{
			OutError = TEXT("ArcEcotope chunk writer byte-field lookup is outside its owned rectangle.");
			return false;
		}
		const int32 Width = Descriptor.RectMax.X - Descriptor.RectMin.X;
		uint64 RowOffset = 0;
		uint64 ValueOffset = 0;
		uint64 EndOffset = 0;
		uint64 DescriptorEndOffset = 0;
		if (Width <= 0
			|| !TryMultiply(static_cast<uint64>(Y - Descriptor.RectMin.Y), static_cast<uint64>(Width), RowOffset)
			|| !TryAdd(Descriptor.ByteOffset, RowOffset, ValueOffset)
			|| !TryAdd(ValueOffset, static_cast<uint64>(X - Descriptor.RectMin.X), ValueOffset)
			|| !TryAdd(ValueOffset, 1, EndOffset)
			|| !TryAdd(Descriptor.ByteOffset, Descriptor.ByteSize, DescriptorEndOffset)
			|| Descriptor.ByteOffset < FArcEcotopeSiteChunkStorage::PayloadHeaderBytes
			|| ValueOffset < Descriptor.ByteOffset || EndOffset > DescriptorEndOffset
			|| EndOffset > Payload.GetSize())
		{
			OutError = TEXT("ArcEcotope chunk writer byte-field lookup has an invalid payload offset.");
			return false;
		}
		OutValue = static_cast<const uint8*>(Payload.GetData())[ValueOffset];
		return true;
	}

	bool ValidateRecordValidCoverage(
		const FArcEcotopeSiteChunkBuildWriterState& WriterState,
		FString& OutError)
	{
		FFieldInfo ValidSurfaceField;
		FFieldInfo RecordValidField;
		if (!FindFieldInfo(WriterState.Metadata, EArcEcotopeSiteField::ValidSurface, ValidSurfaceField)
			|| !FindFieldInfo(WriterState.Metadata, EArcEcotopeSiteField::RecordValid, RecordValidField)
			|| ValidSurfaceField.ValueType != EArcEcotopeSiteValueType::UInt8
			|| ValidSurfaceField.Level != 0
			|| RecordValidField.ValueType != EArcEcotopeSiteValueType::UInt8
			|| RecordValidField.Level != WriterState.Metadata.RecordGridLevel)
		{
			OutError = TEXT("ArcEcotope chunk writer record-valid field contract is invalid.");
			return false;
		}
		const int32 RecordDivisor = 1 << RecordValidField.Level;
		if (RecordDivisor <= 0 || WriterState.Layout.GetCoreBaseCellSpan() % RecordDivisor != 0)
		{
			OutError = TEXT("ArcEcotope chunk writer record-valid ownership is not core-aligned.");
			return false;
		}

		for (int32 ChunkIndex = 0; ChunkIndex < WriterState.Manifest.Chunks.Num(); ++ChunkIndex)
		{
			const FArcEcotopeSiteChunkManifestEntry& Entry = WriterState.Manifest.Chunks[ChunkIndex];
			int32 ValidSurfaceDescriptorIndex = INDEX_NONE;
			int32 RecordValidDescriptorIndex = INDEX_NONE;
			FArcEcotopeChunkRect Chunk;
			FIntRect ExpectedValidSurfaceRect;
			FIntRect ExpectedRecordValidRect;
			if (!WriterState.MutablePayloads.IsValidIndex(ChunkIndex)
				|| !WriterState.Layout.GetChunk(ChunkIndex, Chunk)
				|| !FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
					Chunk.BaseCoreRect, WriterState.Metadata.GridDimensions, 0, ExpectedValidSurfaceRect)
				|| !FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
					Chunk.BaseCoreRect, WriterState.Metadata.GridDimensions, RecordValidField.Level, ExpectedRecordValidRect)
				|| !FindDescriptorIndex(Entry, EArcEcotopeSiteField::ValidSurface, ValidSurfaceDescriptorIndex)
				|| !FindDescriptorIndex(Entry, EArcEcotopeSiteField::RecordValid, RecordValidDescriptorIndex))
			{
				OutError = TEXT("ArcEcotope chunk writer record-valid payload metadata is incomplete.");
				return false;
			}
			const FArcEcotopeSiteFieldBlockDescriptor& ValidSurface = Entry.Fields[ValidSurfaceDescriptorIndex];
			const FArcEcotopeSiteFieldBlockDescriptor& RecordValid = Entry.Fields[RecordValidDescriptorIndex];
			if (ValidSurface.ValueType != EArcEcotopeSiteValueType::UInt8 || ValidSurface.Level != 0
				|| RecordValid.ValueType != EArcEcotopeSiteValueType::UInt8 || RecordValid.Level != RecordValidField.Level
				|| FIntRect(ValidSurface.RectMin, ValidSurface.RectMax) != ExpectedValidSurfaceRect
				|| FIntRect(RecordValid.RectMin, RecordValid.RectMax) != ExpectedRecordValidRect)
			{
				OutError = TEXT("ArcEcotope chunk writer record-valid descriptors do not match final ownership.");
				return false;
			}

			for (int32 RecordY = RecordValid.RectMin.Y; RecordY < RecordValid.RectMax.Y; ++RecordY)
			{
				for (int32 RecordX = RecordValid.RectMin.X; RecordX < RecordValid.RectMax.X; ++RecordX)
				{
					const int64 BaseMinX64 = static_cast<int64>(RecordX) * RecordDivisor;
					const int64 BaseMinY64 = static_cast<int64>(RecordY) * RecordDivisor;
					const int64 BaseMaxX64 = FMath::Min<int64>(
						BaseMinX64 + RecordDivisor, WriterState.Metadata.GridDimensions.X);
					const int64 BaseMaxY64 = FMath::Min<int64>(
						BaseMinY64 + RecordDivisor, WriterState.Metadata.GridDimensions.Y);
					if (BaseMinX64 < ValidSurface.RectMin.X || BaseMinY64 < ValidSurface.RectMin.Y
						|| BaseMaxX64 > ValidSurface.RectMax.X || BaseMaxY64 > ValidSurface.RectMax.Y
						|| BaseMaxX64 <= BaseMinX64 || BaseMaxY64 <= BaseMinY64)
					{
						OutError = TEXT("ArcEcotope chunk writer record cell crosses its minimum-corner owning chunk.");
						return false;
					}
					bool bAnyValidSurface = false;
					for (int32 BaseY = static_cast<int32>(BaseMinY64); BaseY < static_cast<int32>(BaseMaxY64); ++BaseY)
					{
						for (int32 BaseX = static_cast<int32>(BaseMinX64); BaseX < static_cast<int32>(BaseMaxX64); ++BaseX)
						{
							uint8 SurfaceValue = 0;
							if (!ReadByteFieldValue(
									ValidSurface, WriterState.MutablePayloads[ChunkIndex], BaseX, BaseY, SurfaceValue, OutError)
								|| SurfaceValue > 1)
							{
								if (OutError.IsEmpty())
								{
									OutError = TEXT("ArcEcotope chunk writer ValidSurface contains a non-binary value.");
								}
								return false;
							}
							bAnyValidSurface |= SurfaceValue != 0;
						}
					}
					uint8 RecordValue = 0;
					if (!ReadByteFieldValue(
							RecordValid, WriterState.MutablePayloads[ChunkIndex], RecordX, RecordY, RecordValue, OutError)
						|| RecordValue > 1)
					{
						if (OutError.IsEmpty())
						{
							OutError = TEXT("ArcEcotope chunk writer RecordValid contains a non-binary value.");
						}
						return false;
					}
					if ((RecordValue != 0) != bAnyValidSurface)
					{
						OutError = FString::Printf(
							TEXT("ArcEcotope chunk writer RecordValid %d,%d does not equal the OR of its base children."),
							RecordX, RecordY);
						return false;
					}
				}
			}
		}
		return true;
	}

	bool HashWrittenField(
		const FArcEcotopeSiteChunkBuildWriterState& WriterState,
		const FFieldInfo& Field,
		uint64& InOutHash,
		FString& OutError)
	{
		const FIntPoint Dimensions = WriterState.Metadata.GetLevelDimensions(Field.Level);
		const FIntPoint ChunkGrid = WriterState.Layout.GetChunkGridDimensions();
		int32 ExpectedMinY = 0;
		for (int32 ChunkY = 0; ChunkY < ChunkGrid.Y; ++ChunkY)
		{
			FArcEcotopeChunkRect FirstChunk;
			FIntRect FirstOwnedRect;
			if (!WriterState.Layout.GetChunk(FArcEcotopeChunkId{0, ChunkY}, FirstChunk)
				|| !FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
					FirstChunk.BaseCoreRect, WriterState.Metadata.GridDimensions, Field.Level, FirstOwnedRect)
				|| FirstOwnedRect.Min.Y != ExpectedMinY)
			{
				OutError = TEXT("ArcEcotope chunk writer cannot reconstruct a complete field row.");
				return false;
			}
			for (int32 Y = FirstOwnedRect.Min.Y; Y < FirstOwnedRect.Max.Y; ++Y)
			{
				int32 ExpectedMinX = 0;
				for (int32 ChunkX = 0; ChunkX < ChunkGrid.X; ++ChunkX)
				{
					const FArcEcotopeChunkId Id{ChunkX, ChunkY};
					int32 ChunkIndex = INDEX_NONE;
					int32 DescriptorIndex = INDEX_NONE;
					FArcEcotopeChunkRect Chunk;
					FIntRect ExpectedRect;
					if (!TryGetWriterChunkIndex(WriterState.Layout, Id, ChunkIndex)
						|| !WriterState.Layout.GetChunk(Id, Chunk)
						|| !FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
							Chunk.BaseCoreRect, WriterState.Metadata.GridDimensions, Field.Level, ExpectedRect)
						|| !WriterState.Manifest.Chunks.IsValidIndex(ChunkIndex)
						|| !WriterState.MutablePayloads.IsValidIndex(ChunkIndex)
						|| !WriterState.WrittenFields.IsValidIndex(ChunkIndex)
						|| !FindDescriptorIndex(WriterState.Manifest.Chunks[ChunkIndex], Field.Field, DescriptorIndex)
						|| !WriterState.WrittenFields[ChunkIndex].IsValidIndex(DescriptorIndex)
						|| WriterState.WrittenFields[ChunkIndex][DescriptorIndex] == 0)
					{
						OutError = TEXT("ArcEcotope chunk writer is missing a required chunk-field write.");
						return false;
					}
					const FArcEcotopeSiteFieldBlockDescriptor& Descriptor =
						WriterState.Manifest.Chunks[ChunkIndex].Fields[DescriptorIndex];
					if (ExpectedRect != FIntRect(Descriptor.RectMin, Descriptor.RectMax)
						|| ExpectedRect.Min.X != ExpectedMinX || ExpectedRect.Min.Y != FirstOwnedRect.Min.Y
						|| ExpectedRect.Max.Y != FirstOwnedRect.Max.Y || Y < ExpectedRect.Min.Y || Y >= ExpectedRect.Max.Y)
					{
						OutError = TEXT("ArcEcotope chunk writer field ownership has a gap or overlap.");
						return false;
					}
					const int32 RowWidth = ExpectedRect.Width();
					const uint64 ValueSize = Field.ValueType == EArcEcotopeSiteValueType::Float32 ? sizeof(float) : sizeof(uint8);
					const uint64 RowBytes = static_cast<uint64>(RowWidth) * ValueSize;
					const uint64 RowOffset = Descriptor.ByteOffset
						+ static_cast<uint64>(Y - ExpectedRect.Min.Y) * RowBytes;
					const uint8* RowData = static_cast<const uint8*>(WriterState.MutablePayloads[ChunkIndex].GetData()) + RowOffset;
					if (Field.ValueType == EArcEcotopeSiteValueType::UInt8)
					{
						HashBytes(InOutHash, RowData, RowBytes);
					}
					else
					{
						for (int32 X = 0; X < RowWidth; ++X)
						{
							float Value = 0.0f;
							FMemory::Memcpy(&Value, RowData + static_cast<uint64>(X) * sizeof(float), sizeof(float));
							HashFloat(InOutHash, Value);
						}
					}
					ExpectedMinX = ExpectedRect.Max.X;
				}
				if (ExpectedMinX != Dimensions.X)
				{
					OutError = TEXT("ArcEcotope chunk writer field row does not cover the declared dimensions.");
					return false;
				}
			}
			ExpectedMinY = FirstOwnedRect.Max.Y;
		}
		if (ExpectedMinY != Dimensions.Y)
		{
			OutError = TEXT("ArcEcotope chunk writer field rows do not cover the declared dimensions.");
			return false;
		}
		return true;
	}

	bool ValidateFinalMetadata(
		const FArcEcotopeSiteMetadata& ProvisionalMetadata,
		const FArcEcotopeSiteMetadata& FinalMetadata,
		FString& OutError)
	{
		if (FinalMetadata.SiteSchemaVersion != ProvisionalMetadata.SiteSchemaVersion
			|| FinalMetadata.LocalBounds.Min != ProvisionalMetadata.LocalBounds.Min
			|| FinalMetadata.LocalBounds.Max != ProvisionalMetadata.LocalBounds.Max
			|| FinalMetadata.CellSizeCm != ProvisionalMetadata.CellSizeCm
			|| FinalMetadata.GridDimensions != ProvisionalMetadata.GridDimensions
			|| FinalMetadata.RecordGridLevel != ProvisionalMetadata.RecordGridLevel
			|| FinalMetadata.MoistureLevel != ProvisionalMetadata.MoistureLevel
			|| FinalMetadata.FertilityLevel != ProvisionalMetadata.FertilityLevel
			|| FinalMetadata.SunLoadLevel != ProvisionalMetadata.SunLoadLevel
			|| FinalMetadata.WindExposureLevel != ProvisionalMetadata.WindExposureLevel
			|| FinalMetadata.SkyViewFactorLevel != ProvisionalMetadata.SkyViewFactorLevel
			|| FinalMetadata.TemperatureLevel != ProvisionalMetadata.TemperatureLevel
			|| FinalMetadata.ModificationCutLevel != ProvisionalMetadata.ModificationCutLevel
			|| FinalMetadata.BareRockLevel != ProvisionalMetadata.BareRockLevel
			|| FinalMetadata.ChannelExclusionLevel != ProvisionalMetadata.ChannelExclusionLevel
			|| FinalMetadata.EstablishmentWearLevel != ProvisionalMetadata.EstablishmentWearLevel
			|| FinalMetadata.SubstrateMobilityLevel != ProvisionalMetadata.SubstrateMobilityLevel)
		{
			OutError = TEXT("ArcEcotope chunk writer final metadata changed a layout-dependent scalar.");
			return false;
		}
		return true;
	}

	bool CalculateFinalIdentities(
		const FArcEcotopeSiteChunkBuildWriterState& WriterState,
		const FArcEcotopeSiteMetadata& FinalScalarMetadata,
		FArcEcotopeSiteMetadata& OutMetadata,
		uint64& OutLogicalFieldHash,
		FString& OutError)
	{
		OutMetadata = FinalScalarMetadata;
		OutLogicalFieldHash = FnvOffsetBasis;
		const TArray<FFieldInfo> Fields = MakeFieldInfos(OutMetadata);
		for (const FFieldInfo& Field : Fields)
		{
			const FIntPoint Dimensions = OutMetadata.GetLevelDimensions(Field.Level);
			HashByte(OutLogicalFieldHash, static_cast<uint8>(Field.Field));
			HashByte(OutLogicalFieldHash, Field.Level);
			HashInteger(OutLogicalFieldHash, Dimensions.X);
			HashInteger(OutLogicalFieldHash, Dimensions.Y);
			if (!HashWrittenField(WriterState, Field, OutLogicalFieldHash, OutError))
			{
				return false;
			}
		}

		const auto FindInfo = [&Fields](const EArcEcotopeSiteField Field) -> const FFieldInfo*
		{
			for (const FFieldInfo& Candidate : Fields)
			{
				if (Candidate.Field == Field)
				{
					return &Candidate;
				}
			}
			return nullptr;
		};
		uint64 SiteHash = FnvOffsetBasis;
		const auto HashVector = [&SiteHash](const FVector& Value)
		{
			HashFloat(SiteHash, Value.X);
			HashFloat(SiteHash, Value.Y);
			HashFloat(SiteHash, Value.Z);
		};
		const auto HashCanonicalField = [&](const EArcEcotopeSiteField Field)
		{
			const FFieldInfo* FieldInfo = FindInfo(Field);
			if (FieldInfo == nullptr)
			{
				OutError = TEXT("ArcEcotope chunk writer canonical field list is incomplete.");
				return false;
			}
			const FIntPoint Dimensions = OutMetadata.GetLevelDimensions(FieldInfo->Level);
			HashInteger(SiteHash, static_cast<uint64>(static_cast<int64>(Dimensions.X) * Dimensions.Y));
			return HashWrittenField(WriterState, *FieldInfo, SiteHash, OutError);
		};

		HashVector(OutMetadata.LocalBounds.Min);
		HashVector(OutMetadata.LocalBounds.Max);
		HashFloat(SiteHash, OutMetadata.CellSizeCm);
		HashInteger(SiteHash, static_cast<uint32>(OutMetadata.GridDimensions.X));
		HashInteger(SiteHash, static_cast<uint32>(OutMetadata.GridDimensions.Y));
		if (!HashCanonicalField(EArcEcotopeSiteField::LocalSurfaceZ)
			|| !HashCanonicalField(EArcEcotopeSiteField::ValidSurface)
			|| !HashCanonicalField(EArcEcotopeSiteField::Slope)
			|| !HashCanonicalField(EArcEcotopeSiteField::Aspect)
			|| !HashCanonicalField(EArcEcotopeSiteField::Moisture)
			|| !HashCanonicalField(EArcEcotopeSiteField::Fertility))
		{
			return false;
		}
		HashInteger(SiteHash, static_cast<uint32>(OutMetadata.SiteSchemaVersion));
		HashByte(SiteHash, OutMetadata.MoistureLevel);
		HashByte(SiteHash, OutMetadata.FertilityLevel);
		if (!HashCanonicalField(EArcEcotopeSiteField::SunLoad)
			|| !HashCanonicalField(EArcEcotopeSiteField::WindExposure)
			|| !HashCanonicalField(EArcEcotopeSiteField::SkyViewFactor))
		{
			return false;
		}
		HashByte(SiteHash, OutMetadata.SunLoadLevel);
		HashByte(SiteHash, OutMetadata.WindExposureLevel);
		HashByte(SiteHash, OutMetadata.SkyViewFactorLevel);
		if (!HashCanonicalField(EArcEcotopeSiteField::Temperature))
		{
			return false;
		}
		HashByte(SiteHash, OutMetadata.TemperatureLevel);
		if (!HashCanonicalField(EArcEcotopeSiteField::ModificationCut))
		{
			return false;
		}
		HashByte(SiteHash, OutMetadata.ModificationCutLevel);
		// The canonical chain skips RecordValid entirely, so the terrain-derived fields follow
		// ModificationCut here; the reader's VisitField chain mirrors this exact byte order.
		if (!HashCanonicalField(EArcEcotopeSiteField::BareRock)
			|| !HashCanonicalField(EArcEcotopeSiteField::ChannelExclusion)
			|| !HashCanonicalField(EArcEcotopeSiteField::ChannelBuffer)
			|| !HashCanonicalField(EArcEcotopeSiteField::EstablishmentWear)
			|| !HashCanonicalField(EArcEcotopeSiteField::SubstrateMobility))
		{
			return false;
		}
		HashByte(SiteHash, OutMetadata.BareRockLevel);
		HashByte(SiteHash, OutMetadata.ChannelExclusionLevel);
		HashByte(SiteHash, OutMetadata.EstablishmentWearLevel);
		HashByte(SiteHash, OutMetadata.SubstrateMobilityLevel);
		OutMetadata.SiteSnapshotHash = SiteHash;
		OutMetadata.LogicalFieldHash = OutLogicalFieldHash;
		return true;
	}
}

FArcEcotopeSiteChunkBuildWriter::FArcEcotopeSiteChunkBuildWriter() = default;
FArcEcotopeSiteChunkBuildWriter::~FArcEcotopeSiteChunkBuildWriter() = default;

bool FArcEcotopeSiteChunkBuildWriter::Begin(
	const FArcEcotopeSiteMetadata& ProvisionalMetadata,
	const FArcEcotopeChunkLayout& FinalLayout,
	FString& OutError)
{
	using namespace ArcEcotopeSiteChunkStoragePrivate;
	State.Reset();
	OutError.Reset();
	if (!ValidateWriterMetadataAndLayout(ProvisionalMetadata, FinalLayout, OutError))
	{
		return false;
	}

	TUniquePtr<FArcEcotopeSiteChunkBuildWriterState> Candidate = MakeUnique<FArcEcotopeSiteChunkBuildWriterState>();
	Candidate->Metadata = ProvisionalMetadata;
	Candidate->Layout = FinalLayout;
	Candidate->Manifest.FormatVersion = FArcEcotopeSiteChunkStorage::CurrentFormatVersion;
	Candidate->Manifest.CoreBaseCellSpan = FinalLayout.GetCoreBaseCellSpan();
	Candidate->Manifest.ChunkGridDimensions = FinalLayout.GetChunkGridDimensions();
	Candidate->Manifest.Chunks.Reserve(FinalLayout.GetChunkCount());
	Candidate->MutablePayloads.Reserve(FinalLayout.GetChunkCount());
	Candidate->WrittenFields.Reserve(FinalLayout.GetChunkCount());
	for (int32 ChunkIndex = 0; ChunkIndex < FinalLayout.GetChunkCount(); ++ChunkIndex)
	{
		FArcEcotopeChunkRect Chunk;
		FArcEcotopeSiteChunkManifestEntry Entry;
		if (!FinalLayout.GetChunk(ChunkIndex, Chunk)
			|| !MakeExpectedEntry(ProvisionalMetadata, Chunk, ChunkIndex, Entry, OutError)
			|| Entry.PayloadSize < FArcEcotopeSiteChunkStorage::PayloadHeaderBytes
			|| Entry.PayloadSize > MAX_int32)
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("ArcEcotope chunk writer payload is not representable.");
			}
			return false;
		}
		FUniqueBuffer Payload = FUniqueBuffer::Alloc(Entry.PayloadSize);
		FMemory::Memzero(Payload.GetData(), Entry.PayloadSize);
		Candidate->Manifest.Chunks.Add(MoveTemp(Entry));
		Candidate->MutablePayloads.Add(MoveTemp(Payload));
		TArray<uint8>& Written = Candidate->WrittenFields.AddDefaulted_GetRef();
		Written.Init(0, Candidate->Manifest.Chunks.Last().Fields.Num());
	}
	State = MoveTemp(Candidate);
	return true;
}
bool FArcEcotopeSiteChunkStorage::ValidatePayloadBytes(
	const FArcEcotopeSiteChunkManifestEntry& Entry,
	const FSharedBuffer& Payload,
	FString& OutError)
{
	using namespace ArcEcotopeSiteChunkStoragePrivate;
	OutError.Reset();
	if (Payload.IsNull() || Payload.GetSize() != Entry.PayloadSize
		|| Payload.GetSize() < PayloadHeaderBytes)
	{
		OutError = TEXT("payload size does not match the manifest");
		return false;
	}
	const uint8* PayloadData = static_cast<const uint8*>(Payload.GetData());
	uint64 PayloadHash = FnvOffsetBasis;
	HashBytes(PayloadHash, PayloadData, Payload.GetSize());
	if (PayloadHash != Entry.PayloadHash || ReadUInt32(PayloadData, 0) != PayloadMagic
		|| ReadUInt16(PayloadData, 4) != CurrentFormatVersion
		|| ReadUInt16(PayloadData, 6) != PayloadHeaderBytes
		|| static_cast<int32>(ReadUInt32(PayloadData, 8)) != Entry.ChunkId.X
		|| static_cast<int32>(ReadUInt32(PayloadData, 12)) != Entry.ChunkId.Y
		|| ReadUInt32(PayloadData, 16) != static_cast<uint32>(Entry.Fields.Num())
		|| ReadUInt32(PayloadData, 20) != 0
		|| ReadUInt64(PayloadData, 24) != CalculateDescriptorHash(Entry))
	{
		OutError = TEXT("payload header or payload hash is invalid");
		return false;
	}
	for (const FArcEcotopeSiteFieldBlockDescriptor& Descriptor : Entry.Fields)
	{
		uint64 EndOffset = 0;
		if (!TryAdd(Descriptor.ByteOffset, Descriptor.ByteSize, EndOffset)
			|| Descriptor.ByteSize == 0 || EndOffset > Payload.GetSize())
		{
			OutError = TEXT("field byte range is outside the payload");
			return false;
		}
		uint64 DataHash = FnvOffsetBasis;
		HashBytes(DataHash, PayloadData + Descriptor.ByteOffset, Descriptor.ByteSize);
		if (DataHash != Descriptor.DataHash)
		{
			OutError = FString::Printf(
				TEXT("field %d data hash is invalid"), static_cast<int32>(Descriptor.Field));
			return false;
		}
	}
	return true;
}

bool FArcEcotopeSiteChunkBuildWriter::WriteFloatField(
	const FArcEcotopeChunkId& ChunkId,
	const EArcEcotopeSiteField Field,
	const uint8 Level,
	const FIntRect& OwnedRect,
	const TConstArrayView<float> Values,
	FString& OutError)
{
	if (!State.IsValid() || State->bFinalized)
	{
		OutError = TEXT("ArcEcotope chunk writer is not accepting writes.");
		return false;
	}
	return ArcEcotopeSiteChunkStoragePrivate::WriteOwnedField(
		*State, ChunkId, Field, EArcEcotopeSiteValueType::Float32, Level, OwnedRect,
		Values.GetData(), Values.Num(), OutError);
}

bool FArcEcotopeSiteChunkBuildWriter::WriteByteField(
	const FArcEcotopeChunkId& ChunkId,
	const EArcEcotopeSiteField Field,
	const uint8 Level,
	const FIntRect& OwnedRect,
	const TConstArrayView<uint8> Values,
	FString& OutError)
{
	if (!State.IsValid() || State->bFinalized)
	{
		OutError = TEXT("ArcEcotope chunk writer is not accepting writes.");
		return false;
	}
	return ArcEcotopeSiteChunkStoragePrivate::WriteOwnedField(
		*State, ChunkId, Field, EArcEcotopeSiteValueType::UInt8, Level, OwnedRect,
		Values.GetData(), Values.Num(), OutError);
}

bool FArcEcotopeSiteChunkBuildWriter::Finalize(
	const FArcEcotopeSiteMetadata& FinalMetadata,
	FArcEcotopeSiteMetadata& OutMetadata,
	FArcEcotopeSiteChunkManifest& OutManifest,
	TArray<FSharedBuffer>& OutPayloads,
	FString& OutError)
{
	using namespace ArcEcotopeSiteChunkStoragePrivate;
	OutError.Reset();
	if (!State.IsValid() || State->bFinalized)
	{
		OutError = TEXT("ArcEcotope chunk writer has no unfinished build to finalize.");
		return false;
	}
	if (!ValidateFinalMetadata(State->Metadata, FinalMetadata, OutError))
	{
		return false;
	}
	for (const TArray<uint8>& Written : State->WrittenFields)
	{
		for (const uint8 bWritten : Written)
		{
			if (bWritten == 0)
			{
				OutError = TEXT("ArcEcotope chunk writer cannot finalize with missing chunk-field writes.");
				return false;
			}
		}
	}
	if (!ValidateRecordValidCoverage(*State, OutError))
	{
		return false;
	}

	FArcEcotopeSiteMetadata CalculatedMetadata;
	uint64 LogicalFieldHash = 0;
	if (!CalculateFinalIdentities(*State, FinalMetadata, CalculatedMetadata, LogicalFieldHash, OutError))
	{
		return false;
	}
	State->Manifest.SourceSiteSnapshotHash = CalculatedMetadata.SiteSnapshotHash;
	State->Manifest.LogicalFieldHash = LogicalFieldHash;
	for (int32 ChunkIndex = 0; ChunkIndex < State->Manifest.Chunks.Num(); ++ChunkIndex)
	{
		FArcEcotopeSiteChunkManifestEntry& Entry = State->Manifest.Chunks[ChunkIndex];
		uint8* PayloadData = static_cast<uint8*>(State->MutablePayloads[ChunkIndex].GetData());
		WriteUInt32(PayloadData, 0, FArcEcotopeSiteChunkStorage::PayloadMagic);
		WriteUInt16(PayloadData, 4, FArcEcotopeSiteChunkStorage::CurrentFormatVersion);
		WriteUInt16(PayloadData, 6, FArcEcotopeSiteChunkStorage::PayloadHeaderBytes);
		WriteUInt32(PayloadData, 8, static_cast<uint32>(Entry.ChunkId.X));
		WriteUInt32(PayloadData, 12, static_cast<uint32>(Entry.ChunkId.Y));
		WriteUInt32(PayloadData, 16, Entry.Fields.Num());
		WriteUInt32(PayloadData, 20, 0);
		WriteUInt64(PayloadData, 24, CalculateDescriptorHash(Entry));
		Entry.PayloadHash = FnvOffsetBasis;
		HashBytes(Entry.PayloadHash, PayloadData, Entry.PayloadSize);
	}
	State->Manifest.ManifestHash = FArcEcotopeSiteChunkStorage::CalculateManifestHash(State->Manifest);

	TArray<FSharedBuffer> FinalPayloads;
	FinalPayloads.Reserve(State->MutablePayloads.Num());
	for (FUniqueBuffer& Payload : State->MutablePayloads)
	{
		FinalPayloads.Add(Payload.MoveToShared());
	}
	State->bFinalized = true;
	OutMetadata = CalculatedMetadata;
	OutManifest = MoveTemp(State->Manifest);
	OutPayloads = MoveTemp(FinalPayloads);
	return true;
}

void FArcEcotopeSiteChunkBuildWriter::GetAllocatedBytes(uint64& OutPayloadBytes, uint64& OutOverheadBytes) const
{
	OutPayloadBytes = 0;
	OutOverheadBytes = 0;
	if (!State.IsValid())
	{
		return;
	}

	OutOverheadBytes = sizeof(FArcEcotopeSiteChunkBuildWriterState)
		+ State->Manifest.Chunks.GetAllocatedSize()
		+ State->MutablePayloads.GetAllocatedSize()
		+ State->WrittenFields.GetAllocatedSize()
		+ static_cast<uint64>(State->Layout.GetChunks().Num()) * sizeof(FArcEcotopeChunkRect);
	for (const FArcEcotopeSiteChunkManifestEntry& Entry : State->Manifest.Chunks)
	{
		OutOverheadBytes += Entry.Fields.GetAllocatedSize();
	}
	for (const FUniqueBuffer& Payload : State->MutablePayloads)
	{
		OutPayloadBytes += Payload.GetSize();
	}
	for (const TArray<uint8>& Written : State->WrittenFields)
	{
		OutOverheadBytes += Written.GetAllocatedSize();
	}
}

uint64 FArcEcotopeSiteChunkStorage::CalculateManifestHash(const FArcEcotopeSiteChunkManifest& Manifest)
{
	using namespace ArcEcotopeSiteChunkStoragePrivate;
	uint64 Hash = FnvOffsetBasis;
	HashInteger(Hash, Manifest.FormatVersion);
	HashInteger(Hash, Manifest.CoreBaseCellSpan);
	HashInteger(Hash, Manifest.ChunkGridDimensions.X);
	HashInteger(Hash, Manifest.ChunkGridDimensions.Y);
	HashInteger(Hash, Manifest.SourceSiteSnapshotHash);
	HashInteger(Hash, Manifest.LogicalFieldHash);
	HashInteger(Hash, Manifest.Chunks.Num());
	for (const FArcEcotopeSiteChunkManifestEntry& Entry : Manifest.Chunks)
	{
		HashInteger(Hash, Entry.ChunkId.X);
		HashInteger(Hash, Entry.ChunkId.Y);
		HashInteger(Hash, Entry.BaseCoreMin.X);
		HashInteger(Hash, Entry.BaseCoreMin.Y);
		HashInteger(Hash, Entry.BaseCoreMax.X);
		HashInteger(Hash, Entry.BaseCoreMax.Y);
		HashInteger(Hash, Entry.FieldMask);
		HashInteger(Hash, Entry.PayloadIndex);
		HashInteger(Hash, Entry.PayloadSize);
		HashInteger(Hash, Entry.PayloadHash);
		HashInteger(Hash, Entry.Fields.Num());
		for (const FArcEcotopeSiteFieldBlockDescriptor& Field : Entry.Fields)
		{
			HashByte(Hash, static_cast<uint8>(Field.Field));
			HashByte(Hash, static_cast<uint8>(Field.ValueType));
			HashByte(Hash, Field.Level);
			HashInteger(Hash, Field.RectMin.X);
			HashInteger(Hash, Field.RectMin.Y);
			HashInteger(Hash, Field.RectMax.X);
			HashInteger(Hash, Field.RectMax.Y);
			HashInteger(Hash, Field.ByteOffset);
			HashInteger(Hash, Field.ByteSize);
			HashInteger(Hash, Field.DataHash);
		}
	}
	return Hash;
}

bool FArcEcotopeSiteChunkStorage::ValidateManifestMetadata(
	const FArcEcotopeSiteMetadata& Metadata,
	const FArcEcotopeSiteChunkManifest& Manifest,
	const TConstArrayView<int64> PayloadSizes,
	FString& OutError)
{
	using namespace ArcEcotopeSiteChunkStoragePrivate;
	OutError.Reset();
	if (Manifest.FormatVersion != CurrentFormatVersion || Manifest.CoreBaseCellSpan <= 0
		|| Manifest.SourceSiteSnapshotHash == 0 || Manifest.SourceSiteSnapshotHash != Metadata.SiteSnapshotHash
		|| Metadata.LogicalFieldHash == 0 || Manifest.LogicalFieldHash != Metadata.LogicalFieldHash
		|| Manifest.LogicalFieldHash == 0
		|| Manifest.ManifestHash == 0 || Manifest.ManifestHash != CalculateManifestHash(Manifest))
	{
		OutError = TEXT("ArcEcotope site chunk manifest header/hash does not match the site metadata.");
		return false;
	}

	FArcEcotopeChunkLayout Layout;
	if (!FArcEcotopeChunkLayout::Build(Metadata, Manifest.CoreBaseCellSpan, 0, Layout, OutError))
	{
		return false;
	}
	if (Manifest.ChunkGridDimensions != Layout.GetChunkGridDimensions()
		|| Manifest.Chunks.Num() != Layout.GetChunkCount() || PayloadSizes.Num() != Manifest.Chunks.Num())
	{
		OutError = TEXT("ArcEcotope site chunk manifest counts do not match its layout or payloads.");
		return false;
	}

	for (int32 ChunkIndex = 0; ChunkIndex < Layout.GetChunkCount(); ++ChunkIndex)
	{
		FArcEcotopeChunkRect Chunk;
		FArcEcotopeSiteChunkManifestEntry Expected;
		if (!Layout.GetChunk(ChunkIndex, Chunk)
			|| !MakeExpectedEntry(Metadata, Chunk, ChunkIndex, Expected, OutError))
		{
			return false;
		}
		const FArcEcotopeSiteChunkManifestEntry& Actual = Manifest.Chunks[ChunkIndex];
		if (Actual.ChunkId != Expected.ChunkId || Actual.BaseCoreMin != Expected.BaseCoreMin
			|| Actual.BaseCoreMax != Expected.BaseCoreMax || Actual.FieldMask != Expected.FieldMask
			|| Actual.PayloadIndex != ChunkIndex || Actual.PayloadSize != Expected.PayloadSize
			|| Actual.PayloadSize != static_cast<uint64>(PayloadSizes[ChunkIndex])
			|| Actual.PayloadHash == 0 || Actual.Fields.Num() != Expected.Fields.Num())
		{
			OutError = FString::Printf(TEXT("ArcEcotope site chunk %d metadata does not match the site source."), ChunkIndex);
			return false;
		}
		for (int32 FieldIndex = 0; FieldIndex < Actual.Fields.Num(); ++FieldIndex)
		{
			if (!DescriptorsEqual(Actual.Fields[FieldIndex], Expected.Fields[FieldIndex], false)
				|| Actual.Fields[FieldIndex].DataHash == 0)
			{
				OutError = FString::Printf(TEXT("ArcEcotope site chunk %d field %d descriptor is invalid."), ChunkIndex, FieldIndex);
				return false;
			}
		}
	}
	return true;
}
