// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsBindingExtension.h"

#include "ArcPropertyFunctionEditor.h"
#include "ArcTQSBindings.h"
#include "ArcTacticsEditorStyle.h"
#include "ArcTacticsModule.h"
#include "ArcTacticsNodePresentation.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsConfig.h"
#include "Decision/ArcTacticsNode.h"
#include "DetailWidgetRow.h"
#include "IPropertyAccessEditor.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingTypes.h"
#include "PropertyHandle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace ArcTacticsBindingExtensionPrivate
{
	EArcTacticsPropertyRole ResolveRole(const FProperty& Property, const IPropertyHandle* Handle = nullptr)
	{
		const EArcTacticsPropertyRole StaticRole = ArcTacticsNode::ResolvePropertyRole(Property);
		if (StaticRole != EArcTacticsPropertyRole::Invalid || !Handle)
		{
			return StaticRole;
		}
		if (Handle->HasMetaData(TEXT("ArcTacticsInput"))
			|| Handle->GetInstanceMetaData(TEXT("ArcTacticsInput")))
		{
			return EArcTacticsPropertyRole::Input;
		}
		if (Handle->HasMetaData(TEXT("ArcTacticsOutput"))
			|| Handle->GetInstanceMetaData(TEXT("ArcTacticsOutput")))
		{
			return EArcTacticsPropertyRole::Output;
		}
		if (Handle->HasMetaData(TEXT("ArcTacticsParameter"))
			|| Handle->GetInstanceMetaData(TEXT("ArcTacticsParameter")))
		{
			return EArcTacticsPropertyRole::Parameter;
		}
		return EArcTacticsPropertyRole::Invalid;
	}

	EArcTacticsPropertyRole ResolveRootRole(const IPropertyHandle& PropertyHandle)
	{
		EArcTacticsPropertyRole Role = EArcTacticsPropertyRole::Invalid;
		TSharedPtr<const IPropertyHandle> Current = PropertyHandle.AsShared();
		while (Current.IsValid())
		{
			if (const FProperty* Property = Current->GetProperty())
			{
				const EArcTacticsPropertyRole CurrentRole = ResolveRole(*Property, Current.Get());
				if (CurrentRole != EArcTacticsPropertyRole::Invalid)
				{
					Role = CurrentRole;
				}
			}
			Current = Current->GetParentHandle();
		}
		return Role;
	}

	bool BuildSourcePath(
		UArcTacticsConfig& Config,
		const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> Descriptors,
		const TConstArrayView<FBindingChainElement> BindingChain,
		FPropertyBindingPath& OutPath)
	{
		if (BindingChain.IsEmpty() || !Descriptors.IsValidIndex(BindingChain[0].ArrayIndex))
		{
			return false;
		}
		OutPath.Reset();
		OutPath.SetStructID(Descriptors[BindingChain[0].ArrayIndex].Get().ID);
		for (int32 Index = 1; Index < BindingChain.Num(); ++Index)
		{
			if (const FProperty* Property = BindingChain[Index].Field.Get<FProperty>())
			{
				OutPath.AddPathSegment(Property->GetFName(), BindingChain[Index].ArrayIndex);
			}
			else if (const UFunction* Function = BindingChain[Index].Field.Get<UFunction>())
			{
				OutPath.AddPathSegment(Function->GetFName());
			}
		}
		FPropertyBindingDataView DataView;
		if (!Config.GetBindingDataViewByID(OutPath.GetStructID(), DataView))
		{
			return false;
		}
		OutPath.UpdateSegmentsFromValue(DataView);
		return true;
	}

	FName GetRoleBrushName(const EArcTacticsPropertyRole Role)
	{
		return Role == EArcTacticsPropertyRole::Input ? TEXT("ArcTactics.Role.Input")
			: Role == EArcTacticsPropertyRole::Parameter ? TEXT("ArcTactics.Role.Parameter")
			: TEXT("ArcTactics.Role.Output");
	}

	class FCachedBindingData final : public FArcPropertyFunctionCachedBindingData
	{
	public:
		FCachedBindingData(
			IPropertyBindingBindingCollectionOwner* InBindingsOwner,
			const FPropertyBindingPath& InTargetPath,
			const TSharedPtr<const IPropertyHandle>& InPropertyHandle,
			const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs,
			const EArcTacticsPropertyRole InTargetRole)
			: FArcPropertyFunctionCachedBindingData(
				InBindingsOwner, InTargetPath, InPropertyHandle, InAccessibleStructs)
			, TargetRole(InTargetRole)
		{
		}

		EArcTacticsPropertyRole GetTargetRole() const { return TargetRole; }

		virtual bool CanAcceptPropertyOrChildrenInternal(
			const FProperty& InProperty,
			const TConstArrayView<FBindingChainElement> InBindingChain) override
		{
			UArcTacticsConfig* Config = Cast<UArcTacticsConfig>(GetOwner());
			if (!Config)
			{
				return false;
			}
			if (GetChainFunctionStruct(InBindingChain))
			{
				return TargetRole != EArcTacticsPropertyRole::Output
					&& ArcPropertyFunctionEditor::CanAcceptFunctionProperty(InProperty, InBindingChain);
			}
			TArray<FBindingChainElement> CandidateChain(InBindingChain);
			CandidateChain.Emplace(const_cast<FProperty*>(&InProperty), INDEX_NONE);
			FPropertyBindingPath SourcePath;
			if (!BuildSourcePath(*Config, GetAccessibleStructs(), CandidateChain, SourcePath))
			{
				return false;
			}
			const UStruct* SourceStruct = GetAccessibleStructs()[InBindingChain[0].ArrayIndex].Get().Struct.Get();
			TInstancedStruct<FPropertyBindingBindableStructDescriptor> TargetDescriptor;
			FArcTacticsBindingEndpoint SourceEndpoint;
			FArcTacticsBindingEndpoint TargetEndpoint;
			if (!Config->GetBindableStructByID(GetTargetPath().GetStructID(), TargetDescriptor)
				|| !Config->ResolveBindingEndpoint(SourcePath.GetStructID(), SourceEndpoint)
				|| !Config->ResolveBindingEndpoint(GetTargetPath().GetStructID(), TargetEndpoint))
			{
				return false;
			}
			const bool bOutput = TargetRole == EArcTacticsPropertyRole::Output;
			const FArcTacticsBinding Candidate(SourcePath, GetTargetPath(), bOutput);
			const EArcTacticsBindingValidationResult Result = Config->EditorBindings.ValidateEditorBinding(
				Candidate,
				SourceStruct,
				SourceEndpoint,
				TargetDescriptor.Get().Struct.Get(),
				TargetEndpoint,
				Config->IsBindingSourceAccessible(SourcePath.GetStructID(), GetTargetPath().GetStructID(), bOutput));
			return Result == EArcTacticsBindingValidationResult::Valid
				|| Result == EArcTacticsBindingValidationResult::IncompatibleTypes;
		}

		virtual bool CanBindToContextStructInternal(const UStruct*, const int32) override
		{
			return false;
		}

		virtual bool DeterminePropertiesCompatibilityInternal(
			const FProperty* InSourceProperty,
			const FProperty* InTargetProperty,
			const void* InSourcePropertyValue,
			const void* InTargetPropertyValue,
			bool& bOutAreCompatible) const override
		{
			if (TargetRole == EArcTacticsPropertyRole::Output)
			{
				bOutAreCompatible = UE::PropertyBinding::GetPropertyCompatibility(InTargetProperty, InSourceProperty)
					!= UE::PropertyBinding::EPropertyCompatibility::Incompatible;
				return true;
			}
			return false;
		}

		virtual bool AddBindingInternal(
			const TConstStructView<FPropertyBindingBindableStructDescriptor> InDescriptor,
			FPropertyBindingPath& InOutSourcePath,
			const FPropertyBindingPath& InTargetPath) override
		{
			UArcTacticsConfig* Config = Cast<UArcTacticsConfig>(GetOwner());
			if (!Config)
			{
				return true;
			}
			if (ArcPropertyFunctionEditor::GetFunctionStruct(InDescriptor.Get()))
			{
				return TargetRole == EArcTacticsPropertyRole::Output
					|| AddFunctionBinding(InDescriptor, InOutSourcePath, InTargetPath);
			}
			if (TargetRole != EArcTacticsPropertyRole::Output)
			{
				return false;
			}

			Config->EditorBindings.AddBinding(InOutSourcePath, InTargetPath);
			Config->EditorBindings.VisitMutableBindings(
				[&InOutSourcePath, &InTargetPath](FPropertyBindingBinding& Binding)
				{
					if (Binding.GetSourcePath() == InOutSourcePath
						&& Binding.GetTargetPath() == InTargetPath)
					{
						static_cast<FArcTacticsBinding&>(Binding).SetIsOutputBinding(true);
						return FPropertyBindingBindingCollection::EVisitResult::Break;
					}
					return FPropertyBindingBindingCollection::EVisitResult::Continue;
				});
			return true;
		}

	protected:
		virtual FPropertyBindingBinding& AddFunctionBindingToCollection(
			const FConstStructView FunctionNode,
			const FPropertyBindingPath& InSourcePath,
			const FPropertyBindingPath& InTargetPath) override
		{
			return CastChecked<UArcTacticsConfig>(GetOwner())->EditorBindings.AddCompiledBinding(
				FArcTacticsBinding(FunctionNode, InSourcePath, InTargetPath));
		}

	private:
		EArcTacticsPropertyRole TargetRole = EArcTacticsPropertyRole::Invalid;
	};
}

