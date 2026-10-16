// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "ArcFoliageToSkinnedMeshConverter.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UArcFoliageToSkinnedMeshSettings;

/**
 * Orchestration entry point for the foliage card -> skinned skeletal mesh converter (plan Step 5).
 *
 * Ties the pipeline together: FMeshDescription -> FDynamicMesh3, per-slot opacity-mask resolution +
 * contour trace (Arc::FoliageMask), UV-space card cut (Arc::FoliageCut), per-clump skeleton + skin
 * bind (Arc::FoliageSkeleton), then USkeleton + USkeletalMesh asset creation via
 * UE::AssetUtils::CreateSkeletalMeshAsset. No material changes are made — original slot assignments
 * are preserved verbatim.
 *
 * The single BlueprintCallable static is the headless/MCP verification surface as well as the
 * Content Browser menu entry's executor.
 */
UCLASS()
class ARCEDITORTOOLS_API UArcFoliageToSkinnedMeshConverter : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Convert @p StaticMesh (an alpha-masked foliage card) into a skinned USkeletalMesh whose geometry
	 * is cut to the opacity mask, with a few auto-placed clump bones.
	 *
	 * @param StaticMesh  Source foliage card static mesh (LOD0 mesh description is required).
	 * @param Settings    Conversion options; if null, the class-default settings object is used.
	 * @return The created USkeletalMesh, or nullptr on failure (a specific message is reported to the
	 *         "EditorErrors" message log for each failure stage).
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc|Foliage")
	static USkeletalMesh* ConvertStaticMeshToSkinnedFoliage(
		UStaticMesh* StaticMesh,
		UArcFoliageToSkinnedMeshSettings* Settings);
};
