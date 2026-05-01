// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementHierarchyInterface.h"
#include "SkMInstanceElementHierarchyInterface.generated.h"

UCLASS(MinimalAPI)
class USkMInstanceElementHierarchyInterface : public UObject, public ITypedElementHierarchyInterface
{
	GENERATED_BODY()

public:
	ARCMASS_API virtual FTypedElementHandle GetParentElement(const FTypedElementHandle& InElementHandle, const bool bAllowCreate = true) override;
};
