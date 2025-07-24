#pragma once
#include "Items/ArcItemSpec.h"

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