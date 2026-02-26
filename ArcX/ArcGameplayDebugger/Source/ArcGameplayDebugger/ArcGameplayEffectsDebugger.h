// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

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

	// Currently selected ASC (weak pointer to avoid preventing GC)
	TWeakObjectPtr<UAbilitySystemComponent> SelectedASC;
};
