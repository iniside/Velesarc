// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/InstancedStruct.h"
#include "Attributes/ArcAttribute.h"
#include "ArcResourceTypes.generated.h"

struct FArcResourceDef;
struct FArcTagCountContainer;
struct FMassEntityManager;

struct ARCMASSMECHANICS_API FArcResourceContext
{
	FMassEntityManager& EntityManager;
	FMassEntityHandle Entity;
	const FArcTagCountContainer* OwnedTags = nullptr;
	const FArcResourceDef& Resource;
	float CurrentValue = 0.f;

	FArcResourceContext(FMassEntityManager& InEntityManager, const FArcResourceDef& InResource)
		: EntityManager(InEntityManager)
		, Resource(InResource)
	{
	}

	bool ReadAttribute(const FArcAttributeRef& Attribute, float& OutValue) const;
	bool ReadUpperBound(float& OutValue) const;
};

USTRUCT(meta = (ToolTip = "Computes the reference value of a resource: a positive rate moves toward it, a negative rate away from it. The base type keeps the current value."))
struct ARCMASSMECHANICS_API FArcResourceTarget
{
	GENERATED_BODY()

	virtual ~FArcResourceTarget() = default;

	virtual float GetTarget(const FArcResourceContext& Context) const;
};

USTRUCT(meta = (DisplayName = "Max Target", ToolTip = "Targets the resource's upper bound: MaxAttribute when set, else Max."))
struct ARCMASSMECHANICS_API FArcResourceTarget_Max : public FArcResourceTarget
{
	GENERATED_BODY()

	virtual float GetTarget(const FArcResourceContext& Context) const override;
};

USTRUCT(meta = (DisplayName = "Min Target", ToolTip = "Targets the resource's Min."))
struct ARCMASSMECHANICS_API FArcResourceTarget_Min : public FArcResourceTarget
{
	GENERATED_BODY()

	virtual float GetTarget(const FArcResourceContext& Context) const override;
};

USTRUCT(meta = (ToolTip = "Signed resource units per second. Positive moves toward the target and never passes it. Negative moves away from the target: down to Min when the value is below the target, up to the upper bound when above; at the target, up when the target is at or below Min, else down. The base type returns 0."))
struct ARCMASSMECHANICS_API FArcResourceRate
{
	GENERATED_BODY()

	virtual ~FArcResourceRate() = default;

	virtual float GetRate(const FArcResourceContext& Context) const;
};

USTRUCT(meta = (DisplayName = "Constant Rate"))
struct ARCMASSMECHANICS_API FArcResourceRate_Constant : public FArcResourceRate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ClampMin = "0.0", ToolTip = "Units per second toward the target."))
	float UnitsPerSecond = 0.f;

	virtual float GetRate(const FArcResourceContext& Context) const override;
};

namespace ArcResourcePips
{
	// D-100: owner exception to numbers-in-data.
	constexpr int32 Cap = 10;

	ARCMASSMECHANICS_API int32 NetPips(float Regen, float Degen);
}

USTRUCT(meta = (DisplayName = "Pips Rate", ToolTip = "Signed units per second: clamp(floor(RegenPips) - floor(DegenPips), -10, 10) * max(0, PerPip). Attributes are read live every update. 0 when RegenPips or PerPip is unset or missing on the entity; an unset or missing DegenPips counts as 0."))
struct ARCMASSMECHANICS_API FArcResourceRate_Pips : public FArcResourceRate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ToolTip = "Required. Pips toward the target, floored."))
	FArcAttributeRef RegenPips;

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ToolTip = "Pips away from the target, floored. Unset or missing counts as 0."))
	FArcAttributeRef DegenPips;

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ToolTip = "Required. Units per second per pip; below 0 counts as 0."))
	FArcAttributeRef PerPip;

	virtual float GetRate(const FArcResourceContext& Context) const override;
};

USTRUCT(BlueprintType, meta = (ToolTip = "While the owner has Condition, the resource's rate is multiplied by Multiplier."))
struct ARCMASSMECHANICS_API FArcResourceRateMultiplier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Rate")
	FGameplayTag Condition;

	UPROPERTY(EditAnywhere, Category = "Rate", meta = (ClampMin = "0.0"))
	float Multiplier = 1.f;
};

USTRUCT(BlueprintType)
struct ARCMASSMECHANICS_API FArcResourceDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ToolTip = "Attribute holding the resource value. Read every update; the resource does nothing on an entity without it."))
	FArcAttributeRef Value;

	UPROPERTY(EditAnywhere, Category = "Attributes", meta = (ToolTip = "Meta attribute receiving each signed step (handler MetaToResource, Op AddSigned, Resource = Value). Unset: the steps are dropped."))
	FArcAttributeRef Delta;

	UPROPERTY(EditAnywhere, Category = "Range", meta = (ToolTip = "Lower bound of the target."))
	float Min = 0.f;

	UPROPERTY(EditAnywhere, Category = "Range", meta = (ToolTip = "Upper bound when MaxAttribute is unset. A value above the upper bound drops to it in one update, ignoring Rate, Target and BlockedBy."))
	float Max = 100.f;

	UPROPERTY(EditAnywhere, Category = "Range", meta = (ToolTip = "Set: the upper bound is this attribute's current value instead of Max. The resource does not step on an entity without it."))
	FArcAttributeRef MaxAttribute;

	UPROPERTY(EditAnywhere, Category = "Slots", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcResourceRate", ExcludeBaseStruct, ToolTip = "Required. Signed speed: positive toward the target, negative away from it."))
	FInstancedStruct Rate;

	UPROPERTY(EditAnywhere, Category = "Slots", meta = (ToolTip = "Every entry whose Condition the owner has multiplies the signed rate."))
	TArray<FArcResourceRateMultiplier> RateMultipliers;

	UPROPERTY(EditAnywhere, Category = "Slots", meta = (BaseStruct = "/Script/ArcMassMechanics.ArcResourceTarget", ExcludeBaseStruct, ToolTip = "Required. Reference value, clamped to [Min, upper bound]: a positive rate moves toward it, a negative rate away from it."))
	FInstancedStruct Target;

	UPROPERTY(EditAnywhere, Category = "Slots", meta = (ToolTip = "While the owner has any of these tags, a positive rate does not move the resource toward its target; a negative rate (degen) still moves it away. Writes through the Delta or other metas (costs, effects) still apply. Does not stop the drop of a value above the upper bound. An entity without owned tags is never blocked."))
	FGameplayTagContainer BlockedBy;

	float GetRateMultiplier(const FArcResourceContext& Context) const;
	float ComputeStep(const FArcResourceContext& Context, float StepSeconds) const;
};

UCLASS(BlueprintType, meta = (ToolTip = "Resources driven by UArcResourceProcessor for entities with UArcResourceTrait. Resources update in array order."))
class ARCMASSMECHANICS_API UArcResourceConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Resources")
	TArray<FArcResourceDef> Resources;

	UPROPERTY(EditAnywhere, Category = "Resources", meta = (ClampMin = "0.0", Units = "Seconds", ToolTip = "Seconds between per-entity resource updates. Each update applies the full elapsed time."))
	float UpdateInterval = 1.f;
};

USTRUCT()
struct ARCMASSMECHANICS_API FArcResourceStateFragment : public FMassFragment
{
	GENERATED_BODY()

	UPROPERTY()
	float UpdateAccumulator = 0.f;
};

USTRUCT()
struct ARCMASSMECHANICS_API FArcResourceConfigSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UArcResourceConfig> Config = nullptr;
};
