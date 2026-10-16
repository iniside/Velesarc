// Copyright Lukasz Baran. All Rights Reserved.

#include "Diagnostics/ArcForestDiagnostics.h"

#include "ArcEcotopeModule.h"

namespace ArcForestDiagnosticsPrivate
{
	UE::ArcForest::FDiagnosticCallCounter SnapshotValidateCounter;
	UE::ArcForest::FDiagnosticCallCounter SnapshotChecksumCounter;
	UE::ArcForest::FDiagnosticCallCounter SnapshotCanonicalizeCounter;
	UE::ArcForest::FDiagnosticCallCounter InstanceContentHashCounter;
}

namespace UE::ArcForest
{
	void FDiagnosticPhaseTotals::AddSampleAtDepth(const TCHAR* PhaseName, const double Milliseconds, const int32 Depth)
	{
		const FName Key(PhaseName);
		const int32* ExistingIndex = PhaseIndices.Find(Key);
		int32 PhaseIndex = INDEX_NONE;
		if (ExistingIndex != nullptr)
		{
			PhaseIndex = *ExistingIndex;
		}
		else
		{
			PhaseIndex = Phases.Num();
			FPhase& NewPhase = Phases.AddDefaulted_GetRef();
			NewPhase.Name = Key;
			NewPhase.Depth = Depth;
			PhaseIndices.Add(Key, PhaseIndex);
		}
		FPhase& Phase = Phases[PhaseIndex];
		Phase.TotalMilliseconds += Milliseconds;
		++Phase.CallCount;
		Phase.Depth = FMath::Min(Phase.Depth, Depth);
		TotalMilliseconds += Milliseconds;
	}

	void FDiagnosticPhaseTotals::AddSample(const TCHAR* PhaseName, const double Milliseconds)
	{
		AddSampleAtDepth(PhaseName, Milliseconds, OpenScopeCount);
	}

	void FDiagnosticPhaseTotals::BeginScope()
	{
		++OpenScopeCount;
	}

	void FDiagnosticPhaseTotals::EndScope(const TCHAR* PhaseName, const double Milliseconds)
	{
		OpenScopeCount = FMath::Max(0, OpenScopeCount - 1);
		AddSampleAtDepth(PhaseName, Milliseconds, OpenScopeCount);
	}

	double FDiagnosticPhaseTotals::GetTotalMilliseconds() const
	{
		return TotalMilliseconds;
	}

	double FDiagnosticPhaseTotals::GetPhaseMilliseconds(const TCHAR* PhaseName) const
	{
		const int32* PhaseIndex = PhaseIndices.Find(FName(PhaseName));
		return PhaseIndex != nullptr ? Phases[*PhaseIndex].TotalMilliseconds : 0.0;
	}

	bool FDiagnosticPhaseTotals::IsEmpty() const
	{
		return Phases.IsEmpty();
	}

	void FDiagnosticPhaseTotals::AppendTable(FString& OutText, const double ReferenceMilliseconds) const
	{
		TArray<const FPhase*> Ordered;
		Ordered.Reserve(Phases.Num());
		for (const FPhase& Phase : Phases)
		{
			Ordered.Add(&Phase);
		}
		Ordered.Sort(
			[](const FPhase& Left, const FPhase& Right)
			{
				if (Left.TotalMilliseconds != Right.TotalMilliseconds)
				{
					return Left.TotalMilliseconds > Right.TotalMilliseconds;
				}
				return Left.Name.LexicalLess(Right.Name);
			});

		const double Denominator = ReferenceMilliseconds > 0.0
			? ReferenceMilliseconds
			: (TotalMilliseconds > 0.0 ? TotalMilliseconds : 1.0);
		bool bHasNestedPhase = false;
		for (const FPhase* Phase : Ordered)
		{
			bHasNestedPhase = bHasNestedPhase || Phase->Depth > 0;
		}
		if (bHasNestedPhase)
		{
			OutText += TEXT("\n    rows marked '+' are nested and are also counted inside their parent row");
		}
		for (const FPhase* Phase : Ordered)
		{
			const FString Label = Phase->Depth > 0
				? FString::ChrN(Phase->Depth * 2, TEXT(' ')) + TEXT("+ ") + Phase->Name.ToString()
				: Phase->Name.ToString();
			OutText += FString::Printf(
				TEXT("\n    %-48s %12.2f ms  %6.2f%%  x%lld"),
				*Label,
				Phase->TotalMilliseconds,
				Phase->TotalMilliseconds / Denominator * 100.0,
				Phase->CallCount);
		}
	}

	FDiagnosticCallCounter& GetSnapshotValidateCounter()
	{
		return ArcForestDiagnosticsPrivate::SnapshotValidateCounter;
	}

	FDiagnosticCallCounter& GetSnapshotChecksumCounter()
	{
		return ArcForestDiagnosticsPrivate::SnapshotChecksumCounter;
	}

	FDiagnosticCallCounter& GetSnapshotCanonicalizeCounter()
	{
		return ArcForestDiagnosticsPrivate::SnapshotCanonicalizeCounter;
	}

