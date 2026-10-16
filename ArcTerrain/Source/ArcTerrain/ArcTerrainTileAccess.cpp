// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileAccess.h"

#include "ArcTerrainActor.h"
#include "ArcTerrainDiagnostics.h"
#include "ArcTerrainModule.h"
#include "ArcTerrainSubsystem.h"
#include "ArcTerrainTileIO.h"
#include "Engine/World.h"
#include "ArcTerrainTileManifest.h"
#include "ArcTerrainTileNames.h"
#include "ArcTerrainTileUpload.h"

#include "Async/Async.h"
#include "Compression/OodleDataCompression.h"
#include "HAL/CriticalSection.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Serialization/Archive.h"
#include "Tasks/Task.h"
#include "Templates/UniquePtr.h"
#include "UObject/ObjectKey.h"

#include <atomic>

namespace ArcTerrainTileAccessPrivate_4e18
{
	/** Bumped wherever any terrain's Manifest or Unusable changes, which is exactly what
	 *  GatherManifestCoverage reports. Outside the store so its reader never takes GStoreLock;
	 *  bManifestDirty cannot serve as the signal, being cleared by the save and never set by a write. */
	std::atomic<uint32> GCoverageEpoch{0};
	std::atomic<uint32> GLoadedCoverageEpoch{0};

	void BumpCoverageEpoch()
	{
		GCoverageEpoch.fetch_add(1, std::memory_order_relaxed);
	}

	void BumpLoadedCoverageEpoch()
	{
		GLoadedCoverageEpoch.fetch_add(1, std::memory_order_relaxed);
	}

	int32 RailIndex(EArcTerrainRail Rail)
	{
		return FMath::Clamp(static_cast<int32>(Rail), 0, ArcTerrain::NumTileRails - 1);
	}

	/** Manifest and Unusable are private FOR ONE REASON: every change to either must bump the coverage
	 *  epoch, and a field a caller can reach directly is a field a future caller can forget to pair with
	 *  the bump. Go through the methods below instead of adding a new one. */
	struct FTerrainTiles
	{
		FString TileDir;
		bool bTileDirResolved = false;

		TMap<FIntPoint, FArcTerrainTilePayloadPtr> Loaded[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];

		/** Key -> PayloadId of the ONE read that may publish it. A rewrite or a release drops the entry,
		 *  so the read it belonged to lands on a key that no longer accepts it. */
		TMap<FIntPoint, uint32> InFlight[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];

#if WITH_EDITOR
		/** A manifest entry was dropped because its file is gone; the write is deferred to FlushManifest so
		 *  a directory someone deleted costs one save, not one per tile. */
		bool bManifestDirty = false;
#endif
		uint64 LoadedMutationSerial = 0;
		FArcTerrainTileStoreMutationCounters IntervalStats;
		FArcTerrainTileStoreMutationCounters LifetimeStats;

		void BumpLoadedMutation()
		{
			++LoadedMutationSerial;
			BumpLoadedCoverageEpoch();
		}

		bool IsManifestLoaded() const { return bManifestLoaded; }

		const FArcTerrainTileManifest& GetManifest() const { return Manifest; }

		const TMap<FIntPoint, uint32>& GetUnusable(EArcTerrainRail Rail, int32 Tier) const
		{
			return Unusable[RailIndex(Rail)][Tier];
		}

		/** EnsureManifestLoaded's publish: the manifest arriving IS a coverage change. */
		void SetManifest(FArcTerrainTileManifest&& NewManifest)
		{
			Manifest = MoveTemp(NewManifest);
			bManifestLoaded = true;
			BumpCoverageEpoch();
		}

		void AddManifestEntry(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, const FIoHash& RecipeKey)
		{
			Manifest.Add(Rail, Tier, TileKey, RecipeKey);
			BumpCoverageEpoch();
		}

