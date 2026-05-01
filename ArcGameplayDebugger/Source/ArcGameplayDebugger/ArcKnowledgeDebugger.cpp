// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcKnowledgeDebugger.h"

#include "imgui.h"
#include "ArcKnowledgeDebuggerDrawComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "ArcKnowledgeSubsystem.h"
#include "ArcKnowledgeQuery.h"
#include "ArcKnowledgeEntryDefinition.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/UnrealType.h"
#include "MassEntitySubsystem.h"
#include "MassEntityQuery.h"
#include "MassExecutionContext.h"
#include "MassCommonFragments.h"
#include "Mass/ArcKnowledgeFragments.h"
#include "GameFramework/Pawn.h"
#include "ArcAdvertisementInstruction_StateTree.h"
#include "StateTreeReference.h"
#include "StateTree.h"

// ====================================================================
// Helpers
// ====================================================================

namespace Arcx::GameplayDebugger::Knowledge
{
	// Colors
	static const FColor KnowledgeColor(50, 180, 220, 200);
	static const FColor SelectedColor = FColor::Yellow;
	static const FColor ClaimedColor = FColor::Orange;
	static const FColor RadiusColor(80, 180, 220, 80);

	static const ImVec4 ImKnowledgeColor(0.2f, 0.7f, 0.85f, 1.0f);
	static const ImVec4 ImClaimedColor(1.0f, 0.65f, 0.0f, 1.0f);
	static const ImVec4 ImSelectedColor(1.0f, 1.0f, 0.0f, 1.0f);
	static const ImVec4 ImTagColor(0.5f, 0.8f, 1.0f, 1.0f);
	static const ImVec4 ImPayloadColor(0.7f, 1.0f, 0.7f, 1.0f);
	static const ImVec4 ImDimColor(0.5f, 0.5f, 0.5f, 1.0f);

	void DrawPayloadPropertyValue(const FProperty* Prop, const void* ContainerPtr)
	{
		const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerPtr);

		if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
		{
			ImGui::Text("%s", BoolProp->GetPropertyValue(ValuePtr) ? "true" : "false");
		}
		else if (const FIntProperty* IntProp = CastField<FIntProperty>(Prop))
		{
			ImGui::Text("%d", IntProp->GetPropertyValue(ValuePtr));
		}
		else if (const FFloatProperty* FloatProp = CastField<FFloatProperty>(Prop))
		{
			ImGui::Text("%.4f", FloatProp->GetPropertyValue(ValuePtr));
		}
		else if (const FDoubleProperty* DoubleProp = CastField<FDoubleProperty>(Prop))
		{
			ImGui::Text("%.6f", DoubleProp->GetPropertyValue(ValuePtr));
		}
		else if (const FNameProperty* NameProp = CastField<FNameProperty>(Prop))
		{
			ImGui::Text("%s", TCHAR_TO_ANSI(*NameProp->GetPropertyValue(ValuePtr).ToString()));
		}
		else if (const FStrProperty* StrProp = CastField<FStrProperty>(Prop))
		{
			ImGui::Text("%s", TCHAR_TO_ANSI(*StrProp->GetPropertyValue(ValuePtr)));
		}
		else if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			ImGui::Text("%s", TCHAR_TO_ANSI(*TextProp->GetPropertyValue(ValuePtr).ToString()));
		}
		else if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
		{
			FString EnumStr;
			EnumProp->GetUnderlyingProperty()->ExportText_Direct(EnumStr, ValuePtr, ValuePtr, nullptr, 0);
			if (const UEnum* Enum = EnumProp->GetEnum())
			{
				int64 IntVal = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
				FText DisplayText = Enum->GetDisplayNameTextByValue(IntVal);
				if (!DisplayText.IsEmpty())
				{
					EnumStr = DisplayText.ToString();
				}
			}
			ImGui::Text("%s", TCHAR_TO_ANSI(*EnumStr));
		}
		else if (const FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			if (StructProp->Struct == TBaseStructure<FVector>::Get())
			{
				const FVector& V = *static_cast<const FVector*>(ValuePtr);
				ImGui::Text("(%.1f, %.1f, %.1f)", V.X, V.Y, V.Z);
			}
			else if (StructProp->Struct == TBaseStructure<FRotator>::Get())
			{
				const FRotator& R = *static_cast<const FRotator*>(ValuePtr);
				ImGui::Text("(P=%.1f, Y=%.1f, R=%.1f)", R.Pitch, R.Yaw, R.Roll);
			}
			else
			{
				FString NodeId = FString::Printf(TEXT("##%s_%p"), *Prop->GetName(), ValuePtr);
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*NodeId), "%s", TCHAR_TO_ANSI(*StructProp->Struct->GetName())))
				{
					for (TFieldIterator<FProperty> SubIt(StructProp->Struct); SubIt; ++SubIt)
					{
						FProperty* SubProp = *SubIt;
						ImGui::PushID(TCHAR_TO_ANSI(*SubProp->GetName()));
						ImGui::Text("%s:", TCHAR_TO_ANSI(*SubProp->GetName()));
						ImGui::SameLine();
						DrawPayloadPropertyValue(SubProp, ValuePtr);
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
			}
		}
		else if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			const UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
			if (Obj)
			{
				ImGui::Text("%s", TCHAR_TO_ANSI(*Obj->GetName()));
			}
			else
			{
				ImGui::TextDisabled("None");
			}
		}
		else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
		{
			FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
			if (ArrayHelper.Num() == 0)
			{
				ImGui::TextDisabled("Empty [0]");
			}
			else
			{
				FString ArrayLabel = FString::Printf(TEXT("[%d] elements"), ArrayHelper.Num());
				FString ArrayId = FString::Printf(TEXT("##arr_%s_%p"), *Prop->GetName(), ValuePtr);
				if (ImGui::TreeNode(TCHAR_TO_ANSI(*ArrayId), "%s", TCHAR_TO_ANSI(*ArrayLabel)))
				{
					const int32 MaxDisplay = FMath::Min(ArrayHelper.Num(), 32);
					for (int32 i = 0; i < MaxDisplay; ++i)
					{
						ImGui::PushID(i);
						ImGui::Text("[%d]", i);
						ImGui::SameLine();
						DrawPayloadPropertyValue(ArrayProp->Inner, ArrayHelper.GetElementPtr(i));
						ImGui::PopID();
					}
					if (ArrayHelper.Num() > MaxDisplay)
					{
						ImGui::TextDisabled("... %d more", ArrayHelper.Num() - MaxDisplay);
					}
					ImGui::TreePop();
				}
			}
		}
		else
		{
			FString ExportedVal;
			Prop->ExportText_Direct(ExportedVal, ValuePtr, ValuePtr, nullptr, 0);
			if (ExportedVal.Len() > 128)
			{
				ExportedVal = ExportedVal.Left(128) + TEXT("...");
			}
			ImGui::Text("%s", TCHAR_TO_ANSI(*ExportedVal));
		}
	}
}

