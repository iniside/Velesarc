// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "ArcMassVisRenderingProps.generated.h"

USTRUCT(BlueprintType)
struct ARCMASS_API FArcMassVisRenderingProps
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastShadowAsTwoSided = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastHiddenShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastFarShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastInsetShadow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Shadows")
	bool bCastContactShadow = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bHiddenInGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bReceivesDecals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bNeverDistanceCull = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bRenderCustomDepth = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering",
	          meta = (EditCondition = "bRenderCustomDepth"))
	int64 CustomDepthStencilValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	int32 TranslucencySortPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bAffectDynamicIndirectLighting = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bAffectIndirectLightingWhileHidden = false;

	// Lighting channels this primitive is assigned to. Channel 0 is the default.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	FLightingChannels LightingChannels;

	// Whether this primitive is visible in reflection captures. Matches UPrimitiveComponent default (true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bVisibleInReflections = true;

	// Whether this primitive is visible in ray tracing effects. Matches UPrimitiveComponent default (true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bVisibleInRayTracing = true;

	// Whether this primitive is visible in reflection captures (lumen / screen-space). Matches UPrimitiveComponent default (true).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	bool bVisibleInReflectionCaptures = true;

	// Minimum draw distance for this primitive (world units). 0 = no minimum.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	float MinDrawDistance = 0.f;

	// Maximum draw distance for this primitive (world units). 0 = never cull (engine default). Overridden by bNeverDistanceCull.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering")
	float LDMaxDrawDistance = 0.f;

	// Controls which bits in the custom depth stencil buffer are written when bRenderCustomDepth is true.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering",
	          meta = (EditCondition = "bRenderCustomDepth"))
	ERendererStencilMask CustomDepthStencilWriteMask = ERendererStencilMask::ERSM_Default;
};
