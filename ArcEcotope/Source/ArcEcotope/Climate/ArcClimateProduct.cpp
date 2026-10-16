// Copyright Lukasz Baran. All Rights Reserved.

#include "Climate/ArcClimateProduct.h"

namespace ArcClimateProductPrivate
{
	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

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

	bool CheckedMultiply(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Left != 0 && Right > MAX_uint64 / Left)
		{
			OutValue = 0;
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool CheckedAdd(const uint64 Left, const uint64 Right, uint64& OutValue)
	{
		if (Right > MAX_uint64 - Left)
		{
			OutValue = 0;
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

	bool IsRangeWithin(const uint64 Offset, const uint64 ByteSize, const uint64 Size)
	{
		return Offset <= Size && ByteSize <= Size - Offset;
	}

	bool IsFinitePositiveUniformTransform(const FTransform& Transform)
	{
		if (Transform.ContainsNaN())
		{
			return false;
		}
		const FVector Scale = Transform.GetScale3D();
		return Scale.X > 0.0 && Scale.Y > 0.0 && Scale.Z > 0.0
			&& FMath::IsNearlyEqual(Scale.X, Scale.Y)
			&& FMath::IsNearlyEqual(Scale.X, Scale.Z);
	}

	bool IsFiniteInRange(const double Value, const double Minimum, const double Maximum)
	{
		return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
	}

	FIntRect MakeOwnedRect(
		const FIntPoint& BaseMin,
		const FIntPoint& BaseMax,
		const FIntPoint& BaseDimensions,
		const uint8 Level)
	{
		if (Level > FArcClimateProductMetadata::MaxGridLevel
			|| BaseDimensions.X <= 0 || BaseDimensions.Y <= 0
			|| BaseMin.X < 0 || BaseMin.Y < 0
			|| BaseMax.X <= BaseMin.X || BaseMax.Y <= BaseMin.Y
			|| BaseMax.X > BaseDimensions.X || BaseMax.Y > BaseDimensions.Y)
		{
			return FIntRect();
		}
		const int64 Divisor = static_cast<int64>(1) << Level;
		const int64 LevelWidth = (static_cast<int64>(BaseDimensions.X) + Divisor - 1) / Divisor;
		const int64 LevelHeight = (static_cast<int64>(BaseDimensions.Y) + Divisor - 1) / Divisor;
		if (LevelWidth <= 0 || LevelHeight <= 0
			|| LevelWidth > static_cast<int64>(MAX_int32)
			|| LevelHeight > static_cast<int64>(MAX_int32))
		{
			return FIntRect();
		}
		const int64 LevelMinX = static_cast<int64>(BaseMin.X) / Divisor;
		const int64 LevelMinY = static_cast<int64>(BaseMin.Y) / Divisor;
		const int64 LevelMaxX = FMath::Min(
			(static_cast<int64>(BaseMax.X) + Divisor - 1) / Divisor, LevelWidth);
		const int64 LevelMaxY = FMath::Min(
			(static_cast<int64>(BaseMax.Y) + Divisor - 1) / Divisor, LevelHeight);
		if (LevelMinX < 0 || LevelMinY < 0 || LevelMaxX <= LevelMinX || LevelMaxY <= LevelMinY
			|| LevelMinX > static_cast<int64>(MAX_int32) || LevelMinY > static_cast<int64>(MAX_int32)
			|| LevelMaxX > static_cast<int64>(MAX_int32) || LevelMaxY > static_cast<int64>(MAX_int32))
		{
			return FIntRect();
		}
		return FIntRect(
			FIntPoint(static_cast<int32>(LevelMinX), static_cast<int32>(LevelMinY)),
			FIntPoint(
				static_cast<int32>(LevelMaxX), static_cast<int32>(LevelMaxY)));
	}

	const FArcClimateFieldBlockDescriptor* FindDescriptor(
		const FArcClimateChunkManifestEntry& Entry,
		const EArcClimateField Field)
	{
		for (const FArcClimateFieldBlockDescriptor& Descriptor : Entry.Fields)
		{
			if (Descriptor.Field == Field)
			{
				return &Descriptor;
			}
		}
		return nullptr;
	}

	bool ReadFieldValue(
		const FArcClimateFrozenSource& Source,
		const EArcClimateField Field,
		const int32 X,
		const int32 Y,
		float& OutValue,
		FString& OutError)
	{
		const uint8 Level = Source.Metadata.GetFieldLevel(Field);
		const uint64 Divisor = static_cast<uint64>(1) << Level;
		uint64 BaseXUnsigned = 0;
		uint64 BaseYUnsigned = 0;
		if (X < 0 || Y < 0
			|| !CheckedMultiply(static_cast<uint64>(X), Divisor, BaseXUnsigned)
			|| !CheckedMultiply(static_cast<uint64>(Y), Divisor, BaseYUnsigned)
			|| BaseXUnsigned > static_cast<uint64>(MAX_int64)
			|| BaseYUnsigned > static_cast<uint64>(MAX_int64))
		{
			OutError = TEXT("Climate field sample has invalid base coordinates.");
			return false;
		}
		const int64 BaseX = static_cast<int64>(BaseXUnsigned);
		const int64 BaseY = static_cast<int64>(BaseYUnsigned);
		const int64 ChunkIdX = BaseX / static_cast<int64>(Source.Manifest.CoreBaseCellSpan);
		const int64 ChunkIdY = BaseY / static_cast<int64>(Source.Manifest.CoreBaseCellSpan);
		uint64 EntryIndexUnsigned = 0;
		if (ChunkIdX < 0 || ChunkIdY < 0
			|| ChunkIdX > MAX_int32 || ChunkIdY > MAX_int32
			|| !CheckedMultiply(
				static_cast<uint64>(ChunkIdY),
				static_cast<uint64>(Source.Manifest.ChunkGridDimensions.X),
				EntryIndexUnsigned)
			|| !CheckedAdd(
				EntryIndexUnsigned,
				static_cast<uint64>(ChunkIdX),
				EntryIndexUnsigned)
			|| EntryIndexUnsigned > static_cast<uint64>(MAX_int32))
		{
			OutError = TEXT("Climate field sample maps outside the chunk manifest.");
			return false;
		}
		const int32 EntryIndex = static_cast<int32>(EntryIndexUnsigned);
		if (!Source.Manifest.Chunks.IsValidIndex(EntryIndex))
		{
			OutError = TEXT("Climate field sample maps outside the chunk manifest.");
			return false;
		}
		const FArcClimateChunkManifestEntry& Entry = Source.Manifest.Chunks[EntryIndex];
		const FArcClimateFieldBlockDescriptor* Descriptor = FindDescriptor(Entry, Field);
		if (Descriptor == nullptr || X < Descriptor->RectMin.X || X >= Descriptor->RectMax.X
			|| Y < Descriptor->RectMin.Y || Y >= Descriptor->RectMax.Y)
		{
			OutError = TEXT("Climate field sample has no owning field block.");
			return false;
		}
		if (!Source.Payloads.IsValidIndex(Entry.PayloadIndex))
		{
			OutError = TEXT("Climate manifest references a missing payload.");
			return false;
		}
		const FSharedBuffer& Payload = Source.Payloads[Entry.PayloadIndex];
		const int64 Width = static_cast<int64>(Descriptor->RectMax.X)
			- static_cast<int64>(Descriptor->RectMin.X);
		const int64 LocalY = static_cast<int64>(Y) - static_cast<int64>(Descriptor->RectMin.Y);
		const int64 LocalX = static_cast<int64>(X) - static_cast<int64>(Descriptor->RectMin.X);
		uint64 LocalIndexUnsigned = 0;
		uint64 LocalByteOffset = 0;
		uint64 Offset = 0;
		if (Width <= 0 || LocalY < 0 || LocalX < 0
			|| !CheckedMultiply(static_cast<uint64>(LocalY), static_cast<uint64>(Width), LocalIndexUnsigned)
			|| !CheckedAdd(LocalIndexUnsigned, static_cast<uint64>(LocalX), LocalIndexUnsigned)
			|| !CheckedMultiply(LocalIndexUnsigned, sizeof(float), LocalByteOffset)
			|| !CheckedAdd(Descriptor->ByteOffset, LocalByteOffset, Offset)
			|| !IsRangeWithin(Offset, sizeof(float), Payload.GetSize()))
		{
			OutError = TEXT("Climate field sample exceeds its payload.");
			return false;
		}
		FMemory::Memcpy(&OutValue, static_cast<const uint8*>(Payload.GetData()) + Offset, sizeof(float));
		if (!FMath::IsFinite(OutValue) || OutValue < 0.0f || OutValue > 1.0f)
		{
			OutError = TEXT("Climate field sample is non-finite or outside [0, 1].");
			return false;
		}
		OutError.Reset();
		return true;
	}

}

struct FArcClimateProductBuildWriterState
{
	FArcClimateProductMetadata Metadata;
	FArcClimateChunkManifest Manifest;
	TArray<FUniqueBuffer> Payloads;
	TArray<uint8> bBlockWritten;
};

uint8 FArcClimateProductMetadata::GetFieldLevel(const EArcClimateField Field) const
{
	switch (Field)
	{
	case EArcClimateField::Temperature:
		return TemperatureLevel;
	case EArcClimateField::Humidity:
		return HumidityLevel;
	case EArcClimateField::SunLoad:
		return SunLoadLevel;
	case EArcClimateField::WindExposure:
		return WindExposureLevel;
	case EArcClimateField::SkyViewFactor:
		return SkyViewFactorLevel;
	case EArcClimateField::Moisture:
		return MoistureLevel;
	default:
		return MaxGridLevel + 1;
	}
}

FIntPoint FArcClimateProductMetadata::GetLevelDimensions(const uint8 Level) const
{
	if (Level > MaxGridLevel || GridDimensions.X <= 0 || GridDimensions.Y <= 0)
	{
		return FIntPoint::ZeroValue;
	}
	const int64 Divisor = static_cast<int64>(1) << Level;
	const int64 LevelWidth = (static_cast<int64>(GridDimensions.X) + Divisor - 1) / Divisor;
	const int64 LevelHeight = (static_cast<int64>(GridDimensions.Y) + Divisor - 1) / Divisor;
	if (LevelWidth <= 0 || LevelHeight <= 0
		|| LevelWidth > static_cast<int64>(MAX_int32)
		|| LevelHeight > static_cast<int64>(MAX_int32))
	{
		return FIntPoint::ZeroValue;
	}
	return FIntPoint(
		static_cast<int32>(LevelWidth),
		static_cast<int32>(LevelHeight));
}

bool FArcClimateProductMetadata::IsFieldValid(const EArcClimateField Field) const
{
	const uint8 FieldIndex = static_cast<uint8>(Field);
	return FieldIndex < static_cast<uint8>(EArcClimateField::Count)
		&& (FieldValidityMask & (1u << FieldIndex)) != 0;
}

bool FArcClimateProductMetadata::IsValid(FString& OutError) const
{
	if (SchemaVersion != GetCurrentSchemaVersion() || !ClimateVolumeId.IsValid())
	{
		OutError = TEXT("Climate product schema or volume identity is invalid.");
		return false;
	}
	if (!ArcClimateProductPrivate::IsFinitePositiveUniformTransform(BakeTransform)
		|| !LocalBounds.IsValid || LocalBounds.Min.ContainsNaN() || LocalBounds.Max.ContainsNaN())
	{
		OutError = TEXT("Climate product transform or local bounds are invalid.");
		return false;
	}
	const FVector BoundsSize = LocalBounds.GetSize();
	const double GridWidthRatio = BoundsSize.X / static_cast<double>(CellSizeCm);
	const double GridHeightRatio = BoundsSize.Y / static_cast<double>(CellSizeCm);
	if (!FMath::IsFinite(CellSizeCm) || CellSizeCm <= 0.0f || BoundsSize.ContainsNaN()
		|| BoundsSize.X <= 0.0 || BoundsSize.Y <= 0.0
		|| !FMath::IsFinite(GridWidthRatio) || !FMath::IsFinite(GridHeightRatio)
		|| GridWidthRatio <= 0.0 || GridHeightRatio <= 0.0
		|| GridWidthRatio > static_cast<double>(MAX_int32)
		|| GridHeightRatio > static_cast<double>(MAX_int32)
		|| GridDimensions.X <= 0 || GridDimensions.Y <= 0 || ChunkBaseCellSpan <= 0
		|| GridDimensions.X != FMath::CeilToInt64(GridWidthRatio)
		|| GridDimensions.Y != FMath::CeilToInt64(GridHeightRatio))
	{
		OutError = TEXT("Climate product grid layout is invalid.");
		return false;
	}
	uint64 LogicalCellCount = 0;
	if (!ArcClimateProductPrivate::CheckedMultiply(
			static_cast<uint64>(GridDimensions.X), static_cast<uint64>(GridDimensions.Y), LogicalCellCount)
		|| LogicalCellCount > FArcClimateProductStorage::MaxLogicalCells)
	{
		OutError = TEXT("Climate product exceeds the supported logical cell count.");
		return false;
	}
	for (uint8 FieldIndex = 0; FieldIndex < static_cast<uint8>(EArcClimateField::Count); ++FieldIndex)
	{
		if (GetFieldLevel(static_cast<EArcClimateField>(FieldIndex)) > MaxGridLevel)
		{
			OutError = TEXT("Climate product contains an unsupported native field level.");
			return false;
		}
	}
	if ((FieldValidityMask & ~AllFieldValidityMask) != 0
		|| !IsFieldValid(EArcClimateField::Temperature)
		|| !IsFieldValid(EArcClimateField::Humidity)
		|| !IsFieldValid(EArcClimateField::SunLoad)
		|| !IsFieldValid(EArcClimateField::Moisture))
	{
		OutError = TEXT("Climate product field validity mask is invalid.");
		return false;
	}
	if (!ArcClimateProductPrivate::IsFiniteInRange(RiversWeight, 0.0, 1.0)
		|| !ArcClimateProductPrivate::IsFiniteInRange(RiversGamma, 0.01, 10.0)
		|| !ArcClimateProductPrivate::IsFiniteInRange(RiversChannelThreshold, 0.01, 1.0))
	{
		OutError = TEXT("Climate product Ecotope coupling settings are invalid.");
		return false;
	}
	OutError.Reset();
	return true;
}

FArcClimateProductBuildWriter::FArcClimateProductBuildWriter() = default;
FArcClimateProductBuildWriter::~FArcClimateProductBuildWriter() = default;

bool FArcClimateProductBuildWriter::Begin(
	const FArcClimateProductMetadata& ProvisionalMetadata,
	const int32 CoreBaseCellSpan,
	FString& OutError)
{
	State.Reset();
	if (!ProvisionalMetadata.IsValid(OutError) || CoreBaseCellSpan <= 0)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Climate product chunk span must be positive.");
		}
		return false;
	}
	uint8 MaximumLevel = 0;
	for (uint8 FieldIndex = 0; FieldIndex < static_cast<uint8>(EArcClimateField::Count); ++FieldIndex)
	{
		MaximumLevel = FMath::Max(
			MaximumLevel,
			ProvisionalMetadata.GetFieldLevel(static_cast<EArcClimateField>(FieldIndex)));
	}
	const int32 MaximumDivisor = 1 << MaximumLevel;
	if (CoreBaseCellSpan % MaximumDivisor != 0)
	{
		OutError = FString::Printf(
			TEXT("Climate chunk span must be divisible by the maximum native-level divisor %d."),
			MaximumDivisor);
		return false;
	}

	if (ProvisionalMetadata.ChunkBaseCellSpan != CoreBaseCellSpan)
	{
		OutError = TEXT("Climate metadata and staged writer disagree on ChunkBaseCellSpan.");
		return false;
	}
	const int64 ChunkGridWidth = (
		static_cast<int64>(ProvisionalMetadata.GridDimensions.X) + CoreBaseCellSpan - 1) / CoreBaseCellSpan;
	const int64 ChunkGridHeight = (
		static_cast<int64>(ProvisionalMetadata.GridDimensions.Y) + CoreBaseCellSpan - 1) / CoreBaseCellSpan;
	uint64 ChunkCount64 = 0;
	if (ChunkGridWidth <= 0 || ChunkGridHeight <= 0 || ChunkGridWidth > MAX_int32
		|| ChunkGridHeight > MAX_int32
		|| !ArcClimateProductPrivate::CheckedMultiply(
			static_cast<uint64>(ChunkGridWidth), static_cast<uint64>(ChunkGridHeight), ChunkCount64)
		|| ChunkCount64 > FArcClimateProductStorage::MaxClimateChunkCount)
	{
		OutError = FString::Printf(
			TEXT("Climate product requires %lld chunks; the supported limit is %d."),
			static_cast<int64>(ChunkCount64), FArcClimateProductStorage::MaxClimateChunkCount);
		return false;
	}

	TUniquePtr<FArcClimateProductBuildWriterState> Candidate = MakeUnique<FArcClimateProductBuildWriterState>();
	Candidate->Metadata = ProvisionalMetadata;
	Candidate->Manifest.FormatVersion = FArcClimateProductStorage::CurrentFormatVersion;
	Candidate->Manifest.CoreBaseCellSpan = CoreBaseCellSpan;
	Candidate->Manifest.ChunkGridDimensions = FIntPoint(
		static_cast<int32>(ChunkGridWidth), static_cast<int32>(ChunkGridHeight));

	const int32 ChunkCount = static_cast<int32>(ChunkCount64);
	Candidate->Manifest.Chunks.Reserve(ChunkCount);
	Candidate->Payloads.Reserve(ChunkCount);
	uint64 TotalPayloadBytes = 0;
	for (int32 ChunkY = 0; ChunkY < Candidate->Manifest.ChunkGridDimensions.Y; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < Candidate->Manifest.ChunkGridDimensions.X; ++ChunkX)
		{
			FArcClimateChunkManifestEntry& Entry = Candidate->Manifest.Chunks.AddDefaulted_GetRef();
			Entry.ChunkId = FIntPoint(ChunkX, ChunkY);
			uint64 BaseCoreMinXUnsigned = 0;
			uint64 BaseCoreMinYUnsigned = 0;
			if (!ArcClimateProductPrivate::CheckedMultiply(
					static_cast<uint64>(ChunkX), static_cast<uint64>(CoreBaseCellSpan), BaseCoreMinXUnsigned)
				|| !ArcClimateProductPrivate::CheckedMultiply(
					static_cast<uint64>(ChunkY), static_cast<uint64>(CoreBaseCellSpan), BaseCoreMinYUnsigned)
				|| BaseCoreMinXUnsigned > static_cast<uint64>(MAX_int32)
				|| BaseCoreMinYUnsigned > static_cast<uint64>(MAX_int32))
			{
				OutError = TEXT("Climate chunk base coordinates exceed the supported range.");
				return false;
			}
			const int64 BaseCoreMinX = static_cast<int64>(BaseCoreMinXUnsigned);
			const int64 BaseCoreMinY = static_cast<int64>(BaseCoreMinYUnsigned);
			uint64 BaseCoreMaxXUnsigned = 0;
			uint64 BaseCoreMaxYUnsigned = 0;
			if (!ArcClimateProductPrivate::CheckedAdd(
					BaseCoreMinXUnsigned, static_cast<uint64>(CoreBaseCellSpan), BaseCoreMaxXUnsigned)
				|| !ArcClimateProductPrivate::CheckedAdd(
					BaseCoreMinYUnsigned, static_cast<uint64>(CoreBaseCellSpan), BaseCoreMaxYUnsigned))
			{
				OutError = TEXT("Climate chunk base coordinates exceed the supported range.");
				return false;
			}
			const int64 BaseCoreMaxX = static_cast<int64>(BaseCoreMaxXUnsigned);
			const int64 BaseCoreMaxY = static_cast<int64>(BaseCoreMaxYUnsigned);
			Entry.BaseCoreMin = FIntPoint(static_cast<int32>(BaseCoreMinX), static_cast<int32>(BaseCoreMinY));
			Entry.BaseCoreMax = FIntPoint(
				static_cast<int32>(FMath::Min(BaseCoreMaxX, static_cast<int64>(ProvisionalMetadata.GridDimensions.X))),
				static_cast<int32>(FMath::Min(BaseCoreMaxY, static_cast<int64>(ProvisionalMetadata.GridDimensions.Y))));
			Entry.PayloadIndex = Candidate->Manifest.Chunks.Num() - 1;
			for (uint8 FieldIndex = 0; FieldIndex < static_cast<uint8>(EArcClimateField::Count); ++FieldIndex)
			{
				FArcClimateFieldBlockDescriptor& Descriptor = Entry.Fields.AddDefaulted_GetRef();
				Descriptor.Field = static_cast<EArcClimateField>(FieldIndex);
				Descriptor.Level = ProvisionalMetadata.GetFieldLevel(Descriptor.Field);
				const FIntRect OwnedRect = ArcClimateProductPrivate::MakeOwnedRect(
					Entry.BaseCoreMin,
					Entry.BaseCoreMax,
					ProvisionalMetadata.GridDimensions,
					Descriptor.Level);
				Descriptor.RectMin = OwnedRect.Min;
				Descriptor.RectMax = OwnedRect.Max;
				uint64 ValueCount = 0;
				Descriptor.ByteOffset = Entry.PayloadSize;
				if (OwnedRect.Width() <= 0 || OwnedRect.Height() <= 0
					|| !ArcClimateProductPrivate::CheckedMultiply(
						static_cast<uint64>(OwnedRect.Width()),
						static_cast<uint64>(OwnedRect.Height()), ValueCount)
					|| !ArcClimateProductPrivate::CheckedMultiply(
						ValueCount, sizeof(float), Descriptor.ByteSize)
					|| !ArcClimateProductPrivate::CheckedAdd(
						Entry.PayloadSize, Descriptor.ByteSize, Entry.PayloadSize)
					|| Entry.PayloadSize > FArcClimateProductStorage::MaxPayloadBytes)
				{
					OutError = TEXT("Climate chunk payload layout exceeds the supported allocation limit.");
					return false;
				}
			}
			if (!ArcClimateProductPrivate::CheckedAdd(
					TotalPayloadBytes, Entry.PayloadSize, TotalPayloadBytes)
				|| TotalPayloadBytes > FArcClimateProductStorage::MaxTotalPayloadBytes)
			{
				OutError = TEXT("Climate product payloads exceed the supported total byte limit.");
				return false;
			}
			Candidate->Payloads.Add(FUniqueBuffer::Alloc(Entry.PayloadSize));
			FMemory::Memzero(Candidate->Payloads.Last().GetData(), Entry.PayloadSize);
		}
	}
	uint64 BlockCount64 = 0;
	if (!ArcClimateProductPrivate::CheckedMultiply(
			ChunkCount64,
			static_cast<uint64>(EArcClimateField::Count),
			BlockCount64)
		|| BlockCount64 > static_cast<uint64>(MAX_int32))
	{
		OutError = TEXT("Climate product block count exceeds the supported range.");
		return false;
	}
	Candidate->bBlockWritten.Init(0, static_cast<int32>(BlockCount64));
	State = MoveTemp(Candidate);
	OutError.Reset();
	return true;
}

