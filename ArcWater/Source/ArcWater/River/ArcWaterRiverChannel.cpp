// Copyright Lukasz Baran. All Rights Reserved.

#include "River/ArcWaterRiverChannel.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeExit.h"

#include "Algo/Reverse.h"

namespace ArcWaterRiverChannelLocal_91b2
{
	constexpr double GeometryToleranceCm = 1.e-4;

	struct FReachBuildState
	{
		TArray<FVector2D> Points;
		TArray<float> Distances;
		TArray<int32> SourcePointToSection;
		float LengthCm = 0.0f;
	};

	bool IsFinite(const FVector2D& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y);
	}

	float SmoothStep(float Value)
	{
		const float T = FMath::Clamp(Value, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	bool Fail(const FString& Message, FText& OutError)
	{
		OutError = FText::FromString(Message);
		return false;
	}

	bool ContinueOrFail(TFunctionRef<bool()> ShouldContinue, FText& OutError)
	{
		if (ShouldContinue())
		{
			return true;
		}
		return Fail(TEXT("River generation was cancelled."), OutError);
	}

	bool ValidateSettings(const FArcWaterRiverModifierSettings& Settings, FText& OutError)
	{
		if (!FMath::IsFinite(Settings.RiversThreshold) || Settings.RiversThreshold < 0.0f || Settings.RiversThreshold > 1.0f)
		{
			return Fail(TEXT("RiversThreshold must be finite and in [0, 1]."), OutError);
		}
		if (!FMath::IsFinite(Settings.HalfWidthCm) || Settings.HalfWidthCm <= 0.0f)
		{
			return Fail(TEXT("HalfWidthCm must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.DepthCm) || Settings.DepthCm < 0.0f)
		{
			return Fail(TEXT("DepthCm must be finite and non-negative."), OutError);
		}
		if (!FMath::IsFinite(Settings.FalloffCm) || Settings.FalloffCm <= 0.0f)
		{
			return Fail(TEXT("FalloffCm must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.BankFreeboardCm) || Settings.BankFreeboardCm < 0.0f)
		{
			return Fail(TEXT("BankFreeboardCm must be finite and non-negative."), OutError);
		}
		if (!FMath::IsFinite(Settings.SlopeSampleDistanceCm) || Settings.SlopeSampleDistanceCm <= 0.0f)
		{
			return Fail(TEXT("SlopeSampleDistanceCm must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.SplineToleranceCm) || Settings.SplineToleranceCm < 0.0f)
		{
			return Fail(TEXT("SplineToleranceCm must be finite and non-negative."), OutError);
		}
		if (!FMath::IsFinite(Settings.MaxStepCm) || Settings.MaxStepCm <= 0.0f)
		{
			return Fail(TEXT("MaxStepCm must be finite and positive."), OutError);
		}
		if (!FMath::IsFinite(Settings.FlowSpeedCmS))
		{
			return Fail(TEXT("FlowSpeedCmS must be finite."), OutError);
		}
		if (!FMath::IsFinite(Settings.TileCm) || Settings.TileCm <= 0.0f)
		{
			return Fail(TEXT("TileCm must be finite and positive."), OutError);
		}
		if (Settings.WidthSegments < 1)
		{
			return Fail(TEXT("WidthSegments must be positive."), OutError);
		}
		return true;
	}

	bool ValidateTerrain(const FArcWaterRiverTerrainInput& Terrain, FText& OutError)
	{
		if (!Terrain.SampleHeight)
		{
			return Fail(TEXT("Terrain height sampler is unavailable."), OutError);
		}
		if (!FMath::IsFinite(Terrain.WorldZOffsetCm) || !FMath::IsFinite(Terrain.WorldZScale) || Terrain.WorldZScale <= 0.0f)
		{
			return Fail(TEXT("Terrain world-height transform must be finite with positive scale."), OutError);
		}
		return true;
	}

	bool SampleTerrain(const FArcWaterRiverTerrainInput& Terrain, const FVector2D& Position, float& OutHeight)
	{
		return Terrain.SampleHeight && Terrain.SampleHeight(Position, OutHeight) && FMath::IsFinite(OutHeight);
	}

	int32 FindDistanceSegment(const FReachBuildState& State, float DistanceCm, bool bClosed)
	{
		if (State.Points.Num() < 2)
		{
			return INDEX_NONE;
		}
		if (bClosed && DistanceCm >= State.Distances.Last())
		{
			return State.Points.Num() - 1;
		}

		int32 Low = 0;
		int32 High = State.Distances.Num() - 1;
		while (Low + 1 < High)
		{
			const int32 Middle = Low + (High - Low) / 2;
			if (State.Distances[Middle] <= DistanceCm)
			{
				Low = Middle;
			}
			else
			{
				High = Middle;
			}
		}
		return FMath::Min(Low, State.Points.Num() - 2);
	}

	FVector2D PositionAtDistance(const FReachBuildState& State, float DistanceCm, bool bClosed)
	{
		float ResolvedDistance = DistanceCm;
		if (bClosed)
		{
			ResolvedDistance = FMath::Fmod(ResolvedDistance, State.LengthCm);
			if (ResolvedDistance < 0.0f)
			{
				ResolvedDistance += State.LengthCm;
			}
		}
		else
		{
			ResolvedDistance = FMath::Clamp(ResolvedDistance, 0.0f, State.LengthCm);
		}

		const int32 SegmentIndex = FindDistanceSegment(State, ResolvedDistance, bClosed);
		if (SegmentIndex == INDEX_NONE)
		{
			return State.Points.IsEmpty() ? FVector2D::ZeroVector : State.Points[0];
		}

		const int32 NextIndex = SegmentIndex + 1 < State.Points.Num() ? SegmentIndex + 1 : 0;
		const float SegmentStart = State.Distances[SegmentIndex];
		const float SegmentEnd = NextIndex == 0 ? State.LengthCm : State.Distances[NextIndex];
		const float Alpha = SegmentEnd > SegmentStart
			? (ResolvedDistance - SegmentStart) / (SegmentEnd - SegmentStart)
			: 0.0f;
		return FMath::Lerp(State.Points[SegmentIndex], State.Points[NextIndex], Alpha);
	}

	bool MakeReachState(const FArcWaterRiverNetworkReachInput& InputReach,
		const FArcWaterRiverModifierSettings& Settings, FReachBuildState& OutState, FText& OutError,
		TFunctionRef<bool()> ShouldContinue)
	{
		const int32 MinimumPoints = InputReach.bClosed ? 3 : 2;
		if (InputReach.Points.Num() < MinimumPoints)
		{
			return Fail(TEXT("A river reach has too few points for its topology."), OutError);
		}
		for (const FVector2D& Point : InputReach.Points)
		{
			if (!ContinueOrFail(ShouldContinue, OutError))
			{
				return false;
			}
			if (!IsFinite(Point))
			{
				return Fail(TEXT("A river reach contains a non-finite point."), OutError);
			}
		}

		OutState = FReachBuildState();
		OutState.SourcePointToSection.Init(INDEX_NONE, InputReach.Points.Num());
		const float TaperStepCm = Settings.HalfWidthCm * 0.25f;
		const float StationStepCm = FMath::Min(Settings.MaxStepCm, TaperStepCm);
		const int32 SegmentCount = InputReach.bClosed ? InputReach.Points.Num() : InputReach.Points.Num() - 1;
		float AccumulatedDistance = 0.0f;

		for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
		{
			if (!ContinueOrFail(ShouldContinue, OutError))
			{
				return false;
			}
			const int32 NextPointIndex = (SegmentIndex + 1) % InputReach.Points.Num();
			const FVector2D Start = InputReach.Points[SegmentIndex];
			const FVector2D End = InputReach.Points[NextPointIndex];
			const float SegmentLength = static_cast<float>(FVector2D::Distance(Start, End));
			if (SegmentIndex == 0)
			{
				OutState.SourcePointToSection[0] = OutState.Points.Add(Start);
				OutState.Distances.Add(0.0f);
			}

			if (SegmentLength <= UE_KINDA_SMALL_NUMBER)
			{
				OutState.SourcePointToSection[NextPointIndex] = OutState.SourcePointToSection[SegmentIndex];
				continue;
			}

			const int32 MinimumDivisions = !InputReach.bClosed && SegmentCount == 1 &&
				InputReach.bStartIsOriginalEndpoint && InputReach.bEndIsOriginalEndpoint ? 2 : 1;
			const int32 DivisionCount = FMath::Max(FMath::CeilToInt(SegmentLength / StationStepCm), MinimumDivisions);
			for (int32 DivisionIndex = 1; DivisionIndex <= DivisionCount; ++DivisionIndex)
			{
				if (!ContinueOrFail(ShouldContinue, OutError))
				{
					return false;
				}
				const bool bClosingDuplicate = InputReach.bClosed && SegmentIndex == SegmentCount - 1 && DivisionIndex == DivisionCount;
				if (bClosingDuplicate)
				{
					continue;
				}
				const float Alpha = static_cast<float>(DivisionIndex) / static_cast<float>(DivisionCount);
				const int32 SectionIndex = OutState.Points.Add(FMath::Lerp(Start, End, Alpha));
				OutState.Distances.Add(AccumulatedDistance + SegmentLength * Alpha);
				if (DivisionIndex == DivisionCount)
				{
					OutState.SourcePointToSection[NextPointIndex] = SectionIndex;
				}
			}
			AccumulatedDistance += SegmentLength;
		}

		OutState.LengthCm = AccumulatedDistance;
		if (!(OutState.LengthCm > UE_KINDA_SMALL_NUMBER) || OutState.Points.Num() < MinimumPoints)
		{
			return Fail(TEXT("A river reach has zero usable length."), OutError);
		}
		return true;
	}

	bool OrientReachDownhill(FArcWaterRiverNetworkReachInput& Reach,
		const FArcWaterRiverTerrainInput& Terrain, int32 ReachIndex, FText& OutError)
	{
		if (Reach.bClosed || Reach.Points.Num() < 2)
		{
			return true;
		}
		float StartHeight = 0.0f;
		float EndHeight = 0.0f;
		if (!SampleTerrain(Terrain, Reach.Points[0], StartHeight) ||
			!SampleTerrain(Terrain, Reach.Points.Last(), EndHeight))
		{
			return Fail(FString::Printf(TEXT("Terrain height input does not cover reach %d endpoints."), ReachIndex), OutError);
		}
		if (StartHeight >= EndHeight)
		{
			return true;
		}

		const int32 LastPointIndex = Reach.Points.Num() - 1;
		Algo::Reverse(Reach.Points);
		for (FArcWaterRiverNetworkJunctionRef& JunctionRef : Reach.Junctions)
		{
			JunctionRef.PointIndex = LastPointIndex - JunctionRef.PointIndex;
		}
		const bool bWasStartEndpoint = Reach.bStartIsOriginalEndpoint;
		Reach.bStartIsOriginalEndpoint = Reach.bEndIsOriginalEndpoint;
		Reach.bEndIsOriginalEndpoint = bWasStartEndpoint;
		return true;
	}
}

FArcWaterRiverModifierSettings::FArcWaterRiverModifierSettings()
{
	FRichCurve* SlopeCurve = WidthScaleBySlope.GetRichCurve();
	const FKeyHandle FlatKey = SlopeCurve->AddKey(0.0f, 1.0f);
	const FKeyHandle SteepKey = SlopeCurve->AddKey(30.0f, 0.4f);
	SlopeCurve->SetKeyInterpMode(FlatKey, RCIM_Linear);
	SlopeCurve->SetKeyInterpMode(SteepKey, RCIM_Linear);
	WidthScaleByElevation.GetRichCurve()->AddKey(0.0f, 1.0f);
}

namespace ArcWaterRiverChannelLocal_91b2
{
	bool MakeReachSections(const FArcWaterRiverNetworkReachInput& InputReach,
		const FArcWaterRiverTerrainInput& Terrain, const FArcWaterRiverModifierSettings& Settings,
		int32 ReachIndex, FReachBuildState& OutState, FArcWaterRiverGeneratedReach& OutReach,
		float& InOutMinimumUntaperedWidth, FText& OutError, TFunctionRef<bool()> ShouldContinue)
	{
		const double RiverLogStart = FPlatformTime::Seconds();
		double RiverLoopHeartbeat = RiverLogStart;
		uint64 RiverLoopVisits = 0;
		UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN MakeReachSections t=%.6f", RiverLogStart);
		ON_SCOPE_EXIT
		{
			UE_LOGF(LogTemp, Display,"[RiverGen] END MakeReachSections elapsed=%.3fs, error=%ls",
				FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
		};
		if (!MakeReachState(InputReach, Settings, OutState, OutError, ShouldContinue))
		{
			return false;
		}

		TArray<float> SmoothedHeights;
		TArray<float> BaseWidths;
		TArray<float> FlowSpeeds;
		SmoothedHeights.SetNumUninitialized(OutState.Points.Num());
		BaseWidths.SetNumUninitialized(OutState.Points.Num());
		FlowSpeeds.SetNumUninitialized(OutState.Points.Num());

		float WindowCm = FMath::Min(Settings.SlopeSampleDistanceCm, OutState.LengthCm);
		if (InputReach.bClosed)
		{
			WindowCm = FMath::Min(WindowCm, OutState.LengthCm * 0.5f);
		}
		if (!(WindowCm > UE_KINDA_SMALL_NUMBER))
		{
			return Fail(TEXT("SlopeSampleDistanceCm cannot form a usable window on a river reach."), OutError);
		}

		for (int32 SectionIndex = 0; SectionIndex < OutState.Points.Num(); ++SectionIndex)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (!ContinueOrFail(ShouldContinue, OutError))
			{
				return false;
			}
			const float DistanceCm = OutState.Distances[SectionIndex];
			float WindowStartCm = DistanceCm - WindowCm * 0.5f;
			float WindowEndCm = DistanceCm + WindowCm * 0.5f;
			if (!InputReach.bClosed)
			{
				WindowStartCm = FMath::Clamp(WindowStartCm, 0.0f, OutState.LengthCm - WindowCm);
				WindowEndCm = WindowStartCm + WindowCm;
			}

			const FVector2D WindowStart = PositionAtDistance(OutState, WindowStartCm, InputReach.bClosed);
			const FVector2D WindowEnd = PositionAtDistance(OutState, WindowEndCm, InputReach.bClosed);
			float StartHeight = 0.0f;
			float EndHeight = 0.0f;
			if (!SampleTerrain(Terrain, WindowStart, StartHeight) || !SampleTerrain(Terrain, WindowEnd, EndHeight))
			{
				return Fail(FString::Printf(TEXT("Terrain height input does not cover reach %d slope window at section %d."),
					ReachIndex, SectionIndex), OutError);
			}

			const float SmoothedHeight = 0.5f * (StartHeight + EndHeight);
			const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan2(FMath::Abs(EndHeight - StartHeight), WindowCm));
			const float WorldElevationMeters =
				(Terrain.WorldZOffsetCm + SmoothedHeight * Terrain.WorldZScale) * 0.01f;
			const float SlopeScale = Settings.WidthScaleBySlope.GetRichCurveConst()->Eval(SlopeDegrees, 1.0f);
			const float ElevationScale = Settings.WidthScaleByElevation.GetRichCurveConst()->Eval(WorldElevationMeters, 1.0f);
			const float BaseWidth = Settings.HalfWidthCm * SlopeScale * ElevationScale;
			if (!FMath::IsFinite(SlopeScale) || SlopeScale <= 0.0f)
			{
				return Fail(TEXT("WidthScaleBySlope must evaluate to a finite positive value."), OutError);
			}
			if (!FMath::IsFinite(ElevationScale) || ElevationScale <= 0.0f)
			{
				return Fail(TEXT("WidthScaleByElevation must evaluate to a finite positive value."), OutError);
			}
			if (!FMath::IsFinite(BaseWidth) || BaseWidth <= 0.0f)
			{
				return Fail(TEXT("The resolved untapered river half width must be finite and positive."), OutError);
			}

			SmoothedHeights[SectionIndex] = SmoothedHeight;
			BaseWidths[SectionIndex] = BaseWidth;
			const float FlowMagnitude = FMath::Abs(Settings.FlowSpeedCmS);
			FlowSpeeds[SectionIndex] = EndHeight > StartHeight ? -FlowMagnitude : FlowMagnitude;
			InOutMinimumUntaperedWidth = FMath::Min(InOutMinimumUntaperedWidth, BaseWidth);
		}

		float RequiredTaperStepCm = Settings.MaxStepCm;
		if (!InputReach.bClosed && InputReach.bStartIsOriginalEndpoint)
		{
			RequiredTaperStepCm = FMath::Min(RequiredTaperStepCm, BaseWidths[0] * 0.25f);
		}
		if (!InputReach.bClosed && InputReach.bEndIsOriginalEndpoint)
		{
			RequiredTaperStepCm = FMath::Min(RequiredTaperStepCm, BaseWidths.Last() * 0.25f);
		}
		const float CurrentStationStepCm = FMath::Min(Settings.MaxStepCm, Settings.HalfWidthCm * 0.25f);
		if (RequiredTaperStepCm + UE_KINDA_SMALL_NUMBER < CurrentStationStepCm)
		{
			FArcWaterRiverModifierSettings RefinedSettings = Settings;
			RefinedSettings.MaxStepCm = RequiredTaperStepCm;
			return MakeReachSections(InputReach, Terrain, RefinedSettings, ReachIndex, OutState, OutReach,
				InOutMinimumUntaperedWidth, OutError, ShouldContinue);
		}

		OutReach = FArcWaterRiverGeneratedReach();
		OutReach.LengthCm = OutState.LengthCm;
		OutReach.TileCm = Settings.TileCm;
		OutReach.bClosed = InputReach.bClosed;
		OutReach.bStartIsOriginalEndpoint = InputReach.bStartIsOriginalEndpoint;
		OutReach.bEndIsOriginalEndpoint = InputReach.bEndIsOriginalEndpoint;
		OutReach.Sections.Reserve(OutState.Points.Num());
		OutReach.SplinePoints.Reserve(OutState.Points.Num());

		for (int32 SectionIndex = 0; SectionIndex < OutState.Points.Num(); ++SectionIndex)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (!ContinueOrFail(ShouldContinue, OutError))
			{
				return false;
			}
			int32 PreviousIndex = FMath::Max(SectionIndex - 1, 0);
			int32 NextIndex = FMath::Min(SectionIndex + 1, OutState.Points.Num() - 1);
			if (InputReach.bClosed)
			{
				PreviousIndex = (SectionIndex + OutState.Points.Num() - 1) % OutState.Points.Num();
				NextIndex = (SectionIndex + 1) % OutState.Points.Num();
			}
			FVector2D Tangent = OutState.Points[NextIndex] - OutState.Points[PreviousIndex];
			if (!Tangent.Normalize())
			{
				const FVector2D& PreviousPoint = OutState.Points[PreviousIndex];
				const FVector2D& CurrentPoint = OutState.Points[SectionIndex];
				const FVector2D& NextPoint = OutState.Points[NextIndex];
				return Fail(FString::Printf(
					TEXT("A river reach cannot resolve a horizontal channel frame: reach=%d section=%d closed=%d stationStepCm=%.9g previous=(%.9g,%.9g) current=(%.9g,%.9g) next=(%.9g,%.9g)."),
					ReachIndex, SectionIndex, InputReach.bClosed ? 1 : 0, static_cast<double>(CurrentStationStepCm),
					PreviousPoint.X, PreviousPoint.Y, CurrentPoint.X, CurrentPoint.Y, NextPoint.X, NextPoint.Y), OutError);
			}

			float Taper = 1.0f;
			if (!InputReach.bClosed && InputReach.bStartIsOriginalEndpoint)
			{
				Taper = FMath::Min(Taper, SmoothStep(OutState.Distances[SectionIndex] / BaseWidths[0]));
			}
			if (!InputReach.bClosed && InputReach.bEndIsOriginalEndpoint)
			{
				const float DistanceToEnd = OutState.LengthCm - OutState.Distances[SectionIndex];
				Taper = FMath::Min(Taper, SmoothStep(DistanceToEnd / BaseWidths.Last()));
			}

			FArcWaterRiverChannelSection& Section = OutReach.Sections.AddDefaulted_GetRef();
			Section.Position = FVector(OutState.Points[SectionIndex],
				SmoothedHeights[SectionIndex] - Settings.BankFreeboardCm * Taper);
			Section.Lateral = FVector2D(-Tangent.Y, Tangent.X);
			Section.DistanceAlongCm = OutState.Distances[SectionIndex];
			Section.HalfWidthCm = BaseWidths[SectionIndex] * Taper;
			Section.DepthCm = Settings.DepthCm * Taper;
			Section.FalloffCm = Settings.FalloffCm * Taper;
			Section.FreeboardCm = Settings.BankFreeboardCm * Taper;
			Section.FlowSpeedCmS = FlowSpeeds[SectionIndex];
			Section.ReachIndex = ReachIndex;
			Section.SectionIndex = SectionIndex;
			Section.WidthSegments = Settings.WidthSegments;
			Section.bClosesReach = InputReach.bClosed && SectionIndex == OutState.Points.Num() - 1;
			OutReach.SplinePoints.Add(Section.Position);
		}
		return true;
	}

}

bool ArcWaterRiverChannel::BuildChannels(const FArcWaterRiverNetworkInput& Input,
	const FArcWaterRiverModifierSettings& Settings,
	FArcWaterRiverGeneratedData& OutData, FText& OutError,
	TFunctionRef<bool()> ShouldContinue)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	double RiverLoopHeartbeat = RiverLogStart;
	uint64 RiverLoopVisits = 0;
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildChannels t=%.6f", RiverLogStart);
	UE_LOGF(LogTemp, Display,"[RiverGen] CHANNEL INPUT reaches=%d junctions=%d heightSampler=%d",
		Input.Reaches.Num(), Input.JunctionPositions.Num(), Input.Terrain.SampleHeight ? 1 : 0);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END BuildChannels elapsed=%.3fs, error=%ls",
			FPlatformTime::Seconds() - RiverLogStart, *OutError.ToString());
	};
	using namespace ArcWaterRiverChannelLocal_91b2;

	FArcWaterRiverGeneratedData WorkingData;
	OutError = FText::GetEmpty();
	if (!ContinueOrFail(ShouldContinue, OutError) || !ValidateSettings(Settings, OutError) ||
		!ValidateTerrain(Input.Terrain, OutError))
	{
		return false;
	}
	if (Input.Reaches.IsEmpty())
	{
		return Fail(TEXT("The river network contains no reaches."), OutError);
	}
	for (const FVector2D& JunctionPosition : Input.JunctionPositions)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		if (!ContinueOrFail(ShouldContinue, OutError))
		{
			return false;
		}
		if (!IsFinite(JunctionPosition))
		{
			return Fail(TEXT("The river network contains a non-finite junction position."), OutError);
		}
	}

	TArray<FArcWaterRiverNetworkReachInput> OrderedReaches = Input.Reaches;
	for (int32 ReachIndex = 0; ReachIndex < OrderedReaches.Num(); ++ReachIndex)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		if (!ContinueOrFail(ShouldContinue, OutError))
		{
			return false;
		}
		if (!OrientReachDownhill(OrderedReaches[ReachIndex], Input.Terrain, ReachIndex, OutError))
		{
			return false;
		}
	}

	TArray<FReachBuildState> ReachStates;
	ReachStates.SetNum(OrderedReaches.Num());
	WorkingData.Reaches.SetNum(OrderedReaches.Num());
	float MinimumUntaperedWidth = TNumericLimits<float>::Max();
	for (int32 ReachIndex = 0; ReachIndex < OrderedReaches.Num(); ++ReachIndex)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		if (!ContinueOrFail(ShouldContinue, OutError) ||
			!MakeReachSections(OrderedReaches[ReachIndex], Input.Terrain, Settings, ReachIndex,
				ReachStates[ReachIndex], WorkingData.Reaches[ReachIndex], MinimumUntaperedWidth, OutError,
				ShouldContinue))
		{
			return false;
		}
	}

	for (int32 ReachIndex = 0; ReachIndex < OrderedReaches.Num(); ++ReachIndex)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		const FArcWaterRiverNetworkReachInput& InputReach = OrderedReaches[ReachIndex];
		for (const FArcWaterRiverNetworkJunctionRef& JunctionRef : InputReach.Junctions)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (!ContinueOrFail(ShouldContinue, OutError))
			{
				return false;
			}
			if (!Input.JunctionPositions.IsValidIndex(JunctionRef.JunctionId) ||
				!InputReach.Points.IsValidIndex(JunctionRef.PointIndex))
			{
				return Fail(FString::Printf(TEXT("Reach %d contains an invalid junction reference."), ReachIndex), OutError);
			}
			if ((!InputReach.bClosed && InputReach.bStartIsOriginalEndpoint && JunctionRef.PointIndex == 0) ||
				(!InputReach.bClosed && InputReach.bEndIsOriginalEndpoint && JunctionRef.PointIndex == InputReach.Points.Num() - 1))
			{
				return Fail(FString::Printf(TEXT("Reach %d marks a junction as an original degree-one endpoint."), ReachIndex), OutError);
			}

			const int32 SectionIndex = ReachStates[ReachIndex].SourcePointToSection[JunctionRef.PointIndex];
			if (!WorkingData.Reaches[ReachIndex].Sections.IsValidIndex(SectionIndex))
			{
				return Fail(FString::Printf(TEXT("Reach %d cannot map a junction to a channel section."), ReachIndex), OutError);
			}

			float SharedBaseHeight = 0.0f;
			const FVector2D SharedPosition = Input.JunctionPositions[JunctionRef.JunctionId];
			if (!SampleTerrain(Input.Terrain, SharedPosition, SharedBaseHeight))
			{
				return Fail(FString::Printf(TEXT("Terrain height input does not cover junction %d."), JunctionRef.JunctionId), OutError);
			}

			// Every incident reach uses the same sampled junction position and water elevation.
			FArcWaterRiverChannelSection& Section = WorkingData.Reaches[ReachIndex].Sections[SectionIndex];
			Section.Position.X = SharedPosition.X;
			Section.Position.Y = SharedPosition.Y;
			Section.Position.Z = SharedBaseHeight - Settings.BankFreeboardCm;
			Section.DepthCm = Settings.DepthCm;
			Section.FalloffCm = Settings.FalloffCm;
			Section.FreeboardCm = Settings.BankFreeboardCm;
			WorkingData.Reaches[ReachIndex].SplinePoints[SectionIndex] = Section.Position;
		}
	}

	FBox2D CoverageBounds(ForceInit);
	for (const FArcWaterRiverGeneratedReach& Reach : WorkingData.Reaches)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		if (!ContinueOrFail(ShouldContinue, OutError))
		{
			return false;
		}
		for (const FArcWaterRiverChannelSection& Section : Reach.Sections)
		{
			if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
			{
				RiverLoopHeartbeat = FPlatformTime::Seconds();
				UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
			}
			if (!ContinueOrFail(ShouldContinue, OutError))
			{
				return false;
			}
			const FVector2D Center(Section.Position.X, Section.Position.Y);
			const double Radius = static_cast<double>(Section.HalfWidthCm + Section.FalloffCm);
			CoverageBounds += Center - FVector2D(Radius, Radius);
			CoverageBounds += Center + FVector2D(Radius, Radius);
		}
	}
	if (!CoverageBounds.bIsValid)
	{
		return Fail(TEXT("The river network produced invalid coverage bounds."), OutError);
	}

	if (!ContinueOrFail(ShouldContinue, OutError))
	{
		return false;
	}
	WorkingData.JunctionPositions = Input.JunctionPositions;
	WorkingData.CoverageCenterXY = CoverageBounds.GetCenter();
	WorkingData.CoverageXY = CoverageBounds.GetSize();
	WorkingData.MinimumUntaperedHalfWidthCm = MinimumUntaperedWidth;
	WorkingData.MinimumUntaperedFalloffCm = Settings.FalloffCm;
	OutData = MoveTemp(WorkingData);
	return true;
}

