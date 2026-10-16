// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Containers/BitArray.h"
#include "Math/IntRect.h"

#include "Simulation/ArcEcotopeChunkLayout.h"

struct FArcForestFrozenSpeciesSetup;
struct FArcEcotopeSiteMetadata;
class FArcForestCancellationToken;
class FArcEcotopeSiteChunkReader;

/** Immutable, core-local static habitat inputs loaded from the transient factor backing file. */
struct ARCECOTOPE_API FArcForestHabitatFactorChunk
{
	FArcEcotopeChunkId Id;
	FIntRect CoreRect;
	int32 SpeciesCount = 0;
	int32 CoreCellCount = 0;
	/** Species-major: SpeciesIndex * GetCoreCellCount() + CoreLocalCellIndex. */
	TArray<float> StaticProducts;
	TBitArray<> StaticHardZeros;

	int32 GetCoreCellCount() const { return CoreCellCount; }
	int64 GetAllocatedSize() const;
};

struct ARCECOTOPE_API FArcForestHabitatFactorStoreStats
{
	int64 BuiltSamples = 0;
	int64 HardZeroSamples = 0;
	int64 HardZeroSlopeSamples = 0;
	int64 HardZeroSunLoadSamples = 0;
	int64 HardZeroWindSamples = 0;
	int64 HardZeroFertilitySamples = 0;
	int64 HardZeroTemperatureSamples = 0;
	int64 HardZeroMoistureSamples = 0;
	int64 ProductBytes = 0;
	int64 HardZeroBytes = 0;
	int64 BackingBytes = 0;
	int64 CacheBudgetBytes = 0;
	int64 ResidentBytes = 0;
	int64 PinnedBytes = 0;
	int64 ScratchBytes = 0;
	int64 PeakResidentBytes = 0;
	int64 PeakPinnedBytes = 0;
	int64 PeakScratchBytes = 0;
	/** Peak factor-store arrays plus packed-bit load scratch; excludes opaque archive/source-reader allocations. */
	int64 PeakAccountedPayloadBytes = 0;
	uint64 AcquireCount = 0;
	uint64 FailedAcquireCount = 0;
	uint64 CacheHitCount = 0;
	uint64 CacheMissCount = 0;
	uint64 PayloadLoadCount = 0;
	uint64 EvictionCount = 0;
	uint64 BackingReaderOpenAttemptCount = 0;
	uint64 BackingReaderOpenSuccessCount = 0;
	uint64 BackingReaderOpenFailureCount = 0;
	uint64 BackingReaderInvalidationCount = 0;
	int32 ActiveLeases = 0;
	int32 PeakActiveLeases = 0;
	int32 MaxActiveLeases = 0;
	double BuildMilliseconds = 0.0;
	double LoadMilliseconds = 0.0;
	double BackingReaderOpenMilliseconds = 0.0;
};

struct FArcForestHabitatFactorLeaseData;
struct FArcForestHabitatFactorStoreState;

/** Move-only lifetime token. A chunk reference returned by GetChunk is valid only while this lease lives. */
class ARCECOTOPE_API FArcForestHabitatFactorLease
{
public:
	FArcForestHabitatFactorLease() = default;
	FArcForestHabitatFactorLease(FArcForestHabitatFactorLease&&) = default;
	FArcForestHabitatFactorLease& operator=(FArcForestHabitatFactorLease&&) = default;
	FArcForestHabitatFactorLease(const FArcForestHabitatFactorLease&) = delete;
	FArcForestHabitatFactorLease& operator=(const FArcForestHabitatFactorLease&) = delete;

	bool IsValid() const { return Data.IsValid(); }
	const FArcForestHabitatFactorChunk& GetChunk() const;
	void Reset() { Data.Reset(); }

private:
	friend class FArcForestHabitatFactorStore;
	TSharedPtr<const FArcForestHabitatFactorLeaseData, ESPMode::ThreadSafe> Data;
};

/** Fixed-budget cache over a versioned, run-local static habitat factor file. */
class ARCECOTOPE_API FArcForestHabitatFactorStore
{
public:
	FArcForestHabitatFactorStore() = default;
	~FArcForestHabitatFactorStore();
	FArcForestHabitatFactorStore(FArcForestHabitatFactorStore&&) = default;
	FArcForestHabitatFactorStore& operator=(FArcForestHabitatFactorStore&&) = default;
	FArcForestHabitatFactorStore(const FArcForestHabitatFactorStore&) = delete;
	FArcForestHabitatFactorStore& operator=(const FArcForestHabitatFactorStore&) = delete;

	bool Build(
		const FArcEcotopeSiteMetadata& Site,
		TConstArrayView<FArcForestFrozenSpeciesSetup> Species,
		const FArcEcotopeChunkLayout& Layout,
		const FArcEcotopeSiteChunkReader& SiteReader,
		int64 CacheBudgetBytes,
		int32 MaxActiveLeases,
		const FArcForestCancellationToken& Cancellation,
		FString& OutError);

	bool AcquireChunk(
		const FArcEcotopeChunkId& Id,
		FArcForestHabitatFactorLease& OutLease,
		FString& OutError) const;
	/** Conservative fixed-budget chunk count that can be pinned while the next payload is unpacked. */
	int32 GetMaximumBatchChunkCount() const;

	void Release(FArcForestHabitatFactorLease& Lease) const { Lease.Reset(); }
	void Reset();
	bool IsBuilt() const { return State.IsValid(); }
	FArcForestHabitatFactorStoreStats GetStats() const;

private:
	TSharedPtr<FArcForestHabitatFactorStoreState, ESPMode::ThreadSafe> State;
};
