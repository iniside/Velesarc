// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UArcItemsStoreComponent;
class UArcPlayerPersistenceSubsystem;
class UAbilitySystemComponent;
class UAttributeSet;

enum class ECharacterSaveMode : uint8
{
	SelectExisting,
	NewCharacter
};

class FArcCharacterPersistenceDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawSaveTab();
	void DrawSaveTab_SelectExisting();
	void DrawSaveTab_NewCharacter();
	void DrawStoreCheckboxes(const char* IdPrefix);
	void DrawLoadTab();
	void DrawAttributesTab();

	TArray<UArcItemsStoreComponent*> GetPlayerStores() const;
	UArcPlayerPersistenceSubsystem* GetSubsystem() const;

	void SaveCharacter(const FString& Name);
	void LoadCharacter(const FString& Name);
	void DeleteCharacter(const FString& Name);
	void RefreshSavedCharacters();
	void RefreshStores();

	FString GetStoreKeyName(const UArcItemsStoreComponent* Store) const;
	FGuid GetPlayerId() const;
	FString MakeDomain(const FString& CharacterName, const FString& StoreClassName) const;

	UAbilitySystemComponent* GetASC() const;
	TArray<UAttributeSet*> GetAttributeSets() const;
	FString MakeAttributeDomain(const FString& CharacterName, const FString& SetClassName) const;

	// Player ID (derived from machine name in editor)
	FGuid CachedPlayerId;

	// Save tab state
	ECharacterSaveMode SaveMode = ECharacterSaveMode::SelectExisting;
	char CharacterNameBuf[256] = {};
	TArray<TWeakObjectPtr<UArcItemsStoreComponent>> CachedStores;
	TArray<FString> CachedStoreNames;
	TArray<bool> StoreSelection;
	FString SaveStatus;
	int32 SelectedSaveCharacterIndex = -1;

	// Shared character data (used by both tabs)
	TMap<FString, TArray<FString>> SavedCharacterStores; // CharName -> [StoreClassName, ...]
	TArray<FString> SavedCharacterNames; // Sorted keys from map

	// Load tab state
	int32 SelectedCharacterIndex = -1;
	FString LoadStatus;

	// Attributes tab state
	int32 SelectedAttributeSetIndex = -1;
	TMap<FString, float> EditedAttributeValues; // "SetName.AttrName" -> edited base value
	FString AttributeStatus;
};
