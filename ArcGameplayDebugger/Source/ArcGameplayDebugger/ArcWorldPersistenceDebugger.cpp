// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcWorldPersistenceDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "MassEntitySubsystem.h"
#include "ArcMass/Persistence/ArcMassEntityPersistenceSubsystem.h"
#include "ArcMass/Persistence/ArcMassPersistence.h"
#include "ArcMass/Persistence/ArcMassFragmentSerializer.h"
#include "MassEntityView.h"
#include "ArcPersistence/ArcPersistenceSubsystem.h"
#include "Storage/ArcPersistenceBackend.h"
#include "MassEntityTypes.h"
#include "MassCommonFragments.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "GameFramework/Pawn.h"
#include "UObject/FieldIterator.h"
#include "GameFramework/PlayerController.h"

// ====================================================================
// Colors
// ====================================================================

namespace Arcx::GameplayDebugger::WorldPersistence
{
	static const ImU32 BackgroundColor       = IM_COL32(20, 20, 25, 240);
	static const ImU32 OriginColor           = IM_COL32(255, 60, 60, 180);
	static const ImU32 HUDTextColor          = IM_COL32(200, 200, 200, 255);
	static const ImU32 LoadedCellFill        = IM_COL32(40, 160, 80, 60);
	static const ImU32 LoadedCellBorder      = IM_COL32(40, 160, 80, 120);
	static const ImU32 UnloadedCellFill      = IM_COL32(100, 100, 100, 40);
	static const ImU32 UnloadedCellBorder    = IM_COL32(100, 100, 100, 80);
	static const ImU32 EntityColor           = IM_COL32(220, 200, 50, 255);
	static const ImU32 SourceEntityColor     = IM_COL32(50, 140, 255, 255);
	static const ImU32 SelectedEntityColor   = IM_COL32(255, 60, 60, 255);
	static const ImU32 SelectedEntityRing    = IM_COL32(255, 60, 60, 180);
	static const ImU32 HoveredEntityColor    = IM_COL32(255, 255, 255, 255);
}

// ====================================================================
// Lifecycle
// ====================================================================

void FArcWorldPersistenceDebugger::Initialize()
{
	ViewOffset = FVector2D::ZeroVector;
	Zoom = 0.05f;
	bIsPanning = false;
	bHasSelection = false;
	SelectedEntity = FMassEntityHandle();
	FMemory::Memzero(FilterBuf, sizeof(FilterBuf));
	StatusMessage.Empty();
	StatusMessageExpireTime = 0.0;
	bShowWipeConfirmation = false;
}

void FArcWorldPersistenceDebugger::Uninitialize()
{
	CachedEntities.Empty();
}

// ====================================================================
// Helpers
// ====================================================================

UWorld* FArcWorldPersistenceDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcMassEntityPersistenceSubsystem* FArcWorldPersistenceDebugger::GetPersistenceSubsystem() const
{
	UWorld* World = GetDebugWorld();
	return World ? World->GetSubsystem<UArcMassEntityPersistenceSubsystem>() : nullptr;
}

FMassEntityManager* FArcWorldPersistenceDebugger::GetEntityManager() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	UMassEntitySubsystem* MassSub = World->GetSubsystem<UMassEntitySubsystem>();
	return MassSub ? &MassSub->GetMutableEntityManager() : nullptr;
}

ImVec2 FArcWorldPersistenceDebugger::WorldToScreen(float WorldX, float WorldY) const
{
	return ImVec2(
		CanvasCenter.x + (WorldX - static_cast<float>(ViewOffset.X)) * Zoom,
		CanvasCenter.y - (WorldY - static_cast<float>(ViewOffset.Y)) * Zoom
	);
}

FVector2D FArcWorldPersistenceDebugger::ScreenToWorld(const ImVec2& ScreenPos) const
{
	return FVector2D(
		ViewOffset.X + (ScreenPos.x - CanvasCenter.x) / Zoom,
		ViewOffset.Y - (ScreenPos.y - CanvasCenter.y) / Zoom
	);
}

// ====================================================================
// Entity Cache
// ====================================================================

