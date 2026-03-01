// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPlanFeasibilityDebugger.h"

#include "imgui.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "MassEntitySubsystem.h"
#include "SmartObjectPlanner/ArcMassGoalPlanInfoSharedFragment.h"
#include "SmartObjectPlanner/ArcSmartObjectPlannerSubsystem.h"

namespace
{
	UWorld* GetFeasibilityDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetFeasibilityEntityManager()
	{
		UWorld* World = GetFeasibilityDebugWorld();
		if (!World)
		{
			return nullptr;
		}
		UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
		if (!MassSub)
		{
			return nullptr;
		}
		return &MassSub->GetMutableEntityManager();
	}
}

void FArcPlanFeasibilityDebugger::Initialize()
{
	InitialTagsWidget.Initialize();
	RequiredTagsWidget.Initialize();
	bHasResults = false;
	bWaitingForResult = false;
	SelectedPlanIndex = INDEX_NONE;
	LastResponse = FArcSmartObjectPlanResponse();
	StatusMessage = FString();
}

void FArcPlanFeasibilityDebugger::Uninitialize()
{
	InitialTagsWidget.Uninitialize();
	RequiredTagsWidget.Uninitialize();
	bHasResults = false;
	bWaitingForResult = false;
	SelectedPlanIndex = INDEX_NONE;
	LastResponse = FArcSmartObjectPlanResponse();
	StatusMessage = FString();
}

void FArcPlanFeasibilityDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(700, 800), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Plan Feasibility Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	DrawConfigPanel();
	ImGui::Spacing();
	DrawResultsPanel();

	ImGui::End();

	// World-space drawing (after ImGui::End)
	DrawSearchRadiusInWorld();
	DrawSelectedPlanInWorld();
}

// =============================================================================
// Configuration Panel
// =============================================================================

void FArcPlanFeasibilityDebugger::DrawConfigPanel()
{
	if (ImGui::CollapsingHeader("Configuration", ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Search parameters
		ImGui::SliderFloat("Search Radius", &SearchRadius, 100.f, 20000.f, "%.0f");
		ImGui::SliderInt("Max Plans", &MaxPlans, 1, 50);

		ImGui::Spacing();

		// Two tag trees side by side
		float AvailWidth = ImGui::GetContentRegionAvail().x;
		float HalfWidth = AvailWidth * 0.48f;

		// --- Initial Tags (left) ---
		if (ImGui::BeginChild("InitialTagsPanel", ImVec2(HalfWidth, 300), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
		{
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "Initial Tags (what entity has)");
			ImGui::Separator();
			ImGui::PushID("InitialTags");
			InitialTagsWidget.DrawInline();
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// --- Required Tags / Goal (right) ---
		if (ImGui::BeginChild("RequiredTagsPanel", ImVec2(0, 300), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeY))
		{
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Required Tags (goal to achieve)");
			ImGui::Separator();
			ImGui::PushID("RequiredTags");
			RequiredTagsWidget.DrawInline();
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::Spacing();

		// Run button
		const bool bCanRun = !bWaitingForResult && RequiredTagsWidget.GetSelectedTags().Num() > 0;
		if (!bCanRun)
		{
			ImGui::BeginDisabled();
		}
		if (ImGui::Button("Run Planner", ImVec2(150, 30)))
		{
			RunPlanner();
		}
		if (!bCanRun)
		{
			ImGui::EndDisabled();
		}

		// Status
		ImGui::SameLine();
		if (bWaitingForResult)
		{
			ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Planning...");
		}
		else if (!StatusMessage.IsEmpty())
		{
			if (bHasResults && LastResponse.Plans.Num() > 0)
			{
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "%s", TCHAR_TO_ANSI(*StatusMessage));
			}
			else if (bHasResults)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", TCHAR_TO_ANSI(*StatusMessage));
			}
			else
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*StatusMessage));
			}
		}
		else if (RequiredTagsWidget.GetSelectedTags().Num() == 0)
		{
			ImGui::TextDisabled("Select at least one goal tag");
		}
	}
}

// =============================================================================
// Results Panel
// =============================================================================

