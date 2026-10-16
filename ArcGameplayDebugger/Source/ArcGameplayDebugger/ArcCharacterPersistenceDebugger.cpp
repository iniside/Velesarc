// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCharacterPersistenceDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "MassAgentComponent.h"
#include "MassEntitySubsystem.h"
#include "Mass/EntityManager.h"
#include "Mass/EntityView.h"
#include "ArcPlayerPersistenceSubsystem.h"
#include "Fragments/ArcMassItemStoreFragment.h"
#include "Attributes/ArcAttribute.h"
#include "Modifiers/ArcModifierFunctions.h"
#include "Fragments/ArcPendingAttributeOpsFragment.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCharacterPersistenceDebugger, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::Initialize()
{
	CachedPlayerId = GetPlayerId();
	FMemory::Memzero(CharacterNameBuf, sizeof(CharacterNameBuf));
	SaveStatus.Reset();
	LoadStatus.Reset();
	AttributeStatus.Reset();
	SelectedSaveCharacterIndex = -1;
	SelectedLoadCharacterIndex = -1;
	SelectedAttrFragmentIndex = -1;
	EditedAttributeValues.Reset();
	RefreshSavedCharacters();
}

void FArcCharacterPersistenceDebugger::Uninitialize()
{
	CachedPlayerId.Invalidate();
	FMemory::Memzero(CharacterNameBuf, sizeof(CharacterNameBuf));
	SavedCharacterNames.Reset();
	SelectedSaveCharacterIndex = -1;
	SelectedLoadCharacterIndex = -1;
	SaveStatus.Reset();
	LoadStatus.Reset();
	SelectedAttrFragmentIndex = -1;
	EditedAttributeValues.Reset();
	AttributeStatus.Reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::Draw()
{
	ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Character Persistence", &bShow);

	ImGui::TextDisabled("Player: %s", TCHAR_TO_ANSI(*CachedPlayerId.ToString()));
	ImGui::Separator();

	if (ImGui::BeginTabBar("##CharPersistTabs"))
	{
		if (ImGui::BeginTabItem("Save"))
		{
			DrawSaveTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Load"))
		{
			DrawLoadTab();
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("Attributes"))
		{
			DrawAttributesTab();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();
}

// ─────────────────────────────────────────────────────────────────────────────
// Save Tab
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::DrawSaveTab()
{
	const float LeftPanelWidth = 180.0f;

	// ── Left panel: saved character list + new name input ──────────────────
	if (ImGui::BeginChild("SaveCharList", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		ImGui::Text("Characters");
		ImGui::Separator();

		if (SavedCharacterNames.IsEmpty())
		{
			ImGui::TextDisabled("(none)");
		}
		else
		{
			for (int32 i = 0; i < SavedCharacterNames.Num(); ++i)
			{
				const bool bIsSelected = (SelectedSaveCharacterIndex == i);
				if (ImGui::Selectable(TCHAR_TO_ANSI(*SavedCharacterNames[i]), bIsSelected))
				{
					SelectedSaveCharacterIndex = i;
				}
			}
		}

		ImGui::Separator();
		ImGui::Text("New:");
		ImGui::InputText("##NewCharName", CharacterNameBuf, IM_ARRAYSIZE(CharacterNameBuf));

		if (ImGui::Button("Refresh"))
		{
			RefreshSavedCharacters();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// ── Right panel: summary + actions ─────────────────────────────────────
	if (ImGui::BeginChild("SaveDetails", ImVec2(0, 0)))
	{
		// Summary: item store count from entity fragment
		ImGui::Text("Will save:");

		FMassEntityHandle Entity;
		FMassEntityManager* Manager = nullptr;
		const bool bEntityReady = GetPlayerEntity(Entity, Manager);

		if (bEntityReady && Manager != nullptr)
		{
			const FArcMassItemStoreFragment* Store = Manager->GetSparseElementDataForEntity(
				FArcMassItemStoreFragment::StaticStruct(), Entity).GetPtr<FArcMassItemStoreFragment>();
			if (Store != nullptr)
			{
				ImGui::BulletText("Item store: %d items", Store->ReplicatedItems.Items.Num());
			}
			else
			{
				ImGui::BulletText("Item store: (no FArcMassItemStoreFragment)");
			}
		}
		else
		{
			ImGui::TextDisabled("(entity not ready)");
		}

		ImGui::Separator();

		// Determine save name: prefer New buffer, else selected existing
		const bool bHasNewName = FCStringAnsi::Strlen(CharacterNameBuf) > 0;
		const bool bHasSelection = SavedCharacterNames.IsValidIndex(SelectedSaveCharacterIndex);
		const bool bCanSave = bHasNewName || bHasSelection;

		if (!bCanSave)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Save"))
		{
			FString SaveName = bHasNewName
				? FString(ANSI_TO_TCHAR(CharacterNameBuf))
				: SavedCharacterNames[SelectedSaveCharacterIndex];
			SaveCharacter(SaveName);
		}

		if (!bCanSave)
		{
			ImGui::EndDisabled();
		}

		ImGui::SameLine();

		if (!bHasSelection)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Delete"))
		{
			DeleteCharacter(SavedCharacterNames[SelectedSaveCharacterIndex]);
			SelectedSaveCharacterIndex = -1;
		}

		if (!bHasSelection)
		{
			ImGui::EndDisabled();
		}

		if (!SaveStatus.IsEmpty())
		{
			ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*SaveStatus));
		}
	}
	ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// Load Tab
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::DrawLoadTab()
{
	if (ImGui::Button("Refresh"))
	{
		RefreshSavedCharacters();
	}

	ImGui::Separator();
	ImGui::Text("Saved Characters:");

	if (SavedCharacterNames.IsEmpty())
	{
		ImGui::TextDisabled("(none found)");
	}
	else
	{
		for (int32 i = 0; i < SavedCharacterNames.Num(); ++i)
		{
			const bool bIsSelected = (SelectedLoadCharacterIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*SavedCharacterNames[i]), bIsSelected))
			{
				SelectedLoadCharacterIndex = i;
			}
		}
	}

	ImGui::Separator();

	const bool bHasSelection = SavedCharacterNames.IsValidIndex(SelectedLoadCharacterIndex);

	if (!bHasSelection)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Load Character"))
	{
		LoadCharacter(SavedCharacterNames[SelectedLoadCharacterIndex]);
	}

	ImGui::SameLine();

	if (ImGui::Button("Delete Character"))
	{
		DeleteCharacter(SavedCharacterNames[SelectedLoadCharacterIndex]);
		SelectedLoadCharacterIndex = -1;
	}

	if (!bHasSelection)
	{
		ImGui::EndDisabled();
	}

	if (!LoadStatus.IsEmpty())
	{
		ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*LoadStatus));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Attributes Tab
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::DrawAttributesTab()
{
	FMassEntityHandle Entity;
	FMassEntityManager* Manager = nullptr;

	if (!GetPlayerEntity(Entity, Manager) || Manager == nullptr)
	{
		ImGui::TextDisabled("No player Mass entity.");
		return;
	}

	// Build candidate attribute fragment types once (set is fixed at startup).
	// Declare NON-const as required by the plan (TObjectPtr assignment needs non-const).
	static TArray<UScriptStruct*> AttributeFragmentTypes;
	if (AttributeFragmentTypes.Num() == 0)
	{
		const UScriptStruct* AttributesBaseStruct = FArcMassAttributesBase::StaticStruct();
		for (TObjectIterator<UScriptStruct> It; It; ++It)
		{
			UScriptStruct* Struct = *It;
			if (Struct->IsChildOf(AttributesBaseStruct) && Struct != AttributesBaseStruct)
			{
				AttributeFragmentTypes.Add(Struct);
			}
		}
	}

	// Build list of fragment types actually present on this entity
	TArray<UScriptStruct*> PresentTypes;
	for (UScriptStruct* Struct : AttributeFragmentTypes)
	{
		const FConstStructView View = Manager->GetSparseElementDataForEntity(Struct, Entity);
		if (View.IsValid())
		{
			PresentTypes.Add(Struct);
		}
	}

	const float LeftPanelWidth = 180.0f;

	// ── Left panel: fragment type list ─────────────────────────────────────
	if (ImGui::BeginChild("AttrFragList", ImVec2(LeftPanelWidth, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX))
	{
		ImGui::Text("Attribute Fragments");
		ImGui::Separator();

		if (PresentTypes.IsEmpty())
		{
			ImGui::TextDisabled("(none on entity)");
		}
		else
		{
			for (int32 i = 0; i < PresentTypes.Num(); ++i)
			{
				const bool bIsSelected = (SelectedAttrFragmentIndex == i);
				FString TypeName = PresentTypes[i]->GetName();
				if (ImGui::Selectable(TCHAR_TO_ANSI(*TypeName), bIsSelected))
				{
					SelectedAttrFragmentIndex = i;
				}
			}
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// ── Right panel: attribute values + edit ───────────────────────────────
	if (ImGui::BeginChild("AttrDetails", ImVec2(0, 0)))
	{
		if (!PresentTypes.IsValidIndex(SelectedAttrFragmentIndex))
		{
			ImGui::TextDisabled("Select a fragment type.");
		}
		else
		{
			UScriptStruct* ElementType = PresentTypes[SelectedAttrFragmentIndex];
			FConstStructView View = Manager->GetSparseElementDataForEntity(ElementType, Entity);

			if (!View.IsValid())
			{
				ImGui::TextDisabled("Fragment no longer valid.");
			}
			else
			{
				FString FragName = ElementType->GetName();
				ImGui::Text("%s", TCHAR_TO_ANSI(*FragName));
				ImGui::Separator();

				const UScriptStruct* ArcAttributeStruct = FArcAttribute::StaticStruct();

				for (TFieldIterator<FStructProperty> PropIt(ElementType); PropIt; ++PropIt)
				{
					FStructProperty* StructProp = *PropIt;
					if (!StructProp->Struct || !StructProp->Struct->IsChildOf(ArcAttributeStruct))
					{
						continue;
					}

					const FArcAttribute* Attr = StructProp->ContainerPtrToValuePtr<FArcAttribute>(View.GetMemory());
					if (!Attr)
					{
						continue;
					}

					FString PropName = StructProp->GetName();
					FString Key = FragName + TEXT(".") + PropName;

					// Seed the edited value only if not yet tracked
					if (!EditedAttributeValues.Contains(Key))
					{
						EditedAttributeValues.Add(Key, Attr->GetBaseValue());
					}

					ImGui::Text("  %s", TCHAR_TO_ANSI(*PropName));
					ImGui::SameLine(160.0f);
					ImGui::TextDisabled("B:%.2f C:%.2f", Attr->GetBaseValue(), Attr->GetCurrentValue());

					ImGui::SameLine(300.0f);
					ImGui::PushItemWidth(90.0f);
					FString InputLabel = FString::Printf(TEXT("##attr_%s"), *Key);
					float EditedValue = EditedAttributeValues[Key];
					if (ImGui::InputFloat(TCHAR_TO_ANSI(*InputLabel), &EditedValue, 0.0f, 0.0f, "%.2f"))
					{
						EditedAttributeValues[Key] = EditedValue;
					}
					ImGui::PopItemWidth();
				}

				ImGui::Separator();

				if (ImGui::Button("Apply Changes"))
				{
					// Probe for FArcPendingAttributeOpsFragment before applying
					FMassEntityView EntityView(*Manager, Entity);
					const FArcPendingAttributeOpsFragment* PendingFrag =
						EntityView.GetSparseFragmentDataPtr<FArcPendingAttributeOpsFragment>();

					if (PendingFrag == nullptr)
					{
						AttributeStatus = TEXT("Entity has no FArcPendingAttributeOpsFragment — cannot apply");
					}
					else
					{
						const UScriptStruct* ArcAttributeStructInner = FArcAttribute::StaticStruct();
						for (TFieldIterator<FStructProperty> PropIt(ElementType); PropIt; ++PropIt)
						{
							FStructProperty* StructProp = *PropIt;
							if (!StructProp->Struct || !StructProp->Struct->IsChildOf(ArcAttributeStructInner))
							{
								continue;
							}

							FString PropName = StructProp->GetName();
							FString Key = FragName + TEXT(".") + PropName;

							const float* EditedValuePtr = EditedAttributeValues.Find(Key);
							if (!EditedValuePtr)
							{
								continue;
							}

							FArcAttributeRef Ref;
							Ref.FragmentType = ElementType;
							Ref.PropertyName = StructProp->GetFName();

							ArcAttributes::SetValue(*Manager, Entity, Ref, *EditedValuePtr);
						}
						AttributeStatus = TEXT("Applied");
					}
				}

				ImGui::SameLine();

				if (ImGui::Button("Reset to Live"))
				{
					// Two-pass removal: collect keys first, then remove
					TArray<FString> KeysToRemove;
					for (const TPair<FString, float>& Pair : EditedAttributeValues)
					{
						FString Prefix = FragName + TEXT(".");
						if (Pair.Key.StartsWith(Prefix))
						{
							KeysToRemove.Add(Pair.Key);
						}
					}
					for (const FString& Key : KeysToRemove)
					{
						EditedAttributeValues.Remove(Key);
					}
				}
			}
		}

		if (!AttributeStatus.IsEmpty())
		{
			ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*AttributeStatus));
		}
	}
	ImGui::EndChild();
}

// ─────────────────────────────────────────────────────────────────────────────
// Save / Load / Delete
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::SaveCharacter(const FString& Name)
{
	UArcPlayerPersistenceSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		SaveStatus = TEXT("ERROR: No persistence subsystem available.");
		return;
	}

	UMassAgentComponent* AgentComp = GetPlayerAgentComponent();
	if (!AgentComp)
	{
		SaveStatus = TEXT("Cannot save: no Mass agent / entity not ready");
		return;
	}

	FMassEntityHandle Entity = AgentComp->GetEntityHandle();
	if (!Entity.IsValid())
	{
		SaveStatus = TEXT("Cannot save: no Mass agent / entity not ready");
		return;
	}

	Sub->SaveObject(CachedPlayerId, MakeDomain(Name), AgentComp);
	SaveStatus = FString::Printf(TEXT("Saved '%s'"), *Name);
	RefreshSavedCharacters();
}

void FArcCharacterPersistenceDebugger::LoadCharacter(const FString& Name)
{
	UArcPlayerPersistenceSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		LoadStatus = TEXT("ERROR: No persistence subsystem available.");
		return;
	}

	// Guard entity validity BEFORE calling (LoadObject silently skips on invalid entity)
	UMassAgentComponent* AgentComp = GetPlayerAgentComponent();
	if (!AgentComp)
	{
		LoadStatus = TEXT("Cannot load: entity not ready");
		return;
	}

	FMassEntityHandle Entity = AgentComp->GetEntityHandle();
	if (!Entity.IsValid())
	{
		LoadStatus = TEXT("Cannot load: entity not ready");
		return;
	}

	Sub->LoadObject(CachedPlayerId, MakeDomain(Name), AgentComp);
	EditedAttributeValues.Reset();
	LoadStatus = FString::Printf(TEXT("Loaded '%s'"), *Name);
}

void FArcCharacterPersistenceDebugger::DeleteCharacter(const FString& Name)
{
	UArcPlayerPersistenceSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		LoadStatus = TEXT("ERROR: No persistence subsystem available.");
		return;
	}

	Sub->DeletePlayerDomain(CachedPlayerId, MakeDomain(Name));
	LoadStatus = FString::Printf(TEXT("Deleted '%s'"), *Name);
	RefreshSavedCharacters();
}

void FArcCharacterPersistenceDebugger::RefreshSavedCharacters()
{
	SavedCharacterNames.Reset();

	UArcPlayerPersistenceSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	TArray<FString> Domains = Sub->ListPlayerDomains(CachedPlayerId);

	static const FString CharactersPrefix = TEXT("characters/");

	for (const FString& Domain : Domains)
	{
		if (!Domain.StartsWith(CharactersPrefix))
		{
			continue;
		}

		FString AfterCharacters = Domain.Mid(CharactersPrefix.Len());
		int32 SlashIdx = INDEX_NONE;
		if (!AfterCharacters.FindChar(TEXT('/'), SlashIdx) || SlashIdx <= 0)
		{
			continue;
		}

		FString CharName = AfterCharacters.Left(SlashIdx);

		// Dedupe
		if (!SavedCharacterNames.Contains(CharName))
		{
			SavedCharacterNames.Add(CharName);
		}
	}

	SavedCharacterNames.Sort();
}

// ─────────────────────────────────────────────────────────────────────────────
// Resolution helpers
// ─────────────────────────────────────────────────────────────────────────────

UWorld* FArcCharacterPersistenceDebugger::GetDebugWorld() const
{
	return GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
}

UMassAgentComponent* FArcCharacterPersistenceDebugger::GetPlayerAgentComponent() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}

	APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Pawn)
	{
		return nullptr;
	}

	return Pawn->FindComponentByClass<UMassAgentComponent>();
}

