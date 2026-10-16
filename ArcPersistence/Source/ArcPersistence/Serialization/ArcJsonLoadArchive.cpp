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

#include "Serialization/ArcJsonLoadArchive.h"

#include "ArcYyJson.h"
#include "Misc/StringBuilder.h"

#include <type_traits>

namespace ArcJsonLoadArchivePrivate
{
	yyjson_val* FindField(const yyjson_val* Obj, FName Key)
	{
		TUtf8StringBuilder<FName::StringBufferSize> KeyText;
		Key.AppendString(KeyText);
		return yyjson_obj_getn(Obj, reinterpret_cast<const char*>(KeyText.GetData()), static_cast<size_t>(KeyText.Len()));
	}

	template <typename T>
	bool GetInteger(const yyjson_val* Val, T& OutValue)
	{
		if (yyjson_is_uint(Val))
		{
			const uint64 Value = yyjson_get_uint(Val);
			if (Value > static_cast<uint64>(TNumericLimits<T>::Max()))
			{
				return false;
			}
			OutValue = static_cast<T>(Value);
			return true;
		}
		if (yyjson_is_sint(Val))
		{
			const int64 Value = yyjson_get_sint(Val);
			if constexpr (std::is_signed_v<T>)
			{
				if (Value < static_cast<int64>(TNumericLimits<T>::Min()) || Value > static_cast<int64>(TNumericLimits<T>::Max()))
				{
					return false;
				}
			}
			else
			{
				if (Value < 0 || static_cast<uint64>(Value) > static_cast<uint64>(TNumericLimits<T>::Max()))
				{
					return false;
				}
			}
			OutValue = static_cast<T>(Value);
			return true;
		}
		return false;
	}

	template <typename T>
	bool ReadInteger(const yyjson_val* Obj, FName Key, T& OutValue)
	{
		return GetInteger(FindField(Obj, Key), OutValue);
	}

	bool GetNumber(const yyjson_val* Val, double& OutValue)
	{
		if (!yyjson_is_num(Val))
		{
			return false;
		}
		OutValue = yyjson_get_num(Val);
		return true;
	}

	bool GetNumberField(const yyjson_val* Obj, const char* Name, double& OutValue)
	{
		return GetNumber(yyjson_obj_get(Obj, Name), OutValue);
	}

	bool GetVector(const yyjson_val* Obj, FVector& OutValue)
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		if (!GetNumberField(Obj, "X", X) || !GetNumberField(Obj, "Y", Y) || !GetNumberField(Obj, "Z", Z))
		{
			return false;
		}
		OutValue = FVector(X, Y, Z);
		return true;
	}

	bool GetString(const yyjson_val* Obj, FName Key, FString& OutValue)
	{
		const yyjson_val* Val = FindField(Obj, Key);
		if (!yyjson_is_str(Val))
		{
			return false;
		}
		OutValue = ArcJson::ToFString(Val);
		return true;
	}
}

void FArcJsonLoadArchive::FDocDeleter::operator()(yyjson_doc* InDoc) const
{
	yyjson_doc_free(InDoc);
}

FArcJsonLoadArchive::FArcJsonLoadArchive() = default;
FArcJsonLoadArchive::~FArcJsonLoadArchive() = default;
FArcJsonLoadArchive::FArcJsonLoadArchive(FArcJsonLoadArchive&&) = default;
FArcJsonLoadArchive& FArcJsonLoadArchive::operator=(FArcJsonLoadArchive&&) = default;

