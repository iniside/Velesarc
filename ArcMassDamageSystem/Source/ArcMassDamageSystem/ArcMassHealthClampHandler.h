// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttributeHandlerConfig.h"

#include "ArcMassHealthClampHandler.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassHealthClampHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) override;
};
