// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcPrimitiveRenderStateHelper.h"
#include "Templates/Function.h"

struct FArcEditorVisualizationMeshFragment;
struct FArcVisualizationMeshFragment;
class UMaterialInterface;
struct FMaterialRelevance;
struct FArcOverrideMaterialsFragment;

/**
 * Helper to handle all the communication to the renderer from Mass for all type of meshes
 */
struct ARCMASSRENDER_API FArcMeshRenderStateHelper : public FArcPrimitiveRenderStateHelper
{
public:
	using Super = FArcPrimitiveRenderStateHelper;

	FArcMeshRenderStateHelper(FMassEntityHandle InEntityHandle, TNotNull<FMassEntityManager*> InEntityManager, const FArcRenderPrimitiveFragment& RenderPrimitiveFragment, const FArcOverrideMaterialsFragment* OverrideMaterialsFragment);
	virtual ~FArcMeshRenderStateHelper() override = default;

	FMaterialRelevance GetMaterialRelevance(EShaderPlatform InShaderPlatform) const;

	void GetMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& OutMaterialSlotOverlayMaterials) const;

	//Get the asset material slot overlay material
	virtual void GetDefaultMaterialSlotsOverlayMaterial(TArray<TObjectPtr<UMaterialInterface>>& AssetMaterialSlotOverlayMaterials) const = 0;

	virtual const TArray<TObjectPtr<UMaterialInterface>>& GetComponentMaterialSlotsOverlayMaterial() const = 0;
	virtual UMaterialInterface* GetOverlayMaterial() const = 0;

	const FArcOverrideMaterialsFragment* GetOverrideMaterialsFragment() const;
	const FArcVisualizationMeshFragment& GetMeshFragment() const;
#if WITH_EDITOR
	const FArcEditorVisualizationMeshFragment* GetEditorMeshFragment() const;
#endif // WITH_EDITOR

	TConstArrayView<TObjectPtr<UMaterialInterface>> GetOverrideMaterials() const;

protected:
	/**
	 * Returns an OverrideMaterials view where every slot whose effective material is not render-ready
	 * (IsComplete()==false) is replaced by the default surface material, so the proxy/Nanite audit only
	 * ever binds complete shader maps (crash-safe) while staying visible (grey) until the real material
	 * finishes compiling. Complete slots and genuine null overrides are preserved verbatim.
	 * The returned view points into the RenderSafeOverrideMaterials member, valid until the next call.
	 */
	TConstArrayView<TObjectPtr<UMaterialInterface>> BuildRenderSafeOverrideMaterials(int32 NumSlots, TFunctionRef<UMaterialInterface*(int32)> EffectiveMaterialGetter);

	TArray<TObjectPtr<UMaterialInterface>> RenderSafeOverrideMaterials;
};
