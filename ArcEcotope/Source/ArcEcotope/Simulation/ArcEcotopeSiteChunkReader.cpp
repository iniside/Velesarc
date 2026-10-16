// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcEcotopeSiteChunkReader.h"

#include "ArcEcotopeModule.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Simulation/ArcForestGrowthEngine.h"

namespace
{
	bool TryAddReaderBytes(const int64 Left, const int64 Right, int64& OutValue)
	{
		if (Left < 0 || Right < 0 || Left > MAX_int64 - Right)
		{
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

}

#if WITH_EDITORONLY_DATA
struct FArcEcotopeSitePayloadLoadTicket
{
	FArcEcotopeSitePayloadLoadTicket()
		: Event(FPlatformProcess::GetSynchEventFromPool(true))
	{
	}

	~FArcEcotopeSitePayloadLoadTicket()
	{
		FPlatformProcess::ReturnSynchEventToPool(Event);
	}

	FEvent* Event = nullptr;
	bool bSucceeded = false;
	FString Error;
};

enum class EArcEcotopeSiteCacheEntryState : uint8
{
	Empty,
	Loading,
	Ready,
};

struct FArcEcotopeSitePayloadCacheEntry
{
	EArcEcotopeSiteCacheEntryState State = EArcEcotopeSiteCacheEntryState::Empty;
	FSharedBuffer Buffer;
	int32 PinCount = 0;
	int32 PendingPinCount = 0;
	uint64 LastUseSerial = 0;
	TSharedPtr<FArcEcotopeSitePayloadLoadTicket, ESPMode::ThreadSafe> LoadTicket;
};
#endif

struct FArcEcotopeSiteChunkReaderState
{
	FArcEcotopeFrozenSiteSourcePtr Source;
	FArcEcotopeChunkLayout Layout;
	int64 CacheBudgetBytes = 0;

#if WITH_EDITORONLY_DATA
	mutable FCriticalSection CacheMutex;
	mutable TArray<FArcEcotopeSitePayloadCacheEntry> Cache;
	mutable FArcEcotopeSiteBulkReaderStats Stats;
	mutable int64 ReservedLoadBytes = 0;
	mutable uint64 UseSerial = 0;

	void UpdatePeaksLocked() const
	{
		Stats.PeakResidentBytes = FMath::Max(Stats.PeakResidentBytes, Stats.ResidentBytes);
		Stats.PeakPinnedBytes = FMath::Max(Stats.PeakPinnedBytes, Stats.PinnedBytes);
		Stats.PeakScratchBytes = FMath::Max(Stats.PeakScratchBytes, Stats.ScratchBytes);
		int64 ResidentAndReserved = 0;
		int64 ManagedTotal = MAX_int64;
		if (TryAddReaderBytes(Stats.ResidentBytes, ReservedLoadBytes, ResidentAndReserved))
		{
			TryAddReaderBytes(ResidentAndReserved, Stats.ScratchBytes, ManagedTotal);
		}
		Stats.PeakTotalBytes = FMath::Max(Stats.PeakTotalBytes, ManagedTotal);
	}

	void ReleaseBulkLease(const TConstArrayView<int32> PayloadIndices, const int64 ScratchBytes) const
	{
		FScopeLock Lock(&CacheMutex);
		for (const int32 PayloadIndex : PayloadIndices)
		{
			if (Cache.IsValidIndex(PayloadIndex))
			{
				FArcEcotopeSitePayloadCacheEntry& Entry = Cache[PayloadIndex];
				if (Entry.State == EArcEcotopeSiteCacheEntryState::Ready && Entry.PinCount > 0)
				{
					--Entry.PinCount;
					if (Entry.PinCount == 0 && Source.IsValid())
					{
						Stats.PinnedBytes -= Source->Payloads[PayloadIndex].ExpectedSize;
					}
				}
			}
		}
		Stats.ScratchBytes = FMath::Max<int64>(0, Stats.ScratchBytes - ScratchBytes);
	}
#endif
};

struct FArcEcotopeSiteChunkLeaseData
{
	~FArcEcotopeSiteChunkLeaseData()
	{
#if WITH_EDITORONLY_DATA
		if (ReaderState.IsValid() && !PinnedPayloadIndices.IsEmpty())
		{
			ReaderState->ReleaseBulkLease(PinnedPayloadIndices, ScratchBytes);
		}
#endif
	}

	TSharedPtr<const FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe> ReaderState;
	FArcEcotopeSiteChunkView View;
#if WITH_EDITORONLY_DATA
	TArray<int32> PinnedPayloadIndices;
	FSharedBuffer Scratch;
	int64 ScratchBytes = 0;
#endif
};

namespace ArcEcotopeSiteChunkReaderPrivate
{
	constexpr uint64 FnvOffsetBasis = 14695981039346656037ull;
	constexpr uint64 FnvPrime = 1099511628211ull;

	bool TryMultiply(const int64 Left, const int64 Right, int64& OutValue)
	{
		if (Left < 0 || Right < 0 || (Left > 0 && Right > MAX_int64 / Left))
		{
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	bool TryAdd(const int64 Left, const int64 Right, int64& OutValue)
	{
		if (Left < 0 || Right < 0 || Left > MAX_int64 - Right)
		{
			return false;
		}
		OutValue = Left + Right;
		return true;
	}

	// Mirrors FArcEcotopeSiteChunkStorage's writer-side field alignment so descriptor offsets line up.
	bool TryAlign4(const uint64 Value, uint64& OutValue)
	{
		if (Value > MAX_uint64 - 3)
		{
			return false;
		}
		OutValue = (Value + 3) & ~3ull;
		return true;
	}

	void HashByte(uint64& Hash, const uint8 Value)
	{
		Hash ^= Value;
		Hash *= FnvPrime;
	}

	void HashUInt32(uint64& Hash, const uint32 Value)
	{
		for (int32 ByteIndex = 0; ByteIndex < 4; ++ByteIndex)
		{
			HashByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
		}
	}

	void HashUInt64(uint64& Hash, const uint64 Value)
	{
		for (int32 ByteIndex = 0; ByteIndex < 8; ++ByteIndex)
		{
			HashByte(Hash, static_cast<uint8>(Value >> (ByteIndex * 8)));
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
		HashUInt32(Hash, Bits);
	}

	void HashVector(uint64& Hash, const FVector& Value)
	{
		HashFloat(Hash, Value.X);
		HashFloat(Hash, Value.Y);
		HashFloat(Hash, Value.Z);
	}

#if WITH_EDITORONLY_DATA
	bool IsFiniteVector(const FVector& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	}

	bool TryCalculateGridDimension(const double BoundsSize, const double CellSize, int32& OutDimension)
	{
		const double Ratio = BoundsSize / CellSize;
		if (!FMath::IsFinite(Ratio) || Ratio <= 0.0 || Ratio > static_cast<double>(MAX_int32))
		{
			return false;
		}
		const double RoundedDimension = FMath::CeilToDouble(Ratio);
		if (!FMath::IsFinite(RoundedDimension) || RoundedDimension < 1.0
			|| RoundedDimension > static_cast<double>(MAX_int32))
		{
			return false;
		}
		OutDimension = static_cast<int32>(RoundedDimension);
		return true;
	}

	bool ValidateFrozenMetadata(const FArcEcotopeSiteMetadata& Metadata, FString& OutError)
	{
		const bool bLevelsValid = Metadata.RecordGridLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.MoistureLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.FertilityLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.SunLoadLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.WindExposureLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.SkyViewFactorLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.TemperatureLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.ModificationCutLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.BareRockLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.ChannelExclusionLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.EstablishmentWearLevel <= FArcEcotopeSiteMetadata::MaxGridLevel
			&& Metadata.SubstrateMobilityLevel <= FArcEcotopeSiteMetadata::MaxGridLevel;
		if (Metadata.SiteSchemaVersion != FArcEcotopeSiteMetadata::GetCurrentSiteSchemaVersion()
			|| !Metadata.LocalBounds.IsValid || !IsFiniteVector(Metadata.LocalBounds.Min)
			|| !IsFiniteVector(Metadata.LocalBounds.Max)
			|| Metadata.LocalBounds.Max.X <= Metadata.LocalBounds.Min.X
			|| Metadata.LocalBounds.Max.Y <= Metadata.LocalBounds.Min.Y
			|| Metadata.LocalBounds.Max.Z <= Metadata.LocalBounds.Min.Z
			|| !FMath::IsFinite(Metadata.CellSizeCm) || Metadata.CellSizeCm <= 0.0f
			|| Metadata.GridDimensions.X <= 0 || Metadata.GridDimensions.Y <= 0
			|| !bLevelsValid || Metadata.SiteSnapshotHash == 0)
		{
			OutError = TEXT("ArcEcotope frozen bulk site metadata has invalid schema or geometry.");
			return false;
		}
		int32 ExpectedWidth = 0;
		int32 ExpectedHeight = 0;
		const double BoundsWidth = static_cast<double>(Metadata.LocalBounds.Max.X)
			- static_cast<double>(Metadata.LocalBounds.Min.X);
		const double BoundsHeight = static_cast<double>(Metadata.LocalBounds.Max.Y)
			- static_cast<double>(Metadata.LocalBounds.Min.Y);
		const int64 CellCount = static_cast<int64>(Metadata.GridDimensions.X) * Metadata.GridDimensions.Y;
		if (!TryCalculateGridDimension(BoundsWidth, Metadata.CellSizeCm, ExpectedWidth)
			|| !TryCalculateGridDimension(BoundsHeight, Metadata.CellSizeCm, ExpectedHeight)
			|| Metadata.GridDimensions != FIntPoint(ExpectedWidth, ExpectedHeight)
			|| CellCount <= 0 || CellCount > MAX_int32)
		{
			OutError = TEXT("ArcEcotope frozen bulk site dimensions do not match its bounds and cell size.");
			return false;
		}
		return true;
	}
#endif

#if WITH_EDITORONLY_DATA
	struct FBulkFieldPiece
	{
		int32 PayloadIndex = INDEX_NONE;
		const FArcEcotopeSiteFieldBlockDescriptor* Descriptor = nullptr;
		FIntRect Intersection;
	};

	struct FBulkFieldPlan
	{
		EArcEcotopeSiteField Field = EArcEcotopeSiteField::LocalSurfaceZ;
		EArcEcotopeSiteValueType ValueType = EArcEcotopeSiteValueType::Float32;
		uint8 Level = 0;
		FIntRect TargetRect;
		int64 ScratchOffset = 0;
		TArray<FBulkFieldPiece> Pieces;
	};

	FIntRect IntersectRects(const FIntRect& Left, const FIntRect& Right)
	{
		return FIntRect(
			FMath::Max(Left.Min.X, Right.Min.X), FMath::Max(Left.Min.Y, Right.Min.Y),
			FMath::Min(Left.Max.X, Right.Max.X), FMath::Min(Left.Max.Y, Right.Max.Y));
	}

	const FArcEcotopeSiteFieldBlockDescriptor* FindDescriptor(
		const FArcEcotopeSiteChunkManifestEntry& Entry, const EArcEcotopeSiteField Field)
	{
		for (const FArcEcotopeSiteFieldBlockDescriptor& Descriptor : Entry.Fields)
		{
			if (Descriptor.Field == Field)
			{
				return &Descriptor;
			}
		}
		return nullptr;
	}

	bool BuildBulkFieldPlans(
		const FArcEcotopeFrozenSiteSource& Source,
		const FIntRect& BaseReadRect,
		const uint32 FieldMask,
		TArray<FBulkFieldPlan>& OutPlans,
		TArray<int32>& OutPayloadIndices,
		int64& OutScratchBytes,
		FString& OutError)
	{
		OutPlans.Reset();
		OutPayloadIndices.Reset();
		OutScratchBytes = 0;
		TSet<int32> PayloadSet;
		for (int32 FieldValue = 0; FieldValue < static_cast<int32>(EArcEcotopeSiteField::Count); ++FieldValue)
		{
			if ((FieldMask & (1u << FieldValue)) == 0)
			{
				continue;
			}
			FBulkFieldPlan& Plan = OutPlans.AddDefaulted_GetRef();
			Plan.Field = static_cast<EArcEcotopeSiteField>(FieldValue);
			Plan.ValueType = Plan.Field == EArcEcotopeSiteField::ValidSurface
				|| Plan.Field == EArcEcotopeSiteField::RecordValid
				? EArcEcotopeSiteValueType::UInt8 : EArcEcotopeSiteValueType::Float32;
			Plan.Level = Source.Metadata.GetFieldLevel(Plan.Field);
			if (!FArcEcotopeSiteChunkReader::TryMapBaseReadRectToLevel(
					BaseReadRect, Source.Metadata.GridDimensions, Plan.Level, Plan.TargetRect))
			{
				OutError = TEXT("ArcEcotope bulk reader could not map a field read rectangle.");
				return false;
			}

			const int64 Divisor = 1ll << Plan.Level;
			const int64 BaseMinX = static_cast<int64>(Plan.TargetRect.Min.X) * Divisor;
			const int64 BaseMinY = static_cast<int64>(Plan.TargetRect.Min.Y) * Divisor;
			const int64 BaseMaxX = FMath::Min<int64>(
				static_cast<int64>(Plan.TargetRect.Max.X) * Divisor, Source.Metadata.GridDimensions.X);
			const int64 BaseMaxY = FMath::Min<int64>(
				static_cast<int64>(Plan.TargetRect.Max.Y) * Divisor, Source.Metadata.GridDimensions.Y);
			const int32 Span = Source.Manifest.CoreBaseCellSpan;
			const int32 MinChunkX = static_cast<int32>(BaseMinX / Span);
			const int32 MinChunkY = static_cast<int32>(BaseMinY / Span);
			const int32 MaxChunkX = static_cast<int32>((BaseMaxX - 1) / Span);
			const int32 MaxChunkY = static_cast<int32>((BaseMaxY - 1) / Span);
			int64 CoveredCells = 0;
			for (int32 ChunkY = MinChunkY; ChunkY <= MaxChunkY; ++ChunkY)
			{
				for (int32 ChunkX = MinChunkX; ChunkX <= MaxChunkX; ++ChunkX)
				{
					const int32 PayloadIndex = ChunkX + ChunkY * Source.Manifest.ChunkGridDimensions.X;
					if (!Source.Manifest.Chunks.IsValidIndex(PayloadIndex))
					{
						OutError = TEXT("ArcEcotope bulk reader resolved an invalid payload index.");
						return false;
					}
					const FArcEcotopeSiteFieldBlockDescriptor* Descriptor =
						FindDescriptor(Source.Manifest.Chunks[PayloadIndex], Plan.Field);
					if (Descriptor == nullptr)
					{
						// A coarse mip cell belongs only to the chunk containing its minimum-corner
						// base child. Other chunks covered by that cell intentionally omit the field.
						continue;
					}
					if (Descriptor->Level != Plan.Level || Descriptor->ValueType != Plan.ValueType)
					{
						OutError = TEXT("ArcEcotope bulk reader field descriptor is missing or inconsistent.");
						return false;
					}
					const FIntRect OwnedRect(Descriptor->RectMin, Descriptor->RectMax);
					const FIntRect Intersection = IntersectRects(Plan.TargetRect, OwnedRect);
					if (Intersection.Width() <= 0 || Intersection.Height() <= 0)
					{
						continue;
					}
					Plan.Pieces.Add({PayloadIndex, Descriptor, Intersection});
					PayloadSet.Add(PayloadIndex);
					CoveredCells += static_cast<int64>(Intersection.Width()) * Intersection.Height();
				}
			}
			const int64 TargetCells = static_cast<int64>(Plan.TargetRect.Width()) * Plan.TargetRect.Height();
			if (TargetCells <= 0 || TargetCells > MAX_int32 || CoveredCells != TargetCells)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope bulk reader field %d has incomplete or overlapping coverage (%lld/%lld)."),
					FieldValue, CoveredCells, TargetCells);
				return false;
			}
			int64 AlignedOffset = 0;
			if (!TryAdd(OutScratchBytes, 3, AlignedOffset))
			{
				OutError = TEXT("ArcEcotope bulk reader scratch alignment overflowed.");
				return false;
			}
			AlignedOffset &= ~3ll;
			const int64 ValueSize = Plan.ValueType == EArcEcotopeSiteValueType::Float32 ? sizeof(float) : sizeof(uint8);
			int64 FieldBytes = 0;
			if (!TryMultiply(TargetCells, ValueSize, FieldBytes)
				|| !TryAdd(AlignedOffset, FieldBytes, OutScratchBytes))
			{
				OutError = TEXT("ArcEcotope bulk reader scratch size overflowed.");
				return false;
			}
			Plan.ScratchOffset = AlignedOffset;
		}
	OutPayloadIndices.Reserve(PayloadSet.Num());
	for (const int32 PayloadIndex : PayloadSet)
	{
		OutPayloadIndices.Add(PayloadIndex);
	}
	OutPayloadIndices.Sort();
	return true;
	}
#endif
}

#if WITH_EDITORONLY_DATA
namespace ArcEcotopeSiteChunkReaderPrivate
{
	bool PinBulkPayloads(
		const TSharedPtr<const FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe>& ReaderState,
		const TConstArrayView<int32> RequiredPayloads,
		const int64 ScratchBytes,
		FArcEcotopeSiteChunkLeaseData& LeaseData,
		TArray<FSharedBuffer>& OutBuffers,
		FString& OutError)
	{
		const FArcEcotopeFrozenSiteSource& Source = *ReaderState->Source;
		int64 RequiredPayloadBytes = 0;
		for (const int32 PayloadIndex : RequiredPayloads)
		{
			if (!Source.Payloads.IsValidIndex(PayloadIndex)
				|| !TryAdd(RequiredPayloadBytes, Source.Payloads[PayloadIndex].ExpectedSize, RequiredPayloadBytes))
			{
				OutError = TEXT("ArcEcotope bulk reader required payload size overflowed.");
				return false;
			}
		}
		int64 MinimumAcquireBytes = 0;
		if (!TryAdd(RequiredPayloadBytes, ScratchBytes, MinimumAcquireBytes)
			|| MinimumAcquireBytes > ReaderState->CacheBudgetBytes)
		{
			OutError = FString::Printf(
				TEXT("ArcEcotope bulk reader acquisition requires at least %lld bytes, exceeding its %lld-byte budget."),
				MinimumAcquireBytes, ReaderState->CacheBudgetBytes);
			return false;
		}

		TArray<TSharedPtr<FArcEcotopeSitePayloadLoadTicket, ESPMode::ThreadSafe>> Tickets;
		TArray<int32> LoaderPayloads;
		TArray<TSharedPtr<FArcEcotopeSitePayloadLoadTicket, ESPMode::ThreadSafe>> LoaderTickets;
		{
			FScopeLock Lock(&ReaderState->CacheMutex);
			++ReaderState->Stats.AcquireCount;
			int64 NewLoadBytes = 0;
			for (const int32 PayloadIndex : RequiredPayloads)
			{
				if (ReaderState->Cache[PayloadIndex].State == EArcEcotopeSiteCacheEntryState::Empty
					&& !TryAdd(NewLoadBytes, Source.Payloads[PayloadIndex].ExpectedSize, NewLoadBytes))
				{
					++ReaderState->Stats.FailedAcquireCount;
					OutError = TEXT("ArcEcotope bulk reader load reservation overflowed.");
					return false;
				}
			}

			const auto ExceedsBudget = [&]()
			{
				int64 Projected = 0;
				return !TryAddReaderBytes(ReaderState->Stats.ResidentBytes, ReaderState->ReservedLoadBytes, Projected)
					|| !TryAddReaderBytes(Projected, ReaderState->Stats.ScratchBytes, Projected)
					|| !TryAddReaderBytes(Projected, NewLoadBytes, Projected)
					|| !TryAddReaderBytes(Projected, ScratchBytes, Projected)
					|| Projected > ReaderState->CacheBudgetBytes;
			};
			while (ExceedsBudget())
			{
				int32 EvictionIndex = INDEX_NONE;
				uint64 OldestUse = MAX_uint64;
				for (int32 CacheIndex = 0; CacheIndex < ReaderState->Cache.Num(); ++CacheIndex)
				{
					const FArcEcotopeSitePayloadCacheEntry& Entry = ReaderState->Cache[CacheIndex];
					if (Entry.State == EArcEcotopeSiteCacheEntryState::Ready && Entry.PinCount == 0
						&& !RequiredPayloads.Contains(CacheIndex) && Entry.LastUseSerial < OldestUse)
					{
						EvictionIndex = CacheIndex;
						OldestUse = Entry.LastUseSerial;
					}
				}
				if (EvictionIndex == INDEX_NONE)
				{
					++ReaderState->Stats.FailedAcquireCount;
					OutError = TEXT("ArcEcotope bulk reader budget is occupied by pinned or in-flight payloads.");
					return false;
				}
				FArcEcotopeSitePayloadCacheEntry& Evicted = ReaderState->Cache[EvictionIndex];
				ReaderState->Stats.ResidentBytes -= Source.Payloads[EvictionIndex].ExpectedSize;
				Evicted.Buffer.Reset();
				Evicted.State = EArcEcotopeSiteCacheEntryState::Empty;
				Evicted.LastUseSerial = 0;
				++ReaderState->Stats.EvictionCount;
			}

			ReaderState->Stats.ScratchBytes += ScratchBytes;
			for (const int32 PayloadIndex : RequiredPayloads)
			{
				FArcEcotopeSitePayloadCacheEntry& Entry = ReaderState->Cache[PayloadIndex];
				Entry.LastUseSerial = ++ReaderState->UseSerial;
				if (Entry.State == EArcEcotopeSiteCacheEntryState::Ready)
				{
					if (Entry.PinCount++ == 0)
					{
						ReaderState->Stats.PinnedBytes += Source.Payloads[PayloadIndex].ExpectedSize;
					}
					++ReaderState->Stats.CacheHitCount;
					continue;
				}
				if (Entry.State == EArcEcotopeSiteCacheEntryState::Loading)
				{
					++Entry.PendingPinCount;
					Tickets.Add(Entry.LoadTicket);
					++ReaderState->Stats.CoalescedLoadWaitCount;
					continue;
				}

				Entry.State = EArcEcotopeSiteCacheEntryState::Loading;
				Entry.PendingPinCount = 1;
				Entry.LoadTicket = MakeShared<FArcEcotopeSitePayloadLoadTicket, ESPMode::ThreadSafe>();
				ReaderState->ReservedLoadBytes += Source.Payloads[PayloadIndex].ExpectedSize;
				Tickets.Add(Entry.LoadTicket);
				LoaderPayloads.Add(PayloadIndex);
				LoaderTickets.Add(Entry.LoadTicket);
				++ReaderState->Stats.CacheMissCount;
			}
			ReaderState->UpdatePeaksLocked();
		}

		for (int32 LoaderIndex = 0; LoaderIndex < LoaderPayloads.Num(); ++LoaderIndex)
		{
			const int32 PayloadIndex = LoaderPayloads[LoaderIndex];
			const TSharedPtr<FArcEcotopeSitePayloadLoadTicket, ESPMode::ThreadSafe>& Ticket = LoaderTickets[LoaderIndex];
			const double LoadStart = FPlatformTime::Seconds();
			FSharedBuffer Payload = Source.Payloads[PayloadIndex].Data.GetPayload().Get();
			FString LoadError;
			const bool bValid = FArcEcotopeSiteChunkStorage::ValidatePayloadBytes(
				Source.Manifest.Chunks[PayloadIndex], Payload, LoadError);
			const double LoadMilliseconds = (FPlatformTime::Seconds() - LoadStart) * 1000.0;
			{
				FScopeLock Lock(&ReaderState->CacheMutex);
				FArcEcotopeSitePayloadCacheEntry& Entry = ReaderState->Cache[PayloadIndex];
				ReaderState->ReservedLoadBytes -= Source.Payloads[PayloadIndex].ExpectedSize;
				++ReaderState->Stats.PayloadLoadCount;
				ReaderState->Stats.PayloadLoadMilliseconds += LoadMilliseconds;
				if (bValid)
				{
					Entry.Buffer = MoveTemp(Payload);
					Entry.State = EArcEcotopeSiteCacheEntryState::Ready;
					Entry.PinCount = Entry.PendingPinCount;
					Entry.PendingPinCount = 0;
					ReaderState->Stats.ResidentBytes += Source.Payloads[PayloadIndex].ExpectedSize;
					ReaderState->Stats.PinnedBytes += Source.Payloads[PayloadIndex].ExpectedSize;
					ReaderState->Stats.LoadedBytes += Source.Payloads[PayloadIndex].ExpectedSize;
					Ticket->bSucceeded = true;
				}
				else
				{
					Entry.State = EArcEcotopeSiteCacheEntryState::Empty;
					Entry.PendingPinCount = 0;
					Ticket->Error = FString::Printf(
						TEXT("ArcEcotope bulk payload %d failed validation: %s"), PayloadIndex, *LoadError);
				}
				Entry.LoadTicket.Reset();
				ReaderState->UpdatePeaksLocked();
			}
			Ticket->Event->Trigger();
		}

		bool bAnyLoadFailed = false;
		FString FirstLoadError;
		for (const TSharedPtr<FArcEcotopeSitePayloadLoadTicket, ESPMode::ThreadSafe>& Ticket : Tickets)
		{
			Ticket->Event->Wait();
			if (!Ticket->bSucceeded)
			{
				bAnyLoadFailed = true;
				if (FirstLoadError.IsEmpty())
				{
					FirstLoadError = Ticket->Error;
				}
			}
		}
		if (bAnyLoadFailed)
		{
			OutError = MoveTemp(FirstLoadError);
			// Every ticket for this acquisition is settled now, so each successful
			// pending reservation has become an exact Ready pin that can be released.
			ReaderState->ReleaseBulkLease(RequiredPayloads, ScratchBytes);
			FScopeLock Lock(&ReaderState->CacheMutex);
			++ReaderState->Stats.FailedAcquireCount;
			return false;
		}

		OutBuffers.SetNum(Source.Payloads.Num());
		bool bLostPinnedEntry = false;
		{
			FScopeLock Lock(&ReaderState->CacheMutex);
			for (const int32 PayloadIndex : RequiredPayloads)
			{
				const FArcEcotopeSitePayloadCacheEntry& Entry = ReaderState->Cache[PayloadIndex];
				if (Entry.State != EArcEcotopeSiteCacheEntryState::Ready || Entry.Buffer.IsNull())
				{
					bLostPinnedEntry = true;
					break;
				}
				OutBuffers[PayloadIndex] = Entry.Buffer;
			}
		}
		if (bLostPinnedEntry)
		{
			OutError = TEXT("ArcEcotope bulk reader lost a pinned cache entry.");
			ReaderState->ReleaseBulkLease(RequiredPayloads, ScratchBytes);
			FScopeLock Lock(&ReaderState->CacheMutex);
			++ReaderState->Stats.FailedAcquireCount;
			return false;
		}
		LeaseData.PinnedPayloadIndices.Append(RequiredPayloads.GetData(), RequiredPayloads.Num());
		LeaseData.ScratchBytes = ScratchBytes;
		return true;
	}

	template <typename ValueType>
	void SetStitchedView(
		const FBulkFieldPlan& Plan,
		const FIntPoint& Dimensions,
		const uint8* ScratchData,
		TArcEcotopeSiteFieldView<ValueType>& OutView)
	{
		OutView.GlobalRect = Plan.TargetRect;
		OutView.GlobalDimensions = Dimensions;
		OutView.RowStride = Plan.TargetRect.Width();
		OutView.Level = Plan.Level;
		OutView.Values = MakeArrayView(
			reinterpret_cast<const ValueType*>(ScratchData + Plan.ScratchOffset),
			Plan.TargetRect.Width() * Plan.TargetRect.Height());
	}

	bool AcquireEditorBulk(
		const TSharedPtr<const FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe>& ReaderState,
		const FArcEcotopeChunkId& Id,
		const int32 BaseHaloCells,
		const uint32 FieldMask,
		TSharedPtr<const FArcEcotopeSiteChunkLeaseData, ESPMode::ThreadSafe>& OutLeaseData,
		FString& OutError)
	{
		const FArcEcotopeFrozenSiteSource& Source = *ReaderState->Source;
		FArcEcotopeChunkRect Chunk;
		if (!ReaderState->Layout.GetChunk(Id, Chunk))
		{
			OutError = TEXT("ArcEcotope bulk site reader received an invalid chunk id.");
			return false;
		}
		const FIntRect BaseReadRect = FArcEcotopeSiteChunkReader::MakeBaseReadRect(
			Chunk.BaseCoreRect, Source.Metadata.GridDimensions, BaseHaloCells);
		TArray<FBulkFieldPlan> Plans;
		TArray<int32> RequiredPayloads;
		int64 ScratchBytes = 0;
		if (!BuildBulkFieldPlans(
				Source, BaseReadRect, FieldMask, Plans, RequiredPayloads, ScratchBytes, OutError))
		{
			FScopeLock Lock(&ReaderState->CacheMutex);
			++ReaderState->Stats.FailedAcquireCount;
			return false;
		}

		TSharedRef<FArcEcotopeSiteChunkLeaseData, ESPMode::ThreadSafe> LeaseData =
			MakeShared<FArcEcotopeSiteChunkLeaseData, ESPMode::ThreadSafe>();
		LeaseData->ReaderState = ReaderState;
		TArray<FSharedBuffer> PinnedBuffers;
		if (!PinBulkPayloads(
				ReaderState, RequiredPayloads, ScratchBytes, *LeaseData, PinnedBuffers, OutError))
		{
			return false;
		}

		FUniqueBuffer UniqueScratch = FUniqueBuffer::Alloc(static_cast<uint64>(ScratchBytes));
		uint8* ScratchData = static_cast<uint8*>(UniqueScratch.GetData());
		FMemory::Memzero(ScratchData, static_cast<SIZE_T>(ScratchBytes));
		const double StitchStart = FPlatformTime::Seconds();
		for (const FBulkFieldPlan& Plan : Plans)
		{
			const int64 ValueSize = Plan.ValueType == EArcEcotopeSiteValueType::Float32
				? sizeof(float) : sizeof(uint8);
			uint8* Destination = ScratchData + Plan.ScratchOffset;
			for (const FBulkFieldPiece& Piece : Plan.Pieces)
			{
				const FArcEcotopeSiteFieldBlockDescriptor& Descriptor = *Piece.Descriptor;
				const uint8* PayloadData = static_cast<const uint8*>(PinnedBuffers[Piece.PayloadIndex].GetData());
				const int64 SourceStride = Descriptor.RectMax.X - Descriptor.RectMin.X;
				const int64 DestinationStride = Plan.TargetRect.Width();
				const int64 CopyBytes = static_cast<int64>(Piece.Intersection.Width()) * ValueSize;
				for (int32 GlobalY = Piece.Intersection.Min.Y; GlobalY < Piece.Intersection.Max.Y; ++GlobalY)
				{
					const int64 SourceElement = static_cast<int64>(GlobalY - Descriptor.RectMin.Y) * SourceStride
						+ Piece.Intersection.Min.X - Descriptor.RectMin.X;
					const int64 DestinationElement = static_cast<int64>(GlobalY - Plan.TargetRect.Min.Y) * DestinationStride
						+ Piece.Intersection.Min.X - Plan.TargetRect.Min.X;
					FMemory::Memcpy(
						Destination + DestinationElement * ValueSize,
						PayloadData + Descriptor.ByteOffset + SourceElement * ValueSize,
						static_cast<SIZE_T>(CopyBytes));
				}
			}
		}
		const double StitchMilliseconds = (FPlatformTime::Seconds() - StitchStart) * 1000.0;
		LeaseData->Scratch = UniqueScratch.MoveToShared();
		const uint8* ViewData = static_cast<const uint8*>(LeaseData->Scratch.GetData());
		FArcEcotopeSiteChunkView& View = LeaseData->View;
		View.FieldMask = FieldMask;
		View.Id = Id;
		View.BaseCoreRect = Chunk.BaseCoreRect;
		View.BaseReadRect = BaseReadRect;
		for (const FBulkFieldPlan& Plan : Plans)
		{
			const FIntPoint Dimensions = Source.Metadata.GetLevelDimensions(Plan.Level);
			switch (Plan.Field)
			{
			case EArcEcotopeSiteField::LocalSurfaceZ: SetStitchedView(Plan, Dimensions, ViewData, View.LocalSurfaceZ); break;
			case EArcEcotopeSiteField::ValidSurface: SetStitchedView(Plan, Dimensions, ViewData, View.ValidSurface); break;
			case EArcEcotopeSiteField::Slope: SetStitchedView(Plan, Dimensions, ViewData, View.Slope); break;
			case EArcEcotopeSiteField::Aspect: SetStitchedView(Plan, Dimensions, ViewData, View.Aspect); break;
			case EArcEcotopeSiteField::Moisture: SetStitchedView(Plan, Dimensions, ViewData, View.Moisture); break;
			case EArcEcotopeSiteField::Fertility: SetStitchedView(Plan, Dimensions, ViewData, View.Fertility); break;
			case EArcEcotopeSiteField::SunLoad: SetStitchedView(Plan, Dimensions, ViewData, View.SunLoad); break;
			case EArcEcotopeSiteField::WindExposure: SetStitchedView(Plan, Dimensions, ViewData, View.WindExposure); break;
			case EArcEcotopeSiteField::SkyViewFactor: SetStitchedView(Plan, Dimensions, ViewData, View.SkyViewFactor); break;
			case EArcEcotopeSiteField::Temperature: SetStitchedView(Plan, Dimensions, ViewData, View.Temperature); break;
			case EArcEcotopeSiteField::ModificationCut: SetStitchedView(Plan, Dimensions, ViewData, View.ModificationCut); break;
			case EArcEcotopeSiteField::RecordValid: SetStitchedView(Plan, Dimensions, ViewData, View.RecordValid); break;
			case EArcEcotopeSiteField::BareRock: SetStitchedView(Plan, Dimensions, ViewData, View.BareRock); break;
			case EArcEcotopeSiteField::ChannelExclusion: SetStitchedView(Plan, Dimensions, ViewData, View.ChannelExclusion); break;
			case EArcEcotopeSiteField::ChannelBuffer: SetStitchedView(Plan, Dimensions, ViewData, View.ChannelBuffer); break;
			case EArcEcotopeSiteField::EstablishmentWear: SetStitchedView(Plan, Dimensions, ViewData, View.EstablishmentWear); break;
			case EArcEcotopeSiteField::SubstrateMobility: SetStitchedView(Plan, Dimensions, ViewData, View.SubstrateMobility); break;
			default: break;
			}
		}

		{
			FScopeLock Lock(&ReaderState->CacheMutex);
			ReaderState->Stats.StitchedBytes += ScratchBytes;
			ReaderState->Stats.StitchMilliseconds += StitchMilliseconds;
		}
		OutLeaseData = LeaseData;
		return true;
	}
}
#endif

const FArcEcotopeSiteFloatFieldView* FArcEcotopeSiteChunkView::FindFloatField(
	const EArcEcotopeSiteField Field) const
{
	const uint32 FieldValue = static_cast<uint32>(Field);
	if (FieldValue >= static_cast<uint32>(EArcEcotopeSiteField::Count)
		|| (FieldMask & (1u << FieldValue)) == 0)
	{
		return nullptr;
	}
	switch (Field)
	{
	case EArcEcotopeSiteField::LocalSurfaceZ: return &LocalSurfaceZ;
	case EArcEcotopeSiteField::Slope: return &Slope;
	case EArcEcotopeSiteField::Aspect: return &Aspect;
	case EArcEcotopeSiteField::Moisture: return &Moisture;
	case EArcEcotopeSiteField::Fertility: return &Fertility;
	case EArcEcotopeSiteField::SunLoad: return &SunLoad;
	case EArcEcotopeSiteField::WindExposure: return &WindExposure;
	case EArcEcotopeSiteField::SkyViewFactor: return &SkyViewFactor;
	case EArcEcotopeSiteField::Temperature: return &Temperature;
	case EArcEcotopeSiteField::ModificationCut: return &ModificationCut;
	case EArcEcotopeSiteField::BareRock: return &BareRock;
	case EArcEcotopeSiteField::ChannelExclusion: return &ChannelExclusion;
	case EArcEcotopeSiteField::ChannelBuffer: return &ChannelBuffer;
	case EArcEcotopeSiteField::EstablishmentWear: return &EstablishmentWear;
	case EArcEcotopeSiteField::SubstrateMobility: return &SubstrateMobility;
	default: return nullptr;
	}
}

const FArcEcotopeSiteByteFieldView* FArcEcotopeSiteChunkView::FindByteField(
	const EArcEcotopeSiteField Field) const
{
	const uint32 FieldValue = static_cast<uint32>(Field);
	if (FieldValue >= static_cast<uint32>(EArcEcotopeSiteField::Count)
		|| (FieldMask & (1u << FieldValue)) == 0)
	{
		return nullptr;
	}
	switch (Field)
	{
	case EArcEcotopeSiteField::ValidSurface: return &ValidSurface;
	case EArcEcotopeSiteField::RecordValid: return &RecordValid;
	default: return nullptr;
	}
}

const FArcEcotopeSiteChunkView& FArcEcotopeSiteChunkLease::GetView() const
{
	static const FArcEcotopeSiteChunkView EmptyView;
	return Data.IsValid() ? Data->View : EmptyView;
}

bool FArcEcotopeSiteChunkReader::OpenFrozen(
	FArcEcotopeFrozenSiteSourcePtr InSource,
	const int64 InCacheBudgetBytes,
	FString& OutError)
{
#if WITH_EDITORONLY_DATA
	return OpenEditorBulk(MoveTemp(InSource), InCacheBudgetBytes, OutError);
#else
	Close();
	OutError = TEXT("ArcEcotope editor bulk site data is unavailable in this build.");
	return false;
#endif
}

#if WITH_EDITORONLY_DATA
bool FArcEcotopeSiteChunkReader::OpenEditorBulk(
	FArcEcotopeFrozenSiteSourcePtr InSource,
	const int64 InCacheBudgetBytes,
	FString& OutError)
{
	using namespace ArcEcotopeSiteChunkReaderPrivate;
	Close();
	OutError.Reset();
	if (!InSource.IsValid() || InCacheBudgetBytes <= 0)
	{
		OutError = TEXT("ArcEcotope bulk site reader requires a frozen source and positive budget.");
		return false;
	}
	const FArcEcotopeFrozenSiteSource& Source = *InSource;
	if (!ValidateFrozenMetadata(Source.Metadata, OutError))
	{
		return false;
	}
	if (Source.Metadata.LogicalFieldHash == 0
		|| Source.Manifest.LogicalFieldHash == 0
		|| Source.Metadata.LogicalFieldHash != Source.Manifest.LogicalFieldHash)
	{
		OutError = TEXT("ArcEcotope frozen bulk site logical field hash is missing or inconsistent.");
		return false;
	}
	if (Source.Manifest.FormatVersion != FArcEcotopeSiteChunkStorage::CurrentFormatVersion
		|| Source.Manifest.CoreBaseCellSpan <= 0
		|| Source.Manifest.ManifestHash == 0
		|| Source.Manifest.ManifestHash != FArcEcotopeSiteChunkStorage::CalculateManifestHash(Source.Manifest)
		|| Source.Manifest.SourceSiteSnapshotHash != Source.Metadata.SiteSnapshotHash
		|| Source.Manifest.Chunks.Num() != Source.Payloads.Num()
		|| Source.Layout.GetBaseDimensions() != Source.Metadata.GridDimensions
		|| Source.Layout.GetRecordDimensions()
			!= Source.Metadata.GetLevelDimensions(Source.Metadata.RecordGridLevel)
		|| Source.Layout.GetChunkGridDimensions() != Source.Manifest.ChunkGridDimensions
		|| Source.Layout.GetChunkCount() != Source.Manifest.Chunks.Num()
		|| Source.Layout.GetCoreBaseCellSpan() != Source.Manifest.CoreBaseCellSpan
		|| Source.Layout.GetRecordHaloCells() != 0)
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("ArcEcotope frozen bulk site source metadata is inconsistent.");
		}
		return false;
	}
	const int32 RecordDivisor = 1 << Source.Metadata.RecordGridLevel;
	const int64 ExpectedChunkGridX =
		(static_cast<int64>(Source.Metadata.GridDimensions.X) + Source.Manifest.CoreBaseCellSpan - 1)
		/ Source.Manifest.CoreBaseCellSpan;
	const int64 ExpectedChunkGridY =
		(static_cast<int64>(Source.Metadata.GridDimensions.Y) + Source.Manifest.CoreBaseCellSpan - 1)
		/ Source.Manifest.CoreBaseCellSpan;
	const int64 ExpectedChunkCount = ExpectedChunkGridX * ExpectedChunkGridY;
	if (Source.Manifest.CoreBaseCellSpan % RecordDivisor != 0
		|| ExpectedChunkGridX <= 0 || ExpectedChunkGridY <= 0
		|| ExpectedChunkGridX > MAX_int32 || ExpectedChunkGridY > MAX_int32
		|| ExpectedChunkCount <= 0 || ExpectedChunkCount > MAX_int32
		|| Source.Manifest.ChunkGridDimensions
			!= FIntPoint(static_cast<int32>(ExpectedChunkGridX), static_cast<int32>(ExpectedChunkGridY))
		|| Source.Layout.GetCoreRecordCellSpan() != Source.Manifest.CoreBaseCellSpan / RecordDivisor)
	{
		OutError = TEXT("ArcEcotope frozen bulk site layout does not partition the declared grid.");
		return false;
	}
	for (int32 PayloadIndex = 0; PayloadIndex < Source.Payloads.Num(); ++PayloadIndex)
	{
		const FArcEcotopeFrozenSitePayload& Payload = Source.Payloads[PayloadIndex];
		const FArcEcotopeSiteChunkManifestEntry& Entry = Source.Manifest.Chunks[PayloadIndex];
		FArcEcotopeChunkRect ExpectedChunk;
		const int32 ExpectedChunkX = PayloadIndex % Source.Manifest.ChunkGridDimensions.X;
		const int32 ExpectedChunkY = PayloadIndex / Source.Manifest.ChunkGridDimensions.X;
		const FIntPoint ExpectedBaseMin(
			static_cast<int32>(static_cast<int64>(ExpectedChunkX) * Source.Manifest.CoreBaseCellSpan),
			static_cast<int32>(static_cast<int64>(ExpectedChunkY) * Source.Manifest.CoreBaseCellSpan));
		const FIntPoint ExpectedBaseMax(
			static_cast<int32>(FMath::Min<int64>(
				static_cast<int64>(ExpectedBaseMin.X) + Source.Manifest.CoreBaseCellSpan,
				Source.Metadata.GridDimensions.X)),
			static_cast<int32>(FMath::Min<int64>(
				static_cast<int64>(ExpectedBaseMin.Y) + Source.Manifest.CoreBaseCellSpan,
				Source.Metadata.GridDimensions.Y)));
		const FIntRect ExpectedRecordCore(
			ExpectedBaseMin.X / RecordDivisor,
			ExpectedBaseMin.Y / RecordDivisor,
			static_cast<int32>((static_cast<int64>(ExpectedBaseMax.X) + RecordDivisor - 1) / RecordDivisor),
			static_cast<int32>((static_cast<int64>(ExpectedBaseMax.Y) + RecordDivisor - 1) / RecordDivisor));
		if (!Source.Layout.GetChunk(PayloadIndex, ExpectedChunk)
			|| !(ExpectedChunk.Id == FArcEcotopeChunkId{ExpectedChunkX, ExpectedChunkY})
			|| ExpectedChunk.BaseCoreRect != FIntRect(ExpectedBaseMin, ExpectedBaseMax)
			|| ExpectedChunk.BaseReadRect != ExpectedChunk.BaseCoreRect
			|| ExpectedChunk.RecordCoreRect != ExpectedRecordCore
			|| ExpectedChunk.RecordReadRect != ExpectedChunk.RecordCoreRect
			|| Entry.ChunkId != FIntPoint(ExpectedChunkX, ExpectedChunkY)
			|| Entry.BaseCoreMin != ExpectedChunk.BaseCoreRect.Min
			|| Entry.BaseCoreMax != ExpectedChunk.BaseCoreRect.Max
			|| Entry.PayloadSize > MAX_int64
			|| Entry.PayloadHash == 0 || Payload.ExpectedHash == 0
			|| Entry.PayloadIndex != PayloadIndex || Payload.ExpectedSize <= 0
			|| static_cast<uint64>(Payload.ExpectedSize) != Entry.PayloadSize
			|| Payload.ExpectedHash != Entry.PayloadHash
			|| Payload.Data.GetPayloadSize() != Payload.ExpectedSize)
		{
			OutError = FString::Printf(TEXT("ArcEcotope frozen bulk site payload %d is inconsistent."), PayloadIndex);
			return false;
		}
		uint32 SeenFieldMask = 0;
		uint64 NextByteOffset = FArcEcotopeSiteChunkStorage::PayloadHeaderBytes;
		for (const FArcEcotopeSiteFieldBlockDescriptor& Descriptor : Entry.Fields)
		{
			const uint8 FieldValue = static_cast<uint8>(Descriptor.Field);
			const uint32 FieldBit = FieldValue < 32 ? 1u << FieldValue : 0;
			const EArcEcotopeSiteValueType ExpectedValueType = Descriptor.Field == EArcEcotopeSiteField::ValidSurface
				|| Descriptor.Field == EArcEcotopeSiteField::RecordValid
				? EArcEcotopeSiteValueType::UInt8 : EArcEcotopeSiteValueType::Float32;
			const int64 ValueSize = Descriptor.ValueType == EArcEcotopeSiteValueType::Float32
				? sizeof(float) : Descriptor.ValueType == EArcEcotopeSiteValueType::UInt8 ? sizeof(uint8) : 0;
			const FIntPoint Dimensions = Source.Metadata.GetLevelDimensions(Descriptor.Level);
			FIntRect ExpectedOwnedRect;
			const int64 Width = static_cast<int64>(Descriptor.RectMax.X) - Descriptor.RectMin.X;
			const int64 Height = static_cast<int64>(Descriptor.RectMax.Y) - Descriptor.RectMin.Y;
			int64 ExpectedBytes = 0;
			if (FieldValue >= static_cast<uint8>(EArcEcotopeSiteField::Count) || FieldBit == 0)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d has field descriptor value %d out of range."),
					PayloadIndex, FieldValue);
				return false;
			}
			if ((SeenFieldMask & FieldBit) != 0)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d has a duplicate field descriptor for %s."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue));
				return false;
			}
			if (Descriptor.Level != Source.Metadata.GetFieldLevel(Descriptor.Field))
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has level %d but metadata expects level %d."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue),
					Descriptor.Level, Source.Metadata.GetFieldLevel(Descriptor.Field));
				return false;
			}
			if (Descriptor.ValueType != ExpectedValueType)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has value type %d but expected %d."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue),
					static_cast<int32>(Descriptor.ValueType), static_cast<int32>(ExpectedValueType));
				return false;
			}
			if (!TryMapBaseCoreRectToOwnedLevel(
				ExpectedChunk.BaseCoreRect, Source.Metadata.GridDimensions, Descriptor.Level, ExpectedOwnedRect))
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s could not map base core rect to owned level %d."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue), Descriptor.Level);
				return false;
			}
			if (Descriptor.RectMin != ExpectedOwnedRect.Min || Descriptor.RectMax != ExpectedOwnedRect.Max
				|| Dimensions.X <= 0 || Dimensions.Y <= 0 || Descriptor.RectMin.X < 0 || Descriptor.RectMin.Y < 0
				|| Descriptor.RectMax.X > Dimensions.X || Descriptor.RectMax.Y > Dimensions.Y)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has rect (%d,%d)-(%d,%d) but expected (%d,%d)-(%d,%d) within dimensions (%d,%d)."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue),
					Descriptor.RectMin.X, Descriptor.RectMin.Y, Descriptor.RectMax.X, Descriptor.RectMax.Y,
					ExpectedOwnedRect.Min.X, ExpectedOwnedRect.Min.Y, ExpectedOwnedRect.Max.X, ExpectedOwnedRect.Max.Y,
					Dimensions.X, Dimensions.Y);
				return false;
			}
			if (Width <= 0 || Height <= 0)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has non-positive rect width %lld or height %lld."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue), Width, Height);
				return false;
			}
			if (ValueSize == 0
				|| !TryMultiply(Width, Height, ExpectedBytes)
				|| !TryMultiply(ExpectedBytes, ValueSize, ExpectedBytes)
				|| static_cast<uint64>(ExpectedBytes) != Descriptor.ByteSize)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has byte size %llu but computed %lld."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue),
					Descriptor.ByteSize, ExpectedBytes);
				return false;
			}
			if (!TryAlign4(NextByteOffset, NextByteOffset))
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s offset alignment overflowed."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue));
				return false;
			}
			if (Descriptor.ByteOffset != NextByteOffset)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has byte offset %llu but expected %llu."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue),
					Descriptor.ByteOffset, NextByteOffset);
				return false;
			}
			if (Descriptor.DataHash == 0)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s has a zero data hash."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue));
				return false;
			}
			if (Descriptor.ByteOffset > Entry.PayloadSize
				|| Descriptor.ByteSize > Entry.PayloadSize - Descriptor.ByteOffset)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope frozen bulk site payload %d field %s at offset %llu size %llu exceeds payload size %llu."),
					PayloadIndex, *StaticEnum<EArcEcotopeSiteField>()->GetNameStringByValue(FieldValue),
					Descriptor.ByteOffset, Descriptor.ByteSize, Entry.PayloadSize);
				return false;
			}
			NextByteOffset += Descriptor.ByteSize;
			SeenFieldMask |= FieldBit;
		}
		uint32 ExpectedFieldMask = 0;
		for (int32 FieldValue = 0; FieldValue < static_cast<int32>(EArcEcotopeSiteField::Count); ++FieldValue)
		{
			const EArcEcotopeSiteField Field = static_cast<EArcEcotopeSiteField>(FieldValue);
			FIntRect ExpectedOwnedRect;
			if (!TryMapBaseCoreRectToOwnedLevel(
					ExpectedChunk.BaseCoreRect,
					Source.Metadata.GridDimensions,
					Source.Metadata.GetFieldLevel(Field),
					ExpectedOwnedRect))
			{
				OutError = TEXT("ArcEcotope frozen bulk site field ownership is invalid.");
				return false;
			}
			if (ExpectedOwnedRect.Width() > 0 && ExpectedOwnedRect.Height() > 0)
			{
				ExpectedFieldMask |= 1u << FieldValue;
			}
		}
		if (SeenFieldMask != Entry.FieldMask || SeenFieldMask != ExpectedFieldMask
			|| NextByteOffset != Entry.PayloadSize)
		{
			OutError = FString::Printf(
				TEXT("ArcEcotope frozen bulk site payload %d field mask is inconsistent."), PayloadIndex);
			return false;
		}
	}

	TSharedRef<FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe> NewState =
		MakeShared<FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe>();
	NewState->Source = MoveTemp(InSource);
	NewState->Layout = Source.Layout;
	NewState->CacheBudgetBytes = InCacheBudgetBytes;
	NewState->Cache.SetNum(Source.Payloads.Num());
	NewState->Stats.BudgetBytes = InCacheBudgetBytes;
	NewState->Stats.SourceLoadedBytes = Source.GetLoadedSourceBytes();
	NewState->Stats.SourceMemoryOnlyBytes = Source.GetMemoryOnlySourceBytes();
	State = NewState;
	return true;
}

