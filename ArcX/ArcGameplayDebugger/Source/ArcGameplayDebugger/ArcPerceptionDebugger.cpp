// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcPerceptionDebugger.h"

#include "imgui.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MassCommonFragments.h"
#include "MassDebugger.h"
#include "MassEntitySubsystem.h"
#include "Perception/ArcMassPerception.h"
#include "Perception/ArcMassSightPerception.h"
#include "Perception/ArcMassHearingPerception.h"

namespace
{
	UWorld* GetPerceptionDebugWorld()
	{
		if (!GEngine || !GEngine->GameViewport)
		{
			return nullptr;
		}
		return GEngine->GameViewport->GetWorld();
	}

	FMassEntityManager* GetPerceptionEntityManager()
	{
		UWorld* World = GetPerceptionDebugWorld();
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

	// Color constants
	static const FColor SightColor(50, 200, 50, 180);
	static const FColor HearingColor(50, 100, 200, 180);
	static const FColor ActivePerceivedColor = FColor::Red;
	static const FColor FadingPerceivedColor(255, 165, 0); // Orange
	static const FColor HighlightColor = FColor::Yellow;

	static const ImVec4 ImSightColor(0.2f, 0.8f, 0.2f, 1.0f);
	static const ImVec4 ImHearingColor(0.2f, 0.4f, 0.8f, 1.0f);
	static const ImVec4 ImActiveColor(1.0f, 0.3f, 0.3f, 1.0f);
	static const ImVec4 ImFadingColor(1.0f, 0.65f, 0.0f, 1.0f);
	static const ImVec4 ImHighlightColor(1.0f, 1.0f, 0.0f, 1.0f);
}

void FArcPerceptionDebugger::Initialize()
{
	SelectedPerceiverIndex = INDEX_NONE;
	PerceiverFilterBuf[0] = '\0';
	CachedPerceivers.Reset();
	LastRefreshTime = 0.f;
	bDrawSightShapes = true;
	bDrawHearingShapes = true;
	bDrawPerceivedPositions = true;
	HighlightedPerceivedEntity = FMassEntityHandle();
	RefreshPerceiverList();
}

void FArcPerceptionDebugger::Uninitialize()
{
	CachedPerceivers.Reset();
	SelectedPerceiverIndex = INDEX_NONE;
	HighlightedPerceivedEntity = FMassEntityHandle();
}

void FArcPerceptionDebugger::RefreshPerceiverList()
{
#if WITH_MASSENTITY_DEBUG
	CachedPerceivers.Reset();

	FMassEntityManager* Manager = GetPerceptionEntityManager();
	if (!Manager)
	{
		return;
	}

	TArray<FMassArchetypeHandle> Archetypes = FMassDebugger::GetAllArchetypes(*Manager);

	for (const FMassArchetypeHandle& Archetype : Archetypes)
	{
		FMassArchetypeCompositionDescriptor Composition = FMassDebugger::GetArchetypeComposition(Archetype);

		const bool bHasSight = Composition.Contains<FArcMassSightPerceptionResult>();
		const bool bHasHearing = Composition.Contains<FArcMassHearingPerceptionResult>();

		if (!bHasSight && !bHasHearing)
		{
			continue;
		}

		TArray<FMassEntityHandle> Entities = FMassDebugger::GetEntitiesOfArchetype(Archetype);
		for (const FMassEntityHandle& Entity : Entities)
		{
			FPerceiverEntry& Entry = CachedPerceivers.AddDefaulted_GetRef();
			Entry.Entity = Entity;
			Entry.bHasSight = bHasSight;
			Entry.bHasHearing = bHasHearing;

			if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
			{
				Entry.Location = TransformFrag->GetTransform().GetLocation();
			}

			// Build label
			FString Senses;
			if (bHasSight) Senses += TEXT("S");
			if (bHasHearing) Senses += TEXT("H");

			Entry.Label = FString::Printf(TEXT("E%d [%s]"), Entity.Index, *Senses);
		}
	}
#endif
}

void FArcPerceptionDebugger::Draw()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Perception Debugger", &bShow))
	{
		ImGui::End();
		return;
	}

