// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentComponent.h"

#include "ArcUIModelsTestAgentComponent.generated.h"

/** Mass agent component whose entity a test associates directly, without the agent spawn pipeline. */
UCLASS(Hidden, NotBlueprintable)
class UArcUIModelsTestAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

public:
	void AssociateEntityForTest(FMassEntityHandle Entity)
	{
		SetEntityHandleInternal(Entity);
	}
};
