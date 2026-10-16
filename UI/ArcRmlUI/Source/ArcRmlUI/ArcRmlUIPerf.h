// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING
#define ARCRMLUI_PERF(...) __VA_ARGS__
#else
#define ARCRMLUI_PERF(...)
#endif

#if !UE_BUILD_SHIPPING
#ifndef RMLUI_ARC_PERF
#error "ArcRmlUiCore defines RMLUI_ARC_PERF for every non-Shipping configuration; ArcRmlUI perf logging depends on it."
#endif

namespace ArcRmlUIPerf
{
	bool IsEnabled();
	int32 GetLevel();
	int32 GetTopN();
	bool IsLayoutVerifyEnabled();
	bool GetClockOverride(double& OutSeconds);
	double GetIntervalSeconds();
	double CyclesToMs(uint64 StartCycles, uint64 EndCycles);
	double SinceMs(uint64 StartCycles);

	void SetDumpWaker(TFunction<void()>&& Waker);
	bool ConsumeDumpRequest();

	struct FTiming
	{
		int32 Count = 0;
		double TotalMs = 0.0;
		double MaxMs = 0.0;

		void Add(double Ms);
		double GetAverageMs() const;
	};

	class FReportClock
	{
	public:
		bool ShouldReport(double& OutElapsedSeconds);
		double GetSecondsUntilReport() const;

	private:
		double LastReportSeconds = 0.0;
	};
}
#endif