bool FArcClimateProductBuildWriter::WriteFieldBlock(
	const EArcClimateField Field,
	const FIntPoint ChunkId,
	const TConstArrayView<float> Values,
	FString& OutError)
{
	const int32 FieldIndex = static_cast<int32>(Field);
	if (!State.IsValid() || FieldIndex < 0 || FieldIndex >= static_cast<int32>(EArcClimateField::Count))
	{
		OutError = TEXT("Climate product writer is not open or the field is invalid.");
		return false;
	}
	if (ChunkId.X < 0 || ChunkId.Y < 0
		|| ChunkId.X >= State->Manifest.ChunkGridDimensions.X
		|| ChunkId.Y >= State->Manifest.ChunkGridDimensions.Y)
	{
		OutError = TEXT("Climate product writer received an invalid chunk.");
		return false;
	}
	uint64 ChunkIndexUnsigned = 0;
	uint64 BlockIndexUnsigned = 0;
	if (!ArcClimateProductPrivate::CheckedMultiply(
			static_cast<uint64>(ChunkId.Y),
			static_cast<uint64>(State->Manifest.ChunkGridDimensions.X),
			ChunkIndexUnsigned)
		|| !ArcClimateProductPrivate::CheckedAdd(
			ChunkIndexUnsigned, static_cast<uint64>(ChunkId.X), ChunkIndexUnsigned)
		|| !ArcClimateProductPrivate::CheckedMultiply(
			ChunkIndexUnsigned,
			static_cast<uint64>(EArcClimateField::Count),
			BlockIndexUnsigned)
		|| !ArcClimateProductPrivate::CheckedAdd(
			BlockIndexUnsigned, static_cast<uint64>(FieldIndex), BlockIndexUnsigned)
		|| ChunkIndexUnsigned > static_cast<uint64>(MAX_int32)
		|| BlockIndexUnsigned > static_cast<uint64>(MAX_int32)
		|| BlockIndexUnsigned >= static_cast<uint64>(State->bBlockWritten.Num()))
	{
		OutError = TEXT("Climate product writer received an invalid chunk block index.");
		return false;
	}
	const int32 BlockIndex = static_cast<int32>(BlockIndexUnsigned);
	if (State->bBlockWritten[BlockIndex] != 0)
	{
		OutError = TEXT("Climate product chunk field block was written more than once.");
		return false;
	}
	const int32 ChunkIndex = static_cast<int32>(ChunkIndexUnsigned);
	if (!State->Manifest.Chunks.IsValidIndex(ChunkIndex))
	{
		OutError = TEXT("Climate product writer received an invalid chunk index.");
		return false;
	}
	FArcClimateChunkManifestEntry& Entry = State->Manifest.Chunks[ChunkIndex];
	FArcClimateFieldBlockDescriptor* Descriptor = nullptr;
	for (FArcClimateFieldBlockDescriptor& Candidate : Entry.Fields)
	{
		if (Candidate.Field == Field)
		{
			Descriptor = &Candidate;
			break;
		}
	}
	if (Descriptor == nullptr)
	{
		OutError = TEXT("Climate product chunk has no descriptor for the requested field.");
		return false;
	}
	const int64 Width = static_cast<int64>(Descriptor->RectMax.X)
		- static_cast<int64>(Descriptor->RectMin.X);
	const int64 Height = static_cast<int64>(Descriptor->RectMax.Y)
		- static_cast<int64>(Descriptor->RectMin.Y);
	uint64 ExpectedValues = 0;
	uint64 ExpectedBytes = 0;
	uint64 SourceBytes = 0;
	if (Width <= 0 || Height <= 0
		|| !ArcClimateProductPrivate::CheckedMultiply(
			static_cast<uint64>(Width), static_cast<uint64>(Height), ExpectedValues)
		|| !ArcClimateProductPrivate::CheckedMultiply(ExpectedValues, sizeof(float), ExpectedBytes)
		|| !ArcClimateProductPrivate::CheckedMultiply(
			static_cast<uint64>(Values.Num()), sizeof(float), SourceBytes)
		|| ExpectedBytes > FArcClimateProductStorage::MaxPayloadBytes
		|| Descriptor->ByteSize != ExpectedBytes
		|| SourceBytes != ExpectedBytes
		|| static_cast<uint64>(Values.Num()) != ExpectedValues)
	{
		OutError = FString::Printf(
			TEXT("Climate product chunk field block has %d values; expected %llu."), Values.Num(), ExpectedValues);
		return false;
	}
	for (const float Value : Values)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 1.0f)
		{
			OutError = TEXT("Climate product chunk field block contains a non-finite value or a value outside [0, 1].");
			return false;
		}
	}
	if (!State->Payloads.IsValidIndex(Entry.PayloadIndex)
		|| !ArcClimateProductPrivate::IsRangeWithin(
			Descriptor->ByteOffset,
			Descriptor->ByteSize,
			State->Payloads[Entry.PayloadIndex].GetSize()))
	{
		OutError = TEXT("Climate product chunk field block exceeds its staged payload.");
		return false;
	}
	FMemory::Memcpy(
		static_cast<uint8*>(State->Payloads[Entry.PayloadIndex].GetData()) + Descriptor->ByteOffset,
		Values.GetData(),
		Descriptor->ByteSize);
	Descriptor->DataHash = ArcClimateProductPrivate::FnvOffsetBasis;
	ArcClimateProductPrivate::HashBytes(
		Descriptor->DataHash,
		static_cast<const uint8*>(State->Payloads[Entry.PayloadIndex].GetData()) + Descriptor->ByteOffset,
		Descriptor->ByteSize);
	State->bBlockWritten[BlockIndex] = 1;
	OutError.Reset();
	return true;
}

