// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "ArcMass/Elements/MassEntity/ArcMassEntityElementId.h"

struct FArcMassEntityElementData;
template <typename ElementDataType> struct TTypedElementOwner;
struct FArcMassEntityManager;
class IArcMassEntityManager;
struct FArcTypeElementInstanceId;

namespace ArcMassEntityElementUtils
{

/**
 * Get the IArcMassEntityManager for the given Mass entity element ID.
 * Resolves via IArcMassEntityManagerProvider or IArcMassEntityManager cast on the owning AActor.
 */
ARCMASS_API IArcMassEntityManager* GetArcMassEntityManager(const FArcMassEntityElementId& InId);

/**
 * Build an FArcMassEntityManager from a typed element handle.
 * Returns a null/invalid FArcMassEntityManager if the handle does not refer to a Mass entity element
 * or if bSilent is false and the data is missing.
 */
ARCMASS_API FArcMassEntityManager GetArcMassEntityFromHandle(const FTypedElementHandle& InHandle, bool bSilent = false);

/**
 * Build an FArcMassEntityManager from a typed element handle, asserting that the handle is valid.
 */
ARCMASS_API FArcMassEntityManager GetArcMassEntityFromHandleChecked(const FTypedElementHandle& InHandle);

/**
 * Acquire or look up an editor typed element handle for the given Mass entity element ID.
 * Uses the static owner store; only valid in editor (GIsEditor must be true).
 */
ARCMASS_API FTypedElementHandle AcquireEditorArcMassEntityElementHandle(const FArcMassEntityElementId& InId, bool bAllowCreate = true);

/**
 * Destroy the editor typed element for the given Mass entity element ID.
 * Unregisters from the global owner store and destroys the element via the registry.
 */
ARCMASS_API void DestroyEditorArcMassEntityElement(const FArcMassEntityElementId& InId);

#if WITH_EDITOR
/**
 * Resolve a render-side instance id to a live editor typed element handle (bAllowCreate=false).
 * Bound to ArcMassRender's GetArcResolveTypeElementHandleDelegate() at ArcMass startup so render-thread
 * hit proxies can mint handles on demand without ArcMassRender depending on ArcMass. Defined in
 * ArcMassEntityElementUtils.cpp.
 */
ARCMASS_API FTypedElementHandle ResolveRenderInstanceId(const FArcTypeElementInstanceId& InInstanceId);

/**
 * Drain all owners from the global ArcMass typed-element owner store, destroying each element via
 * the registry (or Private_DestroyNoRef if the registry is already gone). Called once at engine
 * pre-exit so the static store destructs empty and ~TTypedElementOwnerStore's checkf is satisfied.
 */
ARCMASS_API void DrainEditorArcMassEntityElementOwnerStore();
#endif // WITH_EDITOR

} // namespace ArcMassEntityElementUtils
