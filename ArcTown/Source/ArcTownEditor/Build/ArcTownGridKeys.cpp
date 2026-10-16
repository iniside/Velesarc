// Copyright Lukasz Baran. All Rights Reserved.

#include "Build/ArcTownGridKeys.h"

#include "Misc/ArchiveMD5.h"

FGuid ArcTownGridKeys::MakeTownGridGuid(const FGuid& TownId, const uint32 GridSize, const FName RuntimeGridName)
{
	FGuid Seed = TownId;
	uint32 Size = GridSize;
	// FName compares case-insensitively, so folding the display casing would split one grid into two families.
	FString GridName = RuntimeGridName.ToString().ToLower();

	FArchiveMD5 ArMD5;
	ArMD5 << Seed;
	ArMD5 << Size;
	ArMD5 << GridName;
	return ArMD5.GetGuidFromHash();
}
