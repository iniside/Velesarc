// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcGameplayTagTreeWidget.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanRequest.h"
#include "SmartObjectPlanner/ArcSmartObjectPlanResponse.h"

/**
 * ImGui debug window for testing SmartObject plan feasibility.
 *
 * Lets you select initial gameplay tags and goal tags, runs the real
 * UArcSmartObjectPlannerSubsystem planner, and displays all resulting plan
 * permutations. Selecting a plan draws it in-world with step markers,
 * execution order arrows, and tag labels.
 */
class FArcPlanFeasibilityDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	// --- ImGui panels ---
	void DrawConfigPanel();
	void DrawResultsPanel();

	// --- World drawing ---
	void DrawSearchRadiusInWorld();
	void DrawSelectedPlanInWorld();

	// --- Planner interaction ---
	void RunPlanner();
	void OnPlannerResponse(const FArcSmartObjectPlanResponse& Response);

	// --- Helpers ---
	FVector GetSearchOrigin() const;
	static float ComputePlanTotalDistance(const FArcSmartObjectPlanContainer& Plan, const FVector& Origin);

	// Tag selection — two embedded widgets
	FArcGameplayTagTreeWidget InitialTagsWidget;
	FArcGameplayTagTreeWidget RequiredTagsWidget;

	// Search parameters
	float SearchRadius = 3000.f;
	int32 MaxPlans = 20;

	// Planner state
	FArcSmartObjectPlanRequestHandle ActiveRequestHandle;
	bool bWaitingForResult = false;

	// Results
	FArcSmartObjectPlanResponse LastResponse;
	int32 SelectedPlanIndex = INDEX_NONE;
	bool bHasResults = false;
	FString StatusMessage;
};
