// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/IntRect.h"

#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcEcotopeFrozenSiteSource.h"
#include "Simulation/ArcEcotopeSiteChunkStorage.h"

class FArcForestCancellationToken;

/**
 * Immutable two-dimensional window into one site field.
 *
 * Values begins at GlobalRect.Min. A row may be followed by data outside the
 * rectangle, so callers must use RowStride rather than GlobalRect.Width().
 */
template <typename ValueType>
struct TArcEcotopeSiteFieldView
{
	FIntRect GlobalRect;
	FIntPoint GlobalDimensions = FIntPoint::ZeroValue;
	TConstArrayView<ValueType> Values;
	int32 RowStride = 0;
	uint8 Level = 0;

	bool IsValid() const
	{
		const int64 RequiredValues = static_cast<int64>(GlobalRect.Height() - 1) * RowStride
			+ GlobalRect.Width();
		return GlobalRect.Min.X >= 0 && GlobalRect.Min.Y >= 0
			&& GlobalRect.Max.X <= GlobalDimensions.X && GlobalRect.Max.Y <= GlobalDimensions.Y
			&& GlobalRect.Width() > 0 && GlobalRect.Height() > 0
			&& RowStride >= GlobalRect.Width()
			&& RequiredValues > 0 && RequiredValues <= Values.Num();
	}

	const ValueType* TryGetRow(const int32 GlobalY) const
	{
		if (!IsValid() || GlobalY < GlobalRect.Min.Y || GlobalY >= GlobalRect.Max.Y)
		{
			return nullptr;
		}
		return Values.GetData() + (GlobalY - GlobalRect.Min.Y) * RowStride;
	}

	const ValueType* TryGet(const int32 GlobalX, const int32 GlobalY) const
	{
		const ValueType* Row = TryGetRow(GlobalY);
		return Row != nullptr && GlobalX >= GlobalRect.Min.X && GlobalX < GlobalRect.Max.X
			? Row + (GlobalX - GlobalRect.Min.X)
			: nullptr;
	}
};

using FArcEcotopeSiteFloatFieldView = TArcEcotopeSiteFieldView<float>;
using FArcEcotopeSiteByteFieldView = TArcEcotopeSiteFieldView<uint8>;

/** All immutable site fields needed for one base-core plus requested halo. */
struct ARCECOTOPE_API FArcEcotopeSiteChunkView
{
	uint32 FieldMask = 0;
	FArcEcotopeChunkId Id;
	FIntRect BaseCoreRect;
	FIntRect BaseReadRect;

	FArcEcotopeSiteFloatFieldView LocalSurfaceZ;
	FArcEcotopeSiteByteFieldView ValidSurface;
	FArcEcotopeSiteFloatFieldView Slope;
	FArcEcotopeSiteFloatFieldView Aspect;
	FArcEcotopeSiteFloatFieldView Moisture;
	FArcEcotopeSiteFloatFieldView Fertility;
	FArcEcotopeSiteFloatFieldView SunLoad;
	FArcEcotopeSiteFloatFieldView WindExposure;
	FArcEcotopeSiteFloatFieldView SkyViewFactor;
	FArcEcotopeSiteFloatFieldView Temperature;
	FArcEcotopeSiteFloatFieldView ModificationCut;
	FArcEcotopeSiteByteFieldView RecordValid;
	FArcEcotopeSiteFloatFieldView BareRock;
	FArcEcotopeSiteFloatFieldView ChannelExclusion;
	FArcEcotopeSiteFloatFieldView ChannelBuffer;
	FArcEcotopeSiteFloatFieldView EstablishmentWear;
	FArcEcotopeSiteFloatFieldView SubstrateMobility;

	const FArcEcotopeSiteFloatFieldView* FindFloatField(EArcEcotopeSiteField Field) const;
	const FArcEcotopeSiteByteFieldView* FindByteField(EArcEcotopeSiteField Field) const;
};

struct FArcEcotopeSiteChunkLeaseData;
struct FArcEcotopeSiteChunkReaderState;

/** Move-only lifetime token for one acquired chunk view. */
class ARCECOTOPE_API FArcEcotopeSiteChunkLease
{
public:
	FArcEcotopeSiteChunkLease() = default;
	FArcEcotopeSiteChunkLease(FArcEcotopeSiteChunkLease&&) = default;
	FArcEcotopeSiteChunkLease& operator=(FArcEcotopeSiteChunkLease&&) = default;
	FArcEcotopeSiteChunkLease(const FArcEcotopeSiteChunkLease&) = delete;
	FArcEcotopeSiteChunkLease& operator=(const FArcEcotopeSiteChunkLease&) = delete;

	bool IsValid() const { return Data.IsValid(); }
	const FArcEcotopeSiteChunkView& GetView() const;
	void Reset() { Data.Reset(); }

private:
	friend class FArcEcotopeSiteChunkReader;
	TSharedPtr<const FArcEcotopeSiteChunkLeaseData, ESPMode::ThreadSafe> Data;
};

