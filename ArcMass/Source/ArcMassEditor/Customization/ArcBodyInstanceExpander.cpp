// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcBodyInstanceExpander.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/CollisionProfile.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "IDocumentation.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcBodyInstanceExpander"

#define RowWidth_Customization 50.f

// Template exploit to access private FBodyInstance::CollisionResponses
template<typename Tag, typename Tag::Type M>
struct TAccessPrivate
{
	friend typename Tag::Type Get(Tag) { return M; }
};

struct FBodyInstanceCollisionResponsesTag
{
	using Type = FCollisionResponse FBodyInstance::*;
	friend Type Get(FBodyInstanceCollisionResponsesTag);
};
template struct TAccessPrivate<FBodyInstanceCollisionResponsesTag, &FBodyInstance::CollisionResponses>;

namespace ArcBodyInstanceExpanderPrivate
{
	FCollisionResponse& GetCollisionResponses(FBodyInstance& BI)
	{
		return BI.*Get(FBodyInstanceCollisionResponsesTag{});
	}

	bool IsValidCollisionProfileName(UCollisionProfile* Profile, FName ProfileName)
	{
		FCollisionResponseTemplate Template;
		return Profile->GetProfileTemplate(ProfileName, Template);
	}
}

////////////////////////////////////////////////////////////////
// FArcBodyInstanceExpander
////////////////////////////////////////////////////////////////