bool FArcClimateProductBuildWriter::Finalize(
	FArcClimateProductMetadata& OutMetadata,
	FArcClimateChunkManifest& OutManifest,
	TArray<FSharedBuffer>& OutPayloads,
	FString& OutError)
{
	if (!State.IsValid())
	{
		OutError = TEXT("Climate product writer is not open.");
		return false;
	}
	for (const uint8 bWritten : State->bBlockWritten)
	{
		if (bWritten == 0)
		{
			OutError = TEXT("Climate product cannot finalize before all chunk field blocks are written.");
			return false;
		}
	}

	for (FArcClimateChunkManifestEntry& Entry : State->Manifest.Chunks)
	{
		Entry.PayloadHash = ArcClimateProductPrivate::FnvOffsetBasis;
		ArcClimateProductPrivate::HashBytes(
			Entry.PayloadHash,
			State->Payloads[Entry.PayloadIndex].GetData(),
			State->Payloads[Entry.PayloadIndex].GetSize());
	}
	TArray<FSharedBuffer> FinalPayloads;
	FinalPayloads.Reserve(State->Payloads.Num());
	for (FUniqueBuffer& Payload : State->Payloads)
	{
		FinalPayloads.Add(Payload.MoveToShared());
	}
	State->Manifest.ManifestHash = FArcClimateProductStorage::CalculateManifestHash(State->Manifest);
	TArray<int64> PayloadSizes;
	PayloadSizes.Reserve(FinalPayloads.Num());
	for (const FSharedBuffer& Payload : FinalPayloads)
	{
		PayloadSizes.Add(static_cast<int64>(Payload.GetSize()));
	}
	if (!FArcClimateProductStorage::ValidateManifestMetadata(
			State->Metadata, State->Manifest, PayloadSizes, OutError))
	{
		return false;
	}
	for (int32 PayloadIndex = 0; PayloadIndex < FinalPayloads.Num(); ++PayloadIndex)
	{
		if (!FArcClimateProductStorage::ValidatePayloadBytes(
				State->Manifest.Chunks[PayloadIndex], FinalPayloads[PayloadIndex], OutError))
		{
			return false;
		}
	}
	OutMetadata = MoveTemp(State->Metadata);
	OutManifest = MoveTemp(State->Manifest);
	OutPayloads = MoveTemp(FinalPayloads);
	State.Reset();
	OutError.Reset();
	return true;
}

