// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "MovementModifier.h"

#include "ArcLayeredMove_Root.generated.h"

class UMoverComponent;
struct FArcMovementModifier_Root;

namespace ArcMover
{
	ARCMOVER_API FMovementModifierHandle QueueRoot(UMoverComponent& Mover, TSharedPtr<FArcMovementModifier_Root> Marker);
}

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcLayeredMove_Root : public FLayeredMoveBase
{
	GENERATED_BODY()

	FArcLayeredMove_Root();
	virtual ~FArcLayeredMove_Root() override = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcMover|Mover")
	FGuid SourceId;

	virtual bool GenerateMove(const FLayeredMoveGenerateMoveParams& Params, FProposedMove& OutProposedMove) override;
	virtual bool SupportsAsync() const override { return true; }
	virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;

private:
	bool GenerateRootMove(const FMoverTickStartData& StartState, FProposedMove& OutProposedMove);
};

template<>
struct TStructOpsTypeTraits<FArcLayeredMove_Root> : public TStructOpsTypeTraitsBase2<FArcLayeredMove_Root>
{
	enum
	{
		WithCopy = true
	};
};