TSharedPtr<UE::PropertyBinding::FCachedBindingData> FArcTacticsBindingExtension::CreateCachedBindingData(
	IPropertyBindingBindingCollectionOwner* InBindingsOwner,
	const FPropertyBindingPath& InTargetPath,
	const TSharedPtr<IPropertyHandle>& InPropertyHandle,
	const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> InAccessibleStructs) const
{
	using namespace ArcTacticsBindingExtensionPrivate;
	const TConstArrayView<TInstancedStruct<FPropertyBindingBindableStructDescriptor>> Descriptors = InAccessibleStructs;

	UE_LOGF(
		LogArcTactics,
		Log,
		"[ArcTacticsPicker] Binding Create Target=%ls Sources=%d",
		*InTargetPath.ToString(),
		Descriptors.Num());
	for (int32 Index = 0; Index < Descriptors.Num(); ++Index)
	{
		const FPropertyBindingBindableStructDescriptor& Source = Descriptors[Index].Get();
		UE_LOGF(
			LogArcTactics,
			Log,
			"[ArcTacticsPicker] Binding Source Index=%d ID=%ls Name=%ls Struct=%ls",
			Index,
			*Source.ID.ToString(),
			*Source.Name.ToString(),
			Source.Struct ? *Source.Struct->GetPathName() : TEXT("<null>"));
	}
	return MakeShared<FCachedBindingData>(
		InBindingsOwner,
		InTargetPath,
		InPropertyHandle,
		Descriptors,
		ResolveRootRole(*InPropertyHandle));
}

