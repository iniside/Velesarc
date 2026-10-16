// Copyright Lukasz Baran. All Rights Reserved.

#include "Customization/ArcRmlDocumentRefCustomization.h"

#include "ArcRmlDocumentRef.h"
#include "ArcRmlUIModule.h"
#include "ContentBrowserDataDragDropOp.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserModule.h"
#include "DetailWidgetRow.h"
#include "IContentBrowserDataModule.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/ArcRmlPaths.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "SDropTarget.h"
#include "Widgets/Input/SSearchableComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcRmlDocumentRefCustomization"

namespace ArcRmlDocumentRefCustomizationPrivate
{
	bool IsDocumentFilename(const FString& Filename)
	{
		return FPaths::GetExtension(Filename).Equals(TEXT("rml"), ESearchCase::IgnoreCase);
	}

	bool ToDocumentPackagePath(const FContentBrowserItem& Item, FString& OutPackagePath)
	{
		FString Filename;
		if (!Item.IsFile() || !Item.GetItemPhysicalPath(Filename) || !IsDocumentFilename(Filename))
		{
			return false;
		}
		return ArcRmlPaths::ToPackagePath(FArcRmlUIModule::Get().GetContentRoots(), Filename, OutPackagePath);
	}
}

TSharedRef<IPropertyTypeCustomization> FArcRmlDocumentRefCustomization::MakeInstance()
{
	return MakeShared<FArcRmlDocumentRefCustomization>();
}

void FArcRmlDocumentRefCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PathHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcRmlDocumentRef, Path));
	check(PathHandle.IsValid());

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(250.0f)
	.MaxDesiredWidth(0.0f)
	[
		SNew(SDropTarget)
		.OnAllowDrop(this, &FArcRmlDocumentRefCustomization::IsDropRecognized)
		.OnIsRecognized(this, &FArcRmlDocumentRefCustomization::IsDropRecognized)
		.OnDropped(this, &FArcRmlDocumentRefCustomization::HandleDropped)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(ComboBox, SSearchableComboBox)
				.OptionsSource(&Options)
				.OnComboBoxOpening(this, &FArcRmlDocumentRefCustomization::RefreshOptions)
				.OnGenerateWidget(this, &FArcRmlDocumentRefCustomization::MakeOptionWidget)
				.OnSelectionChanged(this, &FArcRmlDocumentRefCustomization::HandleSelectionChanged)
				.IsEnabled(this, &FArcRmlDocumentRefCustomization::IsEditable)
				[
					SNew(STextBlock)
					.Text(this, &FArcRmlDocumentRefCustomization::GetCurrentText)
					.Font(CustomizationUtils.GetRegularFont())
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(2.0f, 0.0f)
			[
				PropertyCustomizationHelpers::MakeBrowseButton(FSimpleDelegate::CreateSP(this, &FArcRmlDocumentRefCustomization::HandleBrowseClicked),
					LOCTEXT("BrowseToolTip", "Browse to the document in the Content Browser"),
					TAttribute<bool>::CreateSP(this, &FArcRmlDocumentRefCustomization::CanBrowse))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FArcRmlDocumentRefCustomization::HandleClearClicked),
					LOCTEXT("ClearToolTip", "Clear the document"),
					TAttribute<bool>::CreateSP(this, &FArcRmlDocumentRefCustomization::IsEditable))
			]
		]
	];
}

void FArcRmlDocumentRefCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FArcRmlDocumentRefCustomization::RefreshOptions()
{
	Options.Reset();

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	if (ContentBrowserData)
	{
		FContentBrowserDataFilter Filter;
		Filter.bRecursivePaths = true;
		Filter.ItemTypeFilter = EContentBrowserItemTypeFilter::IncludeFiles;
		Filter.ItemCategoryFilter = EContentBrowserItemCategoryFilter::IncludeMisc;

		TArray<FString> PackagePaths;
		for (const FArcRmlContentRoot& Root : FArcRmlUIModule::Get().GetContentRoots().Roots)
		{
			const FName VirtualRoot = ContentBrowserData->ConvertInternalPathToVirtual(FName(TEXT("/") + Root.MountName));
			for (const FContentBrowserItem& Item : ContentBrowserData->GetItemsUnderPath(VirtualRoot, Filter))
			{
				FString PackagePath;
				if (ArcRmlDocumentRefCustomizationPrivate::ToDocumentPackagePath(Item, PackagePath))
				{
					PackagePaths.AddUnique(MoveTemp(PackagePath));
				}
			}
		}

		PackagePaths.Sort();
		for (FString& PackagePath : PackagePaths)
		{
			Options.Add(MakeShared<FString>(MoveTemp(PackagePath)));
		}
	}

	if (ComboBox.IsValid())
	{
		ComboBox->RefreshOptions();
	}
}

