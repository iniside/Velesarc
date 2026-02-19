#pragma once
#include "GameplayTagContainer.h"
#include "MassEQSBlueprintLibrary.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcMassEQSResultStore.generated.h"
USTRUCT()
struct ARCAI_API FArcMassEQSResults
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	TObjectPtr<AActor> ActorResult;
	
	UPROPERTY(EditAnywhere)
	TArray<TObjectPtr<AActor>> ActorArrayResult;

	UPROPERTY(EditAnywhere)
	FVector VectorResult = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere)
	TArray<FVector> VectorArrayResult;

	UPROPERTY(EditAnywhere)
	FMassEnvQueryEntityInfoBlueprintWrapper EntityResult;

	UPROPERTY(EditAnywhere)
	TArray<FMassEnvQueryEntityInfoBlueprintWrapper> EntityArrayResult;
};

USTRUCT()
struct FArcMassEQSResultTypeMap
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FName, FArcMassEQSResults> Results;
};

UCLASS()
class UArcMassEQSResultStore : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	UPROPERTY()
	TMap<FMassEntityHandle, FArcMassEQSResultTypeMap> EQSResults;
};

USTRUCT()
struct FArcMassDataStoreTypeMap
{
	GENERATED_BODY()

	UPROPERTY()
	TMap<FGameplayTag, FArcMassEQSResults> Results;
};

UCLASS()
class UArcMassGlobalDataStore : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

	UPROPERTY()
	TMap<FMassEntityHandle, FArcMassDataStoreTypeMap> DataStore;
};