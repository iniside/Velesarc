// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Spatial/SampledScalarField2.h"
#include "IO/IoHash.h"

/**
 * Self-contained 2D hydraulic + thermal erosion simulation over a regular grid.
 *
 * Pure C++ (no UObject). The base height field is filled from normalized FBM noise, then
 * eroded by an interleaved gather-form hydraulic + thermal solver. Outputs are
 * FSampledScalarField2f grids carrying GridOrigin/CellDimensions in component-local cm so a
 * caller can BilinearSampleClamped(FVector2f(localX, localY)) directly.
 */
namespace ArcTerrainErosion
{
	enum class EArcErosionMode : uint8 { GridStreamPower, Droplet };

	struct FParams
	{
		EArcErosionMode Mode = EArcErosionMode::GridStreamPower;

		// --- Grid (includes skirt cells) ---
		int32 GridResX = 129;
		int32 GridResY = 129;
		double CoverageX = 10000.0;   // component-local footprint extent (cm), full size
		double CoverageY = 10000.0;   // component-local footprint extent (cm), full size
		double SkirtSize = 1000.0;    // cm padding each side

		// --- Noise ---
		FVector2D NoiseFrequency{ 0.0005, 0.0005 };
		FVector2D NoiseTranslate{ 0.0, 0.0 };
		double NoiseRotation = 0.0;
		int Octaves = 5;
		double Lacunarity = 2.0;
		double Gain = 0.5;
		double Smoothness = 0.02;
		double Gamma = 2.0;
		double RedistributionExponent = 3.0;
		double RedistributionFudge = 1.2;
		double Height = 1500.0;       // vertical scale (cm)

		// --- Erosion ---
		int32 OuterIterations = 40;
		int32 ThermalSubsteps = 1;    // K_t
		double RainAmount = 1.0;
		double DepositRate = 0.3;
		double SedimentCapacity = 1.0;
		double TalusAngleDeg = 33.0;  // thermal repose angle
		double ThermalRate = 0.5;
		int32 Seed = 1337;            // deterministic rain distribution
		double SedimentSmoothingRadiusCm = 0.0; // 0 = skip HeatEquation pass

		// --- Stream-power (GridStreamPower mode) ---
		double StreamPowerK = 0.05;
		double StreamPowerM = 0.5;
		double StreamPowerN = 1.0;
		// Per-cell incision cap in cm AT THE REFERENCE FOOTPRINT (RefCellCm ~ 78 cm, the 100 m / 129-grid case).
		// The internal cap is scaled by (CellSizeCm / RefCellCm) so a fixed cm value erodes proportionally across
		// patch sizes (a coarser grid steps further per cell, so it may carve proportionally more per step).
		// Name kept stable (tests reference it); semantics are now "reference-footprint cap".
		double MaxErodePerStepCm = 50.0;
		double StreamPowerDepositRate = 0.3;
		// Channel-carving threshold as a FRACTION of the peak D8 drainage on the grid [0.0, 1.0]. A cell is on the
		// dendritic channel network (and gets carved) iff its D8 drainage >= ChannelThreshold * MaxD8. Lower =>
		// denser network (more tributaries carve); higher => only the trunk valleys carve. Drives the explicit
		// threshold-based incision that replaced K*A^m*S^n (which never concentrated). StreamPowerM is now reused
		// as the carve drainage exponent; StreamPowerK / StreamPowerN are retained for hash/compat only.
		double ChannelThreshold = 0.03;

		// --- Droplet (Droplet mode) ---
		int32 NumDroplets = 40000;
		int32 DropletLifetime = 64;
		double DropletInertia = 0.05;
		double DropletCapacity = 4.0;
		double DropletErodeRate = 0.15;
		double DropletDepositRate = 0.3;
		double DropletEvaporation = 0.02;
		double DropletGravity = 4.0;
		// SUPERSEDED by FeatureSizeCm: the droplet brush radius is now derived from FeatureSizeCm / CellSizeCm so the
		// carved feature size stays constant in world-cm across patch sizes. Field kept (load-bearing-adjacent), but
		// the simulation no longer reads it for the brush radius.
		int32 DropletErosionRadius = 3;
		double DropletMinSlope = 0.01;

		// --- Optional caller-provided input heightfield ---
		// When set AND its resolution matches GridResX x GridResY, Simulate uses these absolute heights (cm)
		// as the starting grid instead of synthesizing from internal FBM noise. Params.Height is NOT applied
		// (the input is already in absolute cm). On resolution mismatch, falls back to noise synthesis with
		// a warning. MUST remain a plain C++ struct — do NOT add USTRUCT/UPROPERTY here.
		TSharedPtr<const UE::Geometry::FSampledScalarField2f> InitialHeight;

		// --- Scale-invariance + channel depth (shared) ---
		// Fraction of terrain Height usable as the per-cell carve floor / deposit ceiling. Replaces the old
		// hard-coded 0.25. Clamped to [0.1, 0.7]. Deeper => channels may cut/fill further before clamping.
		double ChannelDepthFraction = 0.25;
		// Carved-feature size in world-cm. The droplet brush radius is max(1, round(FeatureSizeCm / CellSizeCm)),
		// so the feature footprint stays constant across patch sizes. Default 120 cm gives radius ~2 at the ~100 m
		// reference footprint (Coverage 10000 cm / 129 grid => CellSizeCm ~ 78 cm: 120/78 = 1.5 -> 2) and radius ~3
		// on the 65^2 / 37.5 cm test grid (120/37.5 = 3.2 -> 3). The old 234 over-smeared droplet erosion (radius 6
		// on the test grid), so the default was lowered to keep channels crisp.
		double FeatureSizeCm = 120.0;
	};

	struct FResult
	{
		UE::Geometry::FSampledScalarField2f Height;           // final eroded height (EvalHeight output units * Height)
		UE::Geometry::FSampledScalarField2f Sediment;
		UE::Geometry::FSampledScalarField2f Flow;             // MFD drainage (display: Flow/Wetness channel)
		UE::Geometry::FSampledScalarField2f IncisionDrainage; // D8 single-flow drainage used for stream-power incision (GridStreamPower only; empty in Droplet mode)
		UE::Geometry::FSampledScalarField2f Curvature;
		UE::Geometry::FSampledScalarField2f Wetness;
	};

	/** Run the full interleaved simulation and return the eroded + derived grids. */
	ARCEDITORTOOLS_API FResult Simulate(const FParams& Params);

	/**
	 * DDC version GUID for the cached erosion result. Bump on serialization-format or algorithm change
	 * to invalidate all previously-cached results.
	 */
	inline const TCHAR* ArcErosionDDCVersion = TEXT("B2E94F17C03A5D8E1F6B7A2C490D3E51");

	/** Deterministic Blake3 hash of all output-affecting FParams fields (field-by-field, no struct memcpy). */
	ARCEDITORTOOLS_API FIoHash HashParams(const FParams& Params);

	/**
	 * Process-wide cached + DDC-backed Simulate. Hashes params, checks an in-memory cache, then the DDC,
	 * and finally runs Simulate on a miss (persisting the result to the DDC). Thread-safe across the
	 * concurrent per-tile ApplyModifications callers — one cold sim per unique param-set.
	 */
	ARCEDITORTOOLS_API TSharedPtr<const FResult> SimulateCached(const FParams& Params);
}
