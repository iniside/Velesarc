// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Tasks/TargetingFilterTask_BasicFilterTemplate.h"
#include "ArcTargetingFilterTask_Interactable.generated.h"

UCLASS()
class ARCCORE_API UArcTargetingFilterTask_Interactable : public UTargetingFilterTask_BasicFilterTemplate
{
	GENERATED_BODY()

public:
	virtual bool ShouldFilterTarget(const FTargetingRequestHandle& TargetingHandle, const FTargetingDefaultResultData& TargetData) const override;
};
