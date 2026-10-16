// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArcEnviroEffectsSubsystem.generated.h"

class AArcEnviroVolume;

struct ARCENVIROEFFECTS_API FArcEnviroLevelOverrides
{
	TOptional<int32> Rain;
	TOptional<int32> Cold;
	TOptional<int32> Heat;
};

UCLASS()
class ARCENVIROEFFECTS_API UArcEnviroEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void RegisterVolume(AArcEnviroVolume* Volume);
	void UnregisterVolume(AArcEnviroVolume* Volume);

	FArcEnviroLevelOverrides GetOverridesAtLocation(const FVector& Location) const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AArcEnviroVolume>> Volumes;
};
