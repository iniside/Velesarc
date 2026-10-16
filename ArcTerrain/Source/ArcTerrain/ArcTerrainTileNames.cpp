// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTerrainTileNames.h"

#include "ArcTerrainActor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace ArcTerrainTileNamesPrivate_3ae9
{
	FString EncodeCoord(int32 Coord)
	{
		return Coord < 0 ? FString::Printf(TEXT("N%lld"), -static_cast<int64>(Coord))
						 : FString::Printf(TEXT("%d"), Coord);
	}

	bool DecodeCoord(const FStringView& Encoded, int32& OutCoord)
	{
		if (Encoded.IsEmpty())
		{
			return false;
		}

		const bool bNegative = Encoded[0] == TEXT('N');
		const FStringView Digits = bNegative ? Encoded.RightChop(1) : Encoded;
		if (Digits.IsEmpty() || (Digits.Len() > 1 && Digits[0] == TEXT('0')))
		{
			return false;
		}
		// "N0" would be a second spelling of 0, which EncodeCoord never produces.
		if (bNegative && Digits[0] == TEXT('0'))
		{
			return false;
		}

		int64 Magnitude = 0;
		for (int32 Index = 0; Index < Digits.Len(); ++Index)
		{
			const TCHAR Digit = Digits[Index];
			if (Digit < TEXT('0') || Digit > TEXT('9'))
			{
				return false;
			}
			Magnitude = Magnitude * 10 + static_cast<int64>(Digit - TEXT('0'));
			if (Magnitude > -static_cast<int64>(MIN_int32))
			{
				return false;
			}
		}

		const int64 Signed = bNegative ? -Magnitude : Magnitude;
		if (Signed < static_cast<int64>(MIN_int32) || Signed > static_cast<int64>(MAX_int32))
		{
			return false;
		}

		OutCoord = static_cast<int32>(Signed);
		return true;
	}

	FString MakeTileDir(const AArcTerrain& Terrain, bool bForWrite)
	{
		const ULevel* Level = Terrain.GetLevel();
		const UPackage* MapPackage = Level != nullptr ? Level->GetPackage() : nullptr;
		if (MapPackage == nullptr)
		{
			return FString();
		}

		FString MapPackageName = MapPackage->GetName();
		if (FPackageName::IsTempPackage(MapPackageName))
		{
			return FString();
		}

		if (MapPackage->HasAnyPackageFlags(PKG_PlayInEditor))
		{
			if (bForWrite)
			{
				return FString();
			}
			MapPackageName = UWorld::RemovePIEPrefix(MapPackageName);
		}

		FString MapFilename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(MapPackageName, MapFilename))
		{
			return FString();
		}

		return FString::Printf(TEXT("%s_ArcTerrain/%s"), *MapFilename, *Terrain.GetName());
	}
}

namespace ArcTerrainTileNames
{
	const TCHAR* RailFilePrefix(EArcTerrainRail Rail)
	{
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
			return TEXT("TileW_");
		case EArcTerrainRail::Data:
			return TEXT("TileD_");
		default:
			return TEXT("TileH_");
		}
	}

	const TCHAR* RailFileExtension(EArcTerrainRail Rail)
	{
		switch (Rail)
		{
		case EArcTerrainRail::Weight:
			return TEXT(".arcw");
		case EArcTerrainRail::Data:
			return TEXT(".arcd");
		default:
			return TEXT(".arch");
		}
	}

	FString MakeTileFileName(EArcTerrainRail Rail, int32 Tier, FIntPoint TileKey)
	{
		using namespace ArcTerrainTileNamesPrivate_3ae9;

		if (Tier < 0 || Tier >= ArcTerrain::MaxTiers)
		{
			return FString();
		}

		const FString TierToken = Tier > 0 ? FString::Printf(TEXT("T%d_"), Tier) : FString();
		return FString::Printf(TEXT("%s%s%s_%s%s"), RailFilePrefix(Rail), *TierToken,
			*EncodeCoord(TileKey.X), *EncodeCoord(TileKey.Y), RailFileExtension(Rail));
	}

	FString MakeTileFileWildcard(EArcTerrainRail Rail)
	{
		return FString::Printf(TEXT("%s*%s"), RailFilePrefix(Rail), RailFileExtension(Rail));
	}

	bool ParseTileFileName(EArcTerrainRail Rail, const FString& FileName, int32& OutTier,
		FIntPoint& OutTileKey)
	{
		using namespace ArcTerrainTileNamesPrivate_3ae9;

		const FString Prefix = RailFilePrefix(Rail);
		const FString Extension = RailFileExtension(Rail);
		if (!FileName.StartsWith(Prefix, ESearchCase::CaseSensitive)
			|| !FileName.EndsWith(Extension, ESearchCase::CaseSensitive))
		{
			return false;
		}

		const int32 BodyLen = FileName.Len() - Prefix.Len() - Extension.Len();
		if (BodyLen <= 0)
		{
			return false;
		}

		FStringView Body = FStringView(FileName).Mid(Prefix.Len(), BodyLen);

		int32 Tier = 0;
		if (Body.Len() >= 2 && Body[0] == TEXT('T') && Body[1] >= TEXT('0') && Body[1] <= TEXT('9'))
		{
			int32 TierSeparator = INDEX_NONE;
			if (!Body.FindChar(TEXT('_'), TierSeparator))
			{
				return false;
			}
			if (!DecodeCoord(Body.Mid(1, TierSeparator - 1), Tier) || Tier <= 0 || Tier >= ArcTerrain::MaxTiers)
			{
				return false;
			}
			Body = Body.RightChop(TierSeparator + 1);
		}

		int32 SeparatorIndex = INDEX_NONE;
		if (!Body.FindChar(TEXT('_'), SeparatorIndex))
		{
			return false;
		}

		int32 X = 0;
		int32 Y = 0;
		if (!DecodeCoord(Body.Left(SeparatorIndex), X) || !DecodeCoord(Body.RightChop(SeparatorIndex + 1), Y))
		{
			return false;
		}

		OutTier = Tier;
		OutTileKey = FIntPoint(X, Y);
		return true;
	}

	FString MakeTileDirForWrite(const AArcTerrain& Terrain)
	{
		return ArcTerrainTileNamesPrivate_3ae9::MakeTileDir(Terrain, /*bForWrite=*/true);
	}

	FString MakeTileDirForRead(const AArcTerrain& Terrain)
	{
		return ArcTerrainTileNamesPrivate_3ae9::MakeTileDir(Terrain, /*bForWrite=*/false);
	}
}
