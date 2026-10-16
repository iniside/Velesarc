// Copyright Lukasz Baran. All Rights Reserved.

#include "Simulation/ArcForestHabitatFactorStore.h"

#include "ArcEcotopeModule.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Serialization/Archive.h"
#include "Simulation/ArcForestGrowthEngine.h"
#include "Simulation/ArcForestHabitatResponse.h"
#include "Simulation/ArcEcotopeSiteChunkReader.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"

namespace ArcForestHabitatFactorStorePrivate
{
	constexpr uint64 BackingMagic = 0x4152464846414354ull; // ARFHFACT
	constexpr uint32 BackingVersion = 5;
	constexpr uint32 ChunkMagic = 0x48464348u; // HFCH

	struct FSpeciesIdentity
	{
		FGuid SpeciesId;
		uint64 EcologyHash = 0;
	};

	struct FChunkIndexEntry
	{
		FArcEcotopeChunkId Id;
		FIntRect CoreRect;
		int32 CoreCellCount = 0;
		int32 ProductCount = 0;
		int32 HardZeroByteCount = 0;
		int64 ResidentAllocationBytes = 0;
		int64 PackedHardZeroScratchBytes = 0;
		uint32 PayloadCrc = 0;
		int64 Offset = 0;
		int64 Size = 0;
	};

	struct FCacheEntry
	{
		TSharedPtr<FArcForestHabitatFactorChunk, ESPMode::ThreadSafe> Chunk;
		int32 PinCount = 0;
		uint64 LastUseSerial = 0;
	};

	void SerializeChunkId(FArchive& Archive, FArcEcotopeChunkId& Id)
	{
		Archive << Id.X;
		Archive << Id.Y;
	}

	void SerializeChunkEntry(FArchive& Archive, FChunkIndexEntry& Entry)
	{
		SerializeChunkId(Archive, Entry.Id);
		Archive << Entry.CoreRect;
		Archive << Entry.CoreCellCount;
		Archive << Entry.ProductCount;
		Archive << Entry.HardZeroByteCount;
		Archive << Entry.ResidentAllocationBytes;
		Archive << Entry.PackedHardZeroScratchBytes;
		Archive << Entry.PayloadCrc;
		Archive << Entry.Offset;
		Archive << Entry.Size;
	}

	uint32 CalculatePayloadCrc(
		const TConstArrayView<float> StaticProducts,
		const TConstArrayView<uint8> HardZeros)
	{
		uint32 Crc = FCrc::MemCrc32(StaticProducts.GetData(), StaticProducts.Num() * sizeof(float));
		return FCrc::MemCrc32(HardZeros.GetData(), HardZeros.Num(), Crc);
	}