uint64 FArcClimateProductStorage::CalculateManifestHash(const FArcClimateChunkManifest& Manifest)
{
	uint64 Hash = ArcClimateProductPrivate::FnvOffsetBasis;
	ArcClimateProductPrivate::HashInteger(Hash, Manifest.FormatVersion);
	ArcClimateProductPrivate::HashInteger(Hash, Manifest.CoreBaseCellSpan);
	ArcClimateProductPrivate::HashInteger(Hash, Manifest.ChunkGridDimensions.X);
	ArcClimateProductPrivate::HashInteger(Hash, Manifest.ChunkGridDimensions.Y);
	ArcClimateProductPrivate::HashInteger(Hash, Manifest.Chunks.Num());
	for (const FArcClimateChunkManifestEntry& Entry : Manifest.Chunks)
	{
		ArcClimateProductPrivate::HashInteger(Hash, Entry.ChunkId.X);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.ChunkId.Y);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.BaseCoreMin.X);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.BaseCoreMin.Y);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.BaseCoreMax.X);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.BaseCoreMax.Y);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.PayloadIndex);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.PayloadSize);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.PayloadHash);
		ArcClimateProductPrivate::HashInteger(Hash, Entry.Fields.Num());
		for (const FArcClimateFieldBlockDescriptor& Descriptor : Entry.Fields)
		{
			ArcClimateProductPrivate::HashByte(Hash, static_cast<uint8>(Descriptor.Field));
			ArcClimateProductPrivate::HashByte(Hash, Descriptor.Level);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.RectMin.X);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.RectMin.Y);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.RectMax.X);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.RectMax.Y);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.ByteOffset);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.ByteSize);
			ArcClimateProductPrivate::HashInteger(Hash, Descriptor.DataHash);
		}
	}
	return Hash;
}

