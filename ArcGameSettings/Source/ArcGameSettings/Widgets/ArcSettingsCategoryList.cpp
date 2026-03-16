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

#include "Widgets/ArcSettingsCategoryList.h"

#include "Widgets/ArcSettingsCategoryAsset.h"
#include "Model/ArcSettingsModel.h"
#include "Model/ArcSettingsTypes.h"

#include "MVVMBlueprintLibrary.h"
#include "MVVMViewModelBase.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcSettingsCategoryList)

void UArcSettingsCategoryList::NativeConstruct()
{
    Super::NativeConstruct();
    BuildList();
}

void UArcSettingsCategoryList::BuildList()
{
    if (!VerticalBox_Settings)
    {
        return;
    }

    VerticalBox_Settings->ClearChildren();

    if (!SettingsAsset)
    {
        return;
    }

    UGameInstance* GI = GetGameInstance();
    if (!GI)
    {
        return;
    }

    UArcSettingsModel* Model = GI->GetSubsystem<UArcSettingsModel>();
    if (!Model)
    {
        return;
    }

    const TArray<FGameplayTag> SettingTags = Model->GetSettingsForCategory(SettingsAsset->CategoryTag);

    for (const FGameplayTag& Tag : SettingTags)
    {
        if (Model->GetVisibility(Tag) == EArcSettingVisibility::Hidden)
        {
            continue;
        }

        UMVVMViewModelBase* VM = Cast<UMVVMViewModelBase>(Model->GetViewModel(Tag));
        if (!VM)
        {
            continue;
        }

        const TSubclassOf<UUserWidget>* WidgetClassPtr = SettingsAsset->WidgetMapping.Find(VM->GetClass());
        if (!WidgetClassPtr || !*WidgetClassPtr)
        {
            UE_LOG(LogTemp, Warning, TEXT("ArcSettingsCategoryList: No widget mapping for ViewModel class '%s' (setting '%s')"),
                *VM->GetClass()->GetName(), *Tag.ToString());
            continue;
        }

        UUserWidget* EntryWidget = CreateWidget<UUserWidget>(this, *WidgetClassPtr);
        if (!EntryWidget)
        {
            continue;
        }

        UMVVMBlueprintLibrary::SetViewModelByClass(EntryWidget, VM);

        VerticalBox_Settings->AddChildToVerticalBox(EntryWidget);
    }
}