	bool ValidateBackingHeader(
		const FString& BackingPath,
		const int64 ExpectedFileSize,
		const uint64 ExpectedSiteSnapshotHash,
		const FArcEcotopeChunkLayout& ExpectedLayout,
		const TConstArrayView<FSpeciesIdentity> ExpectedSpecies,
		const TConstArrayView<FChunkIndexEntry> ExpectedChunks,
		FString& OutError)
	{
		if (IFileManager::Get().FileSize(*BackingPath) != ExpectedFileSize)
		{
			OutError = TEXT("ArcForest habitat factor store backing file size is inconsistent.");
			return false;
		}
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*BackingPath));
		if (!Reader.IsValid())
		{
			OutError = TEXT("ArcForest habitat factor store could not reopen its backing header.");
			return false;
		}
		uint64 Magic = 0;
		uint32 Version = 0;
		uint64 SiteSnapshotHash = 0;
		FIntPoint BaseDimensions;
		FIntPoint RecordDimensions;
		FIntPoint ChunkDimensions;
		int32 CoreBaseCellSpan = 0;
		int32 CoreRecordCellSpan = 0;
		int32 RecordHaloCells = 0;
		int32 SpeciesCount = 0;
		int32 ChunkCount = 0;
		*Reader << Magic;
		*Reader << Version;
		*Reader << SiteSnapshotHash;
		*Reader << BaseDimensions;
		*Reader << RecordDimensions;
		*Reader << ChunkDimensions;
		*Reader << CoreBaseCellSpan;
		*Reader << CoreRecordCellSpan;
		*Reader << RecordHaloCells;
		*Reader << SpeciesCount;
		*Reader << ChunkCount;
		if (Reader->IsError()
			|| Magic != BackingMagic
			|| Version != BackingVersion
			|| SiteSnapshotHash != ExpectedSiteSnapshotHash
			|| BaseDimensions != ExpectedLayout.GetBaseDimensions()
			|| RecordDimensions != ExpectedLayout.GetRecordDimensions()
			|| ChunkDimensions != ExpectedLayout.GetChunkGridDimensions()
			|| CoreBaseCellSpan != ExpectedLayout.GetCoreBaseCellSpan()
			|| CoreRecordCellSpan != ExpectedLayout.GetCoreRecordCellSpan()
			|| RecordHaloCells != ExpectedLayout.GetRecordHaloCells()
			|| SpeciesCount != ExpectedSpecies.Num()
			|| ChunkCount != ExpectedChunks.Num())
		{
			OutError = TEXT("ArcForest habitat factor store backing header identity is inconsistent.");
			return false;
		}
		for (const FSpeciesIdentity& Expected : ExpectedSpecies)
		{
			FSpeciesIdentity Stored;
			*Reader << Stored.SpeciesId;
			*Reader << Stored.EcologyHash;
			if (Stored.SpeciesId != Expected.SpeciesId || Stored.EcologyHash != Expected.EcologyHash)
			{
				OutError = TEXT("ArcForest habitat factor store species identity is inconsistent.");
				return false;
			}
		}
		for (const FChunkIndexEntry& Expected : ExpectedChunks)
		{
			FChunkIndexEntry Stored;
			SerializeChunkEntry(*Reader, Stored);
			if (Stored.Id != Expected.Id || Stored.CoreRect != Expected.CoreRect
				|| Stored.CoreCellCount != Expected.CoreCellCount
				|| Stored.ProductCount != Expected.ProductCount
				|| Stored.HardZeroByteCount != Expected.HardZeroByteCount
				|| Stored.ResidentAllocationBytes != Expected.ResidentAllocationBytes
				|| Stored.PackedHardZeroScratchBytes != Expected.PackedHardZeroScratchBytes
				|| Stored.PayloadCrc != Expected.PayloadCrc
				|| Stored.Offset != Expected.Offset || Stored.Size != Expected.Size)
			{
				OutError = TEXT("ArcForest habitat factor store chunk index is inconsistent.");
				return false;
			}
		}
		return !Reader->IsError();
	}

	void SerializeHeader(
		FArchive& Archive,
		uint64 SiteSnapshotHash,
		const FArcEcotopeChunkLayout& Layout,
		TArray<FSpeciesIdentity>& Species,
		TArray<FChunkIndexEntry>& Chunks)
	{
		uint64 Magic = BackingMagic;
		uint32 Version = BackingVersion;
		Archive << Magic;
		Archive << Version;
		Archive << SiteSnapshotHash;
		FIntPoint BaseDimensions = Layout.GetBaseDimensions();
		FIntPoint RecordDimensions = Layout.GetRecordDimensions();
		FIntPoint ChunkDimensions = Layout.GetChunkGridDimensions();
		int32 CoreBaseCellSpan = Layout.GetCoreBaseCellSpan();
		int32 CoreRecordCellSpan = Layout.GetCoreRecordCellSpan();
		int32 RecordHaloCells = Layout.GetRecordHaloCells();
		int32 SpeciesCount = Species.Num();
		int32 ChunkCount = Chunks.Num();
		Archive << BaseDimensions;
		Archive << RecordDimensions;
		Archive << ChunkDimensions;
		Archive << CoreBaseCellSpan;
		Archive << CoreRecordCellSpan;
		Archive << RecordHaloCells;
		Archive << SpeciesCount;
		Archive << ChunkCount;
		for (FSpeciesIdentity& Identity : Species)
		{
			Archive << Identity.SpeciesId;
			Archive << Identity.EcologyHash;
		}
		for (FChunkIndexEntry& Entry : Chunks)
		{
			SerializeChunkEntry(Archive, Entry);
		}
	}

	bool TryMultiplyPositive(const int64 Left, const int64 Right, int64& OutValue)
	{
		OutValue = 0;
		if (Left <= 0 || Right <= 0 || Left > MAX_int64 / Right)
		{
			return false;
		}
		OutValue = Left * Right;
		return true;
	}

	int64 GetChunkAllocatedSize(const FArcForestHabitatFactorChunk& Chunk)
	{
		return static_cast<int64>(Chunk.StaticProducts.GetAllocatedSize())
			+ static_cast<int64>(Chunk.StaticHardZeros.GetAllocatedSize());
	}

	void UpdateMemoryPeaks(FArcForestHabitatFactorStoreStats& Stats)
	{
		Stats.PeakResidentBytes = FMath::Max(Stats.PeakResidentBytes, Stats.ResidentBytes);
		Stats.PeakPinnedBytes = FMath::Max(Stats.PeakPinnedBytes, Stats.PinnedBytes);
		Stats.PeakScratchBytes = FMath::Max(Stats.PeakScratchBytes, Stats.ScratchBytes);
		Stats.PeakAccountedPayloadBytes = FMath::Max(
			Stats.PeakAccountedPayloadBytes,
			Stats.ResidentBytes + Stats.ScratchBytes);
	}
}

struct FArcForestHabitatFactorStoreState
{
	using FChunkIndexEntry = ArcForestHabitatFactorStorePrivate::FChunkIndexEntry;
	using FCacheEntry = ArcForestHabitatFactorStorePrivate::FCacheEntry;

	~FArcForestHabitatFactorStoreState()
	{
		// FArchive members are destroyed after the destructor body. Close explicitly before
		// deleting because a Windows read handle may not have delete sharing enabled.
		BackingReader.Reset();
		Cache.Empty();
		if (!BackingPath.IsEmpty())
		{
			IFileManager::Get().Delete(*BackingPath, false, true, true);
		}
	}

