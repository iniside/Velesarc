// Copyright Lukasz Baran. All Rights Reserved.

#include "UtilityAI/ArcUtilityScoring.h"
#include "UtilityAI/ArcUtilityConsideration.h"
#include "UtilityAI/ArcUtilityEntry.h"
#include "UtilityAI/ArcUtilityTypes.h"
#include "UtilityAI/Debugger/ArcUtilityTrace.h"

#if WITH_ARCUTILITY_TRACE
namespace ArcUtilityScoringTrace
{
	static FString GetStructDisplayName(const FInstancedStruct& Struct)
	{
		if (const UScriptStruct* ScriptStruct = Struct.GetScriptStruct())
		{
#if WITH_METADATA
			const FString DisplayName = ScriptStruct->GetMetaData(TEXT("DisplayName"));
			return DisplayName.IsEmpty() ? ScriptStruct->GetName() : DisplayName;
#else
			return ScriptStruct->GetName();
#endif
		}
		return TEXT("Unknown");
	}
}
#endif

EArcUtilityPairProgress ArcUtilityScoring::BeginPair(
	const float EntryWeight,
	const int32 NumConsiderations,
	const float MinScore,
	FArcUtilityPairResult& OutResult)
{
	OutResult = FArcUtilityPairResult();
	if (!FMath::IsFinite(EntryWeight) || EntryWeight < 0.0f
		|| NumConsiderations < 0 || !FMath::IsFinite(MinScore) || MinScore < 0.0f)
	{
		OutResult.Outcome = EArcUtilityPairOutcome::Vetoed;
		OutResult.VetoReason = TEXT("InvalidPairParameters");
		return EArcUtilityPairProgress::Invalid;
	}
	const float Threshold = FMath::Pow(
		MinScore / FMath::Max(EntryWeight, UE_KINDA_SMALL_NUMBER),
		static_cast<float>(NumConsiderations));
	OutResult.EarlyOutThreshold = FMath::IsFinite(Threshold) ? Threshold : MAX_flt;
	return EArcUtilityPairProgress::Continue;
}

EArcUtilityPairProgress ArcUtilityScoring::AccumulateConsideration(
	const FArcUtilityConsideration& Consideration,
	const FArcUtilityConsiderationResult& ConsiderationResult,
	const int32 ConsiderationIndex,
	const FString& ConsiderationName,
	FArcUtilityPairResult& OutResult,
	FArcUtilityPairTrace* OptionalTrace)
{
	if (!ConsiderationResult.IsAccepted())
	{
		OutResult.Compensated = 0.0f;
		OutResult.Outcome = EArcUtilityPairOutcome::Vetoed;
		OutResult.VetoReason = ConsiderationResult.RejectionReason;
		return EArcUtilityPairProgress::Complete;
	}
	const float RawScore = ConsiderationResult.Score;
	if (!FMath::IsFinite(RawScore))
	{
		OutResult.Outcome = EArcUtilityPairOutcome::Vetoed;
		OutResult.VetoReason = TEXT("InvalidConsiderationScore");
		return EArcUtilityPairProgress::Invalid;
	}
	const float UnclampedCurveScore = Consideration.ResponseCurve.Evaluate(FMath::Clamp(RawScore, 0.0f, 1.0f));
	if (!FMath::IsFinite(UnclampedCurveScore))
	{
		OutResult.Outcome = EArcUtilityPairOutcome::Vetoed;
		OutResult.VetoReason = TEXT("InvalidResponseCurve");
		return EArcUtilityPairProgress::Invalid;
	}
	const float CurveScore = FMath::Clamp(UnclampedCurveScore, 0.0f, 1.0f);
#if WITH_ARCUTILITY_TRACE
	if (OptionalTrace && OptionalTrace->Snapshots)
	{
		OptionalTrace->Snapshots->Add({OptionalTrace->EntryIndex, OptionalTrace->TargetIndex, ConsiderationIndex,
			ConsiderationName, RawScore, CurveScore});
	}
#else
	(void)ConsiderationName;
	(void)OptionalTrace;
#endif
	OutResult.Product *= CurveScore;
	if (!FMath::IsFinite(OutResult.Product))
	{
		OutResult.Outcome = EArcUtilityPairOutcome::Vetoed;
		OutResult.VetoReason = TEXT("InvalidPairProduct");
		return EArcUtilityPairProgress::Invalid;
	}
	if (OutResult.Product < OutResult.EarlyOutThreshold)
	{
		OutResult.EarlyOutConsiderationIndex = ConsiderationIndex;
		OutResult.Outcome = EArcUtilityPairOutcome::BelowMinScore;
		return EArcUtilityPairProgress::Complete;
	}
	return EArcUtilityPairProgress::Continue;
}

EArcUtilityPairProgress ArcUtilityScoring::FinishPair(
	const float EntryWeight,
	const int32 NumConsiderations,
	const float MinScore,
	FArcUtilityPairResult& OutResult)
{
	OutResult.Compensated = NumConsiderations > 0
		? FMath::Pow(OutResult.Product, 1.0f / static_cast<float>(NumConsiderations))
		: 1.0f;
	OutResult.Score = OutResult.Compensated * EntryWeight;
	if (!FMath::IsFinite(OutResult.Compensated) || !FMath::IsFinite(OutResult.Score))
	{
		OutResult.Outcome = EArcUtilityPairOutcome::Vetoed;
		OutResult.VetoReason = TEXT("InvalidPairScore");
		return EArcUtilityPairProgress::Invalid;
	}
	OutResult.Outcome = OutResult.Score >= MinScore
		? EArcUtilityPairOutcome::Accepted
		: EArcUtilityPairOutcome::BelowMinScore;
	return EArcUtilityPairProgress::Complete;
}

void ArcUtilityScoring::ScorePair(
	const FArcUtilityEntry& Entry,
	const FArcUtilityTarget& Candidate,
	const FArcUtilityContext& Context,
	const float MinScore,
	FArcUtilityPairResult& OutResult,
	FArcUtilityPairTrace* OptionalTrace)
{
	const int32 NumConsiderations = Entry.Considerations.Num();
	if (BeginPair(Entry.Weight, NumConsiderations, MinScore, OutResult)
		== EArcUtilityPairProgress::Invalid)
	{
		return;
	}
	for (int32 ConsiderationIndex = 0; ConsiderationIndex < NumConsiderations; ++ConsiderationIndex)
	{
		const FArcUtilityConsideration* Consideration =
			Entry.Considerations[ConsiderationIndex].GetPtr<FArcUtilityConsideration>();
		if (!Consideration)
		{
			continue;
		}
		const FArcUtilityConsiderationResult Result = Consideration->Score(Candidate, Context);
		FString ConsiderationName;
#if WITH_ARCUTILITY_TRACE
		ConsiderationName = ArcUtilityScoringTrace::GetStructDisplayName(
			Entry.Considerations[ConsiderationIndex]);
#endif
		if (AccumulateConsideration(
			*Consideration, Result, ConsiderationIndex, ConsiderationName,
			OutResult, OptionalTrace) != EArcUtilityPairProgress::Continue)
		{
			return;
		}
	}
	FinishPair(Entry.Weight, NumConsiderations, MinScore, OutResult);
}