// ====================================================================
// World helpers
// ====================================================================

UWorld* FArcKnowledgeDebugger::GetDebugWorld()
{
	if (!GEngine || !GEngine->GameViewport)
	{
		return nullptr;
	}
	return GEngine->GameViewport->GetWorld();
}

UArcKnowledgeSubsystem* FArcKnowledgeDebugger::GetKnowledgeSubsystem() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}
	return World->GetSubsystem<UArcKnowledgeSubsystem>();
}

// ====================================================================
// Lifecycle
// ====================================================================

void FArcKnowledgeDebugger::Initialize()
{
	SelectedEntryIndex = INDEX_NONE;
	EntryFilterBuf[0] = '\0';
	CachedEntries.Reset();
	LastRefreshTime = 0.f;
	bDrawAllEntries = true;
	bDrawSelectedRadius = true;
	bDrawLabels = true;
	bAutoRefresh = true;
	bUseSearchRadius = false;
	SearchRadius = 50000.f;
	bShowAddEntryPopup = false;
	bShowAddTagPopup = false;
	bShowRemoveTagPopup = false;
	AddEntryTagsBuf[0] = '\0';
	AddTagBuf[0] = '\0';
	AddEntryRelevance = 1.0f;
	AddEntryLocation[0] = AddEntryLocation[1] = AddEntryLocation[2] = 0.f;
	RefreshEntryList();
	bShowClaimPopup = false;
	ClaimEntityFilterBuf[0] = '\0';
	CachedClaimableEntities.Reset();
}

void FArcKnowledgeDebugger::Uninitialize()
{
	if (DrawActor.IsValid())
	{
		DrawActor->Destroy();
		DrawActor.Reset();
		DrawComponent.Reset();
	}
	CachedClaimableEntities.Reset();
	CachedEntries.Reset();
	SelectedEntryIndex = INDEX_NONE;
}

// ====================================================================
// Refresh
// ====================================================================

