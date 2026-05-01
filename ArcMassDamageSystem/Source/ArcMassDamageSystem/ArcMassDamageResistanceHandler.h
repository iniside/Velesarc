// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "ArcMassDamageResistanceMappingAsset.h"

#include "ArcMassDamageResistanceHandler.generated.h"

USTRUCT()
struct ARCMASSDAMAGESYSTEM_API FArcMassDamageResistanceHandler : public FArcAttributeHandler
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcMassDamageResistanceMappingAsset> MappingAsset;

	virtual void PostChange(FArcAttributeHandlerContext& Context, FArcAttribute& Attribute, float OldValue, float NewValue) override;
};
