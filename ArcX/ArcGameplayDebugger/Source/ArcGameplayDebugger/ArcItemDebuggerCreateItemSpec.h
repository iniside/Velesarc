#pragma once
#include "Items/ArcItemSpec.h"

class FArcItemDebuggerCreateItemSpec
{
	
public:
	void Initialize();
	void Uninitialize();
	
	void Draw();

	FString ItemMakerFilterText;
	FArcItemSpec TempNewSpec;

	TArray<FAssetData> ItemDefinitions;
	TArray<FString> ItemList;

	TArray<FString> FragmentInstanceNames;
	TArray<UScriptStruct*> FragmentInstanceTypes;

	TArray<FString> Attributes;
	TArray<FGameplayAttribute> GameplayAttributes;

	TArray<FAssetData> GameplayAbilitiesAssets;
	TArray<FString> GameplayAbilitiesNames;
};

class FArcItemDebuggerItemSpecCreator
{
public:
	void CreateItemSpec(UArcItemsStoreComponent* ItemStore, UArcItemDefinition* ItemDef);

	void Initialize();

	void Uninitialize();

	FArcItemSpec TempNewSpec;
	
	TArray<FString> FragmentInstanceNames;
	TArray<UScriptStruct*> FragmentInstanceTypes;

	TArray<FString> Attributes;
	TArray<FGameplayAttribute> GameplayAttributes;

	TArray<FAssetData> GameplayAbilitiesAssets;
	TArray<FAssetData> GameplayEffectsAssets;
	TArray<FString> GameplayAbilitiesNames;
	
};