void FArcBodyInstanceExpander::RefreshCollisionProfiles()
{
	int32 NumProfiles = CollisionProfile->GetNumOfProfiles();

	CollisionProfileComboList.Empty(NumProfiles + 1);

	// First entry is always "Custom..."
	CollisionProfileComboList.Add(MakeShareable(new FString(TEXT("Custom..."))));

	for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
	{
		CollisionProfileComboList.Add(MakeShareable(new FString(CollisionProfile->GetProfileByIndex(ProfileId)->Name.ToString())));
	}

	if (CollisionProfileComboBox.IsValid())
	{
		CollisionProfileComboBox->RefreshOptions();
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FArcBodyInstanceExpander::BuildBodyInstanceUI(TSharedRef<IPropertyHandle> InBodyInstanceHandle, IDetailChildrenBuilder& ChildrenBuilder)
{
	BodyInstanceHandle = InBodyInstanceHandle;
	CollisionProfile = UCollisionProfile::Get();

	// Access raw body instance pointers
	TArray<void*> StructPtrs;
	InBodyInstanceHandle->AccessRawData(StructPtrs);
	check(StructPtrs.Num() != 0);

	BodyInstances.Empty(StructPtrs.Num());
	for (int32 Index = 0; Index < StructPtrs.Num(); ++Index)
	{
		check(StructPtrs[Index]);
		BodyInstances.Add(static_cast<FBodyInstance*>(StructPtrs[Index]));
	}

	RefreshCollisionProfiles();

	CollisionProfileNameHandle = InBodyInstanceHandle->GetChildHandle(TEXT("CollisionProfileName"));
	check(CollisionProfileNameHandle.IsValid());

	CollisionEnabledHandle = InBodyInstanceHandle->GetChildHandle(TEXT("CollisionEnabled"));
	check(CollisionEnabledHandle.IsValid());

	ObjectTypeHandle = InBodyInstanceHandle->GetChildHandle(TEXT("ObjectType"));
	check(ObjectTypeHandle.IsValid());

	CollisionResponsesHandle = InBodyInstanceHandle->GetChildHandle(TEXT("CollisionResponses"));
	check(CollisionResponsesHandle.IsValid());

	CollisionResponsesHandle->SetIgnoreValidation(true);

	// -- Collision group --
	IDetailGroup& CollisionGroup = ChildrenBuilder.AddGroup(TEXT("Collision"), LOCTEXT("CollisionGroupLabel", "Collision"), /*bStartExpanded=*/ true);
	CreateCollisionSetup(CollisionGroup, InBodyInstanceHandle);
	AddCollisionProperties(CollisionGroup, InBodyInstanceHandle);

	// -- Physics group --
	IDetailGroup& PhysicsGroup = ChildrenBuilder.AddGroup(TEXT("Physics"), LOCTEXT("PhysicsGroupLabel", "Physics"), /*bStartExpanded=*/ true);
	AddPhysicsProperties(PhysicsGroup, InBodyInstanceHandle);
}

void FArcBodyInstanceExpander::CreateCollisionSetup(IDetailGroup& CollisionGroup, TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	UpdateValidCollisionChannels();

	if (ValidCollisionChannels.Num() == 0)
	{
		return;
	}

	// Determine current profile for the combo box initial selection
	FName ProfileName;
	TSharedPtr<FString> DisplayName = CollisionProfileComboList[0];
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::Success && ArcBodyInstanceExpanderPrivate::IsValidCollisionProfileName(CollisionProfile, ProfileName))
	{
		DisplayName = GetProfileString(ProfileName);
	}

	const FString PresetsDocLink = TEXT("Shared/Collision");
	TSharedPtr<SToolTip> ProfileTooltip = IDocumentation::Get()->CreateToolTip(
		LOCTEXT("SelectCollisionPreset", "Select collision presets. You can set this data in Project settings."),
		nullptr, PresetsDocLink, TEXT("PresetDetail"));

	// Collision Presets header row
	CollisionGroup.AddWidgetRow()
	.OverrideResetToDefault(FResetToDefaultOverride::Create(
		TAttribute<bool>::Create([this]() { return ShouldShowResetToDefaultProfile(); }),
		FSimpleDelegate::CreateLambda([this]() { SetToDefaultProfile(); })
	))
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CollisionPresetsLabel", "Collision Presets"))
			.ToolTip(ProfileTooltip)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		.IsEnabled(this, &FArcBodyInstanceExpander::IsCollisionEnabled)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		[
			SAssignNew(CollisionProfileComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&CollisionProfileComboList)
			.OnGenerateWidget(this, &FArcBodyInstanceExpander::MakeCollisionProfileComboWidget)
			.OnSelectionChanged(this, &FArcBodyInstanceExpander::OnCollisionProfileChanged)
			.OnComboBoxOpening(this, &FArcBodyInstanceExpander::OnCollisionProfileComboOpening)
			.InitiallySelectedItem(DisplayName)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FArcBodyInstanceExpander::GetCollisionProfileComboBoxContent)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ToolTipText(this, &FArcBodyInstanceExpander::GetCollisionProfileComboBoxToolTip)
			]
		]
	];

	int32 TotalNumChildren = ValidCollisionChannels.Num();
	TAttribute<bool> CustomCollisionEnabled(this, &FArcBodyInstanceExpander::ShouldEnableCustomCollisionSetup);
	TAttribute<EVisibility> CustomCollisionVisibility(this, &FArcBodyInstanceExpander::ShouldShowCustomCollisionSetup);

	int32 IndexSelected = InitializeObjectTypeComboList();
	CollisionGroup.AddPropertyRow(CollisionEnabledHandle.ToSharedRef())
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility);

	if (!StructPropertyHandle->GetProperty()->GetBoolMetaData(TEXT("HideObjectType")))
	{
		CollisionGroup.AddWidgetRow()
		.Visibility(CustomCollisionVisibility)
		.IsEnabled(CustomCollisionEnabled)
		.NameContent()
		[
			ObjectTypeHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SAssignNew(ObjectTypeComboBox, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&ObjectTypeComboList)
			.OnGenerateWidget(this, &FArcBodyInstanceExpander::MakeObjectTypeComboWidget)
			.OnSelectionChanged(this, &FArcBodyInstanceExpander::OnObjectTypeChanged)
			.InitiallySelectedItem(ObjectTypeComboList[IndexSelected])
			.ContentPadding(2.f)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &FArcBodyInstanceExpander::GetObjectTypeComboBoxContent)
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
		];
	}

	// Column headers: Ignore / Overlap / Block
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.ValueContent()
	.MaxDesiredWidth(0.f)
	.MinDesiredWidth(0.f)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.HAlign(HAlign_Left)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IgnoreCollisionLabel", "Ignore"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.HAlign(HAlign_Left)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OverlapCollisionLabel", "Overlap"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BlockCollisionLabel", "Block"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];

	// "All" row with master checkboxes
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.NameContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.Padding(2.0f)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CollisionResponsesLabel", "Collision Responses"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
			.ToolTipText(LOCTEXT("CollisionResponse_ToolTip", "When trace by channel, this information will be used for filtering."))
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		[
			IDocumentation::Get()->CreateAnchor(TEXT("Engine/Physics/Collision"))
		]
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnAllCollisionChannelChanged, ECR_Ignore)
				.IsChecked(this, &FArcBodyInstanceExpander::IsAllCollisionChannelChecked, ECR_Ignore)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnAllCollisionChannelChanged, ECR_Overlap)
				.IsChecked(this, &FArcBodyInstanceExpander::IsAllCollisionChannelChecked, ECR_Overlap)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(RowWidth_Customization)
			.Content()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnAllCollisionChannelChanged, ECR_Block)
				.IsChecked(this, &FArcBodyInstanceExpander::IsAllCollisionChannelChecked, ECR_Block)
			]
		]
	];

	// Trace Responses header
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.NameContent()
	[
		SNew(SBox)
		.Padding(FMargin(10, 0))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CollisionTraceResponsesLabel", "Trace Responses"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];

	// Per-channel rows — trace types first
	for (int32 Index = 0; Index < TotalNumChildren; ++Index)
	{
		if (ValidCollisionChannels[Index].bTraceType)
		{
			FString ChannelDisplayName = ValidCollisionChannels[Index].DisplayName;

			CollisionGroup.AddWidgetRow()
			.IsEnabled(CustomCollisionEnabled)
			.Visibility(CustomCollisionVisibility)
			.OverrideResetToDefault(FResetToDefaultOverride::Create(
				TAttribute<bool>::Create([this, Index]() { return ShouldShowResetToDefaultResponse(Index); }),
				FSimpleDelegate::CreateLambda([this, Index]() { SetToDefaultResponse(Index); })
			))
			.NameContent()
			[
				SNew(SBox)
				.Padding(FMargin(15, 0))
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(ChannelDisplayName))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnCollisionChannelChanged, Index, ECR_Ignore)
						.IsChecked(this, &FArcBodyInstanceExpander::IsCollisionChannelChecked, Index, ECR_Ignore)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnCollisionChannelChanged, Index, ECR_Overlap)
						.IsChecked(this, &FArcBodyInstanceExpander::IsCollisionChannelChecked, Index, ECR_Overlap)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnCollisionChannelChanged, Index, ECR_Block)
					.IsChecked(this, &FArcBodyInstanceExpander::IsCollisionChannelChecked, Index, ECR_Block)
				]
			];
		}
	}

	// Object Responses header
	CollisionGroup.AddWidgetRow()
	.IsEnabled(CustomCollisionEnabled)
	.Visibility(CustomCollisionVisibility)
	.NameContent()
	[
		SNew(SBox)
		.Padding(FMargin(10, 0))
		.Content()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CollisionObjectResponses", "Object Responses"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	];

	// Per-channel rows — object types
	for (int32 Index = 0; Index < TotalNumChildren; ++Index)
	{
		if (!ValidCollisionChannels[Index].bTraceType)
		{
			FString ChannelDisplayName = ValidCollisionChannels[Index].DisplayName;

			CollisionGroup.AddWidgetRow()
			.IsEnabled(CustomCollisionEnabled)
			.Visibility(CustomCollisionVisibility)
			.OverrideResetToDefault(FResetToDefaultOverride::Create(
				TAttribute<bool>::Create([this, Index]() { return ShouldShowResetToDefaultResponse(Index); }),
				FSimpleDelegate::CreateLambda([this, Index]() { SetToDefaultResponse(Index); })
			))
			.NameContent()
			[
				SNew(SBox)
				.Padding(FMargin(15, 0))
				.Content()
				[
					SNew(STextBlock)
					.Text(FText::FromString(ChannelDisplayName))
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnCollisionChannelChanged, Index, ECR_Ignore)
						.IsChecked(this, &FArcBodyInstanceExpander::IsCollisionChannelChecked, Index, ECR_Ignore)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(SCheckBox)
						.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnCollisionChannelChanged, Index, ECR_Overlap)
						.IsChecked(this, &FArcBodyInstanceExpander::IsCollisionChannelChecked, Index, ECR_Overlap)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &FArcBodyInstanceExpander::OnCollisionChannelChanged, Index, ECR_Block)
					.IsChecked(this, &FArcBodyInstanceExpander::IsCollisionChannelChecked, Index, ECR_Block)
				]
			];
		}
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

