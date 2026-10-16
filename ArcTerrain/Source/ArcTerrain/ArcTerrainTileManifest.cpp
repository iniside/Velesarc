// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileManifest.h"

#include "ArcTerrainModule.h"
#include "ArcTerrainTileIO.h"
#include "ArcTerrainTileNames.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace ArcTerrainTileManifestPrivate_5c07
{
	constexpr uint32 ManifestMagic = uint32('A') | (uint32('I') << 8) | (uint32('D') << 16) | (uint32('X') << 24);
	constexpr uint32 ManifestVersion = 2;
	constexpr int32 ManifestHeaderBytes = 20;
	constexpr int32 HashBytes = static_cast<int32>(sizeof(FIoHash::ByteArray));
	constexpr int32 EntryBytes = 12 + HashBytes;

	constexpr EArcTerrainRail RailOrder[ArcTerrain::NumTileRails] = {
		EArcTerrainRail::Height, EArcTerrainRail::Weight, EArcTerrainRail::Data,
	};

	void WriteU32(TArray<uint8>& Out, uint32 Value)
	{
		Out.Add(static_cast<uint8>(Value & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 8) & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 16) & 0xFFu));
		Out.Add(static_cast<uint8>((Value >> 24) & 0xFFu));
	}

	uint32 ReadU32(const uint8* Bytes)
	{
		return static_cast<uint32>(Bytes[0]) | (static_cast<uint32>(Bytes[1]) << 8)
			| (static_cast<uint32>(Bytes[2]) << 16) | (static_cast<uint32>(Bytes[3]) << 24);
	}

	void WriteHash(TArray<uint8>& Out, const FIoHash& Hash)
	{
		Out.Append(Hash.GetBytes(), HashBytes);
	}

	FIoHash ReadHash(const uint8* Bytes)
	{
		return FIoHash::FromView(MakeMemoryView(Bytes, HashBytes));
	}

	// Sorted so the same set always produces the same bytes.
	TArray<FIntVector> SortedKeys(const TMap<FIntVector, FIoHash>& Entries)
	{
		TArray<FIntVector> Sorted;
		Entries.GetKeys(Sorted);
		Sorted.Sort([](const FIntVector& A, const FIntVector& B)
		{
			if (A.X != B.X)
			{
				return A.X < B.X;
			}
			return A.Z != B.Z ? A.Z < B.Z : A.Y < B.Y;
		});
		return Sorted;
	}
}

TMap<FIntVector, FIoHash>& FArcTerrainTileManifest::Rail(EArcTerrainRail InRail)
{
	switch (InRail)
	{
	case EArcTerrainRail::Weight:
		return WeightTiles;
	case EArcTerrainRail::Data:
		return DataTiles;
	default:
		return HeightTiles;
	}
}

const TMap<FIntVector, FIoHash>& FArcTerrainTileManifest::Rail(EArcTerrainRail InRail) const
{
	return const_cast<FArcTerrainTileManifest*>(this)->Rail(InRail);
}

void FArcTerrainTileManifest::Reset()
{
	HeightTiles.Reset();
	WeightTiles.Reset();
	DataTiles.Reset();
}

namespace ArcTerrainTileManifest
{
	FString MakeManifestPath(const FString& TileDir)
	{
		return TileDir.IsEmpty() ? FString() : FPaths::Combine(TileDir, ManifestFileName);
	}

#if WITH_EDITOR
	bool Save(const FString& TileDir, const FArcTerrainTileManifest& Manifest)
	{
		using namespace ArcTerrainTileManifestPrivate_5c07;

		const FString ManifestPath = MakeManifestPath(TileDir);
		if (ManifestPath.IsEmpty())
		{
			return false;
		}

		TArray<uint8> FileBytes;
		FileBytes.Reserve(ManifestHeaderBytes + Manifest.Num() * EntryBytes);
		WriteU32(FileBytes, ManifestMagic);
		WriteU32(FileBytes, ManifestVersion);
		for (EArcTerrainRail Rail : RailOrder)
		{
			WriteU32(FileBytes, static_cast<uint32>(Manifest.Rail(Rail).Num()));
		}

		for (EArcTerrainRail Rail : RailOrder)
		{
			const TMap<FIntVector, FIoHash>& Entries = Manifest.Rail(Rail);
			const TArray<FIntVector> Keys = SortedKeys(Entries);
			for (const FIntVector& Key : Keys)
			{
				WriteU32(FileBytes, static_cast<uint32>(Key.X));
				WriteU32(FileBytes, static_cast<uint32>(Key.Y));
				WriteU32(FileBytes, static_cast<uint32>(Key.Z));
				WriteHash(FileBytes, Entries.FindChecked(Key));
			}
		}

		return ArcTerrainTileIO::SaveFileAtomic(ManifestPath, FileBytes);
	}
#endif // WITH_EDITOR

