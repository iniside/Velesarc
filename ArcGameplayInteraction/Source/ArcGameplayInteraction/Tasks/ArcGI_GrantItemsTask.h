// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayInteractionsTypes.h"
#include "Mass/EntityHandle.h"

#include "ArcGI_GrantItemsTask.generated.h"

USTRUCT()
struct FArcGI_GrantItemsTaskInstanceData
{
	GENERATED_BODY()

	/** Entity owning FArcItemGrantFragment (source). */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle SmartObjectEntity;

	/** Entity owning the destination FArcMassItemStoreFragment (or a derived store). */
	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle ContextEntity;

	/** Which item store fragment on the destination entity should receive the items.
	 *  Must derive from FArcMassItemStoreFragment. Defaults to the base store at runtime. */
	UPROPERTY(EditAnywhere, Category = "Context", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TObjectPtr<UScriptStruct> StoreType = nullptr;
};

USTRUCT(meta = (DisplayName = "Grant Items From Definitions", Category = "Gameplay Interactions"))
struct ARCGAMEPLAYINTERACTION_API FArcGI_GrantItemsTask : public FGameplayInteractionStateTreeTask
{
	GENERATED_BODY()

	using FInstanceDataType = FArcGI_GrantItemsTaskInstanceData;

	FArcGI_GrantItemsTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