void FArcKnowledgeDebugger::RefreshEntryList()
{
	CachedEntries.Reset();

	UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
	if (!Sub)
	{
		return;
	}

	UWorld* World = GetDebugWorld();

	// If radius filtering is enabled, use spatial query from player pawn location
	if (bUseSearchRadius && World)
	{
		FVector QueryOrigin = FVector::ZeroVector;
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				QueryOrigin = Pawn->GetActorLocation();
			}
		}

		TArray<FArcKnowledgeHandle> AllHandles;
		FGameplayTagQuery EmptyFilter;
		Sub->QueryKnowledgeInRadius(QueryOrigin, SearchRadius, AllHandles, EmptyFilter);

		for (const FArcKnowledgeHandle& Handle : AllHandles)
		{
			const FArcKnowledgeEntry* EntryPtr = Sub->GetKnowledgeEntry(Handle);
			if (!EntryPtr)
			{
				continue;
			}
			const FArcKnowledgeEntry& Entry = *EntryPtr;

			FKnowledgeListEntry& ListEntry = CachedEntries.AddDefaulted_GetRef();
			ListEntry.Handle = Entry.Handle;
			ListEntry.Location = Entry.Location;
			ListEntry.Tags = Entry.Tags;
			ListEntry.Relevance = Entry.Relevance;
			ListEntry.bClaimed = Entry.bClaimed;
			ListEntry.bHasPayload = Entry.Payload.IsValid();
			ListEntry.BoundingRadius = Entry.SpatialBroadcastRadius;

			FString TagSummary;
			if (Entry.Tags.Num() > 0)
			{
				TagSummary = Entry.Tags.First().ToString();
				if (Entry.Tags.Num() > 1)
				{
					TagSummary += FString::Printf(TEXT(" +%d"), Entry.Tags.Num() - 1);
				}
			}
			else
			{
				TagSummary = TEXT("(no tags)");
			}
			ListEntry.Label = FString::Printf(TEXT("H%u [%s]"), Entry.Handle.GetValue(), *TagSummary);
		}
		return;
	}

	// No radius filter — return all entries
	FArcKnowledgeQuery AllQuery;
	AllQuery.MaxResults = 500;

	FArcKnowledgeQueryContext QueryContext;
	QueryContext.World = World;
	QueryContext.CurrentTime = World ? World->GetTimeSeconds() : 0.0;

	TArray<FArcKnowledgeQueryResult> AllResults;
	Sub->QueryKnowledge(AllQuery, QueryContext, AllResults);

	for (const FArcKnowledgeQueryResult& Result : AllResults)
	{
		const FArcKnowledgeEntry& Entry = Result.Entry;

		FKnowledgeListEntry& ListEntry = CachedEntries.AddDefaulted_GetRef();
		ListEntry.Handle = Entry.Handle;
		ListEntry.Location = Entry.Location;
		ListEntry.Tags = Entry.Tags;
		ListEntry.Relevance = Entry.Relevance;
		ListEntry.bClaimed = Entry.bClaimed;
		ListEntry.bHasPayload = Entry.Payload.IsValid();
		ListEntry.BoundingRadius = Entry.SpatialBroadcastRadius;

		// Build label
		FString TagSummary;
		if (Entry.Tags.Num() > 0)
		{
			TagSummary = Entry.Tags.First().ToString();
			if (Entry.Tags.Num() > 1)
			{
				TagSummary += FString::Printf(TEXT(" +%d"), Entry.Tags.Num() - 1);
			}
		}
		else
		{
			TagSummary = TEXT("(no tags)");
		}

		ListEntry.Label = FString::Printf(TEXT("H%u [%s]"), Entry.Handle.GetValue(), *TagSummary);
	}
}

void FArcKnowledgeDebugger::RefreshClaimableEntities()
{
	CachedClaimableEntities.Reset();

	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	UMassEntitySubsystem* MassSubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!MassSubsystem)
	{
		return;
	}

	FMassEntityManager& EntityManager = MassSubsystem->GetMutableEntityManager();

	FMassExecutionContext ExecContext(EntityManager);
	FMassEntityQuery Query(EntityManager.AsShared());
	Query.AddRequirement<FArcKnowledgeMemberFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	Query.AddTagRequirement<FArcKnowledgeMemberTag>(EMassFragmentPresence::All);

	Query.ForEachEntityChunk(ExecContext,
		[this](FMassExecutionContext& Context)
		{
			TConstArrayView<FArcKnowledgeMemberFragment> MemberList = Context.GetFragmentView<FArcKnowledgeMemberFragment>();
			TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();

			const int32 NumEntities = Context.GetNumEntities();
			for (int32 i = 0; i < NumEntities; ++i)
			{
				const FMassEntityHandle Entity = Context.GetEntity(i);
				const FArcKnowledgeMemberFragment& Member = MemberList[i];
				const FTransformFragment& Transform = TransformList[i];

				FClaimableEntityEntry& Entry = CachedClaimableEntities.AddDefaulted_GetRef();
				Entry.Entity = Entity;
				Entry.Location = Transform.GetTransform().GetLocation();
				Entry.Role = Member.Role;

				FString RoleStr = Member.Role.IsValid() ? Member.Role.ToString() : TEXT("(no role)");
				Entry.Label = FString::Printf(TEXT("E%d [%s]"), Entity.Index, *RoleStr);
			}
		});
}

// ====================================================================
// Main Draw
// ====================================================================

void FArcKnowledgeDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(1100, 750), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Knowledge Debugger", &bShow))
	{
		ImGui::End();
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearEntryData();
		}
		return;
	}

	UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
	if (!Sub)
	{
		ImGui::TextDisabled("No ArcKnowledgeSubsystem available");
		ImGui::End();
		return;
	}

	DrawToolbar();

	// Auto-refresh
	if (bAutoRefresh)
	{
		UWorld* World = GetDebugWorld();
		if (World)
		{
			float CurrentTime = World->GetTimeSeconds();
			if (CurrentTime - LastRefreshTime > 2.0f)
			{
				LastRefreshTime = CurrentTime;
				RefreshEntryList();
			}
		}
	}

	ImGui::Separator();

	// Split into two panes
	float PanelWidth = ImGui::GetContentRegionAvail().x;
	float LeftPanelWidth = PanelWidth * 0.30f;

	// Left panel: entry list
	if (ImGui::BeginChild("KnowledgeListPanel", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		DrawEntryListPanel();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// Right panel: entry detail
	if (ImGui::BeginChild("KnowledgeDetailPanel", ImVec2(0, 0), ImGuiChildFlags_Borders))
	{
		DrawEntryDetailPanel();
	}
	ImGui::EndChild();

	ImGui::End();

	// World visualization
	DrawWorldVisualization();

	// Popups
	DrawAddEntryPopup();
	if (SelectedEntryIndex != INDEX_NONE && CachedEntries.IsValidIndex(SelectedEntryIndex))
	{
		FArcKnowledgeHandle Handle = CachedEntries[SelectedEntryIndex].Handle;
		const FArcKnowledgeEntry* Entry = Sub->GetKnowledgeEntry(Handle);
		if (Entry)
		{
			DrawAddTagPopup(Handle);
			DrawRemoveTagPopup(Handle, Entry->Tags);
		}
	}
	DrawClaimEntityPopup();
}

// ====================================================================
// Toolbar
// ====================================================================

void FArcKnowledgeDebugger::DrawToolbar()
{
	if (ImGui::Button("Refresh"))
	{
		RefreshEntryList();
	}
	ImGui::SameLine();
	ImGui::Text("Entries: %d", CachedEntries.Num());
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();
	ImGui::Checkbox("Auto-Refresh", &bAutoRefresh);
	ImGui::SameLine();
	ImGui::Checkbox("Draw All", &bDrawAllEntries);
	ImGui::SameLine();
	ImGui::Checkbox("Draw Radius", &bDrawSelectedRadius);
	ImGui::SameLine();
	ImGui::Checkbox("Labels", &bDrawLabels);

	ImGui::SameLine();
	ImGui::Checkbox("Radius Filter", &bUseSearchRadius);
	if (bUseSearchRadius)
	{
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.f);
		ImGui::DragFloat("##SearchRadius", &SearchRadius, 500.f, 1000.f, 500000.f, "%.0f");
	}

	// Add entry button
	ImGui::SameLine();
	ImGui::Separator();
	ImGui::SameLine();
	if (ImGui::Button("+ Add Entry"))
	{
		bShowAddEntryPopup = true;
		AddEntryTagsBuf[0] = '\0';
		AddEntryRelevance = 1.0f;

		// Default location to camera position
		UWorld* World = GetDebugWorld();
		if (World)
		{
			APlayerController* PC = World->GetFirstPlayerController();
			if (PC)
			{
				FVector CamLoc;
				FRotator CamRot;
				PC->GetPlayerViewPoint(CamLoc, CamRot);
				AddEntryLocation[0] = static_cast<float>(CamLoc.X);
				AddEntryLocation[1] = static_cast<float>(CamLoc.Y);
				AddEntryLocation[2] = static_cast<float>(CamLoc.Z);
			}
		}
	}
}

// ====================================================================
// Entry List Panel (Left)
// ====================================================================

void FArcKnowledgeDebugger::DrawEntryListPanel()
{
	ImGui::Text("Knowledge Entries");
	ImGui::Separator();

	ImGui::InputText("Filter", EntryFilterBuf, IM_ARRAYSIZE(EntryFilterBuf));

	FString Filter = FString(ANSI_TO_TCHAR(EntryFilterBuf)).ToLower();

	if (ImGui::BeginChild("KnowledgeScroll", ImVec2(0, 0)))
	{
		for (int32 i = 0; i < CachedEntries.Num(); ++i)
		{
			const FKnowledgeListEntry& Entry = CachedEntries[i];

			if (!Filter.IsEmpty() && !Entry.Label.ToLower().Contains(Filter))
			{
				continue;
			}

			const bool bSelected = (SelectedEntryIndex == i);

			ImVec4 Color = Arcx::GameplayDebugger::Knowledge::ImKnowledgeColor;
			if (Entry.bClaimed)
			{
				Color = Arcx::GameplayDebugger::Knowledge::ImClaimedColor;
			}
			if (Entry.Relevance < 0.3f)
			{
				Color.w = 0.5f; // Dim low-relevance entries
			}

			ImGui::PushStyleColor(ImGuiCol_Text, Color);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*Entry.Label), bSelected))
			{
				SelectedEntryIndex = i;
			}
			ImGui::PopStyleColor();

			// Tooltip with quick info
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::Text("Handle: %u", Entry.Handle.GetValue());
				ImGui::Text("Location: (%.0f, %.0f, %.0f)", Entry.Location.X, Entry.Location.Y, Entry.Location.Z);
				ImGui::Text("Relevance: %.2f", Entry.Relevance);
				ImGui::Text("Tags: %s", TCHAR_TO_ANSI(*Entry.Tags.ToStringSimple()));
				if (Entry.bClaimed) ImGui::TextColored(Arcx::GameplayDebugger::Knowledge::ImClaimedColor, "CLAIMED");
				if (Entry.bHasPayload) ImGui::TextColored(Arcx::GameplayDebugger::Knowledge::ImPayloadColor, "Has Payload");
				ImGui::EndTooltip();
			}
		}
	}
	ImGui::EndChild();
}

