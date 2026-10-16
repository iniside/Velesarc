#include "SArcTimeOfDayPanel.h"

#include "ArcTimeOfDayActor.h"
#include "Components/ActorComponent.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "ObjectEditorUtils.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "Styling/AppStyle.h"
#include "UObject/FieldIterator.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SArcTimeOfDayPanel"

void SArcTimeOfDayPanel::Construct(const FArguments& InArgs)
{
	(void)InArgs;

	FPropertyEditorModule& PropertyEditorModule =
		FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = true;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetIsPropertyVisibleDelegate(
		FIsPropertyVisible::CreateSP(this, &SArcTimeOfDayPanel::IsPropertyVisible));
	DetailsView->SetIsCustomRowVisibleDelegate(
		FIsCustomRowVisible::CreateSP(this, &SArcTimeOfDayPanel::IsCustomRowVisible));

	ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		+ SSplitter::Slot()
		.Value(0.25f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f, 6.0f, 6.0f, 3.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CategoriesHeading", "Categories"))
				.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(3.0f)
			[
				SAssignNew(CategoryListView, SListView<TSharedPtr<FName>>)
				.ListItemsSource(&CategoryItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SArcTimeOfDayPanel::GenerateCategoryRow)
				.OnSelectionChanged(this, &SArcTimeOfDayPanel::HandleCategorySelectionChanged)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f, 6.0f, 6.0f, 3.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ComponentsHeading", "Components"))
				.Font(FAppStyle::GetFontStyle(TEXT("DetailsView.CategoryFontStyle")))
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(3.0f)
			[
				SAssignNew(ComponentListView, SListView<TSharedPtr<FArcTimeOfDayComponentItem>>)
				.ListItemsSource(&ComponentItems)
				.SelectionMode(ESelectionMode::Single)
				.OnGenerateRow(this, &SArcTimeOfDayPanel::GenerateComponentRow)
				.OnSelectionChanged(this, &SArcTimeOfDayPanel::HandleComponentSelectionChanged)
			]
		]

		+ SSplitter::Slot()
		.Value(0.75f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				DetailsView.ToSharedRef()
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SBorder)
				.Padding(18.0f)
				.Visibility(this, &SArcTimeOfDayPanel::GetEmptyStateVisibility)
				[
					SNew(STextBlock)
					.Text(LOCTEXT(
						"NoTargetInstruction",
						"Select an ArcTimeOfDay actor or one of its components."))
					.AutoWrapText(true)
				]
			]
		]
	];

	SelectionChangedHandle = USelection::SelectionChangedEvent.AddSP(
		this, &SArcTimeOfDayPanel::HandleEditorSelectionChanged);
	RefreshFromEditorSelection();
}

SArcTimeOfDayPanel::~SArcTimeOfDayPanel()
{
	if (SelectionChangedHandle.IsValid())
	{
		USelection::SelectionChangedEvent.Remove(SelectionChangedHandle);
		SelectionChangedHandle.Reset();
	}
}

void SArcTimeOfDayPanel::HandleEditorSelectionChanged(UObject* NewSelection)
{
	(void)NewSelection;
	RefreshFromEditorSelection();
}

void SArcTimeOfDayPanel::RefreshFromEditorSelection()
{
	TargetActor = ResolveSelectedActor();
	SelectedCategory = NAME_None;
	SelectedComponent.Reset();
	PanelSelectionMode = EPanelSelectionMode::None;
	DetailsView->SetObject(nullptr, true);

	bUpdatingListSelection = true;
	CategoryListView->ClearSelection();
	ComponentListView->ClearSelection();
	bUpdatingListSelection = false;

	RebuildCategoryItems();
	RebuildComponentItems();
	CategoryListView->RequestListRefresh();
	ComponentListView->RequestListRefresh();

	if (CategoryItems.Num() > 0)
	{
		CategoryListView->SetSelection(CategoryItems[0], ESelectInfo::Direct);
	}
	else if (ComponentItems.Num() > 0)
	{
		ComponentListView->SetSelection(ComponentItems[0], ESelectInfo::Direct);
	}
}

AArcTimeOfDayActor* SArcTimeOfDayPanel::ResolveSelectedActor() const
{
	if (GEditor == nullptr)
	{
		return nullptr;
	}

	USelection* SelectedComponents = GEditor->GetSelectedComponents();
	if (SelectedComponents != nullptr)
	{
		for (FSelectionIterator Iterator(*SelectedComponents); Iterator; ++Iterator)
		{
			UActorComponent* Component = Cast<UActorComponent>(*Iterator);
			if (Component != nullptr)
			{
				AArcTimeOfDayActor* ComponentOwner = Cast<AArcTimeOfDayActor>(Component->GetOwner());
				if (ComponentOwner != nullptr)
				{
					return ComponentOwner;
				}
			}
		}
	}

	USelection* SelectedActors = GEditor->GetSelectedActors();
	return SelectedActors != nullptr ? SelectedActors->GetTop<AArcTimeOfDayActor>() : nullptr;
}