	FMassEntityManager* Manager = GetPerceptionEntityManager();
	if (!Manager)
	{
		ImGui::TextDisabled("No MassEntitySubsystem available");
		ImGui::End();
		return;
	}

	// Toolbar
	if (ImGui::Button("Refresh"))
	{
		RefreshPerceiverList();
	}
	ImGui::SameLine();
	ImGui::Text("Perceiver Entities: %d", CachedPerceivers.Num());
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();
	ImGui::Checkbox("Draw Sight", &bDrawSightShapes);
	ImGui::SameLine();
	ImGui::Checkbox("Draw Hearing", &bDrawHearingShapes);
	ImGui::SameLine();
	ImGui::Checkbox("Draw Perceived Pos", &bDrawPerceivedPositions);

	// Auto-refresh every 2 seconds
	UWorld* World = GetPerceptionDebugWorld();
	if (World)
	{
		float CurrentTime = World->GetTimeSeconds();
		if (CurrentTime - LastRefreshTime > 2.0f)
		{
			LastRefreshTime = CurrentTime;
			RefreshPerceiverList();
		}
	}

	ImGui::Separator();

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.25f;

	// Left panel: perceiver list
	if (ImGui::BeginChild("PerceiverListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawPerceiverListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: perceiver detail
	if (ImGui::BeginChild("PerceiverDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawPerceiverDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// Draw world visualization for selected entity
	DrawWorldVisualization();
#endif
}

void FArcPerceptionDebugger::DrawPerceiverListPanel()
{
#if WITH_MASSENTITY_DEBUG
	ImGui::Text("Perceivers");
	ImGui::Separator();

	ImGui::InputText("Filter", PerceiverFilterBuf, IM_ARRAYSIZE(PerceiverFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(PerceiverFilterBuf)).ToLower();

	if (ImGui::BeginChild("PerceiverScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedPerceivers.Num(); ++i)
		{
			const FPerceiverEntry& Entry = CachedPerceivers[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			const bool bSelected = (SelectedPerceiverIndex == i);

			// Color-code based on senses
			ImVec4 Color(1.0f, 1.0f, 1.0f, 1.0f);
			if (Entry.bHasSight && Entry.bHasHearing)
			{
				Color = ImVec4(0.5f, 0.8f, 1.0f, 1.0f); // Cyan-ish for both
			}
			else if (Entry.bHasSight)
			{
				Color = ImSightColor;
			}
			else if (Entry.bHasHearing)
			{
				Color = ImHearingColor;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, Color);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Label), bSelected))
			{
				SelectedPerceiverIndex = i;
				HighlightedPerceivedEntity = FMassEntityHandle();
			}
			ImGui::PopStyleColor();
		}
	}
	ImGui::EndChild();
#endif
}

void FArcPerceptionDebugger::DrawPerceiverDetailPanel()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedPerceiverIndex == INDEX_NONE || !CachedPerceivers.IsValidIndex(SelectedPerceiverIndex))
	{
		ImGui::TextDisabled("Select a perceiver entity from the list");
		return;
	}

	FMassEntityManager* Manager = GetPerceptionEntityManager();
	if (!Manager)
	{
		return;
	}

	const FPerceiverEntry& Entry = CachedPerceivers[SelectedPerceiverIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entity is no longer active");
		return;
	}

	ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", TCHAR_TO_ANSI(*Entry.Label));

	// Location
	if (const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity))
	{
		FVector Loc = TransformFrag->GetTransform().GetLocation();
		ImGui::Text("Location: (%.0f, %.0f, %.0f)", Loc.X, Loc.Y, Loc.Z);
	}

	UWorld* World = GetPerceptionDebugWorld();
	float CurrentTime = World ? World->GetTimeSeconds() : 0.0;

	ImGui::Spacing();

	// === SIGHT ===
	if (Entry.bHasSight)
	{
		FArcMassSightPerceptionResult* SightResult = Manager->GetFragmentDataPtr<FArcMassSightPerceptionResult>(Entity);
		const FArcPerceptionSightSenseConfigFragment* SightConfig = Manager->GetConstSharedFragmentDataPtr<FArcPerceptionSightSenseConfigFragment>(Entity);
		DrawSenseSection(TEXT("Sight"), SightResult, SightConfig, bDrawSightShapes, CurrentTime);
	}

	// === HEARING ===
	if (Entry.bHasHearing)
	{
		FArcMassHearingPerceptionResult* HearingResult = Manager->GetFragmentDataPtr<FArcMassHearingPerceptionResult>(Entity);
		const FArcPerceptionHearingSenseConfigFragment* HearingConfig = Manager->GetConstSharedFragmentDataPtr<FArcPerceptionHearingSenseConfigFragment>(Entity);
		DrawSenseSection(TEXT("Hearing"), HearingResult, HearingConfig, bDrawHearingShapes, CurrentTime);
	}
#endif
}

void FArcPerceptionDebugger::DrawSenseSection(const TCHAR* SenseName, FArcMassPerceptionResultFragmentBase* Result,
	const FArcPerceptionSenseConfigFragment* Config, bool& bDrawShapes, float CurrentTime)
{
#if WITH_MASSENTITY_DEBUG
	FMassEntityManager* Manager = GetPerceptionEntityManager();

	FString HeaderText = FString::Printf(TEXT("%s (%d perceived)"),
		SenseName,
		Result ? Result->PerceivedEntities.Num() : 0);

	if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*HeaderText), ImGuiTreeNodeFlags_DefaultOpen))
	{
		// Config info
		if (Config)
		{
			if (ImGui::TreeNode(TCHAR_TO_ANSI(*FString::Printf(TEXT("%s Config"), SenseName))))
			{
				DrawSenseConfig(Config);
				ImGui::TreePop();
			}
		}

		if (!Result)
		{
			ImGui::TextDisabled("No result fragment");
			return;
		}

		ImGui::Text("Time Since Last Update: %.3fs", Result->TimeSinceLastUpdate);
		ImGui::Spacing();

		if (Result->PerceivedEntities.IsEmpty())
		{
			ImGui::TextDisabled("Nothing currently perceived");
			return;
		}

		// Perceived entities table
		if (ImGui::BeginTable("PerceivedTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_None, 0.12f);
			ImGui::TableSetupColumn("Distance", ImGuiTableColumnFlags_None, 0.10f);
			ImGui::TableSetupColumn("Strength", ImGuiTableColumnFlags_None, 0.10f);
			ImGui::TableSetupColumn("Last Known Pos", ImGuiTableColumnFlags_None, 0.22f);
			ImGui::TableSetupColumn("Forget Timer", ImGuiTableColumnFlags_None, 0.18f);
			ImGui::TableSetupColumn("Time Perceived", ImGuiTableColumnFlags_None, 0.13f);
			ImGui::TableSetupColumn("Select", ImGuiTableColumnFlags_None, 0.08f);
			ImGui::TableHeadersRow();

			for (int32 i = 0; i < Result->PerceivedEntities.Num(); ++i)
			{
				ImGui::PushID(i);
				DrawPerceivedEntityRow(Result->PerceivedEntities[i], i, Config, CurrentTime, Manager);
				ImGui::PopID();
			}

			ImGui::EndTable();
		}
	}
#endif
}

