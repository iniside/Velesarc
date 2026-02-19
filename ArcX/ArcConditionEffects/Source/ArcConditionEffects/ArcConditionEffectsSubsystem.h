// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcConditionEffectsSubsystem.generated.h"

// ---------------------------------------------------------------------------
// Subsystem — public API for condition application and queries
// ---------------------------------------------------------------------------

UCLASS()
class ARCCONDITIONEFFECTS_API UArcConditionEffectsSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -- Application API (enqueues request + raises per-condition signal) -----

	/** Apply saturation to a condition. Amount > 0 adds, Amount < 0 removes.
	 *  Elemental interactions are resolved by the application processor. */
	UFUNCTION(BlueprintCallable, Category = "ArcConditions")
	void ApplyCondition(const FMassEntityHandle& Entity, EArcConditionType ConditionType, float Amount);

	// -- Query API -----------------------------------------------------------

	/** Get the current saturation of a condition on an entity. Returns 0 if not present. */
	UFUNCTION(BlueprintCallable, Category = "ArcConditions", meta = (WorldContext = "WorldContextObject"))
	static float GetConditionSaturation(const UObject* WorldContextObject, const FMassEntityHandle& Entity, EArcConditionType ConditionType);

	/** Check if a condition is currently active. */
	UFUNCTION(BlueprintCallable, Category = "ArcConditions", meta = (WorldContext = "WorldContextObject"))
	static bool IsConditionActive(const UObject* WorldContextObject, const FMassEntityHandle& Entity, EArcConditionType ConditionType);

	/** Get the overload phase of a condition. */
	UFUNCTION(BlueprintCallable, Category = "ArcConditions", meta = (WorldContext = "WorldContextObject"))
	static EArcConditionOverloadPhase GetConditionOverloadPhase(const UObject* WorldContextObject, const FMassEntityHandle& Entity, EArcConditionType ConditionType);

	// -- Internal (used by processors) ---------------------------------------

	TArray<FArcConditionApplicationRequest>& GetPendingRequests() { return PendingRequests; }

private:
	/** Retrieves the per-condition FArcConditionState from the entity manager, or nullptr if entity lacks that fragment. */
	static const FArcConditionState* GetConditionState(const UWorld* World, const FMassEntityHandle& Entity, EArcConditionType ConditionType);

	TArray<FArcConditionApplicationRequest> PendingRequests;
};
