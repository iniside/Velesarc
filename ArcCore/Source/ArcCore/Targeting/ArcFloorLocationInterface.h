// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "ArcFloorLocationInterface.generated.h"

UINTERFACE(BlueprintType, MinimalAPI)
class UArcFloorLocationInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCCORE_API IArcFloorLocationInterface
{
	GENERATED_BODY()

public:
	/** Returns the floor location for this actor. Different actors may compute this differently. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Arc Core|Targeting")
	FVector GetFloorLocation() const;
};
