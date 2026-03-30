// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace ArcMCPToolsetPrivate
{
	inline FString JsonObjToString(const TSharedPtr<FJsonObject>& JsonObj)
	{
		FString OutputString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
		FJsonSerializer::Serialize(JsonObj.ToSharedRef(), Writer);
		return OutputString;
	}

	inline TSharedPtr<FJsonObject> StringToJsonObj(const FString& JsonString)
	{
		TSharedPtr<FJsonObject> JsonObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
		FJsonSerializer::Deserialize(Reader, JsonObj);
		return JsonObj;
	}

	inline FString MakeError(const FString& Message)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("error"), Message);
		return JsonObjToString(Result);
	}
}
