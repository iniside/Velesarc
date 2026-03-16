// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcConditionTypes.h"
#include "MassEntityHandle.h"

#include "ArcConditionVolume.generated.h"

class UBoxComponent;

/**
 * A volume that periodically applies a configurable condition to overlapping Mass entities.
 */
UCLASS()
class ARCCONDITIONEFFECTS_API AArcConditionVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcConditionVolume();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void OnOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void ApplyConditionToTrackedEntities();

protected:
	UPROPERTY(EditAnywhere, Category = "Condition")
	EArcConditionType ConditionType = EArcConditionType::Burning;

	UPROPERTY(EditAnywhere, Category = "Condition")
	float Amount = 10.f;

	UPROPERTY(EditAnywhere, Category = "Condition", meta = (ClampMin = "0.01"))
	float Interval = 1.f;

	UPROPERTY(VisibleAnywhere, Category = "Collision")
	TObjectPtr<UBoxComponent> CollisionBox;

private:
	TSet<FMassEntityHandle> TrackedEntities;
	FTimerHandle TimerHandle;
};
