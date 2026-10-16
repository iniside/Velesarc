// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "Elements/Interfaces/TypedElementDetailsInterface.h"
#include "ArcMassEntityElementDetailsInterface.generated.h"

UCLASS(MinimalAPI)
class UArcMassEntityElementDetailsInterface : public UObject, public ITypedElementDetailsInterface
{
	GENERATED_BODY()

public:
	ARCMASSEDITOR_API virtual TUniquePtr<ITypedElementDetailsObject> GetDetailsObject(const FTypedElementHandle& InElementHandle) override;
};
