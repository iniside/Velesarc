// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttributeHandlerConfig.h"

#include "ArcMassDamageToHealthHandler.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassDamageToHealthHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	virtual bool PreChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float& NewValue) override;
	virtual void PostExecute(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override;
};