		void RemoveManifestEntry(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
		{
			Manifest.Remove(Rail, Tier, TileKey);
			BumpCoverageEpoch();
		}

		void MarkUnusable(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey, uint32 Signature)
		{
			Unusable[RailIndex(Rail)][Tier].Add(TileKey, Signature);
			BumpCoverageEpoch();
		}

		void ClearUnusable(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
		{
			Unusable[RailIndex(Rail)][Tier].Remove(TileKey);
			BumpCoverageEpoch();
		}

	private:
		FArcTerrainTileManifest Manifest;
		bool bManifestLoaded = false;

		/** Key -> signature of the grid the file failed to decode under. Held IN MEMORY and never on disk:
		 *  the file is a correct tile of another lattice, so a manifest edit would orphan it for good. A
		 *  rewrite clears the entry, and a signature that no longer matches the live grid retires it. */
		TMap<FIntPoint, uint32> Unusable[ArcTerrain::NumTileRails][ArcTerrain::MaxTiers];
	};

	FCriticalSection GStoreLock;
	TMap<FObjectKey, FTerrainTiles> GStore;
	uint32 GNextPayloadId = 1;

	uint32 AllocatePayloadId()
	{
		FScopeLock Lock(&GStoreLock);
		return GNextPayloadId++;
	}

	/** Dropping a terrain's whole entry is also a coverage change: its manifest and Unusable set go with
	 *  it. Caller holds GStoreLock. */
	void RemoveTerrainFromStore(const FObjectKey& TerrainKey)
	{
		GStore.Remove(TerrainKey);
		BumpCoverageEpoch();
		BumpLoadedCoverageEpoch();
	}

	void ClearStore()
	{
		GStore.Empty();
		BumpCoverageEpoch();
		BumpLoadedCoverageEpoch();
	}

	/** Every term ArcTerrainTileFormat::IsCompatible tests, minus the rail (which indexes the arrays), so
	 *  two grids sharing a signature accept exactly the same files. */
	uint32 GridSignature(const FArcTerrainTileGrid& Grid)
	{
		return HashCombine(HashCombine(GetTypeHash(Grid.Resolution()), GetTypeHash(Grid.TileExtentM)),
			GetTypeHash(Grid.TexelSizeCm));
	}

	/** The directory is derived from the level package, so it resolves on the game thread and is then
	 *  cached — a task reading a tile never touches the package system. */
	FString ResolveTileDir(const AArcTerrain& Terrain)
	{
		{
			FScopeLock Lock(&GStoreLock);
			if (const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
			{
				if (Tiles->bTileDirResolved)
				{
					return Tiles->TileDir;
				}
			}
		}

		check(IsInGameThread());
		const FString Resolved = ArcTerrainTileNames::MakeTileDirForRead(Terrain);

		FScopeLock Lock(&GStoreLock);
		FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
		Tiles.TileDir = Resolved;
		Tiles.bTileDirResolved = true;
		return Resolved;
	}

#if WITH_EDITOR
	struct FReapedTile
	{
		EArcTerrainRail Rail = EArcTerrainRail::Height;
		int32 Tier = 0;
		FIntPoint TileKey = FIntPoint::ZeroValue;
	};

	TSet<FString> GReapedTileDirs;

	/** CALLER MUST NOT HOLD GStoreLock. The store entry cannot carry this: ReleaseTerrainTiles drops it
	 *  while tile writes are still in flight, so a second reap could delete a file a bake just moved in. */
	bool ClaimTileDirReap(const FString& TileDir)
	{
		FScopeLock Lock(&GStoreLock);
		bool bAlreadyClaimed = false;
		GReapedTileDirs.Add(TileDir, &bAlreadyClaimed);
		return !bAlreadyClaimed;
	}

	void ReapForeignVersionTiles(const FString& TileDir, TArray<FReapedTile>& OutReaped)
	{
		IFileManager& FileManager = IFileManager::Get();

		int32 DeletedFiles = 0;
		int64 DeletedBytes = 0;
		for (int32 Index = 0; Index < ArcTerrain::NumTileRails; ++Index)
		{
			const EArcTerrainRail Rail = static_cast<EArcTerrainRail>(Index);

			TArray<FString> FileNames;
			const FString Pattern = FPaths::Combine(TileDir, ArcTerrainTileNames::MakeTileFileWildcard(Rail));
			FileManager.FindFiles(FileNames, *Pattern, /*Files=*/true, /*Directories=*/false);

			for (const FString& FileName : FileNames)
			{
				const FString FilePath = FPaths::Combine(TileDir, FileName);

				uint8 LeadingBytes[ArcTerrainTileFormat::VersionPeekBytes] = {};
				int64 FileBytes = 0;
				{
					TUniquePtr<FArchive> Reader(FileManager.CreateFileReader(*FilePath));
					if (!Reader.IsValid() || Reader->TotalSize() < ArcTerrainTileFormat::VersionPeekBytes)
					{
						continue;
					}

					FileBytes = Reader->TotalSize();
					Reader->Serialize(LeadingBytes, ArcTerrainTileFormat::VersionPeekBytes);
					if (!Reader->Close())
					{
						continue;
					}
				}

				uint32 FileVersion = 0;
				if (!ArcTerrainTileFormat::PeekTileVersion(LeadingBytes, FileVersion)
					|| FileVersion == ArcTerrainTileFormat::TileVersion)
				{
					continue;
				}

				if (FileManager.Delete(*FilePath, /*RequireExists=*/false, /*EvenReadOnly=*/true, /*Quiet=*/true))
				{
					++DeletedFiles;
					DeletedBytes += FileBytes;

					int32 Tier = 0;
					FIntPoint TileKey = FIntPoint::ZeroValue;
					if (ArcTerrainTileNames::ParseTileFileName(Rail, FileName, Tier, TileKey))
					{
						FReapedTile& Reaped = OutReaped.AddDefaulted_GetRef();
						Reaped.Rail = Rail;
						Reaped.Tier = Tier;
						Reaped.TileKey = TileKey;
					}
				}
			}
		}

		UE_CLOGF(DeletedFiles > 0, LogArcTerrain, Log,"Reaped %d tile(s) written under a version other than %u from '%ls', reclaiming %" INT64_FMT " B.",
			DeletedFiles, ArcTerrainTileFormat::TileVersion, *TileDir, DeletedBytes);
	}
#endif // WITH_EDITOR

	/** The manifest is a derivative of the disk: no file means rebuild it from the names on disk and
	 *  publish that. */
	void EnsureManifestLoaded(const AArcTerrain& Terrain, const FString& TileDir)
	{
		check(IsInGameThread());

		{
			FScopeLock Lock(&GStoreLock);
			if (GStore.FindOrAdd(FObjectKey(&Terrain)).IsManifestLoaded())
			{
				return;
			}
		}

#if WITH_EDITOR
		TArray<FReapedTile> ReapedTiles;
		if (!TileDir.IsEmpty() && !ArcTerrainTileNames::MakeTileDirForWrite(Terrain).IsEmpty()
			&& ClaimTileDirReap(TileDir))
		{
			ReapForeignVersionTiles(TileDir, ReapedTiles);
		}
#endif

		FArcTerrainTileManifest Loaded;
		if (!TileDir.IsEmpty())
		{
			if (ArcTerrainTileManifest::Load(TileDir, Loaded))
			{
#if WITH_EDITOR
				bool bRemovedEntry = false;
				for (const FReapedTile& Reaped : ReapedTiles)
				{
					if (Loaded.Contains(Reaped.Rail, Reaped.Tier, Reaped.TileKey))
					{
						Loaded.Remove(Reaped.Rail, Reaped.Tier, Reaped.TileKey);
						bRemovedEntry = true;
					}
				}
				if (bRemovedEntry)
				{
					ArcTerrainTileManifest::Save(TileDir, Loaded);
				}
#endif
			}
			else
			{
				ArcTerrainTileManifest::RebuildFromDisk(TileDir, Loaded);
#if WITH_EDITOR
				ArcTerrainTileManifest::Save(TileDir, Loaded);
#endif
			}
		}

		FScopeLock Lock(&GStoreLock);
		FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
		if (!Tiles.IsManifestLoaded())
		{
			Tiles.SetManifest(MoveTemp(Loaded));
		}
	}

	FString MakeTilePath(const FString& TileDir, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		const FString FileName = ArcTerrainTileNames::MakeTileFileName(Rail, Tier, TileKey);
		return FileName.IsEmpty() ? FString() : FPaths::Combine(TileDir, FileName);
	}

	/** File bytes -> payload. Null on every kind of miss, with OutResult naming which one. */
	FArcTerrainTilePayloadPtr ReadPayload(const FString& FilePath, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey, const FArcTerrainTileGrid& Grid, uint32 PayloadId, EArcTerrainTileLoad& OutResult)
	{
		FArcTerrainTileHeader Header;
		TArray<uint8> FileBytes;
		OutResult = ArcTerrainTileIO::LoadTile(FilePath, Rail, Grid, Header, FileBytes);
		if (OutResult != EArcTerrainTileLoad::Loaded)
		{
			return FArcTerrainTilePayloadPtr();
		}

		const int64 BodyBytes = static_cast<int64>(Header.UncompressedBodyBytes);
		if (BodyBytes > static_cast<int64>(MAX_int32))
		{
			// ParseHeader admits any body up to MAX_uint32; a TArray sized from one this large wraps.
			UE_LOGF(LogArcTerrain, Warning,"Tile '%ls' declares a %" INT64_FMT " B body, over the %d an array can hold, and counts as absent.",
				*FilePath, BodyBytes, MAX_int32);
			OutResult = EArcTerrainTileLoad::Unusable;
			return FArcTerrainTilePayloadPtr();
		}

		const uint8* const FileBody = FileBytes.GetData() + ArcTerrainTileFormat::HeaderBytes(Header);
		TArray<uint8> DecompressedBody;
		TConstArrayView<uint8> Body;
		if (Header.CompressionMethod == ArcTerrainTileFormat::CompressionNone)
		{
			Body = TConstArrayView<uint8>(FileBody, static_cast<int32>(BodyBytes));
		}
		else
		{
			DecompressedBody.SetNumUninitialized(static_cast<int32>(BodyBytes));
			// ParseHeader accepts CompressedBodyBytes == 0, and the decoder is not handed a zero-length input.
			const bool bDecoded = Header.CompressedBodyBytes > 0
				&& FOodleDataCompression::DecompressParallel(DecompressedBody.GetData(), BodyBytes, FileBody,
					static_cast<int64>(Header.CompressedBodyBytes));
			if (!bDecoded)
			{
				UE_LOGF(LogArcTerrain, Warning,"Tile '%ls' did not decompress (%u B -> %" INT64_FMT " B) and counts as absent.",
					*FilePath, Header.CompressedBodyBytes, BodyBytes);
				OutResult = EArcTerrainTileLoad::Unusable;
				return FArcTerrainTilePayloadPtr();
			}
			Body = DecompressedBody;
		}

		TSharedRef<FArcTerrainTilePayload, ESPMode::ThreadSafe> Payload =
			MakeShared<FArcTerrainTilePayload, ESPMode::ThreadSafe>();
		Payload->Header = Header;
		Payload->TileKey = TileKey;
		Payload->Rail = Rail;
		Payload->PayloadId = PayloadId;

		const int64 PlaneBytes = ArcTerrainTileFormat::PlaneBytes(Header);
		const int32 NumPlanes = ArcTerrainTileFormat::NumBodyPlanes(Header);

		if (Rail == EArcTerrainRail::Height)
		{
			// Texels from the resolution, never from PlaneBytes / sizeof(float): the body is uint16.
			const int64 TotalTexels = static_cast<int64>(NumPlanes)
				* static_cast<int64>(Header.Resolution) * static_cast<int64>(Header.Resolution);
			Payload->Floats.SetNumUninitialized(static_cast<int32>(TotalTexels));
			ArcTerrainTileFormat::DecodeHeightTexels(Body, Header.PlaneQuantMin[0], Header.PlaneQuantMax[0],
				Payload->Floats);
		}
		else
		{
			Payload->Planes.SetNum(NumPlanes);
			for (int32 PlaneIndex = 0; PlaneIndex < NumPlanes; ++PlaneIndex)
			{
				const int64 Offset = ArcTerrainTileFormat::PlaneBodyOffset(Header, PlaneIndex);
				Payload->Planes[PlaneIndex].SetNumUninitialized(static_cast<int32>(PlaneBytes));
				FMemory::Memcpy(Payload->Planes[PlaneIndex].GetData(), Body.GetData() + Offset,
					static_cast<int32>(PlaneBytes));
			}
		}

		return Payload;
	}

	void PublishSync(const FObjectKey& TerrainKey, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTilePayloadPtr& Payload)
	{
		FScopeLock Lock(&GStoreLock);
		FTerrainTiles& Tiles = GStore.FindOrAdd(TerrainKey);
		if (Payload.IsValid())
		{
			Tiles.Loaded[RailIndex(Rail)][Tier].Add(TileKey, Payload);
			++Tiles.IntervalStats.SyncPublications;
			++Tiles.LifetimeStats.SyncPublications;
		}
		else
		{
			Tiles.Loaded[RailIndex(Rail)][Tier].Remove(TileKey);
		}
		Tiles.BumpLoadedMutation();
	}

	/** False when the ticket has gone: the tile was rewritten or released while the read ran, so these
	 *  bytes describe a state nobody asked for and the key is left not-loaded for the next pass. */
	bool PublishRequested(const FObjectKey& TerrainKey, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTilePayloadPtr& Payload, uint32 PayloadId)
	{
		FScopeLock Lock(&GStoreLock);
		FTerrainTiles* Tiles = GStore.Find(TerrainKey);
		if (Tiles == nullptr)
		{
			return false;
		}
		const uint32* Ticket = Tiles->InFlight[RailIndex(Rail)][Tier].Find(TileKey);
		if (Ticket == nullptr || *Ticket != PayloadId)
		{
			++Tiles->IntervalStats.AsyncFailures;
			++Tiles->LifetimeStats.AsyncFailures;
			return false;
		}
		Tiles->InFlight[RailIndex(Rail)][Tier].Remove(TileKey);
		if (Payload.IsValid())
		{
			Tiles->Loaded[RailIndex(Rail)][Tier].Add(TileKey, Payload);
			++Tiles->IntervalStats.AsyncPublications;
			++Tiles->LifetimeStats.AsyncPublications;
		}
		else
		{
			Tiles->Loaded[RailIndex(Rail)][Tier].Remove(TileKey);
			++Tiles->IntervalStats.AsyncFailures;
			++Tiles->LifetimeStats.AsyncFailures;
		}
		Tiles->BumpLoadedMutation();
		return true;
	}

	/** The manifest names a tile whose file is gone, so the entry goes with it. */
	void DropManifestEntry(const FObjectKey& TerrainKey, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		FScopeLock Lock(&GStoreLock);
		FTerrainTiles* Tiles = GStore.Find(TerrainKey);
		if (Tiles == nullptr || !Tiles->GetManifest().Contains(Rail, Tier, TileKey))
		{
			return;
		}
		Tiles->RemoveManifestEntry(Rail, Tier, TileKey);
#if WITH_EDITOR
		Tiles->bManifestDirty = true;
#endif
	}

	/** The file is there and does not decode under this grid. The manifest keeps naming it — the bytes may
	 *  be a correct tile of a lattice this terrain goes back to — and the stream stops asking for it. */
	void MarkTileUnusable(const FObjectKey& TerrainKey, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		uint32 Signature)
	{
		FScopeLock Lock(&GStoreLock);
		if (FTerrainTiles* Tiles = GStore.Find(TerrainKey))
		{
			Tiles->MarkUnusable(Rail, Tier, TileKey, Signature);
		}
	}

	void RecordReadOutcome(const FObjectKey& TerrainKey, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		EArcTerrainTileLoad Result, uint32 Signature)
	{
		if (Result != EArcTerrainTileLoad::Loaded)
		{
			ArcTerrainDiag::NoteTileRejected(Rail, Tier, TileKey, Result);
		}

		// ReadFailed records NOTHING: the read is retried on the next pass, which is the whole difference
		// between a lock and a file that does not decode.
		if (Result == EArcTerrainTileLoad::FileMissing)
		{
			DropManifestEntry(TerrainKey, Rail, Tier, TileKey);
		}
		else if (Result == EArcTerrainTileLoad::Unusable)
		{
			MarkTileUnusable(TerrainKey, Rail, Tier, TileKey, Signature);
		}
	}

#if WITH_EDITOR
	/** One terrain's deferred manifest write, taken under the store lock and performed outside it. */
	struct FPendingManifestSave
	{
		FString TileDir;
		FArcTerrainTileManifest Manifest;
	};

	/** CALLER HOLDS GStoreLock. Clears the dirty flag, so the entry cannot be written twice. */
	void TakePendingManifestSave(FTerrainTiles& Tiles, FPendingManifestSave& OutPending)
	{
		if (!Tiles.bManifestDirty || Tiles.TileDir.IsEmpty())
		{
			return;
		}
		Tiles.bManifestDirty = false;
		OutPending.TileDir = Tiles.TileDir;
		OutPending.Manifest = Tiles.GetManifest();
	}

	void SavePendingManifest(const FPendingManifestSave& Pending)
	{
		if (!Pending.TileDir.IsEmpty())
		{
			ArcTerrainTileManifest::Save(Pending.TileDir, Pending.Manifest);
		}
	}
#endif // WITH_EDITOR

	int64 PayloadBytes(const FArcTerrainTilePayload& Payload)
	{
		int64 Bytes = static_cast<int64>(Payload.Floats.Num()) * sizeof(float);
		for (const TArray<uint8>& Plane : Payload.Planes)
		{
			Bytes += Plane.Num();
		}
		return Bytes;
	}

	/** Cell quads of the rail the grid belongs to: the tile's own quad count over the cells it spans, so
	 *  the weight rail's runtime-resolved density needs no second authority here. */
	int32 CellQuadsOf(const FArcTerrainTileGrid& Grid)
	{
		return Grid.CellsPerTileEdge > 0 ? Grid.TileQuads / Grid.CellsPerTileEdge : 0;
	}

	bool ResolveCellSubRect(const FArcTerrainTileGrid& Grid, const FArcTerrainTilePayload& Payload,
		FArcTerrainCellKey CellKey, FIntPoint& OutOrigin, int32& OutExtent)
	{
		if (!Grid.IsValid() || Payload.Resolution() != Grid.Resolution())
		{
			return false;
		}
		return ArcTerrainTileUpload::CellSubRectInTile(Grid, Payload.TileKey, CellKey, CellQuadsOf(Grid),
			OutOrigin, OutExtent);
	}

	bool ResolveGroupSubRect(const FArcTerrainTileGrid& Grid, const FArcTerrainTilePayload& Payload,
		FIntPoint GroupKey, int32 GroupQuads, int32 GroupsPerTileEdge, FIntPoint& OutOrigin,
		int32& OutExtent)
	{
		if (!Grid.IsValid() || Payload.Resolution() != Grid.Resolution())
		{
			return false;
		}
		return ArcTerrainTileUpload::GroupSubRectInTile(Grid, Payload.TileKey, GroupKey, GroupQuads,
			GroupsPerTileEdge, OutOrigin, OutExtent);
	}

	bool CopyPlaneSubRect(const FArcTerrainTilePayload& Payload, int32 PlaneIndex, FIntPoint Origin,
		int32 Extent, TArray<uint8>& OutBytes)
	{
		const int32 TileRes = Payload.Resolution();
		if (Origin.X < 0 || Origin.Y < 0 || Origin.X + Extent > TileRes || Origin.Y + Extent > TileRes)
		{
			return false;
		}

		const uint8* Plane = Payload.Planes[PlaneIndex].GetData();
		OutBytes.SetNumUninitialized(Extent * Extent);
		for (int32 Row = 0; Row < Extent; ++Row)
		{
			FMemory::Memcpy(OutBytes.GetData() + static_cast<int64>(Row) * Extent,
				Plane + static_cast<int64>(Origin.Y + Row) * TileRes + Origin.X, Extent);
		}
		return true;
	}
}

int32 FArcTerrainTilePayload::FindWeightPlane(uint32 ChannelNameHash) const
{
	return Header.PlaneIds.IndexOfByKey(ChannelNameHash);
}

int32 FArcTerrainTilePayload::FindDataPlane(uint8 DataMapId) const
{
	return Header.DataMapIds.IndexOfByKey(DataMapId);
}

namespace ArcTerrainTileAccess
{
	int32 ResolveHeightTierShift(const AArcTerrain& Terrain, int32 Tier)
	{
		if (Tier == 0)
		{
			return 0;
		}
		if (!IsValidTier(Tier))
		{
			return INDEX_NONE;
		}

		check(IsInGameThread());

		const UWorld* World = Terrain.GetWorld();
		const UArcTerrainSubsystem* Subsystem =
			World != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
		if (Subsystem == nullptr)
		{
			return INDEX_NONE;
		}

		const FArcTerrainTierLadder Ladder = Subsystem->GetResolvedTierLadder(Terrain);
		return Ladder.Tiers.IsValidIndex(Tier) ? Ladder.Tiers[Tier].CumulativeShift : INDEX_NONE;
	}

	int32 ResolveWeightTierShift(const AArcTerrain& Terrain, int32 Tier)
	{
		if (Tier == 0)
		{
			return 0;
		}
		if (!IsValidTier(Tier))
		{
			return INDEX_NONE;
		}

		check(IsInGameThread());

		const UWorld* World = Terrain.GetWorld();
		const UArcTerrainSubsystem* Subsystem =
			World != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
		if (Subsystem == nullptr)
		{
			return INDEX_NONE;
		}

		const FArcTerrainWeightLadder Ladder = Subsystem->GetResolvedWeightTierLadder(Terrain);
		return Ladder.Tiers.IsValidIndex(Tier) ? Ladder.Tiers[Tier].CumulativeShift : INDEX_NONE;
	}

	bool ResolveGrid(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FArcTerrainTileGrid& OutGrid)
	{
		if (!IsValidTier(Tier))
		{
			return false;
		}

		const FArcTerrainConfig& Config = Terrain.Config;

		FArcTerrainTileGrid Grid;
		switch (Rail)
		{
		case EArcTerrainRail::Height:
		{
			const int32 CumulativeShift = ResolveHeightTierShift(Terrain, Tier);
			if (CumulativeShift == INDEX_NONE)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::HeightTileGrid(Config, CumulativeShift);
			break;
		}
		case EArcTerrainRail::Data:
			if (Tier != 0)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::DataTileGrid(Config);
			break;
		default:
		{
			const int32 CumulativeShift = ResolveWeightTierShift(Terrain, Tier);
			if (CumulativeShift == INDEX_NONE)
			{
				return false;
			}
			const UWorld* World = Terrain.GetWorld();
			const UArcTerrainSubsystem* Subsystem =
				World != nullptr ? World->GetSubsystem<UArcTerrainSubsystem>() : nullptr;
			if (Subsystem == nullptr)
			{
				return false;
			}
			Grid = ArcTerrainTileFormat::WeightTileGrid(Config,
				Subsystem->GetResolvedWeightQuads(Terrain) >> CumulativeShift, CumulativeShift);
			break;
		}
		}

		if (!Grid.IsValid())
		{
			return false;
		}
		OutGrid = Grid;
		return true;
	}

	bool TileKeyFromLocal(const FArcTerrainTileGrid& Grid, FVector2D Local, FIntPoint& OutKey)
	{
		return ArcTerrainTileFormat::TileKeyFromLocal(Grid, Local, OutKey);
	}

	FIntPoint TileKeyFromCell(const FArcTerrainTileGrid& Grid, FArcTerrainCellKey CellKey)
	{
		if (Grid.CellsPerTileEdge <= 0)
		{
			return FIntPoint::ZeroValue;
		}
		return FIntPoint(
			static_cast<int32>(ArcTerrain::FloorDiv(CellKey.X, Grid.CellsPerTileEdge)),
			static_cast<int32>(ArcTerrain::FloorDiv(CellKey.Y, Grid.CellsPerTileEdge)));
	}

	bool TileKeyFromCell(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FArcTerrainCellKey CellKey, FIntPoint& OutKey)
	{
		FArcTerrainTileGrid Grid;
		if (!ResolveGrid(Terrain, Rail, Tier, Grid))
		{
			return false;
		}
		OutKey = TileKeyFromCell(Grid, CellKey);
		return true;
	}

	bool HasTile(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const FArcTerrainTileGrid* KnownGrid)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!IsValidTier(Tier))
		{
			return false;
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return false;
		}
		EnsureManifestLoaded(Terrain, TileDir);

		const FObjectKey TerrainKey(&Terrain);
		{
			FScopeLock Lock(&GStoreLock);
			const FTerrainTiles* Tiles = GStore.Find(TerrainKey);
			if (Tiles == nullptr || !Tiles->GetManifest().Contains(Rail, Tier, TileKey))
			{
				return false;
			}
			if (!Tiles->GetUnusable(Rail, Tier).Contains(TileKey))
			{
				return true;
			}
		}

		// Only ever reached for a key a read already rejected: resolving the weight lattice scans every
		// stamp, so the manifest answers alone on the common path.
		FArcTerrainTileGrid Resolved;
		if (KnownGrid == nullptr)
		{
			if (!ResolveGrid(Terrain, Rail, Tier, Resolved))
			{
				return false;
			}
			KnownGrid = &Resolved;
		}
		const uint32 Signature = GridSignature(*KnownGrid);

		FScopeLock Lock(&GStoreLock);
		FTerrainTiles* Tiles = GStore.Find(TerrainKey);
		if (Tiles == nullptr || !Tiles->GetManifest().Contains(Rail, Tier, TileKey))
		{
			return false;
		}
		const uint32* Recorded = Tiles->GetUnusable(Rail, Tier).Find(TileKey);
		if (Recorded == nullptr)
		{
			return true;
		}
		if (*Recorded == Signature)
		{
			return false;
		}
		Tiles->ClearUnusable(Rail, Tier, TileKey);
		return true;
	}

