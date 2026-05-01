// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcAreaDebuggerDrawComponent.generated.h"

struct FArcAreaDebugDrawEntry
{
	FVector Location = FVector::ZeroVector;
	FString Label;
	FColor Color = FColor::White;
};

struct FArcAreaDebugSlotMarker
{
	FVector SlotLocation = FVector::ZeroVector;
	FColor SlotColor = FColor::White;
	FString SlotLabel;
	bool bSelected = false;
	bool bHasEntity = false;
	FVector EntityLocation = FVector::ZeroVector;
	FColor LineColor = FColor::White;
};

struct FArcAreaDebugDrawData
{
	TArray<FArcAreaDebugDrawEntry> AreaMarkers;
	bool bHasSelectedArea = false;
	FVector SelectedLocation = FVector::ZeroVector;
	FString SelectedLabel;
	FColor SelectedColor = FColor::Yellow;
	float ZOffset = 200.f;
	TArray<FArcAreaDebugSlotMarker> SlotMarkers;
	bool bDrawLabels = true;
};

UCLASS(ClassGroup = Debug)
class UArcAreaDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()
public:
	void UpdateAreaData(const FArcAreaDebugDrawData& InData);
protected:
	virtual void CollectShapes() override;
private:
	FArcAreaDebugDrawData Data;
};
