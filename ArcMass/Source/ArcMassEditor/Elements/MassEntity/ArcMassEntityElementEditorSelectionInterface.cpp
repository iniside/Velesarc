// Copyright Lukasz Baran. All Rights Reserved.

#include "Elements/MassEntity/ArcMassEntityElementEditorSelectionInterface.h"

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementData.h"
#include "Elements/Framework/TypedElementList.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcMassEntityElementEditorSelectionInterface)

bool UArcMassEntityElementEditorSelectionInterface::IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions)
{
	return ITypedElementSelectionInterface::IsElementSelected(InElementHandle, InSelectionSet, InSelectionOptions);
}

bool UArcMassEntityElementEditorSelectionInterface::ShouldPreventTransactions(const FTypedElementHandle& InElementHandle)
{
	return false;
}
