// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "ArcAIPerceptionComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FArcPerceptionUpdatedDelegate, const TArray<AActor*>&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FArcActorPerceptionUpdatedDelegate, AActor*, FAIStimulus);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcActorPerceptionForgetUpdatedDelegate, AActor*);
DECLARE_MULTICAST_DELEGATE_OneParam(FArcActorPerceptionInfoUpdatedDelegate, const FActorPerceptionUpdateInfo&);
DECLARE_MULTICAST_DELEGATE(FArcPerceptionChangedDelegate);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class ARCAI_API UArcAIPerceptionComponent : public UAIPerceptionComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UArcAIPerceptionComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	FArcPerceptionUpdatedDelegate NativeOnPerceptionUpdated;
	FArcActorPerceptionUpdatedDelegate NativeOnTargetPerceptionUpdated;
	FArcActorPerceptionForgetUpdatedDelegate NativeOnTargetPerceptionForgotten;
	FArcActorPerceptionInfoUpdatedDelegate NativeOnTargetPerceptionInfoUpdated;
	FArcPerceptionChangedDelegate NativeOnPerceptionChanged;
	
protected:
	UFUNCTION()
	void HandleOnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	UFUNCTION()
	void HandleOnTargetPerceptionForgotten(AActor* Actor);
	
	UFUNCTION()
	void HandleOnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	UFUNCTION()
	void HandleOnTargetPerceptionInfoUpdated(const FActorPerceptionUpdateInfo& UpdateInfo);
};
