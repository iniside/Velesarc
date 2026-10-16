// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcActiveEffectHandle.h"
#include "Events/ArcGameEvent.h"
#include "Fragments/ArcEffectStackFragment.h"
#include "StructUtils/InstancedStruct.h"
#include "Tags/ArcGameplayTagRequirements.h"
#include "ArcTriggerTypes.generated.h"

struct FMassEntityManager;
struct FArcEffectContext;
struct FArcEffectTask;
class UArcEffectDefinition;
class UArcAbilityDefinition;

UENUM(BlueprintType)
enum class EArcIfChannel : uint8
{
	DamageDoneMultiplier UMETA(ToolTip = "The owner is the hit's source. Multiplies the hit's damage by (1 + Value) after the outgoing percent term, as a separate factor; several multiply."),
	DamageDoneFlat UMETA(ToolTip = "The owner is the hit's source. Added to the flat Outgoing Bonus; several add."),
	CritChance UMETA(ToolTip = "The owner is the hit's source. Added to the crit chance before the roll, so Crit and Zone conditions are never met for it; several add. Executions that roll no crit ignore it."),
	CritDamage UMETA(ToolTip = "The owner is the hit's source. Added to the crit multiplier, on a crit only; several add. Executions that roll no crit ignore it."),
	DamageTakenMultiplier UMETA(ToolTip = "The owner is the struck target. Multiplies the damage by (1 + Value) after the Incoming Bonus percent, before mitigation; several multiply."),
	DamageTakenFlat UMETA(ToolTip = "The owner is the struck target. Added to the flat Incoming Bonus; several add.")
};

UENUM(BlueprintType)
enum class EArcTriggerParty : uint8
{
	Owner UMETA(ToolTip = "The entity that carries the trigger's host effect."),
	Other UMETA(ToolTip = "The other party of the event or hit. There is none for While."),
	HostSource UMETA(ToolTip = "The source entity of the trigger's host effect.")
};

UENUM(BlueprintType)
enum class EArcTriggerCompare : uint8
{
	Less,
	LessOrEqual,
	Greater,
	GreaterOrEqual
};

UENUM(BlueprintType)
enum class EArcTriggerCritFilter : uint8
{
	Any,
	Crit,
	NotCrit
};

UENUM(BlueprintType)
enum class EArcTriggerChargeMode : uint8
{
	Unlimited UMETA(ToolTip = "The trigger fires without limit."),
	Trigger UMETA(ToolTip = "The trigger counts Charges and stops firing when they are spent. With Remove Host When Charges Spent, the host effect is removed once they are spent."),
	HostStack UMETA(ToolTip = "Each firing consumes one stack of the host effect; the host effect ends at zero stacks. The trigger keeps no count of its own.")
};

struct ARCMASSMECHANICS_API FArcTriggerContext
{
	FMassEntityManager* EntityManager = nullptr;
	FMassEntityHandle Owner;
	FMassEntityHandle Other;
	FMassEntityHandle HostSource;
	const FArcEffectContext* HostContext = nullptr;
	const FArcEffectContext* HitContext = nullptr;
	const FArcGameEvent* Event = nullptr;
	const UArcAbilityDefinition* Ability = nullptr;
	const UArcEffectDefinition* EventEffect = nullptr;
	const FGameplayTagContainer* OwnerTags = nullptr;
	const FGameplayTagContainer* OtherTags = nullptr;
	bool bHasHit = false;
	bool bCritRolled = true;
	bool bCrit = false;
	FGameplayTag Zone;
	FGameplayTag DamageType;

