// Copyright Lukasz Baran. All Rights Reserved.

#include "Widgets/SArcTacticsBindings.h"

#include "ArcTacticsEditorViewModel.h"
#include "PropertyBindingDataView.h"
#include "Styling/AppStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SPropertyBindingView.h"
#include "Widgets/Text/STextBlock.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SArcTacticsBindings)

#define LOCTEXT_NAMESPACE "SArcTacticsBindings"

void UArcTacticsBindingsViewOwner::Initialize(UArcTacticsConfig* InConfig)
{
	Config = InConfig;
	Rebuild();
}

void UArcTacticsBindingsViewOwner::Rebuild()
{
	ProjectedBindings.Reset();
	const UArcTacticsConfig* ConfigObject = Config.Get();
	if (!ConfigObject)
	{
		return;
	}
	for (const FArcTacticsBinding& Binding : ConfigObject->EditorBindings.GetBindings())
	{
		ProjectedBindings.AddCompiledBinding(Binding.IsOutputBinding()
			? FArcTacticsBinding(Binding.GetTargetPath(), Binding.GetSourcePath())
			: FArcTacticsBinding(Binding.GetPropertyFunctionNode(), Binding.GetSourcePath(), Binding.GetTargetPath()));
	}
}

bool UArcTacticsBindingsViewOwner::GetBindingDataView(
	const FPropertyBindingBinding& InBinding,
	const EBindingSide InSide,
	FPropertyBindingDataView& OutDataView)
{
	return GetBindingDataViewByID(
		InSide == EBindingSide::Source
			? InBinding.GetSourcePath().GetStructID()
			: InBinding.GetTargetPath().GetStructID(),
		OutDataView);
}

void UArcTacticsBindingsViewOwner::GetBindableStructs(
	const FGuid InTargetStructID,
	TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
{
	if (const UArcTacticsConfig* ConfigObject = Config.Get())
	{
		ConfigObject->GetBindableStructs(InTargetStructID, OutStructDescs);
	}
	else
	{
		OutStructDescs.Reset();
	}
}

bool UArcTacticsBindingsViewOwner::GetBindableStructByID(
	const FGuid InStructID,
	TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	return ConfigObject && ConfigObject->GetBindableStructByID(InStructID, OutStructDesc);
}

bool UArcTacticsBindingsViewOwner::GetBindingDataViewByID(
	const FGuid InStructID,
	FPropertyBindingDataView& OutDataView) const
{
	const UArcTacticsConfig* ConfigObject = Config.Get();
	return ConfigObject && ConfigObject->GetBindingDataViewByID(InStructID, OutDataView);
}

FPropertyBindingBindingCollection* UArcTacticsBindingsViewOwner::GetEditorPropertyBindings()
{
	return nullptr;
}

const FPropertyBindingBindingCollection* UArcTacticsBindingsViewOwner::GetEditorPropertyBindings() const
{
	return &ProjectedBindings;
}

bool UArcTacticsBindingsViewOwner::CanCreateParameter(const FGuid) const
{
	return false;
}

void UArcTacticsBindingsViewOwner::CreateParametersForStruct(
	const FGuid,
	const TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor>)
{
}

SArcTacticsBindings::~SArcTacticsBindings()
{
	if (const TSharedPtr<FArcTacticsEditorViewModel> Model = ViewModel.Pin())
	{
		Model->OnModelChanged().RemoveAll(this);
	}
}

void SArcTacticsBindings::Construct(
	const FArguments& InArgs,
	const TSharedRef<FArcTacticsEditorViewModel> InViewModel)
{
	ViewModel = InViewModel;
	OnBindingPathClicked = InArgs._OnBindingPathClicked;
	InViewModel->OnModelChanged().AddSP(this, &SArcTacticsBindings::HandleModelChanged);
	ViewOwner.Reset(NewObject<UArcTacticsBindingsViewOwner>());
	ViewOwner->Initialize(InViewModel->GetConfig());

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(6.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PropertyBindings", "Property Bindings"))
			.TextStyle(FAppStyle::Get(), TEXT("DetailsView.CategoryTextStyle"))
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(4.0f, 0.0f, 4.0f, 4.0f)
		[
			SNew(UE::PropertyBinding::SBindingView)
			.CollectionOwner(TScriptInterface<const IPropertyBindingBindingCollectionOwner>(ViewOwner.Get()))
			.OnBindingClicked(this, &SArcTacticsBindings::HandleBindingPathClicked)
		]
	];
}

void SArcTacticsBindings::HandleModelChanged(const EArcTacticsAuthoringChange)
{
	if (ViewOwner.IsValid())
	{
		ViewOwner->Rebuild();
	}
}

void SArcTacticsBindings::HandleBindingPathClicked(
	const bool bIsStruct,
	const FPropertyBindingPath& BindingPath) const
{
	OnBindingPathClicked.ExecuteIfBound(bIsStruct, BindingPath);
}

#undef LOCTEXT_NAMESPACE