void FArcPerceptionDebugger::DrawPerceivedEntityRow(const FArcPerceivedEntity& PE, int32 Index,
	const FArcPerceptionSenseConfigFragment* Config, float CurrentTime, FMassEntityManager* Manager)
{
#if WITH_MASSENTITY_DEBUG
	float TimeSinceLastSeen = static_cast<float>(CurrentTime - PE.LastTimeSeen);
	float ForgetTime = Config ? Config->ForgetTime : 10.f;
	float ForgetRemaining = FMath::Max(0.f, ForgetTime - TimeSinceLastSeen);
	float ForgetRatio = ForgetTime > 0.f ? FMath::Clamp(ForgetRemaining / ForgetTime, 0.f, 1.f) : 0.f;

	bool bActivelyPerceived = TimeSinceLastSeen < 0.5f;
	bool bHighlighted = PE.Entity == HighlightedPerceivedEntity;

	ImVec4 RowColor = bHighlighted ? ImHighlightColor : (bActivelyPerceived ? ImActiveColor : ImFadingColor);

	ImGui::TableNextRow();

	// Entity
	ImGui::TableNextColumn();
	ImGui::TextColored(RowColor, "E%d", PE.Entity.Index);

	// Distance
	ImGui::TableNextColumn();
	ImGui::Text("%.0f", PE.Distance);

	// Strength
	ImGui::TableNextColumn();
	if (PE.Strength > 0.f)
	{
		ImGui::Text("%.2f", PE.Strength);
	}
	else
	{
		ImGui::TextDisabled("-");
	}

	// Last Known Position
	ImGui::TableNextColumn();
	ImGui::Text("(%.0f, %.0f, %.0f)", PE.LastKnownLocation.X, PE.LastKnownLocation.Y, PE.LastKnownLocation.Z);

	// Forget Timer (progress bar)
	ImGui::TableNextColumn();
	if (bActivelyPerceived)
	{
		ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Active");
	}
	else
	{
		ImVec4 BarColor;
		if (ForgetRatio > 0.5f)
		{
			BarColor = ImVec4(1.0f, 0.65f, 0.0f, 1.0f);
		}
		else if (ForgetRatio > 0.2f)
		{
			BarColor = ImVec4(1.0f, 0.3f, 0.0f, 1.0f);
		}
		else
		{
			BarColor = ImVec4(1.0f, 0.1f, 0.1f, 1.0f);
		}

		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, BarColor);
		FString ForgetLabel = FString::Printf(TEXT("%.1fs / %.1fs"), ForgetRemaining, ForgetTime);
		ImGui::ProgressBar(ForgetRatio, ImVec2(-FLT_MIN, 0), TCHAR_TO_ANSI(*ForgetLabel));
		ImGui::PopStyleColor();
	}

	// Time Perceived
	ImGui::TableNextColumn();
	ImGui::Text("%.1fs", PE.TimePerceived);

	// Select button
	ImGui::TableNextColumn();
	bool bIsSelected = (PE.Entity == HighlightedPerceivedEntity);
	if (bIsSelected)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.8f, 0.0f, 0.6f));
	}
	if (ImGui::SmallButton(bIsSelected ? "X" : ">"))
	{
		if (bIsSelected)
		{
			HighlightedPerceivedEntity = FMassEntityHandle();
		}
		else
		{
			HighlightedPerceivedEntity = PE.Entity;
		}
	}
	if (bIsSelected)
	{
		ImGui::PopStyleColor();
	}
