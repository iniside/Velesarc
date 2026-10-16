// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Spatial/SampledScalarField2.h"
#include "TerrainTool/ArcTerrainErosion.h"
#include "TerrainTool/Graph/ArcHeightmapGraph.h"

#include "ArcHeightmapNodes.generated.h"

/** Mirrors UE::Geometry::EFBMMode so the FBM mode is authorable on noise nodes. */
UENUM()
enum class EArcHeightmapFBMMode : uint8
{
	Standard,
	Turbulent,
	Ridge
};

/** Mirrors ArcTerrainErosion::EArcErosionMode so the erosion solver mode is authorable. */
UENUM()
enum class EArcHeightmapErosionMode : uint8
{
	GridStreamPower,
	Droplet
};

/** Per-cell binary operation for FArcHeightmapNode_Math. */
UENUM()
enum class EArcHeightmapMathOp : uint8
{
	Add,
	Subtract,
	Multiply,
	Divide,
	Min,
	Max,
	Power
};

/** Distance metric for FArcHeightmapNode_Voronoi cellular noise. */
UENUM()
enum class EArcHeightmapDistanceMetric : uint8
{
	Euclidean,
	Manhattan,
	Chebyshev,
	Hybrid
};

/** Output channel for FArcHeightmapNode_Voronoi cellular noise. */
UENUM()
enum class EArcHeightmapVoronoiOutput : uint8
{
	F1,
	F2,
	F2MinusF1,
	CellValue
};

/**
 * Base node for the bespoke heightmap graph.
 *
 * A USTRUCT with plain C++ virtual methods (legal + idiomatic here — mirrors FArcTargetingNode).
 * Subtypes override Evaluate(), consuming already-resolved upstream outputs (ordered to match
 * InputNodeIds) and writing a single FSampledScalarField2f output. No UFUNCTIONs.
 */
USTRUCT(meta = (ExcludeBaseStruct))
struct ARCEDITORTOOLS_API FArcHeightmapNode
{
	GENERATED_BODY()

	virtual ~FArcHeightmapNode() = default;

	UPROPERTY(EditAnywhere, Category = "Node")
	FGuid NodeId;

	UPROPERTY(EditAnywhere, Category = "Node")
	TArray<FGuid> InputNodeIds;

	/** Number of input pins / expected upstream inputs. */
	virtual int32 GetInputCount() const { return 0; }

	// Inputs are already resolved upstream outputs, ordered to match InputNodeIds.
	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const
	{
	}
};

/**
 * Source node: fractal-Brownian-motion noise terrain (0 inputs).
 *
 * Normalized FBM with redistribution, scaled by Amplitude. Produces absolute height in cm.
 */
// DEPRECATED: hidden from the add-menu (ExcludeBaseStruct); retained so existing assets still load & evaluate. Use the split node types instead.
USTRUCT(meta = (ExcludeBaseStruct))
struct ARCEDITORTOOLS_API FArcHeightmapNode_Noise : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Noise")
	EArcHeightmapFBMMode Mode = EArcHeightmapFBMMode::Standard;

	UPROPERTY(EditAnywhere, Category = "Noise")
	int32 Octaves = 5;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Lacunarity = 2.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Gain = 0.5;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Smoothness = 0.02;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Gamma = 2.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double RedistributionExponent = 3.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double RedistributionFudge = 1.2;

	UPROPERTY(EditAnywhere, Category = "Noise")
	FVector2D Frequency = FVector2D(0.0005, 0.0005);

	UPROPERTY(EditAnywhere, Category = "Noise")
	FVector2D Translate = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Rotation = 0.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Amplitude = 1500.0;

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Shared FBM noise base; mode supplied by subtype.
 *
 * Holds all noise parameters except Mode. Subclasses override GetFBMMode() to select
 * the FBM variant; Evaluate() is implemented once here using GetFBMMode().
 */