bool FArcCharacterPersistenceDebugger::GetPlayerEntity(FMassEntityHandle& OutEntity, FMassEntityManager*& OutManager) const
{
	UMassAgentComponent* AgentComp = GetPlayerAgentComponent();
	if (!AgentComp)
	{
		return false;
	}

	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return false;
	}

	UMassEntitySubsystem* EntitySubsystem = World->GetSubsystem<UMassEntitySubsystem>();
	if (!EntitySubsystem)
	{
		return false;
	}

	FMassEntityHandle Entity = AgentComp->GetEntityHandle();
	if (!Entity.IsValid())
	{
		return false;
	}

	OutEntity = Entity;
	OutManager = &EntitySubsystem->GetMutableEntityManager();
	return true;
}

UArcPlayerPersistenceSubsystem* FArcCharacterPersistenceDebugger::GetSubsystem() const
{
	UWorld* World = GetDebugWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<UArcPlayerPersistenceSubsystem>();
}

FGuid FArcCharacterPersistenceDebugger::GetPlayerId() const
{
	// In editor, derive a stable player GUID from the machine name.
	const FString MachineName = FPlatformProcess::ComputerName();
	return FGuid(
		FCrc::StrCrc32(*MachineName),
		FCrc::StrCrc32(*MachineName) ^ 0x44454247, // "DEBG"
		FCrc::StrCrc32(*FString::Printf(TEXT("%s_debugger"), *MachineName)),
		0x44424752 // "DBGR"
	);
}

FString FArcCharacterPersistenceDebugger::MakeDomain(const FString& CharacterName) const
{
	return FString::Printf(TEXT("characters/%s/MassFragments"), *CharacterName);
}