////////////////////////////////////////////////////////////////
// Combo / selection helpers
////////////////////////////////////////////////////////////////

int32 FArcBodyInstanceExpander::InitializeObjectTypeComboList()
{
	ObjectTypeComboList.Empty();
	ObjectTypeValues.Empty();

	UEnum* Enum = StaticEnum<ECollisionChannel>();
	const FString KeyName = TEXT("DisplayName");
	const FString QueryType = TEXT("TraceQuery");

	int32 NumEnum = Enum->NumEnums();
	int32 Selected = 0;
	uint8 ObjectTypeIndex = 0;
	if (ObjectTypeHandle->GetValue(ObjectTypeIndex) != FPropertyAccess::Result::Success)
	{
		ObjectTypeIndex = 0;
	}

	for (int32 EnumIndex = 0; EnumIndex < NumEnum; ++EnumIndex)
	{
		const FString& QueryTypeMetaData = Enum->GetMetaData(*QueryType, EnumIndex);
		if (QueryTypeMetaData.Len() == 0 || QueryTypeMetaData[0] == '0')
		{
			const FString& KeyNameMetaData = Enum->GetMetaData(*KeyName, EnumIndex);
			if (KeyNameMetaData.Len() > 0)
			{
				int32 NewIndex = ObjectTypeComboList.Add(MakeShareable(new FString(KeyNameMetaData)));
				ObjectTypeValues.Add(static_cast<ECollisionChannel>(EnumIndex));

				if (ObjectTypeIndex == EnumIndex)
				{
					Selected = NewIndex;
				}
			}
		}
	}

	check(ObjectTypeComboList.Num() > 0);
	return Selected;
}

