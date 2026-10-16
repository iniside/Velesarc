// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "Mass/EntityHandle.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcCompositeMeshVisualizationFragments.generated.h"

class UStaticMesh;
class USkinnedAsset;
class UMaterialInterface;
class UTransformProviderData;

USTRUCT()
struct ARCMASS_API FArcCompositeMeshEntryConfig
{
	GENERATED_BODY()

	// Static entry: Mesh is set, SkinnedAsset is null. Skinned entry: the reverse.
	UPROPERTY()
	TWeakObjectPtr<const UStaticMesh> Mesh;

	// Non-const to match FArcMassSkinnedMeshFragment::SkinnedAsset at fragment build time.
	UPROPERTY()
	TWeakObjectPtr<USkinnedAsset> SkinnedAsset;

	UPROPERTY()
	TObjectPtr<UTransformProviderData> TransformProvider;

	UPROPERTY()
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY()
	FArcMassVisualizationMeshConfigFragment MeshConfig;

	bool IsSkinned() const { return SkinnedAsset.IsValid(); }
};

USTRUCT()
struct ARCMASS_API FArcCompositeMeshConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FArcCompositeMeshEntryConfig> Entries;
};

template<>
struct TMassFragmentTraits<FArcCompositeMeshConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Per-sub-mesh ISM link — one entry per authored FArcCompositeMeshEntryConfig.
// Mutated in place on activate/deactivate; fragment is in the archetype from
// spawn so no runtime recomposition is needed.
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcCompositeMeshSubInstance
{
	GENERATED_BODY()

	FMassEntityHandle HolderEntity;
	int32 InstanceIndex = INDEX_NONE;

	// Skinned sub-instances live in skinned ISM holders and are removed via
	// RemoveSkinnedISMInstance (raw sparse slot); static ones via the
	// source-entity-keyed RemoveISMInstance. Dispatching on the wrong API
	// silently leaks the instance slot.
	bool bIsSkinned = false;
};

USTRUCT()
struct ARCMASS_API FArcCompositeMeshVisInstanceLinkFragment : public FMassFragment
{
	GENERATED_BODY()

	TArray<FArcCompositeMeshSubInstance> Instances;
};

template<>
struct TMassFragmentTraits<FArcCompositeMeshVisInstanceLinkFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

// ---------------------------------------------------------------------------
// Tag — identifies composite-mesh visualization entities.
// Distinct from FArcVisEntityTag (single-mesh).
// ---------------------------------------------------------------------------

USTRUCT()
struct ARCMASS_API FArcCompositeMeshVisEntityTag : public FMassTag
{
	GENERATED_BODY()
};
