// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Math/IntRect.h"
#include "Memory/SharedBuffer.h"

#include "ArcClimateProduct.generated.h"

UENUM(BlueprintType)
enum class EArcClimateField : uint8
{
	Temperature,
	Humidity,
	SunLoad,
	WindExposure,
	SkyViewFactor,
	Moisture,
	Count UMETA(Hidden),
};

USTRUCT(BlueprintType)
struct ARCECOTOPE_API FArcClimateProductMetadata
{
	GENERATED_BODY()

	static constexpr uint8 MaxGridLevel = 8;
	static constexpr uint8 AllFieldValidityMask =
		(1u << static_cast<uint8>(EArcClimateField::Count)) - 1u;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	int32 SchemaVersion = 6;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	FGuid ClimateVolumeId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	FTransform BakeTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	FBox LocalBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	float CellSizeCm = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	FIntPoint GridDimensions = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	int32 ChunkBaseCellSpan = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 TemperatureLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 HumidityLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 SunLoadLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 WindExposureLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 SkyViewFactorLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 MoistureLevel = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	uint8 FieldValidityMask = AllFieldValidityMask;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	bool bUseRivers = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	float RiversWeight = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	float RiversGamma = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Climate")
	float RiversChannelThreshold = 0.5f;

	static int32 GetCurrentSchemaVersion() { return 6; }
	bool IsEmpty() const { return GridDimensions.X == 0 || GridDimensions.Y == 0; }
	uint8 GetFieldLevel(EArcClimateField Field) const;
	FIntPoint GetLevelDimensions(uint8 Level) const;
	bool IsFieldValid(EArcClimateField Field) const;
	bool IsValid(FString& OutError) const;
};

USTRUCT()
struct ARCECOTOPE_API FArcClimateFieldBlockDescriptor
{
	GENERATED_BODY()

	UPROPERTY()
	EArcClimateField Field = EArcClimateField::Temperature;

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
struct ARCECOTOPE_API FArcClimateChunkManifestEntry
{
	GENERATED_BODY()

	UPROPERTY()
	FIntPoint ChunkId = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint BaseCoreMin = FIntPoint::ZeroValue;

	UPROPERTY()
	FIntPoint BaseCoreMax = FIntPoint::ZeroValue;

	UPROPERTY()
	int32 PayloadIndex = INDEX_NONE;

	UPROPERTY()
	uint64 PayloadSize = 0;

	UPROPERTY()
	uint64 PayloadHash = 0;

	UPROPERTY()
	TArray<FArcClimateFieldBlockDescriptor> Fields;
};

USTRUCT()
struct ARCECOTOPE_API FArcClimateChunkManifest
{
	GENERATED_BODY()

	UPROPERTY()
	int32 FormatVersion = 1;

	UPROPERTY()
	int32 CoreBaseCellSpan = 0;

	UPROPERTY()
	FIntPoint ChunkGridDimensions = FIntPoint::ZeroValue;

	UPROPERTY()
	uint64 ManifestHash = 0;

	UPROPERTY()
	TArray<FArcClimateChunkManifestEntry> Chunks;

	bool IsEmpty() const { return Chunks.IsEmpty(); }
	void Reset() { *this = FArcClimateChunkManifest(); }
};

struct FArcClimateProductBuildWriterState;

class ARCECOTOPE_API FArcClimateProductBuildWriter
{
public:
	FArcClimateProductBuildWriter();
	~FArcClimateProductBuildWriter();

	FArcClimateProductBuildWriter(const FArcClimateProductBuildWriter&) = delete;
	FArcClimateProductBuildWriter& operator=(const FArcClimateProductBuildWriter&) = delete;

	bool Begin(
		const FArcClimateProductMetadata& ProvisionalMetadata,
		int32 CoreBaseCellSpan,
		FString& OutError);

	bool WriteFieldBlock(
		EArcClimateField Field,
		FIntPoint ChunkId,
		TConstArrayView<float> Values,
		FString& OutError);

	bool Finalize(
		FArcClimateProductMetadata& OutMetadata,
		FArcClimateChunkManifest& OutManifest,
		TArray<FSharedBuffer>& OutPayloads,
		FString& OutError);

private:
	TUniquePtr<FArcClimateProductBuildWriterState> State;
};

class ARCECOTOPE_API FArcClimateProductStorage
{
public:
	static constexpr int32 CurrentFormatVersion = 1;
	static constexpr int32 MaxClimateChunkCount = 64 * 1024;
	static constexpr uint64 MaxLogicalCells = static_cast<uint64>(MAX_int32);
	static constexpr uint64 MaxPayloadBytes = static_cast<uint64>(MAX_int32);
	static constexpr uint64 MaxTotalPayloadBytes = static_cast<uint64>(MAX_int32);

	static uint64 CalculateManifestHash(const FArcClimateChunkManifest& Manifest);

	static bool ValidateManifestMetadata(
		const FArcClimateProductMetadata& Metadata,
		const FArcClimateChunkManifest& Manifest,
		TConstArrayView<int64> PayloadSizes,
		FString& OutError);

	static bool ValidatePayloadBytes(
		const FArcClimateChunkManifestEntry& Entry,
		const FSharedBuffer& Payload,
		FString& OutError);
};

class ARCECOTOPE_API FArcClimateFrozenSource final
{
public:
	FArcClimateProductMetadata Metadata;
	FArcClimateChunkManifest Manifest;
	TArray<FSharedBuffer> Payloads;
};

using FArcClimateFrozenSourcePtr = TSharedPtr<const FArcClimateFrozenSource, ESPMode::ThreadSafe>;

class ARCECOTOPE_API FArcClimateProductReader
{
public:
	bool Open(FArcClimateFrozenSourcePtr Source, FString& OutError);
	void Close();
	bool IsOpen() const { return Source.IsValid(); }

	bool SampleAtWorldPosition(
		const FVector& WorldPosition,
		EArcClimateField Field,
		float& OutValue,
		bool& bOutFieldValid,
		FString& OutError) const;

	bool ValidateAll(FString& OutError) const;

private:
	FArcClimateFrozenSourcePtr Source;
};
