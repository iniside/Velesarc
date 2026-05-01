// Copyright Lukasz Baran. All Rights Reserved.

#if WITH_EDITOR

#include "ArcMass/Elements/SkMInstance/ArcEditorInstancedSkinnedMeshComponent.h"
#include "ArcMass/Elements/SkMInstance/ArcSkMInstanceElementUtils.h"
#include "InstanceDataTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcEditorInstancedSkinnedMeshComponent)

FTypedElementHandle UArcEditorInstancedSkinnedMeshComponent::GetInstanceElementHandle(int32 InstanceIndex, bool bAllowCreate) const
{
	FPrimitiveInstanceId PrimitiveInstanceId = GetInstanceId(InstanceIndex);
	if (!PrimitiveInstanceId.IsValid())
	{
		return FTypedElementHandle();
	}

	return ArcSkMInstanceElementUtils::AcquireEditorSkMInstanceElementHandle(
		const_cast<UArcEditorInstancedSkinnedMeshComponent*>(this),
		PrimitiveInstanceId.GetAsIndex(),
		bAllowCreate);
}

#endif // WITH_EDITOR
