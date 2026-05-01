// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcDebugDrawComponent.h"
#include "ArcIWMinimapDebuggerDrawComponent.generated.h"

struct FArcIWMinimapDebugDrawEntry
{
	FVector Position = FVector::ZeroVector;
	FColor Color = FColor::White;
};

UCLASS(ClassGroup = Debug)
class UArcIWMinimapDebuggerDrawComponent : public UArcDebugDrawComponent
{
	GENERATED_BODY()

public:
	void UpdateEntities(TArrayView<const FArcIWMinimapDebugDrawEntry> InEntries);

protected:
	virtual void CollectShapes() override;

private:
	TArray<FArcIWMinimapDebugDrawEntry> Entries;
};
