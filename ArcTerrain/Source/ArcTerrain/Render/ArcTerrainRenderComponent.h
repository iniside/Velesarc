// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Render/ArcTerrainAtlasTexture.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainTileAtlas.h"

#include "ArcTerrainRenderComponent.generated.h"

/** One base-mesh face — pure TOPOLOGY — as the renderer needs it. */
struct FArcTerrainRenderFace
{
	/** Global texel index of the face's (0,0) corner, derived from the STATIC face id. Every texture
	 *  address a leaf of this face computes starts here; the apron offset belongs to the SECTOR and
	 *  enters after the sector key is resolved. */
	FIntPoint TexelOrigin = FIntPoint::ZeroValue;

	bool operator==(const FArcTerrainRenderFace& Other) const
	{
		return TexelOrigin == Other.TexelOrigin;
	}
};

/** Everything the terrain proxy draws from, resolved on the game thread. */
struct FArcTerrainRenderBindings
{
	FArcTerrainRenderIdentity Identity;

	/** Indexed by face id. */
	TArray<FArcTerrainRenderFace> Faces;

	TSharedPtr<FArcTerrainTileAtlasResources, ESPMode::ThreadSafe> AtlasResources;

	/** The two per-leaf slice lookups. Carried on this rail AND on the CBT base mesh from one snapshot. */
	FArcTerrainResidencyRings Rings;

	int32 SectorQuads = 0;
	int32 FaceQuads = 0;
	int32 SectorQuadsShift = 0;
	int32 WeightQuadsShift = 0;
	float TexelToLocal = 1.0f;

	/** Terrain-local bounds of the WHOLE face grid, with displacement headroom in Z. Static: the mesh
	 *  spans the authored world extent whatever is resident, and bounds drawn round the resident set
	 *  would take the entire terrain out of the frustum test the moment they left it. */
	FBox LocalBounds = FBox(ForceInit);

	/** False only while the atlas arrays are released. The vertex factory rejects every triangle on it;
	 *  it is the one global emptiness left now that a residency miss resolves to a fill slice. */
	bool bResourcesValid = false;

	/** arc.Terrain.Weights.VerboseComposite, read on the GAME thread; the render thread must not read a
	 *  cvar. Deliberately outside operator== so toggling it never republishes bindings. */
	bool bLogWeightVerbose = false;

	/** Layout.GetWeightFillSlice() these bindings were built against — the expected value of the
	 *  WeightFillSlice uniform. Diagnostics only, so likewise outside operator==. */
	int32 LayoutWeightFillSlice = INDEX_NONE;

	bool IsValid() const
	{
		return Identity.IsValid() && SectorQuads > 0 && FaceQuads > 0 && !Faces.IsEmpty() && AtlasResources.IsValid()
			&& LocalBounds.IsValid;
	}

	bool operator==(const FArcTerrainRenderBindings& Other) const
	{
		return Identity == Other.Identity
			&& SectorQuads == Other.SectorQuads
			&& FaceQuads == Other.FaceQuads
			&& bResourcesValid == Other.bResourcesValid
			&& SectorQuadsShift == Other.SectorQuadsShift
			&& WeightQuadsShift == Other.WeightQuadsShift
			&& TexelToLocal == Other.TexelToLocal
			&& AtlasResources == Other.AtlasResources
			&& LocalBounds == Other.LocalBounds
			&& Rings == Other.Rings
			&& Faces == Other.Faces;
	}
};

/**
 * The terrain's single render primitive. One component per AArcTerrain, sitting at the TERRAIN ORIGIN
 * with an identity relative transform: a sector's spatial offset enters the vertex through its face's
 * global texel origin, never through a component transform, which is what lets adjacent sectors weld
 * bit-identically.
 *
 * Transient, created and driven by the terrain subsystem's render-binding owner.
 */
UCLASS(NotBlueprintable, NotPlaceable, Transient, HideCategories = (Physics, Collision, Activation, Cooking, HLOD, Navigation))
class UArcTerrainRenderComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UArcTerrainRenderComponent();

	/**
	 * Binding rail, re-run whenever atlas residency changes: writes the component field FIRST (the
	 * source of truth a recreated proxy reads) and then pushes to the live proxy.
	 */
	void UpdateBindings(FArcTerrainRenderBindings&& InBindings);

	const FArcTerrainRenderBindings& GetBindings() const { return Bindings; }

	/** GC root for the atlas facades the material instance binds: a live material swap drops and
	 *  rebuilds the instance's parameter overrides, which would otherwise be their only reference. */
	void SetAtlasTextures(TArray<TObjectPtr<UArcTerrainAtlasTexture>>&& InAtlasTextures)
	{
		AtlasTextures = MoveTemp(InAtlasTextures);
	}

	//~ Begin UPrimitiveComponent interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* InMaterial) override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	virtual int32 GetNumMaterials() const override { return 1; }
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials) const override;
	//~ End UPrimitiveComponent interface

private:
	FArcTerrainRenderBindings Bindings;

	/** Slot-0 material (the terrain MID). GC-rooted here via the UPROPERTY. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Material;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UArcTerrainAtlasTexture>> AtlasTextures;
};
