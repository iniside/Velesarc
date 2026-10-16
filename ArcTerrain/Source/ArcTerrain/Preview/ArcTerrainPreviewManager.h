// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTerrainTypes.h"
#include "Render/ArcTerrainCBTRenderer.h"
#include "Render/ArcTerrainTileAtlas.h"

class AArcTerrain;
class FArcTerrainTileAtlas;
class FMaterialResource;
class FMaterialShaderMap;
class UArcTerrainAtlasTexture;
class UArcTerrainRenderComponent;
class UArcTerrainSubsystem;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/**
 * Owns the rendering side of a terrain: ONE transient UArcTerrainRenderComponent per AArcTerrain, one
 * material instance on it, and the UTexture facades that let that instance bind the tile atlas arrays.
 * Plain (non-UObject) class owned by value on UArcTerrainSubsystem.
 *
 * The component sits at the TERRAIN ORIGIN with an identity relative transform — a sector's offset
 * enters the vertex through its face's global texel origin, which is the bit-identical edge-weld
 * precondition.
 *
 * MID parameter contract (bound through ArcTerrainMaterialBindings for preview and lighting):
 *   texture  "ArcTerrainHeightAtlas"   = the height atlas array (PF_R32_FLOAT, terrain-local cm)
 *   textures "ArcTerrainWeights0..N"   = the weight atlas arrays (one per 4 global channels)
 *   scalar   "ArcTerrainHeightInvRes"  = 1 / height slice side
 *   scalar   "ArcTerrainWeightInvRes"  = 1 / weight slice side
 *   scalar   "ArcTerrainCellSize"      = Config.CellSize() (cm)
 * The SLICE indices are not here: they vary per cell inside one draw, so they ride vertex-factory
 * interpolants (UV2) instead.
 *
 * GC ROOTING (this class holds weak pointers only): the render component is owned by its AArcTerrain,
 * its Material UPROPERTY roots the MID, and the MID's texture parameter overrides root the atlas
 * texture facades.
 */
struct FArcTerrainRenderBindingSignature
{
	FArcTerrainTileAtlasLayout AtlasLayout;
	const void* AtlasResourceIdentity = nullptr;
	uint64 AtlasResourceGeneration = 0;
	TArray<FName> ChannelLayout;
	TWeakObjectPtr<UMaterialInterface> Material;
	const FMaterialResource* MaterialResourceIdentity = nullptr;
	const FMaterialShaderMap* MaterialShaderMapIdentity = nullptr;
	FGuid MaterialId;
	uint64 MaterialRevision = 0;
	float HeightInvRes = 0.0f;
	float WeightInvRes = 0.0f;
	float CellSizeCm = 0.0f;
	bool bDiagnosticBinding = false;

	bool operator==(const FArcTerrainRenderBindingSignature& Other) const
	{
		return AtlasLayout == Other.AtlasLayout
			&& AtlasResourceIdentity == Other.AtlasResourceIdentity
			&& AtlasResourceGeneration == Other.AtlasResourceGeneration
			&& ChannelLayout == Other.ChannelLayout
			&& Material == Other.Material
			&& MaterialResourceIdentity == Other.MaterialResourceIdentity
			&& MaterialShaderMapIdentity == Other.MaterialShaderMapIdentity
			&& MaterialId == Other.MaterialId
			&& MaterialRevision == Other.MaterialRevision
			&& HeightInvRes == Other.HeightInvRes
			&& WeightInvRes == Other.WeightInvRes
			&& CellSizeCm == Other.CellSizeCm
			&& bDiagnosticBinding == Other.bDiagnosticBinding;
	}

	bool operator!=(const FArcTerrainRenderBindingSignature& Other) const
	{
		return !(*this == Other);
	}
};

class FArcTerrainPreviewManager
{
public:
	/** Bind the owning subsystem (source of the shared fallback material root). */
	void Initialize(UArcTerrainSubsystem& InSubsystem);

	/**
	 * Ensure the terrain's render component, material instance and atlas bindings exist and match the
	 * atlas's current residency. Called once per tick per terrain, after atlas admission.
	 */
	bool EnsureTerrainRender(AArcTerrain& Terrain, const FArcTerrainConfig& Config,
		const FArcTerrainTileAtlas& Atlas, const FArcTerrainResidencyRings& Rings,
		const FArcTerrainRenderIdentity& Identity);

	bool HasAdmittedRender(const AArcTerrain& Terrain) const;

	UArcTerrainAtlasTexture* GetHeightAtlasFacade(AArcTerrain& Terrain) const;
	void InvalidateBindingSignature(AArcTerrain& Terrain);

	/** Destroy a terrain's render component (terrain unregister / dead-terrain reclaim). Accepts a weak
	 *  key so it works even after the actor has been GC'd. */
	void ReleaseTerrainRender(const TWeakObjectPtr<AArcTerrain>& Terrain);

	/** Live TerrainMaterial swap (editor): drop the cached resolution and rebuild the MID; the next
	 *  EnsureTerrainRender rebinds every parameter. */
	void RefreshTerrainMaterial(AArcTerrain& Terrain);

	/** Destroy every render component and drop the subsystem's shared material root. Called from
	 *  UArcTerrainSubsystem::Deinitialize BEFORE the tile atlases are released. */
	void Shutdown();

private:
	struct FTerrainRender
	{
		TWeakObjectPtr<UArcTerrainRenderComponent> Component;
		TWeakObjectPtr<UMaterialInstanceDynamic> MID;
		TWeakObjectPtr<UArcTerrainAtlasTexture> HeightAtlasTexture;
		TArray<TWeakObjectPtr<UArcTerrainAtlasTexture>> WeightAtlasTextures;
		FArcTerrainRenderBindingSignature BindingSignature;
		uint64 MaterialRevision = 0;
		bool bHasBindingSignature = false;

		/** True while the component is parked on invalid resources (see UnadmitTerrainRender). */
		bool bUnadmitted = false;
	};

	/** Park the live component until both render resources and material bindings are valid. */
	void UnadmitTerrainRender(AArcTerrain& Terrain);

	/** Resolve (cached) the base material for a terrain: its editor-assigned TerrainMaterial, or the
	 *  engine default surface material when it has none. */
	UMaterialInterface* GetTerrainMaterial(AArcTerrain& Terrain);
	UMaterialInterface* ResolveTerrainMaterial(AArcTerrain& Terrain);

	UArcTerrainSubsystem* Subsystem = nullptr;

	TMap<TWeakObjectPtr<AArcTerrain>, FTerrainRender> Renders;

	/** Per-terrain resolved base material (weak — the strong root is the actor's TerrainMaterial, or the
	 *  engine default's own root). Dropped per terrain on a live TerrainMaterial edit. */
	TMap<TWeakObjectPtr<AArcTerrain>, TWeakObjectPtr<UMaterialInterface>> ResolvedMaterials;
};
