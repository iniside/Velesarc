// ArcViewportDebugVizMenu.cpp
// Copyright Lukasz Baran. All Rights Reserved.

#include "EditorVisualization/ArcViewportDebugVizMenu.h"

#include "ToolMenus.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "ToolMenuDelegates.h"
#include "Editor.h"
#include "HAL/IConsoleManager.h"
#include "Styling/AppStyle.h"
#include "UObject/NameTypes.h"
#include "Widgets/Input/SCheckBox.h"
#include "Framework/Commands/UICommandInfo.h"
#include "EditorVisualization/ArcEditorEntityDrawContributor.h"
#include "EditorVisualization/ArcEditorEntityDrawSubsystem.h"

#define LOCTEXT_NAMESPACE "ArcViewportDebugViz"

namespace ArcViewportDebugVizMenu_Internal
{
    const FName MenuOwner = "ArcMassViewportDebugViz";

    void AddCvarToggle(FToolMenuSection& Section, FName EntryName, const FText& Label, const FText& Tip, const TCHAR* CvarName)
    {
        FToolUIAction Action;
        Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([CvarName](const FToolMenuContext&)
        {
            IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CvarName);
            if (CVar)
            {
                CVar->Set(!CVar->GetBool(), ECVF_SetByConsole);
            }
        });
        Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([CvarName](const FToolMenuContext&) -> ECheckBoxState
        {
            IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CvarName);
            return (CVar && CVar->GetBool()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        });
        Action.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateLambda([CvarName](const FToolMenuContext&) -> bool
        {
            return IConsoleManager::Get().FindConsoleVariable(CvarName) != nullptr;
        });

        Section.AddMenuEntry(EntryName, Label, Tip, FSlateIcon(), Action, EUserInterfaceActionType::ToggleButton);
    }

    UArcEditorEntityDrawSubsystem* GetDrawSubsystem()
    {
        return GEditor ? GEditor->GetEditorSubsystem<UArcEditorEntityDrawSubsystem>() : nullptr;
    }

    FName MakeCategoryEntryName(FName Category)
    {
        return FName(*(FString(TEXT("ArcDrawCategory_")) + Category.ToString()));
    }

    FToolMenuEntry MakeDetailCategoryEntry(FName Category)
    {
        FToolUIAction Action;
        Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([Category](const FToolMenuContext&)
        {
            UArcEditorEntityDrawSubsystem* Subsystem = GetDrawSubsystem();
            if (Subsystem)
            {
                Subsystem->SetCategoryEnabled(Category, !Subsystem->IsCategoryEnabled(Category));
            }
        });
        Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([Category](const FToolMenuContext&) -> ECheckBoxState
        {
            UArcEditorEntityDrawSubsystem* Subsystem = GetDrawSubsystem();
            return (Subsystem && Subsystem->IsCategoryEnabled(Category)) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
        });

        return FToolMenuEntry::InitMenuEntry(
            MakeCategoryEntryName(Category),
            FText::FromName(Category),
            LOCTEXT("DrawCategoryDetailTip", "Toggle this editor overlay category."),
            FSlateIcon(),
            Action,
            EUserInterfaceActionType::ToggleButton);
    }

    FToolMenuEntry MakeRepresentationCategoryEntry(FName Category)
    {
        FToolUIAction Action;
        Action.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&) {});
        Action.CanExecuteAction = FToolMenuCanExecuteAction::CreateLambda([](const FToolMenuContext&) -> bool
        {
            return false;
        });
        Action.GetActionCheckState = FToolMenuGetActionCheckState::CreateLambda([](const FToolMenuContext&) -> ECheckBoxState
        {
            return ECheckBoxState::Checked;
        });

        return FToolMenuEntry::InitMenuEntry(
            MakeCategoryEntryName(Category),
            FText::FromName(Category),
            LOCTEXT("DrawCategoryRepresentationTip", "This is the only representation these entities have in the viewport, so it cannot be turned off."),
            FSlateIcon(),
            Action,
            EUserInterfaceActionType::ToggleButton);
    }

    void AddDrawCategorySection(UToolMenu* InMenu)
    {
        UArcEditorEntityDrawSubsystem* Subsystem = GetDrawSubsystem();
        if (!Subsystem)
        {
            return;
        }

        TArray<FName> Categories = Subsystem->GetAllCategories();
        Categories.Sort(FNameLexicalLess());

        TArray<FToolMenuEntry> Entries;
        Entries.Reserve(Categories.Num());

        for (const FName& Category : Categories)
        {
            if (Subsystem->GetCategoryKind(Category) == EArcDrawCategoryKind::Representation)
            {
                Entries.Add(MakeRepresentationCategoryEntry(Category));
            }
            else
            {
                Entries.Add(MakeDetailCategoryEntry(Category));
            }
        }

        if (Entries.Num() == 0)
        {
            return;
        }

        FToolMenuSection& Section = InMenu->FindOrAddSection("ArcDrawCategories", LOCTEXT("ArcDrawCategoriesHeading", "Editor Overlay"));
        for (const FToolMenuEntry& Entry : Entries)
        {
            Section.AddEntry(Entry);
        }
    }

    static void PopulateDebugVizSubmenu(UToolMenu* InMenu)
    {
        FToolMenuSection& S = InMenu->FindOrAddSection("ArcDebugDraw", LOCTEXT("ArcRuntimeHeading", "Runtime (PIE)"));

        AddCvarToggle(S, "ShowVisGrid",
            LOCTEXT("ShowVisGridLabel", "Visualization Grid"),
            LOCTEXT("ShowVisGridTip", "Toggle Mass visualization grid debug draw"),
            TEXT("arc.mass.DebugDrawVisualizationGrid"));

        AddCvarToggle(S, "ShowSpatialHash",
            LOCTEXT("ShowSpatialHashLabel", "Spatial Hash"),
            LOCTEXT("ShowSpatialHashTip", "Toggle Mass spatial hash debug draw"),
            TEXT("arc.mass.DebugDrawSpatialHash"));

        AddCvarToggle(S, "ShowInfluence",
            LOCTEXT("ShowInfluenceLabel", "Influence Map"),
            LOCTEXT("ShowInfluenceTip", "Toggle Mass influence map debug draw"),
            TEXT("arc.mass.DebugDrawInfluence"));

#if !UE_BUILD_SHIPPING
        AddCvarToggle(S, "ShowSpatialWorldData",
            LOCTEXT("ShowSpatialWorldDataLabel", "Spatial World Data"),
            LOCTEXT("ShowSpatialWorldDataTip", "Toggle Mass spatial world data debug draw"),
            TEXT("arc.mass.DebugDrawSpatialWorldData"));
#endif

#if WITH_GAMEPLAY_DEBUGGER
        AddCvarToggle(S, "ShowDecayingTags",
            LOCTEXT("ShowDecayingTagsLabel", "Decaying Tags"),
            LOCTEXT("ShowDecayingTagsTip", "Toggle Mass decaying tags debug draw"),
            TEXT("arc.mass.DebugDrawDecayingTags"));
#endif

        AddDrawCategorySection(InMenu);
    }

} // namespace ArcViewportDebugVizMenu_Internal

