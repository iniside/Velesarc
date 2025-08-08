// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/Fragments/ArcItemFragment.h"
#include "Types/TargetingSystemTypes.h"
#include "ArcBuilderComponent.generated.h"

class UStaticMesh;
class UTargetingPreset;

UCLASS()
class UArcBuilderData : public UDataAsset
{
	GENERATED_BODY()

public:
	// TODO:: Probably just add StaticMesh, SkinnedMesh and ActorClass, and then just implement spawning directly. YOLO.
	UPROPERTY(EditAnywhere)
	TSoftClassPtr<AActor> ActorClass;
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
public:
	// Sets default values for this component's properties
	UArcBuilderComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	void BeginPlacement(UArcBuilderData* InBuilderData);

	void HandleTargetingCompleted(FTargetingRequestHandle InTargetingRequestHandle);

	void EndPlacement();
};