// ====================================================================
// Entry Detail Panel (Right)
// ====================================================================

void FArcKnowledgeDebugger::DrawEntryDetailPanel()
{
	if (SelectedEntryIndex == INDEX_NONE || !CachedEntries.IsValidIndex(SelectedEntryIndex))
	{
		ImGui::TextDisabled("Select a knowledge entry from the list");
		return;
	}

	UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
	if (!Sub)
	{
		return;
	}

	const FKnowledgeListEntry& ListEntry = CachedEntries[SelectedEntryIndex];
	const FArcKnowledgeEntry* Entry = Sub->GetKnowledgeEntry(ListEntry.Handle);

	if (!Entry)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Entry no longer exists (H%u)", ListEntry.Handle.GetValue());
		return;
	}

	// Header
	ImGui::TextColored(Arcx::GameplayDebugger::Knowledge::ImSelectedColor, "Knowledge Entry H%u", ListEntry.Handle.GetValue());
	ImGui::Spacing();

	// Location
	ImGui::Text("Location: (%.1f, %.1f, %.1f)", Entry->Location.X, Entry->Location.Y, Entry->Location.Z);

	// Relevance bar
	ImVec4 RelColor;
	if (Entry->Relevance > 0.7f)
	{
		RelColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
	}
	else if (Entry->Relevance > 0.3f)
	{
		RelColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
	}
	else
	{
		RelColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
	}
	ImGui::PushStyleColor(ImGuiCol_PlotHistogram, RelColor);
	FString RelLabel = FString::Printf(TEXT("Relevance: %.3f"), Entry->Relevance);
	ImGui::ProgressBar(Entry->Relevance, ImVec2(-FLT_MIN, 0), TCHAR_TO_ANSI(*RelLabel));
	ImGui::PopStyleColor();

	// Source entity
	if (Entry->SourceEntity.IsValid())
	{
		ImGui::Text("Source Entity: E%d (SN:%d)", Entry->SourceEntity.Index, Entry->SourceEntity.SerialNumber);
	}
	else
	{
		ImGui::TextDisabled("Source Entity: None");
	}

	// Broadcast radius
	if (Entry->SpatialBroadcastRadius > 0.f)
	{
		ImGui::Text("Broadcast Radius: %.0f", Entry->SpatialBroadcastRadius);
	}

	// Timestamp
	DrawMetadataSection(*Entry);

	ImGui::Spacing();
	ImGui::Separator();

	// Tags
	DrawTagsSection(*Entry, ListEntry.Handle);

	ImGui::Spacing();
	ImGui::Separator();

	// Payload
	DrawPayloadSection(*Entry);

	ImGui::Spacing();
	ImGui::Separator();

	// Advertisement status
	DrawAdvertisementSection(*Entry, ListEntry.Handle);

	ImGui::Spacing();
	ImGui::Separator();

	// Instruction
	DrawInstructionSection(*Entry);

	ImGui::Spacing();
	ImGui::Separator();

	// Danger zone
	if (ImGui::CollapsingHeader("Actions"))
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
		if (ImGui::Button("Remove Entry"))
		{
			Sub->RemoveKnowledge(ListEntry.Handle);
			SelectedEntryIndex = INDEX_NONE;
			RefreshEntryList();
		}
		ImGui::PopStyleColor();

		ImGui::SameLine();
		if (ImGui::Button("Refresh Relevance"))
		{
			Sub->RefreshKnowledge(ListEntry.Handle, 1.0f);
			RefreshEntryList();
		}
	}
}

// ====================================================================
// Tags Section
// ====================================================================

void FArcKnowledgeDebugger::DrawTagsSection(const FArcKnowledgeEntry& Entry, FArcKnowledgeHandle Handle)
{
	FString TagHeader = FString::Printf(TEXT("Tags (%d)"), Entry.Tags.Num());
	if (ImGui::CollapsingHeader(TCHAR_TO_ANSI(*TagHeader), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (Entry.Tags.Num() == 0)
		{
			ImGui::TextDisabled("No tags");
		}
		else
		{
			for (const FGameplayTag& Tag : Entry.Tags)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*Tag.ToString()));
			}
		}

		ImGui::Spacing();

		if (ImGui::SmallButton("+ Add Tag"))
		{
			bShowAddTagPopup = true;
			AddTagBuf[0] = '\0';
		}
		ImGui::SameLine();
		if (Entry.Tags.Num() > 0)
		{
			if (ImGui::SmallButton("- Remove Tag"))
			{
				bShowRemoveTagPopup = true;
			}
		}
	}
}

