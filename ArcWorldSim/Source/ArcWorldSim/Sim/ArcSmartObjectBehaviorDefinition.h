// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "MassSmartObjectBehaviorDefinition.h"

#include "ArcSmartObjectBehaviorDefinition.generated.h"

UCLASS(Abstract, EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectMassBehaviorDefinition : public USmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;

	bool IsInterruptible() const
	{
		return bInterruptible;
	}

private:
	UPROPERTY(EditDefaultsOnly, Category = "Smart Object")
	bool bInterruptible = true;
};
