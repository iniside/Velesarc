// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectTask.h"
#include "Effects/ArcEffectStateTreeContext.h"
#include "StateTreeReference.h"
#include "ArcEffectTask_StateTree.generated.h"

struct FArcActiveEffect;

USTRUCT(BlueprintType, meta=(DisplayName="StateTree"))
struct ARCMASSABILITIES_API FArcEffectTask_StateTree : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="StateTree", meta=(Schema="/Script/ArcMassAbilities.ArcEffectStateTreeSchema"))
	FStateTreeReference StateTreeReference;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect) override;

	const FArcEffectStateTreeContext& GetTreeContext() const { return TreeContext; }

private:
	FArcEffectStateTreeContext TreeContext;
};
