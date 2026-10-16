// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcEffectTypes.generated.h"

UENUM(BlueprintType)
enum class EArcModifierOp : uint8
{
	Add,
	MultiplyAdditive,
	DivideAdditive,
	MultiplyCompound,
	AddFinal,
	Override,
	Max UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EArcEffectDuration : uint8
{
	Instant,
	Duration,
	Infinite
};

UENUM(BlueprintType)
enum class EArcStackPolicy : uint8
{
	Aggregate,
	Replace,
	DenyNew
};

UENUM(BlueprintType)
enum class EArcStackDurationRefresh : uint8
{
	Refresh,
	Extend,
	Independent,
	Longer
};

UENUM(BlueprintType)
enum class EArcStackType : uint8
{
	Counter,
	Instance
};

UENUM(BlueprintType)
enum class EArcStackGrouping : uint8
{
	ByTarget,
	BySource
};

UENUM(BlueprintType)
enum class EArcStackOverflowPolicy : uint8
{
	Deny,
	RemoveOldest
};

UENUM(BlueprintType)
enum class EArcStackPeriodPolicy : uint8
{
	Unchanged,
	Reset
};

UENUM(BlueprintType)
enum class EArcEffectPeriodicity : uint8
{
	NonPeriodic,
	Periodic
};

UENUM(BlueprintType)
enum class EArcPeriodicExecutionPolicy : uint8
{
	PeriodOnly,
	PeriodAndApplication
};

UENUM(BlueprintType)
enum class EArcModifierType : uint8
{
	Simple,
	SetByCaller,
	Custom
};

/** How a range-source FArcScalableFloat collapses to a single number when consumed.
 *  No-op for non-range sources (Min==Max==Value). */
UENUM(BlueprintType)
enum class EArcRangeResolution : uint8
{
	Roll,   // re-roll each consumption (default — preserves current behavior)
	Min,
	Max,
	Mid
};

UENUM(BlueprintType)
enum class EArcEffectRemovalReason : uint8
{
	Expired,
	Removed,
	Replaced,
	Overflow,
	EntityDestroyed,
	Invalid
};

UENUM(BlueprintType)
enum class EArcEffectRemovalOrder : uint8
{
	OldestFirst,
	NewestFirst
};
