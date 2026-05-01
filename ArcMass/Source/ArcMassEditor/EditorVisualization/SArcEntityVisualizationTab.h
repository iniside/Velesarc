// SArcEntityVisualizationTab.h
// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UArcEditorEntityDrawSubsystem;

class SArcEntityVisualizationTab : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SArcEntityVisualizationTab) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, UArcEditorEntityDrawSubsystem* InSubsystem);

private:
    TWeakObjectPtr<UArcEditorEntityDrawSubsystem> WeakSubsystem;
};