	void ReleaseLease(const int32 ReleasedChunkIndex, const int64 ChunkBytes)
	{
		FScopeLock Lock(&Mutex);
		if (Cache.IsValidIndex(ReleasedChunkIndex) && Cache[ReleasedChunkIndex].PinCount > 0)
		{
			--Cache[ReleasedChunkIndex].PinCount;
			if (Cache[ReleasedChunkIndex].PinCount == 0)
			{
				Stats.PinnedBytes = FMath::Max<int64>(Stats.PinnedBytes - ChunkBytes, 0);
			}
		}
		Stats.ActiveLeases = FMath::Max(Stats.ActiveLeases - 1, 0);
	}

	void InvalidateBackingReader()
	{
		if (BackingReader.IsValid())
		{
			BackingReader.Reset();
			++Stats.BackingReaderInvalidationCount;
		}
	}

	bool EnsureBackingReader(FString& OutError)
	{
		if (BackingReader.IsValid() && !BackingReader->IsError())
		{
			return true;
		}
		if (BackingReader.IsValid())
		{
			InvalidateBackingReader();
		}

		++Stats.BackingReaderOpenAttemptCount;
		const double OpenStartSeconds = FPlatformTime::Seconds();
		TUniquePtr<FArchive> NewReader(IFileManager::Get().CreateFileReader(*BackingPath));
		Stats.BackingReaderOpenMilliseconds +=
			(FPlatformTime::Seconds() - OpenStartSeconds) * 1000.0;
		if (!NewReader.IsValid())
		{
			++Stats.BackingReaderOpenFailureCount;
			OutError = TEXT("ArcForest habitat factor store could not open its backing file.");
			return false;
		}

		++Stats.BackingReaderOpenSuccessCount;
		BackingReader = MoveTemp(NewReader);
		if (BackingReader->IsError() || BackingReader->TotalSize() != Stats.BackingBytes)
		{
			InvalidateBackingReader();
			OutError = TEXT("ArcForest habitat factor store backing file size changed after validation.");
			return false;
		}
		return true;
	}

	mutable FCriticalSection Mutex;
	FString BackingPath;
	TUniquePtr<FArchive> BackingReader;
	TArray<FChunkIndexEntry> ChunkIndex;
	TArray<FCacheEntry> Cache;
	int32 ChunkGridWidth = 0;
	int32 MaximumBatchChunkCount = 0;
	mutable FArcForestHabitatFactorStoreStats Stats;
	mutable uint64 UseSerial = 0;
};

struct FArcForestHabitatFactorLeaseData
{
	~FArcForestHabitatFactorLeaseData()
	{
		if (State.IsValid() && Chunk.IsValid())
		{
			State->ReleaseLease(ChunkIndex, Chunk->GetAllocatedSize());
		}
	}

	TSharedPtr<FArcForestHabitatFactorStoreState, ESPMode::ThreadSafe> State;
	TSharedPtr<const FArcForestHabitatFactorChunk, ESPMode::ThreadSafe> Chunk;
	int32 ChunkIndex = INDEX_NONE;
};

int64 FArcForestHabitatFactorChunk::GetAllocatedSize() const
{
	return ArcForestHabitatFactorStorePrivate::GetChunkAllocatedSize(*this);
}

const FArcForestHabitatFactorChunk& FArcForestHabitatFactorLease::GetChunk() const
{
	check(Data.IsValid() && Data->Chunk.IsValid());
	return *Data->Chunk;
}

FArcForestHabitatFactorStore::~FArcForestHabitatFactorStore()
{
	Reset();
}

