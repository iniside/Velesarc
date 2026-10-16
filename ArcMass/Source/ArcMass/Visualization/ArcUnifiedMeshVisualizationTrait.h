// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassVisRenderingProps.h"
#include "ArcUnifiedMeshVisualizationTrait.generated.h"

class UStaticMesh;
class USkinnedAsset;
class UMaterialInterface;
class UTransformProviderData;

/** Which fragment set UArcUnifiedMeshVisualizationTrait emits, inferred from authored entries. */
UENUM()
enum class EArcUnifiedMeshVisMode : uint8
{
	/** No authored entry — no visualization fragments are emitted. */
	None,
	/** Exactly one static entry — emits the single static-ISM fragment set (FArcVisEntityTag path). */
	Static,
	/** Exactly one skinned entry — emits the single skinned fragment set (FArcVisEntityTag path). */
	Skinned,
	/** Two or more entries, or a static/skinned mix — emits the composite fragment set. */
	Composite,
};

/** One static sub-mesh. Rendering settings are shared trait-wide (RenderingProps). */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcUnifiedStaticMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh", meta = (AssetBundles = "Client"))
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;
};

/** One skinned sub-mesh. The TransformProvider is shared trait-wide. */
USTRUCT(BlueprintType)
struct ARCMASS_API FArcUnifiedSkinnedMeshEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh", meta = (AssetBundles = "Client"))
	TSoftObjectPtr<USkinnedAsset> SkinnedAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh", meta = (AssetBundles = "Client"))
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;
};

/**
 * Normalized read-only view over the authored arrays: statics first, then skinned, with the
 * trait-wide TransformProvider/RenderingProps stamped into every element. Editor consumers
 * (placed-entity render, preview, physics extraction) iterate this instead of the two arrays;
 * the element index is the combined index SetMeshEntryTransform expects.
 */
struct FArcUnifiedMeshEntry
{
	TSoftObjectPtr<UStaticMesh> StaticMesh;
	TSoftObjectPtr<USkinnedAsset> SkinnedAsset;
	UTransformProviderData* TransformProvider = nullptr;
	FTransform RelativeTransform = FTransform::Identity;
	TArray<TSoftObjectPtr<UMaterialInterface>> MaterialOverrides;
	FArcMassVisRenderingProps RenderingProps;

	bool IsValidEntry() const { return StaticMesh.IsNull() != SkinnedAsset.IsNull(); }
	bool IsSkinnedEntry() const { return !SkinnedAsset.IsNull(); }
};

/**
 * Unified mesh visualization — one trait covering the static, skinned, and composite
 * representations. The fragment set is inferred from the authored entries (see
 * EArcUnifiedMeshVisMode); degenerate single-entry configs collapse onto the exact
 * fragment sets the dedicated single-mesh traits emit, so the runtime pipelines are
 * shared unchanged. Mixed static+skinned entries ride the composite path.
 *
 * Rendering settings (RenderingProps) and the skinned TransformProvider are authored
 * once for the whole trait — per-entry granularity is intentionally limited to the
 * mesh, its relative transform, and material overrides.
 */
UCLASS(BlueprintType, EditInlineNew, meta = (DisplayName = "Arc Unified Mesh Visualization", Category = "Visualization"))
class ARCMASS_API UArcUnifiedMeshVisualizationTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;

	/**
	 * Mode inference from authored entries (soft path set, never residency) — the single
	 * source of truth shared by BuildTemplate (runtime) and the editor classifiers
	 * (placed-entity routing, placement factory), so the two can never disagree.
	 */
	EArcUnifiedMeshVisMode GetInferredMode() const;

	/** Normalized view: statics first, then skinned. See FArcUnifiedMeshEntry. */
	TArray<FArcUnifiedMeshEntry> GetMeshEntries() const;
	bool IsValid() const { return GetInferredMode() != EArcUnifiedMeshVisMode::None; }

#if WITH_EDITOR
	const FArcMassVisRenderingProps& GetRenderingProps() const { return RenderingProps; }
	UTransformProviderData* GetTransformProvider() const { return TransformProvider; }

	/** Non-loading degenerate-case accessors — meaningful only in Static/Skinned mode. */
	UStaticMesh* GetSingleStaticMeshDirect() const;
	USkinnedAsset* GetSingleSkinnedAssetDirect() const;
	FTransform GetSingleRelativeTransform() const;
	TArray<TObjectPtr<UMaterialInterface>> GetSingleMaterialOverridesDirect() const;
	const FArcMassVisRenderingProps& GetSingleRenderingProps() const { return RenderingProps; }

	/** Soft paths for async-load requests — all meshes and materials across both arrays. No loads. */
	void CollectSoftPaths(TArray<FSoftObjectPath>& OutPaths) const;

	/**
	 * Preview-gizmo write-back. Index is the combined GetMeshEntries() index
	 * (statics first, then skinned). Bounds-checked.
	 */
	void SetMeshEntryTransform(int32 Index, const FTransform& InTransform);

	/** Replaces the whole static-mesh list. The only writer besides the preview gizmo. */
	void SetStaticMeshEntries(TConstArrayView<FArcUnifiedStaticMeshEntry> InEntries);
#endif

protected:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<FArcUnifiedStaticMeshEntry> StaticMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TArray<FArcUnifiedSkinnedMeshEntry> SkinnedMeshes;

	/** Animation/skinning data provider shared by every skinned entry. Config-internal object — hard ref. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mesh")
	TObjectPtr<UTransformProviderData> TransformProvider;

	/** Rendering settings shared by every entry (static and skinned). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rendering", meta = (ShowOnlyInnerProperties))
	FArcMassVisRenderingProps RenderingProps;

private:
	/** Counts of authored entries (soft path set). */
	void CountAuthoredEntries(int32& OutStatic, int32& OutSkinned) const;
};
