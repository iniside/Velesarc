// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"

#include "ArcRecoilAutoFireComponent.generated.h"

class UInputAction;

UCLASS(ClassGroup = (ArcRecoil), meta = (BlueprintSpawnableComponent))
class UArcRecoilAutoFireComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcRecoilAutoFireComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc Recoil|Debug", meta = (ClampMin = "1.0"))
	float RoundsPerMinute = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arc Recoil|Debug")
	bool bAutoFire = true;

	UPROPERTY(EditAnywhere, Category = "Arc Recoil|Debug")
	TObjectPtr<UInputAction> FireAction;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool TryBindInput();
	void OnFireStarted();
	void OnFireStopped();
	void FireShots(int32 ShotCount);

	bool bInputBound = false;
	bool bTriggerHeld = false;
	bool bWarnedUnresolvedEntity = false;
	bool bWarnedInputUnbound = false;
	float ShotAccumulator = 0.0f;
	float UnboundTime = 0.0f;
};
