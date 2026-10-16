// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/IntRect.h"
#include "Memory/SharedBuffer.h"

#include "ArcEcotopeSiteChunkStorage.generated.h"

struct FArcEcotopeSiteMetadata;
class FArcEcotopeChunkLayout;
struct FArcEcotopeChunkId;
struct FArcEcotopeSiteChunkBuildWriterState;

/** Identifies one logical field in a site snapshot and its packed chunk payloads. */
UENUM()
enum class EArcEcotopeSiteField : uint8
{
	LocalSurfaceZ,
	ValidSurface,
	Slope,
	Aspect,
	Moisture,
	Fertility,
	SunLoad,
	WindExposure,
	SkyViewFactor,
	Temperature,
	ModificationCut,
	RecordValid,
	BareRock,
	ChannelExclusion,
	ChannelBuffer,
	EstablishmentWear,
	SubstrateMobility,
	Count UMETA(Hidden),
};

UENUM()
enum class EArcEcotopeSiteValueType : uint8
{
	UInt8,
	Float32,
};

USTRUCT()
struct ARCECOTOPE_API FArcEcotopeSiteFieldBlockDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	EArcEcotopeSiteField Field = EArcEcotopeSiteField::LocalSurfaceZ;

	UPROPERTY()
	EArcEcotopeSiteValueType ValueType = EArcEcotopeSiteValueType::Float32;

	UPROPERTY()
	uint8 Level = 0;

	UPROPERTY()
	FIntPoint RectMin = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint RectMax = FIntPoint::ZeroValue;

	UPROPERTY()
	uint64 ByteOffset = 0;

	UPROPERTY()
	uint64 ByteSize = 0;

	UPROPERTY()
	uint64 DataHash = 0;
};

USTRUCT()
struct ARCECOTOPE_API FArcEcotopeSiteChunkManifestEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint ChunkId = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint BaseCoreMin = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint BaseCoreMax = FIntPoint::ZeroValue;

	UPROPERTY()
	uint32 FieldMask = 0;

	UPROPERTY()
	int32 PayloadIndex = INDEX_NONE;

	UPROPERTY()
	uint64 PayloadSize = 0;

	UPROPERTY()
	uint64 PayloadHash = 0;

	UPROPERTY()
	TArray<FArcEcotopeSiteFieldBlockDescriptor> Fields;
};

USTRUCT()
struct ARCECOTOPE_API FArcEcotopeSiteChunkManifest
{
	GENERATED_BODY()

	UPROPERTY()
	int32 FormatVersion = 0;

	UPROPERTY()
	int32 CoreBaseCellSpan = 0;

	UPROPERTY()
	FIntPoint ChunkGridDimensions = FIntPoint::ZeroValue;

	UPROPERTY()
	uint64 SourceSiteSnapshotHash = 0;

	UPROPERTY()
	uint64 LogicalFieldHash = 0;

	UPROPERTY()
	uint64 ManifestHash = 0;

	UPROPERTY()
	TArray<FArcEcotopeSiteChunkManifestEntry> Chunks;

	bool IsEmpty() const { return Chunks.IsEmpty(); }
	void Reset() { *this = FArcEcotopeSiteChunkManifest(); }
};

/**
 * Incrementally writes the exact owned field rectangle for every chunk/field pair.
 *
 * Begin preallocates the final payloads from immutable metadata and layout. Writes
 * are deliberately narrow: a caller cannot append, overlap, or redirect field
 * data. Finalize publishes nothing until every required pair has been supplied
 * and all three storage identities have been recomputed from the written bytes.
 */
class ARCECOTOPE_API FArcEcotopeSiteChunkBuildWriter
{
public:
	FArcEcotopeSiteChunkBuildWriter();
	~FArcEcotopeSiteChunkBuildWriter();

	FArcEcotopeSiteChunkBuildWriter(const FArcEcotopeSiteChunkBuildWriter&) = delete;
	FArcEcotopeSiteChunkBuildWriter& operator=(const FArcEcotopeSiteChunkBuildWriter&) = delete;

	/** Starts one build from scalar metadata whose SiteSnapshotHash and LogicalFieldHash are provisional. */
	bool Begin(
		const FArcEcotopeSiteMetadata& ProvisionalMetadata,
		const FArcEcotopeChunkLayout& FinalLayout,
		FString& OutError);

	/** Writes exactly the owned rectangle for one Float32 field. Values are tightly row-major. */
	bool WriteFloatField(
		const FArcEcotopeChunkId& ChunkId,
		EArcEcotopeSiteField Field,
		uint8 Level,
		const FIntRect& OwnedRect,
		TConstArrayView<float> Values,
		FString& OutError);

	/** Writes exactly the owned rectangle for one UInt8 field. Values are tightly row-major. */
	bool WriteByteField(
		const FArcEcotopeChunkId& ChunkId,
		EArcEcotopeSiteField Field,
		uint8 Level,
		const FIntRect& OwnedRect,
		TConstArrayView<uint8> Values,
		FString& OutError);

	/** Reports live build storage for bake peak-memory diagnostics. */
	void GetAllocatedBytes(uint64& OutPayloadBytes, uint64& OutOverheadBytes) const;

	/**
	 * Completes the build atomically. FinalMetadata may update only finalization-time scalars
	 * (currently the two horizon validity flags); all layout-dependent scalars must exactly match
	 * Begin's provisional metadata. On failure every output is left unchanged.
	 * OutMetadata receives the calculated SiteSnapshotHash and LogicalFieldHash.
	 */
	bool Finalize(
		const FArcEcotopeSiteMetadata& FinalMetadata,
		FArcEcotopeSiteMetadata& OutMetadata,
		FArcEcotopeSiteChunkManifest& OutManifest,
		TArray<FSharedBuffer>& OutPayloads,
		FString& OutError);

private:
	TUniquePtr<FArcEcotopeSiteChunkBuildWriterState> State;
};

/** Validates the authoritative editor-only chunk storage. */
class ARCECOTOPE_API FArcEcotopeSiteChunkStorage
{
public:
	static constexpr int32 CurrentFormatVersion = 1;
	static constexpr uint32 PayloadMagic = 0x43534641u; // 'AFSC' in little-endian bytes.
	static constexpr uint16 PayloadHeaderBytes = 32;

	/** Structural validation against authoritative scalar metadata; does not load bulk payload bytes. */
	static bool ValidateManifestMetadata(
		const FArcEcotopeSiteMetadata& Metadata,
		const FArcEcotopeSiteChunkManifest& Manifest,
		TConstArrayView<int64> PayloadSizes,
		FString& OutError);

	/** Validates one loaded immutable payload against its manifest entry. */
	static bool ValidatePayloadBytes(
		const FArcEcotopeSiteChunkManifestEntry& Entry,
		const FSharedBuffer& Payload,
		FString& OutError);

	static uint64 CalculateManifestHash(const FArcEcotopeSiteChunkManifest& Manifest);
};