USTRUCT(meta = (ExcludeBaseStruct))
struct ARCEDITORTOOLS_API FArcHeightmapNode_NoiseBase : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Noise")
	int32 Octaves = 5;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Lacunarity = 2.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Gain = 0.5;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Smoothness = 0.02;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Gamma = 2.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double RedistributionExponent = 3.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double RedistributionFudge = 1.2;

	UPROPERTY(EditAnywhere, Category = "Noise")
	FVector2D Frequency = FVector2D(0.0005, 0.0005);

	UPROPERTY(EditAnywhere, Category = "Noise")
	FVector2D Translate = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Rotation = 0.0;

	UPROPERTY(EditAnywhere, Category = "Noise")
	double Amplitude = 1500.0;

	virtual EArcHeightmapFBMMode GetFBMMode() const { return EArcHeightmapFBMMode::Standard; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/** Standard (smooth) FBM noise terrain node. */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_NoiseStandard : public FArcHeightmapNode_NoiseBase
{
	GENERATED_BODY()

	virtual EArcHeightmapFBMMode GetFBMMode() const override { return EArcHeightmapFBMMode::Standard; }
};

/** Turbulent (abs-value folded) FBM noise terrain node. */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_NoiseTurbulent : public FArcHeightmapNode_NoiseBase
{
	GENERATED_BODY()

	virtual EArcHeightmapFBMMode GetFBMMode() const override { return EArcHeightmapFBMMode::Turbulent; }
};

/** Ridge (inverted turbulent) FBM noise terrain node. */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_NoiseRidge : public FArcHeightmapNode_NoiseBase
{
	GENERATED_BODY()

	virtual EArcHeightmapFBMMode GetFBMMode() const override { return EArcHeightmapFBMMode::Ridge; }
};

/**
 * Hydraulic + thermal erosion (1 input).
 *
 * Feeds the upstream output into ArcTerrainErosion::SimulateCached as the absolute InitialHeight,
 * with GridRes / Coverage taken from the eval context (SkirtSize = 0 so the grid lines up), then
 * copies the eroded result back out. Noise fields are deliberately NOT exposed — the synthesized
 * fallback is never used here.
 */
// DEPRECATED: hidden from the add-menu (ExcludeBaseStruct); retained so existing assets still load & evaluate. Use the split node types instead.
USTRUCT(meta = (ExcludeBaseStruct))
struct ARCEDITORTOOLS_API FArcHeightmapNode_Erosion : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Erosion")
	EArcHeightmapErosionMode Mode = EArcHeightmapErosionMode::GridStreamPower;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	int32 OuterIterations = 40;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	int32 ThermalSubsteps = 1;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double RainAmount = 1.0;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double DepositRate = 0.3;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double SedimentCapacity = 1.0;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double TalusAngleDeg = 33.0;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double ThermalRate = 0.5;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double SedimentSmoothingRadiusCm = 0.0;

	// --- Stream-power (GridStreamPower mode) ---
	UPROPERTY(EditAnywhere, Category = "Erosion|StreamPower")
	double StreamPowerK = 0.05;

	UPROPERTY(EditAnywhere, Category = "Erosion|StreamPower")
	double StreamPowerM = 0.5;

	UPROPERTY(EditAnywhere, Category = "Erosion|StreamPower")
	double StreamPowerN = 1.0;

	UPROPERTY(EditAnywhere, Category = "Erosion|StreamPower")
	double MaxErodePerStepCm = 50.0;

	UPROPERTY(EditAnywhere, Category = "Erosion|StreamPower")
	double StreamPowerDepositRate = 0.3;

	UPROPERTY(EditAnywhere, Category = "Erosion|StreamPower")
	double ChannelThreshold = 0.03;

	// --- Droplet (Droplet mode) ---
	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	int32 NumDroplets = 40000;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	int32 DropletLifetime = 64;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletInertia = 0.05;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletCapacity = 4.0;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletErodeRate = 0.15;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletDepositRate = 0.3;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletEvaporation = 0.02;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletGravity = 4.0;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	int32 DropletErosionRadius = 3;

	UPROPERTY(EditAnywhere, Category = "Erosion|Droplet")
	double DropletMinSlope = 0.01;

	// --- Shared scale-invariance + channel depth ---
	UPROPERTY(EditAnywhere, Category = "Erosion")
	double ChannelDepthFraction = 0.25;

	UPROPERTY(EditAnywhere, Category = "Erosion")
	double FeatureSizeCm = 120.0;

	virtual int32 GetInputCount() const override { return 1; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/** Full-control erosion (all parameters) — inherits every field and Evaluate from FArcHeightmapNode_Erosion. */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_ErosionAdvanced : public FArcHeightmapNode_Erosion
{
	GENERATED_BODY()
};

/**
 * Simple flow (hydraulic stream-power) erosion — few parameters.
 *
 * Runs the GridStreamPower solver with a small, approachable parameter set; all other FParams fields
 * stay at their sane defaults. Suitable for most terrain without needing to tune the full erosion rig.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_ErosionFlow : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Flow Erosion", meta = (UIMin = "5", UIMax = "80", ClampMin = "1"))
	int32 OuterIterations = 40;

	UPROPERTY(EditAnywhere, Category = "Flow Erosion", meta = (UIMin = "0.01", UIMax = "0.15", ClampMin = "0.0"))
	double ChannelThreshold = 0.03;

	UPROPERTY(EditAnywhere, Category = "Flow Erosion", meta = (UIMin = "0.1", UIMax = "0.5", ClampMin = "0.0"))
	double ChannelDepthFraction = 0.25;

	UPROPERTY(EditAnywhere, Category = "Flow Erosion", meta = (UIMin = "0.3", UIMax = "1.5"))
	double StreamPowerM = 0.5;

	UPROPERTY(EditAnywhere, Category = "Flow Erosion", meta = (UIMin = "0.2", UIMax = "3.0", ClampMin = "0.0"))
	double RainAmount = 1.0;

	virtual int32 GetInputCount() const override { return 1; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Per-cell binary math (2 inputs): Out = Lerp(A, op(A, B), Strength).
 *
 * Missing inputs are treated as zero fields. Divide guards B == 0 (returns A); Power clamps A >= 0.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Math : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Math")
	EArcHeightmapMathOp Op = EArcHeightmapMathOp::Add;

	UPROPERTY(EditAnywhere, Category = "Math")
	double Strength = 1.0;

	virtual int32 GetInputCount() const override { return 2; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/** Constant field (0 inputs): every cell = Value. */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Constant : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Constant")
	double Value = 0.0;

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Graph result marker (1 input): identity copy of its input.
 *
 * Step 3 locates the final result by finding the node of this struct type.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Output : public FArcHeightmapNode
{
	GENERATED_BODY()

	/** Output identity. Empty = the heightfield (exactly one required). Non-empty + unique = a weight-map layer. */
	UPROPERTY(EditAnywhere, Category = "Output")
	FName LayerName;

	virtual int32 GetInputCount() const override { return 1; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Per-cell addition (2 inputs): Out = A + B.
 *
 * Missing inputs are treated as zero. Designed to combine two height fields
 * or blend an offset onto a base terrain layer.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Add : public FArcHeightmapNode
{
	GENERATED_BODY()

	virtual int32 GetInputCount() const override { return 2; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Per-cell multiplication (2 inputs): Out = A * B.
 *
 * Missing inputs are treated as 1.0 (identity) so a single connected field
 * passes through unchanged. Useful for scaling a height field by a mask.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Multiply : public FArcHeightmapNode
{
	GENERATED_BODY()

	virtual int32 GetInputCount() const override { return 2; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Per-cell linear interpolation (3 inputs — A, B, Alpha field): Out = Lerp(A, B, Alpha).
 *
 * The third input (Alpha) is optional; when disconnected the scalar Alpha UPROPERTY is used
 * instead. A and B default to zero when missing. Clamps the alpha to [0, 1].
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Lerp : public FArcHeightmapNode
{
	GENERATED_BODY()

	/** Fallback scalar alpha used when the third input pin is disconnected. */
	UPROPERTY(EditAnywhere, Category = "Lerp", meta = (ClampMin = "0", ClampMax = "1"))
	double Alpha = 0.5;

	virtual int32 GetInputCount() const override { return 3; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Per-cell levels adjustment (1 input): remaps input range [InBlack, InWhite] to [OutBlack, OutWhite]
 * with optional gamma correction.
 *
 * Equivalent to a Photoshop Levels adjustment on the height field. Missing input treated as zero.
 * All height values in cm.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Levels : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Levels", meta = (ForceUnits = "cm"))
	double InBlack = 0.0;

	UPROPERTY(EditAnywhere, Category = "Levels", meta = (ForceUnits = "cm"))
	double InWhite = 1500.0;

	UPROPERTY(EditAnywhere, Category = "Levels", meta = (ForceUnits = "cm"))
	double OutBlack = 0.0;

	UPROPERTY(EditAnywhere, Category = "Levels", meta = (ForceUnits = "cm"))
	double OutWhite = 1500.0;

	/** Gamma correction applied to the normalized t value; 1.0 = linear. */
	UPROPERTY(EditAnywhere, Category = "Levels", meta = (ClampMin = "0.01", UIMax = "8"))
	double Gamma = 1.0;

	virtual int32 GetInputCount() const override { return 1; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Circular falloff mask (0 inputs): emits a dimensionless [0, 1] field.
 *
 * Intended to feed Multiply or Lerp nodes, NOT as a direct height source.
 * Values are 1.0 inside the circle and fall off to 0.0 over FalloffBand cm.
 * Sharpness raises the falloff curve to a power for a harder or softer edge.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Circle : public FArcHeightmapNode
{
	GENERATED_BODY()

	/** Center of the circle in component-local cm. */
	UPROPERTY(EditAnywhere, Category = "Circle", meta = (ForceUnits = "cm"))
	FVector2D Center = FVector2D::ZeroVector;

	/** Radius of the full-value region in cm. */
	UPROPERTY(EditAnywhere, Category = "Circle", meta = (ForceUnits = "cm", ClampMin = "0"))
	double Radius = 5000.0;

	/** Width of the smooth falloff band in cm; 0 = hard edge. */
	UPROPERTY(EditAnywhere, Category = "Circle", meta = (ForceUnits = "cm", ClampMin = "0"))
	double FalloffBand = 1000.0;

	/** Power applied to the falloff curve; >1 sharpens, <1 softens. */
	UPROPERTY(EditAnywhere, Category = "Circle", meta = (ClampMin = "0.1", UIMax = "16"))
	double Sharpness = 1.0;

	/** When true the mask is inverted (0.0 inside, 1.0 outside). */
	UPROPERTY(EditAnywhere, Category = "Circle")
	bool bInvert = false;

	virtual int32 GetInputCount() const override { return 0; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Source node: Voronoi (cellular) noise terrain (0 inputs).
 *
 * Hand-written cellular noise — a 3x3 neighbour search over hash-jittered feature points produces
 * the two nearest distances (F1, F2) under a selectable metric. The chosen Output channel (F1, F2,
 * F2-F1, or the closest cell's hashed value) is scaled by Amplitude to produce absolute height in cm.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Voronoi : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	FVector2D Frequency = FVector2D(0.0005, 0.0005);

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	FVector2D Translate = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	double Rotation = 0.0;

	UPROPERTY(EditAnywhere, Category = "Voronoi", meta = (ClampMin = "0", ClampMax = "0.5"))
	double JitterAmount = 0.45;

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	int32 Seed = 1337;

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	EArcHeightmapDistanceMetric Metric = EArcHeightmapDistanceMetric::Euclidean;

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	EArcHeightmapVoronoiOutput Output = EArcHeightmapVoronoiOutput::F1;

	UPROPERTY(EditAnywhere, Category = "Voronoi")
	double Amplitude = 1500.0;

	virtual int32 GetInputCount() const override { return 0; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Source node: hand-written 2D Simplex (fractal-Brownian-motion) noise terrain (0 inputs).
 *
 * Inherits all 11 NoiseBase parameters and carries its OWN Standard-mode FBM loop over a classic
 * Gustavson 2D simplex basis (skew/unskew + 8-gradient table, x70 normalized to ~[-1,1]) instead of
 * the engine Perlin FractalBrownianMotionNoise. The post-FBM normalization + redistribution math
 * mirrors EvalHeight exactly; Smoothness/Gamma only affect Turbulent/Ridge folds (skipped in Standard
 * mode), so they are unused here. The existing Perlin nodes are untouched.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_Simplex : public FArcHeightmapNode_NoiseBase
{
	GENERATED_BODY()

	/** Seed for the Fisher-Yates permutation table (Knuth LCG). */
	UPROPERTY(EditAnywhere, Category = "Noise")
	int32 Seed = 1337;

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};

/**
 * Domain warp (1 input): warps the upstream rasterized field by an internal Perlin-fBM displacement,
 * then bilinear-resamples the input at the warped position.
 *
 * Model B — bilinear resample of the rasterized input; clamped sampling = edge repetition at the
 * border. The displacement is computed in component-local cm: per cell, an fBM displacement vector
 * (cm) is accumulated over Iterations recursive steps and added to the cell's sampling position
 * before BilinearSampleClamped re-reads the input field.
 */
USTRUCT()
struct ARCEDITORTOOLS_API FArcHeightmapNode_DomainWarp : public FArcHeightmapNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Domain Warp", meta = (ForceUnits = "cm"))
	double WarpAmplitude = 2000.0;

	UPROPERTY(EditAnywhere, Category = "Domain Warp")
	FVector2D WarpFrequency = FVector2D(0.0003, 0.0003);

	UPROPERTY(EditAnywhere, Category = "Domain Warp", meta = (ClampMin = "1"))
	int32 WarpOctaves = 3;

	UPROPERTY(EditAnywhere, Category = "Domain Warp", meta = (ClampMin = "1", ClampMax = "3"))
	int32 Iterations = 1;

	UPROPERTY(EditAnywhere, Category = "Domain Warp")
	int32 Seed = 1337;

	virtual int32 GetInputCount() const override { return 1; }

	virtual void Evaluate(const TArray<const UE::Geometry::FSampledScalarField2f*>& Inputs,
		const FArcHeightmapEvalContext& Ctx,
		UE::Geometry::FSampledScalarField2f& Out) const override;
};
