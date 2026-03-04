// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCharacterPersistenceDebugger.h"

#include "imgui.h"
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Pawn.h"
#include "Items/ArcItemsStoreComponent.h"
#include "ArcPlayerPersistenceSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcCharacterPersistence, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void FArcCharacterPersistenceDebugger::Initialize()
{
	FMemory::Memzero(CharacterNameBuf, sizeof(CharacterNameBuf));
	SaveStatus.Reset();
	LoadStatus.Reset();
	SelectedCharacterIndex = -1;
	CachedPlayerId = GetPlayerId();

	// Register with subsystem for manual editor/debugger path
	if (UArcPlayerPersistenceSubsystem* Sub = GetSubsystem())
	{
		UWorld* World = GEngine->GetCurrentPlayWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		if (PC)
		{
			Sub->RegisterPlayer(CachedPlayerId, PC);
		}
	}

	RefreshSavedCharacters();
	RefreshStores();
}

void FArcCharacterPersistenceDebugger::Uninitialize()
{
	if (CachedPlayerId.IsValid())
	{
		if (UArcPlayerPersistenceSubsystem* Sub = GetSubsystem())
		{
			Sub->UnregisterPlayer(CachedPlayerId);
		}
	}

	CachedStores.Reset();
	CachedStoreNames.Reset();
	StoreSelection.Reset();
	SavedCharacterStores.Reset();
	SavedCharacterNames.Reset();
	SaveStatus.Reset();
	LoadStatus.Reset();
	CachedPlayerId.Invalidate();
	SelectedCharacterIndex = -1;
	SelectedSaveCharacterIndex = -1;
	SaveMode = ECharacterSaveMode::SelectExisting;
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

	// ── Left panel: Character list ──
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
					SaveMode = ECharacterSaveMode::SelectExisting;
				}
			}
		}

		ImGui::Separator();

		if (ImGui::Button("Refresh"))
		{
			RefreshSavedCharacters();
			RefreshStores();
		}
	}
	ImGui::EndChild();

	ImGui::SameLine();

	// ── Right panel: Details & actions ──
	if (ImGui::BeginChild("SaveDetails", ImVec2(0, 0)))
	{
		if (SaveMode == ECharacterSaveMode::NewCharacter)
		{
			DrawSaveTab_NewCharacter();
		}
		else
		{
			DrawSaveTab_SelectExisting();
		}
	}
	ImGui::EndChild();
}

void FArcCharacterPersistenceDebugger::DrawSaveTab_SelectExisting()
{
	const bool bHasSelection = SavedCharacterNames.IsValidIndex(SelectedSaveCharacterIndex);
	const FString SelectedName = bHasSelection ? SavedCharacterNames[SelectedSaveCharacterIndex] : FString();

	// Show saved stores for selected character
	if (bHasSelection)
	{
		ImGui::Text("Saved stores:");
		if (const TArray<FString>* Stores = SavedCharacterStores.Find(SelectedName))
		{
			for (const FString& StoreName : *Stores)
			{
				ImGui::BulletText("%s", TCHAR_TO_ANSI(*StoreName));
			}
		}
		ImGui::Separator();
	}

	DrawStoreCheckboxes("store");

	ImGui::Separator();

	// Action buttons
	if (!bHasSelection)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Save"))
	{
		SaveCharacter(SelectedName);
	}

	ImGui::SameLine();

	if (ImGui::Button("Delete"))
	{
		DeleteCharacter(SelectedName);
		SaveStatus = LoadStatus; // DeleteCharacter writes to LoadStatus
		RefreshSavedCharacters();
		SelectedSaveCharacterIndex = -1;
	}

	if (!bHasSelection)
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (ImGui::Button("+ New Character"))
	{
		SaveMode = ECharacterSaveMode::NewCharacter;
		FMemory::Memzero(CharacterNameBuf, sizeof(CharacterNameBuf));
	}

	if (!SaveStatus.IsEmpty())
	{
		ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*SaveStatus));
	}
}