TSharedPtr<FString> FArcBodyInstanceExpander::GetProfileString(FName ProfileName) const
{
	FString ProfileNameString = ProfileName.ToString();

	int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
	// 1 special profile ("Custom...")
	if (NumProfiles + 1 == CollisionProfileComboList.Num())
	{
		for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
		{
			if (*CollisionProfileComboList[ProfileId + 1].Get() == ProfileNameString)
			{
				return CollisionProfileComboList[ProfileId + 1];
			}
		}
	}

	// Not found — return "Custom..."
	return CollisionProfileComboList[0];
}

void FArcBodyInstanceExpander::UpdateValidCollisionChannels()
{
	UEnum* Enum = StaticEnum<ECollisionChannel>();
	check(Enum);
	const FString KeyName = TEXT("DisplayName");
	const FString TraceType = TEXT("TraceQuery");

	int32 NumEnum = Enum->NumEnums();
	ValidCollisionChannels.Empty(NumEnum);

	for (int32 EnumIndex = 0; EnumIndex < NumEnum; ++EnumIndex)
	{
		const FString& MetaData = Enum->GetMetaData(*KeyName, EnumIndex);
		if (MetaData.Len() > 0)
		{
			FArcCollisionChannelInfo Info;
			Info.DisplayName = MetaData;
			Info.CollisionChannel = static_cast<ECollisionChannel>(EnumIndex);
			Info.bTraceType = (Enum->GetMetaData(*TraceType, EnumIndex) == TEXT("1"));
			ValidCollisionChannels.Add(Info);
		}
	}
}

