// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassObserverProcessor.h"
#include "MassEntityTypes.h"
#include "ArcDefaultEffectsObserver.generated.h"

class UArcEffectDefinition;

USTRUCT()
struct ARCMASSABILITIES_API FArcDefaultEffectsSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<UArcEffectDefinition>> DefaultEffects;
};

UCLASS()
class ARCMASSABILITIES_API UArcDefaultEffectsObserver : public UMassObserverProcessor
{
	GENERATED_BODY()

public:
	UArcDefaultEffectsObserver();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	FMassEntityQuery ObserverQuery;
};