void SArcTimeOfDayPanel::RebuildCategoryItems()
{
	CategoryItems.Reset();
	if (!TargetActor.IsValid())
	{
		return;
	}

	const UClass* ArcTimeOfDayClass = AArcTimeOfDayActor::StaticClass();
	const TSet<FName> InheritedDaySequenceCategories = {
		FName(TEXT("Sequence")),
		FName(TEXT("RuntimeDayCycle")),
		FName(TEXT("Preview"))
	};
	const TSet<FName> ExcludedCategories = {
		FName(TEXT("Components")),
		FName(TEXT("Actor")),
		FName(TEXT("Replication"))
	};
	TSet<FName> AddedCategories;

	for (TFieldIterator<FProperty> PropertyIterator(
		AArcTimeOfDayActor::StaticClass(), EFieldIteratorFlags::IncludeSuper);
		PropertyIterator;
		++PropertyIterator)
	{
		FProperty* Property = *PropertyIterator;
		if (!Property->HasAnyPropertyFlags(CPF_Edit)
			|| Property->HasAnyPropertyFlags(CPF_EditConst))
		{
			continue;
		}

		const FName CategoryName = FObjectEditorUtils::GetCategoryFName(Property);
		const UClass* PropertyOwnerClass = Property->GetOwnerClass();
		const bool bDeclaredByArcTimeOfDay = PropertyOwnerClass == ArcTimeOfDayClass;
		const bool bAllowedInheritedCategory = PropertyOwnerClass != nullptr
			&& PropertyOwnerClass != ArcTimeOfDayClass
			&& PropertyOwnerClass->IsChildOf(ADaySequenceActor::StaticClass())
			&& InheritedDaySequenceCategories.Contains(CategoryName);
		if ((!bDeclaredByArcTimeOfDay && !bAllowedInheritedCategory)
			|| CategoryName.IsNone()
			|| ExcludedCategories.Contains(CategoryName)
			|| AddedCategories.Contains(CategoryName))
		{
			continue;
		}

		AddedCategories.Add(CategoryName);
		CategoryItems.Add(MakeShared<FName>(CategoryName));
	}
}

void SArcTimeOfDayPanel::RebuildComponentItems()
{
	ComponentItems.Reset();
	AArcTimeOfDayActor* Actor = TargetActor.Get();
	if (Actor == nullptr)
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> OwnedComponents(Actor);
	for (UActorComponent* Component : OwnedComponents)
	{
		if (!IsValid(Component) || Component->GetOwner() != Actor)
		{
			continue;
		}

		TSharedPtr<FArcTimeOfDayComponentItem> Item = MakeShared<FArcTimeOfDayComponentItem>();
		Item->Component = Component;
		Item->SortKey = Component->GetName();
		Item->Label = FText::Format(
			LOCTEXT("ComponentItemFormat", "{0} ({1})"),
			FText::FromString(Component->GetName()),
			Component->GetClass()->GetDisplayNameText());
		ComponentItems.Add(Item);
	}

	ComponentItems.Sort([](
		const TSharedPtr<FArcTimeOfDayComponentItem>& Left,
		const TSharedPtr<FArcTimeOfDayComponentItem>& Right) -> bool
	{
		return Left->SortKey < Right->SortKey;
	});
}

bool SArcTimeOfDayPanel::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent) const
{
	if (PanelSelectionMode == EPanelSelectionMode::Component)
	{
		return true;
	}

	if (PanelSelectionMode != EPanelSelectionMode::Category || SelectedCategory.IsNone())
	{
		return false;
	}

	const FProperty* RootProperty = PropertyAndParent.ParentProperties.Num() > 0
		? PropertyAndParent.ParentProperties.Last()
		: &PropertyAndParent.Property;
	return FObjectEditorUtils::GetCategoryFName(RootProperty) == SelectedCategory;
}

bool SArcTimeOfDayPanel::IsCustomRowVisible(FName InRowName, FName InParentName) const
{
	(void)InRowName;
	if (PanelSelectionMode == EPanelSelectionMode::Component)
	{
		return true;
	}

	return PanelSelectionMode == EPanelSelectionMode::Category
		&& InParentName == SelectedCategory;
}

TSharedRef<ITableRow> SArcTimeOfDayPanel::GenerateCategoryRow(
	TSharedPtr<FName> Item,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	FString DisplayName = Item.IsValid() ? Item->ToString() : FString();
	DisplayName.ReplaceInline(TEXT("|"), TEXT(" / "));
	return SNew(STableRow<TSharedPtr<FName>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(DisplayName))
	];
}

TSharedRef<ITableRow> SArcTimeOfDayPanel::GenerateComponentRow(
	TSharedPtr<FArcTimeOfDayComponentItem> Item,
	const TSharedRef<STableViewBase>& OwnerTable) const
{
	return SNew(STableRow<TSharedPtr<FArcTimeOfDayComponentItem>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(Item.IsValid() ? Item->Label : FText::GetEmpty())
	];
}

void SArcTimeOfDayPanel::HandleCategorySelectionChanged(
	TSharedPtr<FName> Item,
	ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (bUpdatingListSelection || !Item.IsValid() || !TargetActor.IsValid())
	{
		return;
	}

	bUpdatingListSelection = true;
	ComponentListView->ClearSelection();
	bUpdatingListSelection = false;
	SelectedCategory = *Item;
	SelectedComponent.Reset();
	PanelSelectionMode = EPanelSelectionMode::Category;
	DetailsView->SetObject(TargetActor.Get(), true);
}

void SArcTimeOfDayPanel::HandleComponentSelectionChanged(
	TSharedPtr<FArcTimeOfDayComponentItem> Item,
	ESelectInfo::Type SelectInfo)
{
	(void)SelectInfo;
	if (bUpdatingListSelection || !Item.IsValid() || !Item->Component.IsValid())
	{
		return;
	}

	bUpdatingListSelection = true;
	CategoryListView->ClearSelection();
	bUpdatingListSelection = false;
	SelectedCategory = NAME_None;
	SelectedComponent = Item->Component;
	PanelSelectionMode = EPanelSelectionMode::Component;
	DetailsView->SetObject(SelectedComponent.Get(), true);
}

EVisibility SArcTimeOfDayPanel::GetEmptyStateVisibility() const
{
	return TargetActor.IsValid() ? EVisibility::Collapsed : EVisibility::Visible;
}

#undef LOCTEXT_NAMESPACE
