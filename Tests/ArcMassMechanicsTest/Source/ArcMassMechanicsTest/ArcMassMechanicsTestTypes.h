// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attributes/ArcAttribute.h"
#include "Effects/ArcEffectComponent.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "NativeGameplayTags.h"
#include "ArcMassMechanics/Costs/ArcCostTypes.h"
#include "ArcMassMechanics/Resources/ArcResourceTypes.h"
#include "ArcMassMechanicsTestTypes.generated.h"

struct FArcItemData;

USTRUCT(meta = (Hidden))
struct FArcMechTest_ResourceSet : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute ResourceA;

	UPROPERTY(EditAnywhere)
	FArcAttribute ResourceADelta;

	UPROPERTY(EditAnywhere)
	FArcAttribute ResourceB;

	UPROPERTY(EditAnywhere)
	FArcAttribute ResourceBDelta;

	UPROPERTY(EditAnywhere)
	FArcAttribute RegenPips;

	UPROPERTY(EditAnywhere)
	FArcAttribute DegenPips;

	UPROPERTY(EditAnywhere)
	FArcAttribute PerPip;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, ResourceA)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, ResourceADelta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, ResourceB)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, ResourceBDelta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, RegenPips)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, DegenPips)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_ResourceSet, PerPip)
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_CostSet : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Mana;

	UPROPERTY(EditAnywhere)
	FArcAttribute ManaMeta;

	UPROPERTY(EditAnywhere)
	FArcAttribute Stamina;

	UPROPERTY(EditAnywhere)
	FArcAttribute StaminaMeta;

	UPROPERTY(EditAnywhere)
	FArcAttribute TierValue;

	UPROPERTY(EditAnywhere)
	FArcAttribute TierThreshold;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CostSet, Mana)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CostSet, ManaMeta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CostSet, Stamina)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CostSet, StaminaMeta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CostSet, TierValue)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_CostSet, TierThreshold)
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_FlowSet : public FArcMassAttributesBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FArcAttribute Resource;

	UPROPERTY(EditAnywhere)
	FArcAttribute GainMeta;

	UPROPERTY(EditAnywhere)
	FArcAttribute UnhandledMeta;

	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_FlowSet, Resource)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_FlowSet, GainMeta)
	ARC_MASS_ATTRIBUTE_ACCESSORS(FArcMechTest_FlowSet, UnhandledMeta)
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_SourceItemProbe : public FArcEffectComponent
{
	GENERATED_BODY()

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_FixedTarget : public FArcResourceTarget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	float Value = 0.f;

	virtual float GetTarget(const FArcResourceContext& Context) const override;
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_FixedRate : public FArcResourceRate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	float Rate = 0.f;

	virtual float GetRate(const FArcResourceContext& Context) const override;
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_FixedCost : public FArcMassAbilityCost
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	FArcAttributeRef Resource;

	UPROPERTY(EditAnywhere, Category = "Test")
	FArcAttributeRef Meta;

	UPROPERTY(EditAnywhere, Category = "Test")
	float Amount = 0.f;

	virtual bool CheckCost(const FArcMassAbilityCostContext& Context, FArcCostCheckResult* OutResult = nullptr) const override;
	virtual void ApplyCost(const FArcMassAbilityCostContext& Context) const override;
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_CountComponent : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 Slot = 0;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
	virtual void OnRemoved(const FArcEffectContext& Context, EArcEffectRemovalReason Reason) const override;
};

namespace ArcMassMechanicsTestTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Key_A);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Key_B);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier_Pos1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier_Pos2);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier_Pos3);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier_Neg1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier_Neg2);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier_Neg3);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(KeyB_Tier1);
}

namespace ArcMassMechanicsTest
{
	enum ESlot : int32
	{
		SlotPos1,
		SlotPos2,
		SlotPos3,
		SlotNeg1,
		SlotNeg2,
		SlotNeg3,
		SlotExtremePos,
		SlotExtremeNeg,
		SlotInstant,
		SlotKeyBTier1,
		SlotTier0,
		SlotTaskTier0,
		SlotTaskTier1,
		SlotTaskTier2,
		SlotTaskTier3,
		SlotTaskDecoyKey
	};

	constexpr int32 SlotCount = 24;

	extern int32 AppliedCounts[SlotCount];
	extern int32 RemovedCounts[SlotCount];

	extern int32 SourceProbeCount;
	extern const FArcItemData* LastProbedSourceItem;

	void ResetCounts();
	int32 TotalApplied();
	int32 TotalRemoved();
}

template <>
struct TMassFragmentTraits<FArcMechTest_ResourceSet> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

template <>
struct TMassFragmentTraits<FArcMechTest_CostSet> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

template <>
struct TMassFragmentTraits<FArcMechTest_FlowSet> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};
