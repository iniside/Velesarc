// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArcSettlementTypes.h"
#include "ArcSettlementVolume.generated.h"

class USphereComponent;
class UArcSettlementDefinition;

/**
 * Level-placed actor that defines a settlement's spatial bounds.
 * Registers with UArcSettlementSubsystem on BeginPlay, cleans up on EndPlay.
 *
 * The sphere component defines the settlement's bounding radius.
 * Place one per settlement in the level.
 */
UCLASS()
class ARCSETTLEMENT_API AArcSettlementVolume : public AActor
{
	GENERATED_BODY()

public:
	AArcSettlementVolume();

	/** Settlement definition data asset — tags, initial knowledge, display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Settlement")
	TObjectPtr<UArcSettlementDefinition> Definition;

	/** Visual/spatial representation of the settlement bounds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Settlement")
	TObjectPtr<USphereComponent> SphereComponent;

	/** The runtime handle assigned by the subsystem. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Settlement")
	FArcSettlementHandle SettlementHandle;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
