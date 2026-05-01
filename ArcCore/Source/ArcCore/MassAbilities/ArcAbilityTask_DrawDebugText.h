// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/ArcAbilityTaskBase.h"
#include "ArcAbilityTask_DrawDebugText.generated.h"

USTRUCT()
struct FArcAbilityTask_DrawDebugTextInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FString Text = TEXT("Debug");

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FVector Offset = FVector(0.f, 0.f, 100.f);

	UPROPERTY(EditAnywhere, Category = "Parameter")
	FColor Color = FColor::Yellow;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float Duration = 0.f;

	UPROPERTY(EditAnywhere, Category = "Parameter")
	float FontScale = 1.f;
};

USTRUCT(DisplayName = "Draw Debug Text", Category = "Arc|Ability|Debug")
struct ARCCORE_API FArcAbilityTask_DrawDebugText : public FArcAbilityTaskBase
{
	GENERATED_BODY()

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FArcAbilityTask_DrawDebugTextInstanceData::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
