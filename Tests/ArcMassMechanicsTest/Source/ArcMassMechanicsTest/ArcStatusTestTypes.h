// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcStatusTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcMechTest_RunForeverTaskInstanceData
{
	GENERATED_BODY()
};

USTRUCT(meta = (Hidden))
struct FArcMechTest_RunForeverTask : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcMechTest_RunForeverTaskInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

namespace ArcStatusTestTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier2);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tier3);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageFire);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageCold);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Group);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(OtherGroup);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Marker);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputA);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputB);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputC);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AbilityMelee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AbilitySpell);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(AbilityPassive);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Elite);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss);
}

