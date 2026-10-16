// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Foliage/ArcEcotopeFoliageFieldPayload.h"
#include "RHIFwd.h"
#include "ShaderParameterMacros.h"
#include "Templates/SharedPointer.h"

class AArcEcotopeVolume;
class FRDGBuilder;

/**
 * Held by shared reference so the scatter's render-thread delegate can capture the field itself rather
 * than a patch pointer, and so the RHI texture outlives a patch destroyed the same frame.
 */
struct FArcEcotopeFoliageField
{
	/** Written by the upload command, so it is only readable on the render thread. */
	FTextureRHIRef TextureRHI;

	FIntPoint Dimensions = FIntPoint::ZeroValue;

	/** Volume actor transform, LWC-split exactly as the terrain atlas splits the terrain's. */
	FMatrix44f WorldToVolumeLocal = FMatrix44f::Identity;
	FVector3f LWCTile = FVector3f::ZeroVector;

	/** Volume-local cm of the raster's minimum corner, and the reciprocal of the raster's own extent. */
	FVector2f OriginLocal = FVector2f::ZeroVector;
	FVector2f InvSizeLocal = FVector2f::ZeroVector;

	/** Feeds the patch signature; compared for inequality only. */
	uint32 Revision = 0;
};

using FArcEcotopeFoliageFieldRef = TSharedPtr<FArcEcotopeFoliageField, ESPMode::ThreadSafe>;

/** Loose HLSL globals of Shaders/Private/ArcEcotopeFoliageField.ush, 1:1 by name. */
BEGIN_SHADER_PARAMETER_STRUCT(FArcEcotopeFoliageFieldParameters, ARCECOTOPE_API)
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, ArcEcotopeFieldTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, ArcEcotopeFieldSampler)
	SHADER_PARAMETER(uint32, ArcEcotopeFieldValid)
	SHADER_PARAMETER(FMatrix44f, ArcEcotopeFieldWorldToLocal)
	SHADER_PARAMETER(FVector3f, ArcEcotopeFieldLWCTile)
	SHADER_PARAMETER(FVector2f, ArcEcotopeFieldOriginLocal)
	SHADER_PARAMETER(FVector2f, ArcEcotopeFieldInvSizeLocal)
END_SHADER_PARAMETER_STRUCT()

namespace ArcEcotopeFoliageFields
{

#if WITH_EDITOR
/**
 * GAME THREAD, EDITOR ONLY. Derives the volume's field from its baked site and committed snapshot,
 * through the DDC: a hit costs a fetch, a miss rasterizes and stores. Called when a snapshot is
 * committed, never from a tick. False with OutError set when the volume carries no valid snapshot or
 * no usable baked site.
 */
ARCECOTOPE_API bool DeriveFieldPayload(
	AArcEcotopeVolume& Volume,
	FArcEcotopeFoliageFieldPayload& OutPayload,
	FText& OutError);
#endif

/**
 * GAME THREAD. Uploads a stored payload and returns the field bound by the scatter. Null for a payload
 * that is empty or does not describe itself consistently.
 *
 * Revision is supplied by the caller and only has to differ from the previous one.
 */
ARCECOTOPE_API FArcEcotopeFoliageFieldRef MakeFieldFromPayload(
	const FArcEcotopeFoliageFieldPayload& Payload,
	const FTransform& VolumeTransform,
	uint32 Revision);

/**
 * GAME THREAD. The same texels under a new volume transform: the texels are volume-local, so a moved
 * volume only re-splits its matrices. The source's texture is adopted on the render thread, which is
 * the only thread allowed to touch it. Null for a null source.
 */
ARCECOTOPE_API FArcEcotopeFoliageFieldRef MakeFieldWithTransform(
	const FArcEcotopeFoliageFieldRef& Source,
	const FTransform& VolumeTransform,
	uint32 Revision);

/** RENDER THREAD. A null or not-yet-uploaded field binds a system texture and clears ArcEcotopeFieldValid. */
ARCECOTOPE_API void BindField(FRDGBuilder& GraphBuilder, const FArcEcotopeFoliageFieldRef& Field,
	FArcEcotopeFoliageFieldParameters& OutParameters);

/** Drops the game thread's reference on the render thread, as FArcEcotopeFoliagePrimitive::Release does. */
ARCECOTOPE_API void ReleaseField(FArcEcotopeFoliageFieldRef&& Field);

} // namespace ArcEcotopeFoliageFields
