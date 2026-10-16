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

#include "Serialization/ArcJsonSaveArchive.h"

#include "ArcYyJson.h"

namespace ArcJsonSaveArchivePrivate
{
	void Put(yyjson_mut_doc* Doc, yyjson_mut_val* Obj, FName Key, yyjson_mut_val* Value)
	{
		yyjson_mut_obj_put(Obj, ArcJson::CopyName(Doc, Key), Value);
	}

	yyjson_mut_val* MakeVector(yyjson_mut_doc* Doc, const FVector& Value)
	{
		yyjson_mut_val* Obj = yyjson_mut_obj(Doc);
		yyjson_mut_obj_add_double(Doc, Obj, "X", Value.X);
		yyjson_mut_obj_add_double(Doc, Obj, "Y", Value.Y);
		yyjson_mut_obj_add_double(Doc, Obj, "Z", Value.Z);
		return Obj;
	}
}

void FArcJsonSaveArchive::FDocDeleter::operator()(yyjson_mut_doc* InDoc) const
{
	yyjson_mut_doc_free(InDoc);
}

FArcJsonSaveArchive::FArcJsonSaveArchive()
	: Doc(yyjson_mut_doc_new(nullptr))
{
	check(Doc.IsValid());
	Root = yyjson_mut_obj(Doc.Get());
}

FArcJsonSaveArchive::~FArcJsonSaveArchive() = default;
FArcJsonSaveArchive::FArcJsonSaveArchive(FArcJsonSaveArchive&&) = default;
FArcJsonSaveArchive& FArcJsonSaveArchive::operator=(FArcJsonSaveArchive&&) = default;

yyjson_mut_val* FArcJsonSaveArchive::Current() const
{
	if (ScopeStack.Num() > 0)
	{
		return ScopeStack.Last();
	}
	return Root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Primitive property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, bool Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_bool(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, int32 Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_sint(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, int64 Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_sint(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint8 Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_uint(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint16 Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_uint(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint32 Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_uint(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint64 Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_uint(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, float Value)
{
	yyjson_mut_val* Number = yyjson_mut_double(Doc.Get(), static_cast<double>(Value));
	yyjson_mut_set_fp_to_float(Number, true);
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, Number);
}

void FArcJsonSaveArchive::WriteProperty(FName Key, double Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, yyjson_mut_double(Doc.Get(), Value));
}

// ─────────────────────────────────────────────────────────────────────────────
// String / name / text property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FString& Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, ArcJson::CopyString(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FName& Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, ArcJson::CopyName(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FText& Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, ArcJson::CopyString(Doc.Get(), Value.ToString()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Identifier property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FGuid& Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, ArcJson::CopyString(Doc.Get(), Value.ToString(EGuidFormats::DigitsWithHyphens)));
}

// ─────────────────────────────────────────────────────────────────────────────
// Gameplay tag property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FGameplayTag& Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, ArcJson::CopyName(Doc.Get(), Value.GetTagName()));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FGameplayTagContainer& Value)
{
	yyjson_mut_val* TagArray = yyjson_mut_arr(Doc.Get());
	for (const FGameplayTag& Tag : Value)
	{
		yyjson_mut_arr_append(TagArray, ArcJson::CopyName(Doc.Get(), Tag.GetTagName()));
	}
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, TagArray);
}

// ─────────────────────────────────────────────────────────────────────────────
// Math property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FVector& Value)
{
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, ArcJsonSaveArchivePrivate::MakeVector(Doc.Get(), Value));
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FRotator& Value)
{
	yyjson_mut_val* Obj = yyjson_mut_obj(Doc.Get());
	yyjson_mut_obj_add_double(Doc.Get(), Obj, "Pitch", Value.Pitch);
	yyjson_mut_obj_add_double(Doc.Get(), Obj, "Yaw", Value.Yaw);
	yyjson_mut_obj_add_double(Doc.Get(), Obj, "Roll", Value.Roll);
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, Obj);
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FTransform& Value)
{
	const FQuat Rot = Value.GetRotation();

	yyjson_mut_val* RotObj = yyjson_mut_obj(Doc.Get());
	yyjson_mut_obj_add_double(Doc.Get(), RotObj, "X", Rot.X);
	yyjson_mut_obj_add_double(Doc.Get(), RotObj, "Y", Rot.Y);
	yyjson_mut_obj_add_double(Doc.Get(), RotObj, "Z", Rot.Z);
	yyjson_mut_obj_add_double(Doc.Get(), RotObj, "W", Rot.W);

	yyjson_mut_val* Obj = yyjson_mut_obj(Doc.Get());
	yyjson_mut_obj_add_val(Doc.Get(), Obj, "Rotation", RotObj);
	yyjson_mut_obj_add_val(Doc.Get(), Obj, "Translation", ArcJsonSaveArchivePrivate::MakeVector(Doc.Get(), Value.GetTranslation()));
	yyjson_mut_obj_add_val(Doc.Get(), Obj, "Scale3D", ArcJsonSaveArchivePrivate::MakeVector(Doc.Get(), Value.GetScale3D()));

	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, Obj);
}

// ─────────────────────────────────────────────────────────────────────────────
// Struct scope
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::BeginStruct(FName Key)
{
	yyjson_mut_val* Child = yyjson_mut_obj(Doc.Get());
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, Child);
	ScopeStack.Push(Child);
}

void FArcJsonSaveArchive::EndStruct()
{
	check(ScopeStack.Num() > 0);
	ScopeStack.Pop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Array scope
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::BeginArray(FName Key, int32 Count)
{
	yyjson_mut_val* Child = yyjson_mut_arr(Doc.Get());
	ArcJsonSaveArchivePrivate::Put(Doc.Get(), Current(), Key, Child);
	ScopeStack.Push(Child);
}

void FArcJsonSaveArchive::BeginArrayElement(int32 Index)
{
	check(ScopeStack.Num() > 0);
	yyjson_mut_val* Element = yyjson_mut_obj(Doc.Get());
	yyjson_mut_arr_append(ScopeStack.Last(), Element);
	ScopeStack.Push(Element);
}

void FArcJsonSaveArchive::EndArrayElement()
{
	check(ScopeStack.Num() > 0);
	ScopeStack.Pop();
}

void FArcJsonSaveArchive::EndArray()
{
	check(ScopeStack.Num() > 0);
	ScopeStack.Pop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Finalization
// ─────────────────────────────────────────────────────────────────────────────

TArray<uint8> FArcJsonSaveArchive::Finalize()
{
	if (Envelope == nullptr)
	{
		Envelope = yyjson_mut_obj(Doc.Get());
		yyjson_mut_obj_add_uint(Doc.Get(), Envelope, "version", Version);
		yyjson_mut_obj_add_val(Doc.Get(), Envelope, "data", Root);
		yyjson_mut_doc_set_root(Doc.Get(), Envelope);
	}
	else
	{
		yyjson_mut_obj_put(Envelope, yyjson_mut_str(Doc.Get(), "version"), yyjson_mut_uint(Doc.Get(), Version));
	}

	return ArcJson::WritePretty(Doc.Get());
}
