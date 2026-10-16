// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Effects/ArcEffectTask.h"
#include "Events/ArcGameEvent.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcTriggerOn.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Trigger On", ToolTip = "While the owning stored effect is active, fires Effects on each game bus event on the owner on exactly EventTag and Role (delivered one frame late), when Conditions are met and the Chance roll succeeds. Events whose captured source tags carry ArcMech.Trigger.Origin (effects applied and abilities cast by any trigger) fire nothing: procs never trigger procs. Torn down when the effect is removed."))
struct ARCMASSMECHANICS_API FArcEffectTask_TriggerOn : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ToolTip = "The exact event channel. A parent tag never matches its children: ArcMech.Hit.Outcome does not fire on ArcMech.Hit.Outcome.Block; author one On per channel. ArcEvent.Tag.Added and ArcEvent.Tag.Removed need Tag."))
	FGameplayTag EventTag;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ToolTip = "Only with EventTag ArcEvent.Tag.Added or ArcEvent.Tag.Removed: the owned tag whose gain or loss on the owner fires the trigger. Exact: a change of a child tag does not fire it. Must be empty on any other channel."))
	FGameplayTag Tag;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ToolTip = "Instigator: the owner acts (event instigator). Target: the owner is acted on (event target). Any: either."))
	EArcEventRole Role = EArcEventRole::Instigator;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Chance in (0, 1] that a matching event fires the trigger. Always authored; there is no default."))
	float Chance = 0.f;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcTriggerCondition", ExcludeBaseStruct))
	TArray<FInstancedStruct> Conditions;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcTriggerEffect", ExcludeBaseStruct))
	TArray<FInstancedStruct> Effects;

	UPROPERTY(EditAnywhere, Category = "Trigger")
	EArcTriggerChargeMode ChargeMode = EArcTriggerChargeMode::Unlimited;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (EditCondition = "ChargeMode==EArcTriggerChargeMode::Trigger", EditConditionHides, ClampMin = "1", ToolTip = "Number of firings before the trigger is spent."))
	int32 Charges = 1;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (EditCondition = "ChargeMode==EArcTriggerChargeMode::Trigger", EditConditionHides, ToolTip = "Removes the host effect once this trigger has spent its charges."))
	bool bRemoveHostWhenChargesSpent = true;

	int32 RemainingCharges = -1;
	int32 PendingHostStackConsumes = 0;
	FArcTriggerAppliedList RemoveWithHost;
	FDelegateHandle Subscription;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
};
