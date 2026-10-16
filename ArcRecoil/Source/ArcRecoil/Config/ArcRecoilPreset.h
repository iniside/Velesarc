// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "ArcRecoilPreset.generated.h"

/**
 * One layer's own kick. Layers do not share a base impulse: this block is the whole description
 * of that layer's per-shot push, so editing one layer's block never touches the other's.
 */
USTRUCT(BlueprintType)
struct ARCRECOIL_API FArcRecoilLayerKick
{
	GENERATED_BODY()

	/** This layer's per-shot push in degrees for a weapon with Stability 0 (X = up, Y = sideways). Stability scales both components down to 0 at 100. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", DisplayPriority = "10"))
	FVector2f KickDeg = FVector2f(1.0f, 0.4f);

	/**
	 * Which way this layer walks, as a phase rather than a magnitude. 100 = dead vertical.
	 * Values ending in 5 also come out vertical; values ending in 0 drift hardest sideways,
	 * and the side flips every 10 (50 left, 60 right, 70 left...). Amplitude fades as this
	 * approaches 100. Deliberately NOT a designer stat: it is not monotonic, so 55 -> 60
	 * makes the weapon worse, not better.
	 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", ClampMax = "100", DisplayPriority = "20"))
	float RecoilPhase = 100.0f;

	/** How much of this layer's sideways kick is random rather than the fixed RecoilPhase bias, at Control 0. Control scales this to 0 at 100, making the pattern learnable. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", DisplayPriority = "30"))
	float YawNoise = 0.5f;

	/** Vertical kick multiplier for this layer during the opening shots of a burst (see SuppressionShots). Sideways kick is not multiplied. */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", DisplayPriority = "40"))
	float FirstShotMul = 1.6f;

	/**
	 * How many shots one full swing of the walk takes. Lower reads as a tighter zigzag; higher
	 * stretches one long arc across the burst. 0 disables the walk entirely and the layer kicks
	 * in the fixed RecoilPhase direction for the whole burst. Vertical only actually reverses
	 * when WalkAmplitude.X is above 1.
	 */
	UPROPERTY(EditAnywhere, meta = (ClampMin = "0", DisplayPriority = "50"))
	float WalkPeriodShots = 0.0f;

	/**
	 * How far the walk swings the direction over one period (X = up, Y = sideways). X multiplies the
	 * vertical kick, so 0.7 makes the climb run between 1.7x and 0.3x its normal strength and above 1
	 * pulls the layer down through the bottom of the swing. Y adds to the sideways bias, so 0.8 against
	 * a +0.4 RecoilPhase bias sweeps the drift from hard right to left and back.
	 */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "60"))
	FVector2f WalkAmplitude = FVector2f::ZeroVector;

	/** How far the sideways swing lags the vertical one, in degrees of the walk cycle. 0 walks a straight diagonal that retraces itself; 90 opens it into a loop. */
	UPROPERTY(EditAnywhere, meta = (DisplayPriority = "70"))
	float WalkPhaseOffsetDeg = 90.0f;

	FArcRecoilLayerKick() = default;

	FArcRecoilLayerKick(const FVector2f& InKickDeg,
		const float InRecoilPhase,
		const float InYawNoise,
		const float InFirstShotMul,
		const float InWalkPeriodShots = 0.0f,
		const FVector2f& InWalkAmplitude = FVector2f::ZeroVector,
		const float InWalkPhaseOffsetDeg = 90.0f)
		: KickDeg(InKickDeg)
		, RecoilPhase(InRecoilPhase)
		, YawNoise(InYawNoise)
		, FirstShotMul(InFirstShotMul)
		, WalkPeriodShots(InWalkPeriodShots)
		, WalkAmplitude(InWalkAmplitude)
		, WalkPhaseOffsetDeg(InWalkPhaseOffsetDeg)
	{
	}
};

/**
 * Recoil coefficients shared by a whole weapon class (rifle, SMG, MG, cannon).
 * Paired with four 0-100 per-weapon stats (Stability / Control / Accuracy / Handling)
 * and baked once into FArcRecoilProfileFragment; the runtime never reads this asset.
 *
 * Each shot pushes three independent layers. Layers 1 and 2 each carry their own kick block,
 * so magnitude, direction and first-shot behaviour are set per layer rather than shared:
 *   Layer 1 - Camera Kick: rotates the view. The player fights it back down with the mouse.
 *   Layer 2 - Aim Offset:  walks where the shots actually land away from the reticle, without
 *                          moving the view. This is the reticle-vs-impact drift.
 *   Layer 3 - Spread:      random cone around wherever layers 1 and 2 left the muzzle pointing.
 *
 * Defaults describe a mid-weight assault rifle.
 */
