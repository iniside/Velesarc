// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassProcessor.h"
#include "ArcMassStateTreeTickProcessor.generated.h"

class UMassStateTreeSubsystem;

USTRUCT()
struct FArcMassTickStateTreeTag : public FMassTag
{
	GENERATED_BODY()
	
};

/**
 * 
 */
UCLASS()
class ARCAI_API UArcMassStateTreeTickProcessor : public UMassProcessor
{
	GENERATED_BODY()
	
	UArcMassStateTreeTickProcessor();

	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void InitializeInternal(UObject& Owner, const TSharedRef<FMassEntityManager>& ) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

	FMassEntityQuery EntityQuery_Conditional;
	
	UPROPERTY(Transient)
	TObjectPtr<UMassStateTreeSubsystem> MassStateTreeSubsystem = nullptr;
};
