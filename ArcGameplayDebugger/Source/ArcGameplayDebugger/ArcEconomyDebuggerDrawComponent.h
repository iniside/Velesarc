// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"

#include "ArcEconomyDebuggerDrawComponent.generated.h"

struct FArcEconomyDebugDrawData
{
	struct FSettlementMarker
	{
		FVector Location = FVector::ZeroVector;
		float Radius = 0.f;
		FString Label;
		FColor Color = FColor::White;
		bool bSelected = false;
	};

	struct FBuildingMarker
	{
		FVector Location = FVector::ZeroVector;
		FString Label;
	};

	struct FNPCMarker
	{
		FVector Location = FVector::ZeroVector;
		FColor Color = FColor::White;
	};

	struct FLink
	{
		FVector Start = FVector::ZeroVector;
		FVector End = FVector::ZeroVector;
		FColor Color = FColor::White;
	};

	struct FSOSlotMarker
	{
		FVector Location = FVector::ZeroVector;
		FColor Color = FColor::White;
		FString Label; // state + tags summary
	};

	struct FResourceMarker
	{
		FVector Location = FVector::ZeroVector;
		FColor Color = FColor::White;
		FString Label;
	};

	TArray<FSettlementMarker> SettlementMarkers;
	TArray<FBuildingMarker> BuildingMarkers;
	TArray<FNPCMarker> NPCMarkers;
	TArray<FLink> Links;
	TArray<FSOSlotMarker> SOSlotMarkers;
	TArray<FResourceMarker> ResourceMarkers;
	bool bDrawLabels = true;

	// Selected entity highlight
	bool bHasSelectedBuilding = false;
	FVector SelectedBuildingLocation = FVector::ZeroVector;
	float SelectedBuildingSearchRadius = 0.f;

	bool bHasSelectedNPC = false;
	FVector SelectedNPCLocation = FVector::ZeroVector;

	FVector PlayerLocation = FVector::ZeroVector;
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