#endif
}

void FArcPerceptionDebugger::DrawSenseConfig(const FArcPerceptionSenseConfigFragment* Config)
{
#if WITH_MASSENTITY_DEBUG
	if (!Config)
	{
		return;
	}

	ImGui::Text("Eye Offset: %.1f", Config->EyeOffset);

	if (Config->ShapeType == EArcPerceptionShapeType::Radius)
	{
		ImGui::Text("Shape: Radius (%.0f)", Config->Radius);
	}
	else
	{
		ImGui::Text("Shape: Cone (Length: %.0f, Half-Angle: %.1f deg)", Config->ConeLength, Config->ConeHalfAngleDegrees);
	}

	ImGui::Text("Update Interval: %.2fs", Config->UpdateInterval);
	ImGui::Text("Forget Time: %.1fs", Config->ForgetTime);

	if (!Config->RequiredTags.IsEmpty())
	{
		ImGui::Text("Required Tags: %s", TCHAR_TO_ANSI(*Config->RequiredTags.ToStringSimple()));
	}
	if (!Config->IgnoredTags.IsEmpty())
	{
		ImGui::Text("Ignored Tags: %s", TCHAR_TO_ANSI(*Config->IgnoredTags.ToStringSimple()));
	}

	// Check if this is a hearing config (has extra fields)
	if (const FArcPerceptionHearingSenseConfigFragment* HearingConfig = static_cast<const FArcPerceptionHearingSenseConfigFragment*>(Config))
	{
		// We can only safely cast if the caller passed us a hearing config.
		// Since we don't have RTTI, check via a sentinel: hearing configs have SoundMaxAge > 0 typically.
		// Actually, we just display what base config has. The hearing-specific data is shown separately if needed.
	}
#endif
}

