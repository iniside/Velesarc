#pragma once
#include "MassEntityHandle.h"
#include "StateTreeEvents.h"

#include "ArcMassStateTreeFunctionLibrary.generated.h"

class UObject;
class AActor;

USTRUCT(BlueprintType)
struct FArcStopMassStateTreeEvent
{
	GENERATED_BODY()
};

UCLASS()
class UArcMassStateTreeFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	static void SendStateTreeEvent(UObject* WorldContext, const FStateTreeEvent& Event, const TArray<FMassEntityHandle>& EntityHandles);

	UFUNCTION(BlueprintCallable, Category = "Arc|Mass")
	static void SendStateTreeEventToActorEntities(UObject* WorldContext, const FStateTreeEvent& Event, const TArray<AActor*>& Actors);
};