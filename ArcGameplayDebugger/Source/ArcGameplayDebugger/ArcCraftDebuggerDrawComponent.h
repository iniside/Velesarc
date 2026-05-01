// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcCraftDebuggerDrawComponent.generated.h"

struct FArcCraftDebugDrawData
{
	bool bHasSelectedStation = false;
	FVector SelectedStationLocation = FVector::ZeroVector;
	FVector PlayerLocation = FVector::ZeroVector;
};

UCLASS(ClassGroup = Debug)
class UArcCraftDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateData(const FArcCraftDebugDrawData& InData);

protected:
	virtual void CollectShapes() override;

private:
	FArcCraftDebugDrawData Data;
};
