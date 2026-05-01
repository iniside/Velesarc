// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcImGuiStructEditorWidget.h"

#include "imgui.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UObject/TextProperty.h"
#include "Engine/AssetManager.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "GameplayTagContainer.h"

void FArcImGuiStructEditorWidget::SetBaseStruct(const UScriptStruct* InBaseStruct)
{
	BaseStruct = InBaseStruct;
	RebuildStructList();
	Reset();
}

bool FArcImGuiStructEditorWidget::HasValidInstance() const
{
	return EditedInstance.IsValid();
}

FConstStructView FArcImGuiStructEditorWidget::GetStructView() const
{
	return FConstStructView(EditedInstance);
}

void FArcImGuiStructEditorWidget::Reset()
{
	SelectedStruct = nullptr;
	EditedInstance.Reset();
	StringBuffers.Reset();
	CachedAssetLists.Reset();
	StructFilterBuf[0] = '\0';
	AssetFilterBuf[0] = '\0';
}

void FArcImGuiStructEditorWidget::RebuildStructList()
{
	CachedStructList.Reset();

	if (!BaseStruct)
	{
		return;
	}

	for (TObjectIterator<UScriptStruct> It; It; ++It)
	{
		const UScriptStruct* Struct = *It;
		if (Struct->IsChildOf(BaseStruct))
		{
			CachedStructList.Add(Struct);
		}
	}

	CachedStructList.Sort([](const UScriptStruct& A, const UScriptStruct& B)
	{
		return A.GetName() < B.GetName();
	});
}

