// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Mass/EntityHandle.h"
#include "ArcMass/Spatial/ArcMassSpatialHashSubsystem.h"
#include "ArcTQSPreviewEntities.h"
#include "ArcTQSPreviewTargetActor.generated.h"

class AArcTQSPreviewTargetActor;

DECLARE_MULTICAST_DELEGATE_OneParam(FArcTQSPreviewTargetMoved, AArcTQSPreviewTargetActor*);

UCLASS(hidecategories=(Rendering, Physics, Collision, Activation, Cooking, HLOD, Networking, Input))
class ARCTARGETQUERY_API AArcTQSPreviewTargetActor : public AActor
{
	GENERATED_BODY()

public:
	AArcTQSPreviewTargetActor();

	UPROPERTY(EditAnywhere, Category = "Preview")
	FGameplayTagContainer OwnedTags;

	UPROPERTY(EditAnywhere, Category = "Preview")
	FArcMassSpatialHashIndexDesc SpatialIndex;

	FArcTQSPreviewTargetMoved OnMoved;

	FMassEntityHandle GetEntity() const { return Entity; }

	virtual void PostRegisterAllComponents() override;
	virtual void UnregisterAllComponents(bool bForReregister = false) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditUndo() override;
#endif

private:
	void CreateEntity();
	void DestroyEntity();

	FArcTQSPreviewEntities PreviewEntities;
	FMassEntityHandle Entity;
};
