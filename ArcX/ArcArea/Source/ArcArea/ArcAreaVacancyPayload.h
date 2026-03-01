// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ArcAreaTypes.h"
#include "ArcKnowledgePayload.h"
#include "ArcAreaVacancyPayload.generated.h"

/**
 * FInstancedStruct payload attached to vacancy knowledge entries posted to ArcKnowledge.
 * Allows NPCs querying the knowledge system to identify which area/slot a vacancy belongs to.
 */
USTRUCT(BlueprintType)
struct ARCAREA_API FArcAreaVacancyPayload : public FArcKnowledgePayload
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vacancy")
	FArcAreaHandle AreaHandle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vacancy")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vacancy")
	FGameplayTag RoleTag;
};
