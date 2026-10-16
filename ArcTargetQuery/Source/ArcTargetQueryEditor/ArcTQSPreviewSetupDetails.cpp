// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSPreviewSetupDetails.h"

#include "ArcTQSEditorUtils.h"
#include "ArcTQSPreviewTypes.h"
#include "ArcTQSQueryDefinition.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "StructUtils/PropertyBag.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcTQSPreviewSetupDetails"

namespace ArcTQSPreviewSetupDetailsPrivate
{
	const FName PreviewCategoryName(TEXT("Preview"));
}

TSharedRef<IDetailCustomization> FArcTQSPreviewSetupDetails::MakeInstance()
{
	return MakeShared<FArcTQSPreviewSetupDetails>();
}

void FArcTQSPreviewSetupDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	using namespace ArcTQSPreviewSetupDetailsPrivate;

	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	Definition = Objects.Num() == 1 ? Cast<UArcTQSQueryDefinition>(Objects[0].Get()) : nullptr;
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	TArray<FName> CategoryNames;
	DetailBuilder.GetCategoryNames(CategoryNames);
	for (const FName& CategoryName : CategoryNames)
	{
		if (CategoryName != PreviewCategoryName)
		{
			DetailBuilder.HideCategory(CategoryName);
		}
	}

	const TSharedRef<IPropertyHandle> SetupHandle = DetailBuilder.GetProperty(
		GET_MEMBER_NAME_CHECKED(UArcTQSQueryDefinition, PreviewSetup));
	DetailBuilder.HideProperty(SetupHandle);

	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(PreviewCategoryName);
	Category.AddProperty(SetupHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTQSPreviewSetup, Querier)));
	Category.AddProperty(SetupHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTQSPreviewSetup, Targets)));
	Category.AddProperty(SetupHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTQSPreviewSetup, Reference)));

	TArray<const FPropertyBagPropertyDesc*> EntityParameters;
	ArcTQSEditor::CollectEntityParameters(Definition.Get(), EntityParameters);
	if (!EntityParameters.IsEmpty())
	{
		IDetailGroup& Group = Category.AddGroup(TEXT("EntityParameters"), LOCTEXT("EntityParameters", "Entity Parameters"), false, true);
		for (const FPropertyBagPropertyDesc* Parameter : EntityParameters)
		{
			const FGuid ParameterID = Parameter->ID;
			Group.AddWidgetRow()
			.NameContent()
			[
				SNew(STextBlock)
				.Text(FText::FromName(Parameter->Name))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.ValueContent()
			.MinDesiredWidth(200.0f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FArcTQSPreviewSetupDetails::MakeSourceMenu, ParameterID)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FArcTQSPreviewSetupDetails::GetSourceText, ParameterID)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
		}
	}

	SeedHandle = SetupHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FArcTQSPreviewSetup, Seed));
	Category.AddProperty(SeedHandle).CustomWidget()
	.NameContent()
	[
		SeedHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(200.0f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SeedHandle->CreatePropertyValueWidget()
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 0.0f, 0.0f, 0.0f)
		[
			SNew(SButton)
			.Text(LOCTEXT("Reroll", "Reroll"))
			.OnClicked_Lambda([WeakSeedHandle = TWeakPtr<IPropertyHandle>(SeedHandle)]() -> FReply
			{
				if (const TSharedPtr<IPropertyHandle> Handle = WeakSeedHandle.Pin())
				{
					Handle->SetValue(FMath::Rand());
				}
				return FReply::Handled();
			})
		]
	];

	ArcTQS::Delegates::OnPostCompile.AddSP(this, &FArcTQSPreviewSetupDetails::HandleDefinitionCompiled);
}

TSharedRef<SWidget> FArcTQSPreviewSetupDetails::MakeSourceMenu(const FGuid ParameterID) const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NoSource", "None"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FArcTQSPreviewSetupDetails::SetSource, ParameterID, FName())));

	const UArcTQSQueryDefinition* EditedDefinition = Definition.Get();
	if (!EditedDefinition)
	{
		return MenuBuilder.MakeWidget();
	}

	TArray<FName> MockNames;
	MockNames.Add(EditedDefinition->PreviewSetup.Querier.Name);
	for (const FArcTQSPreviewMock& Target : EditedDefinition->PreviewSetup.Targets)
	{
		MockNames.AddUnique(Target.Name);
	}
	for (const FName MockName : MockNames)
	{
		if (MockName.IsNone())
		{
			continue;
		}
		MenuBuilder.AddMenuEntry(
			FText::FromName(MockName),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FArcTQSPreviewSetupDetails::SetSource, ParameterID, MockName)));
	}
	return MenuBuilder.MakeWidget();
}

FText FArcTQSPreviewSetupDetails::GetSourceText(const FGuid ParameterID) const
{
	const UArcTQSQueryDefinition* EditedDefinition = Definition.Get();
	const FName* Source = EditedDefinition ? EditedDefinition->PreviewSetup.EntityParameterSources.Find(ParameterID) : nullptr;
	return Source ? FText::FromName(*Source) : LOCTEXT("NoSource", "None");
}

void FArcTQSPreviewSetupDetails::SetSource(const FGuid ParameterID, const FName MockName) const
{
	UArcTQSQueryDefinition* EditedDefinition = Definition.Get();
	if (!EditedDefinition)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetEntityParameterSource", "Set Entity Parameter Source"));
	EditedDefinition->Modify();
	if (MockName.IsNone())
	{
		EditedDefinition->PreviewSetup.EntityParameterSources.Remove(ParameterID);
	}
	else
	{
		EditedDefinition->PreviewSetup.EntityParameterSources.Add(ParameterID, MockName);
	}
	ArcTQSEditor::NotifyPreviewSetupChanged(*EditedDefinition);

	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

void FArcTQSPreviewSetupDetails::HandleDefinitionCompiled(const UArcTQSQueryDefinition& CompiledDefinition) const
{
	if (&CompiledDefinition == Definition.Get() && PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