void FArcWorldPersistenceDebugger::RebuildEntityCache()
{
	CachedEntities.Reset();

	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager)
	{
		return;
	}

	// Query all entities with the persistence tag — includes both
	// freshly spawned and loaded-from-save entities.
	FMassExecutionContext ExecContext(*EntityManager);
	FMassEntityQuery Query(EntityManager->AsShared());
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FArcMassPersistenceFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement<FArcMassPersistenceTag>(EMassFragmentPresence::All);

	Query.ForEachEntityChunk(ExecContext,
		[this](FMassExecutionContext& Ctx)
		{
			TConstArrayView<FTransformFragment> Transforms = Ctx.GetFragmentView<FTransformFragment>();
			const int32 NumEntities = Ctx.GetNumEntities();

			for (int32 i = 0; i < NumEntities; ++i)
			{
				FMassEntityHandle Handle = Ctx.GetEntity(i);
				FVector Position = Transforms[i].GetTransform().GetLocation();
				const bool bIsSource = Ctx.DoesArchetypeHaveTag<FArcMassPersistenceSourceTag>();

				CachedEntities.Add({Handle, Position, bIsSource});
			}
		});
}

// ====================================================================
// Minimap Input
// ====================================================================

void FArcWorldPersistenceDebugger::HandleMinimapInput()
{
	const ImGuiIO& IO = ImGui::GetIO();
	const bool bCanvasHovered = ImGui::IsItemHovered();

	// --- Panning (middle mouse button) ---
	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
	{
		bIsPanning = true;
		PanStartMouse = IO.MousePos;
		PanStartOffset = ViewOffset;
	}

	if (bIsPanning)
	{
		if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
		{
			const float DeltaX = IO.MousePos.x - PanStartMouse.x;
			const float DeltaY = IO.MousePos.y - PanStartMouse.y;
			ViewOffset.X = PanStartOffset.X - DeltaX / Zoom;
			ViewOffset.Y = PanStartOffset.Y + DeltaY / Zoom;
		}
		else
		{
			bIsPanning = false;
		}
	}

	// --- Zooming (scroll wheel, centered on mouse) ---
	if (bCanvasHovered && FMath::Abs(IO.MouseWheel) > 0.0f)
	{
		const FVector2D WorldBeforeZoom = ScreenToWorld(IO.MousePos);

		const float ZoomFactor = 1.15f;
		if (IO.MouseWheel > 0.0f)
		{
			Zoom = FMath::Min(Zoom * ZoomFactor, MaxZoom);
		}
		else
		{
			Zoom = FMath::Max(Zoom / ZoomFactor, MinZoom);
		}

		const FVector2D WorldAfterZoom = ScreenToWorld(IO.MousePos);
		ViewOffset.X -= (WorldAfterZoom.X - WorldBeforeZoom.X);
		ViewOffset.Y -= (WorldAfterZoom.Y - WorldBeforeZoom.Y);
	}

	// --- Left-click to select entity ---
	if (bCanvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
	{
		float BestDistSq = 64.0f;
		FMassEntityHandle BestEntity;
		bool bFound = false;

		for (const FEntityEntry& Entry : CachedEntities)
		{
			const ImVec2 ScreenPos = WorldToScreen(Entry.Position.X, Entry.Position.Y);
			const float DX = IO.MousePos.x - ScreenPos.x;
			const float DY = IO.MousePos.y - ScreenPos.y;
			const float DistSq = DX * DX + DY * DY;

			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				BestEntity = Entry.Handle;
				bFound = true;
			}
		}

		if (bFound)
		{
			SelectedEntity = BestEntity;
			bHasSelection = true;
		}
	}
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcWorldPersistenceDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1200, 700), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("World Persistence", &bShow, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::End();
		return;
	}

	UArcMassEntityPersistenceSubsystem* PersistSub = GetPersistenceSubsystem();
	if (!PersistSub)
	{
		ImGui::TextDisabled("No UArcMassEntityPersistenceSubsystem available");
		ImGui::End();
		return;
	}

	// Invalidate selection if entity is no longer valid
	if (bHasSelection)
	{
		FMassEntityManager* EntityManager = GetEntityManager();
		if (!EntityManager || !EntityManager->IsEntityValid(SelectedEntity))
		{
			bHasSelection = false;
			SelectedEntity = FMassEntityHandle();
		}
	}

	RebuildEntityCache();
	DrawToolbar();
	DrawWipeConfirmationPopup();

	ImGui::Separator();

	// Three-column layout
	const float AvailWidth = ImGui::GetContentRegionAvail().x;
	const float LeftWidth = AvailWidth * 0.25f;
	const float CenterWidth = AvailWidth * 0.45f;
	const float RightWidth = AvailWidth * 0.30f;

	DrawEntityList(LeftWidth);
	ImGui::SameLine();
	DrawMinimap(CenterWidth);
	ImGui::SameLine();
	DrawEntityDetails(RightWidth);

	ImGui::End();
}

