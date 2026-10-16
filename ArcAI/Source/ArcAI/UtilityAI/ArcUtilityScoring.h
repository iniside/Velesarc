// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FArcUtilityContext;
struct FArcUtilityConsideration;
struct FArcUtilityConsiderationResult;
struct FArcUtilityEntry;
struct FArcUtilityTarget;

enum class EArcUtilityPairOutcome : uint8
{
	Accepted,
	BelowMinScore,
	/** A consideration returned a rejection reason; terminal whatever MinScore is. */
	Vetoed
};

enum class EArcUtilityPairProgress : uint8
{
	Continue,
	Complete,
	Invalid
};

struct FArcUtilityConsiderationSnapshot
{
	int32 EntryIndex = INDEX_NONE;
	int32 TargetIndex = INDEX_NONE;
	int32 ConsiderationIndex = INDEX_NONE;
	FString ConsiderationName;
	float RawScore = 0.0f;
	float CurvedScore = 0.0f;
};

/** Per-consideration trace sink. A null Snapshots array disables tracing for the pair. */
struct FArcUtilityPairTrace
{
	TArray<FArcUtilityConsiderationSnapshot>* Snapshots = nullptr;
	int32 EntryIndex = INDEX_NONE;
	int32 TargetIndex = INDEX_NONE;
};

struct FArcUtilityPairResult
{
	EArcUtilityPairOutcome Outcome = EArcUtilityPairOutcome::BelowMinScore;

	float Score = 0.0f;

	/** Product of the clamped consideration curve scores, before the geometric mean. */
	float Product = 1.0f;

	/** Geometric mean of the curve scores; 1.0 when the entry has no considerations. */
	float Compensated = 1.0f;

	float EarlyOutThreshold = 0.0f;

	/** Consideration the early-out fired on, INDEX_NONE when the whole list ran. */
	int32 EarlyOutConsiderationIndex = INDEX_NONE;

	FName VetoReason = NAME_None;
};

namespace ArcUtilityScoring
{
	ARCAI_API EArcUtilityPairProgress BeginPair(
		float EntryWeight,
		int32 NumConsiderations,
		float MinScore,
		FArcUtilityPairResult& OutResult);

	ARCAI_API EArcUtilityPairProgress AccumulateConsideration(
		const FArcUtilityConsideration& Consideration,
		const FArcUtilityConsiderationResult& ConsiderationResult,
		int32 ConsiderationIndex,
		const FString& ConsiderationName,
		FArcUtilityPairResult& OutResult,
		FArcUtilityPairTrace* OptionalTrace = nullptr);

	ARCAI_API EArcUtilityPairProgress FinishPair(
		float EntryWeight,
		int32 NumConsiderations,
		float MinScore,
		FArcUtilityPairResult& OutResult);

	/**
	 * Score one entry x candidate pair. Carries no enumeration or time-slicing state so both the
	 * async scheduler and synchronous callers run identical arithmetic.
	 */
	ARCAI_API void ScorePair(
		const FArcUtilityEntry& Entry,
		const FArcUtilityTarget& Candidate,
		const FArcUtilityContext& Context,
		float MinScore,
		FArcUtilityPairResult& OutResult,
		FArcUtilityPairTrace* OptionalTrace = nullptr);
}
