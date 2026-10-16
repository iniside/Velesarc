// Copyright Lukasz Baran. All Rights Reserved.

#include "Customizations/ArcItemRecordCustomizableObject.h"

#include "Equipment/Fragments/ArcItemFragment_CustomizableObject.h"
#include "Items/ArcItemDefinitionData.h"
#include "MuCO/CustomizableObject.h"
#include "PropertyHandle.h"
#include "UObject/StructOnScope.h"

namespace ArcItemRecordCustomizableObject
{
	namespace
	{
		const FArcItemDefinitionData* FindOwningItem(const TSharedPtr<IPropertyHandle>& Handle)
		{
			TSharedPtr<IPropertyHandle> Current = Handle;
			while (Current.IsValid())
			{
				TArray<TSharedPtr<FStructOnScope>> Structs;
				Current->GetOuterStructs(Structs);
				for (const TSharedPtr<FStructOnScope>& Struct : Structs)
				{
					if (Struct.IsValid() && Struct->GetStruct() != nullptr && Struct->GetStruct()->IsChildOf(FArcItemDefinitionData::StaticStruct()))
					{
						return reinterpret_cast<const FArcItemDefinitionData*>(Struct->GetStructMemory());
					}
				}
				Current = Current->GetParentHandle();
			}
			return nullptr;
		}
	}

	UCustomizableObject* Find(const TSharedPtr<IPropertyHandle>& Handle)
	{
		const FArcItemDefinitionData* Item = FindOwningItem(Handle);
		if (Item == nullptr)
		{
			return nullptr;
		}

		const FArcItemFragment_CustomizableObject* Frag = Item->FindFragment<FArcItemFragment_CustomizableObject>();
		if (Frag == nullptr || Frag->CustomizableObject.IsNull())
		{
			return nullptr;
		}

		UCustomizableObject* CO = Frag->CustomizableObject.LoadSynchronous();
		if (CO != nullptr && !CO->IsCompiled())
		{
			FCompileParams CompileParams;
			CompileParams.bSkipIfCompiled = true;
			CompileParams.bAsync = false;
			CO->Compile(CompileParams);
		}
		return CO;
	}
}