TSharedRef<SWidget> FArcBodyInstanceExpander::MakeObjectTypeComboWidget(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(*InItem)).Font(IDetailLayoutBuilder::GetDetailFont());
}

void FArcBodyInstanceExpander::OnObjectTypeChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		ECollisionChannel NewEnumVal = ECC_WorldStatic;

		for (int32 Index = 0; Index < ObjectTypeComboList.Num(); ++Index)
		{
			if (*ObjectTypeComboList[Index].Get() == NewValue)
			{
				NewEnumVal = ObjectTypeValues[Index];
			}
		}
		ensure(ObjectTypeHandle->SetValue(static_cast<uint8>(NewEnumVal)) == FPropertyAccess::Result::Success);
	}
}

FText FArcBodyInstanceExpander::GetObjectTypeComboBoxContent() const
{
	FName ObjectTypeName;
	if (ObjectTypeHandle->GetValue(ObjectTypeName) == FPropertyAccess::Result::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::FromString(*ObjectTypeComboBox.Get()->GetSelectedItem().Get());
}

TSharedRef<SWidget> FArcBodyInstanceExpander::MakeCollisionProfileComboWidget(TSharedPtr<FString> InItem)
{
	FString ProfileMessage;

	FCollisionResponseTemplate ProfileData;
	if (CollisionProfile->GetProfileTemplate(FName(**InItem), ProfileData))
	{
		ProfileMessage = ProfileData.HelpMessage;
	}

	return
		SNew(STextBlock)
		.Text(FText::FromString(*InItem))
		.ToolTipText(FText::FromString(ProfileMessage))
		.Font(IDetailLayoutBuilder::GetDetailFont());
}

void FArcBodyInstanceExpander::OnCollisionProfileComboOpening()
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) != FPropertyAccess::Result::MultipleValues)
	{
		TSharedPtr<FString> ComboStringPtr = GetProfileString(ProfileName);
		if (ComboStringPtr.IsValid())
		{
			CollisionProfileComboBox->SetSelectedItem(ComboStringPtr);
		}
	}
}

void FArcBodyInstanceExpander::OnCollisionProfileChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
		for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
		{
			const FCollisionResponseTemplate* CurProfile = CollisionProfile->GetProfileByIndex(ProfileId);
			if (NewValue == CurProfile->Name.ToString())
			{
				const FScopedTransaction Transaction(LOCTEXT("ChangeCollisionProfile", "Change Collision Profile"));
				ensure(CollisionProfileNameHandle->SetValue(NewValue) == FPropertyAccess::Result::Success);
				UpdateCollisionProfile();
				return;
			}
		}

		// "Custom..." selected — clear profile name
		if (NewSelection == CollisionProfileComboList[0])
		{
			// nothing extra needed, just set custom name
		}

		FName Name = UCollisionProfile::CustomCollisionProfileName;
		ensure(CollisionProfileNameHandle->SetValue(Name) == FPropertyAccess::Result::Success);
	}
}

