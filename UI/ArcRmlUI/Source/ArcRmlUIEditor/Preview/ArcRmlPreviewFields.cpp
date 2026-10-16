// Copyright Lukasz Baran. All Rights Reserved.

#include "Preview/ArcRmlPreviewFields.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameplayTagContainer.h"
#include "INotifyFieldValueChanged.h"
#include "MVVMViewModelBase.h"
#include "SGameplayTagCombo.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ArcRmlPreviewFields"

namespace ArcRmlPreviewFieldsPrivate
{
	using FNotifyFieldId = UE::FieldNotification::FFieldId;
	using FNotifyFieldVariant = UE::FieldNotification::FFieldVariant;

	class FValueBuffer
	{
	public:
		UE_NONCOPYABLE(FValueBuffer);

		explicit FValueBuffer(const FProperty& InProperty)
			: Property(InProperty)
			, Memory(FMemory::Malloc(InProperty.GetSize(), InProperty.GetMinAlignment()))
		{
			Property.InitializeValue(Memory);
		}

		~FValueBuffer()
		{
			Property.DestroyValue(Memory);
			FMemory::Free(Memory);
		}

		void* Get() const
		{
			return Memory;
		}

	private:
		const FProperty& Property;
		void* Memory = nullptr;
	};

	enum class EFieldEditor : uint8
	{
		ReadOnly,
		Bool,
		Integer,
		Float,
		String,
		Name,
		Text,
		Enum,
		GameplayTag,
		LinearColor,
		Color
	};

	const FProperty* FindFieldProperty(const UObject& Object, FName FieldName)
	{
		return Object.GetClass()->FindPropertyByName(FieldName);
	}

