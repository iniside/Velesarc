// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "UObject/Object.h"
#include "InteractableTargetInterface.h"
#include "InteractionTypes.h"
#include "ArcCoreAsyncMessageTypes.generated.h"

USTRUCT(BlueprintType)
struct ARCCORE_API FArcCoreGenericTagMessage
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Tag;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionAcquiredMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TScriptInterface<IInteractionTarget> Target;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FInteractionQueryResults QueryResults;
};

USTRUCT(BlueprintType)
struct ARCCORE_API FArcInteractionInvalidatedMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	TScriptInterface<IInteractionTarget> PreviousTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FInteractionQueryResults PreviousQueryResults;
};

namespace Arcx::InteractionMessages
{
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Acquired);
	ARCCORE_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Interaction_Invalidated);
}
