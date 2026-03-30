// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTypes.h"
#include "ArcInstancedWorld/ArcIWTypes.h"
#include "ArcIWLifecycleTypes.generated.h"

USTRUCT(BlueprintType)
struct ARCINSTANCEDWORLD_API FArcIWLifecycleMeshOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	uint8 Phase = 0;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	TObjectPtr<UStaticMesh> MeshOverride = nullptr;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	TObjectPtr<USkinnedAsset> SkinnedAssetOverride = nullptr;

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;
};

USTRUCT(BlueprintType)
struct ARCINSTANCEDWORLD_API FArcIWLifecycleVisConfigFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Lifecycle|Visuals")
	TArray<FArcIWLifecycleMeshOverride> MeshOverrides;

	const FArcIWLifecycleMeshOverride* FindOverride(uint8 Phase) const;
};

template<>
struct TMassFragmentTraits<FArcIWLifecycleVisConfigFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

namespace UE::ArcIW::Lifecycle
{
	FArcIWMeshEntry ResolvePhaseOverride(
		const FArcIWMeshEntry& BaseMesh,
		const FArcIWLifecycleMeshOverride& Override);
}
