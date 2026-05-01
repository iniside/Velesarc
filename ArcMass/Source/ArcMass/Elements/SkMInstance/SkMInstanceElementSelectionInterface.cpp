// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementSelectionInterface.h"

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkMInstanceElementSelectionInterface)

bool USkMInstanceElementSelectionInterface::SelectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance && ITypedElementSelectionInterface::SelectElement(InElementHandle, InSelectionSet, InSelectionOptions))
	{
		SkMInstance.NotifySkMInstanceSelectionChanged(/*bIsSelected*/true);
		return true;
	}
	return false;
}

bool USkMInstanceElementSelectionInterface::DeselectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions)
{
	FSkMInstanceManager SkMInstance = SkMInstanceElementDataUtil::GetSkMInstanceFromHandle(InElementHandle);
	if (SkMInstance && ITypedElementSelectionInterface::DeselectElement(InElementHandle, InSelectionSet, InSelectionOptions))
	{
		SkMInstance.NotifySkMInstanceSelectionChanged(/*bIsSelected*/false);
		return true;
	}
	return false;
}
