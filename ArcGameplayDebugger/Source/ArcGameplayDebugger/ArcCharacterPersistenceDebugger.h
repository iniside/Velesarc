// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"

class UMassAgentComponent;
struct FMassEntityHandle;
struct FMassEntityManager;
class UArcPlayerPersistenceSubsystem;

/**
 * ImGui debug window for character persistence over the Mass entity save/load pipeline.
 *
 * Replaces the old GAS/UArcItemsStoreComponent-based implementation (deleted in 7fe2902b)
 * with the current Mass architecture: entity fragments are saved/loaded via
 * UArcPlayerPersistenceSubsystem using UMassAgentComponent as the serialization root.
 * The Attributes tab lets you inspect and directly edit FArcMassAttributesBase-derived
 * sparse fragments on the player entity.
 */
class FArcCharacterPersistenceDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	// ── Tab drawers ────────────────────────────────────────────────────────
	void DrawSaveTab();
	void DrawLoadTab();
	void DrawAttributesTab();

	// ── Resolution helpers ─────────────────────────────────────────────────
	UWorld* GetDebugWorld() const;
	UMassAgentComponent* GetPlayerAgentComponent() const;
	bool GetPlayerEntity(FMassEntityHandle& OutEntity, FMassEntityManager*& OutManager) const;
	UArcPlayerPersistenceSubsystem* GetSubsystem() const;
	FGuid GetPlayerId() const;
	FString MakeDomain(const FString& CharacterName) const;

	// ── Actions ────────────────────────────────────────────────────────────
	void SaveCharacter(const FString& Name);
	void LoadCharacter(const FString& Name);
	void DeleteCharacter(const FString& Name);
	void RefreshSavedCharacters();

	// ── State ──────────────────────────────────────────────────────────────
	FGuid CachedPlayerId;
	char CharacterNameBuf[256] = {};
	TArray<FString> SavedCharacterNames;
	int32 SelectedSaveCharacterIndex = -1;
	int32 SelectedLoadCharacterIndex = -1;
	FString SaveStatus;
	FString LoadStatus;

	// Attributes tab
	int32 SelectedAttrFragmentIndex = -1;
	TMap<FString, float> EditedAttributeValues;
	FString AttributeStatus;
};
