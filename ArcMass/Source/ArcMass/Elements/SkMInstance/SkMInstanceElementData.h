// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Elements/Framework/TypedElementData.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceManager.h"

struct FTypedElementHandle;

/** Registered typed element type name for skinned mesh instances. */
ARCMASS_API extern const FName NAME_SkMInstance;

/**
 * Element data that represents a specific instance within an instanced skinned mesh component.
 */
struct FSkMInstanceElementData
{
	ARCMASS_API UE_DECLARE_TYPED_ELEMENT_DATA_RTTI(FSkMInstanceElementData);

	FSkMInstanceElementId InstanceElementId;
};

template <>
inline FString GetTypedElementDebugId<FSkMInstanceElementData>(const FSkMInstanceElementData& InElementData)
{
	return InElementData.InstanceElementId.SkMComponent
		? FString::Printf(TEXT("SkMInstance:%d"), InElementData.InstanceElementId.InstanceId)
		: TEXT("null");
}

namespace SkMInstanceElementDataUtil
{

/**
 * Get the skinned mesh instance manager for the given instance.
 * Checks the owner actor for ISkMInstanceManagerProvider first, then ISkMInstanceManager cast.
 * @return The instance manager, or null if this instance cannot be managed.
 */
ARCMASS_API ISkMInstanceManager* GetSkMInstanceManager(const FSkMInstanceId& InstanceId);

/**
 * Attempt to get the skinned mesh instance manager from the given element handle.
 * @return The FSkMInstanceManager if the handle contains FSkMInstanceElementData and resolves
 *         to a valid manager, otherwise an invalid (empty) manager.
 */
ARCMASS_API FSkMInstanceManager GetSkMInstanceFromHandle(const FTypedElementHandle& InHandle, bool bSilent = false);

/**
 * Attempt to get the skinned mesh instance manager from the given element handle,
 * asserting if the handle doesn't contain FSkMInstanceElementData.
 * @return The FSkMInstanceManager, or an invalid one if it didn't resolve to a valid manager.
 */
ARCMASS_API FSkMInstanceManager GetSkMInstanceFromHandleChecked(const FTypedElementHandle& InHandle);

} // namespace SkMInstanceElementDataUtil
