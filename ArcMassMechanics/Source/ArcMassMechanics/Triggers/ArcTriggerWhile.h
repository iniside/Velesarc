// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectTask.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcTriggerWhile.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Trigger While", ToolTip = "While the owning stored effect is active, fires Effects when all Conditions become true and removes the effects it applied when they stop holding. Re-evaluates when a watched attribute (Attribute Threshold: Attribute and MaxAttribute) of the condition's party changes, or when the party gains or loses a tag named in a Tags condition's Requirements or a child of one. Torn down when the effect is removed."))
struct ARCMASSMECHANICS_API FArcEffectTask_TriggerWhile : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcTriggerCondition", ExcludeBaseStruct))
	TArray<FInstancedStruct> Conditions;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcTriggerEffect", ExcludeBaseStruct))
	TArray<FInstancedStruct> Effects;

	bool bActive = false;
	FArcTriggerAppliedList Applied;
	TArray<FDelegateHandle> Subscriptions;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
};
