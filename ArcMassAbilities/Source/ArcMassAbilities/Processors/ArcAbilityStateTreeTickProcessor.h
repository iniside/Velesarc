// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "ArcAbilityStateTreeTickProcessor.generated.h"

class UArcAbilityStateTreeSubsystem;

UCLASS()
class ARCMASSABILITIES_API UArcAbilityStateTreeTickProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcAbilityStateTreeTickProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UArcAbilityStateTreeSubsystem> AbilitySubsystem = nullptr;
};
