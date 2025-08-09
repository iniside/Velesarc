// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Tasks/TargetingTask.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcBuilderComponent.generated.h"

class UStaticMesh;
class UTargetingPreset;

USTRUCT()
struct FArcBuildSocketInfo
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer SocketTags;

	UPROPERTY(EditAnywhere)
	TArray<FName> SocketNames;
};

class UArcBuilderData;
class UArcBuilderComponent;

USTRUCT()
struct ARCBUILDER_API FArcBuildRequirement
{
	GENERATED_BODY()

public:
	virtual bool CanStartPlacement(UArcBuilderData* InBuildData, UArcBuilderComponent* InBuilderComponent) const { return true; }

	virtual bool CanPlace(const FTransform& InTransform, UArcBuilderData* InBuildData, UArcBuilderComponent* InBuilderComponent) const { return true; }
	
	virtual ~FArcBuildRequirement() = default;
};

UCLASS()
class UArcBuilderData : public UDataAsset
{
	GENERATED_BODY()

public:
	// TODO:: Probably just add StaticMesh, SkinnedMesh and ActorClass, and then just implement spawning directly. YOLO.
	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AActor> ActorClass;

	UPROPERTY(EditAnywhere)
	FArcBuildSocketInfo SocketInfo;

	// Will snap, only if target object hass all these tags.
	UPROPERTY(EditAnywhere)
	FGameplayTagContainer SnappingRequiredTags;

	UPROPERTY(EditAnywhere)
	TArray<TInstancedStruct<FArcBuildRequirement>> BuildRequirements;
	
	// Only Sockets with the same name will be matched.
	UPROPERTY(EditAnywhere)
	bool bMatchSocketsByName = true;
	
	UPROPERTY(EditAnywhere)
	bool bSnapToClosestSocket = false;

	// Default size of grid cell, when we are using grid snapping. 
	UPROPERTY(EditAnywhere)
	int32 DefaultGridSize = 100;

	UPROPERTY(EditAnywhere)
	FVector ApproximateSize = FVector(100.0, 100.0, 100.0);
};

USTRUCT()
struct FArcItemFragment_BuilderData : public FArcItemFragment
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, meta = (AssetBundles = "Game"))
	TSoftObjectPtr<UArcBuilderData> BuilderData;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCBUILDER_API UArcBuilderComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<UTargetingPreset> PlacementTargetingPreset;

	FTargetingRequestHandle TargetingRequestHandle;

	FVector PlacementOffsetLocation = FVector::ZeroVector;
	FRotator PlacementOffsetRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Config")
	TObjectPtr<AActor> TemporaryPlacementActor;

	TWeakObjectPtr<UArcBuilderData> CurrentBuildData;
	
	bool bUsePlacementGrid = false;

	// If true, we will use grid relative to location of current object or location.
	bool bUseRelativeGrid = false;
	bool bAlignGridToSurfaceNormal = true;
	
	FVector RelativeGridOrigin = FVector::ZeroVector;
	FQuat RelativeGridRotation = FQuat::Identity;
	
	int32 CurrentGridSize = 100;

	FVector FinalPlacementLocation = FVector::ZeroVector;
public:
	// Sets default values for this component's properties
	UArcBuilderComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	void SetUsePlacementGrid(bool bNewUsePlacementGrid)
	{
		bUsePlacementGrid = bNewUsePlacementGrid;
	}

	void SetUseRelativeGrid(bool bInUseRelativeGrid)
	{
		bUseRelativeGrid = bInUseRelativeGrid;
	}

	void SetAlignGridToSurfaceNormal(bool bInAlignGridToSurfaceNormal)
	{
		bAlignGridToSurfaceNormal = bInAlignGridToSurfaceNormal;
	}
	
	void SetNewGridSize(int32 NewGridSize)
	{
		CurrentGridSize = NewGridSize;
	}
	int32 GetGridSize() const
	{
		return CurrentGridSize;
	}
	void SetPlacementOffsetLocation(const FVector& NewLocation);
	const FVector& GetPlacementOffsetLocation() const
	{
		return PlacementOffsetLocation;
	}
	
	void BeginPlacement(UArcBuilderData* InBuilderData);

	void HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle);

	void EndPlacement();
};

UCLASS()
class UArcTargetingTask_BuildTrace : public UTargetingTask
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Target Trace Selection | Collision Data")
	float SphereRadius = 400.f;
	
public:
	UArcTargetingTask_BuildTrace(const FObjectInitializer& ObjectInitializer);

	/** Evaluation function called by derived classes to process the targeting request */
	virtual void Execute(const FTargetingRequestHandle& TargetingHandle) const override;
};