FArcEcotopeSiteBulkReaderStats FArcEcotopeSiteChunkReader::GetBulkStats() const
{
	const TSharedPtr<const FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe> ReaderState = State;
	if (!ReaderState.IsValid())
	{
		return FArcEcotopeSiteBulkReaderStats();
	}
	FScopeLock Lock(&ReaderState->CacheMutex);
	return ReaderState->Stats;
}

void FArcEcotopeSiteChunkReader::LogBulkStats(const TCHAR* Context) const
{
	const FArcEcotopeSiteBulkReaderStats Value = GetBulkStats();
	constexpr double MiB = 1024.0 * 1024.0;
	UE_LOGF(LogArcEcotope, Log,"ArcEcotope site bulk reader | context=%ls acquires=%llu failed=%llu hits=%llu misses=%llu waits=%llu loads=%llu evictions=%llu loaded=%.2fMiB stitched=%.2fMiB peakResident=%.2fMiB peakPinned=%.2fMiB peakScratch=%.2fMiB peakManaged=%.2fMiB budget=%.2fMiB sourceLoadedOutsideBudget=%.2fMiB sourceMemoryOnlyOutsideBudget=%.2fMiB load=%.2fms stitch=%.2fms",
		Context != nullptr ? Context : TEXT("unknown"), Value.AcquireCount, Value.FailedAcquireCount,
		Value.CacheHitCount, Value.CacheMissCount, Value.CoalescedLoadWaitCount,
		Value.PayloadLoadCount, Value.EvictionCount, static_cast<double>(Value.LoadedBytes) / MiB,
		static_cast<double>(Value.StitchedBytes) / MiB, static_cast<double>(Value.PeakResidentBytes) / MiB,
		static_cast<double>(Value.PeakPinnedBytes) / MiB, static_cast<double>(Value.PeakScratchBytes) / MiB,
		static_cast<double>(Value.PeakTotalBytes) / MiB, static_cast<double>(Value.BudgetBytes) / MiB,
		static_cast<double>(Value.SourceLoadedBytes) / MiB,
		static_cast<double>(Value.SourceMemoryOnlyBytes) / MiB,
		Value.PayloadLoadMilliseconds, Value.StitchMilliseconds);
}

