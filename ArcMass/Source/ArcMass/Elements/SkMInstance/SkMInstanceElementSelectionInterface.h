// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementSelectionInterface.h"
#include "SkMInstanceElementSelectionInterface.generated.h"

UCLASS(MinimalAPI)
class USkMInstanceElementSelectionInterface : public UObject, public ITypedElementSelectionInterface
{
	GENERATED_BODY()

public:
	ARCMASS_API virtual bool SelectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions) override;
	ARCMASS_API virtual bool DeselectElement(const FTypedElementHandle& InElementHandle, const FTypedElementListPtr& InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions) override;
};