void FArcBodyInstanceExpander::UpdateCollisionProfile()
{
	FName ProfileName;

	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::Success && ArcBodyInstanceExpanderPrivate::IsValidCollisionProfileName(CollisionProfile, ProfileName))
	{
		int32 NumProfiles = CollisionProfile->GetNumOfProfiles();
		for (int32 ProfileId = 0; ProfileId < NumProfiles; ++ProfileId)
		{
			const FCollisionResponseTemplate* CurProfile = CollisionProfile->GetProfileByIndex(ProfileId);
			if (ProfileName == CurProfile->Name)
			{
				ensure(CollisionEnabledHandle->SetValue(static_cast<uint8>(CurProfile->CollisionEnabled)) == FPropertyAccess::Result::Success);
				ensure(ObjectTypeHandle->SetValue(static_cast<uint8>(CurProfile->ObjectType)) == FPropertyAccess::Result::Success);

				SetCollisionResponseContainer(CurProfile->ResponseToChannels);

				// +1 for "Custom..." at index 0
				CollisionProfileComboBox.Get()->SetSelectedItem(CollisionProfileComboList[ProfileId + 1]);
				if (ObjectTypeComboBox.IsValid())
				{
					for (int32 ObjIdx = 0; ObjIdx < ObjectTypeValues.Num(); ++ObjIdx)
					{
						if (ObjectTypeValues[ObjIdx] == CurProfile->ObjectType)
						{
							ObjectTypeComboBox.Get()->SetSelectedItem(ObjectTypeComboList[ObjIdx]);
							break;
						}
					}
				}

				return;
			}
		}
	}

	// Fall back to "Custom..."
	CollisionProfileComboBox.Get()->SetSelectedItem(CollisionProfileComboList[0]);
}

void FArcBodyInstanceExpander::SetToDefaultProfile()
{
	const FScopedTransaction Transaction(LOCTEXT("ResetCollisionProfile", "Reset Collision Profile"));
	CollisionProfileNameHandle.Get()->ResetToDefault();
	UpdateCollisionProfile();
}

bool FArcBodyInstanceExpander::ShouldShowResetToDefaultProfile() const
{
	return CollisionProfileNameHandle.Get()->DiffersFromDefault();
}

FText FArcBodyInstanceExpander::GetCollisionProfileComboBoxContent() const
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::MultipleValues)
	{
		return LOCTEXT("MultipleValues", "Multiple Values");
	}

	return FText::FromString(*GetProfileString(ProfileName).Get());
}

FText FArcBodyInstanceExpander::GetCollisionProfileComboBoxToolTip() const
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::Success)
	{
		FCollisionResponseTemplate ProfileData;
		if (CollisionProfile->GetProfileTemplate(ProfileName, ProfileData))
		{
			return FText::FromString(ProfileData.HelpMessage);
		}
		return FText::GetEmpty();
	}

	return LOCTEXT("MultipleValues", "Multiple Values");
}

////////////////////////////////////////////////////////////////
// Collision channel responses
////////////////////////////////////////////////////////////////

void FArcBodyInstanceExpander::OnCollisionChannelChanged(ECheckBoxState InNewValue, int32 ValidIndex, ECollisionResponse InCollisionResponse)
{
	if (ValidCollisionChannels.IsValidIndex(ValidIndex))
	{
		SetResponse(ValidIndex, InCollisionResponse);
	}
}

