// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

class UAbilitySystemComponent;

class FArcGameplayAbilitiesDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawASCSelector();
	void DrawAbilitySystemDetails(UAbilitySystemComponent* InASC);

	// Currently selected ASC (weak pointer to avoid preventing GC)
	TWeakObjectPtr<UAbilitySystemComponent> SelectedASC;
};
