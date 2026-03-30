/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2026 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or -
 * as soon as they will be approved by the European Commission - later versions
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

#include "Serialization/ArcReflectionSerializer.h"
#include "Serialization/ArcSaveArchive.h"
#include "Serialization/ArcLoadArchive.h"

#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/SoftObjectPtr.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "Serialization/ArcSerializerRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogArcReflectionSerializer, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Save
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::Save(const UStruct* Type, const void* Data, FArcSaveArchive& Ar)
{
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_SaveGame))
		{
			continue;
		}
		SaveProperty(Prop, Data, Ar);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveProperty — extracts value pointer from container, then delegates
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveProperty(FProperty* Prop, const void* ContainerData, FArcSaveArchive& Ar)
{
	const FName Key = Prop->GetFName();
	const void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerData);
	SaveValueDirect(Prop, Key, ValuePtr, Ar);
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveValueDirect — dispatch by FProperty subtype given a raw value pointer
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveValueDirect(FProperty* Prop, FName Key, const void* ValuePtr, FArcSaveArchive& Ar)
{
	// ── Bool ────────────────────────────────────────────────────────────

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		Ar.WriteProperty(Key, BoolProp->GetPropertyValue(ValuePtr));
		return;
	}

	// ── Integer types ───────────────────────────────────────────────────

	if (CastField<FIntProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const int32*>(ValuePtr));
		return;
	}

	if (CastField<FInt64Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const int64*>(ValuePtr));
		return;
	}

	if (CastField<FByteProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint8*>(ValuePtr));
		return;
	}

	if (CastField<FUInt16Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint16*>(ValuePtr));
		return;
	}

	if (CastField<FUInt32Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint32*>(ValuePtr));
		return;
	}

	if (CastField<FUInt64Property>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const uint64*>(ValuePtr));
		return;
	}

	// ── Floating point ──────────────────────────────────────────────────

	if (CastField<FFloatProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const float*>(ValuePtr));
		return;
	}

	if (CastField<FDoubleProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const double*>(ValuePtr));
		return;
	}

	// ── String / Name / Text ────────────────────────────────────────────

	if (CastField<FStrProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const FString*>(ValuePtr));
		return;
	}

	if (CastField<FNameProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const FName*>(ValuePtr));
		return;
	}

	if (CastField<FTextProperty>(Prop))
	{
		Ar.WriteProperty(Key, *static_cast<const FText*>(ValuePtr));
		return;
	}

	// ── Enum ────────────────────────────────────────────────────────────

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		const int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		Ar.WriteProperty(Key, EnumValue);

		// Write the enum value name as a debug string alongside the numeric value
		const FName EnumNameKey(*FString::Printf(TEXT("%s_Name"), *Key.ToString()));
		const UEnum* Enum = EnumProp->GetEnum();
		if (Enum)
		{
			const FString EnumValueName = Enum->GetNameStringByValue(EnumValue);
			Ar.WriteProperty(EnumNameKey, EnumValueName);
		}
		return;
	}

	// ── Struct ──────────────────────────────────────────────────────────

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		const UScriptStruct* Struct = StructProp->Struct;
		const FName StructFName = Struct->GetFName();

		// Known types — use typed WriteProperty overloads
		static const FName NAME_FGuid(TEXT("Guid"));
		static const FName NAME_GameplayTag(TEXT("GameplayTag"));
		static const FName NAME_GameplayTagContainer(TEXT("GameplayTagContainer"));

		if (StructFName == NAME_FGuid)
		{
			Ar.WriteProperty(Key, *static_cast<const FGuid*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTag)
		{
			Ar.WriteProperty(Key, *static_cast<const FGameplayTag*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTagContainer)
		{
			Ar.WriteProperty(Key, *static_cast<const FGameplayTagContainer*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Vector)
		{
			Ar.WriteProperty(Key, *static_cast<const FVector*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Rotator)
		{
			Ar.WriteProperty(Key, *static_cast<const FRotator*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Transform)
		{
			Ar.WriteProperty(Key, *static_cast<const FTransform*>(ValuePtr));
			return;
		}

		static const FName NAME_PrimaryAssetId(TEXT("PrimaryAssetId"));
		if (StructFName == NAME_PrimaryAssetId)
		{
			const FPrimaryAssetId& Id = *static_cast<const FPrimaryAssetId*>(ValuePtr);
			Ar.WriteProperty(Key, Id.ToString());
			return;
		}

		// Check registry for custom serializer before generic recursion
		const FArcPersistenceSerializerInfo* RegisteredInfo =
			FArcSerializerRegistry::Get().Find(Struct);
		if (RegisteredInfo)
		{
			Ar.BeginStruct(Key);
			RegisteredInfo->SaveFunc(ValuePtr, Ar);
			Ar.EndStruct();
			return;
		}

		// Generic struct — recurse
		Ar.BeginStruct(Key);
		Save(Struct, ValuePtr, Ar);
		Ar.EndStruct();
		return;
	}

	// ── Array ───────────────────────────────────────────────────────────

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		const int32 Num = ArrayHelper.Num();

		Ar.BeginArray(Key, Num);
		for (int32 i = 0; i < Num; ++i)
		{
			Ar.BeginArrayElement(i);

			FProperty* InnerProp = ArrayProp->Inner;
			const void* ElementPtr = ArrayHelper.GetRawPtr(i);

			// Use a value key matching the array element context.
			// Inside an array element scope the key is typically "value" or the property name,
			// but the archive element scope handles naming — we just write with the same key.
			SaveValueDirect(InnerProp, Key, ElementPtr, Ar);

			Ar.EndArrayElement();
		}
		Ar.EndArray();
		return;
	}

	// ── Map ────────────────────────────────────────────────────────────

	if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		const int32 Num = MapHelper.Num();

		Ar.BeginArray(Key, Num);
		int32 ElementIndex = 0;
		for (int32 SparseIndex = 0; SparseIndex < MapHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!MapHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			Ar.BeginArrayElement(ElementIndex);

			const void* PairKeyPtr = MapHelper.GetKeyPtr(SparseIndex);
			const void* PairValuePtr = MapHelper.GetValuePtr(SparseIndex);

			SaveValueDirect(MapProp->KeyProp, FName("k"), PairKeyPtr, Ar);
			SaveValueDirect(MapProp->ValueProp, FName("v"), PairValuePtr, Ar);

			Ar.EndArrayElement();
			++ElementIndex;
		}
		Ar.EndArray();
		return;
	}

	// ── Set ────────────────────────────────────────────────────────────

	if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		FScriptSetHelper SetHelper(SetProp, ValuePtr);
		const int32 Num = SetHelper.Num();

		Ar.BeginArray(Key, Num);
		int32 ElementIndex = 0;
		for (int32 SparseIndex = 0; SparseIndex < SetHelper.GetMaxIndex(); ++SparseIndex)
		{
			if (!SetHelper.IsValidIndex(SparseIndex))
			{
				continue;
			}

			Ar.BeginArrayElement(ElementIndex);

			const void* ElemPtr = SetHelper.GetElementPtr(SparseIndex);
			SaveValueDirect(SetProp->ElementProp, Key, ElemPtr, Ar);

			Ar.EndArrayElement();
			++ElementIndex;
		}
		Ar.EndArray();
		return;
	}

	// ── Soft object reference ───────────────────────────────────────────

	if (CastField<FSoftObjectProperty>(Prop))
	{
		const FSoftObjectPtr& SoftPtr = *static_cast<const FSoftObjectPtr*>(ValuePtr);
		Ar.WriteProperty(Key, SoftPtr.ToString());
		return;
	}

	// ── Hard object reference ──────────────────────────────────────────

	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		const UObject* Obj = ObjProp->GetObjectPropertyValue(ValuePtr);
		if (Obj)
		{
			UE_LOG(LogArcReflectionSerializer, Warning,
				TEXT("Serializing hard object reference for property '%s' (%s) — consider using TSoftObjectPtr for persistence."),
				*Prop->GetName(), *Obj->GetPathName());
			const FSoftObjectPath SoftPath(Obj);
			Ar.WriteProperty(Key, SoftPath.ToString());
		}
		else
		{
			Ar.WriteProperty(Key, FString());
		}
		return;
	}

	// ── Unsupported type ────────────────────────────────────────────────

	UE_LOG(LogArcReflectionSerializer, Warning,
		TEXT("Skipping unsupported property type '%s' for property '%s'."),
		*Prop->GetClass()->GetName(), *Prop->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// Load
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::Load(const UStruct* Type, void* Data, FArcLoadArchive& Ar)
{
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_SaveGame))
		{
			continue;
		}
		LoadProperty(Prop, Data, Ar);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadProperty — extracts value pointer from container, then delegates
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::LoadProperty(FProperty* Prop, void* ContainerData, FArcLoadArchive& Ar)
{
	const FName Key = Prop->GetFName();
	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(ContainerData);
	LoadValueDirect(Prop, Key, ValuePtr, Ar);
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadValueDirect — dispatch by FProperty subtype given a raw value pointer
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::LoadValueDirect(FProperty* Prop, FName Key, void* ValuePtr, FArcLoadArchive& Ar)
{
	// ── Bool ────────────────────────────────────────────────────────────

	if (FBoolProperty* BoolProp = CastField<FBoolProperty>(Prop))
	{
		bool TempValue = BoolProp->GetPropertyValue(ValuePtr);
		if (Ar.ReadProperty(Key, TempValue))
		{
			BoolProp->SetPropertyValue(ValuePtr, TempValue);
		}
		return;
	}

	// ── Integer types ───────────────────────────────────────────────────

	if (CastField<FIntProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<int32*>(ValuePtr));
		return;
	}

	if (CastField<FInt64Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<int64*>(ValuePtr));
		return;
	}

	if (CastField<FByteProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint8*>(ValuePtr));
		return;
	}

	if (CastField<FUInt16Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint16*>(ValuePtr));
		return;
	}

	if (CastField<FUInt32Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint32*>(ValuePtr));
		return;
	}

	if (CastField<FUInt64Property>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<uint64*>(ValuePtr));
		return;
	}

	// ── Floating point ──────────────────────────────────────────────────

	if (CastField<FFloatProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<float*>(ValuePtr));
		return;
	}

	if (CastField<FDoubleProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<double*>(ValuePtr));
		return;
	}

	// ── String / Name / Text ────────────────────────────────────────────

	if (CastField<FStrProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<FString*>(ValuePtr));
		return;
	}

	if (CastField<FNameProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<FName*>(ValuePtr));
		return;
	}

	if (CastField<FTextProperty>(Prop))
	{
		Ar.ReadProperty(Key, *static_cast<FText*>(ValuePtr));
		return;
	}

	// ── Enum ────────────────────────────────────────────────────────────

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
	{
		FNumericProperty* UnderlyingProp = EnumProp->GetUnderlyingProperty();
		int64 EnumValue = UnderlyingProp->GetSignedIntPropertyValue(ValuePtr);
		if (Ar.ReadProperty(Key, EnumValue))
		{
			UnderlyingProp->SetIntPropertyValue(ValuePtr, EnumValue);
		}
		// The _Name field is debug-only, we don't read it back
		return;
	}

	// ── Struct ──────────────────────────────────────────────────────────

	if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
	{
		const UScriptStruct* Struct = StructProp->Struct;
		const FName StructFName = Struct->GetFName();

		// Known types — use typed ReadProperty overloads
		static const FName NAME_FGuid(TEXT("Guid"));
		static const FName NAME_GameplayTag(TEXT("GameplayTag"));
		static const FName NAME_GameplayTagContainer(TEXT("GameplayTagContainer"));

		if (StructFName == NAME_FGuid)
		{
			Ar.ReadProperty(Key, *static_cast<FGuid*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTag)
		{
			Ar.ReadProperty(Key, *static_cast<FGameplayTag*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_GameplayTagContainer)
		{
			Ar.ReadProperty(Key, *static_cast<FGameplayTagContainer*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Vector)
		{
			Ar.ReadProperty(Key, *static_cast<FVector*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Rotator)
		{
			Ar.ReadProperty(Key, *static_cast<FRotator*>(ValuePtr));
			return;
		}
		if (StructFName == NAME_Transform)
		{
			Ar.ReadProperty(Key, *static_cast<FTransform*>(ValuePtr));
			return;
		}

		static const FName NAME_PrimaryAssetId(TEXT("PrimaryAssetId"));
		if (StructFName == NAME_PrimaryAssetId)
		{
			FString AssetIdStr;
			if (Ar.ReadProperty(Key, AssetIdStr))
			{
				*static_cast<FPrimaryAssetId*>(ValuePtr) = FPrimaryAssetId(AssetIdStr);
			}
			return;
		}

		// Check registry for custom serializer before generic recursion
		const FArcPersistenceSerializerInfo* RegisteredInfo =
			FArcSerializerRegistry::Get().Find(Struct);
		if (RegisteredInfo)
		{
			if (Ar.BeginStruct(Key))
			{
				RegisteredInfo->LoadFunc(ValuePtr, Ar);
				Ar.EndStruct();
			}
			return;
		}

		// Generic struct — recurse
		if (Ar.BeginStruct(Key))
		{
			Load(Struct, ValuePtr, Ar);
			Ar.EndStruct();
		}
		return;
	}

	// ── Array ───────────────────────────────────────────────────────────

	if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Prop))
	{
		int32 Num = 0;
		if (!Ar.BeginArray(Key, Num))
		{
			return;
		}

		FScriptArrayHelper ArrayHelper(ArrayProp, ValuePtr);
		ArrayHelper.Resize(Num);

		for (int32 i = 0; i < Num; ++i)
		{
			if (!Ar.BeginArrayElement(i))
			{
				break;
			}

			FProperty* InnerProp = ArrayProp->Inner;
			void* ElementPtr = ArrayHelper.GetRawPtr(i);

			LoadValueDirect(InnerProp, Key, ElementPtr, Ar);

			Ar.EndArrayElement();
		}
		Ar.EndArray();
		return;
	}

	// ── Map ────────────────────────────────────────────────────────────

	if (FMapProperty* MapProp = CastField<FMapProperty>(Prop))
	{
		int32 Num = 0;
		if (!Ar.BeginArray(Key, Num))
		{
			return;
		}

		FScriptMapHelper MapHelper(MapProp, ValuePtr);
		MapHelper.EmptyValues();

		for (int32 i = 0; i < Num; ++i)
		{
			if (!Ar.BeginArrayElement(i))
			{
				break;
			}

			const int32 NewIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			void* PairKeyPtr = MapHelper.GetKeyPtr(NewIndex);
			void* PairValuePtr = MapHelper.GetValuePtr(NewIndex);

			LoadValueDirect(MapProp->KeyProp, FName("k"), PairKeyPtr, Ar);
			LoadValueDirect(MapProp->ValueProp, FName("v"), PairValuePtr, Ar);

			Ar.EndArrayElement();
		}

		MapHelper.Rehash();
		Ar.EndArray();
		return;
	}

	// ── Set ────────────────────────────────────────────────────────────

	if (FSetProperty* SetProp = CastField<FSetProperty>(Prop))
	{
		int32 Num = 0;
		if (!Ar.BeginArray(Key, Num))
		{
			return;
		}

		FScriptSetHelper SetHelper(SetProp, ValuePtr);
		SetHelper.EmptyElements();

		for (int32 i = 0; i < Num; ++i)
		{
			if (!Ar.BeginArrayElement(i))
			{
				break;
			}

			const int32 NewIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			void* ElemPtr = SetHelper.GetElementPtr(NewIndex);

			LoadValueDirect(SetProp->ElementProp, Key, ElemPtr, Ar);

			Ar.EndArrayElement();
		}

		SetHelper.Rehash();
		Ar.EndArray();
		return;
	}

	// ── Soft object reference ───────────────────────────────────────────

	if (CastField<FSoftObjectProperty>(Prop))
	{
		FString SoftPathStr;
		if (Ar.ReadProperty(Key, SoftPathStr))
		{
			FSoftObjectPtr& SoftPtr = *static_cast<FSoftObjectPtr*>(ValuePtr);
			SoftPtr = FSoftObjectPath(SoftPathStr);
		}
		return;
	}

	// ── Hard object reference ──────────────────────────────────────────

	if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
	{
		FString PathStr;
		if (Ar.ReadProperty(Key, PathStr))
		{
			if (PathStr.IsEmpty())
			{
				ObjProp->SetObjectPropertyValue(ValuePtr, nullptr);
			}
			else
			{
				const FSoftObjectPath SoftPath(PathStr);
				UObject* Loaded = SoftPath.TryLoad();
				if (Loaded)
				{
					ObjProp->SetObjectPropertyValue(ValuePtr, Loaded);
				}
				else
				{
					UE_LOG(LogArcReflectionSerializer, Warning,
						TEXT("Failed to resolve object path '%s' for property '%s'."),
						*PathStr, *Prop->GetName());
				}
			}
		}
		return;
	}

	// ── Unsupported type ────────────────────────────────────────────────

	UE_LOG(LogArcReflectionSerializer, Warning,
		TEXT("Skipping unsupported property type '%s' for property '%s'."),
		*Prop->GetClass()->GetName(), *Prop->GetName());
}

// ─────────────────────────────────────────────────────────────────────────────
// ComputeSchemaVersion
// ─────────────────────────────────────────────────────────────────────────────

uint32 FArcReflectionSerializer::ComputeSchemaVersion(const UStruct* Type)
{
	uint32 Hash = 0;
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		if ((*It)->HasAnyPropertyFlags(CPF_SaveGame))
		{
			Hash = HashCombine(Hash, GetTypeHash((*It)->GetFName()));
			Hash = HashCombine(Hash, GetTypeHash(FName((*It)->GetCPPType())));
		}
	}
	return Hash;
}

// ─────────────────────────────────────────────────────────────────────────────
// SaveAll — serialize all properties (no SaveGame filter)
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::SaveAll(const UStruct* Type, const void* Data, FArcSaveArchive& Ar)
{
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		SaveProperty(*It, Data, Ar);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadAll — deserialize all properties (no SaveGame filter)
// ─────────────────────────────────────────────────────────────────────────────

void FArcReflectionSerializer::LoadAll(const UStruct* Type, void* Data, FArcLoadArchive& Ar)
{
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		LoadProperty(*It, Data, Ar);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// ComputeSchemaVersionAll — hash all properties (no SaveGame filter)
// ─────────────────────────────────────────────────────────────────────────────

uint32 FArcReflectionSerializer::ComputeSchemaVersionAll(const UStruct* Type)
{
	uint32 Hash = 0;
	for (TFieldIterator<FProperty> It(Type); It; ++It)
	{
		Hash = HashCombine(Hash, GetTypeHash((*It)->GetFName()));
		Hash = HashCombine(Hash, GetTypeHash(FName((*It)->GetCPPType())));
	}
	return Hash;
}
