// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Effects/ArcEffectComponent.h"
#include "NativeGameplayTags.h"
#include "ArcRPGMechanicsTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcRPGMechTest_CountComponent : public FArcEffectComponent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Test")
	int32 Slot = 0;

	virtual void OnApplied(const FArcEffectContext& Context) const override;
};

namespace ArcRPGMechanicsTestTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Input);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Domain_A);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Domain_B);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SlotTorso);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SlotShield);
}

namespace ArcRPGMechanicsTest
{
	enum ESlot : int32
	{
		SlotDomainATier1,
		SlotDomainATier2,
		SlotDomainATier3,
		SlotDomainBTier2
	};

	constexpr int32 SlotCount = 8;

	extern int32 AppliedCounts[SlotCount];

	void ResetCounts();
	int32 TotalApplied();
}
