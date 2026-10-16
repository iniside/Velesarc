// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcTQSPropertyFunction.h"
#include "ArcTQSFunctions_Vector.generated.h"

/** A + B. */
USTRUCT(meta = (DisplayName = "Vector Add", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_VectorAdd : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector A = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector B = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** A - B. */
USTRUCT(meta = (DisplayName = "Vector Subtract", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_VectorSubtract : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector A = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector B = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Vector multiplied by Scale. */
USTRUCT(meta = (DisplayName = "Vector Scale", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_VectorScale : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Vector = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Scale = 1.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Location + Direction * Distance. */
USTRUCT(meta = (DisplayName = "Vector Offset", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_VectorOffset : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector Direction = FVector::ForwardVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	float Distance = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Normalized direction from A to B, or zero when the two points coincide; b2D ignores height. */
USTRUCT(meta = (DisplayName = "Vector Direction", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_VectorDirection : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector A = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector B = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool b2D = false;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Distance between A and B; b2D ignores height. */
USTRUCT(meta = (DisplayName = "Vector Distance", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_VectorDistance : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector A = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	FVector B = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Input")
	bool b2D = false;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	float Result = 0.0f;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};

/** Vector built from X, Y and Z. */
USTRUCT(meta = (DisplayName = "Make Vector", Category = "Vector"))
struct ARCTARGETQUERY_API FArcTQSFunction_MakeVector : public FArcTQSPropertyFunction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Input")
	double X = 0.0;

	UPROPERTY(EditAnywhere, Category = "Input")
	double Y = 0.0;

	UPROPERTY(EditAnywhere, Category = "Input")
	double Z = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "Output", meta = (ArcTQSOutput))
	FVector Result = FVector::ZeroVector;

	virtual void Execute(const FArcTQSFunctionContext& Context) override;
};
