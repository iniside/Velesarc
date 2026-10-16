// Copyright Lukasz Baran. All Rights Reserved.

#include "Terrain/ArcTownStampKeys.h"

#include "Hash/Blake3.h"

namespace ArcTownStampKeysPrivate
{
	template <typename T>
	void HashField(FBlake3& Hash, const T& Value)
	{
		Hash.Update(&Value, sizeof(T));
	}

	FGuid FinalizeGuid(FBlake3& Hash)
	{
		const FBlake3Hash Digest = Hash.Finalize();
		uint32 Words[4] = {0, 0, 0, 0};
		FMemory::Memcpy(Words, Digest.GetBytes(), sizeof(Words));
		// An all-zero guid reads as "no content" to the source cache, which drops the stamp silently
		// instead of failing; one forced bit keeps every result a valid identity.
		Words[0] |= 1u;
		return FGuid(Words[0], Words[1], Words[2], Words[3]);
	}
}

FGuid ArcTownStampKeys::MakePadContentId(
	const FGuid& TownId,
	const int32 BuildingIndex,
	const FVector2D& FootprintXY,
	const double FloorHeight)
{
	using namespace ArcTownStampKeysPrivate;

	// Field by field, never the whole struct: a struct-wide hash feeds the compiler's padding bytes,
	// which are uninitialised, into a key that has to be reproducible across sessions.
	FBlake3 Hash;
	HashField(Hash, TownId.A);
	HashField(Hash, TownId.B);
	HashField(Hash, TownId.C);
	HashField(Hash, TownId.D);
	HashField(Hash, BuildingIndex);
	HashField(Hash, FootprintXY.X);
	HashField(Hash, FootprintXY.Y);
	HashField(Hash, FloorHeight);

	return FinalizeGuid(Hash);
}