	const FNumericProperty* GetEnumNumeric(const FProperty& Property, UEnum*& OutEnum)
	{
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(&Property))
		{
			OutEnum = EnumProperty->GetEnum();
			return EnumProperty->GetUnderlyingProperty();
		}
		const FNumericProperty* Numeric = CastField<FNumericProperty>(&Property);
		if (Numeric && Numeric->IsEnum())
		{
			OutEnum = Numeric->GetIntPropertyEnum();
			return Numeric;
		}
		OutEnum = nullptr;
		return nullptr;
	}

	EFieldEditor ClassifyProperty(const FProperty& Property)
	{
		UEnum* Enum = nullptr;
		if (GetEnumNumeric(Property, Enum) && Enum)
		{
			return EFieldEditor::Enum;
		}
		if (Property.IsA<FBoolProperty>())
		{
			return EFieldEditor::Bool;
		}
		if (const FNumericProperty* Numeric = CastField<FNumericProperty>(&Property))
		{
			return Numeric->IsFloatingPoint() ? EFieldEditor::Float : EFieldEditor::Integer;
		}
		if (Property.IsA<FStrProperty>())
		{
			return EFieldEditor::String;
		}
		if (Property.IsA<FNameProperty>())
		{
			return EFieldEditor::Name;
		}
		if (Property.IsA<FTextProperty>())
		{
			return EFieldEditor::Text;
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			if (StructProperty->Struct == FGameplayTag::StaticStruct())
			{
				return EFieldEditor::GameplayTag;
			}
			if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				return EFieldEditor::LinearColor;
			}
			if (StructProperty->Struct == TBaseStructure<FColor>::Get())
			{
				return EFieldEditor::Color;
			}
		}
		return EFieldEditor::ReadOnly;
	}

	void ReadValue(const TWeakObjectPtr<UObject>& WeakObject, FName FieldName, TFunctionRef<void(const FProperty&, const void*)> Visitor)
	{
		const UObject* Object = WeakObject.Get();
		const FProperty* Property = Object ? FindFieldProperty(*Object, FieldName) : nullptr;
		if (!Property)
		{
			return;
		}
		const FValueBuffer Value(*Property);
		Property->GetValue_InContainer(Object, Value.Get());
		Visitor(*Property, Value.Get());
	}

	UFunction* FindSetterFunction(const UClass& Class, const FProperty& Property, FName FieldName)
	{
		UFunction* Function = Class.FindFunctionByName(FName(*(TEXT("Set") + FieldName.ToString())));
		if (!Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintCallable))
		{
			return nullptr;
		}

		const FProperty* Parameter = nullptr;
		int32 ParameterCount = 0;
		for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
		{
			Parameter = *It;
			++ParameterCount;
		}
		if (ParameterCount != 1 || Parameter->HasAnyPropertyFlags(CPF_ReturnParm)
			|| (Parameter->HasAnyPropertyFlags(CPF_OutParm) && !Parameter->HasAnyPropertyFlags(CPF_ConstParm)) || !Parameter->SameType(&Property))
		{
			return nullptr;
		}
		return Function;
	}

	void ApplyValue(UObject& Object, const FProperty& Property, FName FieldName, const void* NewValue)
	{
		if (Property.HasSetter())
		{
			Property.CallSetter(&Object, NewValue);
			return;
		}

		if (UFunction* Function = FindSetterFunction(*Object.GetClass(), Property, FieldName))
		{
			uint8* Parameters = static_cast<uint8*>(FMemory_Alloca_Aligned(Function->ParmsSize, Function->GetMinAlignment()));
			FMemory::Memzero(Parameters, Function->ParmsSize);
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->InitializeValue_InContainer(Parameters);
				It->CopyCompleteValue(It->ContainerPtrToValuePtr<void>(Parameters), NewValue);
			}
			Object.ProcessEvent(Function, Parameters);
			for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Parameters);
			}
			return;
		}

		Property.CopyCompleteValue(Property.ContainerPtrToValuePtr<void>(&Object), NewValue);
		UMVVMViewModelBase* ViewModel = Cast<UMVVMViewModelBase>(&Object);
		const INotifyFieldValueChanged* Notify = Cast<INotifyFieldValueChanged>(&Object);
		if (ViewModel && Notify)
		{
			const FNotifyFieldId FieldId = Notify->GetFieldNotificationDescriptor().GetField(Object.GetClass(), FieldName);
			if (FieldId.IsValid())
			{
				ViewModel->BroadcastFieldValueChanged(FieldId);
			}
		}
	}

	void WriteValue(const TWeakObjectPtr<UObject>& WeakObject, FName FieldName, TFunctionRef<void(const FProperty&, void*)> Mutator)
	{
		UObject* Object = WeakObject.Get();
		const FProperty* Property = Object ? FindFieldProperty(*Object, FieldName) : nullptr;
		if (!Property)
		{
			return;
		}
		const FValueBuffer Value(*Property);
		Property->GetValue_InContainer(Object, Value.Get());
		Mutator(*Property, Value.Get());
		ApplyValue(*Object, *Property, FieldName, Value.Get());
	}

	FLinearColor ReadColor(const TWeakObjectPtr<UObject>& WeakObject, FName FieldName, bool bSRGB)
	{
		FLinearColor Result = FLinearColor::Black;
		ReadValue(WeakObject, FieldName, [&Result, bSRGB](const FProperty&, const void* Value)
		{
			Result = bSRGB ? FLinearColor(*static_cast<const FColor*>(Value)) : *static_cast<const FLinearColor*>(Value);
		});
		return Result;
	}

	void WriteColor(const TWeakObjectPtr<UObject>& WeakObject, FName FieldName, bool bSRGB, const FLinearColor& NewColor)
	{
		WriteValue(WeakObject, FieldName, [bSRGB, &NewColor](const FProperty&, void* Value)
		{
			if (bSRGB)
			{
				*static_cast<FColor*>(Value) = NewColor.ToFColor(true);
			}
			else
			{
				*static_cast<FLinearColor*>(Value) = NewColor;
			}
		});
	}

	TSharedRef<SWidget> MakeEnumMenu(const TWeakObjectPtr<UObject>& WeakObject, FName FieldName, UEnum& Enum)
	{
		FMenuBuilder MenuBuilder(true, nullptr);
		const int32 Count = Enum.ContainsExistingMax() ? Enum.NumEnums() - 1 : Enum.NumEnums();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Enum.HasMetaData(TEXT("Hidden"), Index))
			{
				continue;
			}
			const int64 EnumValue = Enum.GetValueByIndex(Index);
			MenuBuilder.AddMenuEntry(Enum.GetDisplayNameTextByIndex(Index), FText::GetEmpty(), FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([WeakObject, FieldName, EnumValue]()
				{
					WriteValue(WeakObject, FieldName, [EnumValue](const FProperty& Property, void* Value)
					{
						UEnum* PropertyEnum = nullptr;
						if (const FNumericProperty* Numeric = GetEnumNumeric(Property, PropertyEnum))
						{
							Numeric->SetIntPropertyValue(Value, EnumValue);
						}
					});
				})));
		}
		return MenuBuilder.MakeWidget();
	}

	TSharedRef<SWidget> MakeValueWidget(const TWeakObjectPtr<UObject>& WeakObject, FName FieldName, EFieldEditor Editor, UEnum* Enum)
	{
		switch (Editor)
		{
		case EFieldEditor::Bool:
			return SNew(SCheckBox)
				.IsChecked_Lambda([WeakObject, FieldName]() -> ECheckBoxState
				{
					bool bValue = false;
					ReadValue(WeakObject, FieldName, [&bValue](const FProperty& Property, const void* Value)
					{
						bValue = CastFieldChecked<FBoolProperty>(&Property)->GetPropertyValue(Value);
					});
					return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([WeakObject, FieldName](ECheckBoxState State)
				{
					WriteValue(WeakObject, FieldName, [State](const FProperty& Property, void* Value)
					{
						CastFieldChecked<FBoolProperty>(&Property)->SetPropertyValue(Value, State == ECheckBoxState::Checked);
					});
				});

		case EFieldEditor::Integer:
		{
			const TFunction<void(int64)> Apply = [WeakObject, FieldName](int64 NewValue)
			{
				WriteValue(WeakObject, FieldName, [NewValue](const FProperty& Property, void* Value)
				{
					CastFieldChecked<FNumericProperty>(&Property)->SetIntPropertyValue(Value, NewValue);
				});
			};
			return SNew(SSpinBox<int64>)
				.Delta(1)
				.Value_Lambda([WeakObject, FieldName]() -> int64
				{
					int64 Result = 0;
					ReadValue(WeakObject, FieldName, [&Result](const FProperty& Property, const void* Value)
					{
						Result = CastFieldChecked<FNumericProperty>(&Property)->GetSignedIntPropertyValue(Value);
					});
					return Result;
				})
				.OnValueChanged_Lambda([Apply](int64 NewValue)
				{
					Apply(NewValue);
				})
				.OnValueCommitted_Lambda([Apply](int64 NewValue, ETextCommit::Type)
				{
					Apply(NewValue);
				});
		}

		case EFieldEditor::Float:
		{
			const TFunction<void(double)> Apply = [WeakObject, FieldName](double NewValue)
			{
				WriteValue(WeakObject, FieldName, [NewValue](const FProperty& Property, void* Value)
				{
					CastFieldChecked<FNumericProperty>(&Property)->SetFloatingPointPropertyValue(Value, NewValue);
				});
			};
			return SNew(SSpinBox<double>)
				.Value_Lambda([WeakObject, FieldName]() -> double
				{
					double Result = 0.0;
					ReadValue(WeakObject, FieldName, [&Result](const FProperty& Property, const void* Value)
					{
						Result = CastFieldChecked<FNumericProperty>(&Property)->GetFloatingPointPropertyValue(Value);
					});
					return Result;
				})
				.OnValueChanged_Lambda([Apply](double NewValue)
				{
					Apply(NewValue);
				})
				.OnValueCommitted_Lambda([Apply](double NewValue, ETextCommit::Type)
				{
					Apply(NewValue);
				});
		}

		case EFieldEditor::String:
		case EFieldEditor::Name:
		case EFieldEditor::Text:
			return SNew(SEditableTextBox)
				.Text_Lambda([WeakObject, FieldName, Editor]() -> FText
				{
					FText Result;
					ReadValue(WeakObject, FieldName, [&Result, Editor](const FProperty&, const void* Value)
					{
						Result = Editor == EFieldEditor::String ? FText::FromString(*static_cast<const FString*>(Value))
							: Editor == EFieldEditor::Name ? FText::FromName(*static_cast<const FName*>(Value))
							: *static_cast<const FText*>(Value);
					});
					return Result;
				})
				.OnTextCommitted_Lambda([WeakObject, FieldName, Editor](const FText& NewText, ETextCommit::Type)
				{
					WriteValue(WeakObject, FieldName, [&NewText, Editor](const FProperty&, void* Value)
					{
						if (Editor == EFieldEditor::String)
						{
							*static_cast<FString*>(Value) = NewText.ToString();
						}
						else if (Editor == EFieldEditor::Name)
						{
							*static_cast<FName*>(Value) = FName(*NewText.ToString());
						}
						else
						{
							*static_cast<FText*>(Value) = NewText;
						}
					});
				});

		case EFieldEditor::Enum:
		{
			const TWeakObjectPtr<UEnum> WeakEnum = Enum;
			return SNew(SComboButton)
				.OnGetMenuContent_Lambda([WeakObject, FieldName, WeakEnum]() -> TSharedRef<SWidget>
				{
					UEnum* MenuEnum = WeakEnum.Get();
					return MenuEnum ? MakeEnumMenu(WeakObject, FieldName, *MenuEnum) : SNullWidget::NullWidget;
				})
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text_Lambda([WeakObject, FieldName, WeakEnum]() -> FText
					{
						FText Result;
						ReadValue(WeakObject, FieldName, [&Result, &WeakEnum](const FProperty& Property, const void* Value)
						{
							UEnum* PropertyEnum = nullptr;
							const FNumericProperty* Numeric = GetEnumNumeric(Property, PropertyEnum);
							if (Numeric && PropertyEnum && PropertyEnum == WeakEnum.Get())
							{
								Result = PropertyEnum->GetDisplayNameTextByValue(Numeric->GetSignedIntPropertyValue(Value));
							}
						});
						return Result;
					})
				];
		}

		case EFieldEditor::GameplayTag:
			return SNew(SGameplayTagCombo)
				.Tag_Lambda([WeakObject, FieldName]() -> FGameplayTag
				{
					FGameplayTag Result;
					ReadValue(WeakObject, FieldName, [&Result](const FProperty&, const void* Value)
					{
						Result = *static_cast<const FGameplayTag*>(Value);
					});
					return Result;
				})
				.OnTagChanged_Lambda([WeakObject, FieldName](const FGameplayTag NewTag)
				{
					WriteValue(WeakObject, FieldName, [&NewTag](const FProperty&, void* Value)
					{
						*static_cast<FGameplayTag*>(Value) = NewTag;
					});
				});

		case EFieldEditor::LinearColor:
		case EFieldEditor::Color:
		{
			const bool bSRGB = Editor == EFieldEditor::Color;
			return SNew(SColorBlock)
				.Color_Lambda([WeakObject, FieldName, bSRGB]() -> FLinearColor
				{
					return ReadColor(WeakObject, FieldName, bSRGB);
				})
				.ShowBackgroundForAlpha(true)
				.Size(FVector2D(64.0, 16.0))
				.OnMouseButtonDown_Lambda([WeakObject, FieldName, bSRGB](const FGeometry&, const FPointerEvent& MouseEvent) -> FReply
				{
					if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
					{
						return FReply::Unhandled();
					}
					FColorPickerArgs PickerArgs(ReadColor(WeakObject, FieldName, bSRGB),
						FOnLinearColorValueChanged::CreateLambda([WeakObject, FieldName, bSRGB](FLinearColor NewColor)
						{
							WriteColor(WeakObject, FieldName, bSRGB, NewColor);
						}));
					PickerArgs.bUseAlpha = true;
					OpenColorPicker(PickerArgs);
					return FReply::Handled();
				});
		}

		case EFieldEditor::ReadOnly:
		default:
			return SNew(STextBlock).Text(LOCTEXT("ReadOnly", "read-only"));
		}
	}

	FText MakeFieldType(const FNotifyFieldVariant& Field)
	{
		if (Field.IsProperty())
		{
			return FText::FromString(Field.GetProperty()->GetCPPType());
		}
		if (Field.IsFunction())
		{
			const FProperty* ReturnProperty = Field.GetFunction()->GetReturnProperty();
			return FText::Format(LOCTEXT("FunctionFieldType", "{0} (function)"),
				FText::FromString(ReturnProperty ? ReturnProperty->GetCPPType() : FString(TEXT("void"))));
		}
		return FText::GetEmpty();
	}
}