bool FArcClimateProductStorage::ValidateManifestMetadata(
	const FArcClimateProductMetadata& Metadata,
	const FArcClimateChunkManifest& Manifest,
	const TConstArrayView<int64> PayloadSizes,
	FString& OutError)
{
	if (!Metadata.IsValid(OutError))
	{
		return false;
	}
	uint64 TotalPayloadBytes = 0;
	for (const int64 PayloadSize : PayloadSizes)
	{
		if (PayloadSize <= 0
			|| static_cast<uint64>(PayloadSize) > MaxPayloadBytes
			|| !ArcClimateProductPrivate::CheckedAdd(
				TotalPayloadBytes, static_cast<uint64>(PayloadSize), TotalPayloadBytes)
			|| TotalPayloadBytes > MaxTotalPayloadBytes)
		{
			OutError = TEXT("Climate payload sizes exceed the supported allocation limits.");
			return false;
		}
	}
	if (Manifest.FormatVersion != CurrentFormatVersion || Manifest.CoreBaseCellSpan <= 0
		|| Manifest.CoreBaseCellSpan != Metadata.ChunkBaseCellSpan
		|| Manifest.ChunkGridDimensions.X <= 0 || Manifest.ChunkGridDimensions.Y <= 0
		|| Manifest.Chunks.Num() > MaxClimateChunkCount
		|| PayloadSizes.Num() > MaxClimateChunkCount)
	{
		OutError = TEXT("Climate chunk manifest header is invalid.");
		return false;
	}
	uint8 MaximumLevel = 0;
	for (uint8 FieldIndex = 0; FieldIndex < static_cast<uint8>(EArcClimateField::Count); ++FieldIndex)
	{
		MaximumLevel = FMath::Max(MaximumLevel, Metadata.GetFieldLevel(static_cast<EArcClimateField>(FieldIndex)));
	}
	if (Manifest.CoreBaseCellSpan % (1 << MaximumLevel) != 0)
	{
		OutError = TEXT("Climate chunk span is not aligned to every native field level.");
		return false;
	}
	const int64 ExpectedChunkWidth = (
		static_cast<int64>(Metadata.GridDimensions.X) + Manifest.CoreBaseCellSpan - 1)
		/ Manifest.CoreBaseCellSpan;
	const int64 ExpectedChunkHeight = (
		static_cast<int64>(Metadata.GridDimensions.Y) + Manifest.CoreBaseCellSpan - 1)
		/ Manifest.CoreBaseCellSpan;
	if (ExpectedChunkWidth <= 0 || ExpectedChunkHeight <= 0
		|| ExpectedChunkWidth > MAX_int32 || ExpectedChunkHeight > MAX_int32)
	{
		OutError = TEXT("Climate chunk dimensions exceed the supported range.");
		return false;
	}
	const FIntPoint ExpectedChunkDimensions(
		static_cast<int32>(ExpectedChunkWidth), static_cast<int32>(ExpectedChunkHeight));
	uint64 ExpectedChunkCount64 = 0;
	if (ExpectedChunkDimensions.X <= 0 || ExpectedChunkDimensions.Y <= 0
		|| !ArcClimateProductPrivate::CheckedMultiply(
			static_cast<uint64>(ExpectedChunkDimensions.X),
			static_cast<uint64>(ExpectedChunkDimensions.Y), ExpectedChunkCount64)
		|| ExpectedChunkCount64 > MaxClimateChunkCount)
	{
		OutError = TEXT("Climate chunk manifest exceeds the supported chunk count.");
		return false;
	}
	const int32 ExpectedChunkCount = static_cast<int32>(ExpectedChunkCount64);
	if (Manifest.ChunkGridDimensions != ExpectedChunkDimensions || Manifest.Chunks.Num() != ExpectedChunkCount
		|| PayloadSizes.Num() != ExpectedChunkCount)
	{
		OutError = TEXT("Climate chunk manifest dimensions or payload count are invalid.");
		return false;
	}
	for (int32 EntryIndex = 0; EntryIndex < Manifest.Chunks.Num(); ++EntryIndex)
	{
		const FArcClimateChunkManifestEntry& Entry = Manifest.Chunks[EntryIndex];
		const FIntPoint ExpectedChunkId(EntryIndex % ExpectedChunkDimensions.X, EntryIndex / ExpectedChunkDimensions.X);
		uint64 ExpectedBaseMinXUnsigned = 0;
		uint64 ExpectedBaseMinYUnsigned = 0;
		if (!ArcClimateProductPrivate::CheckedMultiply(
				static_cast<uint64>(ExpectedChunkId.X),
				static_cast<uint64>(Manifest.CoreBaseCellSpan),
				ExpectedBaseMinXUnsigned)
			|| !ArcClimateProductPrivate::CheckedMultiply(
				static_cast<uint64>(ExpectedChunkId.Y),
				static_cast<uint64>(Manifest.CoreBaseCellSpan),
				ExpectedBaseMinYUnsigned)
			|| ExpectedBaseMinXUnsigned > static_cast<uint64>(MAX_int32)
			|| ExpectedBaseMinYUnsigned > static_cast<uint64>(MAX_int32))
		{
			OutError = FString::Printf(TEXT("Climate chunk manifest entry %d has overflowing base coordinates."), EntryIndex);
			return false;
		}
		const int64 ExpectedBaseMinX = static_cast<int64>(ExpectedBaseMinXUnsigned);
		const int64 ExpectedBaseMinY = static_cast<int64>(ExpectedBaseMinYUnsigned);
		uint64 ExpectedBaseMaxXUnsigned = 0;
		uint64 ExpectedBaseMaxYUnsigned = 0;
		if (!ArcClimateProductPrivate::CheckedAdd(
				ExpectedBaseMinXUnsigned,
				static_cast<uint64>(Manifest.CoreBaseCellSpan),
				ExpectedBaseMaxXUnsigned)
			|| !ArcClimateProductPrivate::CheckedAdd(
				ExpectedBaseMinYUnsigned,
				static_cast<uint64>(Manifest.CoreBaseCellSpan),
				ExpectedBaseMaxYUnsigned))
		{
			OutError = FString::Printf(TEXT("Climate chunk manifest entry %d has overflowing base coordinates."), EntryIndex);
			return false;
		}
		const FIntPoint ExpectedBaseMin(
			static_cast<int32>(ExpectedBaseMinX), static_cast<int32>(ExpectedBaseMinY));
		const FIntPoint ExpectedBaseMax(
			static_cast<int32>(FMath::Min(
				static_cast<int64>(ExpectedBaseMaxXUnsigned),
				static_cast<int64>(Metadata.GridDimensions.X))),
			static_cast<int32>(FMath::Min(
				static_cast<int64>(ExpectedBaseMaxYUnsigned),
				static_cast<int64>(Metadata.GridDimensions.Y))));
		if (Entry.ChunkId != ExpectedChunkId || Entry.BaseCoreMin != ExpectedBaseMin
			|| Entry.BaseCoreMax != ExpectedBaseMax || Entry.PayloadIndex != EntryIndex
			|| Entry.PayloadSize == 0 || Entry.PayloadSize > MaxPayloadBytes || Entry.PayloadHash == 0
			|| PayloadSizes[EntryIndex] < 0
			|| Entry.PayloadSize != static_cast<uint64>(PayloadSizes[EntryIndex])
			|| Entry.Fields.Num() != static_cast<int32>(EArcClimateField::Count))
		{
			OutError = FString::Printf(TEXT("Climate chunk manifest entry %d is invalid."), EntryIndex);
			return false;
		}
		uint64 ExpectedOffset = 0;
		for (uint8 FieldIndex = 0; FieldIndex < static_cast<uint8>(EArcClimateField::Count); ++FieldIndex)
		{
			const FArcClimateFieldBlockDescriptor& Descriptor = Entry.Fields[FieldIndex];
			const EArcClimateField ExpectedField = static_cast<EArcClimateField>(FieldIndex);
			const uint8 ExpectedLevel = Metadata.GetFieldLevel(ExpectedField);
			const FIntRect ExpectedRect = ArcClimateProductPrivate::MakeOwnedRect(
				ExpectedBaseMin, ExpectedBaseMax, Metadata.GridDimensions, ExpectedLevel);
			uint64 ExpectedValueCount = 0;
			uint64 ExpectedSize = 0;
			if (ExpectedRect.Width() <= 0 || ExpectedRect.Height() <= 0
				|| !ArcClimateProductPrivate::CheckedMultiply(
					static_cast<uint64>(ExpectedRect.Width()),
					static_cast<uint64>(ExpectedRect.Height()), ExpectedValueCount)
				|| !ArcClimateProductPrivate::CheckedMultiply(
					ExpectedValueCount, sizeof(float), ExpectedSize)
				|| Descriptor.Field != ExpectedField || Descriptor.Level != ExpectedLevel
				|| Descriptor.RectMin != ExpectedRect.Min || Descriptor.RectMax != ExpectedRect.Max
				|| Descriptor.ByteOffset != ExpectedOffset || Descriptor.ByteSize != ExpectedSize
				|| Descriptor.DataHash == 0)
			{
				OutError = FString::Printf(
					TEXT("Climate chunk %d field descriptor %d is invalid."), EntryIndex, FieldIndex);
				return false;
			}
			if (!ArcClimateProductPrivate::CheckedAdd(ExpectedOffset, ExpectedSize, ExpectedOffset))
			{
				OutError = FString::Printf(
					TEXT("Climate chunk %d payload layout overflows."), EntryIndex);
				return false;
			}
		}
		if (ExpectedOffset != Entry.PayloadSize)
		{
			OutError = FString::Printf(TEXT("Climate chunk %d payload layout is not tightly packed."), EntryIndex);
			return false;
		}
	}
	if (Manifest.ManifestHash == 0 || Manifest.ManifestHash != CalculateManifestHash(Manifest))
	{
		OutError = TEXT("Climate manifest hash does not match its canonical contents.");
		return false;
	}
	OutError.Reset();
	return true;
}