bool FArcTacticsBindingExtension::CanBindToProperty(
	const FPropertyBindingPath& InTargetPath,
	const IPropertyHandle& InPropertyHandle) const
{
	for (TSharedPtr<const IPropertyHandle> Current = InPropertyHandle.AsShared(); Current.IsValid(); Current = Current->GetParentHandle())
	{
		const FProperty* Property = Current->GetProperty();
		if (Property && ArcTQS::IsLiteralOnlyProperty(*Property))
		{
			return false;
		}
	}
	const EArcTacticsPropertyRole Role = ArcTacticsBindingExtensionPrivate::ResolveRootRole(InPropertyHandle);
	return Role == EArcTacticsPropertyRole::Input
		|| Role == EArcTacticsPropertyRole::Parameter
		|| Role == EArcTacticsPropertyRole::Output;
}

bool FArcTacticsBindingExtension::GetPromotionToParameterOverrideInternal(
	const FProperty& InProperty,
	bool& bOutOverride) const
{
	bOutOverride = false;
	return true;
}

void FArcTacticsBindingExtension::UpdateContextStruct(
	const TConstStructView<FPropertyBindingBindableStructDescriptor> InStructDesc,
	FBindingContextStruct& InOutContextStruct,
	TMap<FString, FText>& InOutSectionNames) const
{
	if (ArcPropertyFunctionEditor::UpdateFunctionContextStruct(InStructDesc, InOutContextStruct, InOutSectionNames))
	{
		return;
	}
	const FString Section = InStructDesc.Get().GetSection();
	if (Section.IsEmpty())
	{
		return;
	}
	if (const FText* SectionText = InOutSectionNames.Find(Section))
	{
		InOutContextStruct.Section = *SectionText;
	}
	else
	{
		InOutContextStruct.Section = InOutSectionNames.Add(Section, FText::FromString(Section));
	}
}