	FMassEntityHandle GetEntity(EArcTriggerParty Party) const;
	FGameplayTagContainer GetTags(EArcTriggerParty Party) const;
};

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCMASSMECHANICS_API FArcTriggerCondition
{
	GENERATED_BODY()

	virtual ~FArcTriggerCondition() = default;

	virtual bool IsMet(const FArcTriggerContext& Context) const { return true; }

	virtual bool GetAsTagRequirements(EArcTriggerParty& OutParty, FArcGameplayTagRequirements& OutRequirements) const { return false; }

	virtual bool GetWatchedAttributes(EArcTriggerParty& OutParty, TArray<FArcAttributeRef>& OutAttributes) const { return false; }

	virtual void ValidateInOn(TArray<FText>& OutErrors) const {}

	virtual void ValidateInWhile(TArray<FText>& OutErrors) const {}

	virtual void ValidateInIf(TArray<FText>& OutErrors) const {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Tags", ToolTip = "Met when the party's tags satisfy Requirements. On reads the event's captured tags for Owner and Other on every event; If reads the hit's captured tags; While reads the party's owned tags. Host Source always reads its owned tags. A While re-evaluates when the party gains or loses a tag named in Requirements or a child of one."))
struct ARCMASSMECHANICS_API FArcTriggerCondition_Tags : public FArcTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcTriggerParty Party = EArcTriggerParty::Owner;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcGameplayTagRequirements Requirements;

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual bool GetAsTagRequirements(EArcTriggerParty& OutParty, FArcGameplayTagRequirements& OutRequirements) const override;
	virtual void ValidateInOn(TArray<FText>& OutErrors) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Attribute Threshold", ToolTip = "Compares the party's Attribute current value, or its ratio to MaxAttribute when set, against Value. A While re-evaluates it whenever Attribute or MaxAttribute of the party changes."))
struct ARCMASSMECHANICS_API FArcTriggerCondition_AttributeThreshold : public FArcTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcTriggerParty Party = EArcTriggerParty::Owner;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcAttributeRef Attribute;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcAttributeRef MaxAttribute;

	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcTriggerCompare Compare = EArcTriggerCompare::Less;

	UPROPERTY(EditAnywhere, Category = "Condition")
	float Value = 0.f;

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual bool GetWatchedAttributes(EArcTriggerParty& OutParty, TArray<FArcAttributeRef>& OutAttributes) const override;
	virtual void ValidateInOn(TArray<FText>& OutErrors) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Event Magnitude", ToolTip = "On only. Compares the event's Magnitude against Value. Each event channel sets its own Magnitude (for example the final damage on ArcMech.Hit.Outcome.Hit and ArcMech.Damage.Dealt)."))
struct ARCMASSMECHANICS_API FArcTriggerCondition_EventMagnitude : public FArcTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcTriggerCompare Compare = EArcTriggerCompare::GreaterOrEqual;

	UPROPERTY(EditAnywhere, Category = "Condition")
	float Value = 0.f;

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Hit Result", ToolTip = "Met on a hit event with the Hit outcome (ArcMech.Hit.Outcome.Hit), on an ArcMech.Damage.Dealt event, or inside an If hit; filters on crit, struck zone (any of Zones) and the hit's damage type (matching any of DamageTypes, a category tag covers its children). A Hit event carries the crit, zone and damage type of the execution that published it. A Crit filter other than Any and a non-empty Zones filter are unmet for an If read before the crit roll (Crit Chance). Empty containers match anything."))
struct ARCMASSMECHANICS_API FArcTriggerCondition_HitResult : public FArcTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcTriggerCritFilter Crit = EArcTriggerCritFilter::Any;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (Categories = "ArcMech.Hit.Zone"))
	FGameplayTagContainer Zones;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (Categories = "ArcEffect.Damage"))
	FGameplayTagContainer DamageTypes;

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Effect Tags", ToolTip = "Met when the event's effect definition tags satisfy Requirements. Only an effect event (ArcEvent.Effect.Applied, Executed, Removed, Rejected), a hit outcome event (ArcMech.Hit.Outcome.Hit, Miss, Dodge, Parry, Block) or ArcMech.Damage.Dealt carries an effect; on any other event the condition fails."))
struct ARCMASSMECHANICS_API FArcTriggerCondition_EffectTags : public FArcTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcGameplayTagRequirements Requirements;

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Ability", ToolTip = "Action filter: met when the acting ability is Ability (when set) and its AbilityTags satisfy Requirements. The ability comes from the hit for If, and for On from an ability event (ArcEvent.Ability.Activated, Ended, CostFailed, CooldownFailed, CooldownExpired); without one the condition fails."))
struct ARCMASSMECHANICS_API FArcTriggerCondition_Ability : public FArcTriggerCondition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Condition")
	TObjectPtr<UArcAbilityDefinition> Ability = nullptr;

	UPROPERTY(EditAnywhere, Category = "Condition")
	FArcGameplayTagRequirements Requirements;

	virtual bool IsMet(const FArcTriggerContext& Context) const override;
	virtual void ValidateInOn(TArray<FText>& OutErrors) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