	FArcTerrainTilePayloadPtr FindLoadedTile(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!IsValidTier(Tier))
		{
			return FArcTerrainTilePayloadPtr();
		}

		FScopeLock Lock(&GStoreLock);
		const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
		if (Tiles == nullptr)
		{
			return FArcTerrainTilePayloadPtr();
		}
		const FArcTerrainTilePayloadPtr* Found =
			Tiles->Loaded[RailIndex(Rail)][Tier].Find(TileKey);
		return Found != nullptr ? *Found : FArcTerrainTilePayloadPtr();
	}

	FArcTerrainTilePayloadPtr LoadTileSync(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		if (FArcTerrainTilePayloadPtr Cached = FindLoadedTile(Terrain, Rail, Tier, TileKey))
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
			++Tiles.IntervalStats.SyncCacheHits;
			++Tiles.LifetimeStats.SyncCacheHits;
			return Cached;
		}

		// Past the cache this is a disk read on the game thread — the frame waits for it.
		ArcTerrainDiag::FScopedBlockingGameThread BlockingScope(TEXT("sync tile read"), TileKey);

		FArcTerrainTileGrid Grid;
		if (!ResolveGrid(Terrain, Rail, Tier, Grid))
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
			++Tiles.IntervalStats.SyncFailures;
			++Tiles.LifetimeStats.SyncFailures;
			return FArcTerrainTilePayloadPtr();
		}
		if (!HasTile(Terrain, Rail, Tier, TileKey, &Grid))
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
			++Tiles.IntervalStats.SyncFailures;
			++Tiles.LifetimeStats.SyncFailures;
			return FArcTerrainTilePayloadPtr();
		}

		const FString TileDir = ResolveTileDir(Terrain);
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
			++Tiles.IntervalStats.SyncPhysicalAttempts;
			++Tiles.LifetimeStats.SyncPhysicalAttempts;
		}
		EArcTerrainTileLoad Result = EArcTerrainTileLoad::Loaded;
		const FArcTerrainTilePayloadPtr Payload = ReadPayload(MakeTilePath(TileDir, Rail, Tier, TileKey),
			Rail, Tier, TileKey, Grid, AllocatePayloadId(), Result);
		PublishSync(FObjectKey(&Terrain), Rail, Tier, TileKey, Payload);
		if (!Payload.IsValid())
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
			++Tiles.IntervalStats.SyncFailures;
			++Tiles.LifetimeStats.SyncFailures;
		}
		RecordReadOutcome(FObjectKey(&Terrain), Rail, Tier, TileKey, Result, GridSignature(Grid));
		// IsOn() ahead of the call: PayloadBytes walks every plane, and an argument is evaluated whatever the
		// inline test inside does with it.
		if (Payload.IsValid() && ArcTerrainDiag::IsOn())
		{
			ArcTerrainDiag::NoteTileArrived(Rail, Tier, TileKey, Payload->PayloadId, PayloadBytes(*Payload));
		}
		return Payload;
	}

	void GatherLoadedTileKeys(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		TArray<FIntPoint>& OutKeys)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		OutKeys.Reset();
		if (!IsValidTier(Tier))
		{
			return;
		}

		FScopeLock Lock(&GStoreLock);
		if (const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
		{
			const TMap<FIntPoint, FArcTerrainTilePayloadPtr>& Loaded =
				Tiles->Loaded[RailIndex(Rail)][Tier];
			OutKeys.Reserve(Loaded.Num());
			for (const TPair<FIntPoint, FArcTerrainTilePayloadPtr>& Pair : Loaded)
			{
				OutKeys.Add(Pair.Key);
			}
		}
	}

	void GatherManifestTileKeys(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		TArray<FIntPoint>& OutKeys)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		OutKeys.Reset();
		if (!IsValidTier(Tier))
		{
			return;
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return;
		}
		EnsureManifestLoaded(Terrain, TileDir);

		FScopeLock Lock(&GStoreLock);
		if (const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
		{
			const TMap<FIntVector, FIoHash>& Entries = Tiles->GetManifest().Rail(Rail);
			OutKeys.Reserve(Entries.Num());
			for (const TPair<FIntVector, FIoHash>& Entry : Entries)
			{
				if (Entry.Key.X == Tier)
				{
					OutKeys.Add(FIntPoint(Entry.Key.Y, Entry.Key.Z));
				}
			}
		}
	}

	int32 GatherManifestCoverage(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		TArray<FArcTerrainManifestTile>& OutTiles)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		OutTiles.Reset();
		if (!IsValidTier(Tier))
		{
			return 0;
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return 0;
		}
		EnsureManifestLoaded(Terrain, TileDir);

		FScopeLock Lock(&GStoreLock);
		const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
		if (Tiles == nullptr)
		{
			return 0;
		}

		const TMap<FIntVector, FIoHash>& Entries = Tiles->GetManifest().Rail(Rail);
		const TMap<FIntPoint, uint32>& Unusable = Tiles->GetUnusable(Rail, Tier);
		OutTiles.Reserve(FMath::Min(Entries.Num(), MaxManifestCoverageKeys));
		int32 NumTierEntries = 0;
		for (const TPair<FIntVector, FIoHash>& Entry : Entries)
		{
			if (Entry.Key.X != Tier)
			{
				continue;
			}
			++NumTierEntries;
			if (OutTiles.Num() >= MaxManifestCoverageKeys)
			{
				continue;
			}
			FArcTerrainManifestTile& Tile = OutTiles.AddDefaulted_GetRef();
			Tile.TileKey = FIntPoint(Entry.Key.Y, Entry.Key.Z);
			Tile.bUnusable = Unusable.Contains(Tile.TileKey);
		}
		return NumTierEntries;
	}

	uint32 GetManifestCoverageEpoch()
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		return GCoverageEpoch.load(std::memory_order_relaxed);
	}

	uint32 GetLoadedCoverageEpoch()
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		return GLoadedCoverageEpoch.load(std::memory_order_relaxed);
	}

	void GatherStoreStats(const AArcTerrain& Terrain, bool bLoadManifest,
		FArcTerrainTileStoreStats& OutStats)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		OutStats = FArcTerrainTileStoreStats();
		OutStats.ProcessLoadedCoverageEpoch = GetLoadedCoverageEpoch();

		if (bLoadManifest)
		{
			const FString TileDir = ResolveTileDir(Terrain);
			if (!TileDir.IsEmpty())
			{
				EnsureManifestLoaded(Terrain, TileDir);
			}
		}

		FScopeLock Lock(&GStoreLock);
		const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
		if (Tiles == nullptr)
		{
			return;
		}
		OutStats.bManifestLoaded = Tiles->IsManifestLoaded();
		OutStats.LoadedMutationSerial = Tiles->LoadedMutationSerial;
		OutStats.Interval = Tiles->IntervalStats;
		OutStats.Lifetime = Tiles->LifetimeStats;
		for (int32 Rail = 0; Rail < ArcTerrain::NumTileRails; ++Rail)
		{
			for (int32 Tier = 0; Tier < ArcTerrain::MaxTiers; ++Tier)
			{
				OutStats.Loaded[Rail][Tier] += Tiles->Loaded[Rail][Tier].Num();
				OutStats.InFlight[Rail][Tier] += Tiles->InFlight[Rail][Tier].Num();
				OutStats.Unusable[Rail][Tier] +=
					Tiles->GetUnusable(static_cast<EArcTerrainRail>(Rail), Tier).Num();
			}
			for (const TPair<FIntVector, FIoHash>& Entry :
				Tiles->GetManifest().Rail(static_cast<EArcTerrainRail>(Rail)))
			{
				const int32 EntryTier = FMath::Clamp(Entry.Key.X, 0, ArcTerrain::MaxTiers - 1);
				++OutStats.ManifestEntries[Rail][EntryTier];
			}
		}
	}

	uint64 GetLoadedMutationSerial(const AArcTerrain& Terrain)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		FScopeLock Lock(&GStoreLock);
		const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
		return Tiles != nullptr ? Tiles->LoadedMutationSerial : 0;
	}

	void ResetStoreIntervalStats(const AArcTerrain& Terrain)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		FScopeLock Lock(&GStoreLock);
		if (FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
		{
			Tiles->IntervalStats = FArcTerrainTileStoreMutationCounters();
		}
	}

	void GatherTileResidency(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint MinKey,
		FIntPoint MaxKey, TArray<EArcTerrainTileResidency>& OutStates)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		OutStates.Reset();
		if (!IsValidTier(Tier))
		{
			return;
		}

		const int64 Width = static_cast<int64>(MaxKey.X) - static_cast<int64>(MinKey.X) + 1;
		const int64 Height = static_cast<int64>(MaxKey.Y) - static_cast<int64>(MinKey.Y) + 1;
		if (Width <= 0 || Height <= 0 || Width * Height > static_cast<int64>(MaxTileResidencyKeys))
		{
			return;
		}
		OutStates.SetNumUninitialized(static_cast<int32>(Width * Height));

		FScopeLock Lock(&GStoreLock);
		const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
		if (Tiles == nullptr)
		{
			for (EArcTerrainTileResidency& State : OutStates)
			{
				State = EArcTerrainTileResidency::Unknown;
			}
			return;
		}

		const int32 Index = RailIndex(Rail);
		const EArcTerrainTileResidency NoEntry = Tiles->IsManifestLoaded()
			? EArcTerrainTileResidency::Absent
			: EArcTerrainTileResidency::Unknown;

		int32 Slot = 0;
		for (int32 Y = MinKey.Y; Y <= MaxKey.Y; ++Y)
		{
			for (int32 X = MinKey.X; X <= MaxKey.X; ++X, ++Slot)
			{
				const FIntPoint TileKey(X, Y);
				if (Tiles->Loaded[Index][Tier].Contains(TileKey))
				{
					OutStates[Slot] = EArcTerrainTileResidency::Resident;
				}
				else if (Tiles->InFlight[Index][Tier].Contains(TileKey))
				{
					OutStates[Slot] = EArcTerrainTileResidency::InFlight;
				}
				else if (Tiles->GetUnusable(Rail, Tier).Contains(TileKey))
				{
					OutStates[Slot] = EArcTerrainTileResidency::Unusable;
				}
				else if (Tiles->GetManifest().Contains(Rail, Tier, TileKey))
				{
					OutStates[Slot] = EArcTerrainTileResidency::InManifest;
				}
				else
				{
					OutStates[Slot] = NoEntry;
				}
			}
		}
	}

	bool ProbeTile(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		FArcTerrainTileProbe& OutProbe)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		OutProbe = FArcTerrainTileProbe();
		if (!IsValidTier(Tier) || !ResolveGrid(Terrain, Rail, Tier, OutProbe.Grid))
		{
			return false;
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return true;
		}
		EnsureManifestLoaded(Terrain, TileDir);
		OutProbe.FilePath = MakeTilePath(TileDir, Rail, Tier, TileKey);

		{
			FScopeLock Lock(&GStoreLock);
			if (const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
			{
				OutProbe.bInManifest = Tiles->GetManifest().Contains(Rail, Tier, TileKey);
				OutProbe.bLoaded = Tiles->Loaded[RailIndex(Rail)][Tier].Contains(TileKey);
				OutProbe.bSuppressedUnusable = Tiles->GetUnusable(Rail, Tier).Contains(TileKey);
			}
		}

		OutProbe.bFileExists = IFileManager::Get().FileExists(*OutProbe.FilePath);
		if (!OutProbe.bFileExists)
		{
			return true;
		}
		OutProbe.FileBytes = IFileManager::Get().FileSize(*OutProbe.FilePath);

		TArray<uint8> FileBytes;
		if (!ArcTerrainTileIO::LoadFileBytes(OutProbe.FilePath, FileBytes))
		{
			OutProbe.Mismatch = TEXT("file could not be read");
			return true;
		}
		OutProbe.bParsed = ArcTerrainTileFormat::ParseHeader(FileBytes, OutProbe.Header);
		if (!OutProbe.bParsed)
		{
			OutProbe.Mismatch = TEXT("header did not parse (magic, version, plane order or length)");
			return true;
		}

		OutProbe.bCompatible = ArcTerrainTileFormat::IsCompatible(OutProbe.Header, Rail, OutProbe.Grid);
		if (!OutProbe.bCompatible)
		{
			TArray<FString> Differences;
			if (OutProbe.Header.Rail != Rail)
			{
				Differences.Add(FString::Printf(TEXT("rail %u != %u"),
					static_cast<uint32>(OutProbe.Header.Rail), static_cast<uint32>(Rail)));
			}
			if (OutProbe.Header.Resolution != OutProbe.Grid.Resolution())
			{
				Differences.Add(FString::Printf(TEXT("resolution %d != %d"), OutProbe.Header.Resolution,
					OutProbe.Grid.Resolution()));
			}
			if (OutProbe.Header.TileExtentM != static_cast<uint32>(OutProbe.Grid.TileExtentM))
			{
				Differences.Add(FString::Printf(TEXT("tile extent %u m != %d m"), OutProbe.Header.TileExtentM,
					OutProbe.Grid.TileExtentM));
			}
			if (OutProbe.Header.TexelSizeCm != static_cast<uint32>(OutProbe.Grid.TexelSizeCm))
			{
				Differences.Add(FString::Printf(TEXT("texel %u cm != %d cm"), OutProbe.Header.TexelSizeCm,
					OutProbe.Grid.TexelSizeCm));
			}
			OutProbe.Mismatch = Differences.IsEmpty()
				? FString(TEXT("the live lattice is invalid"))
				: FString::Join(Differences, TEXT(", "));
		}
		return true;
	}

	void RequestTileAsync(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		TFunction<void(EArcTerrainRail, int32, FIntPoint, FArcTerrainTilePayloadPtr)> OnLoaded)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		FArcTerrainTileGrid Grid;
		if (!ResolveGrid(Terrain, Rail, Tier, Grid) || !HasTile(Terrain, Rail, Tier, TileKey, &Grid))
		{
			if (OnLoaded)
			{
				OnLoaded(Rail, Tier, TileKey, FArcTerrainTilePayloadPtr());
			}
			return;
		}

		const FObjectKey TerrainKey(&Terrain);
		const FString TileDir = ResolveTileDir(Terrain);
		const FString FilePath = MakeTilePath(TileDir, Rail, Tier, TileKey);
		const uint32 PayloadId = AllocatePayloadId();
		const uint32 Signature = GridSignature(Grid);
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles& Tiles = GStore.FindOrAdd(TerrainKey);
			if (Tiles.InFlight[RailIndex(Rail)][Tier].Contains(TileKey))
			{
				return;
			}
			Tiles.InFlight[RailIndex(Rail)][Tier].Add(TileKey, PayloadId);
			Tiles.BumpLoadedMutation();
		}
		ArcTerrainDiag::NoteStreamRequest(Rail, Tier, TileKey);

		UE::Tasks::Launch(UE_SOURCE_LOCATION,
			[TerrainKey, Rail, Tier, TileKey, Grid, FilePath, PayloadId, Signature,
				Callback = MoveTemp(OnLoaded)]() mutable
			{
				EArcTerrainTileLoad Result = EArcTerrainTileLoad::Loaded;
				FArcTerrainTilePayloadPtr Payload =
					ReadPayload(FilePath, Rail, Tier, TileKey, Grid, PayloadId, Result);
				// The publish and the callback both land on the game thread: the callback's consumers are
				// game-thread terrain state, and publishing there keeps the two in one order.
				AsyncTask(ENamedThreads::GameThread,
					[TerrainKey, Rail, Tier, TileKey, PayloadId, Result, Signature,
						Payload = MoveTemp(Payload), Callback = MoveTemp(Callback)]()
					{
						const bool bPublished =
							PublishRequested(TerrainKey, Rail, Tier, TileKey, Payload, PayloadId);
						if (bPublished && !Payload.IsValid())
						{
							RecordReadOutcome(TerrainKey, Rail, Tier, TileKey, Result, Signature);
						}
						else if (bPublished && ArcTerrainDiag::IsOn())
						{
							ArcTerrainDiag::NoteTileArrived(Rail, Tier, TileKey, Payload->PayloadId,
								PayloadBytes(*Payload));
						}
						if (Callback)
						{
							Callback(Rail, Tier, TileKey,
								bPublished ? Payload : FArcTerrainTilePayloadPtr());
						}
					});
			});
	}

	bool ReleaseTileIfUnreferenced(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!IsValidTier(Tier))
		{
			return false;
		}

		bool bReleased = false;
		{
			FScopeLock Lock(&GStoreLock);
			FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
			if (Tiles == nullptr)
			{
				return false;
			}
			const FArcTerrainTilePayloadPtr* Found =
				Tiles->Loaded[RailIndex(Rail)][Tier].Find(TileKey);
			if (Found != nullptr && Found->GetSharedReferenceCount() > 1)
			{
				return false;
			}
			Tiles->Loaded[RailIndex(Rail)][Tier].Remove(TileKey);
			Tiles->InFlight[RailIndex(Rail)][Tier].Remove(TileKey);
			++Tiles->IntervalStats.Releases;
			++Tiles->LifetimeStats.Releases;
			Tiles->BumpLoadedMutation();
			ArcTerrainDiag::NoteTileReleased(Rail, Tier, TileKey);
			bReleased = true;
		}
		if (bReleased)
		{
			if (UWorld* World = Terrain.GetWorld())
			{
				if (UArcTerrainSubsystem* Subsystem = World->GetSubsystem<UArcTerrainSubsystem>())
				{
					FArcTerrainTileAuthorityEvent Event;
					Event.Rail = Rail;
					Event.Tier = Tier;
					Event.TileKey = TileKey;
					Event.Reason = EArcTerrainTileAuthorityReason::TileReleased;
					Subsystem->NotifyLocalTileAuthority(Terrain, Event);
				}
			}
		}
		return bReleased;
	}

	void FlushManifest(const AArcTerrain& Terrain)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