// ====================================================================
// Payload Section
// ====================================================================

void FArcKnowledgeDebugger::DrawPayloadSection(const FArcKnowledgeEntry& Entry)
{
	if (ImGui::CollapsingHeader("Payload", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!Entry.Payload.IsValid())
		{
			ImGui::TextDisabled("No payload");
			return;
		}

		const UScriptStruct* PayloadType = Entry.Payload.GetScriptStruct();
		ImGui::TextColored(Arcx::GameplayDebugger::Knowledge::ImPayloadColor, "Type: %s", TCHAR_TO_ANSI(*PayloadType->GetName()));
		ImGui::Spacing();

		DrawPayloadProperties(PayloadType, Entry.Payload.GetMemory());
	}
}

// ====================================================================
// Payload Properties
// ====================================================================

void FArcKnowledgeDebugger::DrawPayloadProperties(const UScriptStruct* PayloadType, const void* PayloadData)
{
	if (!PayloadType || !PayloadData)
	{
		return;
	}

	int32 PropCount = 0;
	for (TFieldIterator<FProperty> It(PayloadType); It; ++It)
	{
		PropCount++;
	}

	if (PropCount == 0)
	{
		ImGui::TextDisabled("No reflected properties");
		return;
	}

	if (ImGui::BeginTable("##payloadprops", 2, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg | ImGuiTableFlags_NoBordersInBody))
	{
		ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 200.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		for (TFieldIterator<FProperty> It(PayloadType); It; ++It)
		{
			FProperty* Prop = *It;
			ImGui::PushID(TCHAR_TO_ANSI(*Prop->GetName()));
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextDisabled("%s", TCHAR_TO_ANSI(*Prop->GetName()));
			ImGui::TableSetColumnIndex(1);
			Arcx::GameplayDebugger::Knowledge::DrawPayloadPropertyValue(Prop, PayloadData);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
}

// ====================================================================
// Advertisement Section
// ====================================================================

void FArcKnowledgeDebugger::DrawAdvertisementSection(const FArcKnowledgeEntry& Entry, FArcKnowledgeHandle Handle)
{
	if (ImGui::CollapsingHeader("Advertisement Status"))
	{
		if (Entry.bClaimed)
		{
			ImGui::TextColored(Arcx::GameplayDebugger::Knowledge::ImClaimedColor, "CLAIMED");
			ImGui::Text("Claimed By: E%d (SN:%d)", Entry.ClaimedBy.Index, Entry.ClaimedBy.SerialNumber);

			UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
			if (Sub)
			{
				if (ImGui::SmallButton("Cancel Claim"))
				{
					Sub->CancelAdvertisement(Handle);
					RefreshEntryList();
				}
				ImGui::SameLine();
				if (ImGui::SmallButton("Complete"))
				{
					Sub->CompleteAdvertisement(Handle);
					SelectedEntryIndex = INDEX_NONE;
					RefreshEntryList();
				}
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Available (unclaimed)");
			ImGui::SameLine();
			if (ImGui::SmallButton("Claim -> Entity"))
			{
				bShowClaimPopup = true;
				ClaimEntityFilterBuf[0] = '\0';
				RefreshClaimableEntities();
			}
		}
	}
}

// ====================================================================
// Instruction Section
// ====================================================================

void FArcKnowledgeDebugger::DrawInstructionSection(const FArcKnowledgeEntry& Entry)
{
	if (ImGui::CollapsingHeader("Instruction", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!Entry.Instruction.IsValid())
		{
			ImGui::TextDisabled("No instruction");
			return;
		}

		const UScriptStruct* InstructionType = Entry.Instruction.GetScriptStruct();
		ImGui::Text("Type: %s", TCHAR_TO_ANSI(*InstructionType->GetName()));

		if (const FArcAdvertisementInstruction_StateTree* STInstruction = Entry.Instruction.GetPtr<FArcAdvertisementInstruction_StateTree>())
		{
			const UStateTree* StateTree = STInstruction->StateTreeReference.GetStateTree();
			if (StateTree)
			{
				ImGui::Text("StateTree: %s", TCHAR_TO_ANSI(*StateTree->GetName()));
			}
			else
			{
				ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "StateTree: None (not set)");
			}
		}
		else
		{
			// Generic instruction — render properties
			DrawPayloadProperties(InstructionType, Entry.Instruction.GetMemory());
		}
	}
}

// ====================================================================
// Metadata Section
// ====================================================================

void FArcKnowledgeDebugger::DrawMetadataSection(const FArcKnowledgeEntry& Entry)
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	double CurrentTime = World->GetTimeSeconds();
	double Age = CurrentTime - Entry.Timestamp;

	ImGui::TextDisabled("Age: %.1fs | Timestamp: %.1f", Age, Entry.Timestamp);
}

// ====================================================================
// World Visualization
// ====================================================================

void FArcKnowledgeDebugger::EnsureDrawActor(UWorld* World)
{
	if (DrawActor.IsValid())
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.ObjectFlags = RF_Transient;
	AActor* NewActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
	if (!NewActor)
	{
		return;
	}

#if WITH_EDITOR
	NewActor->SetActorLabel(TEXT("KnowledgeDebuggerDraw"));
#endif

	UArcKnowledgeDebuggerDrawComponent* NewComponent = NewObject<UArcKnowledgeDebuggerDrawComponent>(NewActor, UArcKnowledgeDebuggerDrawComponent::StaticClass());
	NewComponent->RegisterComponent();
	NewActor->AddInstanceComponent(NewComponent);

	DrawActor = NewActor;
	DrawComponent = NewComponent;
}

void FArcKnowledgeDebugger::DrawWorldVisualization()
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return;
	}

	if (CachedEntries.Num() == 0)
	{
		if (DrawComponent.IsValid())
		{
			DrawComponent->ClearEntryData();
		}
		return;
	}

	EnsureDrawActor(World);
	if (!DrawComponent.IsValid())
	{
		return;
	}

	// Build lightweight draw entries from cached data
	TArray<FArcKnowledgeDebugDrawEntry> DrawEntries;
	DrawEntries.Reserve(CachedEntries.Num());

	UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();

	for (int32 i = 0; i < CachedEntries.Num(); ++i)
	{
		const FKnowledgeListEntry& Src = CachedEntries[i];
		FArcKnowledgeDebugDrawEntry& Dst = DrawEntries.AddDefaulted_GetRef();
		Dst.Location = Src.Location;
		Dst.Relevance = Src.Relevance;
		Dst.bClaimed = Src.bClaimed;

		// Short label for non-selected, detail label for selected
		if (i == SelectedEntryIndex && Sub)
		{
			const FArcKnowledgeEntry* Entry = Sub->GetKnowledgeEntry(Src.Handle);
			if (Entry)
			{
				Dst.Label = FString::Printf(TEXT("H%u"), Src.Handle.GetValue());
				if (Entry->Tags.Num() > 0)
				{
					Dst.Label += TEXT("\n") + Entry->Tags.ToStringSimple();
				}
				Dst.BroadcastRadius = Entry->SpatialBroadcastRadius;
			}
		}
		else
		{
			Dst.Label = FString::Printf(TEXT("H%u"), Src.Handle.GetValue());
		}
	}

	DrawComponent->UpdateEntryData(DrawEntries, SelectedEntryIndex, bDrawAllEntries, bDrawLabels, bDrawSelectedRadius);
}

