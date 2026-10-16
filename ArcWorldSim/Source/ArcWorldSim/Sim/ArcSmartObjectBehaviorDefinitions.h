// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Sim/ArcSmartObjectBehaviorDefinition.h"

#include "ArcSmartObjectBehaviorDefinitions.generated.h"

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectWorkBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Work", meta = (ClampMin = "0"))
	float WorkShiftDuration = 60.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Work", meta = (ClampMin = "0"))
	float WorkLoopDuration = 5.0f;
};

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectPickupBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
};

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectDropBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
};

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectGatherBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
};

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectDrawBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
};

UCLASS(EditInlineNew)
class ARCWORLDSIM_API UArcSmartObjectSleepBehaviorDefinition : public UArcSmartObjectMassBehaviorDefinition
{
	GENERATED_BODY()

public:
	UArcSmartObjectSleepBehaviorDefinition();

	virtual void Activate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;
	virtual void Deactivate(FMassCommandBuffer& CommandBuffer, const FMassBehaviorEntityContext& EntityContext) const override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Sleep", meta = (ClampMin = "0"))
	float Comfort = 1.0f;
};
