// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcRmlUIPerf.h"

#if !UE_BUILD_SHIPPING

#include "ArcRmlUILog.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"

#include <atomic>

namespace ArcRmlUIPerfPrivate
{
	TAutoConsoleVariable<int32> CVarPerfLog(
		TEXT("ArcRml.PerfLog"),
		2,
		TEXT("ArcRmlUI performance lines prefixed 'ArcRmlUI perf:'. 0 off; 1 summaries, RmlUi update/render breakdown and counts per view, load breakdown; ")
		TEXT("2 additionally per-element and per-property attribution lists ('who') per view."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<float> CVarPerfLogInterval(
		TEXT("ArcRml.PerfLogInterval"),
		2.0f,
		TEXT("Seconds between periodic ArcRmlUI performance summaries."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarPerfTopN(
		TEXT("ArcRml.PerfTopN"),
		10,
		TEXT("Entries per attribution list in ArcRmlUI performance logs (ArcRml.PerfLog 2 and ArcRml.PerfDump)."),
		ECVF_RenderThreadSafe);

	TAutoConsoleVariable<int32> CVarLayoutVerify(
		TEXT("ArcRml.LayoutVerify"),
		0,
		TEXT("1 follows every outermost RmlUi document layout pass with a full reference layout, which stays applied, and logs 'ArcRmlUI perf:' lines with the elements compared ")
		TEXT("and the first ArcRml.PerfTopN mismatches (box, offsets, baseline, scrollable overflow, text lines). 0 off."),
		ECVF_RenderThreadSafe);

	std::atomic<bool> bDumpRequested{false};
	TFunction<void()> DumpWaker;
	std::atomic<bool> bClockFrozen{false};
	std::atomic<double> FrozenClockSeconds{0.0};

	void WakeUIThread()
	{
		if (DumpWaker)
		{
			DumpWaker();
		}
	}

	FAutoConsoleCommand ClockCommand(
		TEXT("ArcRml.Clock"),
		TEXT("Freezes the RmlUi clock at the given seconds, or 'live' for wall time. It drives animations, transitions, double-click detection, smooth scrolling, the text cursor blink and slider/scroll repeat. Set it before the first ArcRml.Open; a frozen value below times already used runs animations backwards: ArcRml.Clock <seconds>|live"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOGF(LogArcRmlUI, Display, "ArcRml.Clock: %ls", bClockFrozen.load() ? *FString::Printf(TEXT("frozen at %.6f s"), FrozenClockSeconds.load()) : TEXT("live"));
				return;
			}
			if (Args[0] == TEXT("live"))
			{
				bClockFrozen.store(false);
				UE_LOGF(LogArcRmlUI, Display, "ArcRml.Clock: live");
			}
			else if (Args[0].IsNumeric())
			{
				FrozenClockSeconds.store(FCString::Atod(*Args[0]));
				bClockFrozen.store(true);
				UE_LOGF(LogArcRmlUI, Display, "ArcRml.Clock: frozen at %.6f s", FrozenClockSeconds.load());
			}
			else
			{
				UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.Clock <seconds>|live");
				return;
			}
			WakeUIThread();
		}));

	FAutoConsoleCommand ClockAdvanceCommand(
		TEXT("ArcRml.ClockAdvance"),
		TEXT("Advances the frozen RmlUi clock by the given seconds: ArcRml.ClockAdvance <seconds>"),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1 || !Args[0].IsNumeric() || !bClockFrozen.load())
			{
				UE_LOGF(LogArcRmlUI, Warning, "Usage: ArcRml.ClockAdvance <seconds>, after ArcRml.Clock <seconds>");
				return;
			}
			FrozenClockSeconds.store(FrozenClockSeconds.load() + FCString::Atod(*Args[0]));
			UE_LOGF(LogArcRmlUI, Display, "ArcRml.Clock: frozen at %.6f s", FrozenClockSeconds.load());
			WakeUIThread();
		}));

	FAutoConsoleCommand PerfDumpCommand(
		TEXT("ArcRml.PerfDump"),
		TEXT("On the next ArcRmlUI thread pump, logs for each open view: element count by tag, stylesheet rules, active animations and transitions, and the attribution lists of the last interval."),
		FConsoleCommandDelegate::CreateLambda([]()
		{
			bDumpRequested.store(true);
			if (DumpWaker)
			{
				DumpWaker();
			}
		}));
}

bool ArcRmlUIPerf::IsEnabled()
{
	return GetLevel() > 0;
}

int32 ArcRmlUIPerf::GetLevel()
{
	return FMath::Max(0, ArcRmlUIPerfPrivate::CVarPerfLog.GetValueOnAnyThread());
}

int32 ArcRmlUIPerf::GetTopN()
{
	return FMath::Max(1, ArcRmlUIPerfPrivate::CVarPerfTopN.GetValueOnAnyThread());
}

bool ArcRmlUIPerf::IsLayoutVerifyEnabled()
{
	return ArcRmlUIPerfPrivate::CVarLayoutVerify.GetValueOnAnyThread() > 0;
}

bool ArcRmlUIPerf::GetClockOverride(double& OutSeconds)
{
	if (!ArcRmlUIPerfPrivate::bClockFrozen.load())
	{
		return false;
	}
	OutSeconds = ArcRmlUIPerfPrivate::FrozenClockSeconds.load();
	return true;
}

double ArcRmlUIPerf::GetIntervalSeconds()
{
	return FMath::Max(0.1, static_cast<double>(ArcRmlUIPerfPrivate::CVarPerfLogInterval.GetValueOnAnyThread()));
}

double ArcRmlUIPerf::CyclesToMs(uint64 StartCycles, uint64 EndCycles)
{
	return FPlatformTime::ToMilliseconds64(EndCycles - StartCycles);
}

double ArcRmlUIPerf::SinceMs(uint64 StartCycles)
{
	return CyclesToMs(StartCycles, FPlatformTime::Cycles64());
}

void ArcRmlUIPerf::SetDumpWaker(TFunction<void()>&& Waker)
{
	check(IsInGameThread());
	ArcRmlUIPerfPrivate::DumpWaker = MoveTemp(Waker);
}

bool ArcRmlUIPerf::ConsumeDumpRequest()
{
	return ArcRmlUIPerfPrivate::bDumpRequested.exchange(false);
}

void ArcRmlUIPerf::FTiming::Add(double Ms)
{
	++Count;
	TotalMs += Ms;
	MaxMs = FMath::Max(MaxMs, Ms);
}

double ArcRmlUIPerf::FTiming::GetAverageMs() const
{
	return Count > 0 ? TotalMs / static_cast<double>(Count) : 0.0;
}

bool ArcRmlUIPerf::FReportClock::ShouldReport(double& OutElapsedSeconds)
{
	const double Now = FPlatformTime::Seconds();
	if (LastReportSeconds == 0.0)
	{
		LastReportSeconds = Now;
		return false;
	}

	OutElapsedSeconds = Now - LastReportSeconds;
	if (OutElapsedSeconds < GetIntervalSeconds())
	{
		return false;
	}
	LastReportSeconds = Now;
	return true;
}

double ArcRmlUIPerf::FReportClock::GetSecondsUntilReport() const
{
	if (LastReportSeconds == 0.0)
	{
		return GetIntervalSeconds();
	}
	return FMath::Max(0.0, LastReportSeconds + GetIntervalSeconds() - FPlatformTime::Seconds());
}

#endif