#if WITH_EDITOR
		FPendingManifestSave Pending;
		{
			FScopeLock Lock(&GStoreLock);
			if (FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
			{
				TakePendingManifestSave(*Tiles, Pending);
			}
		}
		if (!Pending.TileDir.IsEmpty())
		{
			SavePendingManifest(Pending);
			BumpCoverageEpoch();
		}
#endif
	}

	void ReleaseTerrainTiles(const AArcTerrain& Terrain)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

#if WITH_EDITOR
		// The drop of a manifest entry is deferred, and this is the last moment it exists: a load outside
		// the stream (an editor baker, the collision bootstrap) can find an entry with no file in a session
		// that never ticks the stream, and dropping the state without the write loses that finding.
		FPendingManifestSave Pending;
		{
			FScopeLock Lock(&GStoreLock);
			if (FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain)))
			{
				TakePendingManifestSave(*Tiles, Pending);
			}
			RemoveTerrainFromStore(FObjectKey(&Terrain));
		}
		SavePendingManifest(Pending);
#else
		FScopeLock Lock(&GStoreLock);
		RemoveTerrainFromStore(FObjectKey(&Terrain));
#endif
	}

	void ReleaseAllTiles()
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

#if WITH_EDITOR
		TArray<FPendingManifestSave> Pendings;
		{
			FScopeLock Lock(&GStoreLock);
			Pendings.Reserve(GStore.Num());
			for (TPair<FObjectKey, FTerrainTiles>& Pair : GStore)
			{
				TakePendingManifestSave(Pair.Value, Pendings.AddDefaulted_GetRef());
			}
			ClearStore();
		}
		for (const FPendingManifestSave& Pending : Pendings)
		{
			SavePendingManifest(Pending);
		}
