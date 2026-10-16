// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcTacticsDebuggerDrawComponent.generated.h"

struct FArcTacticsDebugDrawEntity
{
	FVector Location = FVector::ZeroVector;
	float Radius = 0.0f;
	FColor Color = FColor::White;
	FString Label;
};

struct FArcTacticsDebugDrawLine
{
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	FColor Color = FColor::White;
	float Thickness = 0.0f;
};

struct FArcTacticsDebugDrawPoint
{
	FVector Location = FVector::ZeroVector;
	float Size = 0.0f;
	FColor Color = FColor::White;
	FString Label;
};

struct FArcTacticsDebugDrawPolyline
{
	TArray<FVector> Points;
	FColor Color = FColor::White;
	float Thickness = 0.0f;
};

struct FArcTacticsDebugDrawData
{
	TArray<FArcTacticsDebugDrawEntity> Entities;
	TArray<FArcTacticsDebugDrawLine> Lines;
	TArray<FArcTacticsDebugDrawPoint> Points;
	TArray<FArcTacticsDebugDrawPolyline> Polylines;

	bool IsEmpty() const
	{
		return Entities.IsEmpty() && Lines.IsEmpty() && Points.IsEmpty() && Polylines.IsEmpty();
	}
};

UCLASS(ClassGroup = Debug)
class UArcTacticsDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void SetShapes(const FArcTacticsDebugDrawData& InData);

protected:
	virtual void CollectShapes() override;

private:
	FArcTacticsDebugDrawData Data;
};
