// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include <atomic>

#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityHandle.h"
#include "UObject/PrimaryAssetId.h"

#include "ArcTransportJobFragment.generated.h"

UENUM()
enum class EArcTransportJobPhase : uint8
{
	Pickup,
	Drop
};

USTRUCT()
struct ARCWORLDSIM_API FArcTransportJobFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 Generation = 0;
	FMassEntityHandle Source;
	FMassEntityHandle Sink;
	FPrimaryAssetId Item;
	int32 Quantity = 0;
	uint32 ActiveCommandSerial = 0;
	EArcTransportJobPhase Phase = EArcTransportJobPhase::Pickup;
};

namespace ArcWorldSim
{
inline uint32 NextTransportJobGeneration()
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