UCLASS(BlueprintType)
class ARCRECOIL_API UArcRecoilPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	/** How many opening shots get each layer's FirstShotMul before the weapon settles into its normal kick. Shared by both layers; what differs per layer is the multiplier itself. */
	UPROPERTY(EditAnywhere, Category = "Impulse", meta = (ClampMin = "0", DisplayPriority = "10"))
	int32 SuppressionShots = 2;

	/**
	 * How long the weapon must stay quiet before the walk has given a whole swing back and the next burst
	 * starts over at the top of the pattern, in seconds. Nothing is given back for the first half of that
	 * window, so set this to at least twice the slowest shot spacing the weapon is meant to keep walking
	 * at - a gap wider than half this value unwinds faster than firing advances, and the pattern pins at
	 * its opening phase. A pause inside the second half hands back part of the swing and the next burst
	 * picks up mid-pattern. Timed off whichever layer has the longer WalkPeriodShots so both come back
	 * together. 0 never rewinds and the walk runs on unbroken across every burst.
	 */
	UPROPERTY(EditAnywhere, Category = "Impulse", meta = (ClampMin = "0", DisplayPriority = "20"))
	float WalkResetSeconds = 1.5f;

	/**
	 * The push this layer gives the view - classic camera kick the player fights back down with the mouse.
	 * Zero KickDeg means the view holds still. The default climbs hard and stays close to vertical.
	 * Set independently of Layer 2's AimOffsetKick: nothing here caps or feeds that layer, and both loud
	 * simply means a louder weapon overall.
	 */
	UPROPERTY(EditAnywhere, Category = "Layer 1 - Camera Kick", meta = (DisplayPriority = "10"))
	FArcRecoilLayerKick CameraKick = FArcRecoilLayerKick(FVector2f(1.0f, 0.4f), 100.0f, 0.5f, 1.6f);

	/** Quiet time after the last shot before the camera starts returning, in seconds. Stops recovery from fighting the player mid-burst. */
	UPROPERTY(EditAnywhere, Category = "Layer 1 - Camera Kick", meta = (ClampMin = "0", DisplayPriority = "20"))
	float RecoveryDelayBase = 0.25f;

	/** How fast the camera gives back the climb it took. Only the part the player did not already pull down is returned, so compensating is never punished. */
	UPROPERTY(EditAnywhere, Category = "Layer 1 - Camera Kick", meta = (ClampMin = "0", DisplayPriority = "30"))
	float RecoveryRateBase = 6.0f;

	/**
	 * The push that walks the shots off the reticle without moving the view. This is the drift between
	 * where the player is pointing and where the round actually lands - the gap the impact marker shows.
	 * Zero KickDeg means shots keep landing on the reticle; no amount of mouse correction compensates
	 * whatever this layer takes. Set independently of Layer 1's CameraKick - raising one never lowers
	 * the other, and the default phase walks the aim point sideways while the view climbs vertically.
	 */
	UPROPERTY(EditAnywhere, Category = "Layer 2 - Aim Offset", meta = (DisplayPriority = "10"))
	FArcRecoilLayerKick AimOffsetKick = FArcRecoilLayerKick(FVector2f(0.42f, 0.56f), 60.0f, 0.8f, 1.0f, 20.0f, FVector2f(1.4f, 0.8f), 90.0f);

	/**
	 * How hard the drifted aim point is pulled back onto the reticle; Control scales this up.
	 * Sustained fire settles where this balances the incoming impulses, so raising it shrinks
	 * the reticle-to-impact gap and shortens the settle; lowering it opens the gap wider.
	 */
	UPROPERTY(EditAnywhere, Category = "Layer 2 - Aim Offset", meta = (ClampMin = "0", DisplayPriority = "20"))
	float StiffnessBase = 9.0f;

	/** 1.0 returns to the reticle as fast as possible without overshooting. Below 1 overshoots slightly and reads as a weapon settling; above 1 feels heavy and sluggish. */
	UPROPERTY(EditAnywhere, Category = "Layer 2 - Aim Offset", meta = (ClampMin = "0", DisplayPriority = "30"))
	float DampingRatio = 0.9f;

	/** How far the aim point may drift off the reticle before it is pinned, in degrees (X = up, Y = sideways). A hard ceiling, not the resting point of sustained fire. Stability shrinks it. */
	UPROPERTY(EditAnywhere, Category = "Layer 2 - Aim Offset", meta = (DisplayPriority = "40"))
	FVector2f MaxOffsetBaseDeg = FVector2f(6.0f, 3.5f);

	/** How much of each kick enters the aim-offset spring as velocity instead of raw displacement. Higher reads as a punchier ramp; 0 makes the aim point teleport. */
	UPROPERTY(EditAnywhere, Category = "Layer 2 - Aim Offset", meta = (ClampMin = "0", DisplayPriority = "50"))
	float ImpulseToVelGain = 6.0f;

	/** Extra cone width in degrees at full heat, on top of the floor set by Accuracy. Full angle, not half. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "10"))
	float SpreadRangeFullDeg = 8.0f;

	/** Shape of heat -> cone width. 1 = straight line; above 1 keeps the cone tight until the weapon is genuinely hot; below 1 opens it up immediately. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "20"))
	float SpreadCurveExp = 1.5f;

	/** Where shots land inside the cone. 0.5 spreads them evenly across the disc (shotgun); 1.0 packs them toward the centre (rifle); higher packs them tighter still. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "30"))
	float SpreadExponent = 1.0f;

	/** Heat added per shot at Accuracy 100. Heat runs 0-1 and exists only to drive the cone above; lower Accuracy raises this. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "40"))
	float HeatPerShotBase = 0.12f;

	/** Heat shed per second once the weapon starts cooling. Handling scales this up. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "50"))
	float CoolRateBase = 0.6f;

	/** Quiet time after the last shot before heat starts falling, in seconds. This is what makes burst discipline pay off. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "60"))
	float CooldownDelayBase = 0.12f;

	/** How fast the shot counter unwinds per second once firing stops. Governs how soon FirstShotMul comes back, so tap-firing cannot farm it. */
	UPROPERTY(EditAnywhere, Category = "Layer 3 - Spread", meta = (ClampMin = "0", DisplayPriority = "70"))
	float ShotIndexDecayBase = 4.0f;
};