void FArcTacticsBindingExtension::CustomizeDetailWidgetRow(
	FDetailWidgetRow& InWidgetRow,
	const IDetailLayoutBuilder& InDetailBuilder,
	IPropertyBindingBindingCollectionOwner* InBindingsOwner,
	TSharedPtr<IPropertyHandle> InPropertyHandle,
	const FPropertyBindingPath& InTargetPath,
	TSharedPtr<UE::PropertyBinding::FCachedBindingData> InCachedBindingData) const
{
	FPropertyBindingExtension::CustomizeDetailWidgetRow(
		InWidgetRow, InDetailBuilder, InBindingsOwner, InPropertyHandle, InTargetPath, InCachedBindingData);
	const TSharedPtr<ArcTacticsBindingExtensionPrivate::FCachedBindingData> Cached =
		StaticCastSharedPtr<ArcTacticsBindingExtensionPrivate::FCachedBindingData>(InCachedBindingData);
	if (!Cached.IsValid() || Cached->GetTargetRole() == EArcTacticsPropertyRole::Invalid)
	{
		return;
	}
	const EArcTacticsPropertyRole Role = Cached->GetTargetRole();
	const TSharedRef<SWidget> NameWidget = InWidgetRow.NameWidget.Widget;
	const TSharedRef<SHorizontalBox> NameBox = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.VAlign(VAlign_Center)
		[
			NameWidget
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FArcTacticsEditorStyle::Get().GetBrush(
				ArcTacticsBindingExtensionPrivate::GetRoleBrushName(Role)))
			.Padding(FMargin(5.0f, 1.0f))
			.ToolTipText(FArcTacticsNodePresentation::GetPropertyRoleTooltip(Role))
			[
				SNew(STextBlock)
				.Text(FArcTacticsNodePresentation::GetPropertyRoleLabel(Role))
				.ColorAndOpacity(FArcTacticsNodePresentation::GetPropertyRoleColor(Role))
				.TextStyle(&FArcTacticsEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(
					TEXT("ArcTactics.Role.Label")))
			]
		];
	const FProperty* RowProperty = InPropertyHandle.IsValid() ? InPropertyHandle->GetProperty() : nullptr;
	if (RowProperty
		&& InTargetPath.NumSegments() == 1
		&& InTargetPath.GetSegment(0).GetArrayIndex() == INDEX_NONE
		&& InTargetPath.GetSegment(0).GetName() == RowProperty->GetFName()
		&& ArcTacticsNode::ResolvePropertyRole(*RowProperty) == EArcTacticsPropertyRole::Input)
	{
		const bool bRequired = ArcTacticsNode::IsRequiredInput(*RowProperty);
		NameBox->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SBorder)
			.BorderImage(FArcTacticsEditorStyle::Get().GetBrush(bRequired
				? TEXT("ArcTactics.Requirement.Required")
				: TEXT("ArcTactics.Requirement.Optional")))
			.Padding(FMargin(5.0f, 1.0f))
			.ToolTipText(FArcTacticsNodePresentation::GetInputRequirementTooltip(bRequired))
			[
				SNew(STextBlock)
				.Text(FArcTacticsNodePresentation::GetInputRequirementLabel(bRequired))
				.ColorAndOpacity(FArcTacticsNodePresentation::GetInputRequirementColor(bRequired))
				.TextStyle(&FArcTacticsEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(
					TEXT("ArcTactics.Role.Label")))
			]
		];
	}
	InWidgetRow.NameContent()
	[
		NameBox
	];
}

void FArcTacticsBindingExtension::CustomizePropertyBindingWidgetArgs(
	FPropertyBindingWidgetArgs& Args,
	IPropertyBindingBindingCollectionOwner* InBindingsOwner,
	const FPropertyBindingPath& InTargetPath,
	TSharedPtr<UE::PropertyBinding::FCachedBindingData> InCachedBindingData)
{
	using namespace ArcTacticsBindingExtensionPrivate;
	const TSharedPtr<FCachedBindingData> Cached = StaticCastSharedPtr<FCachedBindingData>(InCachedBindingData);
	if (!Cached.IsValid())
	{
		return;
	}

	const FOnAddBinding OriginalAdd = Args.OnAddBinding;
	Args.OnAddBinding = FOnAddBinding::CreateLambda(
		[OriginalAdd, Cached](const FName PropertyName, const TArray<FBindingChainElement>& BindingChain)
		{
			const FProperty* Property = BindingChain.IsEmpty()
				? nullptr
				: BindingChain.Last().Field.Get<FProperty>();
			UE_LOGF(
				LogArcTactics,
				Log,
				"[ArcTacticsPicker] Binding Select Target=%ls Property=%ls Type=%ls Chain=%d",
				*Cached->GetTargetPath().ToString(),
				Property ? *Property->GetName() : TEXT("<root>"),
				Property ? *Property->GetCPPType() : TEXT("<struct>"),
				BindingChain.Num());
			OriginalAdd.ExecuteIfBound(PropertyName, BindingChain);
		});
}
