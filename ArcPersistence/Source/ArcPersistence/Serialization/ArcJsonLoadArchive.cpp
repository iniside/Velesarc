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

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

nlohmann::json* FArcJsonLoadArchive::Current()
{
	if (ScopeStack.Num() > 0)
	{
		return ScopeStack.Last();
	}
	return &Root;
}

// ─────────────────────────────────────────────────────────────────────────────
// Initialization
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::InitializeFromData(const TArray<uint8>& Data)
{
	const std::string JsonStr(reinterpret_cast<const char*>(Data.GetData()), Data.Num());
	nlohmann::json Envelope = nlohmann::json::parse(JsonStr, nullptr, false);

	if (Envelope.is_discarded())
	{
		return false;
	}

	if (!Envelope.contains("version") || !Envelope.contains("data"))
	{
		return false;
	}

	Version = Envelope["version"].get<uint32>();
	Root = Envelope["data"];
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Primitive property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, bool& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<bool>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, int32& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<int32>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, int64& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<int64>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint8& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<uint8>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint16& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<uint16>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint32& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<uint32>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, uint64& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<uint64>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, float& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<float>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, double& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	OutValue = (*Current())[KeyStr].get<double>();
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// String / name / text property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FString& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	const std::string Str = (*Current())[KeyStr].get<std::string>();
	OutValue = UTF8_TO_TCHAR(Str.c_str());
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FName& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	const std::string Str = (*Current())[KeyStr].get<std::string>();
	OutValue = FName(UTF8_TO_TCHAR(Str.c_str()));
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FText& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	const std::string Str = (*Current())[KeyStr].get<std::string>();
	OutValue = FText::FromString(UTF8_TO_TCHAR(Str.c_str()));
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Identifier property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FGuid& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	const std::string Str = (*Current())[KeyStr].get<std::string>();
	FGuid ParsedGuid;
	if (!FGuid::Parse(UTF8_TO_TCHAR(Str.c_str()), ParsedGuid))
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
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}
	const std::string Str = (*Current())[KeyStr].get<std::string>();
	OutValue = FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(Str.c_str())));
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FGameplayTagContainer& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}

	const nlohmann::json& TagArray = (*Current())[KeyStr];
	if (!TagArray.is_array())
	{
		return false;
	}

	OutValue.Reset();
	for (const auto& TagEntry : TagArray)
	{
		const std::string TagStr = TagEntry.get<std::string>();
		OutValue.AddTag(FGameplayTag::RequestGameplayTag(FName(UTF8_TO_TCHAR(TagStr.c_str()))));
	}
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Math property readers
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::ReadProperty(FName Key, FVector& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}

	const nlohmann::json& Obj = (*Current())[KeyStr];
	if (!Obj.is_object() || !Obj.contains("X") || !Obj.contains("Y") || !Obj.contains("Z"))
	{
		return false;
	}

	OutValue.X = Obj["X"].get<double>();
	OutValue.Y = Obj["Y"].get<double>();
	OutValue.Z = Obj["Z"].get<double>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FRotator& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}

	const nlohmann::json& Obj = (*Current())[KeyStr];
	if (!Obj.is_object() || !Obj.contains("Pitch") || !Obj.contains("Yaw") || !Obj.contains("Roll"))
	{
		return false;
	}

	OutValue.Pitch = Obj["Pitch"].get<double>();
	OutValue.Yaw = Obj["Yaw"].get<double>();
	OutValue.Roll = Obj["Roll"].get<double>();
	return true;
}

bool FArcJsonLoadArchive::ReadProperty(FName Key, FTransform& OutValue)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}

	const nlohmann::json& Obj = (*Current())[KeyStr];
	if (!Obj.is_object() || !Obj.contains("Rotation") || !Obj.contains("Translation") || !Obj.contains("Scale3D"))
	{
		return false;
	}

	const nlohmann::json& RotObj = Obj["Rotation"];
	const nlohmann::json& TransObj = Obj["Translation"];
	const nlohmann::json& ScaleObj = Obj["Scale3D"];

	if (!RotObj.contains("X") || !RotObj.contains("Y") || !RotObj.contains("Z") || !RotObj.contains("W"))
	{
		return false;
	}
	if (!TransObj.contains("X") || !TransObj.contains("Y") || !TransObj.contains("Z"))
	{
		return false;
	}
	if (!ScaleObj.contains("X") || !ScaleObj.contains("Y") || !ScaleObj.contains("Z"))
	{
		return false;
	}

	const FQuat Rotation(
		RotObj["X"].get<double>(),
		RotObj["Y"].get<double>(),
		RotObj["Z"].get<double>(),
		RotObj["W"].get<double>()
	);

	const FVector Translation(
		TransObj["X"].get<double>(),
		TransObj["Y"].get<double>(),
		TransObj["Z"].get<double>()
	);

	const FVector Scale3D(
		ScaleObj["X"].get<double>(),
		ScaleObj["Y"].get<double>(),
		ScaleObj["Z"].get<double>()
	);

	OutValue = FTransform(Rotation, Translation, Scale3D);
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Struct scope
// ─────────────────────────────────────────────────────────────────────────────

bool FArcJsonLoadArchive::BeginStruct(FName Key)
{
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}

	nlohmann::json& Child = (*Current())[KeyStr];
	if (!Child.is_object())
	{
		return false;
	}

	ScopeStack.Push(&Child);
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
	const std::string KeyStr = TCHAR_TO_UTF8(*Key.ToString());
	if (!Current()->contains(KeyStr))
	{
		return false;
	}

	nlohmann::json& Child = (*Current())[KeyStr];
	if (!Child.is_array())
	{
		return false;
	}

	OutCount = static_cast<int32>(Child.size());
	ScopeStack.Push(&Child);
	return true;
}

bool FArcJsonLoadArchive::BeginArrayElement(int32 Index)
{
	nlohmann::json* ArrayPtr = Current();
	if (!ArrayPtr->is_array())
	{
		return false;
	}

	if (Index < 0 || Index >= static_cast<int32>(ArrayPtr->size()))
	{
		return false;
	}

	ScopeStack.Push(&(*ArrayPtr)[Index]);
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