yyjson_val* FArcJsonLoadArchive::Current() const
{
	if (ScopeStack.Num() > 0)
	{
		return ScopeStack.Last().Value;
	}
	return Root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::InitializeFromData(const TArray<uint8>& Data)
{
	ArcJson::FDocPtr Parsed = ArcJson::Parse(Data);
	if (!Parsed.IsValid())
	{
		return false;
	}

	const yyjson_val* Envelope = yyjson_doc_get_root(Parsed.Get());
	yyjson_val* VersionVal = yyjson_obj_get(Envelope, "version");
	yyjson_val* DataVal = yyjson_obj_get(Envelope, "data");
	uint32 ParsedVersion = 0;
	if (DataVal == nullptr || !ArcJsonLoadArchivePrivate::GetInteger(VersionVal, ParsedVersion))
	{
		return false;
	}

	Doc.Reset(Parsed.Release());
	Root = DataVal;
	Version = ParsedVersion;
	ScopeStack.Reset();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Primitive property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, bool& OutValue)
{
	const yyjson_val* Val = ArcJsonLoadArchivePrivate::FindField(Current(), Key);
	if (!yyjson_is_bool(Val))
	{
		return false;
	}
	OutValue = yyjson_get_bool(Val);
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, int32& OutValue)
{
	return ArcJsonLoadArchivePrivate::ReadInteger(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, int64& OutValue)
{
	return ArcJsonLoadArchivePrivate::ReadInteger(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint8& OutValue)
{
	return ArcJsonLoadArchivePrivate::ReadInteger(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint16& OutValue)
{
	return ArcJsonLoadArchivePrivate::ReadInteger(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint32& OutValue)
{
	return ArcJsonLoadArchivePrivate::ReadInteger(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint64& OutValue)
{
	return ArcJsonLoadArchivePrivate::ReadInteger(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, float& OutValue)
{
	double Value = 0.0;
	if (!ArcJsonLoadArchivePrivate::GetNumber(ArcJsonLoadArchivePrivate::FindField(Current(), Key), Value))
	{
		return false;
	}
	OutValue = static_cast<float>(Value);
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, double& OutValue)
{
	return ArcJsonLoadArchivePrivate::GetNumber(ArcJsonLoadArchivePrivate::FindField(Current(), Key), OutValue);
}

// ─────────────────────────────────────────────────────────────────────────────
// String / name / text property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FString& OutValue)
{
	return ArcJsonLoadArchivePrivate::GetString(Current(), Key, OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FName& OutValue)
{
	FString Str;
	if (!ArcJsonLoadArchivePrivate::GetString(Current(), Key, Str))
	{
		return false;
	}
	OutValue = FName(*Str);
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FText& OutValue)
{
	FString Str;
	if (!ArcJsonLoadArchivePrivate::GetString(Current(), Key, Str))
	{
		return false;
	}
	OutValue = FText::FromString(MoveTemp(Str));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Identifier property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FGuid& OutValue)
{
	FString Str;
	if (!ArcJsonLoadArchivePrivate::GetString(Current(), Key, Str))
	{
		return false;
	}
	FGuid ParsedGuid;
	if (!FGuid::Parse(Str, ParsedGuid))
	{
		return false;
	}
	OutValue = ParsedGuid;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Gameplay tag property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FGameplayTag& OutValue)
{
	FString Str;
	if (!ArcJsonLoadArchivePrivate::GetString(Current(), Key, Str))
	{
		return false;
	}
	if (Str.IsEmpty() || Str.Equals(TEXT("None"), ESearchCase::CaseSensitive))
	{
		OutValue = FGameplayTag::EmptyTag;
		return true;
	}
	OutValue = FGameplayTag::RequestGameplayTag(FName(*Str));
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FGameplayTagContainer& OutValue)
{
	yyjson_val* TagArray = ArcJsonLoadArchivePrivate::FindField(Current(), Key);
	if (!yyjson_is_arr(TagArray))
	{
		return false;
	}

	size_t Idx = 0;
	size_t Max = 0;
	yyjson_val* TagEntry = nullptr;
	yyjson_arr_foreach(TagArray, Idx, Max, TagEntry)
	{
		if (!yyjson_is_str(TagEntry))
		{
			return false;
		}
	}

	OutValue.Reset();
	yyjson_arr_foreach(TagArray, Idx, Max, TagEntry)
	{
		OutValue.AddTag(FGameplayTag::RequestGameplayTag(FName(*ArcJson::ToFString(TagEntry))));
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Math property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FVector& OutValue)
{
	return ArcJsonLoadArchivePrivate::GetVector(ArcJsonLoadArchivePrivate::FindField(Current(), Key), OutValue);
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FRotator& OutValue)
{
	const yyjson_val* Obj = ArcJsonLoadArchivePrivate::FindField(Current(), Key);
	double Pitch = 0.0;
	double Yaw = 0.0;
	double Roll = 0.0;
	if (!ArcJsonLoadArchivePrivate::GetNumberField(Obj, "Pitch", Pitch)
		|| !ArcJsonLoadArchivePrivate::GetNumberField(Obj, "Yaw", Yaw)
		|| !ArcJsonLoadArchivePrivate::GetNumberField(Obj, "Roll", Roll))
	{
		return false;
	}
	OutValue = FRotator(Pitch, Yaw, Roll);
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FTransform& OutValue)
{
	const yyjson_val* Obj = ArcJsonLoadArchivePrivate::FindField(Current(), Key);
	const yyjson_val* RotObj = yyjson_obj_get(Obj, "Rotation");

	double X = 0.0;
	double Y = 0.0;
	double Z = 0.0;
	double W = 0.0;
	if (!ArcJsonLoadArchivePrivate::GetNumberField(RotObj, "X", X)
		|| !ArcJsonLoadArchivePrivate::GetNumberField(RotObj, "Y", Y)
		|| !ArcJsonLoadArchivePrivate::GetNumberField(RotObj, "Z", Z)
		|| !ArcJsonLoadArchivePrivate::GetNumberField(RotObj, "W", W))
	{
		return false;
	}

	FVector Translation;
	FVector Scale3D;
	if (!ArcJsonLoadArchivePrivate::GetVector(yyjson_obj_get(Obj, "Translation"), Translation)
		|| !ArcJsonLoadArchivePrivate::GetVector(yyjson_obj_get(Obj, "Scale3D"), Scale3D))
	{
		return false;
	}

	OutValue = FTransform(FQuat(X, Y, Z, W), Translation, Scale3D);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Struct scope
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::BeginStruct(FName Key)
{
	yyjson_val* Child = ArcJsonLoadArchivePrivate::FindField(Current(), Key);
	if (!yyjson_is_obj(Child))
	{
		return false;
	}

	ScopeStack.Add(FScope{Child, nullptr, INDEX_NONE});
	return true;
}

void FArcJsonLoadArchive::EndStruct()
{
	check(ScopeStack.Num() > 0);
	ScopeStack.Pop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Array scope
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::BeginArray(FName Key, int32& OutCount)
{
	yyjson_val* Child = ArcJsonLoadArchivePrivate::FindField(Current(), Key);
	if (!yyjson_is_arr(Child))
	{
		return false;
	}

	OutCount = static_cast<int32>(yyjson_arr_size(Child));
	ScopeStack.Add(FScope{Child, nullptr, INDEX_NONE});
	return true;
}

bool FArcJsonLoadArchive::BeginArrayElement(int32 Index)
{
	FScope* ArrayScope = ScopeStack.Num() > 0 ? &ScopeStack.Last() : nullptr;
	yyjson_val* Array = ArrayScope != nullptr ? ArrayScope->Value : Root;
	if (!yyjson_is_arr(Array))
	{
		return false;
	}

	if (Index < 0 || static_cast<size_t>(Index) >= yyjson_arr_size(Array))
	{
		return false;
	}

	yyjson_val* Element = nullptr;
	// The cursor resumes the unsafe_yyjson_get_next walk that yyjson_arr_get restarts from element 0 (yyjson/yyjson.h:5615).
	if (ArrayScope != nullptr && ArrayScope->CursorElement != nullptr && Index == ArrayScope->CursorIndex + 1)
	{
		Element = unsafe_yyjson_get_next(ArrayScope->CursorElement);
	}
	else if (ArrayScope != nullptr && ArrayScope->CursorElement != nullptr && Index == ArrayScope->CursorIndex)
	{
		Element = ArrayScope->CursorElement;
	}
	else
	{
		Element = yyjson_arr_get(Array, static_cast<size_t>(Index));
	}

	if (ArrayScope != nullptr)
	{
		ArrayScope->CursorElement = Element;
		ArrayScope->CursorIndex = Index;
	}

	ScopeStack.Add(FScope{Element, nullptr, INDEX_NONE});
	return true;
}

void FArcJsonLoadArchive::EndArrayElement()
{
	check(ScopeStack.Num() > 0);
	ScopeStack.Pop();
}

void FArcJsonLoadArchive::EndArray()
{
	check(ScopeStack.Num() > 0);
	ScopeStack.Pop();
}
