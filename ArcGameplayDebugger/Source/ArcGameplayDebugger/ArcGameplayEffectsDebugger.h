// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

struct FAssetData;
class UAbilitySystemComponent;

class FArcGameplayEffectsDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawASCSelector();
	void DrawEffectsDetails(UAbilitySystemComponent* InASC);
	void DrawApplyEffect(UAbilitySystemComponent* InASC);
	void RefreshEffectAssetList();

	// Currently selected ASC (weak pointer to avoid preventing GC)
	TWeakObjectPtr<UAbilitySystemComponent> SelectedASC;

	// Apply-effect state
	TArray<FAssetData> CachedEffectAssets;
	int32 SelectedEffectIndex = -1;
	char EffectApplyFilter[256] = {};
};
