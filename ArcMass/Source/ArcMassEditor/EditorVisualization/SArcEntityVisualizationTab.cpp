// SArcEntityVisualizationTab.cpp
// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/SArcEntityVisualizationTab.h"
#include "EditorVisualization/ArcEditorEntityDrawSubsystem.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

void SArcEntityVisualizationTab::Construct(const FArguments& InArgs, UArcEditorEntityDrawSubsystem* InSubsystem)
{
    WeakSubsystem = InSubsystem;

    TArray<FName> Categories = InSubsystem ? InSubsystem->GetAllCategories() : TArray<FName>();

    TSharedPtr<SVerticalBox> VBox = SNew(SVerticalBox);

    for (const FName& Category : Categories)
    {
        TWeakObjectPtr<UArcEditorEntityDrawSubsystem> WeakSub = InSubsystem;

        VBox->AddSlot()
        .AutoHeight()
        .Padding(4.f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SCheckBox)
                .IsChecked_Lambda([WeakSub, Category]()
                {
                    UArcEditorEntityDrawSubsystem* Sub = WeakSub.Get();
                    if (!Sub) return ECheckBoxState::Unchecked;
                    return Sub->IsCategoryEnabled(Category) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
                })
                .OnCheckStateChanged_Lambda([WeakSub, Category](ECheckBoxState NewState)
                {
                    UArcEditorEntityDrawSubsystem* Sub = WeakSub.Get();
                    if (!Sub) return;
                    Sub->SetCategoryEnabled(Category, NewState == ECheckBoxState::Checked);
                })
            ]
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            .Padding(4.f, 0, 0, 0)
            [
                SNew(STextBlock)
                .Text(FText::FromName(Category))
            ]
        ];
    }

    ChildSlot
    [
        SNew(SScrollBox)
        + SScrollBox::Slot()
        [
            VBox.ToSharedRef()
        ]
    ];
}