#else
		FScopeLock Lock(&GStoreLock);
		ClearStore();
#endif
	}

	FIoHash FindTileWrittenKey(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier,
		FIntPoint TileKey)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		check(IsInGameThread());

		if (!IsValidTier(Tier))
		{
			return FIoHash();
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return FIoHash();
		}
		EnsureManifestLoaded(Terrain, TileDir);

		FScopeLock Lock(&GStoreLock);
		const FTerrainTiles* Tiles = GStore.Find(FObjectKey(&Terrain));
		return Tiles != nullptr ? Tiles->GetManifest().FindRecipeKey(Rail, Tier, TileKey) : FIoHash();
	}

#if WITH_EDITOR
	void NotifyTileWritten(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey,
		const FIoHash& RecipeKey)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!IsValidTier(Tier))
		{
			return;
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return;
		}
		EnsureManifestLoaded(Terrain, TileDir);

		FScopeLock Lock(&GStoreLock);
		FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
		Tiles.AddManifestEntry(Rail, Tier, TileKey, RecipeKey);
		// The bytes on disk are new, so both the cached payload and any read still running against the
		// old ones describe a tile that no longer exists. Dropping them leaves the key not-loaded and
		// not in flight, which is what makes the stream request it again under a fresh PayloadId.
		Tiles.Loaded[RailIndex(Rail)][Tier].Remove(TileKey);
		Tiles.InFlight[RailIndex(Rail)][Tier].Remove(TileKey);
		Tiles.ClearUnusable(Rail, Tier, TileKey);
		Tiles.BumpLoadedMutation();
		ArcTerrainTileManifest::Save(TileDir, Tiles.GetManifest());
	}

	void NotifyTileDropped(const AArcTerrain& Terrain, EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!IsValidTier(Tier))
		{
			return;
		}

		const FString TileDir = ResolveTileDir(Terrain);
		if (TileDir.IsEmpty())
		{
			return;
		}
		EnsureManifestLoaded(Terrain, TileDir);

		FScopeLock Lock(&GStoreLock);
		FTerrainTiles& Tiles = GStore.FindOrAdd(FObjectKey(&Terrain));
		if (!Tiles.GetManifest().Contains(Rail, Tier, TileKey))
		{
			return;
		}
		Tiles.RemoveManifestEntry(Rail, Tier, TileKey);
		Tiles.Loaded[RailIndex(Rail)][Tier].Remove(TileKey);
		Tiles.InFlight[RailIndex(Rail)][Tier].Remove(TileKey);
		Tiles.ClearUnusable(Rail, Tier, TileKey);
		Tiles.BumpLoadedMutation();
		ArcTerrainTileManifest::Save(TileDir, Tiles.GetManifest());
	}