// ====================================================================
// Add Entry Popup
// ====================================================================

void FArcKnowledgeDebugger::DrawAddEntryPopup()
{
	if (!bShowAddEntryPopup)
	{
		return;
	}

	ImGui::OpenPopup("Add Knowledge Entry");

	if (ImGui::BeginPopupModal("Add Knowledge Entry", &bShowAddEntryPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Tags (comma-separated gameplay tag strings):");
		ImGui::InputText("##addtags", AddEntryTagsBuf, IM_ARRAYSIZE(AddEntryTagsBuf));

		ImGui::Text("Location:");
		ImGui::InputFloat3("##addloc", AddEntryLocation);

		ImGui::Text("Relevance:");
		ImGui::SliderFloat("##addrel", &AddEntryRelevance, 0.0f, 1.0f);

		ImGui::Spacing();

		if (ImGui::Button("Register", ImVec2(120, 0)))
		{
			UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
			if (Sub)
			{
				FArcKnowledgeEntry NewEntry;
				NewEntry.Location = FVector(AddEntryLocation[0], AddEntryLocation[1], AddEntryLocation[2]);
				NewEntry.Relevance = AddEntryRelevance;

				// Parse tags
				FString TagStr = ANSI_TO_TCHAR(AddEntryTagsBuf);
				TArray<FString> TagStrings;
				TagStr.ParseIntoArray(TagStrings, TEXT(","));
				for (FString& S : TagStrings)
				{
					S.TrimStartAndEndInline();
					FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*S), false);
					if (Tag.IsValid())
					{
						NewEntry.Tags.AddTag(Tag);
					}
				}

				Sub->RegisterKnowledge(NewEntry);
				RefreshEntryList();
			}

			bShowAddEntryPopup = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowAddEntryPopup = false;
		}

		ImGui::EndPopup();
	}
}

// ====================================================================
// Add Tag Popup
// ====================================================================

