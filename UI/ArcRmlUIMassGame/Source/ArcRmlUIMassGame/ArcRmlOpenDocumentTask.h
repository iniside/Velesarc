// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcGameplayInteractionStateTreeBases.h"
#include "ArcRmlDocumentRef.h"
#include "Mass/EntityHandle.h"

#include "ArcRmlOpenDocumentTask.generated.h"

class ULocalPlayer;

USTRUCT()
struct FArcRmlOpenDocumentTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Context")
	FMassEntityHandle ContextEntity;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FArcRmlDocumentRef Document;

	UPROPERTY()
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;

	UPROPERTY()
	bool bTracking = false;

	UPROPERTY()
	bool bOwnsDocument = false;
};

USTRUCT(meta = (DisplayName = "Arc Open RML Document", Category = "Gameplay Interactions"))
struct ARCRMLUIMASSGAME_API FArcRmlOpenDocumentTask : public FArcGameplayInteractionMassTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FArcRmlOpenDocumentTaskInstanceData;

	FArcRmlOpenDocumentTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
