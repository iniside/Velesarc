// Copyright Lukasz Baran. All Rights Reserved.

#include "Elements/SkMInstance/SkMInstanceElementEditorSelectionInterface.h"

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementData.h"
#include "Elements/Framework/TypedElementList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkMInstanceElementEditorSelectionInterface)

bool USkMInstanceElementEditorSelectionInterface::IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions)
{
	return ITypedElementSelectionInterface::IsElementSelected(InElementHandle, InSelectionSet, InSelectionOptions);
}

bool USkMInstanceElementEditorSelectionInterface::ShouldPreventTransactions(const FTypedElementHandle& InElementHandle)
{
	return false;
}
