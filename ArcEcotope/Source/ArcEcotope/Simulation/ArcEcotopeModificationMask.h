// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

#include "ArcEcotopeModificationMask.generated.h"

class UWorld;

/**
 * A rectangular cut authored content places to push the biome back where people have been. It is the
 * climate volume's primitive with a negative sign and a two-sided band, so the forest meets a road or a
 * building smoothly from both directions instead of stepping at the footprint edge.
 *
 * The class is the contribution entry point: a road or a building contributes by owning one, exactly as
 * ArcTown contributes to terrain by owning a stamp. It is not final and its two evaluators are virtual,
 * so a spline-shaped road mask subclasses it without touching the gather.
 */
UCLASS(ClassGroup = (ArcEcotope), meta = (BlueprintSpawnableComponent))
class ARCECOTOPE_API UArcEcotopeModificationMask : public USceneComponent
{
	GENERATED_BODY()

public:
	UArcEcotopeModificationMask();

	/** Cut depth at full strength: 0 leaves the biome untouched, 1 is the deepest cut a mask can author. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
		meta = (DisplayName = "Cut Strength", ClampMin = "0.0", ClampMax = "1.0"))
	double CutStrength = 1.0;

	/**
	 * Cut footprint extent in the component's local XY, centred on the component. With Gradient Offset at
	 * 0 the band straddles this rectangle, so its edge is the 50% cut contour and NOT the full-cut extent:
	 * the cut is full only over the inner CoverageXY/2 - Gradient Band/2. Size this to a road width and the
	 * nominal road edge gets a half-strength cut; set Gradient Offset to -Gradient Band / 2 to push the full
	 * cut back out to it.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
		meta = (DisplayName = "Coverage", ClampMin = "1.0", ForceUnits = "cm"))
	FVector2D CoverageXY = FVector2D(10000.0, 10000.0);

	/**
	 * Shifts the falloff band relative to the coverage edge, in component-local cm. The cut reaches zero
	 * where the distance outside the rectangle passes Gradient Band / 2 - Gradient Offset, so a POSITIVE
	 * offset shrinks the cut and the biome creeps into it, and a NEGATIVE offset grows the cut and the
	 * biome retreats further out.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
		meta = (DisplayName = "Gradient Offset", ForceUnits = "cm"))
	double GradientOffsetCm = 0.0;

	/** Width of the two-sided band the cut ramps across, in component-local cm; 0 is a hard edge. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
		meta = (DisplayName = "Gradient Band", ClampMin = "0.0", ForceUnits = "cm"))
	double GradientBandCm = 2000.0;

	/** Gather order, ascending. It orders nothing today: the fold takes the maximum cut. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
		meta = (DisplayName = "Mask Priority"))
	double MaskPriority = 100.0;

	/** Excluded from every gather and cuts nothing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mask",
		meta = (DisplayName = "Disabled"))
	bool bIsDisabled = false;

	/**
	 * The coverage rect widened in local XY by the extreme of the band over both offset signs, taken to
	 * world, then made unbounded on Z: the cut reads local XY only, so for an upright mask no Z arrangement
	 * can exclude it from a gather. A null or degenerate coverage yields an INVALID box, which keeps the
	 * mask out of every gather on geometry alone.
	 */
	virtual FBox ComputeContributionBounds() const;

	/**
	 * CutStrength over the coverage rect, ramping to 0 across the band. The distance outside the rect is a
	 * Chebyshev measure, so the contour keeps the rect's square corners and the field is only C0 along the
	 * corner diagonals where that maximum's branch flips.
	 *
	 * An override must return a finite value in [0, 1]: the fold drops anything else, because a NaN
	 * propagates through FMath::Max and every clamp downstream would read it as a full cut.
	 */
	virtual double EvaluateCut(const FVector& WorldPosition) const;

	virtual void OnRegister() override;
};

namespace UE::ArcEcotope
{
	/** Enabled masks reaching QueryWorldBounds, ascending by MaskPriority then object path. */
	ARCECOTOPE_API void GatherSortedMasks(
		const UWorld& World,
		const FBox& QueryWorldBounds,
		TArray<UArcEcotopeModificationMask*>& OutSorted);

	/**
	 * The deepest cut any gathered mask makes at this position. The fold takes the maximum rather than
	 * blending, so two overlapping cuts cannot weaken each other; the sorted order the gather produces is
	 * therefore not read here, and is carried for a later mode that needs one.
	 */
	ARCECOTOPE_API double EvaluateCutAt(
		const TArray<UArcEcotopeModificationMask*>& SortedMasks,
		const FVector& WorldPosition);
}