void FArcPlanFeasibilityDebugger::DrawResultsPanel()
{
	if (!bHasResults)
	{
		return;
	}

	if (ImGui::CollapsingHeader("Results", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (LastResponse.Plans.Num() == 0)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
				"No plans possible from selected conditions");
			return;
		}

		const FVector Origin = GetSearchOrigin();

		ImGui::Text("Plans found: %d", LastResponse.Plans.Num());
		ImGui::Separator();

		// Plan list
		float AvailHeight = ImGui::GetContentRegionAvail().y;
		float ListHeight = FMath::Min(AvailHeight * 0.35f, 200.f);

		if (ImGui::BeginChild("PlanList", ImVec2(0, ListHeight), ImGuiChildFlags_Borders))
		{
			for (int32 PlanIdx = 0; PlanIdx < LastResponse.Plans.Num(); PlanIdx++)
			{
				const FArcSmartObjectPlanContainer& Plan = LastResponse.Plans[PlanIdx];
				float TotalDist = ComputePlanTotalDistance(Plan, Origin);

				FString Label = FString::Printf(TEXT("Plan %d  (%d steps, %.0fm)"),
					PlanIdx, Plan.Items.Num(), TotalDist / 100.f); // cm to m

				const bool bSelected = (SelectedPlanIndex == PlanIdx);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*Label), bSelected))
				{
					SelectedPlanIndex = PlanIdx;
				}
			}
		}
		ImGui::EndChild();

		// Selected plan detail
		if (SelectedPlanIndex != INDEX_NONE && LastResponse.Plans.IsValidIndex(SelectedPlanIndex))
		{
			ImGui::Separator();
			ImGui::Text("Selected Plan Detail");
			ImGui::Separator();

			const FArcSmartObjectPlanContainer& Plan = LastResponse.Plans[SelectedPlanIndex];
			FMassEntityManager* Manager = GetFeasibilityEntityManager();

			for (int32 StepIdx = 0; StepIdx < Plan.Items.Num(); StepIdx++)
			{
				const FArcSmartObjectPlanStep& Step = Plan.Items[StepIdx];

				ImGui::PushID(StepIdx);

				FString StepHeader = FString::Printf(TEXT("Step %d"), StepIdx);
				if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(*StepHeader), ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Text("Entity: %s", TCHAR_TO_ANSI(*Step.EntityHandle.DebugGetDescription()));
					ImGui::Text("Location: (%.0f, %.0f, %.0f)", Step.Location.X, Step.Location.Y, Step.Location.Z);

					if (!Step.Requires.IsEmpty())
					{
						ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Requires: %s",
							TCHAR_TO_ANSI(*Step.Requires.ToStringSimple()));
					}

					ImGui::Text("Candidate Slots: %d", Step.FoundCandidateSlots.NumSlots);

					// Show Provides from GoalPlanInfo if entity is still active
					if (Manager && Step.EntityHandle.IsSet() && Manager->IsEntityActive(Step.EntityHandle))
					{
						const FArcMassGoalPlanInfoSharedFragment* GoalInfo =
							Manager->GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Step.EntityHandle);
						if (GoalInfo)
						{
							if (!GoalInfo->Provides.IsEmpty())
							{
								ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Provides: %s",
									TCHAR_TO_ANSI(*GoalInfo->Provides.ToStringSimple()));
							}
							if (!GoalInfo->Requires.IsEmpty())
							{
								ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Entity Requires: %s",
									TCHAR_TO_ANSI(*GoalInfo->Requires.ToStringSimple()));
							}
						}
					}

					ImGui::TreePop();
				}

				ImGui::PopID();
			}
		}
	}
}

// =============================================================================
// Planner Integration
// =============================================================================

void FArcPlanFeasibilityDebugger::RunPlanner()
{
	UWorld* World = GetFeasibilityDebugWorld();
	if (!World)
	{
		StatusMessage = TEXT("No world available");
		return;
	}

	UArcSmartObjectPlannerSubsystem* PlannerSubsystem = World->GetSubsystem<UArcSmartObjectPlannerSubsystem>();
	if (!PlannerSubsystem)
	{
		StatusMessage = TEXT("PlannerSubsystem not available");
		return;
	}

	// Reset previous results
	bHasResults = false;
	SelectedPlanIndex = INDEX_NONE;
	LastResponse = FArcSmartObjectPlanResponse();

	// Build request
	FArcSmartObjectPlanRequest Request;
	Request.Requires = RequiredTagsWidget.GetSelectedTags();
	Request.InitialTags = InitialTagsWidget.GetSelectedTags();
	Request.SearchRadius = SearchRadius;
	Request.SearchOrigin = GetSearchOrigin();
	Request.MaxPlans = MaxPlans;
	// RequestingEntity left default (unset) — planner handles this

	Request.FinishedDelegate.BindRaw(this, &FArcPlanFeasibilityDebugger::OnPlannerResponse);

	ActiveRequestHandle = PlannerSubsystem->AddRequest(Request);
	bWaitingForResult = true;
	StatusMessage = TEXT("Planning...");
}

void FArcPlanFeasibilityDebugger::OnPlannerResponse(const FArcSmartObjectPlanResponse& Response)
{
	LastResponse = Response;
	bWaitingForResult = false;
	bHasResults = true;

	if (Response.Plans.Num() > 0)
	{
		StatusMessage = FString::Printf(TEXT("%d plan(s) found"), Response.Plans.Num());
		SelectedPlanIndex = 0; // Auto-select first plan
	}
	else
	{
		StatusMessage = TEXT("No plans possible from selected conditions");
		SelectedPlanIndex = INDEX_NONE;
	}
}

// =============================================================================
// World Drawing
// =============================================================================