bool FArcClimateProductStorage::ValidatePayloadBytes(
	const FArcClimateChunkManifestEntry& Entry,
	const FSharedBuffer& Payload,
	FString& OutError)
{
	if (Payload.GetSize() != Entry.PayloadSize
		|| Entry.PayloadSize == 0 || Entry.PayloadSize > MaxPayloadBytes)
	{
		OutError = TEXT("Climate payload size does not match its manifest entry.");
		return false;
	}
	uint64 PayloadHash = ArcClimateProductPrivate::FnvOffsetBasis;
	ArcClimateProductPrivate::HashBytes(PayloadHash, Payload.GetData(), Payload.GetSize());
	if (PayloadHash != Entry.PayloadHash)
	{
		OutError = TEXT("Climate payload hash does not match its manifest entry.");
		return false;
	}
	for (const FArcClimateFieldBlockDescriptor& Descriptor : Entry.Fields)
	{
		if (!ArcClimateProductPrivate::IsRangeWithin(
				Descriptor.ByteOffset, Descriptor.ByteSize, Payload.GetSize()))
		{
			OutError = TEXT("Climate payload field block exceeds its payload.");
			return false;
		}
		uint64 DataHash = ArcClimateProductPrivate::FnvOffsetBasis;
		ArcClimateProductPrivate::HashBytes(
			DataHash,
			static_cast<const uint8*>(Payload.GetData()) + Descriptor.ByteOffset,
			Descriptor.ByteSize);
		if (DataHash != Descriptor.DataHash)
		{
			OutError = TEXT("Climate payload field hash does not match its descriptor.");
			return false;
		}
	}
	OutError.Reset();
	return true;
}

