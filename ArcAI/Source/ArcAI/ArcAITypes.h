// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "ArcAITypes.generated.h"

class UStateTree;
/**
 * 
 */
USTRUCT(BlueprintType)
struct ARCAI_API FArcAIBaseEvent
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct ARCAI_API FArcAIStateTreeEvent : public FArcAIBaseEvent
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UStateTree> StateTree;
};