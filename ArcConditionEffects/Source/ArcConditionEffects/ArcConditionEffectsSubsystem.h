// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcConditionTypes.h"
#include "ArcConditionEffectsConfig.h"
#include "ArcConditionMassEffectsConfig.h"
#include "Subsystems/WorldSubsystem.h"

#include "ArcConditionEffectsSubsystem.generated.h"

// ---------------------------------------------------------------------------
// Subsystem — public API for condition application and queries
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FArcConditionEvent,
	FMassEntityHandle, Entity,
	EArcConditionType, ConditionType,
	float, Saturation);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FArcConditionOverloadEvent,
	FMassEntityHandle, Entity,
	EArcConditionType, ConditionType,
	EArcConditionOverloadPhase, Phase);

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

	// -- Config API -----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "ArcConditions")
	void SetEffectsConfig(UArcConditionEffectsConfig* Config);

	UFUNCTION(BlueprintCallable, Category = "ArcConditions")
	UArcConditionEffectsConfig* GetEffectsConfig() const { return EffectsConfig; }

	UFUNCTION(BlueprintCallable, Category = "ArcConditions")
	void SetMassEffectsConfig(UArcConditionMassEffectsConfig* Config);

	UFUNCTION(BlueprintCallable, Category = "ArcConditions")
	UArcConditionMassEffectsConfig* GetMassEffectsConfig() const { return MassEffectsConfig; }

	// -- Delegates ------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "ArcConditions")
	FArcConditionEvent OnConditionActivated;

	UPROPERTY(BlueprintAssignable, Category = "ArcConditions")
	FArcConditionEvent OnConditionDeactivated;

	UPROPERTY(BlueprintAssignable, Category = "ArcConditions")
	FArcConditionEvent OnConditionBreak;

	UPROPERTY(BlueprintAssignable, Category = "ArcConditions")
	FArcConditionOverloadEvent OnConditionOverloadChanged;

	// -- Internal (used by processors) ---------------------------------------

	TArray<FArcConditionApplicationRequest>& GetPendingRequests() { return PendingRequests; }

private:
	/** Retrieves the per-condition FArcConditionState from the entity manager, or nullptr if entity lacks that fragment. */
	static const FArcConditionState* GetConditionState(const UWorld* World, const FMassEntityHandle& Entity, EArcConditionType ConditionType);

	TArray<FArcConditionApplicationRequest> PendingRequests;

	UPROPERTY()
	TObjectPtr<UArcConditionEffectsConfig> EffectsConfig;

	UPROPERTY()
	TObjectPtr<UArcConditionMassEffectsConfig> MassEffectsConfig;
};