bool FArcClimateProductReader::Open(FArcClimateFrozenSourcePtr InSource, FString& OutError)
{
	Close();
	if (!InSource.IsValid())
	{
		OutError = TEXT("Climate reader received no frozen source.");
		return false;
	}
	if (InSource->Payloads.Num() > FArcClimateProductStorage::MaxClimateChunkCount)
	{
		OutError = TEXT("Climate reader payload count exceeds the supported limit.");
		return false;
	}
	TArray<int64> PayloadSizes;
	PayloadSizes.Reserve(InSource->Payloads.Num());
	uint64 TotalPayloadBytes = 0;
	for (const FSharedBuffer& Payload : InSource->Payloads)
	{
		if (Payload.GetSize() == 0
			|| Payload.GetSize() > FArcClimateProductStorage::MaxPayloadBytes
			|| !ArcClimateProductPrivate::CheckedAdd(
				TotalPayloadBytes, Payload.GetSize(), TotalPayloadBytes)
			|| TotalPayloadBytes > FArcClimateProductStorage::MaxTotalPayloadBytes)
		{
			OutError = TEXT("Climate reader payload sizes exceed the supported limits.");
			return false;
		}
		PayloadSizes.Add(static_cast<int64>(Payload.GetSize()));
	}
	if (!FArcClimateProductStorage::ValidateManifestMetadata(
			InSource->Metadata, InSource->Manifest, PayloadSizes, OutError))
	{
		return false;
	}
	for (int32 PayloadIndex = 0; PayloadIndex < InSource->Payloads.Num(); ++PayloadIndex)
	{
		if (!FArcClimateProductStorage::ValidatePayloadBytes(
				InSource->Manifest.Chunks[PayloadIndex], InSource->Payloads[PayloadIndex], OutError))
		{
			return false;
		}
	}
	Source = MoveTemp(InSource);
	if (!ValidateAll(OutError))
	{
		Close();
		return false;
	}
	OutError.Reset();
	return true;
}

