// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFeatureAction.h"
#include "InstancedActorsComponent.h"
#include "InstancedActorsData.h"
#include "InstancedActorsSubsystem.h"
#include "MassEntityTraitBase.h"
#include "SmartObjectTypes.h"
#include "Engine/DeveloperSettings.h"
#include "ArcCoreInstancedActorsSubsystem.generated.h"

class USmartObjectDefinition;

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

template<>
struct TMassFragmentTraits<FArcActorInstanceFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct FArcSmartObjectDefinitionSharedFragment : public FMassConstSharedFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	TObjectPtr<USmartObjectDefinition> SmartObjectDefinition;
};

template<>
struct TMassFragmentTraits<FArcSmartObjectDefinitionSharedFragment> final
{
	enum
	{
		AuthorAcceptsItsNotTriviallyCopyable = true
	};
};

USTRUCT()
struct FArcSmartObjectOwnerFragment : public FMassFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere)
	FSmartObjectHandle SmartObjectHandle;
};

UCLASS(MinimalAPI)
class UArcMassSmartObjectOwnerTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FArcSmartObjectDefinitionSharedFragment SmartObjectDefinition;
	
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};

UCLASS(Blueprintable, BlueprintType)
class ARCCORE_API UArcCoreInstancedActorsData : public UInstancedActorsData
{
	GENERATED_BODY()

public:
	virtual void ApplyInstanceDelta(FMassEntityManager& EntityManager, const FInstancedActorsDelta& InstanceDelta, TArray<FInstancedActorsInstanceIndex>& OutEntitiesToRemove) override;
	void SwitchInstanceVisualizationWithContext(FMassExecutionContext& Context, FInstancedActorsInstanceIndex InstanceToSwitch, uint8 NewVisualizationIndex);

	virtual void OnSpawnEntities() override;
};

UCLASS()
class AArcCoreInstancedActorsManager : public AInstancedActorsManager
{
	GENERATED_BODY()

public:
	AArcCoreInstancedActorsManager();
#if WITH_EDITOR
	virtual UInstancedActorsData* CreateNextInstanceActorData(TSubclassOf<AActor> ActorClass, const FInstancedActorsTagSet& AdditionalInstanceTags) override;
#endif
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