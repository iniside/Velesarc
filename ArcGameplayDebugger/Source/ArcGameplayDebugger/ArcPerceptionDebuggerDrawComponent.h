// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcPerceptionDebuggerDrawComponent.generated.h"

struct FArcPerceptionDebugEntityMarker
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	FColor Color = FColor::Cyan;
};

struct FArcPerceptionDebugSenseShape
{
	enum class EType : uint8 { Sphere, Cone };
	EType Type = EType::Sphere;
	FVector Origin = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	float Radius = 0.f;
	float ConeHalfAngleDegrees = 0.f;
	float ConeLength = 0.f;
	FColor Color = FColor::White;
};

struct FArcPerceptionDebugPerceivedEntity
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	FColor Color = FColor::White;
	float SphereRadius = 30.f;
	float Thickness = 1.5f;
	float ZOffset = 0.f;
	bool bHighlighted = false;
};

struct FArcPerceptionDebugDrawData
{
	FArcPerceptionDebugEntityMarker EntityMarker;
	TArray<FArcPerceptionDebugSenseShape> SenseShapes;
	TArray<FArcPerceptionDebugPerceivedEntity> PerceivedEntities;
	bool bDrawSenseShapes = true;
	bool bDrawPerceivedPositions = true;
};

UCLASS(ClassGroup = Debug)
class UArcPerceptionDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()
public:
	void UpdatePerceptionData(const FArcPerceptionDebugDrawData& InData);
protected:
	virtual void CollectShapes() override;
private:
	FArcPerceptionDebugDrawData Data;
};
