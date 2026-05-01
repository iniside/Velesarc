// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Elements/Interfaces/TypedElementDetailsInterface.h"
#include "SkMInstanceElementDetailsInterface.generated.h"

UCLASS(MinimalAPI)
class USkMInstanceElementDetailsInterface : public UObject, public ITypedElementDetailsInterface
{
	GENERATED_BODY()

public:
	ARCMASSEDITOR_API virtual TUniquePtr<ITypedElementDetailsObject> GetDetailsObject(const FTypedElementHandle& InElementHandle) override;
};
