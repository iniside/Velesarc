// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Elements/MassEntity/ArcMassEntityElementSelectionInterface.h"
#include "ArcMassEntityElementEditorSelectionInterface.generated.h"

UCLASS(MinimalAPI)
class UArcMassEntityElementEditorSelectionInterface : public UArcMassEntityElementSelectionInterface
{
	GENERATED_BODY()

public:
	ARCMASSEDITOR_API virtual bool IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions) override;
	ARCMASSEDITOR_API virtual bool ShouldPreventTransactions(const FTypedElementHandle& InElementHandle) override;
};