	bool Load(const FString& TileDir, FArcTerrainTileManifest& OutManifest)
	{
		using namespace ArcTerrainTileManifestPrivate_5c07;

		OutManifest.Reset();

		TArray<uint8> FileBytes;
		if (!ArcTerrainTileIO::LoadFileBytes(MakeManifestPath(TileDir), FileBytes))
		{
			return false;
		}

		if (FileBytes.Num() < ManifestHeaderBytes)
		{
			return false;
		}

		const uint8* Bytes = FileBytes.GetData();
		if (ReadU32(Bytes) != ManifestMagic || ReadU32(Bytes + 4) != ManifestVersion)
		{
			return false;
		}

		int64 TotalEntries = 0;
		int32 RailCounts[ArcTerrain::NumTileRails] = {};
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			const uint32 Count = ReadU32(Bytes + 8 + RailIndex * 4);
			if (Count > static_cast<uint32>(MAX_int32))
			{
				return false;
			}
			RailCounts[RailIndex] = static_cast<int32>(Count);
			TotalEntries += static_cast<int64>(Count);
		}

		if (static_cast<int64>(FileBytes.Num()) != static_cast<int64>(ManifestHeaderBytes) + TotalEntries * EntryBytes)
		{
			return false;
		}

		int32 ByteOffset = ManifestHeaderBytes;
		for (int32 RailIndex = 0; RailIndex < ArcTerrain::NumTileRails; ++RailIndex)
		{
			TMap<FIntVector, FIoHash>& Entries = OutManifest.Rail(RailOrder[RailIndex]);
			Entries.Reserve(RailCounts[RailIndex]);
			for (int32 EntryIndex = 0; EntryIndex < RailCounts[RailIndex]; ++EntryIndex)
			{
				const int32 Tier = static_cast<int32>(ReadU32(Bytes + ByteOffset));
				const int32 X = static_cast<int32>(ReadU32(Bytes + ByteOffset + 4));
				const int32 Y = static_cast<int32>(ReadU32(Bytes + ByteOffset + 8));
				Entries.Add(FIntVector(Tier, X, Y), ReadHash(Bytes + ByteOffset + 12));
				ByteOffset += EntryBytes;
			}
		}

		return true;
	}

	void RebuildFromDisk(const FString& TileDir, FArcTerrainTileManifest& OutManifest)
	{
		using namespace ArcTerrainTileManifestPrivate_5c07;

		OutManifest.Reset();
		if (TileDir.IsEmpty())
		{
			return;
		}

		IFileManager& FileManager = IFileManager::Get();
		for (EArcTerrainRail Rail : RailOrder)
		{
			TArray<FString> FileNames;
			const FString Pattern = FPaths::Combine(TileDir, ArcTerrainTileNames::MakeTileFileWildcard(Rail));
			FileManager.FindFiles(FileNames, *Pattern, /*Files=*/true, /*Directories=*/false);

			TMap<FIntVector, FIoHash>& Entries = OutManifest.Rail(Rail);
			Entries.Reserve(FileNames.Num());
			for (const FString& FileName : FileNames)
			{
				int32 Tier = 0;
				FIntPoint TileKey;
				if (ArcTerrainTileNames::ParseTileFileName(Rail, FileName, Tier, TileKey))
				{
					Entries.Add(FArcTerrainTileManifest::MakeKey(Tier, TileKey), FIoHash());
				}
			}
		}

		UE_LOGF(LogArcTerrain, Verbose,"Tile manifest rebuilt from '%ls': %d height, %d weight, %d data.",
			*TileDir, OutManifest.HeightTiles.Num(), OutManifest.WeightTiles.Num(), OutManifest.DataTiles.Num());
	}
}