void FArcKnowledgeDebugger::DrawAddTagPopup(FArcKnowledgeHandle Handle)
{
	if (!bShowAddTagPopup)
	{
		return;
	}

	ImGui::OpenPopup("Add Tag");

	if (ImGui::BeginPopupModal("Add Tag", &bShowAddTagPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Gameplay Tag:");
		ImGui::InputText("##addtaginput", AddTagBuf, IM_ARRAYSIZE(AddTagBuf));

		ImGui::Spacing();

		if (ImGui::Button("Add", ImVec2(120, 0)))
		{
			UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
			if (Sub)
			{
				FString TagStr = ANSI_TO_TCHAR(AddTagBuf);
				TagStr.TrimStartAndEndInline();
				FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagStr), false);
				if (Tag.IsValid())
				{
					Sub->AddTagToKnowledge(Handle, Tag);
					RefreshEntryList();
				}
			}
			bShowAddTagPopup = false;
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowAddTagPopup = false;
		}

		ImGui::EndPopup();
	}
}

// ====================================================================
// Remove Tag Popup
// ====================================================================

void FArcKnowledgeDebugger::DrawRemoveTagPopup(FArcKnowledgeHandle Handle, const FGameplayTagContainer& CurrentTags)
{
	if (!bShowRemoveTagPopup)
	{
		return;
	}

	ImGui::OpenPopup("Remove Tag");

	if (ImGui::BeginPopupModal("Remove Tag", &bShowRemoveTagPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("Select tag to remove:");
		ImGui::Spacing();

		bool bRemoved = false;
		for (const FGameplayTag& Tag : CurrentTags)
		{
			FString ButtonLabel = Tag.ToString();
			if (ImGui::Button(TCHAR_TO_ANSI(*ButtonLabel)))
			{
				UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
				if (Sub)
				{
					Sub->RemoveTagFromKnowledge(Handle, Tag);
					RefreshEntryList();
				}
				bRemoved = true;
				bShowRemoveTagPopup = false;
			}
		}

		if (!bRemoved)
		{
			ImGui::Spacing();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				bShowRemoveTagPopup = false;
			}
		}

		ImGui::EndPopup();
	}
}

// ====================================================================
// Claim Entity Popup
// ====================================================================

void FArcKnowledgeDebugger::DrawClaimEntityPopup()
{
	if (!bShowClaimPopup)
	{
		return;
	}

	if (SelectedEntryIndex == INDEX_NONE || !CachedEntries.IsValidIndex(SelectedEntryIndex))
	{
		bShowClaimPopup = false;
		return;
	}

	ImGui::OpenPopup("Claim Advertisement");

	if (ImGui::BeginPopupModal("Claim Advertisement", &bShowClaimPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		UArcKnowledgeSubsystem* Sub = GetKnowledgeSubsystem();
		const FKnowledgeListEntry& ListEntry = CachedEntries[SelectedEntryIndex];

		ImGui::Text("Entry: H%u", ListEntry.Handle.GetValue());
		ImGui::Text("Tags: %s", TCHAR_TO_ANSI(*ListEntry.Tags.ToStringSimple()));
		ImGui::Separator();

		ImGui::InputText("Search Entities", ClaimEntityFilterBuf, IM_ARRAYSIZE(ClaimEntityFilterBuf));

		if (ImGui::SmallButton("Refresh Entities"))
		{
			RefreshClaimableEntities();
		}

		ImGui::Spacing();
		ImGui::Text("Knowledge Members (%d):", CachedClaimableEntities.Num());

		FString EntFilter = FString(ANSI_TO_TCHAR(ClaimEntityFilterBuf)).ToLower();

		if (ImGui::BeginChild("ClaimEntityList", ImVec2(500, 300), ImGuiChildFlags_Borders))
		{
			for (int32 i = 0; i < CachedClaimableEntities.Num(); ++i)
			{
				const FClaimableEntityEntry& EntEntry = CachedClaimableEntities[i];

				if (!EntFilter.IsEmpty() && !EntEntry.Label.ToLower().Contains(EntFilter))
				{
					continue;
				}

				ImGui::PushID(i);

				ImGui::Text("%s", TCHAR_TO_ANSI(*EntEntry.Label));

				if (ImGui::IsItemHovered())
				{
					ImGui::BeginTooltip();
					ImGui::Text("Entity: E%d (SN:%d)", EntEntry.Entity.Index, EntEntry.Entity.SerialNumber);
					ImGui::Text("Location: (%.0f, %.0f, %.0f)", EntEntry.Location.X, EntEntry.Location.Y, EntEntry.Location.Z);
					if (EntEntry.Role.IsValid())
					{
						ImGui::Text("Role: %s", TCHAR_TO_ANSI(*EntEntry.Role.ToString()));
					}
					ImGui::EndTooltip();
				}

				ImGui::SameLine(420);
				if (ImGui::SmallButton("Claim"))
				{
					if (Sub)
					{
						Sub->ClaimAdvertisement(ListEntry.Handle, EntEntry.Entity);
						RefreshEntryList();
					}
					bShowClaimPopup = false;
				}

				ImGui::PopID();
			}
		}
		ImGui::EndChild();

		ImGui::Spacing();
		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowClaimPopup = false;
		}

		ImGui::EndPopup();
	}
}
