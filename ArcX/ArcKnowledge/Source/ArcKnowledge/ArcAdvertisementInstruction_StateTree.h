// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcAdvertisementInstruction.h"
#include "StateTreeReference.h"
#include "ArcAdvertisementInstruction_StateTree.generated.h"

/**
 * Advertisement instruction that carries a StateTree to execute.
 * The claiming entity runs this StateTree when fulfilling the advertisement.
 */
USTRUCT(BlueprintType, DisplayName = "StateTree Instruction")
struct ARCKNOWLEDGE_API FArcAdvertisementInstruction_StateTree : public FArcAdvertisementInstruction
{
	GENERATED_BODY()

	/** The StateTree to run when fulfilling this advertisement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Instruction",
		meta = (Schema = "/Script/ArcKnowledge.UArcAdvertisementStateTreeSchema"))
	FStateTreeReference StateTreeReference;
};