void FArcPerceptionDebugger::DrawWorldVisualization()
{
#if WITH_MASSENTITY_DEBUG
	if (SelectedPerceiverIndex == INDEX_NONE || !CachedPerceivers.IsValidIndex(SelectedPerceiverIndex))
	{
		return;
	}

	UWorld* World = GetPerceptionDebugWorld();
	if (!World)
	{
		return;
	}

	FMassEntityManager* Manager = GetPerceptionEntityManager();
	if (!Manager)
	{
		return;
	}

	const FPerceiverEntry& Entry = CachedPerceivers[SelectedPerceiverIndex];
	const FMassEntityHandle Entity = Entry.Entity;

	if (!Manager->IsEntityActive(Entity))
	{
		return;
	}

	const FTransformFragment* TransformFrag = Manager->GetFragmentDataPtr<FTransformFragment>(Entity);
	if (!TransformFrag)
	{
		return;
	}

	const FVector EntityLoc = TransformFrag->GetTransform().GetLocation();
	const FVector Forward = TransformFrag->GetTransform().GetRotation().GetForwardVector();
	const float CurrentTime = World->GetTimeSeconds();

	// Draw entity marker
	constexpr float ZOffset = 100.f;
	DrawDebugSphere(World, EntityLoc + FVector(0, 0, ZOffset), 40.f, 12, FColor::Cyan, false, -1.f, 0, 2.f);
	DrawDebugString(World, EntityLoc + FVector(0, 0, ZOffset + 60.f), *Entry.Label, nullptr, FColor::White, -1.f, true, 1.2f);

	// Draw sight range + perceived entities
	if (bDrawSightShapes && Entry.bHasSight)
	{
		const FArcPerceptionSightSenseConfigFragment* SightConfig = Manager->GetConstSharedFragmentDataPtr<FArcPerceptionSightSenseConfigFragment>(Entity);
		if (SightConfig)
		{
			FVector EyePos = EntityLoc + FVector(0, 0, SightConfig->EyeOffset);
			DrawSenseRange(EyePos, Forward, SightConfig, SightColor);
		}

		if (bDrawPerceivedPositions)
		{
			if (const FArcMassSightPerceptionResult* SightResult = Manager->GetFragmentDataPtr<FArcMassSightPerceptionResult>(Entity))
			{
				for (const FArcPerceivedEntity& PE : SightResult->PerceivedEntities)
				{
					bool bHighlighted = (PE.Entity == HighlightedPerceivedEntity);
					float TimeSinceLastSeen = static_cast<float>(CurrentTime - PE.LastTimeSeen);
					bool bActive = TimeSinceLastSeen < 0.5f;
					FColor Color = bHighlighted ? HighlightColor : (bActive ? ActivePerceivedColor : FadingPerceivedColor);
					DrawPerceivedEntityMarker(PE, Color, bHighlighted, Manager);
				}
			}
		}
	}

	// Draw hearing range + perceived entities
	if (bDrawHearingShapes && Entry.bHasHearing)
	{
		const FArcPerceptionHearingSenseConfigFragment* HearingConfig = Manager->GetConstSharedFragmentDataPtr<FArcPerceptionHearingSenseConfigFragment>(Entity);
		if (HearingConfig)
		{
			FVector EyePos = EntityLoc + FVector(0, 0, HearingConfig->EyeOffset);
			DrawSenseRange(EyePos, Forward, HearingConfig, HearingColor);
		}

		if (bDrawPerceivedPositions)
		{
			if (const FArcMassHearingPerceptionResult* HearingResult = Manager->GetFragmentDataPtr<FArcMassHearingPerceptionResult>(Entity))
			{
				for (const FArcPerceivedEntity& PE : HearingResult->PerceivedEntities)
				{
					bool bHighlighted = (PE.Entity == HighlightedPerceivedEntity);
					float TimeSinceLastSeen = static_cast<float>(CurrentTime - PE.LastTimeSeen);
					bool bActive = TimeSinceLastSeen < 0.5f;
					FColor Color = bHighlighted ? HighlightColor : (bActive ? FColor(200, 50, 50) : FColor(200, 130, 50));
					DrawPerceivedEntityMarker(PE, Color, bHighlighted, Manager);
				}
			}
		}
	}
#endif
}