void FArcCharacterPersistenceDebugger::DrawSaveTab_NewCharacter()
{
	ImGui::Text("New Character");
	ImGui::Separator();

	ImGui::Text("Name:");
	ImGui::SameLine();
	ImGui::InputText("##NewCharName", CharacterNameBuf, IM_ARRAYSIZE(CharacterNameBuf));

	ImGui::Separator();

	DrawStoreCheckboxes("newstore");

	ImGui::Separator();

	const bool bHasName = FCStringAnsi::Strlen(CharacterNameBuf) > 0;

	if (!bHasName)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Save"))
	{
		FString Name = ANSI_TO_TCHAR(CharacterNameBuf);
		SaveCharacter(Name);
		SaveMode = ECharacterSaveMode::SelectExisting;

		// Select the newly saved character
		SelectedSaveCharacterIndex = SavedCharacterNames.IndexOfByKey(Name);
	}

	if (!bHasName)
	{
		ImGui::EndDisabled();
	}

	ImGui::SameLine();

	if (ImGui::Button("Cancel"))
	{
		SaveMode = ECharacterSaveMode::SelectExisting;
	}

	if (!SaveStatus.IsEmpty())
	{
		ImGui::TextWrapped("%s", TCHAR_TO_ANSI(*SaveStatus));
	}
}

void FArcCharacterPersistenceDebugger::DrawStoreCheckboxes(const char* IdPrefix)
{
	ImGui::Text("Stores to save:");
	for (int32 i = 0; i < CachedStores.Num(); ++i)
	{
		if (!CachedStores[i].IsValid())
		{
			continue;
		}

		bool bSelected = StoreSelection.IsValidIndex(i) ? StoreSelection[i] : false;

		UArcItemsStoreComponent* Store = CachedStores[i].Get();
		int32 ItemCount = Store->GetItems().Num();

		FString LabelStr = FString::Printf(TEXT("%s (%d items)##%s_%d"), *CachedStoreNames[i], ItemCount, ANSI_TO_TCHAR(IdPrefix), i);

		if (ImGui::Checkbox(TCHAR_TO_ANSI(*LabelStr), &bSelected))
		{
			StoreSelection[i] = bSelected;
		}
	}
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
			const bool bIsSelected = (SelectedCharacterIndex == i);
			if (ImGui::Selectable(TCHAR_TO_ANSI(*SavedCharacterNames[i]), bIsSelected))
			{
				SelectedCharacterIndex = i;
			}
		}
	}

	ImGui::Separator();

	const bool bHasSelection = SavedCharacterNames.IsValidIndex(SelectedCharacterIndex);

	if (!bHasSelection)
	{
		ImGui::BeginDisabled();
	}

	if (ImGui::Button("Load Character"))
	{
		LoadCharacter(SavedCharacterNames[SelectedCharacterIndex]);
	}

	ImGui::SameLine();

	if (ImGui::Button("Delete Character"))
	{
		DeleteCharacter(SavedCharacterNames[SelectedCharacterIndex]);
		RefreshSavedCharacters();
		SelectedCharacterIndex = -1;
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

	RefreshStores();

	int32 StoresSaved = 0;

	for (int32 i = 0; i < CachedStores.Num(); ++i)
	{
		if (!StoreSelection.IsValidIndex(i) || !StoreSelection[i])
		{
			continue;
		}

		UArcItemsStoreComponent* Store = CachedStores[i].IsValid() ? CachedStores[i].Get() : nullptr;
		if (!Store)
		{
			continue;
		}

		FString Domain = MakeDomain(Name, CachedStoreNames[i]);
		Sub->SaveObject(CachedPlayerId, Domain, Store);
		++StoresSaved;
	}

	SaveStatus = FString::Printf(TEXT("Saved %d stores for '%s'."), StoresSaved, *Name);

	// Refresh the character list so it shows up in Load tab
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

	RefreshStores();

	// Load attempts all stores (ignoring save-tab selection) — we want to
	// restore everything that was saved for this character.
	int32 StoresAttempted = 0;

	for (int32 i = 0; i < CachedStores.Num(); ++i)
	{
		if (!CachedStores[i].IsValid())
		{
			continue;
		}

		FString Domain = MakeDomain(Name, CachedStoreNames[i]);
		Sub->LoadObject(CachedPlayerId, Domain, CachedStores[i].Get());
		++StoresAttempted;
	}

	LoadStatus = FString::Printf(TEXT("Load applied to %d stores for '%s'."), StoresAttempted, *Name);
}

