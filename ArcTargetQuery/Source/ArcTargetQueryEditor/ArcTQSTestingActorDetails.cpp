// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSTestingActorDetails.h"

#include "ArcTQSEditorUtils.h"
#include "ArcTQSPreviewTargetActor.h"
#include "ArcTQSQueryDefinition.h"
#include "ArcTQSTestingActor.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "StructUtils/PropertyBag.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcTQSTestingActorDetails"

TSharedRef<IDetailCustomization> FArcTQSTestingActorDetails::MakeInstance()
{
	return MakeShared<FArcTQSTestingActorDetails>();
}

void FArcTQSTestingActorDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() != 1)
	{
		return;
	}
	TestingActor = Cast<AArcTQSTestingActor>(Objects[0].Get());
	if (!TestingActor.IsValid())
	{
		return;
	}
	PropertyUtilities = DetailBuilder.GetPropertyUtilities();

	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(AArcTQSTestingActor, EntityParameterSources));

	const TSharedRef<IPropertyHandle> QueryHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(AArcTQSTestingActor, Query));
	if (const TSharedPtr<IPropertyHandle> DefinitionHandle = QueryHandle->GetChildHandle(TEXT("Definition")))
	{
		DefinitionHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FArcTQSTestingActorDetails::RequestRefresh));
	}

	TArray<const FPropertyBagPropertyDesc*> EntityParameters;
	ArcTQSEditor::CollectEntityParameters(TestingActor->Query.GetDefinition(), EntityParameters);
	if (!EntityParameters.IsEmpty())
	{
		IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(TEXT("Query"));
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
				.OnGetMenuContent(this, &FArcTQSTestingActorDetails::MakeSourceMenu, ParameterID)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FArcTQSTestingActorDetails::GetSourceText, ParameterID)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
		}
	}

	ArcTQS::Delegates::OnPostCompile.AddSP(this, &FArcTQSTestingActorDetails::HandleDefinitionCompiled);
}

TSharedRef<SWidget> FArcTQSTestingActorDetails::MakeSourceMenu(const FGuid ParameterID) const
{
	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NoSource", "None"),
		FText::GetEmpty(),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateSP(this, &FArcTQSTestingActorDetails::SetSource, ParameterID, TWeakObjectPtr<AArcTQSPreviewTargetActor>())));

	const AArcTQSTestingActor* Actor = TestingActor.Get();
	if (!Actor)
	{
		return MenuBuilder.MakeWidget();
	}

	for (AArcTQSPreviewTargetActor* Target : Actor->Targets)
	{
		if (!Target)
		{
			continue;
		}
		MenuBuilder.AddMenuEntry(
			FText::FromString(Target->GetActorNameOrLabel()),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &FArcTQSTestingActorDetails::SetSource, ParameterID, TWeakObjectPtr<AArcTQSPreviewTargetActor>(Target))));
	}
	return MenuBuilder.MakeWidget();
}

FText FArcTQSTestingActorDetails::GetSourceText(const FGuid ParameterID) const
{
	const AArcTQSTestingActor* Actor = TestingActor.Get();
	const TObjectPtr<AArcTQSPreviewTargetActor>* Source = Actor ? Actor->EntityParameterSources.Find(ParameterID) : nullptr;
	if (!Source || !*Source)
	{
		return LOCTEXT("NoSource", "None");
	}
	return FText::FromString((*Source)->GetActorNameOrLabel());
}

void FArcTQSTestingActorDetails::SetSource(const FGuid ParameterID, const TWeakObjectPtr<AArcTQSPreviewTargetActor> Target) const
{
	AArcTQSTestingActor* Actor = TestingActor.Get();
	if (!Actor)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("SetEntityParameterSource", "Set Entity Parameter Source"));
	Actor->Modify();
	if (AArcTQSPreviewTargetActor* TargetActor = Target.Get())
	{
		Actor->EntityParameterSources.Add(ParameterID, TargetActor);
	}
	else
	{
		Actor->EntityParameterSources.Remove(ParameterID);
	}

	FProperty* SourcesProperty = FindFProperty<FProperty>(
		AArcTQSTestingActor::StaticClass(),
		GET_MEMBER_NAME_CHECKED(AArcTQSTestingActor, EntityParameterSources));
	FPropertyChangedEvent Event(SourcesProperty, EPropertyChangeType::ValueSet);
	Actor->PostEditChangeProperty(Event);

	RequestRefresh();
}

void FArcTQSTestingActorDetails::HandleDefinitionCompiled(const UArcTQSQueryDefinition& CompiledDefinition) const
{
	const AArcTQSTestingActor* Actor = TestingActor.Get();
	if (Actor && Actor->Query.GetDefinition() == &CompiledDefinition)
	{
		RequestRefresh();
	}
}

void FArcTQSTestingActorDetails::RequestRefresh() const
{
	if (PropertyUtilities.IsValid())
	{
		PropertyUtilities->RequestForceRefresh();
	}
}

#undef LOCTEXT_NAMESPACE
