#pragma once

#include "ArcTQSPropertyFunction.h"

#include "ArcTacticsFunctionTestTypes.generated.h"

USTRUCT(meta = (Hidden))
struct FArcTacticsTestAccumulateFunction : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	float Amount = 2.5f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	float Result = 0.0f;

	virtual void Execute(const FArcTQSFunctionContext&) override
	{
		Result += Amount;
	}
};
