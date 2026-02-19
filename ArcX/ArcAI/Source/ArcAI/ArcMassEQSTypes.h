#pragma once
#include "MassActorSubsystem.h"
#include "MassEntityHandle.h"
#include "MassEQSBlueprintLibrary.h"
#include "MassNavigationTypes.h"
#include "MassStateTreeDependency.h"
#include "MassStateTreeExecutionContext.h"
#include "MassStateTreeSchema.h"
#include "MassStateTreeTypes.h"
#include "StateTreePropertyFunctionBase.h"
#include "StateTreePropertyRef.h"
#include "Components/StateTreeAIComponent.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "DataProviders/AIDataProvider.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "Generators/MassEnvQueryGenerator.h"

#include "ArcMassEQSTypes.generated.h"

USTRUCT()
struct ARCAI_API FArcMassEnvQueryRequest : public FEnvQueryRequest
{
	GENERATED_BODY()

	FMassEntityHandle EntityHandle;
};

UCLASS(MinimalAPI)
class UEnvQueryContext_MassEntityQuerier : public UEnvQueryContext
{
	GENERATED_UCLASS_BODY()
 
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

UENUM(BlueprintType)
enum class EArcMassEQSInputType : uint8
{
	Actor,
	ActorArray,
	Vector,
	VectorArray,
	Entity,
	EntityArray
};

UENUM(BlueprintType)
enum class EArcMassEQSResultType : uint8
{
	Actor,
	ActorArray,
	Vector,
	VectorArray,
	Entity,
	EntityArray
};

UCLASS(MinimalAPI, Blueprintable, EditInlineNew)
class UEnvQueryContext_MassEQSStoreBase : public UEnvQueryContext
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = "MassEQS")
	FName ResultName;

	UPROPERTY(EditAnywhere, Category = "MassEQS")
	EArcMassEQSInputType InputType;

	UPROPERTY(EditAnywhere, Category = "MassEQS")
	EArcMassEQSResultType ResultType;
	
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};



UCLASS(meta = (DisplayName = "Arc Mass Entity Handles"), MinimalAPI)
class UArcMassEnvQueryGenerator_MassEntityHandles : public UEnvQueryGenerator
{
	GENERATED_UCLASS_BODY()

public:
	// Begin IMassEQSRequestInterface
	//virtual TUniquePtr<FMassEQSRequestData> GetRequestData(FEnvQueryInstance& QueryInstance) const override;
	//virtual UClass* GetRequestClass() const override { return StaticClass(); }
	//
	//virtual bool TryAcquireResults(FEnvQueryInstance& QueryInstance) const override;
	// ~IMassEQSRequestInterface

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

protected:
	/** Any Entity who is within SearchRadius of any SearchCenter will be acquired */
	UPROPERTY(EditDefaultsOnly, Category=Generator)
	FAIDataProviderFloatValue SearchRadius;

	UPROPERTY(EditDefaultsOnly, Category=Generator)
	float Radius = 10000.f;
	
	/** Context of query */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> SearchCenter;
};





