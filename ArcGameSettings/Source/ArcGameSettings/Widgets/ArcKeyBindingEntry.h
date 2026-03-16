/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#pragma once

#include "Blueprint/UserWidget.h"
#include "ArcKeyBindingEntry.generated.h"

class UArcSettingsModel;

/**
 * C++ base for keyboard/gamepad key binding entries.
 * Handles key capture, duplicate detection, and two-slot binding (primary/secondary).
 * Blueprint subclass provides the visual layout.
 */
UCLASS(Abstract, Blueprintable)
class ARCGAMESETTINGS_API UArcKeyBindingEntry : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Set the action this entry binds. Call before widget is displayed. */
	UFUNCTION(BlueprintCallable)
	void SetBindingAction(FName InActionName);

	FName GetBindingAction() const { return ActionName; }

protected:
	/** Called when user confirms a new key binding. Override in Blueprint. */
	UFUNCTION(BlueprintImplementableEvent)
	void OnBindingChanged(FKey InNewKey, bool bIsPrimarySlot);

	/** Called to start listening for a key press. */
	UFUNCTION(BlueprintCallable)
	void StartKeyCapture(bool bPrimarySlot);

	/** Called to cancel key capture without applying. */
	UFUNCTION(BlueprintCallable)
	void CancelKeyCapture();

	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

private:
	FName ActionName;
	bool bIsCapturing = false;
	bool bCapturingPrimarySlot = true;
};
