// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Pure scalar definitions of the ArcTerrain layer-blend weight math — the SINGLE SOURCE OF TRUTH for
 * the numeric formulas that UMaterialExpressionArcTerrainLayerBlend::Compile() mirrors in
 * FMaterialCompiler node-calls (ArcTerrainMaterialExpressions.cpp, ArcTerrainEditor module).
 *
 * Correspondence to the compiler-call mirror in Compile():
 *   ArcTerrainHeightBlendWeight(Raw, H)  <->  Compiler->Clamp(
 *       Compiler->Add(Compiler->Lerp(Const(-1), Const(1), RawWeight), Height),
 *       Const(1e-4f), Const(1.f))
 *   ArcTerrainAlphaBlend(Under, Over, W) <->  Compiler->Lerp(Under, Over, Weight)
 *
 * Any change to these formulas MUST be mirrored in both this header and Compile() (and busts the
 * material shader-map DDC via ArcTerrainNodeVersion.ush).
 */
namespace ArcTerrainLayerBlend
{
	/**
	 * HeightBlend weight: remap the [0,1] raw channel weight into [-1,1], add the per-layer height
	 * offset, then clamp to (1e-4, 1]. Mirrors LandscapeLayerBlend's height-modified weight.
	 */
	inline float ArcTerrainHeightBlendWeight(float RawWeight, float Height)
	{
		return FMath::Clamp(FMath::Lerp(-1.0f, 1.0f, RawWeight) + Height, 1e-4f, 1.0f);
	}

	/**
	 * AlphaBlend fold: linear blend of the already-accumulated Under output toward this layer's Over
	 * input by Weight. AlphaBlend layers are excluded from the weight-sum normalization group and
	 * folded in after it (mirrors LandscapeLayerBlend's LB_AlphaBlend second pass).
	 */
	inline float ArcTerrainAlphaBlend(float Under, float Over, float Weight)
	{
		return FMath::Lerp(Under, Over, Weight);
	}
}