void FArcCharacterPersistenceDebugger::DeleteCharacter(const FString& Name)
{
	UArcPlayerPersistenceSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		LoadStatus = TEXT("ERROR: No persistence subsystem available.");
		return;
	}

	FString CharPrefix = FString::Printf(TEXT("characters/%s/"), *Name);
	TArray<FString> AllDomains = Sub->ListPlayerDomains(CachedPlayerId);

	int32 DeletedCount = 0;
	for (const FString& Domain : AllDomains)
	{
		if (Domain.StartsWith(CharPrefix))
		{
			Sub->DeletePlayerDomain(CachedPlayerId, Domain);
			++DeletedCount;
		}
	}

	LoadStatus = FString::Printf(TEXT("Deleted character '%s' (%d entries)."), *Name, DeletedCount);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

TArray<UArcItemsStoreComponent*> FArcCharacterPersistenceDebugger::GetPlayerStores() const
{
	TArray<UArcItemsStoreComponent*> Result;

	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
	if (!World)
	{
		return Result;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return Result;
	}

	APawn* Pawn = PC->GetPawn();

	TArray<AActor*> ActorsToSearch;
	if (Pawn)
	{
		ActorsToSearch.Add(Pawn);
	}
	ActorsToSearch.Add(PC);
	if (PC->GetPlayerState<APlayerState>())
	{
		ActorsToSearch.Add(PC->GetPlayerState<APlayerState>());
	}

	for (AActor* Actor : ActorsToSearch)
	{
		if (!Actor)
		{
			continue;
		}
		TArray<UArcItemsStoreComponent*> Components;
		Actor->GetComponents<UArcItemsStoreComponent>(Components);
		for (UArcItemsStoreComponent* Comp : Components)
		{
			Result.AddUnique(Comp);
		}
	}

	return Result;
}

UArcPlayerPersistenceSubsystem* FArcCharacterPersistenceDebugger::GetSubsystem() const
{
	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;
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

void FArcCharacterPersistenceDebugger::RefreshSavedCharacters()
{
	SavedCharacterStores.Reset();
	SavedCharacterNames.Reset();
	SelectedCharacterIndex = -1;
	SelectedSaveCharacterIndex = -1;

	UArcPlayerPersistenceSubsystem* Sub = GetSubsystem();
	if (!Sub)
	{
		return;
	}

	TArray<FString> AllDomains = Sub->ListPlayerDomains(CachedPlayerId);

	static const FString CharactersPrefix = TEXT("characters/");

	for (const FString& Domain : AllDomains)
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

		// Extract store name: last segment after final '/'
		int32 LastSlash = INDEX_NONE;
		if (Domain.FindLastChar(TEXT('/'), LastSlash) && LastSlash < Domain.Len() - 1)
		{
			FString StoreName = Domain.Mid(LastSlash + 1);
			SavedCharacterStores.FindOrAdd(CharName).AddUnique(StoreName);
		}
	}

	SavedCharacterStores.KeySort([](const FString& A, const FString& B) { return A < B; });
	SavedCharacterStores.GenerateKeyArray(SavedCharacterNames);
}

void FArcCharacterPersistenceDebugger::RefreshStores()
{
	TArray<UArcItemsStoreComponent*> Stores = GetPlayerStores();

	CachedStores.Reset();
	CachedStoreNames.Reset();
	StoreSelection.Reset();

	for (UArcItemsStoreComponent* Store : Stores)
	{
		CachedStores.Add(Store);
		CachedStoreNames.Add(GetStoreKeyName(Store));
		StoreSelection.Add(true); // All selected by default
	}
}

FString FArcCharacterPersistenceDebugger::GetStoreKeyName(const UArcItemsStoreComponent* Store) const
{
	if (!Store)
	{
		return TEXT("Unknown");
	}

	// Use the component's class name as the key
	return Store->GetClass()->GetName();
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

FString FArcCharacterPersistenceDebugger::MakeDomain(
	const FString& CharacterName, const FString& StoreClassName) const
{
	return FString::Printf(TEXT("characters/%s/PlayerState/%s"), *CharacterName, *StoreClassName);
}
