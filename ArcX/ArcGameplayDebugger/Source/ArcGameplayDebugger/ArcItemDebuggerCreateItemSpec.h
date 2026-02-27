#pragma once
#include "Items/ArcItemSpec.h"

class UArcItemsStoreComponent;
class UArcItemDefinition;

/**
 * Reusable widget for editing an FArcItemSpec before adding it to a store.
 * Call Begin() to start editing for a given definition, Draw() each frame,
 * and check IsFinished()/WasCancelled() for the result.
 */
class FArcItemDebuggerItemSpecCreator
{
public:
	void Initialize();
	void Uninitialize();

	/** Start editing a new spec for the given definition. Resets internal state. */
	void Begin(UArcItemDefinition* ItemDef);

	/** Draw the editor UI. Returns true while still editing. */
	void Draw();

	/** True after the user clicked Confirm. */
	bool IsFinished() const { return bFinished; }

	/** True after the user clicked Cancel. */
	bool WasCancelled() const { return bCancelled; }

	/** Get the resulting spec. Only valid after IsFinished() returns true. */
	FArcItemSpec& GetResultSpec() { return TempNewSpec; }

	/** Reset finished/cancelled state for next use. */
	void Reset();

private:
	FArcItemSpec TempNewSpec;
	TWeakObjectPtr<UArcItemDefinition> CurrentDefinition;
	bool bFinished = false;
	bool bCancelled = false;

	// Cached data for combo selectors
	TArray<FString> FragmentInstanceNames;
	TArray<UScriptStruct*> FragmentInstanceTypes;

	TArray<FString> Attributes;
	TArray<FGameplayAttribute> GameplayAttributes;

	TArray<FAssetData> GameplayAbilitiesAssets;
	TArray<FAssetData> GameplayEffectsAssets;
	TArray<FString> GameplayAbilitiesNames;

	// Internal draw helpers
	void DrawSpecProperties();
	void DrawFragmentInstances();
};