#if WITH_EDITORONLY_DATA
struct ARCECOTOPE_API FArcEcotopeSiteBulkReaderStats
{
	uint64 AcquireCount = 0;
	uint64 FailedAcquireCount = 0;
	uint64 CacheHitCount = 0;
	uint64 CacheMissCount = 0;
	uint64 CoalescedLoadWaitCount = 0;
	uint64 PayloadLoadCount = 0;
	uint64 EvictionCount = 0;
	uint64 LoadedBytes = 0;
	uint64 StitchedBytes = 0;
	int64 ResidentBytes = 0;
	int64 PinnedBytes = 0;
	int64 ScratchBytes = 0;
	int64 PeakResidentBytes = 0;
	int64 PeakPinnedBytes = 0;
	int64 PeakScratchBytes = 0;
	int64 PeakTotalBytes = 0;
	int64 BudgetBytes = 0;
	int64 SourceLoadedBytes = 0;
	int64 SourceMemoryOnlyBytes = 0;
	double PayloadLoadMilliseconds = 0.0;
	double StitchMilliseconds = 0.0;
};

struct ARCECOTOPE_API FArcEcotopeSiteBulkValidationResult
{
	uint64 CanonicalHash = 0;
	uint64 LogicalFieldHash = 0;
	int64 LogicalValueCount = 0;
	int64 AcquireCount = 0;
	int64 PeakValidationScratchBytes = 0;
};
#endif

/**
 * Reader over an immutable frozen chunk source. Reader state and leases retain
 * that source, so reads do not depend on caller-owned storage.
 */
class ARCECOTOPE_API FArcEcotopeSiteChunkReader
{
public:
	static constexpr uint32 AllFieldMask =
		(1u << static_cast<uint32>(EArcEcotopeSiteField::Count)) - 1u;

	bool OpenFrozen(
		FArcEcotopeFrozenSiteSourcePtr Source,
		int64 CacheBudgetBytes,
		FString& OutError);

#if WITH_EDITORONLY_DATA
	bool OpenEditorBulk(
		FArcEcotopeFrozenSiteSourcePtr Source,
		int64 CacheBudgetBytes,
		FString& OutError);

	FArcEcotopeSiteBulkReaderStats GetBulkStats() const;
	void LogBulkStats(const TCHAR* Context) const;

	/** Validates the immutable chunked source and opens OutReader on success. */
	static bool ValidateEditorBulkSource(
		FArcEcotopeFrozenSiteSourcePtr Source,
		int64 CacheBudgetBytes,
		FArcEcotopeSiteChunkReader& OutReader,
		FArcEcotopeSiteBulkValidationResult& OutResult,
		FArcEcotopeSiteBulkReaderStats& OutStats,
		FString& OutError,
		const FArcForestCancellationToken* Cancellation = nullptr);

#endif

	/**
	 * Acquires every field over the chunk core plus an arbitrary base-grid halo.
	 * Fields crossing payload boundaries are stitched into lease-owned storage;
	 * pins and stitching storage are charged to the same budget.
	 */
	bool Acquire(
		const FArcEcotopeChunkId& Id,
		int32 BaseHaloCells,
		FArcEcotopeSiteChunkLease& OutLease,
		FString& OutError) const;

	/** Acquires only the requested fields, reducing bulk stitching and scratch storage. */
	bool AcquireFields(
		const FArcEcotopeChunkId& Id,
		int32 BaseHaloCells,
		uint32 FieldMask,
		FArcEcotopeSiteChunkLease& OutLease,
		FString& OutError) const;

	void Close();
	bool IsOpen() const;
	int64 GetCacheBudgetBytes() const;

	/** Maps a base-grid read rectangle to every level cell intersecting it. */
	static bool TryMapBaseReadRectToLevel(
		const FIntRect& BaseRect,
		const FIntPoint& BaseDimensions,
		uint8 Level,
		FIntRect& OutLevelRect);

	/**
	 * Maps base-core ownership to a disjoint level-cell ownership rectangle.
	 * A level cell belongs to the chunk containing its minimum-corner base child.
	 */
	static bool TryMapBaseCoreRectToOwnedLevel(
		const FIntRect& BaseCoreRect,
		const FIntPoint& BaseDimensions,
		uint8 Level,
		FIntRect& OutLevelRect);

	/** Expands a base-grid core rectangle by a halo and clamps it to the base grid. */
	static FIntRect MakeBaseReadRect(
		const FIntRect& CoreRect,
		const FIntPoint& GridDimensions,
		int32 HaloCells);

private:
	/** Leases retain the reader state, which in turn owns the frozen source. */
	TSharedPtr<const FArcEcotopeSiteChunkReaderState, ESPMode::ThreadSafe> State;
};
