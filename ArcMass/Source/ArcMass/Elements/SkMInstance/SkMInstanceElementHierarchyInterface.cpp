// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementHierarchyInterface.h"

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "Elements/Framework/EngineElementsLibrary.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkMInstanceElementHierarchyInterface)

FTypedElementHandle USkMInstanceElementHierarchyInterface::GetParentElement(const FTypedElementHandle& InElementHandle, const bool bAllowCreate)
{
	const FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance)
	{
#if WITH_EDITOR
		UInstancedSkinnedMeshComponent* SkMComponent = SkMInstance.GetSkMComponent();
		if (SkMComponent)
		{
			return UEngineElementsLibrary::AcquireEditorComponentElementHandle(SkMComponent, bAllowCreate);
		}
#endif // WITH_EDITOR
	}
	return FTypedElementHandle();
}
