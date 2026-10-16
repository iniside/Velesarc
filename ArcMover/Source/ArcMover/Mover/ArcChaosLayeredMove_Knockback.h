// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "ArcChaosLayeredMove_Knockback.generated.h"

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcChaosLayeredMove_Knockback : public FLayeredMoveBase
{
	GENERATED_BODY()

	FArcChaosLayeredMove_Knockback();
	virtual ~FArcChaosLayeredMove_Knockback() override = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DistanceCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover", meta = (ClampMin = "0.001", ForceUnits = "s"))
	float DurationSeconds = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover")
	FVector UpDirection = FVector::UpVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ArcCore|Mover")
	bool bTerminalCleanupGenerated = false;

	bool HasValidParameters() const;

	virtual bool SupportsAsync() const override { return true; }

	virtual void OnActivated(const FLayeredMoveOnActivatedParams& Params) override;
	virtual bool GenerateMove(const FLayeredMoveGenerateMoveParams& Params, FProposedMove& OutProposedMove) override;
	virtual void OnStart_Async(UMoverBlackboard* SimBlackboard, const FMoverTime& SimTime) override;
	virtual bool GenerateMove_Async(const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, UMoverBlackboard* SimBlackboard, FProposedMove& OutProposedMove) override;
	virtual bool IsFinished(double CurrentSimTimeMs) const override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;
	virtual FString ToSimpleString() const override;

private:
	void ResetForActivation();
	void SyncNativeDuration();
};

template<>
struct TStructOpsTypeTraits<FArcChaosLayeredMove_Knockback> : public TStructOpsTypeTraitsBase2<FArcChaosLayeredMove_Knockback>
{
	enum
	{
		WithCopy = true
	};
};
