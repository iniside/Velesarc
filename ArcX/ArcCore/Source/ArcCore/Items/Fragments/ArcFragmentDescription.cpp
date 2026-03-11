/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
 * of the EUPL (the "License");
 *
 * You may not use this work except in compliance with the License.
 * You may get a copy of the License at:
 *
 * https://eupl.eu/
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 */

#include "ArcFragmentDescription.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UObject/UnrealType.h"
#include "UObject/Class.h"

TSharedPtr<FJsonObject> FArcFragmentPropertyDescription::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("name"), PropertyName.ToString());
	Obj->SetStringField(TEXT("type"), TypeName);
	if (!Description.IsEmpty())
	{
		Obj->SetStringField(TEXT("description"), Description);
	}
	if (!Category.IsEmpty())
	{
		Obj->SetStringField(TEXT("category"), Category);
	}
	return Obj;
}

TSharedPtr<FJsonObject> FArcFragmentDescription::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("struct_name"), StructName.ToString());
	Obj->SetStringField(TEXT("display_name"), DisplayName);

	if (!Description.IsEmpty())
	{
		Obj->SetStringField(TEXT("description"), Description);
	}
	if (!Category.IsEmpty())
	{
		Obj->SetStringField(TEXT("category"), Category);
	}

	// Properties
	TArray<TSharedPtr<FJsonValue>> PropsArray;
	for (const FArcFragmentPropertyDescription& Prop : Properties)
	{
		PropsArray.Add(MakeShared<FJsonValueObject>(Prop.ToJson()));
	}
	Obj->SetArrayField(TEXT("properties"), PropsArray);

	// CommonPairings
	if (CommonPairings.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> PairingsArray;
		for (const FName& Pairing : CommonPairings)
		{
			PairingsArray.Add(MakeShared<FJsonValueString>(Pairing.ToString()));
		}
		Obj->SetArrayField(TEXT("common_pairings"), PairingsArray);
	}

	// Prerequisites
	if (Prerequisites.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> PrereqArray;
		for (const FName& Prereq : Prerequisites)
		{
			PrereqArray.Add(MakeShared<FJsonValueString>(Prereq.ToString()));
		}
		Obj->SetArrayField(TEXT("prerequisites"), PrereqArray);
	}

	// UsageNotes
	if (!UsageNotes.IsEmpty())
	{
		Obj->SetStringField(TEXT("usage_notes"), UsageNotes);
	}

	return Obj;
}

FArcFragmentDescription FArcFragmentDescription::BuildBaseDescription(const UScriptStruct* InStruct)
{
	FArcFragmentDescription Desc;

	if (!InStruct)
	{
		return Desc;
	}

	Desc.StructName = InStruct->GetFName();

#if WITH_METADATA
	// Read USTRUCT metadata (editor-only — WITH_METADATA == WITH_EDITORONLY_DATA)
	const FString MetaDisplayName = InStruct->GetMetaData(TEXT("DisplayName"));
	Desc.DisplayName = MetaDisplayName.IsEmpty() ? InStruct->GetName() : MetaDisplayName;
	Desc.Description = InStruct->GetMetaData(TEXT("ToolTip"));
	Desc.Category = InStruct->GetMetaData(TEXT("Category"));

	// Auto-reflect properties
	for (TFieldIterator<FProperty> PropIt(InStruct, EFieldIteratorFlags::IncludeSuper); PropIt; ++PropIt)
	{
		const FProperty* Prop = *PropIt;

		// Only include editor-exposed or blueprint-visible properties
		if (!Prop->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible))
		{
			continue;
		}

		FArcFragmentPropertyDescription PropDesc;
		PropDesc.PropertyName = Prop->GetFName();

		FString ExtendedType;
		PropDesc.TypeName = Prop->GetCPPType(&ExtendedType);
		if (!ExtendedType.IsEmpty())
		{
			PropDesc.TypeName += ExtendedType;
		}

		PropDesc.Description = Prop->GetMetaData(TEXT("ToolTip"));
		PropDesc.Category = Prop->GetMetaData(TEXT("Category"));

		Desc.Properties.Add(MoveTemp(PropDesc));
	}
#else
	Desc.DisplayName = InStruct->GetName();
#endif

	return Desc;
}