// ====================================================================
// Toolbar
// ====================================================================

void FArcWorldPersistenceDebugger::DrawToolbar()
{
	UArcMassEntityPersistenceSubsystem* PersistSub = GetPersistenceSubsystem();

	if (ImGui::Button("Save All"))
	{
		SaveAll();
	}
	ImGui::SameLine();

	if (ImGui::Button("Load All"))
	{
		LoadAll();
	}
	ImGui::SameLine();

	if (ImGui::Button("Wipe Save Data"))
	{
		bShowWipeConfirmation = true;
	}
	ImGui::SameLine();

	const int32 EntityCount = CachedEntities.Num();
	const int32 CellCount = PersistSub ? PersistSub->LoadedCells.Num() : 0;
	ImGui::Text("| %d entities, %d cells", EntityCount, CellCount);

	if (!StatusMessage.IsEmpty())
	{
		const double Now = FPlatformTime::Seconds();
		if (Now < StatusMessageExpireTime)
		{
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "| %s", TCHAR_TO_ANSI(*StatusMessage));
		}
		else
		{
			StatusMessage.Empty();
		}
	}
}

void FArcWorldPersistenceDebugger::DrawWipeConfirmationPopup()
{
	if (bShowWipeConfirmation)
	{
		ImGui::OpenPopup("Confirm Wipe");
		bShowWipeConfirmation = false;
	}

	if (ImGui::BeginPopupModal("Confirm Wipe", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("This will delete ALL save data for the current world.\nThis cannot be undone.");
		ImGui::Separator();

		if (ImGui::Button("Wipe", ImVec2(120, 0)))
		{
			WipeSaveData();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

// ====================================================================
// Operations
// ====================================================================

void FArcWorldPersistenceDebugger::SaveAll()
{
	UArcMassEntityPersistenceSubsystem* PersistSub = GetPersistenceSubsystem();
	if (!PersistSub)
	{
		return;
	}

	for (const TPair<FIntVector, TSet<FGuid>>& Pair : PersistSub->CellEntityMap)
	{
		PersistSub->SaveCell(Pair.Key);
	}

	StatusMessage = TEXT("Saved all cells");
	StatusMessageExpireTime = FPlatformTime::Seconds() + 3.0;
}

void FArcWorldPersistenceDebugger::LoadAll()
{
	UArcMassEntityPersistenceSubsystem* PersistSub = GetPersistenceSubsystem();
	if (!PersistSub)
	{
		return;
	}

	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->GetPawn())
	{
		StatusMessage = TEXT("No local player pawn");
		StatusMessageExpireTime = FPlatformTime::Seconds() + 3.0;
		return;
	}

	const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
	PersistSub->UpdateActiveCells({PlayerLocation});

	StatusMessage = TEXT("Loading cells around player");
	StatusMessageExpireTime = FPlatformTime::Seconds() + 3.0;
}

void FArcWorldPersistenceDebugger::WipeSaveData()
{
	UArcMassEntityPersistenceSubsystem* PersistSub = GetPersistenceSubsystem();
	if (!PersistSub)
	{
		return;
	}

	// Collect all known cell keys before unloading
	TSet<FIntVector> CellsToDelete;
	for (const FIntVector& Cell : PersistSub->LoadedCells)
	{
		CellsToDelete.Add(Cell);
	}
	for (const TPair<FIntVector, TSet<FGuid>>& Pair : PersistSub->CellEntityMap)
	{
		CellsToDelete.Add(Pair.Key);
	}

	// Unload all entities (SaveAndUnloadAll saves then destroys)
	PersistSub->SaveAndUnloadAll();

	// Now delete the saved cell entries from the backend
	UWorld* World = GetDebugWorld();
	UArcPersistenceSubsystem* BackendSub = World ? World->GetGameInstance()->GetSubsystem<UArcPersistenceSubsystem>() : nullptr;
	IArcPersistenceBackend* Backend = BackendSub ? BackendSub->GetBackend() : nullptr;
	if (Backend)
	{
		for (const FIntVector& Cell : CellsToDelete)
		{
			const FString Key = PersistSub->MakeCellStorageKey(Cell);
			Backend->DeleteEntry(Key);
		}
	}

	StatusMessage = TEXT("Wiped save data");
	StatusMessageExpireTime = FPlatformTime::Seconds() + 3.0;
}

// ====================================================================
// Entity List Panel
// ====================================================================

void FArcWorldPersistenceDebugger::DrawEntityList(float Width)
{
	ImGui::BeginChild("##EntityList", ImVec2(Width, 0), ImGuiChildFlags_Borders);

	ImGui::Text("Entities");
	ImGui::Separator();

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##Filter", "Filter...", FilterBuf, sizeof(FilterBuf));

	ImGui::BeginChild("##EntityListScroll", ImVec2(0, 0));

	const FString FilterStr(FilterBuf);

	for (const FEntityEntry& Entry : CachedEntities)
	{
		char Label[64];
		FCStringAnsi::Snprintf(Label, sizeof(Label), "E[%d:%d]", Entry.Handle.Index, Entry.Handle.SerialNumber);

		// Apply filter
		if (FilterBuf[0] != '\0')
		{
			FString LabelStr(Label);
			if (!LabelStr.Contains(FilterStr))
			{
				continue;
			}
		}

		const bool bIsSelected = bHasSelection && SelectedEntity == Entry.Handle;
		if (ImGui::Selectable(Label, bIsSelected))
		{
			SelectedEntity = Entry.Handle;
			bHasSelection = true;
		}
	}

	ImGui::EndChild();
	ImGui::EndChild();
}

// ====================================================================
// Minimap Panel
// ====================================================================

void FArcWorldPersistenceDebugger::DrawMinimap(float Width)
{
	namespace WPColors = Arcx::GameplayDebugger::WorldPersistence;

	ImGui::BeginChild("##Minimap", ImVec2(Width, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	CanvasPos = ImGui::GetCursorScreenPos();
	CanvasSize = ImGui::GetContentRegionAvail();

	if (CanvasSize.x < 1.0f || CanvasSize.y < 1.0f)
	{
		ImGui::EndChild();
		return;
	}

	CanvasCenter = ImVec2(
		CanvasPos.x + CanvasSize.x * 0.5f,
		CanvasPos.y + CanvasSize.y * 0.5f
	);

	ImGui::InvisibleButton("##minimapCanvas", CanvasSize, ImGuiButtonFlags_MouseButtonMiddle | ImGuiButtonFlags_MouseButtonLeft);
	const bool bCanvasHovered = ImGui::IsItemHovered();

	HandleMinimapInput();

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->PushClipRect(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), true);

	DrawList->AddRectFilled(CanvasPos, ImVec2(CanvasPos.x + CanvasSize.x, CanvasPos.y + CanvasSize.y), WPColors::BackgroundColor);

	{
		const ImVec2 Origin = WorldToScreen(0.0f, 0.0f);
		DrawList->AddLine(ImVec2(CanvasPos.x, Origin.y), ImVec2(CanvasPos.x + CanvasSize.x, Origin.y), WPColors::OriginColor, 1.0f);
		DrawList->AddLine(ImVec2(Origin.x, CanvasPos.y), ImVec2(Origin.x, CanvasPos.y + CanvasSize.y), WPColors::OriginColor, 1.0f);
	}

	UArcMassEntityPersistenceSubsystem* PersistSub = GetPersistenceSubsystem();
	if (PersistSub)
	{
		const float CellSize = PersistSub->GetCellSize();
		if (CellSize > 0.0f)
		{
			for (const TPair<FIntVector, TSet<FGuid>>& Pair : PersistSub->CellEntityMap)
			{
				const FIntVector& Coords = Pair.Key;
				const bool bLoaded = PersistSub->LoadedCells.Contains(Coords);

				const float WorldMinX = Coords.X * CellSize;
				const float WorldMinY = Coords.Y * CellSize;
				const float WorldMaxX = WorldMinX + CellSize;
				const float WorldMaxY = WorldMinY + CellSize;

				const ImVec2 ScreenMin = WorldToScreen(WorldMinX, WorldMaxY);
				const ImVec2 ScreenMax = WorldToScreen(WorldMaxX, WorldMinY);

				if (ScreenMax.x < CanvasPos.x || ScreenMin.x > CanvasPos.x + CanvasSize.x ||
					ScreenMax.y < CanvasPos.y || ScreenMin.y > CanvasPos.y + CanvasSize.y)
				{
					continue;
				}

				const ImU32 FillColor = bLoaded ? WPColors::LoadedCellFill : WPColors::UnloadedCellFill;
				const ImU32 BorderColor = bLoaded ? WPColors::LoadedCellBorder : WPColors::UnloadedCellBorder;

				DrawList->AddRectFilled(ScreenMin, ScreenMax, FillColor);
				DrawList->AddRect(ScreenMin, ScreenMax, BorderColor, 0.0f, 0, 1.0f);

				const float CellScreenSize = ScreenMax.x - ScreenMin.x;
				if (CellScreenSize > 24.0f)
				{
					char CountBuf[16];
					FCStringAnsi::Snprintf(CountBuf, sizeof(CountBuf), "%d", Pair.Value.Num());
					const ImVec2 TextSize = ImGui::CalcTextSize(CountBuf);
					DrawList->AddText(
						ImVec2(
							(ScreenMin.x + ScreenMax.x - TextSize.x) * 0.5f,
							(ScreenMin.y + ScreenMax.y - TextSize.y) * 0.5f
						),
						WPColors::HUDTextColor, CountBuf
					);
				}
			}
		}
	}

	// Find hovered entity first
	const ImGuiIO& IO = ImGui::GetIO();
	const FEntityEntry* HoveredEntry = nullptr;
	if (bCanvasHovered)
	{
		float BestDistSq = 64.0f;
		for (const FEntityEntry& Entry : CachedEntities)
		{
			const ImVec2 ScreenPos = WorldToScreen(Entry.Position.X, Entry.Position.Y);
			const float DX = IO.MousePos.x - ScreenPos.x;
			const float DY = IO.MousePos.y - ScreenPos.y;
			const float DistSq = DX * DX + DY * DY;

			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				HoveredEntry = &Entry;
			}
		}
	}

	// Draw entities
	const float EntityRadius = FMath::Clamp(Zoom * 50.0f, 2.0f, 6.0f);

	for (const FEntityEntry& Entry : CachedEntities)
	{
		const ImVec2 ScreenPos = WorldToScreen(Entry.Position.X, Entry.Position.Y);

		if (ScreenPos.x < CanvasPos.x - 10.0f || ScreenPos.x > CanvasPos.x + CanvasSize.x + 10.0f ||
			ScreenPos.y < CanvasPos.y - 10.0f || ScreenPos.y > CanvasPos.y + CanvasSize.y + 10.0f)
		{
			continue;
		}

		const bool bIsSelected = bHasSelection && SelectedEntity == Entry.Handle;
		const bool bIsHovered = HoveredEntry && HoveredEntry->Handle == Entry.Handle;

		ImU32 Color = Entry.bIsSource ? WPColors::SourceEntityColor : WPColors::EntityColor;
		float Radius = Entry.bIsSource ? EntityRadius * 1.5f : EntityRadius;

		if (bIsSelected)
		{
			DrawList->AddCircle(ScreenPos, EntityRadius + 4.0f, WPColors::SelectedEntityRing, 0, 2.0f);
			Color = WPColors::SelectedEntityColor;
			Radius = EntityRadius + 1.0f;
		}
		else if (bIsHovered)
		{
			Color = WPColors::HoveredEntityColor;
			Radius = EntityRadius + 1.0f;
		}

		DrawList->AddCircleFilled(ScreenPos, Radius, Color);
	}

	DrawList->PopClipRect();

	// Tooltip for hovered entity
	if (HoveredEntry)
	{
		ImGui::BeginTooltip();
		ImGui::Text("E[%d:%d]", HoveredEntry->Handle.Index, HoveredEntry->Handle.SerialNumber);
		ImGui::Text("(%.0f, %.0f, %.0f)", HoveredEntry->Position.X, HoveredEntry->Position.Y, HoveredEntry->Position.Z);
		ImGui::EndTooltip();
	}

	ImGui::EndChild();
}

// ====================================================================
// Entity Details Panel
// ====================================================================

void FArcWorldPersistenceDebugger::DrawEntityDetails(float Width)
{
	ImGui::BeginChild("##EntityDetails", ImVec2(Width, 0), ImGuiChildFlags_Borders);

	ImGui::Text("Details");
	ImGui::Separator();

	if (!bHasSelection)
	{
		ImGui::TextDisabled("No entity selected");
		ImGui::EndChild();
		return;
	}

	FMassEntityManager* EntityManager = GetEntityManager();
	if (!EntityManager || !EntityManager->IsEntityValid(SelectedEntity))
	{
		ImGui::TextDisabled("Selected entity is no longer valid");
		bHasSelection = false;
		ImGui::EndChild();
		return;
	}

	ImGui::Text("E[%d:%d]", SelectedEntity.Index, SelectedEntity.SerialNumber);

	const FArcMassPersistenceConfigFragment* Config = nullptr;
	const FMassArchetypeHandle Archetype = EntityManager->GetArchetypeForEntity(SelectedEntity);

	{
		FMassEntityView EntityView(*EntityManager, SelectedEntity);
		Config = EntityView.GetConstSharedFragmentDataPtr<FArcMassPersistenceConfigFragment>();
	}

	const FMassArchetypeCompositionDescriptor& Composition = EntityManager->GetArchetypeComposition(Archetype);

	ImGui::Separator();
	ImGui::BeginChild("##FragmentScroll", ImVec2(0, 0));

	{
		FMassEntityView EntityView(*EntityManager, SelectedEntity);

		TArray<const UScriptStruct*> FragmentTypes;
		Composition.DebugGetFragments().ExportTypes(FragmentTypes);

		for (const UScriptStruct* FragmentType : FragmentTypes)
		{
			if (Config && !FArcMassFragmentSerializer::ShouldSerializeFragment(FragmentType, *Config))
			{
				continue;
			}

			const FString FragmentName = FragmentType->GetName();

			if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*FragmentName), ImGuiTreeNodeFlags_DefaultOpen))
			{
				FStructView FragmentView = EntityView.GetFragmentDataStruct(FragmentType);
				if (!FragmentView.IsValid())
				{
					continue;
				}

				const void* FragmentData = FragmentView.GetMemory();

				for (TFieldIterator<FProperty> PropIt(FragmentType); PropIt; ++PropIt)
				{
					const FProperty* Prop = *PropIt;
					if (!Prop->HasAnyPropertyFlags(CPF_SaveGame))
					{
						continue;
					}

					const FString PropName = Prop->GetName();
					const void* PropValuePtr = Prop->ContainerPtrToValuePtr<void>(FragmentData);

					FString ValueStr;
					Prop->ExportTextItem_Direct(ValueStr, PropValuePtr, nullptr, nullptr, PPF_None);

					ImGui::Text("  %s: %s", TCHAR_TO_ANSI(*PropName), TCHAR_TO_ANSI(*ValueStr));
				}
			}
		}
	}

	ImGui::EndChild();
	ImGui::EndChild();
}
