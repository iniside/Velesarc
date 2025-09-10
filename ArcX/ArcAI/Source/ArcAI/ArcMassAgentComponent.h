// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentComponent.h"
#include "ArcMassAgentComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCAI_API UArcMassAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcMassAgentComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
