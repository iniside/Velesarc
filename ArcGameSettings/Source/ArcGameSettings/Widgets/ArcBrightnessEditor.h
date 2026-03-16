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

#include "CommonActivatableWidget.h"
#include "ArcBrightnessEditor.generated.h"

class UArcSettingsModel;

/**
 * Brightness/gamma adjustment with live preview.
 */
UCLASS(Abstract, Blueprintable)
class ARCGAMESETTINGS_API UArcBrightnessEditor : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetModel(UArcSettingsModel* InModel) { Model = InModel; }

	UFUNCTION(BlueprintCallable)
	void SetBrightness(float InValue);

	UFUNCTION(BlueprintCallable)
	void ApplyAndClose();

	UFUNCTION(BlueprintCallable)
	void CancelAndClose();

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void OnBrightnessChanged(float InNewValue);

	UPROPERTY(BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	float BrightnessValue = 2.2f;

private:
	float InitialBrightness = 2.2f;

	UPROPERTY()
	TObjectPtr<UArcSettingsModel> Model;
};
