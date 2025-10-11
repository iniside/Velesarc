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

	virtual void SetEntityHandle(const FMassEntityHandle NewHandle) override;
};