void FArcImGuiStructEditorWidget::DrawStructPicker()
{
	FString ButtonLabel = SelectedStruct
		? SelectedStruct->GetName()
		: FString(TEXT("Select Struct..."));

	if (ImGui::Button(TCHAR_TO_ANSI(*ButtonLabel)))
	{
		ImGui::OpenPopup("StructPickerPopup");
		StructFilterBuf[0] = '\0';
	}

	if (ImGui::BeginPopup("StructPickerPopup"))
	{
		ImGui::InputText("Filter##StructFilter", StructFilterBuf, IM_ARRAYSIZE(StructFilterBuf));

		FString Filter = FString(ANSI_TO_TCHAR(StructFilterBuf)).ToLower();

		ImGui::BeginChild("StructList", ImVec2(350, 300), true);
		for (const UScriptStruct* Struct : CachedStructList)
		{
			FString StructName = Struct->GetName();
			if (!Filter.IsEmpty() && !StructName.ToLower().Contains(Filter))
			{
				continue;
			}

			if (ImGui::Selectable(TCHAR_TO_ANSI(*StructName), Struct == SelectedStruct))
			{
				SelectedStruct = Struct;
				EditedInstance.InitializeAs(SelectedStruct);
				StringBuffers.Reset();
				CachedAssetLists.Reset();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndChild();

		ImGui::EndPopup();
	}

	if (SelectedStruct)
	{
		ImGui::SameLine();
		if (ImGui::SmallButton("Reset"))
		{
			Reset();
		}
	}
}

bool FArcImGuiStructEditorWidget::DrawAssetPicker(const FObjectPropertyBase* ObjProp, void* ContainerPtr)
{
	bool bChanged = false;
	const UClass* RequiredClass = ObjProp->PropertyClass;
	const FString PropName = ObjProp->GetName();

	UObject* CurrentObj = ObjProp->GetObjectPropertyValue_InContainer(ContainerPtr);
	FString CurrentLabel = CurrentObj ? CurrentObj->GetName() : FString(TEXT("None"));

	FString ButtonLabel = FString::Printf(TEXT("%s: %s###%s"), *PropName, *CurrentLabel, *PropName);
	if (ImGui::Button(TCHAR_TO_ANSI(*ButtonLabel)))
	{
		ImGui::OpenPopup(TCHAR_TO_ANSI(*FString::Printf(TEXT("AssetPicker_%s"), *PropName)));

		// Refresh asset list for this class
		UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
		if (AssetManager)
		{
			TArray<FAssetData>& AssetList = CachedAssetLists.FindOrAdd(RequiredClass);
			AssetList.Reset();

			IAssetRegistry& AR = AssetManager->GetAssetRegistry();
			AR.GetAssetsByClass(RequiredClass->GetClassPathName(), AssetList, true);

			AssetList.Sort([](const FAssetData& A, const FAssetData& B)
			{
				return A.AssetName.LexicalLess(B.AssetName);
			});
		}
	}

	FString PopupId = FString::Printf(TEXT("AssetPicker_%s"), *PropName);
	if (ImGui::BeginPopup(TCHAR_TO_ANSI(*PopupId)))
	{
		ImGui::InputText("Filter##AssetFilter", AssetFilterBuf, IM_ARRAYSIZE(AssetFilterBuf));
		FString Filter = FString(ANSI_TO_TCHAR(AssetFilterBuf)).ToLower();

		// "None" option
		if (ImGui::Selectable("None", CurrentObj == nullptr))
		{
			ObjProp->SetObjectPropertyValue_InContainer(ContainerPtr, nullptr);
			bChanged = true;
			AssetFilterBuf[0] = '\0';
			ImGui::CloseCurrentPopup();
		}

		ImGui::Separator();

		ImGui::BeginChild("AssetList", ImVec2(400, 300), true);

		const TArray<FAssetData>* AssetList = CachedAssetLists.Find(RequiredClass);
		if (AssetList)
		{
			for (const FAssetData& Asset : *AssetList)
			{
				FString AssetName = Asset.AssetName.ToString();
				if (!Filter.IsEmpty() && !AssetName.ToLower().Contains(Filter))
				{
					continue;
				}

				bool bSelected = (CurrentObj && CurrentObj->GetPathName() == Asset.GetSoftObjectPath().ToString());
				if (ImGui::Selectable(TCHAR_TO_ANSI(*AssetName), bSelected))
				{
					UObject* LoadedAsset = Asset.GetAsset();
					if (LoadedAsset)
					{
						ObjProp->SetObjectPropertyValue_InContainer(ContainerPtr, LoadedAsset);
						bChanged = true;
					}
					AssetFilterBuf[0] = '\0';
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", TCHAR_TO_ANSI(*Asset.GetSoftObjectPath().ToString()));
				}
			}
		}

		ImGui::EndChild();
		ImGui::EndPopup();
	}

	return bChanged;
}

void FArcImGuiStructEditorWidget::DrawEditableProperty(const FProperty* Property, void* ContainerPtr)
{
	if (!Property || !ContainerPtr)
	{
		return;
	}

	const FString PropName = Property->GetName();
	const FTCHARToUTF8 PropNameUtf8(*PropName);
	const char* NameAnsi = PropNameUtf8.Get();

	// Bool
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value = BoolProp->GetPropertyValue_InContainer(ContainerPtr);
		if (ImGui::Checkbox(NameAnsi, &Value))
		{
			BoolProp->SetPropertyValue_InContainer(ContainerPtr, Value);
		}
		return;
	}

	// Numeric
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);

		if (NumProp->IsFloatingPoint())
		{
			double Value = NumProp->GetFloatingPointPropertyValue(ValuePtr);
			float FloatValue = static_cast<float>(Value);
			FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
			if (ImGui::InputFloat(TCHAR_TO_ANSI(*Label), &FloatValue, 0.1f, 1.0f, "%.4f"))
			{
				NumProp->SetFloatingPointPropertyValue(ValuePtr, static_cast<double>(FloatValue));
			}
		}
		else if (NumProp->IsInteger())
		{
			if (const UEnum* Enum = NumProp->GetIntPropertyEnum())
			{
				int64 Value = NumProp->GetSignedIntPropertyValue(ValuePtr);
				int32 CurrentIndex = Enum->GetIndexByValue(Value);

				FString ComboLabel = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
				FString CurrentName = Enum->GetNameStringByValue(Value);
				if (ImGui::BeginCombo(TCHAR_TO_ANSI(*ComboLabel), TCHAR_TO_ANSI(*CurrentName)))
				{
					int32 NumEnums = Enum->NumEnums() - 1;
					for (int32 i = 0; i < NumEnums; i++)
					{
						int64 EnumValue = Enum->GetValueByIndex(i);
						FString EnumName = Enum->GetNameStringByIndex(i);
						bool bSelected = (EnumValue == Value);
						if (ImGui::Selectable(TCHAR_TO_ANSI(*EnumName), bSelected))
						{
							NumProp->SetIntPropertyValue(ValuePtr, EnumValue);
						}
						if (bSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
			}
			else
			{
				int64 Value = NumProp->GetSignedIntPropertyValue(ValuePtr);
				int32 IntValue = static_cast<int32>(Value);
				FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
				if (ImGui::InputInt(TCHAR_TO_ANSI(*Label), &IntValue))
				{
					NumProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(IntValue));
				}
			}
		}
		return;
	}

	// Enum property (C++ enum class)
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		const FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		int64 Value = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);

		const UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			FString ComboLabel = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
			FString CurrentName = Enum->GetNameStringByValue(Value);
			if (ImGui::BeginCombo(TCHAR_TO_ANSI(*ComboLabel), TCHAR_TO_ANSI(*CurrentName)))
			{
				int32 NumEnums = Enum->NumEnums() - 1;
				for (int32 i = 0; i < NumEnums; i++)
				{
					int64 EnumValue = Enum->GetValueByIndex(i);
					FString EnumName = Enum->GetNameStringByIndex(i);
					bool bSelected = (EnumValue == Value);
					if (ImGui::Selectable(TCHAR_TO_ANSI(*EnumName), bSelected))
					{
						UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumValue);
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}
		}
		else
		{
			int32 IntValue = static_cast<int32>(Value);
			FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
			if (ImGui::InputInt(TCHAR_TO_ANSI(*Label), &IntValue))
			{
				UnderlyingProp->SetIntPropertyValue(ValuePtr, static_cast<int64>(IntValue));
			}
		}
		return;
	}

	// FString
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		TArray<char>& Buffer = StringBuffers.FindOrAdd(Property->GetFName());
		if (Buffer.Num() == 0)
		{
			Buffer.SetNumZeroed(512);
			const FString& Current = StrProp->GetPropertyValue_InContainer(ContainerPtr);
			FCStringAnsi::Strncpy(Buffer.GetData(), TCHAR_TO_ANSI(*Current), Buffer.Num());
		}
		FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
		if (ImGui::InputText(TCHAR_TO_ANSI(*Label), Buffer.GetData(), Buffer.Num()))
		{
			StrProp->SetPropertyValue_InContainer(ContainerPtr, FString(ANSI_TO_TCHAR(Buffer.GetData())));
		}
		return;
	}

	// FName
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		TArray<char>& Buffer = StringBuffers.FindOrAdd(Property->GetFName());
		if (Buffer.Num() == 0)
		{
			Buffer.SetNumZeroed(512);
			const FName& Current = NameProp->GetPropertyValue_InContainer(ContainerPtr);
			FCStringAnsi::Strncpy(Buffer.GetData(), TCHAR_TO_ANSI(*Current.ToString()), Buffer.Num());
		}
		FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
		if (ImGui::InputText(TCHAR_TO_ANSI(*Label), Buffer.GetData(), Buffer.Num()))
		{
			NameProp->SetPropertyValue_InContainer(ContainerPtr, FName(ANSI_TO_TCHAR(Buffer.GetData())));
		}
		return;
	}

	// FText
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		TArray<char>& Buffer = StringBuffers.FindOrAdd(Property->GetFName());
		if (Buffer.Num() == 0)
		{
			Buffer.SetNumZeroed(512);
			const FText& Current = TextProp->GetPropertyValue_InContainer(ContainerPtr);
			FCStringAnsi::Strncpy(Buffer.GetData(), TCHAR_TO_ANSI(*Current.ToString()), Buffer.Num());
		}
		FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
		if (ImGui::InputText(TCHAR_TO_ANSI(*Label), Buffer.GetData(), Buffer.Num()))
		{
			TextProp->SetPropertyValue_InContainer(ContainerPtr, FText::FromString(FString(ANSI_TO_TCHAR(Buffer.GetData()))));
		}
		return;
	}

	// Struct
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		void* StructData = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		const UScriptStruct* InnerStruct = StructProp->Struct;

		// FVector
		if (InnerStruct == TBaseStructure<FVector>::Get())
		{
			FVector& Vec = *static_cast<FVector*>(StructData);
			float Floats[3] = { static_cast<float>(Vec.X), static_cast<float>(Vec.Y), static_cast<float>(Vec.Z) };
			FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
			if (ImGui::InputFloat3(TCHAR_TO_ANSI(*Label), Floats))
			{
				Vec.X = Floats[0];
				Vec.Y = Floats[1];
				Vec.Z = Floats[2];
			}
			return;
		}

		// FRotator
		if (InnerStruct == TBaseStructure<FRotator>::Get())
		{
			FRotator& Rot = *static_cast<FRotator*>(StructData);
			float Floats[3] = { static_cast<float>(Rot.Pitch), static_cast<float>(Rot.Yaw), static_cast<float>(Rot.Roll) };
			FString Label = FString::Printf(TEXT("P/Y/R %s##%s"), *PropName, *PropName);
			if (ImGui::InputFloat3(TCHAR_TO_ANSI(*Label), Floats))
			{
				Rot.Pitch = Floats[0];
				Rot.Yaw = Floats[1];
				Rot.Roll = Floats[2];
			}
			return;
		}

		// FGameplayTag
		if (InnerStruct == FGameplayTag::StaticStruct())
		{
			FGameplayTag& Tag = *static_cast<FGameplayTag*>(StructData);
			TArray<char>& Buffer = StringBuffers.FindOrAdd(Property->GetFName());
			if (Buffer.Num() == 0)
			{
				Buffer.SetNumZeroed(512);
				if (Tag.IsValid())
				{
					FCStringAnsi::Strncpy(Buffer.GetData(), TCHAR_TO_ANSI(*Tag.ToString()), Buffer.Num());
				}
			}
			FString Label = FString::Printf(TEXT("%s##%s"), *PropName, *PropName);
			if (ImGui::InputText(TCHAR_TO_ANSI(*Label), Buffer.GetData(), Buffer.Num(), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				Tag = FGameplayTag::RequestGameplayTag(FName(ANSI_TO_TCHAR(Buffer.GetData())), false);
			}
			if (Tag.IsValid())
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(%s)", TCHAR_TO_ANSI(*Tag.ToString()));
			}
			return;
		}

		// Generic nested struct — recurse
		ImGui::PushID(NameAnsi);
		if (ImGui::TreeNode(NameAnsi))
		{
			ImGui::TextDisabled("%s", TCHAR_TO_ANSI(*InnerStruct->GetName()));
			DrawEditableStructProperties(InnerStruct, StructData);
			ImGui::TreePop();
		}
		ImGui::PopID();
		return;
	}

	// Array
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		void* ArrayPtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
		int32 Num = ArrayHelper.Num();

		ImGui::PushID(NameAnsi);
		FString ArrayLabel = FString::Printf(TEXT("%s [%d]##%s"), *PropName, Num, *PropName);
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*ArrayLabel)))
		{
			if (ImGui::SmallButton("+"))
			{
				ArrayHelper.AddValue();
			}
			ImGui::SameLine();
			if (Num > 0 && ImGui::SmallButton("-"))
			{
				ArrayHelper.RemoveValues(Num - 1, 1);
			}

			Num = ArrayHelper.Num();

			for (int32 i = 0; i < Num; i++)
			{
				ImGui::PushID(i);
				uint8* ElemPtr = ArrayHelper.GetRawPtr(i);

				if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					FString ElemLabel = FString::Printf(TEXT("[%d]"), i);
					if (ImGui::TreeNode(TCHAR_TO_ANSI(*ElemLabel)))
					{
						DrawEditableStructProperties(InnerStructProp->Struct, ElemPtr);
						ImGui::TreePop();
					}
				}
				else
				{
					FString ElemLabel = FString::Printf(TEXT("[%d]"), i);
					ImGui::Text("%s", TCHAR_TO_ANSI(*ElemLabel));
					ImGui::SameLine();

					if (const FNumericProperty* NumInner = CastField<FNumericProperty>(ArrayProp->Inner))
					{
						if (NumInner->IsFloatingPoint())
						{
							double Value = NumInner->GetFloatingPointPropertyValue(ElemPtr);
							float FloatValue = static_cast<float>(Value);
							if (ImGui::InputFloat("##val", &FloatValue, 0.1f, 1.0f, "%.4f"))
							{
								NumInner->SetFloatingPointPropertyValue(ElemPtr, static_cast<double>(FloatValue));
							}
						}
						else
						{
							int64 Value = NumInner->GetSignedIntPropertyValue(ElemPtr);
							int32 IntValue = static_cast<int32>(Value);
							if (ImGui::InputInt("##val", &IntValue))
							{
								NumInner->SetIntPropertyValue(ElemPtr, static_cast<int64>(IntValue));
							}
						}
					}
					else if (CastField<FStrProperty>(ArrayProp->Inner))
					{
						FString& Str = *reinterpret_cast<FString*>(ElemPtr);
						FName BufferKey(*FString::Printf(TEXT("arr_%s_%d"), *PropName, i));
						TArray<char>& Buffer = StringBuffers.FindOrAdd(BufferKey);
						if (Buffer.Num() == 0)
						{
							Buffer.SetNumZeroed(512);
							FCStringAnsi::Strncpy(Buffer.GetData(), TCHAR_TO_ANSI(*Str), Buffer.Num());
						}
						if (ImGui::InputText("##val", Buffer.GetData(), Buffer.Num()))
						{
							Str = FString(ANSI_TO_TCHAR(Buffer.GetData()));
						}
					}
					else if (CastField<FNameProperty>(ArrayProp->Inner))
					{
						FName& Name = *reinterpret_cast<FName*>(ElemPtr);
						FName BufferKey(*FString::Printf(TEXT("arr_%s_%d"), *PropName, i));
						TArray<char>& Buffer = StringBuffers.FindOrAdd(BufferKey);
						if (Buffer.Num() == 0)
						{
							Buffer.SetNumZeroed(512);
							FCStringAnsi::Strncpy(Buffer.GetData(), TCHAR_TO_ANSI(*Name.ToString()), Buffer.Num());
						}
						if (ImGui::InputText("##val", Buffer.GetData(), Buffer.Num()))
						{
							Name = FName(ANSI_TO_TCHAR(Buffer.GetData()));
						}
					}
					else if (const FBoolProperty* BoolInner = CastField<FBoolProperty>(ArrayProp->Inner))
					{
						bool Value = BoolInner->GetPropertyValue(ElemPtr);
						if (ImGui::Checkbox("##val", &Value))
						{
							BoolInner->SetPropertyValue(ElemPtr, Value);
						}
					}
					else
					{
						ImGui::TextDisabled("(%s)", TCHAR_TO_ANSI(*ArrayProp->Inner->GetCPPType()));
					}
				}

				ImGui::PopID();
			}

			ImGui::TreePop();
		}
		ImGui::PopID();
		return;
	}

	// Object reference (TObjectPtr)
	if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		DrawAssetPicker(ObjProp, ContainerPtr);
		return;
	}

	// Fallback for unsupported types
	ImGui::TextDisabled("%s: (%s)", NameAnsi, TCHAR_TO_ANSI(*Property->GetCPPType()));
}

void FArcImGuiStructEditorWidget::DrawEditableStructProperties(const UScriptStruct* Struct, void* Data)
{
	if (!Struct || !Data)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		FProperty* Property = *It;
		DrawEditableProperty(Property, Data);
	}
}

void FArcImGuiStructEditorWidget::DrawInline()
{
	DrawStructPicker();

	if (!EditedInstance.IsValid())
	{
		return;
	}

	ImGui::Separator();
	DrawEditableStructProperties(
		EditedInstance.GetScriptStruct(),
		EditedInstance.GetMutableMemory()
	);
}
