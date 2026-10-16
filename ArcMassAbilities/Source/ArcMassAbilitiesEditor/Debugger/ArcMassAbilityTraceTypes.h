// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Per-event-type record structs — plain C++, no UObject
// ---------------------------------------------------------------------------

struct FArcMABAbilityActivatedRecord
{
	double   Time          = 0.0;
	uint32   EntityIndex   = 0;
	uint32   EntitySerial  = 0;
	uint64   AbilityObjId  = 0;
	FString  AbilityName;
};

struct FArcMABAbilityEndedRecord
{
	double   Time          = 0.0;
	uint32   EntityIndex   = 0;
	uint32   EntitySerial  = 0;
	uint64   AbilityObjId  = 0;
	FString  AbilityName;
	uint8    Reason        = 0;
	uint32   InstigatorIndex  = 0;
	uint32   InstigatorSerial = 0;
};

struct FArcMABEffectAppliedRecord
{
	double   Time         = 0.0;
	uint32   EntityIndex  = 0;
	uint32   EntitySerial = 0;
	uint64   EffectObjId  = 0;
	FString  EffectName;
};

struct FArcMABEffectExecutedRecord
{
	double   Time         = 0.0;
	uint32   EntityIndex  = 0;
	uint32   EntitySerial = 0;
	uint64   EffectObjId  = 0;
	FString  EffectName;
};

struct FArcMABAttributeChangedRecord
{
	double   Time          = 0.0;
	uint32   EntityIndex   = 0;
	uint32   EntitySerial  = 0;
	FString  AttributeName;
	float    OldValue      = 0.f;
	float    NewValue      = 0.f;
};

struct FArcMABTagCountChangedRecord
{
	double   Time         = 0.0;
	uint32   EntityIndex  = 0;
	uint32   EntitySerial = 0;
	FString  TagName;
	int32    OldCount     = 0;
	int32    NewCount     = 0;
};

struct FArcMABCostFailedRecord
{
	double   Time         = 0.0;
	uint32   EntityIndex  = 0;
	uint32   EntitySerial = 0;
	uint64   AbilityObjId = 0;
	FString  AbilityName;
	uint8    Reason       = 0;
	float    MissingAmount = 0.f;
};

struct FArcMABCooldownFailedRecord
{
	double   Time          = 0.0;
	uint32   EntityIndex   = 0;
	uint32   EntitySerial  = 0;
	uint64   AbilityObjId  = 0;
	FString  AbilityName;
	float    RemainingMs   = 0.f;
};

struct FArcMABSignalRaisedRecord
{
	double   Time         = 0.0;
	uint32   EntityIndex  = 0;
	uint32   EntitySerial = 0;
	FString  SignalName;
};

// ---------------------------------------------------------------------------
// Per-entity bundle — all timelines for one entity keyed by InstanceId
// ---------------------------------------------------------------------------

struct FArcMABEntityRecord
{
	uint64   InstanceId   = 0;
	uint32   EntityIndex  = 0;
	uint32   EntitySerial = 0;

	TArray<FArcMABAbilityActivatedRecord>  AbilityActivated;
	TArray<FArcMABAbilityEndedRecord>      AbilityEnded;
	TArray<FArcMABEffectAppliedRecord>     EffectApplied;
	TArray<FArcMABEffectExecutedRecord>    EffectExecuted;
	TArray<FArcMABAttributeChangedRecord>  AttributeChanged;
	TArray<FArcMABTagCountChangedRecord>   TagCountChanged;
	TArray<FArcMABCostFailedRecord>        CostFailed;
	TArray<FArcMABCooldownFailedRecord>    CooldownFailed;
	TArray<FArcMABSignalRaisedRecord>      SignalRaised;
};
