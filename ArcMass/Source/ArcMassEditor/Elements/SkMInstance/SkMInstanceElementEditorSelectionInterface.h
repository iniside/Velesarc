// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcMass/Elements/SkMInstance/SkMInstanceElementSelectionInterface.h"
#include "SkMInstanceElementEditorSelectionInterface.generated.h"

UCLASS(MinimalAPI)
class USkMInstanceElementEditorSelectionInterface : public USkMInstanceElementSelectionInterface
{
	GENERATED_BODY()

public:
	ARCMASSEDITOR_API virtual bool IsElementSelected(const FTypedElementHandle& InElementHandle, const FTypedElementListConstPtr& InSelectionSet, const FTypedElementIsSelectedOptions& InSelectionOptions) override;
	ARCMASSEDITOR_API virtual bool ShouldPreventTransactions(const FTypedElementHandle& InElementHandle) override;
};