ECheckBoxState FArcBodyInstanceExpander::IsCollisionChannelChecked(int32 ValidIndex, ECollisionResponse InCollisionResponse) const
{
	TArray<ECollisionResponse> CollisionResponses;

	if (ValidCollisionChannels.IsValidIndex(ValidIndex))
	{
		for (int32 Idx = 0; Idx < BodyInstances.Num(); ++Idx)
		{
			FBodyInstance* BI = BodyInstances[Idx];
			CollisionResponses.AddUnique(ArcBodyInstanceExpanderPrivate::GetCollisionResponses(*BI).GetResponse(ValidCollisionChannels[ValidIndex].CollisionChannel));
		}

		if (CollisionResponses.Num() == 1)
		{
			if (CollisionResponses[0] == InCollisionResponse)
			{
				return ECheckBoxState::Checked;
			}
			else
			{
				return ECheckBoxState::Unchecked;
			}
		}
		else if (CollisionResponses.Contains(InCollisionResponse))
		{
			return ECheckBoxState::Undetermined;
		}

		return ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Undetermined;
}

void FArcBodyInstanceExpander::OnAllCollisionChannelChanged(ECheckBoxState InNewValue, ECollisionResponse InCollisionResponse)
{
	FCollisionResponseContainer NewContainer;
	NewContainer.SetAllChannels(InCollisionResponse);
	SetCollisionResponseContainer(NewContainer);
}

ECheckBoxState FArcBodyInstanceExpander::IsAllCollisionChannelChecked(ECollisionResponse InCollisionResponse) const
{
	ECheckBoxState State = ECheckBoxState::Undetermined;

	uint32 TotalNumChildren = ValidCollisionChannels.Num();
	if (TotalNumChildren >= 1)
	{
		State = IsCollisionChannelChecked(0, InCollisionResponse);

		for (uint32 Index = 1; Index < TotalNumChildren; ++Index)
		{
			if (State != IsCollisionChannelChecked(Index, InCollisionResponse))
			{
				State = ECheckBoxState::Undetermined;
				break;
			}
		}
	}

	return State;
}

void FArcBodyInstanceExpander::SetResponse(int32 ValidIndex, ECollisionResponse InCollisionResponse)
{
	const FScopedTransaction Transaction(LOCTEXT("ChangeIndividualChannel", "Change Individual Channel"));

	CollisionResponsesHandle->NotifyPreChange();

	for (FBodyInstance* BI : BodyInstances)
	{
		ArcBodyInstanceExpanderPrivate::GetCollisionResponses(*BI).SetResponse(ValidCollisionChannels[ValidIndex].CollisionChannel, InCollisionResponse);
	}

	CollisionResponsesHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

void FArcBodyInstanceExpander::SetCollisionResponseContainer(const FCollisionResponseContainer& ResponseContainer)
{
	uint32 TotalNumChildren = ValidCollisionChannels.Num();

	if (TotalNumChildren)
	{
		const FScopedTransaction Transaction(LOCTEXT("Collision", "Collision Channel Changes"));

		CollisionResponsesHandle->NotifyPreChange();

		for (FBodyInstance* BI : BodyInstances)
		{
			for (uint32 Index = 0; Index < TotalNumChildren; ++Index)
			{
				ECollisionChannel Channel = ValidCollisionChannels[Index].CollisionChannel;
				ECollisionResponse Response = ResponseContainer.GetResponse(Channel);
				ArcBodyInstanceExpanderPrivate::GetCollisionResponses(*BI).SetResponse(Channel, Response);
			}
		}

		CollisionResponsesHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

void FArcBodyInstanceExpander::SetToDefaultResponse(int32 Index)
{
	if (ValidCollisionChannels.IsValidIndex(Index))
	{
		const FScopedTransaction Transaction(LOCTEXT("ResetCollisionResponse", "Reset Collision Response"));
		ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(ValidCollisionChannels[Index].CollisionChannel);
		SetResponse(Index, DefaultResponse);
	}
}

bool FArcBodyInstanceExpander::ShouldShowResetToDefaultResponse(int32 Index) const
{
	if (ValidCollisionChannels.IsValidIndex(Index))
	{
		ECollisionResponse DefaultResponse = FCollisionResponseContainer::GetDefaultResponseContainer().GetResponse(ValidCollisionChannels[Index].CollisionChannel);
		if (IsCollisionChannelChecked(Index, DefaultResponse) != ECheckBoxState::Checked)
		{
			return true;
		}
	}

	return false;
}

bool FArcBodyInstanceExpander::IsCollisionEnabled() const
{
	bool bEnabled = false;
	if (BodyInstanceHandle.IsValid())
	{
		bEnabled = BodyInstanceHandle->IsEditable() && FSlateApplication::Get().GetNormalExecutionAttribute().Get();
	}
	return bEnabled;
}

bool FArcBodyInstanceExpander::ShouldEnableCustomCollisionSetup() const
{
	FName ProfileName;
	if (CollisionProfileNameHandle->GetValue(ProfileName) == FPropertyAccess::Result::Success && ArcBodyInstanceExpanderPrivate::IsValidCollisionProfileName(CollisionProfile, ProfileName) == false)
	{
		return IsCollisionEnabled();
	}

	return false;
}

EVisibility FArcBodyInstanceExpander::ShouldShowCustomCollisionSetup() const
{
	return EVisibility::Visible;
}

////////////////////////////////////////////////////////////////
// Collision properties
////////////////////////////////////////////////////////////////

IDetailPropertyRow* FArcBodyInstanceExpander::AddPropertyToGroup(IDetailGroup& Group, TSharedRef<IPropertyHandle> StructHandle, FName PropertyName)
{
	TSharedPtr<IPropertyHandle> ChildHandle = StructHandle->GetChildHandle(PropertyName);
	if (ChildHandle.IsValid())
	{
		return &Group.AddPropertyRow(ChildHandle.ToSharedRef());
	}
	return nullptr;
}

void FArcBodyInstanceExpander::AddCollisionProperties(IDetailGroup& CollisionGroup, TSharedRef<IPropertyHandle> InBodyInstanceHandle)
{
	AddPropertyToGroup(CollisionGroup, InBodyInstanceHandle, TEXT("PhysMaterialOverride"));
	AddPropertyToGroup(CollisionGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bNotifyRigidBodyCollision));
	AddPropertyToGroup(CollisionGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bUseCCD));
	AddPropertyToGroup(CollisionGroup, InBodyInstanceHandle, TEXT("bUseMACD"));
	AddPropertyToGroup(CollisionGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bIgnoreAnalyticCollisions));
	AddPropertyToGroup(CollisionGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bSmoothEdgeCollisions));
}

