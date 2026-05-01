// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementId.h"

class UInstancedSkinnedMeshComponent;
struct FSkMInstanceElementData;
template <typename ElementDataType> struct TTypedElementOwner;

namespace ArcSkMInstanceElementUtils
{

/**
 * Create a new typed element owner for the given skinned mesh instance.
 * The caller is responsible for managing the lifetime of the returned owner.
 */
ARCMASS_API TTypedElementOwner<FSkMInstanceElementData> CreateSkMInstanceElement(const FSkMInstanceId& InSkMInstanceId);

/**
 * Acquire or look up an editor typed element handle for the given component and instance id.
 * Constructs FSkMInstanceId from the raw component+id and delegates to the main overload.
 */
ARCMASS_API FTypedElementHandle AcquireEditorSkMInstanceElementHandle(UInstancedSkinnedMeshComponent* InComponent, int32 InInstanceId, bool bAllowCreate = true);

/**
 * Acquire or look up an editor typed element handle for the given skinned mesh instance id.
 * Uses the static owner store; only valid in editor (GIsEditor must be true).
 */
ARCMASS_API FTypedElementHandle AcquireEditorSkMInstanceElementHandle(const FSkMInstanceId& InSkMInstanceId, bool bAllowCreate = true);

/**
 * Destroy the editor typed element for the given skinned mesh instance element id.
 * Unregisters from the global owner store and destroys the element via the registry.
 */
ARCMASS_API void DestroyEditorSkMInstanceElement(const FSkMInstanceElementId& InSkMInstanceElementId);

} // namespace ArcSkMInstanceElementUtils