FText FArcRmlDocumentRefCustomization::GetCurrentText() const
{
	FString Path;
	const FPropertyAccess::Result Result = PathHandle->GetValue(Path);
	if (Result == FPropertyAccess::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}
	if (Result != FPropertyAccess::Success || Path.IsEmpty())
	{
		return LOCTEXT("None", "None");
	}
	return FText::FromString(Path);
}

TSharedRef<SWidget> FArcRmlDocumentRefCustomization::MakeOptionWidget(TSharedPtr<FString> Option) const
{
	return SNew(STextBlock)
		.Text(FText::FromString(Option.IsValid() ? *Option : FString()));
}

void FArcRmlDocumentRefCustomization::HandleSelectionChanged(TSharedPtr<FString> Selection, ESelectInfo::Type SelectInfo)
{
	if (Selection.IsValid() && SelectInfo != ESelectInfo::Direct)
	{
		SetPath(*Selection);
	}
}

bool FArcRmlDocumentRefCustomization::IsDropRecognized(TSharedPtr<FDragDropOperation> Operation) const
{
	FString PackagePath;
	return FindDraggedDocument(Operation, PackagePath);
}

FReply FArcRmlDocumentRefCustomization::HandleDropped(const FGeometry& Geometry, const FDragDropEvent& DragDropEvent)
{
	FString PackagePath;
	if (!FindDraggedDocument(DragDropEvent.GetOperation(), PackagePath))
	{
		return FReply::Unhandled();
	}
	SetPath(PackagePath);
	return FReply::Handled();
}

void FArcRmlDocumentRefCustomization::HandleBrowseClicked() const
{
	const FContentBrowserItem Item = FindCurrentItem();
	if (!Item.IsValid())
	{
		return;
	}
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().SyncBrowserToItems({ Item });
}

void FArcRmlDocumentRefCustomization::HandleClearClicked()
{
	SetPath(FString());
}

bool FArcRmlDocumentRefCustomization::IsEditable() const
{
	return PathHandle.IsValid() && PathHandle->IsEditable();
}

bool FArcRmlDocumentRefCustomization::CanBrowse() const
{
	FString Path;
	return PathHandle->GetValue(Path) == FPropertyAccess::Success && !Path.IsEmpty();
}

bool FArcRmlDocumentRefCustomization::FindDraggedDocument(TSharedPtr<FDragDropOperation> Operation, FString& OutPackagePath) const
{
	if (!Operation.IsValid() || !Operation->IsOfType<FContentBrowserDataDragDropOp>())
	{
		return false;
	}
	const FContentBrowserDataDragDropOp& ContentBrowserOperation = static_cast<const FContentBrowserDataDragDropOp&>(*Operation);
	const TArray<FContentBrowserItem>& Files = ContentBrowserOperation.GetDraggedFiles();
	return Files.Num() == 1 && ArcRmlDocumentRefCustomizationPrivate::ToDocumentPackagePath(Files[0], OutPackagePath);
}

FContentBrowserItem FArcRmlDocumentRefCustomization::FindCurrentItem() const
{
	FString Path;
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();
	if (!ContentBrowserData || PathHandle->GetValue(Path) != FPropertyAccess::Success || Path.IsEmpty())
	{
		return FContentBrowserItem();
	}
	const FName VirtualPath = ContentBrowserData->ConvertInternalPathToVirtual(FName(*ArcRmlPaths::NormalizePackagePath(Path)));
	return ContentBrowserData->GetItemAtPath(VirtualPath, EContentBrowserItemTypeFilter::IncludeFiles);
}

void FArcRmlDocumentRefCustomization::SetPath(const FString& PackagePath)
{
	PathHandle->SetValue(PackagePath);
}

#undef LOCTEXT_NAMESPACE
