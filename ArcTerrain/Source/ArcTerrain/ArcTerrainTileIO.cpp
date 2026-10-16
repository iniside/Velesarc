// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileIO.h"

#include "ArcTerrainModule.h"
#include "Compression/OodleDataCompression.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace ArcTerrainTileIOPrivate_2f68
{
	FString MakeTempPath(const FString& FilePath)
	{
		return FString::Printf(TEXT("%s.%s.tmp"), *FilePath, *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	}

	constexpr FOodleDataCompression::ECompressor BodyCompressor = FOodleDataCompression::ECompressor::Kraken;
	constexpr FOodleDataCompression::ECompressionLevel BodyCompressionLevel =
		FOodleDataCompression::ECompressionLevel::Optimal3;

	/** DecompressParallel can only use more than one thread when the encode set this
	 *  (OodleDataCompression.h:195). */
	constexpr bool bBodyIndependentChunks = true;
}

namespace ArcTerrainTileIO
{
	bool SaveFileAtomic(const FString& FilePath, TConstArrayView<uint8> FileBytes)
	{
		using namespace ArcTerrainTileIOPrivate_2f68;

		if (FilePath.IsEmpty() || FileBytes.Num() <= 0)
		{
			return false;
		}

		IFileManager& FileManager = IFileManager::Get();
		const FString Directory = FPaths::GetPath(FilePath);
		if (!Directory.IsEmpty() && !FileManager.MakeDirectory(*Directory, /*Tree=*/true))
		{
			UE_LOGF(LogArcTerrain, Error,"Tile write failed: cannot create '%ls'.", *Directory);
			return false;
		}

		const FString TempPath = MakeTempPath(FilePath);
		const TArrayView64<const uint8> Payload(FileBytes.GetData(), static_cast<int64>(FileBytes.Num()));
		if (!FFileHelper::SaveArrayToFile(Payload, *TempPath, &FileManager))
		{
			UE_LOGF(LogArcTerrain, Error,"Tile write failed: cannot write '%ls'.", *TempPath);
			return false;
		}

		if (!FileManager.Move(*FilePath, *TempPath, /*Replace=*/true, /*EvenIfReadOnly=*/true))
		{
			UE_LOGF(LogArcTerrain, Error,"Tile write failed: cannot move '%ls' into place.", *TempPath);
			FileManager.Delete(*TempPath, /*RequireExists=*/false, /*EvenIfReadOnly=*/true, /*Quiet=*/true);
			return false;
		}

		return true;
	}

	int64 SaveTile(const FString& FilePath, const FArcTerrainTileHeader& Header, TConstArrayView<uint8> Body)
	{
		using namespace ArcTerrainTileIOPrivate_2f68;

		if (!Header.IsValid())
		{
			UE_LOGF(LogArcTerrain, Error,"Tile write refused: '%ls' would carry an invalid header (rail %u, %d texels, %d planes).",
				*FilePath, static_cast<uint32>(Header.Rail), Header.Resolution,
				ArcTerrainTileFormat::NumPlaneIds(Header));
			return 0;
		}

		const int64 HeaderBytes = ArcTerrainTileFormat::HeaderBytes(Header);
		const int64 BodyBytes = static_cast<int64>(Body.Num());
		const int64 ExpectedBodyBytes = static_cast<int64>(ArcTerrainTileFormat::NumBodyPlanes(Header))
			* ArcTerrainTileFormat::PlaneBytes(Header);
		if (BodyBytes != ExpectedBodyBytes)
		{
			UE_LOGF(LogArcTerrain, Error,"Tile write refused: body of %" INT64_FMT " B is not the %d planes of %" INT64_FMT
					" B the header of '%ls' describes.",
				BodyBytes, ArcTerrainTileFormat::NumBodyPlanes(Header), ArcTerrainTileFormat::PlaneBytes(Header),
				*FilePath);
			return 0;
		}

		// The encode scratch and the assembled file are both TArray<uint8>, which sizes in int32.
		const int64 CompressedBufferSize = FOodleDataCompression::CompressedBufferSizeNeeded(BodyBytes);
		if (CompressedBufferSize > static_cast<int64>(MAX_int32) - HeaderBytes)
		{
			UE_LOGF(LogArcTerrain, Error,"Tile write refused: body of %" INT64_FMT " B needs %" INT64_FMT
					" B of encode buffer plus a %" INT64_FMT " B header, over the %d an array can hold ('%ls').",
				BodyBytes, CompressedBufferSize, HeaderBytes, MAX_int32, *FilePath);
			return 0;
		}

		TArray<uint8> CompressedBody;
		CompressedBody.SetNumUninitialized(static_cast<int32>(CompressedBufferSize));
		const int64 CompressedBytes = FOodleDataCompression::CompressParallel(CompressedBody.GetData(),
			CompressedBufferSize, Body.GetData(), BodyBytes, BodyCompressor, BodyCompressionLevel,
			bBodyIndependentChunks);

		const bool bStoreCompressed = CompressedBytes > 0 && CompressedBytes < BodyBytes;

		FArcTerrainTileHeader StampedHeader = Header;
		StampedHeader.CompressionMethod = bStoreCompressed
			? ArcTerrainTileFormat::CompressionOodleKraken
			: ArcTerrainTileFormat::CompressionNone;
		StampedHeader.UncompressedBodyBytes = static_cast<uint32>(BodyBytes);
		StampedHeader.CompressedBodyBytes = static_cast<uint32>(bStoreCompressed ? CompressedBytes : BodyBytes);

		TArray<uint8> FileBytes;
		FileBytes.Reserve(static_cast<int32>(HeaderBytes)
			+ static_cast<int32>(StampedHeader.CompressedBodyBytes));
		ArcTerrainTileFormat::SerializeHeader(StampedHeader, FileBytes);
		if (bStoreCompressed)
		{
			FileBytes.Append(CompressedBody.GetData(), static_cast<int32>(CompressedBytes));
		}
		else
		{
			FileBytes.Append(Body.GetData(), Body.Num());
		}

		return SaveFileAtomic(FilePath, FileBytes) ? static_cast<int64>(FileBytes.Num()) : 0;
	}

	bool LoadFileBytes(const FString& FilePath, TArray<uint8>& OutFileBytes)
	{
		return !FilePath.IsEmpty() && FFileHelper::LoadFileToArray(OutFileBytes, *FilePath);
	}

	EArcTerrainTileLoad LoadTile(const FString& FilePath, EArcTerrainRail ExpectedRail,
		const FArcTerrainTileGrid& ExpectedGrid, FArcTerrainTileHeader& OutHeader, TArray<uint8>& OutFileBytes)
	{
		if (!ExpectedGrid.IsValid())
		{
			// Says nothing about the file, so nothing about the file may be recorded.
			return EArcTerrainTileLoad::ReadFailed;
		}

		TArray<uint8> FileBytes;
		if (!LoadFileBytes(FilePath, FileBytes))
		{
			// A failed OPEN is never Unusable: the file is there, nothing about it has been decoded, and
			// suppressing the key on it would hide a perfectly good tile until the editor restarts.
			return IFileManager::Get().FileExists(*FilePath)
				? EArcTerrainTileLoad::ReadFailed
				: EArcTerrainTileLoad::FileMissing;
		}

		FArcTerrainTileHeader Header;
		if (!ArcTerrainTileFormat::ParseHeader(FileBytes, Header))
		{
			UE_LOGF(LogArcTerrain, Warning,"Tile '%ls' is corrupt and counts as absent.", *FilePath);
			return EArcTerrainTileLoad::Unusable;
		}

		if (!ArcTerrainTileFormat::IsCompatible(Header, ExpectedRail, ExpectedGrid))
		{
			UE_LOGF(LogArcTerrain, Verbose,"Tile '%ls' (rail %u, %d texels) does not match the current grid (%d texels).",
				*FilePath, static_cast<uint32>(Header.Rail), Header.Resolution, ExpectedGrid.Resolution());
			return EArcTerrainTileLoad::Unusable;
		}

		OutHeader = MoveTemp(Header);
		OutFileBytes = MoveTemp(FileBytes);
		return EArcTerrainTileLoad::Loaded;
	}
}
