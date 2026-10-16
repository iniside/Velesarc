// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include <atomic>

namespace UE::ArcForest
{
	/** Wall-clock phase totals. One instance belongs to a single scope on a single thread. */
	class ARCECOTOPE_API FDiagnosticPhaseTotals
	{
	public:
		void AddSample(const TCHAR* PhaseName, double Milliseconds);
		double GetTotalMilliseconds() const;
		double GetPhaseMilliseconds(const TCHAR* PhaseName) const;
		bool IsEmpty() const;

		/** Depth is the number of scopes still open around this one, so nested rows can be marked. */
		void BeginScope();
		void EndScope(const TCHAR* PhaseName, double Milliseconds);

		void AppendTable(FString& OutText, double ReferenceMilliseconds) const;

	private:
		struct FPhase
		{
			FName Name;
			double TotalMilliseconds = 0.0;
			int64 CallCount = 0;
			int32 Depth = 0;
		};

		void AddSampleAtDepth(const TCHAR* PhaseName, double Milliseconds, int32 Depth);

		TMap<FName, int32> PhaseIndices;
		TArray<FPhase> Phases;
		double TotalMilliseconds = 0.0;
		int32 OpenScopeCount = 0;
	};

	/** Cumulative counter for a helper reached from the worker, the commit path and editor validation. */
	class FDiagnosticCallCounter
	{
	public:
		void AddSample(const double Milliseconds)
		{
			CallCount.fetch_add(1, std::memory_order_relaxed);
			TotalMicroseconds.fetch_add(
				static_cast<int64>(Milliseconds * 1000.0),
				std::memory_order_relaxed);
		}

		int64 GetCallCount() const
		{
			return CallCount.load(std::memory_order_relaxed);
		}

		double GetTotalMilliseconds() const
		{
			return static_cast<double>(TotalMicroseconds.load(std::memory_order_relaxed)) / 1000.0;
		}

	private:
		std::atomic<int64> CallCount{0};
		std::atomic<int64> TotalMicroseconds{0};
	};

	ARCECOTOPE_API FDiagnosticCallCounter& GetSnapshotValidateCounter();
	ARCECOTOPE_API FDiagnosticCallCounter& GetSnapshotChecksumCounter();
	ARCECOTOPE_API FDiagnosticCallCounter& GetSnapshotCanonicalizeCounter();
	ARCECOTOPE_API FDiagnosticCallCounter& GetInstanceContentHashCounter();

	/** Reading of the process-wide counters, taken so a scope can report its own delta. */
	struct FSharedHelperCounters
	{
		int64 ValidateCalls = 0;
		double ValidateMilliseconds = 0.0;
		int64 ChecksumCalls = 0;
		double ChecksumMilliseconds = 0.0;
		int64 CanonicalizeCalls = 0;
		double CanonicalizeMilliseconds = 0.0;
		int64 ContentHashCalls = 0;
		double ContentHashMilliseconds = 0.0;
	};

	ARCECOTOPE_API FSharedHelperCounters CaptureSharedHelperCounters();
	ARCECOTOPE_API void LogSharedHelperCounterDelta(const TCHAR* Context, const FSharedHelperCounters& Baseline);
	ARCECOTOPE_API void LogSharedHelperCounterTotals(const TCHAR* Context);

	/** Adds its wall-clock lifetime to a phase table, a shared call counter, or a millisecond total. */
	class FDiagnosticScopeTimer
	{
	public:
		FDiagnosticScopeTimer(FDiagnosticPhaseTotals& InTotals, const TCHAR* InPhaseName)
			: Totals(&InTotals)
			, PhaseName(InPhaseName)
			, StartSeconds(FPlatformTime::Seconds())
		{
			Totals->BeginScope();
		}

		explicit FDiagnosticScopeTimer(FDiagnosticCallCounter& InCounter)
			: Counter(&InCounter)
			, StartSeconds(FPlatformTime::Seconds())
		{
		}

		explicit FDiagnosticScopeTimer(double& InOutMilliseconds)
			: Milliseconds(&InOutMilliseconds)
			, StartSeconds(FPlatformTime::Seconds())
		{
		}

		~FDiagnosticScopeTimer()
		{
			const double ElapsedMilliseconds = (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
			if (Totals != nullptr)
			{
				Totals->EndScope(PhaseName, ElapsedMilliseconds);
			}
			if (Counter != nullptr)
			{
				Counter->AddSample(ElapsedMilliseconds);
			}
			if (Milliseconds != nullptr)
			{
				*Milliseconds += ElapsedMilliseconds;
			}
		}

		FDiagnosticScopeTimer(const FDiagnosticScopeTimer&) = delete;
		FDiagnosticScopeTimer& operator=(const FDiagnosticScopeTimer&) = delete;

	private:
		FDiagnosticPhaseTotals* Totals = nullptr;
		FDiagnosticCallCounter* Counter = nullptr;
		double* Milliseconds = nullptr;
		const TCHAR* PhaseName = nullptr;
		double StartSeconds = 0.0;
	};

	/** Emits one multi-line report when its scope exits, including on early returns. */
	class ARCECOTOPE_API FDiagnosticScopeReport
	{
	public:
		explicit FDiagnosticScopeReport(const TCHAR* InTitle);
		~FDiagnosticScopeReport();

		FDiagnosticScopeReport(const FDiagnosticScopeReport&) = delete;
		FDiagnosticScopeReport& operator=(const FDiagnosticScopeReport&) = delete;

		FDiagnosticPhaseTotals& GetPhases();
		void SetOutcome(FString&& InOutcome);
		void AddDetail(FString&& InDetail);

	private:
		const TCHAR* Title = nullptr;
		FString Outcome;
		TArray<FString> Details;
		FDiagnosticPhaseTotals Phases;
		double StartSeconds = 0.0;
	};

	inline double GetElapsedMilliseconds(const double StartSeconds)
	{
		return (FPlatformTime::Seconds() - StartSeconds) * 1000.0;
	}
}

#define ARCFOREST_SCOPED_PHASE(Totals, PhaseName) \
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_##PhaseName); \
	const UE::ArcForest::FDiagnosticScopeTimer UE_JOIN(ArcForestPhaseTimer, __LINE__)((Totals), TEXT(#PhaseName))

#define ARCFOREST_SCOPED_COUNTER(Counter, ScopeName) \
	TRACE_CPUPROFILER_EVENT_SCOPE(ArcForest_##ScopeName); \
	const UE::ArcForest::FDiagnosticScopeTimer UE_JOIN(ArcForestCounterTimer, __LINE__)((Counter))
