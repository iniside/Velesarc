// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcImGuiReflectionWidget.h"

#include "imgui.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "StructUtils/StructView.h"
#include "GameplayTagContainer.h"
#include "UObject/UnrealType.h"

namespace ArcImGuiReflection
{

void DrawStructView(const TCHAR* Label, FConstStructView StructView)
{
	const UScriptStruct* Struct = StructView.GetScriptStruct();
	if (!Struct)
	{
		ImGui::TextDisabled("%s: (null)", TCHAR_TO_ANSI(Label));
		return;
	}

	const void* Data = StructView.GetMemory();
	if (!Data)
	{
		ImGui::TextDisabled("%s: (null data)", TCHAR_TO_ANSI(Label));
		return;
	}

	ImGui::PushID(TCHAR_TO_ANSI(Label));
	if (ImGui::TreeNodeEx(TCHAR_TO_ANSI(Label), ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::TextDisabled("%s", TCHAR_TO_ANSI(*Struct->GetName()));
		DrawStructProperties(Struct, Data);
		ImGui::TreePop();
	}
	ImGui::PopID();
}

void DrawStructProperties(const UScriptStruct* Struct, const void* Data)
{
	if (!Struct || !Data)
	{
		return;
	}

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		const FProperty* Property = *It;
		DrawProperty(Property, Data);
	}
}

void DrawProperty(const FProperty* Property, const void* ContainerPtr)
{
	if (!Property || !ContainerPtr)
	{
		return;
	}

	const FString PropName = Property->GetName();
	const char* NameAnsi = TCHAR_TO_ANSI(*PropName);

	// Bool
	if (const FBoolProperty* BoolProp = CastField<FBoolProperty>(Property))
	{
		bool Value = BoolProp->GetPropertyValue_InContainer(ContainerPtr);
		ImGui::Text("%s: %s", NameAnsi, Value ? "true" : "false");
		return;
	}

	// Numeric (covers int8/16/32/64, uint8/16/32/64, float, double)
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(Property))
	{
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);

