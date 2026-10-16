// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "ArcFoliageToStaticMeshConverter.generated.h"

class UStaticMesh;
class UArcFoliageToStaticMeshSettings;

/**
 * Orchestration entry point for the foliage card -> Nanite static mesh converter (plan Step 4).
 *
 * Shares the source -> cut half with the skinned converter (Arc::FoliageCut::BuildCutMeshFromStaticMesh):
 * FMeshDescription -> FDynamicMesh3, per-slot opacity-mask resolution + contour trace, UV-space card cut.
 * The cut geometry is then emitted as a Nanite-enabled UStaticMesh via
 * UE::AssetUtils::CreateStaticMeshAsset — no skeleton, no bones, no wind. Original slot material
 * assignments are preserved verbatim.
 *
 * The single BlueprintCallable static is the headless/MCP verification surface as well as the
 * Content Browser menu entry's executor.
 */
UCLASS()
class ARCEDITORTOOLS_API UArcFoliageToStaticMeshConverter : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Convert @p StaticMesh (an alpha-masked foliage card) into a Nanite-enabled UStaticMesh whose
	 * geometry is cut to the opacity mask so the material can render opaque instead of masked.
	 *
	 * @param StaticMesh  Source foliage card static mesh (LOD0 mesh description is required).
	 * @param Settings    Conversion options; if null, the class-default settings object is used.
	 * @return The created UStaticMesh, or nullptr on failure (a specific message is reported to the
	 *         "EditorErrors" message log for each failure stage).
	 */
	UFUNCTION(BlueprintCallable, Category = "Arc|Foliage")
	static UStaticMesh* ConvertStaticMeshToNaniteStaticMesh(
		UStaticMesh* StaticMesh,
		UArcFoliageToStaticMeshSettings* Settings);
};
