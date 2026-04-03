// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcMass/Visualization/ArcMassVisualizationConfigFragments.h"
#include "ArcCompositeMeshVisualizationFragments.generated.h"

class UStaticMesh;
class UMaterialInterface;

USTRUCT()
struct ARCMASS_API FArcCompositeMeshEntryConfig
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<const UStaticMesh> Mesh;

	UPROPERTY()
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY()
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY()
	FArcMassVisualizationMeshConfigFragment MeshConfig;
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