void ArcMassEditor::ViewportDebugViz::Register()
{
    static bool bRegistered = false;
    if (bRegistered)
    {
        return;
    }
    bRegistered = true;

    FToolMenuOwnerScoped OwnerScoped(ArcViewportDebugVizMenu_Internal::MenuOwner);

    UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.ViewportToolbar");
    if (!Toolbar)
    {
        return;
    }

    FToolMenuSection& Right = Toolbar->FindOrAddSection("Right");

    FToolMenuEntry Entry = FToolMenuEntry::InitDynamicEntry(
        "ArcMassDebugViz",
        FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InDynamicSection)
        {
            FToolMenuEntry& Sub = InDynamicSection.AddSubMenu(
                "ArcMassDebugVizMenu",
                LOCTEXT("ArcMassDebugVizLabel", "Mass Debug"),
                LOCTEXT("ArcMassDebugVizTip", "Toggle Mass debug visualization in the viewport"),
                FNewToolMenuDelegate::CreateStatic(&ArcViewportDebugVizMenu_Internal::PopulateDebugVizSubmenu),
                false,
                FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility")
            );
            Sub.ToolBarData.ResizeParams.ClippingPriority = 800;
        })
    );
    Entry.InsertPosition = FToolMenuInsert("DynamicCameraOptions", EToolMenuInsertType::Before);
    Right.AddEntry(Entry);
}

void ArcMassEditor::ViewportDebugViz::Unregister()
{
    if (UObjectInitialized() && UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::Get()->UnregisterOwnerByName(ArcViewportDebugVizMenu_Internal::MenuOwner);
    }
}

#undef LOCTEXT_NAMESPACE
