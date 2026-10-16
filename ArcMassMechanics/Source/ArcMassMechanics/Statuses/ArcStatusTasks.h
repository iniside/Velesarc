// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectTask.h"
#include "ArcStatusTasks.generated.h"

struct FArcActiveEffect;

UENUM(BlueprintType)
enum class EArcUpkeepPayer : uint8
{
	Source,
	Owner
};

USTRUCT(BlueprintType, meta = (DisplayName = "Upkeep", ToolTip = "On every periodic execution of the owning effect, the payer pays GetAmount: Resource must hold at least the amount, which is then added to Meta as an Execute op (the meta's handler performs the resource write). When the payer is gone, lacks either attribute or cannot pay, the owning effect is removed after the current Mass phase. Needs a periodic effect."))
struct ARCMASSMECHANICS_API FArcEffectTask_Upkeep : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Upkeep")
	EArcUpkeepPayer Payer = EArcUpkeepPayer::Source;

	UPROPERTY(EditAnywhere, Category = "Upkeep")
	FArcAttributeRef Resource;

	UPROPERTY(EditAnywhere, Category = "Upkeep")
	FArcAttributeRef Meta;

	UPROPERTY(EditAnywhere, Category = "Upkeep", meta = (ClampMin = "0.0"))
	float Amount = 0.f;

	virtual float GetAmount(const FArcActiveEffect& OwningEffect) const;

	virtual void OnExecute(FArcActiveEffect& OwningEffect) override;
};
