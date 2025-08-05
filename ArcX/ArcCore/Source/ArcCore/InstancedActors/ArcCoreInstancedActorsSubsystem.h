// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "InstancedActorsData.h"
#include "InstancedActorsSubsystem.h"
#include "Engine/DeveloperSettings.h"
#include "ArcCoreInstancedActorsSubsystem.generated.h"


USTRUCT()
struct FArcActorInstanceTag : public FMassTag
{
	GENERATED_BODY();
};

USTRUCT()
struct FArcActorInstanceFragment : public FMassFragment 
{
	GENERATED_BODY()

	UPROPERTY()
	uint8 bActive:1 = true;

	UPROPERTY()
	float RespawnTime = 10.f;

	UPROPERTY()
	float CurrentRespawnTime = 10.f;

	UPROPERTY()
	TObjectPtr<UStaticMesh> CurrentMesh;
};

UCLASS(Blueprintable, BlueprintType)
class ARCCORE_API UArcCoreInstancedActorsData : public UInstancedActorsData
{
	GENERATED_BODY()

public:
	virtual void ApplyInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta, TArray<FInstancedActorsInstanceIndex>& OutEntitiesToRemove) override;
};

UCLASS()
class AArcCoreInstancedActorsManager : public AInstancedActorsManager
{
	GENERATED_BODY()

public:
	AArcCoreInstancedActorsManager();

	virtual UInstancedActorsData* CreateNextInstanceActorData(TSubclassOf<AActor> ActorClass, const FInstancedActorsTagSet& AdditionalInstanceTags) override;

	virtual AActor* ArcFindActor(const FActorInstanceHandle& Handle);
	
};
/**
 * 
 */
UCLASS()
class ARCCORE_API UArcCoreInstancedActorsSubsystem : public UInstancedActorsSubsystem
{
	GENERATED_BODY()

public:
	UArcCoreInstancedActorsSubsystem();

	TMap<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>> ActorToDataClassMap;
};

UCLASS(DefaultConfig)
class UArcCoreInstancedActorsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "ArcCore")
	TMap<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>> ActorToDataClassMap;
};

UCLASS()
class UArcGameFeatureAction_RegisterActorToInstanceData : public UGameFeatureAction
{
	GENERATED_BODY()

public:	
	//~ Begin UGameFeatureAction interface
	virtual void OnGameFeatureRegistering() override;
	virtual void OnGameFeatureUnregistering() override;
	//~ End UGameFeatureAction interface

private:

	UPROPERTY(EditAnywhere, Category=InstancedActors)
	TMap<TSubclassOf<AActor>, TSubclassOf<UInstancedActorsData>> ActorToDataClassMap;
};
