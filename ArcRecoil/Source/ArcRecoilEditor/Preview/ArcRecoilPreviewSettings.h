// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Fragments/ArcRecoilFragments.h"
#include "UObject/Object.h"

#include "ArcRecoilPreviewSettings.generated.h"

namespace ArcRecoil::Preview
{
	constexpr float MinPlaybackSpeed = 0.05f;
	constexpr float MaxPlaybackSpeed = 4.0f;
}

UCLASS(Transient)
class UArcRecoilPreviewSettings : public UObject
{
	GENERATED_BODY()

public:
	/** The four per-weapon stats the preset is baked against; the preset alone does not determine the pattern. */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FArcRecoilStats Stats;

	/** Fire rate the burst is simulated at, in rounds per minute. */
	UPROPERTY(EditAnywhere, Category = "Weapon", meta = (ClampMin = "1"))
	float RoundsPerMinute = 600.0f;

	/** How many shots the simulated burst fires. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "600", UIMin = "1", UIMax = "600"))
	int32 MaxShots = 30;

	/** How many independent random runs are simulated, one per seed. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "64", UIMin = "1", UIMax = "64"))
	int32 SeedCount = 24;

	/** Seed of the first run; the rest count up from it. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "0"))
	int32 BaseSeed = 1;

	/** How many cone samples are drawn around the aim point at the displayed shot. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "4096", UIMin = "1", UIMax = "4096"))
	int32 SpreadSamplesPerShot = 64;

	/** Integration rate of the simulation, in substeps per second. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "30", ClampMax = "1000", UIMin = "30", UIMax = "1000"))
	float SimulationHz = 120.0f;

	/** How long the simulation keeps running after the last shot, in seconds, so recovery and cooldown are visible. */
	UPROPERTY(EditAnywhere, Category = "Simulation", meta = (ClampMin = "0"))
	float TailSeconds = 1.5f;

	/** Draws the per-seed camera paths and their mean. */
	UPROPERTY(EditAnywhere, Category = "Display")
	bool bShowCameraPaths = true;

	/** Draws the per-seed aim-offset points and their trail. */
	UPROPERTY(EditAnywhere, Category = "Display")
	bool bShowAimOffsetCloud = true;

	/** Draws the spread cone and the shots landing inside it. */
	UPROPERTY(EditAnywhere, Category = "Display")
	bool bShowSpread = true;

	/** Draws the ellipse the aim offset is clamped to. */
	UPROPERTY(EditAnywhere, Category = "Display")
	bool bShowClampEllipse = true;

	/** Time dilation of playback; 1 runs at the weapon's real fire rate. */
	UPROPERTY(EditAnywhere, Category = "Playback",
		meta = (ClampMin = "0.05", ClampMax = "4.0", UIMin = "0.05", UIMax = "4.0"))
	float PlaybackSpeed = 1.0f;

	/** Restarts playback from the first shot instead of stopping at the end. */
	UPROPERTY(EditAnywhere, Category = "Playback")
	bool bLoop = true;
};