void FArcPlanFeasibilityDebugger::DrawSearchRadiusInWorld()
{
	UWorld* World = GetFeasibilityDebugWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetSearchOrigin();

	// Draw circle on ground showing search radius
	DrawDebugCircle(World, Origin, SearchRadius, 64, FColor(100, 200, 255, 128),
		false, -1.f, 0, 2.f, FVector::RightVector, FVector::ForwardVector, false);

	// Small sphere at origin
	DrawDebugSphere(World, Origin + FVector(0, 0, 50.f), 20.f, 8, FColor::Cyan, false, -1.f, 0, 2.f);
}

void FArcPlanFeasibilityDebugger::DrawSelectedPlanInWorld()
{
	if (!bHasResults || SelectedPlanIndex == INDEX_NONE || !LastResponse.Plans.IsValidIndex(SelectedPlanIndex))
	{
		return;
	}

	UWorld* World = GetFeasibilityDebugWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager* Manager = GetFeasibilityEntityManager();

	const FArcSmartObjectPlanContainer& Plan = LastResponse.Plans[SelectedPlanIndex];
	if (Plan.Items.Num() == 0)
	{
		return;
	}

	const FVector Origin = GetSearchOrigin();
	constexpr float ZOffset = 100.f;
	constexpr float SphereRadius = 35.f;
	static const FColor FirstStepColor = FColor::Green;
	static const FColor MidStepColor = FColor::Yellow;
	static const FColor LastStepColor = FColor::Red;
	static const FColor ArrowColor = FColor(255, 140, 0); // Orange

	// Arrow from origin to first step
	{
		FVector StartLoc = Origin + FVector(0, 0, ZOffset);
		FVector EndLoc = Plan.Items[0].Location + FVector(0, 0, ZOffset);
		DrawDebugDirectionalArrow(World, StartLoc, EndLoc, 40.f, FColor::Cyan, false, -1.f, 0, 3.f);
	}

	const int32 LastIdx = Plan.Items.Num() - 1;

	for (int32 StepIdx = 0; StepIdx < Plan.Items.Num(); StepIdx++)
	{
		const FArcSmartObjectPlanStep& Step = Plan.Items[StepIdx];
		FVector StepLoc = Step.Location + FVector(0, 0, ZOffset);

		// Color: green for first, red for last, yellow for mid
		FColor StepColor = MidStepColor;
		if (StepIdx == 0)
		{
			StepColor = FirstStepColor;
		}
		else if (StepIdx == LastIdx)
		{
			StepColor = LastStepColor;
		}

		// Sphere
		DrawDebugSphere(World, StepLoc, SphereRadius, 12, StepColor, false, -1.f, 0, 2.f);

		// Text label: step number + provides/requires
		FString Label = FString::Printf(TEXT("Step %d"), StepIdx);
		if (Manager && Step.EntityHandle.IsSet() && Manager->IsEntityActive(Step.EntityHandle))
		{
			const FArcMassGoalPlanInfoSharedFragment* GoalInfo =
				Manager->GetConstSharedFragmentDataPtr<FArcMassGoalPlanInfoSharedFragment>(Step.EntityHandle);
			if (GoalInfo && !GoalInfo->Provides.IsEmpty())
			{
				Label += FString::Printf(TEXT("\nProvides: %s"), *GoalInfo->Provides.ToStringSimple());
			}
		}
		if (!Step.Requires.IsEmpty())
		{
			Label += FString::Printf(TEXT("\nRequires: %s"), *Step.Requires.ToStringSimple());
		}

		DrawDebugString(World, StepLoc + FVector(0, 0, 50.f), Label, nullptr, FColor::White, -1.f, true, 1.0f);

		// Arrow to next step
		if (StepIdx < LastIdx)
		{
			FVector NextLoc = Plan.Items[StepIdx + 1].Location + FVector(0, 0, ZOffset);
			DrawDebugDirectionalArrow(World, StepLoc, NextLoc, 40.f, ArrowColor, false, -1.f, 0, 3.f);
		}
	}
}

// =============================================================================
// Helpers
// =============================================================================

FVector FArcPlanFeasibilityDebugger::GetSearchOrigin() const
{
	UWorld* World = GetFeasibilityDebugWorld();
	if (!World)
	{
		return FVector::ZeroVector;
	}

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC)
	{
		return FVector::ZeroVector;
	}

	if (APawn* Pawn = PC->GetPawn())
	{
		return Pawn->GetActorLocation();
	}

	// Fallback: camera location
	FVector CamLoc;
	FRotator CamRot;
	PC->GetPlayerViewPoint(CamLoc, CamRot);
	return CamLoc;
}

float FArcPlanFeasibilityDebugger::ComputePlanTotalDistance(const FArcSmartObjectPlanContainer& Plan, const FVector& Origin)
{
	if (Plan.Items.Num() == 0)
	{
		return 0.f;
	}

	float TotalDist = FVector::Dist(Origin, Plan.Items[0].Location);
	for (int32 i = 0; i + 1 < Plan.Items.Num(); i++)
	{
		TotalDist += FVector::Dist(Plan.Items[i].Location, Plan.Items[i + 1].Location);
	}
	return TotalDist;
}
