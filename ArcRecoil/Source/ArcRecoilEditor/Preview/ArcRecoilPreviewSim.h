// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcRecoilFragments.h"

class UArcRecoilPreset;
class UArcRecoilPreviewSettings;

namespace ArcRecoil::Preview
{
	// The draft caps seeds and drawn spread samples only, never SimulationHz: SampleCount has to stay put or a sample index stops meaning the same shot.
	constexpr int32 DraftSeedCount = 4;
	constexpr int32 DraftSpreadSamples = 256;

	struct FArcRecoilPreviewQuality
	{
		int32 MaxSeedCount = 0;
		int32 MaxSpreadSamples = 0;
	};

	struct FArcRecoilPreviewRun
	{
		TArray<FVector2f> CameraDeg;
		TArray<FVector2f> AimOffsetDeg;
	};

	struct FArcRecoilPreviewResult
	{
		TArray<FArcRecoilPreviewRun> Runs;
		TArray<FVector2f> MeanCameraDeg;
		TArray<FVector2f> MeanAimOffsetDeg;
		TArray<float> SpreadHalfAngleDeg;
		TArray<int32> ShotSampleIndices;
		FArcRecoilProfileFragment BakedProfile;
		int32 SampleCount = 0;
		int32 StepsPerShot = 0;
		int32 ShotCount = 0;
		int32 RunCount = 0;
		int32 FullRunCount = 0;
		int32 MaxSpreadSamples = 0;
		float SecondsPerSample = 0.0f;
		float CameraMaxAbsDeg = 1.0f;
		float ImpactMaxAbsDeg = 1.0f;
		bool bDraft = false;
		bool bSeedCapBound = false;
		bool bSpreadCapBound = false;
	};

	FArcRecoilPreviewResult Simulate(const UArcRecoilPreset& Preset,
		const UArcRecoilPreviewSettings& Settings,
		const FArcRecoilPreviewQuality& Quality);
}
