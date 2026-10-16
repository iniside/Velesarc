// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Effects/ArcEffectTask.h"
#include "Events/ArcGameEvent.h"
#include "StructUtils/InstancedStruct.h"
#include "ArcMassMechanics/Triggers/ArcTriggerTypes.h"
#include "ArcTriggerIf.generated.h"

struct FMassEntityManager;
struct FArcEffectSpec;

UENUM(BlueprintType)
enum class EArcIfSpendOn : uint8
{
	AnyContributingHit UMETA(ToolTip = "A charge is paid on every hit the If contributes to."),
	OnlyCrit UMETA(ToolTip = "A charge is paid only on a crit; on any other hit the Modifiers still apply, the charge is refunded and the Effects do not fire.")
};

USTRUCT(BlueprintType, meta = (DisplayName = "Trigger If", ToolTip = "While the owning stored effect is active, a damage Execution reads it on application where the owner is the hit's source or target, for that hit only (ArcTriggers::GetIfModifier): Modifiers change the hit's damage through fixed channels. Once the damage is final, Effects and the charge run after the current Mass phase (ArcTriggers::CommitIf). On hits of effects applied by any trigger, Modifiers still apply and pay but Effects do not fire."))
struct ARCMASSMECHANICS_API FArcEffectTask_TriggerIf : public FArcEffectTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ToolTip = "Instigator: the owner is the hit's source. Target: the owner is the struck target. Any: either."))
	EArcEventRole Role = EArcEventRole::Instigator;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcTriggerCondition", ExcludeBaseStruct))
	TArray<FInstancedStruct> Conditions;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (ToolTip = "Changes this hit's damage on each entry's channel when the conditions are met. A Done channel needs the owner to be the hit's source, a Taken channel the struck target."))
	TArray<FArcIfModifier> Modifiers;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcTriggerEffect", ExcludeBaseStruct))
	TArray<FInstancedStruct> Effects;

	UPROPERTY(EditAnywhere, Category = "Trigger")
	EArcTriggerChargeMode ChargeMode = EArcTriggerChargeMode::Unlimited;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (EditCondition = "ChargeMode==EArcTriggerChargeMode::Trigger", EditConditionHides, ClampMin = "1", ToolTip = "Number of hits before the trigger is spent."))
	int32 Charges = 1;

	UPROPERTY(EditAnywhere, Category = "Trigger")
	EArcIfSpendOn SpendOn = EArcIfSpendOn::AnyContributingHit;

	UPROPERTY(EditAnywhere, Category = "Trigger", meta = (EditCondition = "ChargeMode==EArcTriggerChargeMode::Trigger", EditConditionHides, ToolTip = "Removes the host effect once this trigger has spent its charges."))
	bool bRemoveHostWhenChargesSpent = true;

	int32 RemainingCharges = -1;
	int32 PendingHostStackConsumes = 0;
	FArcTriggerAppliedList RemoveWithHost;

	virtual void OnApplication(FArcActiveEffect& OwningEffect) override;
	virtual void OnRemoved(FArcActiveEffect& OwningEffect, EArcEffectRemovalReason Reason) override;
};

namespace ArcTriggers
{
	struct FArcIfUse
	{
		FMassEntityHandle Owner;
		FArcActiveEffectHandle HostHandle;
		int32 TaskIndex = INDEX_NONE;
		FChargeOutcome Outcome;
	};

	struct FArcIfHit
	{
		const FArcEffectSpec* Spec = nullptr;
		FMassEntityHandle Source;
		FMassEntityHandle Target;
		bool bCritRolled = true;
		bool bCrit = false;
		FGameplayTag Zone;
		FGameplayTag DamageType;
		TArray<FArcIfUse> Used;
	};

	ARCMASSMECHANICS_API float GetIfModifier(FMassEntityManager& EntityManager, FArcIfHit& Hit, EArcIfChannel Channel);

	ARCMASSMECHANICS_API void CommitIf(FMassEntityManager& EntityManager, const FArcIfHit& Hit);
}