bool FArcForestHabitatFactorStore::Build(
	const FArcEcotopeSiteMetadata& Site,
	const TConstArrayView<FArcForestFrozenSpeciesSetup> Species,
	const FArcEcotopeChunkLayout& Layout,
	const FArcEcotopeSiteChunkReader& SiteReader,
	const int64 CacheBudgetBytes,
	const int32 MaxActiveLeases,
	const FArcForestCancellationToken& Cancellation,
	FString& OutError)
{
	using namespace ArcForestHabitatFactorStorePrivate;
	OutError.Reset();
	if (Site.SiteSnapshotHash == 0 || Species.IsEmpty() || Layout.GetChunkCount() <= 0
		|| Layout.GetBaseDimensions() != Site.GridDimensions || !SiteReader.IsOpen()
		|| CacheBudgetBytes <= 0 || MaxActiveLeases <= 0)
	{
		OutError = TEXT("ArcForest habitat factor store received invalid build inputs.");
		return false;
	}

	TSharedPtr<FArcForestHabitatFactorStoreState, ESPMode::ThreadSafe> NewState =
		MakeShared<FArcForestHabitatFactorStoreState, ESPMode::ThreadSafe>();
	NewState->Stats.CacheBudgetBytes = CacheBudgetBytes;
	NewState->Stats.MaxActiveLeases = MaxActiveLeases;
	NewState->ChunkIndex.SetNum(Layout.GetChunkCount());
	NewState->Cache.SetNum(Layout.GetChunkCount());
	NewState->ChunkGridWidth = Layout.GetChunkGridDimensions().X;

	TArray<FSpeciesIdentity> SpeciesIdentity;
	SpeciesIdentity.Reserve(Species.Num());
	TArray<UE::ArcForest::Habitat::FHabitatResponseTable> SpeciesResponses;
	SpeciesResponses.SetNum(Species.Num());
	for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
	{
		if (!Species[SpeciesIndex].SpeciesId.IsValid() || Species[SpeciesIndex].EcologyHash == 0)
		{
			OutError = TEXT("ArcForest habitat factor store received an invalid species identity.");
			return false;
		}
		FSpeciesIdentity& Identity = SpeciesIdentity.AddDefaulted_GetRef();
		Identity.SpeciesId = Species[SpeciesIndex].SpeciesId;
		Identity.EcologyHash = Species[SpeciesIndex].EcologyHash;
		UE::ArcForest::Habitat::GetHabitatResponses(
			Species[SpeciesIndex], SpeciesResponses[SpeciesIndex].Responses);
	}

	const FString Directory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("ArcForest"), TEXT("TransientHabitat"));
	if (!IFileManager::Get().MakeDirectory(*Directory, true))
	{
		OutError = FString::Printf(TEXT("ArcForest habitat factor store could not create %s."), *Directory);
		return false;
	}
	NewState->BackingPath = FPaths::Combine(
		Directory,
		FString::Printf(
			TEXT("HabitatFactors_%016llx_%s.bin"),
			static_cast<unsigned long long>(Site.SiteSnapshotHash),
			*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
	TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*NewState->BackingPath));
	if (!Writer.IsValid())
	{
		OutError = FString::Printf(
			TEXT("ArcForest habitat factor store could not create backing file %s."), *NewState->BackingPath);
		return false;
	}

	SerializeHeader(*Writer, Site.SiteSnapshotHash, Layout, SpeciesIdentity, NewState->ChunkIndex);
	if (Writer->IsError())
	{
		OutError = TEXT("ArcForest habitat factor store could not write its backing header.");
		return false;
	}

	const auto FieldBit = [](const EArcEcotopeSiteField Field)
	{
		return 1u << static_cast<uint32>(Field);
	};
	const uint32 FieldMask = FieldBit(EArcEcotopeSiteField::Slope)
		| FieldBit(EArcEcotopeSiteField::Moisture)
		| FieldBit(EArcEcotopeSiteField::Fertility)
		| FieldBit(EArcEcotopeSiteField::SunLoad)
		| FieldBit(EArcEcotopeSiteField::WindExposure)
		| FieldBit(EArcEcotopeSiteField::Temperature)
		| FieldBit(EArcEcotopeSiteField::ModificationCut)
		| FieldBit(EArcEcotopeSiteField::BareRock)
		| FieldBit(EArcEcotopeSiteField::ChannelBuffer);
	const double BuildStartSeconds = FPlatformTime::Seconds();
	int64 WorkCounter = 0;
	int64 MaximumResidentAllocationBytes = 0;
	int64 MaximumPackedHardZeroScratchBytes = 0;
	for (int32 ChunkIndex = 0; ChunkIndex < Layout.GetChunkCount(); ++ChunkIndex)
	{
		if (Cancellation.IsCancellationRequested())
		{
			OutError = TEXT("ArcForest habitat factor store build was cancelled.");
			return false;
		}
		FArcEcotopeChunkRect ChunkRect;
		if (!Layout.GetChunk(ChunkIndex, ChunkRect))
		{
			OutError = TEXT("ArcForest habitat factor store encountered an invalid layout entry.");
			return false;
		}
		const int64 CoreCellCount64 =
			static_cast<int64>(ChunkRect.BaseCoreRect.Width()) * ChunkRect.BaseCoreRect.Height();
		int64 ProductCount64 = 0;
		if (CoreCellCount64 <= 0 || CoreCellCount64 > MAX_int32
			|| !TryMultiplyPositive(CoreCellCount64, Species.Num(), ProductCount64)
			|| ProductCount64 > MAX_int32)
		{
			OutError = TEXT("ArcForest habitat factor store chunk counts are not representable.");
			return false;
		}

		FArcEcotopeSiteChunkLease SourceLease;
		if (!SiteReader.AcquireFields(ChunkRect.Id, 0, FieldMask, SourceLease, OutError))
		{
			OutError = FString::Printf(
				TEXT("ArcForest habitat factor store could not acquire source chunk (%d,%d): %s"),
				ChunkRect.Id.X, ChunkRect.Id.Y, *OutError);
			return false;
		}
		const FArcEcotopeSiteChunkView& View = SourceLease.GetView();
		if (View.Id != ChunkRect.Id || View.BaseCoreRect != ChunkRect.BaseCoreRect)
		{
			OutError = TEXT("ArcForest habitat factor store acquired a mismatched source chunk.");
			return false;
		}

		const int32 CoreCellCount = static_cast<int32>(CoreCellCount64);
		const int32 ProductCount = static_cast<int32>(ProductCount64);
		TArray<UE::ArcForest::Habitat::FHabitatSiteSample> SiteSamples;
		SiteSamples.SetNumUninitialized(CoreCellCount);
		for (int32 BaseY = ChunkRect.BaseCoreRect.Min.Y; BaseY < ChunkRect.BaseCoreRect.Max.Y; ++BaseY)
		{
			for (int32 BaseX = ChunkRect.BaseCoreRect.Min.X; BaseX < ChunkRect.BaseCoreRect.Max.X; ++BaseX)
			{
				if ((++WorkCounter & 4095) == 0 && Cancellation.IsCancellationRequested())
				{
					OutError = TEXT("ArcForest habitat factor store build was cancelled.");
					return false;
				}
				const int32 LocalIndex = (BaseX - ChunkRect.BaseCoreRect.Min.X)
					+ (BaseY - ChunkRect.BaseCoreRect.Min.Y) * ChunkRect.BaseCoreRect.Width();
				if (!UE::ArcForest::Habitat::TryReadHabitatSiteSample(
						View, BaseX, BaseY, SiteSamples[LocalIndex]))
				{
					OutError = TEXT("ArcForest habitat factor store could not read every source field.");
					return false;
				}
			}
		}

		TArray<float> StaticProducts;
		StaticProducts.SetNumUninitialized(ProductCount);
		TArray<uint8> HardZeros;
		HardZeros.Init(0, FMath::DivideAndRoundUp(ProductCount, 8));
		for (int32 SpeciesIndex = 0; SpeciesIndex < Species.Num(); ++SpeciesIndex)
		{
			for (int32 LocalIndex = 0; LocalIndex < CoreCellCount; ++LocalIndex)
			{
				UE::ArcForest::Habitat::FHabitatStaticFactor Factor;
				UE::ArcForest::Habitat::BuildStaticHabitatFactor(
					SiteSamples[LocalIndex],
					SpeciesResponses[SpeciesIndex],
					Species[SpeciesIndex],
					Factor);
				const int32 ProductIndex = SpeciesIndex * CoreCellCount + LocalIndex;
				StaticProducts[ProductIndex] = Factor.SiteHabitat;
				if (Factor.bHardZero)
				{
					HardZeros[ProductIndex >> 3] |= static_cast<uint8>(1u << (ProductIndex & 7));
					++NewState->Stats.HardZeroSamples;
					switch (Factor.HardZeroReason)
					{
					case UE::ArcForest::Habitat::EHabitatHardZeroReason::Slope:
						++NewState->Stats.HardZeroSlopeSamples;
						break;
					case UE::ArcForest::Habitat::EHabitatHardZeroReason::SunLoad:
						++NewState->Stats.HardZeroSunLoadSamples;
						break;
					case UE::ArcForest::Habitat::EHabitatHardZeroReason::Wind:
						++NewState->Stats.HardZeroWindSamples;
						break;
					case UE::ArcForest::Habitat::EHabitatHardZeroReason::Fertility:
						++NewState->Stats.HardZeroFertilitySamples;
						break;
					case UE::ArcForest::Habitat::EHabitatHardZeroReason::Temperature:
						++NewState->Stats.HardZeroTemperatureSamples;
						break;
					case UE::ArcForest::Habitat::EHabitatHardZeroReason::Moisture:
						++NewState->Stats.HardZeroMoistureSamples;
						break;
					default:
						break;
					}
				}
			}
		}
		SourceLease.Reset();
		TBitArray<> ExpandedHardZeros;
		ExpandedHardZeros.Init(false, ProductCount);

		FChunkIndexEntry& Entry = NewState->ChunkIndex[ChunkIndex];
		Entry.Id = ChunkRect.Id;
		Entry.CoreRect = ChunkRect.BaseCoreRect;
		Entry.CoreCellCount = CoreCellCount;
		Entry.ProductCount = ProductCount;
		Entry.HardZeroByteCount = HardZeros.Num();
		Entry.ResidentAllocationBytes = static_cast<int64>(StaticProducts.GetAllocatedSize())
			+ static_cast<int64>(ExpandedHardZeros.GetAllocatedSize());
		Entry.PackedHardZeroScratchBytes = static_cast<int64>(HardZeros.GetAllocatedSize());
		if (Entry.ResidentAllocationBytes <= 0
			|| Entry.PackedHardZeroScratchBytes < Entry.HardZeroByteCount
			|| Entry.ResidentAllocationBytes > CacheBudgetBytes
			|| Entry.PackedHardZeroScratchBytes > CacheBudgetBytes - Entry.ResidentAllocationBytes)
		{
			OutError = FString::Printf(
				TEXT("ArcForest habitat factor chunk (%d,%d) requires %lld resident bytes plus %lld packed scratch bytes, exceeding the %lld-byte cache budget."),
				Entry.Id.X,
				Entry.Id.Y,
				Entry.ResidentAllocationBytes,
				Entry.PackedHardZeroScratchBytes,
				CacheBudgetBytes);
			return false;
		}
		MaximumResidentAllocationBytes = FMath::Max(
			MaximumResidentAllocationBytes, Entry.ResidentAllocationBytes);
		MaximumPackedHardZeroScratchBytes = FMath::Max(
			MaximumPackedHardZeroScratchBytes, Entry.PackedHardZeroScratchBytes);
		Entry.PayloadCrc = CalculatePayloadCrc(StaticProducts, HardZeros);
		Entry.Offset = Writer->Tell();
		uint32 PayloadMagic = ChunkMagic;
		Writer->Serialize(&PayloadMagic, sizeof(PayloadMagic));
		SerializeChunkId(*Writer, Entry.Id);
		*Writer << Entry.CoreRect;
		*Writer << Entry.CoreCellCount;
		*Writer << Entry.ProductCount;
		*Writer << Entry.HardZeroByteCount;
		Writer->Serialize(StaticProducts.GetData(), StaticProducts.Num() * sizeof(float));
		Writer->Serialize(HardZeros.GetData(), HardZeros.Num());
		Entry.Size = Writer->Tell() - Entry.Offset;
		if (Writer->IsError() || Entry.Offset < 0 || Entry.Size <= 0)
		{
			OutError = TEXT("ArcForest habitat factor store could not write a chunk payload.");
			return false;
		}

		NewState->Stats.BuiltSamples += ProductCount;
		NewState->Stats.ProductBytes += StaticProducts.Num() * sizeof(float);
		NewState->Stats.HardZeroBytes += HardZeros.Num();
		NewState->Stats.ScratchBytes = static_cast<int64>(SiteSamples.GetAllocatedSize())
			+ static_cast<int64>(StaticProducts.GetAllocatedSize())
			+ static_cast<int64>(ExpandedHardZeros.GetAllocatedSize())
			+ static_cast<int64>(HardZeros.GetAllocatedSize());
		UpdateMemoryPeaks(NewState->Stats);
		NewState->Stats.ScratchBytes = 0;
	}
	if (MaximumResidentAllocationBytes <= 0
		|| MaximumPackedHardZeroScratchBytes < 0
		|| MaximumPackedHardZeroScratchBytes > CacheBudgetBytes)
	{
		OutError = TEXT("ArcForest habitat factor store could not derive a safe batch capacity.");
		return false;
	}
	const int64 MemoryLimitedBatchChunkCount =
		(CacheBudgetBytes - MaximumPackedHardZeroScratchBytes) / MaximumResidentAllocationBytes;
	NewState->MaximumBatchChunkCount = static_cast<int32>(FMath::Min<int64>(
		FMath::Min<int64>(MemoryLimitedBatchChunkCount, MaxActiveLeases),
		Layout.GetChunkCount()));
	if (NewState->MaximumBatchChunkCount < 1)
	{
		OutError = TEXT("ArcForest habitat factor store cache budget cannot admit one batch chunk.");
		return false;
	}

	const int64 BackingSize = Writer->Tell();
	Writer->Seek(0);
	SerializeHeader(*Writer, Site.SiteSnapshotHash, Layout, SpeciesIdentity, NewState->ChunkIndex);
	Writer->Close();
	if (Writer->IsError() || BackingSize <= 0)
	{
		OutError = TEXT("ArcForest habitat factor store could not finalize its backing file.");
		return false;
	}
	if (!ValidateBackingHeader(
			NewState->BackingPath,
			BackingSize,
			Site.SiteSnapshotHash,
			Layout,
			SpeciesIdentity,
			NewState->ChunkIndex,
			OutError))
	{
		return false;
	}
	UE_CLOGF(
		NewState->Stats.HardZeroSlopeSamples + NewState->Stats.HardZeroSunLoadSamples
			+ NewState->Stats.HardZeroWindSamples + NewState->Stats.HardZeroFertilitySamples
			+ NewState->Stats.HardZeroTemperatureSamples + NewState->Stats.HardZeroMoistureSamples
			!= NewState->Stats.HardZeroSamples,
		LogArcEcotope,
		Warning,
		"ArcForest habitat factor store hard-zero attribution mismatch: slope=%lld sunLoad=%lld wind=%lld"
		" fertility=%lld temperature=%lld moisture=%lld sum != hardZeroSamples=%lld",
		NewState->Stats.HardZeroSlopeSamples,
		NewState->Stats.HardZeroSunLoadSamples,
		NewState->Stats.HardZeroWindSamples,
		NewState->Stats.HardZeroFertilitySamples,
		NewState->Stats.HardZeroTemperatureSamples,
		NewState->Stats.HardZeroMoistureSamples,
		NewState->Stats.HardZeroSamples);
	NewState->Stats.BackingBytes = BackingSize;
	NewState->Stats.BuildMilliseconds = (FPlatformTime::Seconds() - BuildStartSeconds) * 1000.0;
	State = MoveTemp(NewState);
	return true;
}

