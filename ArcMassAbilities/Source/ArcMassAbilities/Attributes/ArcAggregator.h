// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameplayEffectTypes.h"
#include "Mass/EntityHandle.h"
#include "StructUtils/SharedStruct.h"
#include "Effects/ArcEffectTypes.h"
#include "ArcAggregator.generated.h"

struct FMassEntityManager;
struct FArcOwnedTagsFragment;

USTRUCT(BlueprintType)
struct ARCMASSABILITIES_API FArcModifierHandle
{
	GENERATED_BODY()

	UPROPERTY()
	uint64 Id = 0;

	bool IsValid() const { return Id != 0; }

	bool operator==(const FArcModifierHandle& Other) const { return Id == Other.Id; }
	bool operator!=(const FArcModifierHandle& Other) const { return Id != Other.Id; }

	friend uint32 GetTypeHash(const FArcModifierHandle& H) { return GetTypeHash(H.Id); }
};

struct ARCMASSABILITIES_API FArcAggregatorMod
{
	float Magnitude = 0.f;
	FMassEntityHandle Source;
	const FGameplayTagRequirements* SourceTagReqs = nullptr;
	const FGameplayTagRequirements* TargetTagReqs = nullptr;
	FArcModifierHandle Handle;
	uint8 Channel = 0;

	mutable bool bQualified = true;

	bool Qualifies(const FGameplayTagContainer& SourceTags,
	               const FGameplayTagContainer& TargetTags) const;

	void UpdateQualifies(const FGameplayTagContainer& SourceTags,
	                     const FGameplayTagContainer& TargetTags) const;
};

struct ARCMASSABILITIES_API FArcAggregationContext
{
	FMassEntityHandle Owner;
	const FGameplayTagContainer& OwnerTags;
	FMassEntityManager& EntityManager;
	float BaseValue;
};

USTRUCT()
struct ARCMASSABILITIES_API FArcAggregationPolicy
{
	GENERATED_BODY()

	virtual ~FArcAggregationPolicy() = default;

	virtual float Aggregate(
		const FArcAggregationContext& Context,
		const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
	) const;

	static float EvaluateDefault(
		float BaseValue,
		const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)]
	);
};

struct ARCMASSABILITIES_API FArcAggregator
{
	TArray<FArcAggregatorMod> Mods[static_cast<uint8>(EArcModifierOp::Max)];
	FConstSharedStruct AggregationPolicy;

	FArcModifierHandle AddMod(EArcModifierOp Op, float Magnitude,
	                          FMassEntityHandle Source,
	                          const FGameplayTagRequirements* SourceTagReqs,
	                          const FGameplayTagRequirements* TargetTagReqs,
	                          uint8 Channel = 0);

	void AddMod(EArcModifierOp Op, float Magnitude,
	            FMassEntityHandle Source,
	            const FGameplayTagRequirements* SourceTagReqs,
	            const FGameplayTagRequirements* TargetTagReqs,
	            FArcModifierHandle ExistingHandle,
	            uint8 Channel = 0);

	void RemoveMod(FArcModifierHandle Handle);

	bool IsEmpty() const;

	void UpdateQualifications(const FGameplayTagContainer& TargetTags,
	                          FMassEntityManager& EntityManager) const;

	float Evaluate(const FArcAggregationContext& Context) const;

	float EvaluateWithTags(float BaseValue,
	                       const FGameplayTagContainer& SourceTags,
	                       const FGameplayTagContainer& TargetTags) const;
};

namespace ArcModifiers
{
	FArcModifierHandle GenerateHandle();
}