void FArcClimateProductReader::Close()
{
	Source.Reset();
}

bool FArcClimateProductReader::SampleAtWorldPosition(
	const FVector& WorldPosition,
	const EArcClimateField Field,
	float& OutValue,
	bool& bOutFieldValid,
	FString& OutError) const
{
	bOutFieldValid = false;
	if (!Source.IsValid() || static_cast<uint8>(Field) >= static_cast<uint8>(EArcClimateField::Count))
	{
		OutError = TEXT("Climate reader is closed or the requested field is invalid.");
		return false;
	}
	const FVector LocalPosition = Source->Metadata.BakeTransform.InverseTransformPosition(WorldPosition);
	const FBox& Bounds = Source->Metadata.LocalBounds;
	if (LocalPosition.X < Bounds.Min.X || LocalPosition.Y < Bounds.Min.Y
		|| LocalPosition.X >= Bounds.Max.X || LocalPosition.Y >= Bounds.Max.Y)
	{
		OutError = TEXT("World position is outside the half-open climate product footprint.");
		return false;
	}
	const uint8 Level = Source->Metadata.GetFieldLevel(Field);
	const double NativeCellSize = static_cast<double>(Source->Metadata.CellSizeCm) * (1 << Level);
	const FIntPoint Dimensions = Source->Metadata.GetLevelDimensions(Level);
	const int32 X = FMath::Clamp(FMath::FloorToInt((LocalPosition.X - Bounds.Min.X) / NativeCellSize), 0, Dimensions.X - 1);
	const int32 Y = FMath::Clamp(FMath::FloorToInt((LocalPosition.Y - Bounds.Min.Y) / NativeCellSize), 0, Dimensions.Y - 1);
	if (!ArcClimateProductPrivate::ReadFieldValue(*Source, Field, X, Y, OutValue, OutError))
	{
		return false;
	}
	bOutFieldValid = Source->Metadata.IsFieldValid(Field);
	return true;
}

bool FArcClimateProductReader::ValidateAll(FString& OutError) const
{
	if (!Source.IsValid())
	{
		OutError = TEXT("Climate reader is closed.");
		return false;
	}
	for (uint8 FieldIndex = 0; FieldIndex < static_cast<uint8>(EArcClimateField::Count); ++FieldIndex)
	{
		const EArcClimateField Field = static_cast<EArcClimateField>(FieldIndex);
		const FIntPoint Dimensions = Source->Metadata.GetLevelDimensions(Source->Metadata.GetFieldLevel(Field));
		for (int32 Y = 0; Y < Dimensions.Y; ++Y)
		{
			for (int32 X = 0; X < Dimensions.X; ++X)
			{
				float Value = 0.0f;
				if (!ArcClimateProductPrivate::ReadFieldValue(*Source, Field, X, Y, Value, OutError))
				{
					return false;
				}
			}
		}
	}
	OutError.Reset();
	return true;
}