bool FArcEcotopeSiteChunkReader::ValidateEditorBulkSource(
	FArcEcotopeFrozenSiteSourcePtr BulkSource,
	const int64 CacheBudgetBytes,
	FArcEcotopeSiteChunkReader& OutReader,
	FArcEcotopeSiteBulkValidationResult& OutResult,
	FArcEcotopeSiteBulkReaderStats& OutStats,
	FString& OutError,
	const FArcForestCancellationToken* Cancellation)
{
	using namespace ArcEcotopeSiteChunkReaderPrivate;
	OutReader.Close();
	OutResult = FArcEcotopeSiteBulkValidationResult();
	OutStats = FArcEcotopeSiteBulkReaderStats();
	OutError.Reset();
	const auto IsCancelled = [Cancellation]()
	{
		return Cancellation != nullptr && Cancellation->IsCancellationRequested();
	};
	if (IsCancelled())
	{
		OutError = TEXT("ArcEcotope bulk site snapshot validation was cancelled.");
		return false;
	}
	if (!BulkSource.IsValid())
	{
		OutError = TEXT("ArcEcotope bulk site snapshot validation requires a frozen source.");
		return false;
	}

	const FArcEcotopeSiteMetadata& Metadata = BulkSource->Metadata;
	FArcEcotopeSiteChunkReader BulkReader;
	if (!BulkReader.OpenEditorBulk(BulkSource, CacheBudgetBytes, OutError))
	{
		return false;
	}

	const FIntPoint ChunkGrid = BulkSource->Layout.GetChunkGridDimensions();

	OutResult.CanonicalHash = FnvOffsetBasis;
	OutResult.LogicalFieldHash = FnvOffsetBasis;
	HashVector(OutResult.CanonicalHash, Metadata.LocalBounds.Min);
	HashVector(OutResult.CanonicalHash, Metadata.LocalBounds.Max);
	HashFloat(OutResult.CanonicalHash, Metadata.CellSizeCm);
	HashUInt32(OutResult.CanonicalHash, static_cast<uint32>(Metadata.GridDimensions.X));
	HashUInt32(OutResult.CanonicalHash, static_cast<uint32>(Metadata.GridDimensions.Y));

	const auto VisitField = [&](const EArcEcotopeSiteField Field, const TCHAR* FieldName, const bool bHashValues)
	{
		const uint8 Level = Metadata.GetFieldLevel(Field);
		const FIntPoint Dimensions = Metadata.GetLevelDimensions(Level);
		const int64 FieldCount = static_cast<int64>(Dimensions.X) * Dimensions.Y;
		if (FieldCount <= 0 || FieldCount > MAX_int32)
		{
			OutError = FString::Printf(TEXT("ArcEcotope bulk field %s is not representable."), FieldName);
			return false;
		}
		if (bHashValues)
		{
			HashUInt64(OutResult.CanonicalHash, static_cast<uint64>(FieldCount));
		}
		HashByte(OutResult.LogicalFieldHash, static_cast<uint8>(Field));
		HashByte(OutResult.LogicalFieldHash, Level);
		HashUInt32(OutResult.LogicalFieldHash, static_cast<uint32>(Dimensions.X));
		HashUInt32(OutResult.LogicalFieldHash, static_cast<uint32>(Dimensions.Y));
		const bool bByteField = Field == EArcEcotopeSiteField::ValidSurface
			|| Field == EArcEcotopeSiteField::RecordValid;
		const int64 ValueSize = bByteField ? sizeof(uint8) : sizeof(float);
		for (int32 ChunkY = 0; ChunkY < ChunkGrid.Y; ++ChunkY)
		{
			if (IsCancelled())
			{
				OutError = TEXT("ArcEcotope bulk site snapshot validation was cancelled.");
				return false;
			}
			FArcEcotopeChunkRect FirstChunk;
			FIntRect RowOwnedRect;
			if (!BulkSource->Layout.GetChunk(FArcEcotopeChunkId{0, ChunkY}, FirstChunk)
				|| !TryMapBaseCoreRectToOwnedLevel(
					FirstChunk.BaseCoreRect, Metadata.GridDimensions, Level, RowOwnedRect))
			{
				OutError = FString::Printf(TEXT("ArcEcotope bulk field %s has invalid row ownership."), FieldName);
				return false;
			}
			if (RowOwnedRect.Height() == 0)
			{
				continue;
			}
			const int64 RowElements = static_cast<int64>(RowOwnedRect.Height()) * Dimensions.X;
			const int64 RowBytes = RowElements * ValueSize;
			if (RowElements <= 0 || RowBytes <= 0 || RowBytes > MAX_int32
				|| RowBytes > CacheBudgetBytes)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope bulk field %s row scratch requires %lld bytes, exceeding its %lld-byte validation budget."),
					FieldName, RowBytes, CacheBudgetBytes);
				return false;
			}
			TArray<uint8> RowBlock;
			RowBlock.SetNumUninitialized(static_cast<int32>(RowBytes));
			if (static_cast<int64>(RowBlock.GetAllocatedSize()) > CacheBudgetBytes)
			{
				OutError = FString::Printf(
					TEXT("ArcEcotope bulk field %s row allocation exceeds its validation budget."), FieldName);
				return false;
			}
			OutResult.PeakValidationScratchBytes = FMath::Max(
				OutResult.PeakValidationScratchBytes,
				static_cast<int64>(RowBlock.GetAllocatedSize()));
			int32 ExpectedX = 0;
			for (int32 ChunkX = 0; ChunkX < ChunkGrid.X; ++ChunkX)
			{
				if (IsCancelled())
				{
					OutError = TEXT("ArcEcotope bulk site snapshot validation was cancelled.");
					return false;
				}
				const FArcEcotopeChunkId Id{ChunkX, ChunkY};
				FArcEcotopeChunkRect Chunk;
				FIntRect OwnedRect;
				if (!BulkSource->Layout.GetChunk(Id, Chunk)
					|| !TryMapBaseCoreRectToOwnedLevel(
						Chunk.BaseCoreRect, Metadata.GridDimensions, Level, OwnedRect))
				{
					OutError = FString::Printf(TEXT("ArcEcotope bulk field %s has invalid chunk ownership."), FieldName);
					return false;
				}
				if (OwnedRect.Width() == 0)
				{
					continue;
				}
				if (OwnedRect.Min.X != ExpectedX || OwnedRect.Min.Y != RowOwnedRect.Min.Y
					|| OwnedRect.Max.Y != RowOwnedRect.Max.Y)
				{
					OutError = FString::Printf(TEXT("ArcEcotope bulk field %s has an ownership gap or overlap."), FieldName);
					return false;
				}
				FArcEcotopeSiteChunkLease Lease;
				if (!BulkReader.AcquireFields(
						Id, 0, 1u << static_cast<uint32>(Field), Lease, OutError))
				{
					return false;
				}
				++OutResult.AcquireCount;
				for (int32 Y = OwnedRect.Min.Y; Y < OwnedRect.Max.Y; ++Y)
				{
					const int64 DestinationElement = static_cast<int64>(Y - RowOwnedRect.Min.Y) * Dimensions.X
						+ OwnedRect.Min.X;
					if (bByteField)
					{
						const FArcEcotopeSiteByteFieldView* View = Lease.GetView().FindByteField(Field);
						const uint8* Values = View != nullptr ? View->TryGet(OwnedRect.Min.X, Y) : nullptr;
						if (Values == nullptr)
						{
							OutError = FString::Printf(TEXT("ArcEcotope bulk field %s row is missing."), FieldName);
							return false;
						}
						FMemory::Memcpy(RowBlock.GetData() + DestinationElement, Values, OwnedRect.Width());
					}
					else
					{
						const FArcEcotopeSiteFloatFieldView* View = Lease.GetView().FindFloatField(Field);
						const float* Values = View != nullptr ? View->TryGet(OwnedRect.Min.X, Y) : nullptr;
						if (Values == nullptr)
						{
							OutError = FString::Printf(TEXT("ArcEcotope bulk field %s row is missing."), FieldName);
							return false;
						}
						FMemory::Memcpy(
							RowBlock.GetData() + DestinationElement * sizeof(float), Values,
							static_cast<SIZE_T>(OwnedRect.Width()) * sizeof(float));
					}
				}
				ExpectedX = OwnedRect.Max.X;
			}
			if (ExpectedX != Dimensions.X)
			{
				OutError = FString::Printf(TEXT("ArcEcotope bulk field %s does not cover its row."), FieldName);
				return false;
			}

			const int64 GlobalStart = static_cast<int64>(RowOwnedRect.Min.Y) * Dimensions.X;
			for (int64 ValueIndex = 0; ValueIndex < RowElements; ++ValueIndex)
			{
				const int64 GlobalIndex = GlobalStart + ValueIndex;
				if (bByteField)
				{
					const uint8 Value = RowBlock[static_cast<int32>(ValueIndex)];
					if (Value > 1)
					{
						OutError = FString::Printf(TEXT("ArcEcotope bulk field %s cell %lld contains invalid data."), FieldName, GlobalIndex);
						return false;
					}
					HashByte(OutResult.LogicalFieldHash, Value);
					if (bHashValues)
					{
						HashByte(OutResult.CanonicalHash, Value);
					}
				}
				else
				{
					const float Value = reinterpret_cast<const float*>(RowBlock.GetData())[ValueIndex];
					const bool bFinite = FMath::IsFinite(Value);
					const bool bValid = Field == EArcEcotopeSiteField::LocalSurfaceZ ? bFinite
						: Field == EArcEcotopeSiteField::Slope ? bFinite && Value >= 0.0f && Value <= 180.0f
						: Field == EArcEcotopeSiteField::Aspect ? bFinite && Value >= 0.0f && Value < 1.0f
						: bFinite && Value >= 0.0f && Value <= 1.0f;
					if (!bValid)
					{
						OutError = FString::Printf(TEXT("ArcEcotope bulk field %s cell %lld contains invalid data."), FieldName, GlobalIndex);
						return false;
					}
					HashFloat(OutResult.LogicalFieldHash, Value);
					if (bHashValues)
					{
						HashFloat(OutResult.CanonicalHash, Value);
					}
				}
			}
			OutResult.LogicalValueCount += RowElements;
		}
		return true;
	};

	if (!VisitField(EArcEcotopeSiteField::LocalSurfaceZ, TEXT("LocalSurfaceZ"), true)
		|| !VisitField(EArcEcotopeSiteField::ValidSurface, TEXT("ValidSurface"), true)
		|| !VisitField(EArcEcotopeSiteField::Slope, TEXT("Slope"), true)
		|| !VisitField(EArcEcotopeSiteField::Aspect, TEXT("Aspect"), true)
		|| !VisitField(EArcEcotopeSiteField::Moisture, TEXT("Moisture"), true)
		|| !VisitField(EArcEcotopeSiteField::Fertility, TEXT("Fertility"), true))
	{
		return false;
	}
	HashUInt32(OutResult.CanonicalHash, static_cast<uint32>(Metadata.SiteSchemaVersion));
	HashByte(OutResult.CanonicalHash, Metadata.MoistureLevel);
	HashByte(OutResult.CanonicalHash, Metadata.FertilityLevel);
	if (!VisitField(EArcEcotopeSiteField::SunLoad, TEXT("SunLoad"), true)
		|| !VisitField(EArcEcotopeSiteField::WindExposure, TEXT("WindExposure"), true)
		|| !VisitField(EArcEcotopeSiteField::SkyViewFactor, TEXT("SkyViewFactor"), true))
	{
		return false;
	}
	HashByte(OutResult.CanonicalHash, Metadata.SunLoadLevel);
	HashByte(OutResult.CanonicalHash, Metadata.WindExposureLevel);
	HashByte(OutResult.CanonicalHash, Metadata.SkyViewFactorLevel);
	if (!VisitField(EArcEcotopeSiteField::Temperature, TEXT("Temperature"), true))
	{
		return false;
	}
	HashByte(OutResult.CanonicalHash, Metadata.TemperatureLevel);
	if (!VisitField(EArcEcotopeSiteField::ModificationCut, TEXT("ModificationCut"), true))
	{
		return false;
	}
	HashByte(OutResult.CanonicalHash, Metadata.ModificationCutLevel);
	if (!VisitField(EArcEcotopeSiteField::RecordValid, TEXT("RecordValid"), false))
	{
		return false;
	}
	// RecordValid contributes no canonical bytes, so the canonical order still matches the writer's.
	if (!VisitField(EArcEcotopeSiteField::BareRock, TEXT("BareRock"), true)
		|| !VisitField(EArcEcotopeSiteField::ChannelExclusion, TEXT("ChannelExclusion"), true)
		|| !VisitField(EArcEcotopeSiteField::ChannelBuffer, TEXT("ChannelBuffer"), true)
		|| !VisitField(EArcEcotopeSiteField::EstablishmentWear, TEXT("EstablishmentWear"), true)
		|| !VisitField(EArcEcotopeSiteField::SubstrateMobility, TEXT("SubstrateMobility"), true))
	{
		return false;
	}
	HashByte(OutResult.CanonicalHash, Metadata.BareRockLevel);
	HashByte(OutResult.CanonicalHash, Metadata.ChannelExclusionLevel);
	HashByte(OutResult.CanonicalHash, Metadata.EstablishmentWearLevel);
	HashByte(OutResult.CanonicalHash, Metadata.SubstrateMobilityLevel);
	const uint32 ValidityFieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::ValidSurface))
		| (1u << static_cast<uint32>(EArcEcotopeSiteField::RecordValid));
	for (int32 ChunkY = 0; ChunkY < ChunkGrid.Y; ++ChunkY)
	{
		for (int32 ChunkX = 0; ChunkX < ChunkGrid.X; ++ChunkX)
		{
			if (IsCancelled())
			{
				OutError = TEXT("ArcEcotope bulk site snapshot validation was cancelled.");
				return false;
			}
			const FArcEcotopeChunkId Id{ChunkX, ChunkY};
			FArcEcotopeChunkRect Chunk;
			FArcEcotopeSiteChunkLease Lease;
			FIntRect RecordOwnedRect;
			if (!BulkSource->Layout.GetChunk(Id, Chunk)
				|| !TryMapBaseCoreRectToOwnedLevel(
					Chunk.BaseCoreRect, Metadata.GridDimensions, Metadata.RecordGridLevel, RecordOwnedRect)
				|| !BulkReader.AcquireFields(Id, 0, ValidityFieldMask, Lease, OutError))
			{
				return false;
			}
			++OutResult.AcquireCount;
			const FArcEcotopeSiteByteFieldView* ValidSurface =
				Lease.GetView().FindByteField(EArcEcotopeSiteField::ValidSurface);
			const FArcEcotopeSiteByteFieldView* RecordValid =
				Lease.GetView().FindByteField(EArcEcotopeSiteField::RecordValid);
			if (ValidSurface == nullptr || RecordValid == nullptr)
			{
				OutError = TEXT("ArcEcotope bulk validity pass is missing required fields.");
				return false;
			}
			const int32 ChildSpan = 1 << Metadata.RecordGridLevel;
			for (int32 RecordY = RecordOwnedRect.Min.Y; RecordY < RecordOwnedRect.Max.Y; ++RecordY)
			{
				for (int32 RecordX = RecordOwnedRect.Min.X; RecordX < RecordOwnedRect.Max.X; ++RecordX)
				{
					bool bExpectedValid = false;
					const int32 BaseMinY = static_cast<int32>(static_cast<int64>(RecordY) * ChildSpan);
					const int32 BaseMaxY = static_cast<int32>(FMath::Min<int64>(
						(static_cast<int64>(RecordY) + 1) * ChildSpan, Metadata.GridDimensions.Y));
					const int32 BaseMinX = static_cast<int32>(static_cast<int64>(RecordX) * ChildSpan);
					const int32 BaseMaxX = static_cast<int32>(FMath::Min<int64>(
						(static_cast<int64>(RecordX) + 1) * ChildSpan, Metadata.GridDimensions.X));
					for (int32 BaseY = BaseMinY;
						BaseY < BaseMaxY && !bExpectedValid;
						++BaseY)
					{
						for (int32 BaseX = BaseMinX;
							BaseX < BaseMaxX;
							++BaseX)
						{
							const uint8* Value = ValidSurface->TryGet(BaseX, BaseY);
							if (Value == nullptr)
							{
								OutError = TEXT("ArcEcotope bulk validity pass could not read a base cell.");
								return false;
							}
							bExpectedValid |= *Value != 0;
							if (bExpectedValid)
							{
								break;
							}
						}
					}
					const uint8* Actual = RecordValid->TryGet(RecordX, RecordY);
					if (Actual == nullptr || (*Actual != 0) != bExpectedValid)
					{
						OutError = FString::Printf(
							TEXT("ArcEcotope bulk RecordValid cell (%d,%d) disagrees with ValidSurface."),
							RecordX, RecordY);
						return false;
					}
				}
			}
		}
	}
	if (OutResult.CanonicalHash != Metadata.SiteSnapshotHash
		|| OutResult.CanonicalHash != BulkSource->Manifest.SourceSiteSnapshotHash)
	{
		OutError = FString::Printf(
			TEXT("ArcEcotope bulk canonical hash %llu does not match snapshot hash %llu."),
			OutResult.CanonicalHash, Metadata.SiteSnapshotHash);
		return false;
	}
	if (OutResult.LogicalFieldHash != BulkSource->Manifest.LogicalFieldHash)
	{
		OutError = FString::Printf(
			TEXT("ArcEcotope bulk logical field hash %llu does not match manifest hash %llu."),
			OutResult.LogicalFieldHash, BulkSource->Manifest.LogicalFieldHash);
		return false;
	}
	OutStats = BulkReader.GetBulkStats();
	BulkReader.LogBulkStats(TEXT("source-validation"));
	OutReader.State = MoveTemp(BulkReader.State);
	return true;
}

