// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Mass/EntityHandle.h"
#include "ArcIWEntityComponent.generated.h"

/**
 * Component placed on hydrated actors to link them back to their Mass entity.
 * Added automatically during actor hydration if not already present on the actor class.
 */
UCLASS(ClassGroup = (ArcIW), meta = (BlueprintSpawnableComponent))
class ARCINSTANCEDWORLD_API UArcIWEntityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** Returns the Mass entity handle this actor is currently representing. */
	FMassEntityHandle GetEntityHandle() const { return EntityHandle; }

	/** Returns true if this actor is currently linked to a valid entity. */
	bool HasEntity() const { return EntityHandle.IsValid(); }

	/** Called by the hydration system when the actor is assigned to an entity. */
	void SetEntityHandle(FMassEntityHandle InHandle) { EntityHandle = InHandle; }

	/** Called by the hydration system when the actor is released back to pool. */
	void ClearEntityHandle() { EntityHandle = FMassEntityHandle(); }

	/** Keep this entity hydrated (prevent dehydration processor from returning to ISM).
	 *  Call when starting an interaction. */
	UFUNCTION(BlueprintCallable, Category = "ArcIW")
	void SetKeepHydrated(bool bKeep);

	/** Returns true if this entity is marked to stay hydrated. */
	UFUNCTION(BlueprintPure, Category = "ArcIW")
	bool IsKeepHydrated() const;

private:
	FMassEntityHandle EntityHandle;
};