TSharedRef<SWidget> ArcRmlPreviewFields::MakeFieldList(UObject& ViewModel)
{
	using namespace ArcRmlPreviewFieldsPrivate;

	const TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	const INotifyFieldValueChanged* Notify = Cast<INotifyFieldValueChanged>(&ViewModel);
	if (!Notify)
	{
		return List;
	}

	const TWeakObjectPtr<UObject> WeakObject = &ViewModel;
	Notify->GetFieldNotificationDescriptor().ForEachField(ViewModel.GetClass(), [&List, &ViewModel, &WeakObject](FNotifyFieldId FieldId) -> bool
	{
		const FNotifyFieldVariant Field = FieldId.ToVariant(&ViewModel);
		const FName FieldName = FieldId.GetName();
		EFieldEditor Editor = EFieldEditor::ReadOnly;
		UEnum* Enum = nullptr;
		if (Field.IsProperty())
		{
			Editor = ClassifyProperty(*Field.GetProperty());
			GetEnumNumeric(*Field.GetProperty(), Enum);
		}

		List->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 1.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(0.35f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromName(FieldName))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.25f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(MakeFieldType(Field))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				.VAlign(VAlign_Center)
				[
					MakeValueWidget(WeakObject, FieldName, Editor, Enum)
				]
			];
		return true;
	});
	return List;
}

#undef LOCTEXT_NAMESPACE
