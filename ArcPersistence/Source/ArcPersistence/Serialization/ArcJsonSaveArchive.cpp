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

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json& FArcJsonSaveArchive::Current()
{
	if (ScopeStack.Num() > 0)
	{
		return *ScopeStack.Last();
	}
	return Root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Primitive property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, bool Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, int32 Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, int64 Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint8 Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint16 Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint32 Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, uint64 Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, float Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

void FArcJsonSaveArchive::WriteProperty(FName Key, double Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = Value;
}

// ─────────────────────────────────────────────────────────────────────────────
// String / name / text property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FString& Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = TCHAR_TO_UTF8(*Value);
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FName& Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = TCHAR_TO_UTF8(*Value.ToString());
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FText& Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = TCHAR_TO_UTF8(*Value.ToString());
}

// ─────────────────────────────────────────────────────────────────────────────
// Identifier property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FGuid& Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = TCHAR_TO_UTF8(*Value.ToString(EGuidFormats::DigitsWithHyphens));
}

// ─────────────────────────────────────────────────────────────────────────────
// Gameplay tag property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FGameplayTag& Value)
{
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = TCHAR_TO_UTF8(*Value.GetTagName().ToString());
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FGameplayTagContainer& Value)
{
	nlohmann::json TagArray = nlohmann::json::array();

	for (const FGameplayTag& Tag : Value)
	{
		TagArray.push_back(TCHAR_TO_UTF8(*Tag.GetTagName().ToString()));
	}

	Current()[TCHAR_TO_UTF8(*Key.ToString())] = MoveTemp(TagArray);
}

// ─────────────────────────────────────────────────────────────────────────────
// Math property writers
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::WriteProperty(FName Key, const FVector& Value)
{
	nlohmann::json Obj;
	Obj["X"] = Value.X;
	Obj["Y"] = Value.Y;
	Obj["Z"] = Value.Z;
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = MoveTemp(Obj);
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FRotator& Value)
{
	nlohmann::json Obj;
	Obj["Pitch"] = Value.Pitch;
	Obj["Yaw"] = Value.Yaw;
	Obj["Roll"] = Value.Roll;
	Current()[TCHAR_TO_UTF8(*Key.ToString())] = MoveTemp(Obj);
}

void FArcJsonSaveArchive::WriteProperty(FName Key, const FTransform& Value)
{
	const FQuat Rot = Value.GetRotation();
	const FVector Trans = Value.GetTranslation();
	const FVector Scale = Value.GetScale3D();

	nlohmann::json RotObj;
	RotObj["X"] = Rot.X;
	RotObj["Y"] = Rot.Y;
	RotObj["Z"] = Rot.Z;
	RotObj["W"] = Rot.W;

	nlohmann::json TransObj;
	TransObj["X"] = Trans.X;
	TransObj["Y"] = Trans.Y;
	TransObj["Z"] = Trans.Z;

	nlohmann::json ScaleObj;
	ScaleObj["X"] = Scale.X;
	ScaleObj["Y"] = Scale.Y;
	ScaleObj["Z"] = Scale.Z;

	nlohmann::json Obj;
	Obj["Rotation"] = MoveTemp(RotObj);
	Obj["Translation"] = MoveTemp(TransObj);
	Obj["Scale3D"] = MoveTemp(ScaleObj);

	Current()[TCHAR_TO_UTF8(*Key.ToString())] = MoveTemp(Obj);
}

// ─────────────────────────────────────────────────────────────────────────────
// Struct scope
// ─────────────────────────────────────────────────────────────────────────────

void FArcJsonSaveArchive::BeginStruct(FName Key)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());

	// Create the object in the current scope first, then take its address.
	Current()[KeyStr] = nlohmann::json::object();
	ScopeStack.Push(&Current()[KeyStr]);
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
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());

	// Create the array in the current scope, then push its address.
	Current()[KeyStr] = nlohmann::json::array();
	ScopeStack.Push(&Current()[KeyStr]);
}

void FArcJsonSaveArchive::BeginArrayElement(int32 Index)
{
	// Create a temporary JSON object for this element via TUniquePtr for pointer stability.
	// TArray reallocation won't invalidate pointers to earlier temp elements held in ScopeStack.
	TempElements.Add(MakeUnique<nlohmann::json>(nlohmann::json::object()));
	ScopeStack.Push(TempElements.Last().Get());
}

void FArcJsonSaveArchive::EndArrayElement()
{
	check(ScopeStack.Num() > 0);
	check(TempElements.Num() > 0);

	// Pop the temp element off the scope stack.
	ScopeStack.Pop();

	// Move the completed temp element into the parent array.
	nlohmann::json Element = MoveTemp(*TempElements.Last());
	TempElements.Pop();

	// The array should be the current scope (we pushed it in BeginArray).
	check(ScopeStack.Num() > 0);
	ScopeStack.Last()->push_back(MoveTemp(Element));
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
	nlohmann::json Envelope;
	Envelope["version"] = Version;
	Envelope["data"] = Root;

	const std::string JsonStr = Envelope.dump(2); // pretty-print with 2-space indent

	TArray<uint8> Result;
	Result.Append(reinterpret_cast<const uint8*>(JsonStr.data()), static_cast<int32>(JsonStr.size()));
	return Result;
}
