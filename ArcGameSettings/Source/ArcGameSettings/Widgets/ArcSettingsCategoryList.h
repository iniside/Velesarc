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

#include "ArcSettingsCategoryList.generated.h"

class UArcSettingsCategoryAsset;
class UVerticalBox;

UCLASS(BlueprintType, Blueprintable)
class ARCGAMESETTINGS_API UArcSettingsCategoryList : public UUserWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TObjectPtr<UArcSettingsCategoryAsset> SettingsAsset;

    UFUNCTION(BlueprintCallable)
    void BuildList();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VerticalBox_Settings;
};
