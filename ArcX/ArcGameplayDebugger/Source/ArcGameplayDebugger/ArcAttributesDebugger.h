// Copyright Lukasz Baran. All Rights Reserved.

#pragma once
#include "UObject/WeakObjectPtrTemplates.h"

class UAbilitySystemComponent;

class FArcAttributesDebugger
{
public:
	void Initialize();
	void Uninitialize();
	void Draw();

	bool bShow = false;

private:
	void DrawASCSelector();
	void DrawAttributeDetails(UAbilitySystemComponent* InASC);

	// Currently selected ASC (weak pointer to avoid preventing GC)
	TWeakObjectPtr<UAbilitySystemComponent> SelectedASC;
};
