// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSFunctions_Vector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSFunctions_Vector)

void FArcTQSFunction_VectorAdd::Execute(const FArcTQSFunctionContext& Context)
{
	Result = A + B;
}

void FArcTQSFunction_VectorSubtract::Execute(const FArcTQSFunctionContext& Context)
{
	Result = A - B;
}

void FArcTQSFunction_VectorScale::Execute(const FArcTQSFunctionContext& Context)
{
	Result = Vector * Scale;
}

void FArcTQSFunction_VectorOffset::Execute(const FArcTQSFunctionContext& Context)
{
	Result = Location + Direction * Distance;
}

void FArcTQSFunction_VectorDirection::Execute(const FArcTQSFunctionContext& Context)
{
	FVector Delta = B - A;
	if (b2D)
	{
		Delta.Z = 0.0;
	}
	Result = Delta.GetSafeNormal();
}

void FArcTQSFunction_VectorDistance::Execute(const FArcTQSFunctionContext& Context)
{
	Result = static_cast<float>(b2D ? FVector::Dist2D(A, B) : FVector::Dist(A, B));
}

void FArcTQSFunction_MakeVector::Execute(const FArcTQSFunctionContext& Context)
{
	Result = FVector(X, Y, Z);
}
