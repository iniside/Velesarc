// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Sim/ArcSmartObjectBehaviorDefinition.h"

#include "ArcSmartObjectEatBehaviorDefinition.generated.h"

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectEatBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
};
