// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"

#include "ArcEconomyDebuggerDrawComponent.generated.h"

struct FArcEconomyDebugDrawData
{
	struct FBuildingMarker
	{
		FVector Location = FVector::ZeroVector;
		FString Label;
		bool bSelected = false;
	};

	struct FNPCMarker
	{
		FVector Location = FVector::ZeroVector;
		FColor Color = FColor::White;
		bool bSelected = false;
	};

	struct FLink
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FColor Color = FColor::White;
	};

	TArray<FBuildingMarker> BuildingMarkers;
	TArray<FNPCMarker> NPCMarkers;
	TArray<FLink> Links;
	bool bDrawLabels = true;
};

UCLASS(ClassGroup = Debug)
class UArcEconomyDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateData(const FArcEconomyDebugDrawData& InData);

protected:
	virtual void CollectShapes() override;

private:
	FArcEconomyDebugDrawData Data;
};