	FDiagnosticCallCounter& GetInstanceContentHashCounter()
	{
		return ArcForestDiagnosticsPrivate::InstanceContentHashCounter;
	}

	FSharedHelperCounters CaptureSharedHelperCounters()
	{
		FSharedHelperCounters Snapshot;
		Snapshot.ValidateCalls = GetSnapshotValidateCounter().GetCallCount();
		Snapshot.ValidateMilliseconds = GetSnapshotValidateCounter().GetTotalMilliseconds();
		Snapshot.ChecksumCalls = GetSnapshotChecksumCounter().GetCallCount();
		Snapshot.ChecksumMilliseconds = GetSnapshotChecksumCounter().GetTotalMilliseconds();
		Snapshot.CanonicalizeCalls = GetSnapshotCanonicalizeCounter().GetCallCount();
		Snapshot.CanonicalizeMilliseconds = GetSnapshotCanonicalizeCounter().GetTotalMilliseconds();
		Snapshot.ContentHashCalls = GetInstanceContentHashCounter().GetCallCount();
		Snapshot.ContentHashMilliseconds = GetInstanceContentHashCounter().GetTotalMilliseconds();
		return Snapshot;
	}

	void LogSharedHelperCounterDelta(const TCHAR* Context, const FSharedHelperCounters& Baseline)
	{
		const FSharedHelperCounters Current = CaptureSharedHelperCounters();
		UE_LOGF(
			LogArcEcotope,
			Log,"Shared helper usage during %ls: Snapshot.Validate x%lld %.2f ms | Snapshot.Checksum x%lld %.2f ms | Snapshot.Canonicalize x%lld %.2f ms | Instance.ContentHash x%lld %.2f ms (cumulative since editor start: x%lld %.2f ms | x%lld %.2f ms | x%lld %.2f ms | x%lld %.2f ms)",
			Context,
			Current.ValidateCalls - Baseline.ValidateCalls,
			Current.ValidateMilliseconds - Baseline.ValidateMilliseconds,
			Current.ChecksumCalls - Baseline.ChecksumCalls,
			Current.ChecksumMilliseconds - Baseline.ChecksumMilliseconds,
			Current.CanonicalizeCalls - Baseline.CanonicalizeCalls,
			Current.CanonicalizeMilliseconds - Baseline.CanonicalizeMilliseconds,
			Current.ContentHashCalls - Baseline.ContentHashCalls,
			Current.ContentHashMilliseconds - Baseline.ContentHashMilliseconds,
			Current.ValidateCalls,
			Current.ValidateMilliseconds,
			Current.ChecksumCalls,
			Current.ChecksumMilliseconds,
			Current.CanonicalizeCalls,
			Current.CanonicalizeMilliseconds,
			Current.ContentHashCalls,
			Current.ContentHashMilliseconds);
	}

	void LogSharedHelperCounterTotals(const TCHAR* Context)
	{
		const FSharedHelperCounters Current = CaptureSharedHelperCounters();
		UE_LOGF(
			LogArcEcotope,
			Log,"Shared helper cumulative totals since editor start (%ls): Snapshot.Validate x%lld %.2f ms | Snapshot.Checksum x%lld %.2f ms | Snapshot.Canonicalize x%lld %.2f ms | Instance.ContentHash x%lld %.2f ms",
			Context,
			Current.ValidateCalls,
			Current.ValidateMilliseconds,
			Current.ChecksumCalls,
			Current.ChecksumMilliseconds,
			Current.CanonicalizeCalls,
			Current.CanonicalizeMilliseconds,
			Current.ContentHashCalls,
			Current.ContentHashMilliseconds);
	}

	FDiagnosticScopeReport::FDiagnosticScopeReport(const TCHAR* InTitle)
		: Title(InTitle)
		, StartSeconds(FPlatformTime::Seconds())
	{
	}

	FDiagnosticScopeReport::~FDiagnosticScopeReport()
	{
		if (!UE_LOG_ACTIVE(LogArcEcotope, Log))
		{
			return;
		}
		const double ElapsedMilliseconds = GetElapsedMilliseconds(StartSeconds);
		FString Text = FString::Printf(TEXT("%s: %.2f ms"), Title, ElapsedMilliseconds);
		if (!Outcome.IsEmpty())
		{
			Text += TEXT(" [") + Outcome + TEXT("]");
		}
		for (const FString& Detail : Details)
		{
			Text += TEXT("\n    ") + Detail;
		}
		Phases.AppendTable(Text, ElapsedMilliseconds);
		UE_LOGF(LogArcEcotope, Log,"%ls", *Text);
	}

	FDiagnosticPhaseTotals& FDiagnosticScopeReport::GetPhases()
	{
		return Phases;
	}

	void FDiagnosticScopeReport::SetOutcome(FString&& InOutcome)
	{
		Outcome = MoveTemp(InOutcome);
	}

	void FDiagnosticScopeReport::AddDetail(FString&& InDetail)
	{
		Details.Add(MoveTemp(InDetail));
	}
}