using FArcTriggerAppliedList = TArray<TPair<FMassEntityHandle, FArcActiveEffectHandle>>;

USTRUCT(BlueprintType, meta = (Hidden))
struct ARCMASSMECHANICS_API FArcTriggerEffect
{
	GENERATED_BODY()

	virtual ~FArcTriggerEffect() = default;

	virtual void Fire(const FArcTriggerContext& Context, FArcTriggerAppliedList* OutWhileApplied, FArcTriggerAppliedList* OutRemoveWithHost) const {}

	virtual void ValidateInOn(TArray<FText>& OutErrors) const {}

	virtual void ValidateInWhile(TArray<FText>& OutErrors) const {}

	virtual void ValidateInIf(TArray<FText>& OutErrors) const {}
};

USTRUCT(BlueprintType, meta = (DisplayName = "Apply Effect", ToolTip = "Applies Effect to Recipient with Instigator as its source. The applied effect's context is a child of the host effect's context: it keeps the host's source data (item), ability and origin tags, plus the trigger origin tag. Duration, when set, is the base duration passed to the applied effect's duration calculation; unset uses the effect's own duration. Effects applied by a While are removed when the condition stops holding; with RemoveWithHost, effects applied by On or If are removed when the host effect ends."))
struct ARCMASSMECHANICS_API FArcTriggerEffect_ApplyEffect : public FArcTriggerEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Effect")
	TObjectPtr<UArcEffectDefinition> Effect = nullptr;

	UPROPERTY(EditAnywhere, Category = "Effect")
	EArcTriggerParty Recipient = EArcTriggerParty::Owner;

	UPROPERTY(EditAnywhere, Category = "Effect")
	EArcTriggerParty Instigator = EArcTriggerParty::Owner;

	UPROPERTY(EditAnywhere, Category = "Effect")
	TOptional<float> Duration;

	UPROPERTY(EditAnywhere, Category = "Effect")
	bool bRemoveWithHost = false;

	virtual void Fire(const FArcTriggerContext& Context, FArcTriggerAppliedList* OutWhileApplied, FArcTriggerAppliedList* OutRemoveWithHost) const override;
	virtual void ValidateInOn(TArray<FText>& OutErrors) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Cast Ability", ToolTip = "Pushes InputTag on Caster through ArcAbilities::PushInput, so the granted ability bound to InputTag activates under its normal gates. With ReleaseInput the input is released right after the press."))
struct ARCMASSMECHANICS_API FArcTriggerEffect_CastAbility : public FArcTriggerEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ability", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;

	UPROPERTY(EditAnywhere, Category = "Ability")
	EArcTriggerParty Caster = EArcTriggerParty::Owner;

	UPROPERTY(EditAnywhere, Category = "Ability")
	bool bReleaseInput = true;

	virtual void Fire(const FArcTriggerContext& Context, FArcTriggerAppliedList* OutWhileApplied, FArcTriggerAppliedList* OutRemoveWithHost) const override;
	virtual void ValidateInOn(TArray<FText>& OutErrors) const override;
	virtual void ValidateInWhile(TArray<FText>& OutErrors) const override;
	virtual void ValidateInIf(TArray<FText>& OutErrors) const override;
};

