// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "ArcMassEntityHandleWrapper.h"
#include "ArcMassEntityLibrary.generated.h"

class UMassAgentComponent;

UENUM()
enum class EArcMassResult : uint8
{
	Valid,
	NotValid,
};

/**
 * Blueprint function library for interacting with Mass entities.
 */
UCLASS(MinimalAPI)
class UArcMassEntityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/**
	 * Gets a fragment value from the given Mass entity.
	 * The output pin type is determined by the connected struct type.
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = "ArcMass|Entity",
		meta = (WorldContext = "WorldContextObject", CustomStructureParam = "OutFragment", ExpandEnumAsExecs = "ExecResult"))
	static ARCMASS_API void GetEntityFragment(const UObject* WorldContextObject,
		const FMassEntityHandle& Entity, EArcMassResult& ExecResult, int32& OutFragment);

private:
	DECLARE_FUNCTION(execGetEntityFragment);
	
public:
	UFUNCTION(BlueprintCallable, Category = "ArcMass")
	static ARCMASS_API FMassEntityHandle GetEntityFromComponent(const UMassAgentComponent* Component);
};
