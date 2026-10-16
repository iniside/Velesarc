// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Mass/ArchetypeCompositionTypes.h"

#include "ArcSmartObjectBehaviorFragments.generated.h"

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
	float UseElapsed = 0.0f;
	float UseDuration = 0.0f;
	bool bEffectApplied = false;
	bool bCompletionQueued = false;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectWorkBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
	float WorkShiftElapsed = 0.0f;
	float WorkShiftDuration = 0.0f;
	float WorkLoopElapsed = 0.0f;
	float WorkLoopDuration = 0.0f;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectPickupBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectDropBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectGatherBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectDrawBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectEatBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
};

USTRUCT()
struct ARCWORLDSIM_API FArcSmartObjectSleepBehaviorFragment : public FMassSparseFragment
{
	GENERATED_BODY()

	uint32 CommandSerial = 0;
	double LastRecoveryTimeSeconds = 0.0;
	float Comfort = 1.0f;
};