USTRUCT(BlueprintType)
struct ARCMASSMECHANICS_API FArcIfModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Modifier")
	EArcIfChannel Channel = EArcIfChannel::DamageDoneMultiplier;

	UPROPERTY(EditAnywhere, Category = "Modifier", meta = (ToolTip = "Fraction for a multiplier channel (0.3 gives x1.3), amount for a flat or crit channel. A negative Value reduces."))
	float Value = 0.f;
};

namespace ArcTriggers
{
	enum class EChargeResult : uint8
	{
		Unlimited,
		Consumed,
		Spent
	};

	struct FChargeOutcome
	{
		EChargeResult Result = EChargeResult::Unlimited;
		bool bAllSpent = false;
	};

	ARCMASSMECHANICS_API bool Compare(float Lhs, EArcTriggerCompare Op, float Rhs);

	ARCMASSMECHANICS_API bool IsTriggerOriginated(const FGameplayTagContainer& Tags);

	ARCMASSMECHANICS_API bool IsTagChangeChannel(const FGameplayTag& EventTag);

	ARCMASSMECHANICS_API FMassEntityManager* GetEntityManager(const UWorld* World);

	ARCMASSMECHANICS_API FArcActiveEffect* FindHostEffect(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle);

	// ActiveEffects compacts and reallocates, so callbacks keep (host handle, task index) and re-find the task instead of holding a pointer.
	template<typename TaskType>
	TaskType* FindTask(FArcActiveEffect* HostEffect, int32 TaskIndex)
	{
		return HostEffect && HostEffect->Tasks.IsValidIndex(TaskIndex) ? HostEffect->Tasks[TaskIndex].template GetMutablePtr<TaskType>() : nullptr;
	}

	ARCMASSMECHANICS_API int32 FindTaskIndex(const FArcActiveEffect& HostEffect, const FArcEffectTask* Task);

	ARCMASSMECHANICS_API bool AreConditionsMet(TConstArrayView<FInstancedStruct> Conditions, const FArcTriggerContext& Context);

	ARCMASSMECHANICS_API void FireEffects(TConstArrayView<FInstancedStruct> Effects, const FArcTriggerContext& Context,
		FArcTriggerAppliedList* OutWhileApplied, FArcTriggerAppliedList* OutRemoveWithHost);

	ARCMASSMECHANICS_API void RemoveAppliedEffects(FMassEntityManager& EntityManager, const FArcTriggerAppliedList& Applied);

	ARCMASSMECHANICS_API void PruneAppliedEffects(FMassEntityManager& EntityManager, FArcTriggerAppliedList& Applied);

	ARCMASSMECHANICS_API void DeferRemoveAppliedEffects(UWorld* World, FArcTriggerAppliedList&& Applied);

	ARCMASSMECHANICS_API int32 GetInitialCharges(EArcTriggerChargeMode ChargeMode, int32 Charges);

	ARCMASSMECHANICS_API FChargeOutcome TryConsumeCharge(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle,
		EArcTriggerChargeMode ChargeMode, int32& RemainingCharges, int32& PendingHostStackConsumes, const FString& Description);

	ARCMASSMECHANICS_API void RefundCharge(EArcTriggerChargeMode ChargeMode, const FChargeOutcome& Outcome, int32& RemainingCharges,
		int32& PendingHostStackConsumes);

	ARCMASSMECHANICS_API void PayCharge(FMassEntityManager& EntityManager, FMassEntityHandle Owner, FArcActiveEffectHandle HostHandle,
		EArcTriggerChargeMode ChargeMode, const FChargeOutcome& Outcome, bool bRemoveHostWhenChargesSpent, const FString& Description);

	ARCMASSMECHANICS_API const TCHAR* GetChargeResultName(EChargeResult Result);

	ARCMASSMECHANICS_API FString DescribeTask(const FArcActiveEffect& HostEffect, int32 TaskIndex, const TCHAR* Kind);
}