////////////////////////////////////////////////////////////////
// Physics properties
////////////////////////////////////////////////////////////////

void FArcBodyInstanceExpander::AddPhysicsProperties(IDetailGroup& PhysicsGroup, TSharedRef<IPropertyHandle> InBodyInstanceHandle)
{
	// Primary
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bSimulatePhysics));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, TEXT("MassInKgOverride"));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, LinearDamping));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, AngularDamping));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bEnableGravity));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, GravityGroupIndex));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, TEXT("bInertiaConditioning"));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, DOFMode));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, CustomDOFPlaneNormal));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockTranslation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockRotation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockXTranslation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockYTranslation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockZTranslation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockXRotation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockYRotation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bLockZRotation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, TEXT("WalkableSlopeOverride"));

	// Advanced
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bAutoWeld));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bStartAwake));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bGenerateWakeEvents));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bUpdateKinematicFromSimulation));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bGyroscopicTorqueEnabled));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, COMNudge));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, MassScale));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bUpdateMassWhenScaleChanges));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, InertiaTensorScale));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, MaxAngularVelocity));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, TEXT("MaxDepenetrationVelocity"));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, TEXT("bOneWayInteraction"));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, TEXT("bAllowPartialIslandSleep"));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, SleepFamily));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, CustomSleepThresholdMultiplier));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, StabilizationThresholdMultiplier));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, bOverrideSolverAsyncDeltaTime));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, SolverAsyncDeltaTime));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, PositionSolverIterationCount));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, VelocitySolverIterationCount));
	AddPropertyToGroup(PhysicsGroup, InBodyInstanceHandle, GET_MEMBER_NAME_CHECKED(FBodyInstance, ProjectionSolverIterationCount));
}

////////////////////////////////////////////////////////////////
// FArcBodyInstanceNodeBuilder
////////////////////////////////////////////////////////////////

void FArcBodyInstanceNodeBuilder::GenerateHeaderRowContent(FDetailWidgetRow& NodeRow)
{
	NodeRow.NameContent()
	[
		BodyInstanceHandle->CreatePropertyNameWidget()
	];
}

void FArcBodyInstanceNodeBuilder::GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder)
{
	Expander->BuildBodyInstanceUI(BodyInstanceHandle, ChildrenBuilder);
}

#undef RowWidth_Customization
#undef LOCTEXT_NAMESPACE
