// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityHandle.h"
#include "Components/ActorComponent.h"
#include "ArcVisEntityComponent.generated.h"

class UMassEntityConfigAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArcVisActorEvent, AActor*, Actor);

/**
 * Component for actors managed by the entity visualization system.
 *
 * When placed on a level actor with an EntityConfig, BeginPlay registers
 * the actor with Mass as an entity already in Actor representation.
 *
 * When the visualization system spawns/destroys actors during ISM<->Actor
 * swaps, it calls OnVisActorCreated / OnVisActorPreDestroy on this component.
 */
UCLASS(BlueprintType, ClassGroup = "ArcMass", meta = (BlueprintSpawnableComponent))
class ARCMASS_API UArcVisEntityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArcVisEntityComponent();

	/** Entity config asset used when this actor is pre-placed on a level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	TObjectPtr<const UMassEntityConfigAsset> EntityConfig;

	/** Called after the actor has been created by the visualization system (ISM → Actor swap). */
	UPROPERTY(BlueprintAssignable, Category = "Visualization")
	FArcVisActorEvent OnVisActorCreated;

	/** Called before the actor is about to be destroyed by the visualization system (Actor → ISM swap). */
	UPROPERTY(BlueprintAssignable, Category = "Visualization")
	FArcVisActorEvent OnVisActorPreDestroy;

	/** The Mass entity handle associated with this actor. */
	UFUNCTION(BlueprintCallable, Category = "Visualization")
	FMassEntityHandle GetEntityHandle() const { return EntityHandle; }

	/** Called by the activate processor after spawning this actor. */
	virtual void NotifyVisActorCreated(FMassEntityHandle InEntityHandle);

	/** Called by the deactivate processor before destroying this actor. */
	virtual void NotifyVisActorPreDestroy();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Register a pre-placed actor with the Mass entity system. */
	void RegisterPrePlacedActor();

	/** Clean up a Mass entity we own (pre-placed actor path). */
	void UnregisterPrePlacedActor();

	FMassEntityHandle EntityHandle;

	/** True if this component created the Mass entity (pre-placed actor). */
	bool bOwnsEntity = false;
};
