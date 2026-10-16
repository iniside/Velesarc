// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "UObject/ObjectMacros.h"

class AActor;
class UActorComponent;
class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class USceneComponent;

namespace UE::ArcEcotope
{
	/** Generated instance components are derived data: renaming or destroying one never dirties the transaction buffer. */
	constexpr ERenameFlags DerivedRenameFlags = REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional;

	ARCECOTOPE_API bool IsGeneratedComponentName(const UActorComponent& Component);

	/** Author-controlled collision and navigation are hashed as content, not asserted here: failing this predicate destroys the bake. */
	ARCECOTOPE_API bool HasValidGeneratedComponentContract(const USceneComponent& Component, const USceneComponent* RootComponent);

	ARCECOTOPE_API void RenameForDestruction(UActorComponent& Component, const TCHAR* BaseName);

	ARCECOTOPE_API void RemoveInstanceComponents(
		AActor& Owner,
		TArray<TObjectPtr<UInstancedStaticMeshComponent>>& StaticComponents,
		TArray<TObjectPtr<UInstancedSkinnedMeshComponent>>& SkinnedComponents);

	/** Canonical field-wise hash of the actual component contents, independent of array order. */
	ARCECOTOPE_API bool TryComputeContentHash(
		TConstArrayView<TObjectPtr<UInstancedStaticMeshComponent>> StaticComponents,
		TConstArrayView<TObjectPtr<UInstancedSkinnedMeshComponent>> SkinnedComponents,
		uint64& OutHash,
		FText& OutError);
}