namespace ArcWaterRiverChannelLocal_91b2
{
	void AppendWaterTriangle(const TArray<FDynamicMeshVertex>& Vertices,
		uint32 A, uint32 B, uint32 C, TArray<uint32>& OutIndices, int32& InOutSkippedTriangles)
	{
		const FVector3f AB = Vertices[B].Position - Vertices[A].Position;
		const FVector3f AC = Vertices[C].Position - Vertices[A].Position;
		if (FVector3f::CrossProduct(AB, AC).SizeSquared() <=
			FMath::Square(static_cast<float>(GeometryToleranceCm)))
		{
			++InOutSkippedTriangles;
			return;
		}
		OutIndices.Add(A);
		OutIndices.Add(B);
		OutIndices.Add(C);
	}
}

void ArcWaterRiverChannel::BuildWaterMesh(const FArcWaterRiverGeneratedData& Data, int32 ReachIndex,
	TArray<FDynamicMeshVertex>& OutVertices, TArray<uint32>& OutIndices)
{
	const double RiverLogStart = FPlatformTime::Seconds();
	double RiverLoopHeartbeat = RiverLogStart;
	uint64 RiverLoopVisits = 0;
	UE_LOGF(LogTemp, Display,"[RiverGen] BEGIN BuildWaterMesh t=%.6f", RiverLogStart);
	ON_SCOPE_EXIT
	{
		UE_LOGF(LogTemp, Display,"[RiverGen] END BuildWaterMesh elapsed=%.3fs",
			FPlatformTime::Seconds() - RiverLogStart);
	};
	using namespace ArcWaterRiverChannelLocal_91b2;

	OutVertices.Reset();
	OutIndices.Reset();
	if (!Data.Reaches.IsValidIndex(ReachIndex))
	{
		return;
	}
	const FArcWaterRiverGeneratedReach& Reach = Data.Reaches[ReachIndex];
	if (Reach.Sections.Num() < 2)
	{
		return;
	}
	const int32 WidthSegments = Reach.Sections[0].WidthSegments;
	if (WidthSegments < 1)
	{
		return;
	}
	for (const FArcWaterRiverChannelSection& Section : Reach.Sections)
	{
		if (Section.WidthSegments != WidthSegments || Section.Position.ContainsNaN() ||
			!IsFinite(Section.Lateral) || !FMath::IsFinite(Section.HalfWidthCm) ||
			Section.HalfWidthCm < 0.0f || !FMath::IsFinite(Section.DistanceAlongCm) ||
			!FMath::IsFinite(Section.FlowSpeedCmS))
		{
			return;
		}
	}

	const int64 SectionRowCount64 = static_cast<int64>(Reach.Sections.Num()) + (Reach.bClosed ? 1ll : 0ll);
	const int64 RowStride64 = static_cast<int64>(WidthSegments) + 1ll;
	const int64 VertexCount = SectionRowCount64 * RowStride64;
	if (VertexCount <= 0 || VertexCount > MAX_int32)
	{
		return;
	}
	const int64 IndexCount = (SectionRowCount64 - 1ll) * static_cast<int64>(WidthSegments) * 6ll;
	if (IndexCount <= 0 || IndexCount > MAX_int32)
	{
		return;
	}
	const int32 SectionRowCount = static_cast<int32>(SectionRowCount64);
	const int32 RowStride = static_cast<int32>(RowStride64);
	OutVertices.Reserve(static_cast<int32>(VertexCount));
	for (int32 RowIndex = 0; RowIndex < SectionRowCount; ++RowIndex)
	{
		if ((++RiverLoopVisits & 4095ull) == 0 && FPlatformTime::Seconds() - RiverLoopHeartbeat >= 2.0)
		{
			RiverLoopHeartbeat = FPlatformTime::Seconds();
			UE_LOGF(LogTemp, Display,"[RiverGen] LOOP %s:%d visits=%llu elapsed=%.3fs", __FUNCTION__, __LINE__, RiverLoopVisits, RiverLoopHeartbeat - RiverLogStart);
		}
		const int32 SectionIndex = RowIndex < Reach.Sections.Num() ? RowIndex : 0;
		const FArcWaterRiverChannelSection& Section = Reach.Sections[SectionIndex];
		FVector2D Lateral = Section.Lateral;
		if (!Lateral.Normalize())
		{
			OutVertices.Reset();
			return;
		}
		const float DistanceAlongCm = RowIndex < Reach.Sections.Num()
			? Section.DistanceAlongCm
			: Reach.LengthCm;
		const FVector3f Tangent(
			static_cast<float>(Lateral.Y), static_cast<float>(-Lateral.X), 0.0f);
		const FVector3f Bitangent(
			static_cast<float>(Lateral.X), static_cast<float>(Lateral.Y), 0.0f);
		for (int32 WidthIndex = 0; WidthIndex <= WidthSegments; ++WidthIndex)
		{
			const float Across = static_cast<float>(WidthIndex) / static_cast<float>(WidthSegments);
			const float OffsetCm = FMath::Lerp(-Section.HalfWidthCm, Section.HalfWidthCm, Across);
			const FVector Position = Section.Position + FVector(Lateral.X, Lateral.Y, 0.0) * OffsetCm;
			FDynamicMeshVertex Vertex{FVector3f(Position)};
			Vertex.SetTangents(Tangent, Bitangent, FVector3f::UpVector);
			Vertex.TextureCoordinate[0] = FVector2f(
				DistanceAlongCm / FMath::Max(Reach.TileCm, 1.0f), Across);
			Vertex.TextureCoordinate[1] = FVector2f(Section.FlowSpeedCmS, 0.0f);
			OutVertices.Add(Vertex);
		}
	}

	const int32 SegmentCount = Reach.bClosed ? Reach.Sections.Num() : Reach.Sections.Num() - 1;
	OutIndices.Reserve(static_cast<int32>(IndexCount));
	int32 SkippedTriangleCount = 0;
	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const uint32 StartRow = static_cast<uint32>(SegmentIndex * RowStride);
		const uint32 EndRow = static_cast<uint32>((SegmentIndex + 1) * RowStride);
		for (int32 WidthIndex = 0; WidthIndex < WidthSegments; ++WidthIndex)
		{
			const uint32 A = StartRow + static_cast<uint32>(WidthIndex);
			const uint32 B = A + 1u;
			const uint32 C = EndRow + static_cast<uint32>(WidthIndex);
			const uint32 D = C + 1u;
			AppendWaterTriangle(OutVertices, A, C, B, OutIndices, SkippedTriangleCount);
			AppendWaterTriangle(OutVertices, B, C, D, OutIndices, SkippedTriangleCount);
		}
	}
	if (SkippedTriangleCount > 0)
	{
		UE_LOGF(LogTemp, Warning,"[RiverGen] Skipped %d degenerate water ribbon triangles for reach %d.",
			SkippedTriangleCount, ReachIndex);
	}
}
