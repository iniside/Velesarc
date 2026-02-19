// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassSignalProcessorBase.h"
#include "UObject/Interface.h"
#include "ArcMassHealthSignalProcessor.generated.h"

namespace UE::ArcMass::Signals
{
	const FName HealthChanged = FName(TEXT("HealthChanged"));
}

UINTERFACE(BlueprintType, MinimalAPI)
class UArcMassHealthObserverInterface : public UInterface
{
	GENERATED_BODY()
};

class ARCMASS_API IArcMassHealthObserverInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "ArcMass|Health")
	void OnHealthUpdated(AActor* Actor, float CurrentHealth, float MaxHealth, double CurrentTime, double LastUpdateTime);
};

UCLASS()
class ARCMASS_API UArcMassHealthSignalProcessor : public UMassSignalProcessorBase
{
	GENERATED_BODY()

public:
	UArcMassHealthSignalProcessor();

protected:
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void SignalEntities(FMassEntityManager& EntityManager, FMassExecutionContext& Context, FMassSignalNameLookup& EntitySignals) override;
};
