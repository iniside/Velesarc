// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/SkMInstance/ArcSkMInstanceElementUtils.h"

#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementOwnerStore.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "ArcMass/Elements/SkMInstance/SkMInstanceElementId.h"
#include "Components/InstancedSkinnedMeshComponent.h"

#if WITH_EDITOR
namespace
{
	TTypedElementOwnerStore<FSkMInstanceElementData, FSkMInstanceElementId> GSkMInstanceElementOwnerStore;
} // anonymous namespace
#endif // WITH_EDITOR

namespace ArcSkMInstanceElementUtils
{

static TTypedElementOwner<FSkMInstanceElementData> CreateSkMInstanceElementImpl(const FSkMInstanceId& InSkMInstanceId, const FSkMInstanceElementId& InSkMInstanceElementId)
{
	UTypedElementRegistry* Registry = UTypedElementRegistry::GetInstance();

	TTypedElementOwner<FSkMInstanceElementData> SkMInstanceElement;
	if (ensureAlways(Registry) && SkMInstanceElementDataUtil::GetSkMInstanceManager(InSkMInstanceId))
	{
		SkMInstanceElement = Registry->CreateElement<FSkMInstanceElementData>(NAME_SkMInstance);
		if (SkMInstanceElement)
		{
			SkMInstanceElement.GetDataChecked().InstanceElementId = InSkMInstanceElementId;
		}
	}

	return SkMInstanceElement;
}

TTypedElementOwner<FSkMInstanceElementData> CreateSkMInstanceElement(const FSkMInstanceId& InSkMInstanceId)
{
	const FSkMInstanceElementId SkMInstanceElementId = SkMInstanceIdToElementId(InSkMInstanceId);
	return CreateSkMInstanceElementImpl(InSkMInstanceId, SkMInstanceElementId);
}

FTypedElementHandle AcquireEditorSkMInstanceElementHandle(UInstancedSkinnedMeshComponent* InComponent, const int32 InInstanceId, const bool bAllowCreate)
{
	FSkMInstanceId SkMInstanceId;
	SkMInstanceId.SkMComponent = InComponent;
	SkMInstanceId.InstanceId   = InInstanceId;
	return AcquireEditorSkMInstanceElementHandle(SkMInstanceId, bAllowCreate);
}

FTypedElementHandle AcquireEditorSkMInstanceElementHandle(const FSkMInstanceId& InSkMInstanceId, const bool bAllowCreate)
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		const FSkMInstanceElementId SkMInstanceElementId = SkMInstanceIdToElementId(InSkMInstanceId);

		TTypedElementOwnerScopedAccess<FSkMInstanceElementData> SkMInstanceElement = bAllowCreate
			? GSkMInstanceElementOwnerStore.FindOrRegisterElementOwner(
				SkMInstanceElementId,
				[&InSkMInstanceId, &SkMInstanceElementId]()
				{
					return CreateSkMInstanceElementImpl(InSkMInstanceId, SkMInstanceElementId);
				})
			: GSkMInstanceElementOwnerStore.FindElementOwner(SkMInstanceElementId);

		if (SkMInstanceElement)
		{
			return SkMInstanceElement->AcquireHandle();
		}
	}
#endif // WITH_EDITOR

	return FTypedElementHandle();
}

void DestroyEditorSkMInstanceElement(const FSkMInstanceElementId& InSkMInstanceElementId)
{
#if WITH_EDITOR
	if (TTypedElementOwner<FSkMInstanceElementData> SkMInstanceElement =
			GSkMInstanceElementOwnerStore.UnregisterElementOwner(InSkMInstanceElementId))
	{
		UTypedElementRegistry::GetInstance()->DestroyElement(SkMInstanceElement);
	}
#endif
}

} // namespace ArcSkMInstanceElementUtils
