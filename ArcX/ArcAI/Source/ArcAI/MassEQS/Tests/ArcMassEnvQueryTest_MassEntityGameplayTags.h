// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Processors/MassEnvQueryProcessorBase.h"
#include "Tests/MassEnvQueryTest.h"
#include "ArcMassEnvQueryTest_MassEntityGameplayTags.generated.h"

USTRUCT()
struct FMyStruct_TestDumm
{
	GENERATED_BODY()
	
};
/**
 * 
 */

UCLASS()
class UArcMassEnvQueryTest_MassEntityGameplayTags : public UMassEnvQueryTest
{
	GENERATED_BODY()
	
protected:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer HasTags;

	UPROPERTY(EditAnywhere)
	FGameplayTagContainer MustNotHaveTags;
	
public:
	UArcMassEnvQueryTest_MassEntityGameplayTags(const FObjectInitializer& ObjectInitializer);
	
	virtual TUniquePtr<FMassEQSRequestData> GetRequestData(FEnvQueryInstance& QueryInstance) const override;
	virtual UClass* GetRequestClass() const override { return StaticClass(); }

	virtual bool TryAcquireResults(FEnvQueryInstance& QueryInstance) const override;
};


struct FMassEQSRequestData_MassEntityGameplayTags : public FMassEQSRequestData
{
	FMassEQSRequestData_MassEntityGameplayTags(const FGameplayTagContainer& InHasTags, const FGameplayTagContainer& InMustNotHaveTags)
		: HasTags(InHasTags)
		, MustNotHaveTags(InMustNotHaveTags)
	{
	}
	
	FGameplayTagContainer HasTags;
	FGameplayTagContainer MustNotHaveTags;
};


struct FMassEnvQueryResultData_MassEntityGameplayTags : public FMassEQSRequestData
{
	FMassEnvQueryResultData_MassEntityGameplayTags(TArray<FMassEntityHandle> && InResultMap)
		: EntityHandles(MoveTemp(InResultMap))
	{
	}

	TArray<FMassEntityHandle> EntityHandles;
};


UCLASS(meta = (DisplayName = "ARc Mass Entity Gameplay Tags Test Processor"))
class UArcMassEnvQueryTestProcessor_MassEntityGameplayTags : public UMassEnvQueryProcessorBase
{
	GENERATED_BODY()
public:
	UArcMassEnvQueryTestProcessor_MassEntityGameplayTags();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;
	virtual bool ShouldAllowQueryBasedPruning(const bool bRuntimeMode = true) const override { return false; }

	FMassEntityQuery EntityQuery;
};