void FArcPerceptionDebugger::DrawSenseRange(const FVector& Origin, const FVector& Forward,
	const FArcPerceptionSenseConfigFragment* Config, const FColor& Color) const
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = GetPerceptionDebugWorld();
	if (!World || !Config)
	{
		return;
	}

	if (Config->ShapeType == EArcPerceptionShapeType::Radius)
	{
		DrawDebugSphere(World, Origin, Config->Radius, 24, Color, false, -1.f, 0, 1.5f);
	}
	else // Cone
	{
		float HalfAngleRad = FMath::DegreesToRadians(Config->ConeHalfAngleDegrees);
		float ConeRadius = Config->ConeLength * FMath::Tan(HalfAngleRad);

		// Draw cone approximation with lines
		FVector ConeEnd = Origin + Forward * Config->ConeLength;
		constexpr int32 NumSegments = 16;

		// Build a right/up basis
		FVector Right = FVector::CrossProduct(Forward, FVector::UpVector);
		if (Right.IsNearlyZero())
		{
			Right = FVector::CrossProduct(Forward, FVector::RightVector);
		}
		Right.Normalize();
		FVector Up = FVector::CrossProduct(Right, Forward);
		Up.Normalize();

		TArray<FVector> RingPoints;
		RingPoints.SetNum(NumSegments);
		for (int32 i = 0; i < NumSegments; ++i)
		{
			float Angle = (2.f * UE_PI * i) / NumSegments;
			FVector Offset = (Right * FMath::Cos(Angle) + Up * FMath::Sin(Angle)) * ConeRadius;
			RingPoints[i] = ConeEnd + Offset;
		}

		// Draw cone lines from origin to ring
		for (int32 i = 0; i < NumSegments; i += 2)
		{
			DrawDebugLine(World, Origin, RingPoints[i], Color, false, -1.f, 0, 1.5f);
		}

		// Draw ring at cone end
		for (int32 i = 0; i < NumSegments; ++i)
		{
			int32 Next = (i + 1) % NumSegments;
			DrawDebugLine(World, RingPoints[i], RingPoints[Next], Color, false, -1.f, 0, 1.5f);
		}
	}
#endif
}

void FArcPerceptionDebugger::DrawPerceivedEntityMarker(const FArcPerceivedEntity& PE, const FColor& Color,
	bool bHighlighted, FMassEntityManager* Manager) const
{
#if WITH_MASSENTITY_DEBUG
	UWorld* World = GetPerceptionDebugWorld();
	if (!World)
	{
		return;
	}

	constexpr float ZOffset = 100.f;
	float SphereRadius = bHighlighted ? 50.f : 30.f;
	float Thickness = bHighlighted ? 3.f : 1.5f;

	// Try to get current position if entity is still alive
	FVector DrawPos = PE.LastKnownLocation;
	if (Manager && Manager->IsEntityActive(PE.Entity))
	{
		if (const FTransformFragment* Transform = Manager->GetFragmentDataPtr<FTransformFragment>(PE.Entity))
		{
			DrawPos = Transform->GetTransform().GetLocation();
		}
	}

	DrawDebugSphere(World, DrawPos + FVector(0, 0, ZOffset), SphereRadius, 8, Color, false, -1.f, 0, Thickness);

	FString Label = FString::Printf(TEXT("E%d (%.0f)"), PE.Entity.Index, PE.Distance);
	DrawDebugString(World, DrawPos + FVector(0, 0, ZOffset + SphereRadius + 20.f), Label, nullptr, Color, -1.f, true, 1.0f);

	if (bHighlighted)
	{
		// Draw a vertical line for extra visibility
		DrawDebugLine(World, DrawPos, DrawPos + FVector(0, 0, ZOffset + SphereRadius + 10.f), Color, false, -1.f, 0, 2.f);
	}
#endif
}
