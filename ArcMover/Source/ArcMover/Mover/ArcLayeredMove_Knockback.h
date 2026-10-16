// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "LayeredMove.h"
#include "ArcLayeredMove_Knockback.generated.h"

namespace ArcMover
{
	ARCMOVER_API bool IsKnockbackDurationRepresentable(float DurationSeconds);
	ARCMOVER_API bool HasValidKnockbackParameters(const FVector& Direction, float DistanceCm, float DurationSeconds, const FVector& UpDirection);
}

USTRUCT(BlueprintType)
struct ARCMOVER_API FArcLayeredMove_Knockback : public FLayeredMoveBase
{
	GENERATED_BODY()

	FArcLayeredMove_Knockback();
	virtual ~FArcLayeredMove_Knockback() override = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover", meta = (ClampMin = "0.0", ForceUnits = "cm"))
	float DistanceCm = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ArcCore|Mover", meta = (ClampMin = "0.001", ForceUnits = "s"))
	float DurationSeconds = 0.3f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "ArcCore|Mover")
	bool bTerminalCleanupGenerated = false;

	bool HasValidParameters(const FVector& UpDirection) const;

	virtual void OnActivated(const FLayeredMoveOnActivatedParams& Params) override;
	virtual bool GenerateMove(const FLayeredMoveGenerateMoveParams& Params, FProposedMove& OutProposedMove) override;
	virtual bool IsFinished(double CurrentSimTimeMs) const override;
	virtual FLayeredMoveBase* Clone() const override;
	virtual void NetSerialize(FArchive& Ar) override;
	virtual UScriptStruct* GetScriptStruct() const override;

private:
	bool CanRepresentNativeDuration() const;
	void SyncNativeDuration();
};

template<>
struct TStructOpsTypeTraits<FArcLayeredMove_Knockback> : public TStructOpsTypeTraitsBase2<FArcLayeredMove_Knockback>
{
	enum
	{
		WithCopy = true
	};
};