#endif // WITH_EDITOR

	bool ExtractCellFloats(const FArcTerrainTileGrid& Grid, const FArcTerrainTilePayload& Payload,
		int32 PlaneIndex, FArcTerrainCellKey CellKey, TArray<float>& OutFloats, int32& OutResolution)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (Payload.Rail != EArcTerrainRail::Height || PlaneIndex < 0
			|| PlaneIndex >= ArcTerrainTileFormat::HeightPlaneCount)
		{
			return false;
		}

		const int32 TileRes = Payload.Resolution();
		const int64 PlaneTexels = static_cast<int64>(TileRes) * TileRes;
		if (static_cast<int64>(Payload.Floats.Num())
			< PlaneTexels * ArcTerrainTileFormat::HeightPlaneCount)
		{
			return false;
		}

		FIntPoint Origin = FIntPoint::ZeroValue;
		int32 Extent = 0;
		if (!ResolveCellSubRect(Grid, Payload, CellKey, Origin, Extent))
		{
			return false;
		}

		const float* Plane = Payload.Floats.GetData() + PlaneIndex * PlaneTexels;
		OutFloats.SetNumUninitialized(Extent * Extent);
		for (int32 Row = 0; Row < Extent; ++Row)
		{
			FMemory::Memcpy(OutFloats.GetData() + static_cast<int64>(Row) * Extent,
				Plane + static_cast<int64>(Origin.Y + Row) * TileRes + Origin.X,
				static_cast<int64>(Extent) * sizeof(float));
		}
		OutResolution = Extent;
		return true;
	}

	bool ExtractCellPlane(const FArcTerrainTileGrid& Grid, const FArcTerrainTilePayload& Payload,
		int32 PlaneIndex, FArcTerrainCellKey CellKey, TArray<uint8>& OutBytes, int32& OutResolution)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!Payload.Planes.IsValidIndex(PlaneIndex))
		{
			return false;
		}

		const int32 TileRes = Payload.Resolution();
		if (static_cast<int64>(Payload.Planes[PlaneIndex].Num()) != static_cast<int64>(TileRes) * TileRes)
		{
			return false;
		}

		FIntPoint Origin = FIntPoint::ZeroValue;
		int32 Extent = 0;
		if (!ResolveCellSubRect(Grid, Payload, CellKey, Origin, Extent)
			|| !CopyPlaneSubRect(Payload, PlaneIndex, Origin, Extent, OutBytes))
		{
			return false;
		}

		OutResolution = Extent;
		return true;
	}

	bool ExtractGroupPlane(const FArcTerrainTileGrid& Grid, const FArcTerrainTilePayload& Payload,
		int32 PlaneIndex, FIntPoint GroupKey, int32 GroupQuads, int32 GroupsPerTileEdge,
		TArray<uint8>& OutBytes, int32& OutResolution)
	{
		using namespace ArcTerrainTileAccessPrivate_4e18;

		if (!Payload.Planes.IsValidIndex(PlaneIndex))
		{
			return false;
		}

		const int32 TileRes = Payload.Resolution();
		if (static_cast<int64>(Payload.Planes[PlaneIndex].Num()) != static_cast<int64>(TileRes) * TileRes)
		{
			return false;
		}

		FIntPoint Origin = FIntPoint::ZeroValue;
		int32 Extent = 0;
		if (!ResolveGroupSubRect(Grid, Payload, GroupKey, GroupQuads, GroupsPerTileEdge, Origin, Extent)
			|| !CopyPlaneSubRect(Payload, PlaneIndex, Origin, Extent, OutBytes))
		{
			return false;
		}

		OutResolution = Extent;
		return true;
	}

	bool LoadCellHeightsSync(const AArcTerrain& Terrain, FArcTerrainCellKey CellKey, TArray<float>& OutHeights,
		int32& OutResolution)
	{
		FIntPoint TileKey = FIntPoint::ZeroValue;
		if (!TileKeyFromCell(Terrain, EArcTerrainRail::Height, 0, CellKey, TileKey))
		{
			return false;
		}

		FArcTerrainTileGrid Grid;
		if (!ResolveGrid(Terrain, EArcTerrainRail::Height, 0, Grid))
		{
			return false;
		}

		const FArcTerrainTilePayloadPtr Payload = LoadTileSync(Terrain, EArcTerrainRail::Height, 0, TileKey);
		if (!Payload.IsValid())
		{
			return false;
		}
		return ExtractCellFloats(Grid, *Payload, 0, CellKey, OutHeights, OutResolution);
	}
}