bool FArcForestHabitatFactorStore::AcquireChunk(
	const FArcEcotopeChunkId& Id,
	FArcForestHabitatFactorLease& OutLease,
	FString& OutError) const
{
	using namespace ArcForestHabitatFactorStorePrivate;
	OutLease.Reset();
	OutError.Reset();
	if (!State.IsValid())
	{
		OutError = TEXT("ArcForest habitat factor store is not built.");
		return false;
	}
	FScopeLock Lock(&State->Mutex);
	++State->Stats.AcquireCount;
	const int64 ChunkIndex64 = static_cast<int64>(Id.X) + static_cast<int64>(Id.Y) * State->ChunkGridWidth;
	if (!Id.IsValid() || State->ChunkGridWidth <= 0 || ChunkIndex64 < 0 || ChunkIndex64 >= State->ChunkIndex.Num()
		|| State->ChunkIndex[static_cast<int32>(ChunkIndex64)].Id != Id)
	{
		++State->Stats.FailedAcquireCount;
		OutError = TEXT("ArcForest habitat factor store received an invalid chunk id.");
		return false;
	}
	if (State->Stats.ActiveLeases >= State->Stats.MaxActiveLeases)
	{
		++State->Stats.FailedAcquireCount;
		OutError = TEXT("ArcForest habitat factor store active lease bound was exceeded.");
		return false;
	}

	const int32 ChunkIndex = static_cast<int32>(ChunkIndex64);
	FCacheEntry& CacheEntry = State->Cache[ChunkIndex];
	if (CacheEntry.Chunk.IsValid())
	{
		++State->Stats.CacheHitCount;
	}
	else
	{
		++State->Stats.CacheMissCount;
		const FChunkIndexEntry& DiskEntry = State->ChunkIndex[ChunkIndex];
		if (DiskEntry.Offset < 0 || DiskEntry.Size <= 0
			|| DiskEntry.Size > State->Stats.BackingBytes
			|| DiskEntry.Offset > State->Stats.BackingBytes - DiskEntry.Size)
		{
			++State->Stats.FailedAcquireCount;
			State->InvalidateBackingReader();
			OutError = TEXT("ArcForest habitat factor store chunk range is outside the validated backing file.");
			return false;
		}
		if (DiskEntry.ResidentAllocationBytes <= 0
			|| DiskEntry.PackedHardZeroScratchBytes < DiskEntry.HardZeroByteCount
			|| DiskEntry.ResidentAllocationBytes > State->Stats.CacheBudgetBytes
			|| DiskEntry.PackedHardZeroScratchBytes
				> State->Stats.CacheBudgetBytes - DiskEntry.ResidentAllocationBytes)
		{
			++State->Stats.FailedAcquireCount;
			OutError = TEXT("ArcForest habitat factor store chunk allocation requirements are invalid.");
			return false;
		}
		const int64 LoadAdmissionBytes =
			DiskEntry.ResidentAllocationBytes + DiskEntry.PackedHardZeroScratchBytes;
		while (State->Stats.ResidentBytes > State->Stats.CacheBudgetBytes - LoadAdmissionBytes)
		{
			int32 EvictionIndex = INDEX_NONE;
			uint64 OldestUse = MAX_uint64;
			for (int32 CandidateIndex = 0; CandidateIndex < State->Cache.Num(); ++CandidateIndex)
			{
				const FCacheEntry& Candidate = State->Cache[CandidateIndex];
				if (Candidate.Chunk.IsValid() && Candidate.PinCount == 0 && Candidate.LastUseSerial < OldestUse)
				{
					EvictionIndex = CandidateIndex;
					OldestUse = Candidate.LastUseSerial;
				}
			}
			if (EvictionIndex == INDEX_NONE)
			{
				++State->Stats.FailedAcquireCount;
				OutError = TEXT("ArcForest habitat factor store cache budget is exhausted by pinned chunks.");
				return false;
			}
			State->Stats.ResidentBytes -= State->Cache[EvictionIndex].Chunk->GetAllocatedSize();
			State->Cache[EvictionIndex].Chunk.Reset();
			State->Cache[EvictionIndex].LastUseSerial = 0;
			++State->Stats.EvictionCount;
		}

		const double LoadStartSeconds = FPlatformTime::Seconds();
		if (!State->EnsureBackingReader(OutError))
		{
			++State->Stats.FailedAcquireCount;
			return false;
		}
		FArchive& Reader = *State->BackingReader;
		if (Reader.Tell() != DiskEntry.Offset)
		{
			Reader.Seek(DiskEntry.Offset);
		}
		if (Reader.IsError())
		{
			++State->Stats.FailedAcquireCount;
			State->InvalidateBackingReader();
			OutError = TEXT("ArcForest habitat factor store could not seek to its backing chunk.");
			return false;
		}
		uint32 PayloadMagic = 0;
		FArcEcotopeChunkId StoredId;
		FIntRect StoredRect;
		int32 StoredCoreCellCount = 0;
		int32 StoredProductCount = 0;
		int32 StoredHardZeroByteCount = 0;
		Reader.Serialize(&PayloadMagic, sizeof(PayloadMagic));
		SerializeChunkId(Reader, StoredId);
		Reader << StoredRect;
		Reader << StoredCoreCellCount;
		Reader << StoredProductCount;
		Reader << StoredHardZeroByteCount;
		if (Reader.IsError() || PayloadMagic != ChunkMagic || StoredId != DiskEntry.Id
			|| StoredRect != DiskEntry.CoreRect
			|| StoredCoreCellCount != DiskEntry.CoreCellCount || StoredProductCount != DiskEntry.ProductCount
			|| StoredHardZeroByteCount != DiskEntry.HardZeroByteCount)
		{
			++State->Stats.FailedAcquireCount;
			State->InvalidateBackingReader();
			OutError = TEXT("ArcForest habitat factor store backing chunk header is inconsistent.");
			return false;
		}

		TSharedPtr<FArcForestHabitatFactorChunk, ESPMode::ThreadSafe> Loaded =
			MakeShared<FArcForestHabitatFactorChunk, ESPMode::ThreadSafe>();
		Loaded->Id = StoredId;
		Loaded->CoreRect = StoredRect;
		Loaded->SpeciesCount = StoredProductCount / StoredCoreCellCount;
		Loaded->CoreCellCount = StoredCoreCellCount;
		Loaded->StaticProducts.SetNumUninitialized(StoredProductCount);
		TArray<uint8> HardZeroBytes;
		HardZeroBytes.SetNumUninitialized(StoredHardZeroByteCount);
		if (static_cast<int64>(Loaded->StaticProducts.GetAllocatedSize())
				> DiskEntry.ResidentAllocationBytes
			|| static_cast<int64>(HardZeroBytes.GetAllocatedSize())
				> DiskEntry.PackedHardZeroScratchBytes)
		{
			++State->Stats.FailedAcquireCount;
			OutError = TEXT("ArcForest habitat factor store runtime allocation exceeded its recorded requirement.");
			return false;
		}
		State->Stats.ScratchBytes = Loaded->GetAllocatedSize() + HardZeroBytes.GetAllocatedSize();
		UpdateMemoryPeaks(State->Stats);
		Reader.Serialize(Loaded->StaticProducts.GetData(), Loaded->StaticProducts.Num() * sizeof(float));
		Reader.Serialize(HardZeroBytes.GetData(), HardZeroBytes.Num());
		const int64 ReadSize = Reader.Tell() - DiskEntry.Offset;
		if (Reader.IsError() || ReadSize != DiskEntry.Size)
		{
			State->Stats.ScratchBytes = 0;
			++State->Stats.FailedAcquireCount;
			State->InvalidateBackingReader();
			OutError = TEXT("ArcForest habitat factor store could not read a complete chunk payload.");
			return false;
		}
		if (CalculatePayloadCrc(Loaded->StaticProducts, HardZeroBytes)
			!= DiskEntry.PayloadCrc)
		{
			State->Stats.ScratchBytes = 0;
			++State->Stats.FailedAcquireCount;
			State->InvalidateBackingReader();
			OutError = TEXT("ArcForest habitat factor store chunk payload checksum failed.");
			return false;
		}
		Loaded->StaticHardZeros.Init(false, StoredProductCount);
		State->Stats.ScratchBytes = Loaded->GetAllocatedSize() + HardZeroBytes.GetAllocatedSize();
		const int64 LoadedBytes = Loaded->GetAllocatedSize();
		if (LoadedBytes > DiskEntry.ResidentAllocationBytes
			|| static_cast<int64>(HardZeroBytes.GetAllocatedSize())
				> DiskEntry.PackedHardZeroScratchBytes
			|| State->Stats.ScratchBytes > LoadAdmissionBytes)
		{
			State->Stats.ScratchBytes = 0;
			++State->Stats.FailedAcquireCount;
			OutError = TEXT("ArcForest habitat factor store expanded allocation exceeded its recorded requirement.");
			return false;
		}
		UpdateMemoryPeaks(State->Stats);
		for (int32 ProductIndex = 0; ProductIndex < StoredProductCount; ++ProductIndex)
		{
			Loaded->StaticHardZeros[ProductIndex] =
				(HardZeroBytes[ProductIndex >> 3] & static_cast<uint8>(1u << (ProductIndex & 7))) != 0;
		}
		CacheEntry.Chunk = MoveTemp(Loaded);
		State->Stats.ResidentBytes += LoadedBytes;
		State->Stats.ScratchBytes = 0;
		++State->Stats.PayloadLoadCount;
		State->Stats.LoadMilliseconds += (FPlatformTime::Seconds() - LoadStartSeconds) * 1000.0;
		UpdateMemoryPeaks(State->Stats);
	}

	CacheEntry.LastUseSerial = ++State->UseSerial;
	const bool bWasUnpinned = CacheEntry.PinCount == 0;
	++CacheEntry.PinCount;
	++State->Stats.ActiveLeases;
	State->Stats.PeakActiveLeases = FMath::Max(State->Stats.PeakActiveLeases, State->Stats.ActiveLeases);
	const int64 ChunkBytes = CacheEntry.Chunk->GetAllocatedSize();
	if (bWasUnpinned)
	{
		State->Stats.PinnedBytes += ChunkBytes;
	}
	UpdateMemoryPeaks(State->Stats);
	TSharedPtr<FArcForestHabitatFactorLeaseData, ESPMode::ThreadSafe> LeaseData =
		MakeShared<FArcForestHabitatFactorLeaseData, ESPMode::ThreadSafe>();
	LeaseData->State = State;
	LeaseData->Chunk = CacheEntry.Chunk;
	LeaseData->ChunkIndex = ChunkIndex;
	OutLease.Data = MoveTemp(LeaseData);
	return true;
}

int32 FArcForestHabitatFactorStore::GetMaximumBatchChunkCount() const
{
	if (!State.IsValid())
	{
		return 0;
	}
	FScopeLock Lock(&State->Mutex);
	return State->MaximumBatchChunkCount;
}

void FArcForestHabitatFactorStore::Reset()
{
	State.Reset();
}

FArcForestHabitatFactorStoreStats FArcForestHabitatFactorStore::GetStats() const
{
	if (!State.IsValid())
	{
		return {};
	}
	FScopeLock Lock(&State->Mutex);
	return State->Stats;
}
