// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassAgentComponent.h"
#include "Mass/EntityHandle.h"
#include "ArcMassAgentComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCAI_API UArcMassAgentComponent : public UMassAgentComponent
{
	GENERATED_BODY()

public:
	UArcMassAgentComponent();

	virtual void SetEntityHandle(const FMassEntityHandle NewHandle) override;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Handle returned by UArcMassSpawnerSubsystem::SpawnForAgentComponent; used for EndPlay teardown. */
	FMassEntityHandle SpawnedHandle;
};