#endif

void FArcEcotopeSiteChunkReader::Close()
{
	State.Reset();
}

bool FArcEcotopeSiteChunkReader::IsOpen() const
{
	return State.IsValid() && State->CacheBudgetBytes > 0 && State->Source.IsValid();
}

int64 FArcEcotopeSiteChunkReader::GetCacheBudgetBytes() const
{
	return State.IsValid() ? State->CacheBudgetBytes : 0;
}

bool FArcEcotopeSiteChunkReader::Acquire(
	const FArcEcotopeChunkId& Id,
	const int32 BaseHaloCells,
	FArcEcotopeSiteChunkLease& OutLease,
	FString& OutError) const
{
	return AcquireFields(Id, BaseHaloCells, AllFieldMask, OutLease, OutError);
}

bool FArcEcotopeSiteChunkReader::AcquireFields(
	const FArcEcotopeChunkId& Id,
	const int32 BaseHaloCells,
	const uint32 FieldMask,
	FArcEcotopeSiteChunkLease& OutLease,
	FString& OutError) const
{
	using namespace ArcEcotopeSiteChunkReaderPrivate;
	static_assert(static_cast<uint32>(EArcEcotopeSiteField::Count) < 32);
	OutLease.Reset();
	OutError.Reset();
	const TSharedPtr<const FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe> ReaderState = State;
	if (!ReaderState.IsValid() || ReaderState->CacheBudgetBytes <= 0)
	{
		OutError = TEXT("ArcEcotope site reader is not open.");
		return false;
	}
	if (BaseHaloCells < 0)
	{
		OutError = TEXT("ArcEcotope site reader halo must not be negative.");
		return false;
	}
	if (FieldMask == 0 || (FieldMask & ~AllFieldMask) != 0)
	{
		OutError = TEXT("ArcEcotope site reader field mask is empty or invalid.");
		return false;
	}
#if WITH_EDITORONLY_DATA
	return AcquireEditorBulk(ReaderState, Id, BaseHaloCells, FieldMask, OutLease.Data, OutError);
#else
	OutError = TEXT("ArcEcotope editor bulk site data is unavailable in this build.");
	return false;
#endif
}