		if (NumProp->IsFloatingPoint())
		{
			double Value = NumProp->GetFloatingPointPropertyValue(ValuePtr);
			ImGui::Text("%s: %.4f", NameAnsi, Value);
		}
		else if (NumProp->IsInteger())
		{
			if (const UEnum* Enum = NumProp->GetIntPropertyEnum())
			{
				int64 Value = NumProp->GetSignedIntPropertyValue(ValuePtr);
				FString EnumName = Enum->GetNameStringByValue(Value);
				ImGui::Text("%s: %s (%lld)", NameAnsi, TCHAR_TO_ANSI(*EnumName), Value);
			}
			else
			{
				int64 Value = NumProp->GetSignedIntPropertyValue(ValuePtr);
				ImGui::Text("%s: %lld", NameAnsi, Value);
			}
		}
		return;
	}

	// Enum
	if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Property))
	{
		const FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		const void* ValuePtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		int64 Value = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);

		const UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			FString EnumName = Enum->GetNameStringByValue(Value);
			ImGui::Text("%s: %s", NameAnsi, TCHAR_TO_ANSI(*EnumName));
		}
		else
		{
			ImGui::Text("%s: %lld", NameAnsi, Value);
		}
		return;
	}

	// FString
	if (const FStrProperty* StrProp = CastField<FStrProperty>(Property))
	{
		const FString& Value = StrProp->GetPropertyValue_InContainer(ContainerPtr);
		ImGui::Text("%s: %s", NameAnsi, TCHAR_TO_ANSI(*Value));
		return;
	}

	// FName
	if (const FNameProperty* NameProp = CastField<FNameProperty>(Property))
	{
		const FName& Value = NameProp->GetPropertyValue_InContainer(ContainerPtr);
		ImGui::Text("%s: %s", NameAnsi, TCHAR_TO_ANSI(*Value.ToString()));
		return;
	}

	// FText
	if (const FTextProperty* TextProp = CastField<FTextProperty>(Property))
	{
		const FText& Value = TextProp->GetPropertyValue_InContainer(ContainerPtr);
		ImGui::Text("%s: %s", NameAnsi, TCHAR_TO_ANSI(*Value.ToString()));
		return;
	}

	// Struct
	if (const FStructProperty* StructProp = CastField<FStructProperty>(Property))
	{
		const void* StructData = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		const UScriptStruct* InnerStruct = StructProp->Struct;

		// FVector
		if (InnerStruct == TBaseStructure<FVector>::Get())
		{
			const FVector& Vec = *static_cast<const FVector*>(StructData);
			ImGui::Text("%s: (%.2f, %.2f, %.2f)", NameAnsi, Vec.X, Vec.Y, Vec.Z);
			return;
		}

		// FRotator
		if (InnerStruct == TBaseStructure<FRotator>::Get())
		{
			const FRotator& Rot = *static_cast<const FRotator*>(StructData);
			ImGui::Text("%s: (P=%.2f, Y=%.2f, R=%.2f)", NameAnsi, Rot.Pitch, Rot.Yaw, Rot.Roll);
			return;
		}

		// FGameplayTag
		if (InnerStruct == FGameplayTag::StaticStruct())
		{
			const FGameplayTag& Tag = *static_cast<const FGameplayTag*>(StructData);
			ImGui::Text("%s: %s", NameAnsi, Tag.IsValid() ? TCHAR_TO_ANSI(*Tag.ToString()) : "(None)");
			return;
		}

		// FGameplayTagContainer
		if (InnerStruct == FGameplayTagContainer::StaticStruct())
		{
			const FGameplayTagContainer& Tags = *static_cast<const FGameplayTagContainer*>(StructData);
			ImGui::Text("%s: %s", NameAnsi, TCHAR_TO_ANSI(*Tags.ToStringSimple()));
			return;
		}

		// FInstancedStruct — show contained struct instead of internals
		if (InnerStruct == FInstancedStruct::StaticStruct())
		{
			const FInstancedStruct& Instance = *static_cast<const FInstancedStruct*>(StructData);
			if (Instance.IsValid())
			{
				ImGui::PushID(NameAnsi);
				if (ImGui::TreeNode(NameAnsi))
				{
					ImGui::TextDisabled("%s", TCHAR_TO_ANSI(*Instance.GetScriptStruct()->GetName()));
					DrawStructProperties(Instance.GetScriptStruct(), Instance.GetMemory());
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			else
			{
				ImGui::TextDisabled("%s: (empty)", NameAnsi);
			}
			return;
		}

		// FInstancedPropertyBag — show dynamic property bag contents
		if (InnerStruct == FInstancedPropertyBag::StaticStruct())
		{
			const FInstancedPropertyBag& Bag = *static_cast<const FInstancedPropertyBag*>(StructData);
			if (Bag.IsValid())
			{
				FConstStructView BagView = Bag.GetValue();
				ImGui::PushID(NameAnsi);
				if (ImGui::TreeNode(NameAnsi))
				{
					ImGui::TextDisabled("PropertyBag (%s)", TCHAR_TO_ANSI(*BagView.GetScriptStruct()->GetName()));
					DrawStructProperties(BagView.GetScriptStruct(), BagView.GetMemory());
					ImGui::TreePop();
				}
				ImGui::PopID();
			}
			else
			{
				ImGui::TextDisabled("%s: (empty)", NameAnsi);
			}
			return;
		}

		// Generic nested struct - recurse
		ImGui::PushID(NameAnsi);
		if (ImGui::TreeNode(NameAnsi))
		{
			ImGui::TextDisabled("%s", TCHAR_TO_ANSI(*InnerStruct->GetName()));
			DrawStructProperties(InnerStruct, StructData);
			ImGui::TreePop();
		}
		ImGui::PopID();
		return;
	}

	// Object reference
	if (const FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Property))
	{
		const UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(ContainerPtr);
		if (Obj)
		{
			ImGui::Text("%s: %s", NameAnsi, TCHAR_TO_ANSI(*Obj->GetName()));
		}
		else
		{
			ImGui::Text("%s: null", NameAnsi);
		}
		return;
	}

	// Array
	if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
	{
		const void* ArrayPtr = Property->ContainerPtrToValuePtr<void>(ContainerPtr);
		FScriptArrayHelper ArrayHelper(ArrayProp, ArrayPtr);
		const int32 Num = ArrayHelper.Num();

		ImGui::PushID(NameAnsi);
		FString ArrayLabel = FString::Printf(TEXT("%s [%d]"), *PropName, Num);
		if (ImGui::TreeNode(TCHAR_TO_ANSI(*ArrayLabel)))
		{
			for (int32 i = 0; i < Num; i++)
			{
				const uint8* ElemPtr = ArrayHelper.GetRawPtr(i);
				ImGui::PushID(i);

				if (const FStructProperty* InnerStructProp = CastField<FStructProperty>(ArrayProp->Inner))
				{
					if (InnerStructProp->Struct == FInstancedStruct::StaticStruct())
					{
						const FInstancedStruct& Instance = *reinterpret_cast<const FInstancedStruct*>(ElemPtr);
						if (Instance.IsValid())
						{
							FString ElemLabel = FString::Printf(TEXT("[%d] %s"), i, *Instance.GetScriptStruct()->GetName());
							if (ImGui::TreeNode(TCHAR_TO_ANSI(*ElemLabel)))
							{
								DrawStructProperties(Instance.GetScriptStruct(), Instance.GetMemory());
								ImGui::TreePop();
							}
						}
						else
						{
							ImGui::TextDisabled("[%d]: (empty)", i);
						}
					}
					else if (InnerStructProp->Struct == FInstancedPropertyBag::StaticStruct())
					{
						const FInstancedPropertyBag& Bag = *reinterpret_cast<const FInstancedPropertyBag*>(ElemPtr);
						if (Bag.IsValid())
						{
							FConstStructView BagView = Bag.GetValue();
							FString ElemLabel = FString::Printf(TEXT("[%d] PropertyBag"), i);
							if (ImGui::TreeNode(TCHAR_TO_ANSI(*ElemLabel)))
							{
								DrawStructProperties(BagView.GetScriptStruct(), BagView.GetMemory());
								ImGui::TreePop();
							}
						}
						else
						{
							ImGui::TextDisabled("[%d]: (empty)", i);
						}
					}
					else
					{
						FString ElemLabel = FString::Printf(TEXT("[%d]"), i);
						if (ImGui::TreeNode(TCHAR_TO_ANSI(*ElemLabel)))
						{
							DrawStructProperties(InnerStructProp->Struct, ElemPtr);
							ImGui::TreePop();
						}
					}
				}
				else if (const FNumericProperty* NumProp = CastField<FNumericProperty>(ArrayProp->Inner))
				{
					if (NumProp->IsFloatingPoint())
					{
						double Val = NumProp->GetFloatingPointPropertyValue(ElemPtr);
						ImGui::Text("[%d]: %.4f", i, Val);
					}
					else
					{
						int64 Val = NumProp->GetSignedIntPropertyValue(ElemPtr);
						ImGui::Text("[%d]: %lld", i, Val);
					}
				}
				else if (CastField<FStrProperty>(ArrayProp->Inner))
				{
					const FString& Val = *reinterpret_cast<const FString*>(ElemPtr);
					ImGui::Text("[%d]: %s", i, TCHAR_TO_ANSI(*Val));
				}
				else if (CastField<FNameProperty>(ArrayProp->Inner))
				{
					const FName& Val = *reinterpret_cast<const FName*>(ElemPtr);
					ImGui::Text("[%d]: %s", i, TCHAR_TO_ANSI(*Val.ToString()));
				}
				else
				{
					ImGui::TextDisabled("[%d]: (%s)", i, TCHAR_TO_ANSI(*ArrayProp->Inner->GetCPPType()));
				}

				ImGui::PopID();
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
		return;
	}

	// Fallback for unsupported types
	ImGui::TextDisabled("%s: (%s)", NameAnsi, TCHAR_TO_ANSI(*Property->GetCPPType()));
}

} // namespace ArcImGuiReflection
