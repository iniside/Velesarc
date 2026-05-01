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
	Independent
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