bool FArcEcotopeSiteChunkReader::TryMapBaseReadRectToLevel(
	const FIntRect& BaseRect,
	const FIntPoint& BaseDimensions,
	const uint8 Level,
	FIntRect& OutLevelRect)
{
	OutLevelRect = FIntRect();
	if (Level > FArcEcotopeSiteMetadata::MaxGridLevel || BaseDimensions.X <= 0 || BaseDimensions.Y <= 0
		|| BaseRect.Min.X < 0 || BaseRect.Min.Y < 0 || BaseRect.Max.X > BaseDimensions.X
		|| BaseRect.Max.Y > BaseDimensions.Y || BaseRect.Width() <= 0 || BaseRect.Height() <= 0)
	{
		return false;
	}
	const int64 Divisor = 1ll << Level;
	const FIntPoint LevelDimensions(
		static_cast<int32>((static_cast<int64>(BaseDimensions.X) + Divisor - 1) / Divisor),
		static_cast<int32>((static_cast<int64>(BaseDimensions.Y) + Divisor - 1) / Divisor));
	OutLevelRect = FIntRect(
		static_cast<int32>(BaseRect.Min.X / Divisor),
		static_cast<int32>(BaseRect.Min.Y / Divisor),
		static_cast<int32>((static_cast<int64>(BaseRect.Max.X) + Divisor - 1) / Divisor),
		static_cast<int32>((static_cast<int64>(BaseRect.Max.Y) + Divisor - 1) / Divisor));
	return OutLevelRect.Min.X >= 0 && OutLevelRect.Min.Y >= 0
		&& OutLevelRect.Max.X <= LevelDimensions.X && OutLevelRect.Max.Y <= LevelDimensions.Y
		&& OutLevelRect.Width() > 0 && OutLevelRect.Height() > 0;
}

