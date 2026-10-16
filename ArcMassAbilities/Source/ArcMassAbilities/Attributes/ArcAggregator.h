// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Tags/ArcGameplayTagRequirements.h"
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
	mutable float Magnitude = 0.f;
	FConstSharedStruct LiveMagnitude;
	int32 StackCount = 1;
	FMassEntityHandle Source;
	// Stored by value — never point into definition assets or container-owned memory
	// (TMap rehash / asset reload would dangle). AddMod copies from the nullable-pointer
	// interim API (nullptr = "no reqs").
	FArcGameplayTagRequirements SourceTagReqs;
	FArcGameplayTagRequirements TargetTagReqs;
	FArcModifierHandle Handle;
	uint8 Channel = 0;

	mutable bool bQualified = true;

	bool IsLive() const { return LiveMagnitude.IsValid(); }

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

	// Nullable-pointer = "no reqs" interim API: nullptr copies nothing (mod keeps default-empty reqs).
	// A follow-up may migrate callers to const-ref to remove the nullable ambiguity.
	FArcModifierHandle AddMod(EArcModifierOp Op, float Magnitude,
	                          FMassEntityHandle Source,
	                          const FArcGameplayTagRequirements* SourceTagReqs,
	                          const FArcGameplayTagRequirements* TargetTagReqs,
	                          uint8 Channel = 0);

	// Nullable-pointer = "no reqs" interim API: nullptr copies nothing (mod keeps default-empty reqs).
	void AddMod(EArcModifierOp Op, float Magnitude,
	            FMassEntityHandle Source,
	            const FArcGameplayTagRequirements* SourceTagReqs,
	            const FArcGameplayTagRequirements* TargetTagReqs,
	            FArcModifierHandle ExistingHandle,
	            uint8 Channel = 0);

	FArcModifierHandle AddLiveMod(EArcModifierOp Op, const FConstSharedStruct& LiveMagnitude,
	                              int32 StackCount, float InitialMagnitude,
	                              FMassEntityHandle Source,
	                              const FArcGameplayTagRequirements* SourceTagReqs,
	                              const FArcGameplayTagRequirements* TargetTagReqs,
	                              uint8 Channel = 0);

	bool RemoveMod(FArcModifierHandle Handle, FConstSharedStruct* OutLiveMagnitude = nullptr);

	bool IsEmpty() const;

	void RefreshLiveMagnitudes(const FArcAggregationContext& Context) const;

	void UpdateQualifications(const FGameplayTagContainer& TargetTags,
	                          FMassEntityManager& EntityManager) const;

	float Evaluate(const FArcAggregationContext& Context) const;

	float EvaluateWithTags(const FArcAggregationContext& Context,
	                       const FGameplayTagContainer& SourceTags,
	                       const FGameplayTagContainer& TargetTags) const;
};

namespace ArcAggregatorEval
{
	ARCMASSABILITIES_API float ScaleByStack(EArcModifierOp Op, float Magnitude, int32 StackCount);

	ARCMASSABILITIES_API float EvaluateChannels(
		float BaseValue,
		const TArray<FArcAggregatorMod> (&Mods)[static_cast<uint8>(EArcModifierOp::Max)],
		TFunctionRef<bool(const FArcAggregatorMod&)> Qualifies);
}

namespace ArcModifiers
{
	ARCMASSABILITIES_API FArcModifierHandle GenerateHandle();
}
