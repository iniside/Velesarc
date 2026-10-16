// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include <atomic>

#include "Mass/ArchetypeCompositionTypes.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcItemInteractionFragment.generated.h"

USTRUCT()
struct ARCWORLDSIM_API FArcItemInteractionFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 Generation = 0;
	uint32 CommandSerial = 0;
	FPrimaryAssetId Item;
	int32 RequestedQuantity = 0;
	int32 ProcessedQuantity = 0;
};

namespace ArcWorldSim
{
inline uint32 NextItemInteractionGeneration()
{
	static std::atomic<uint32> Counter{0};
	uint32 Generation = ++Counter;
	if (Generation == 0)
	{
		Generation = ++Counter;
	}
	return Generation;
}
}