bool FArcEcotopeSiteChunkReader::TryMapBaseCoreRectToOwnedLevel(
	const FIntRect& BaseCoreRect,
	const FIntPoint& BaseDimensions,
	const uint8 Level,
	FIntRect& OutLevelRect)
{
	OutLevelRect = FIntRect();
	if (Level > FArcEcotopeSiteMetadata::MaxGridLevel || BaseDimensions.X <= 0 || BaseDimensions.Y <= 0
		|| BaseCoreRect.Min.X < 0 || BaseCoreRect.Min.Y < 0 || BaseCoreRect.Max.X > BaseDimensions.X
		|| BaseCoreRect.Max.Y > BaseDimensions.Y || BaseCoreRect.Width() <= 0 || BaseCoreRect.Height() <= 0)
	{
		return false;
	}
	const int64 Divisor = 1ll << Level;
	const FIntPoint LevelDimensions(
		static_cast<int32>((static_cast<int64>(BaseDimensions.X) + Divisor - 1) / Divisor),
		static_cast<int32>((static_cast<int64>(BaseDimensions.Y) + Divisor - 1) / Divisor));
	OutLevelRect = FIntRect(
		static_cast<int32>((static_cast<int64>(BaseCoreRect.Min.X) + Divisor - 1) / Divisor),
		static_cast<int32>((static_cast<int64>(BaseCoreRect.Min.Y) + Divisor - 1) / Divisor),
		static_cast<int32>((static_cast<int64>(BaseCoreRect.Max.X) + Divisor - 1) / Divisor),
		static_cast<int32>((static_cast<int64>(BaseCoreRect.Max.Y) + Divisor - 1) / Divisor));
	return OutLevelRect.Min.X >= 0 && OutLevelRect.Min.Y >= 0
		&& OutLevelRect.Max.X <= LevelDimensions.X && OutLevelRect.Max.Y <= LevelDimensions.Y;
}

FIntRect FArcEcotopeSiteChunkReader::MakeBaseReadRect(
	const FIntRect& CoreRect,
	const FIntPoint& GridDimensions,
	const int32 HaloCells)
{
	const auto Clamp = [](const int64 Value, const int32 Limit)
	{
		return static_cast<int32>(FMath::Clamp<int64>(Value, 0, Limit));
	};
	return FIntRect(
		Clamp(static_cast<int64>(CoreRect.Min.X) - HaloCells, GridDimensions.X),
		Clamp(static_cast<int64>(CoreRect.Min.Y) - HaloCells, GridDimensions.Y),
		Clamp(static_cast<int64>(CoreRect.Max.X) + HaloCells, GridDimensions.X),
		Clamp(static_cast<int64>(CoreRect.Max.Y) + HaloCells, GridDimensions.Y));
}
