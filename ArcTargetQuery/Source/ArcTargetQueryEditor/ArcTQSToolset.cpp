// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTQSToolset.h"

#include "ArcTQSDefinitionJson.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTQSToolset)

namespace ArcTQSToolsetPrivate
{
	FString ToString(const TSharedRef<FJsonObject>& Object)
	{
		FString Out;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
		FJsonSerializer::Serialize(Object, Writer);
		return Out;
	}

	FString MakeError(const FString& Message)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(Message));
		Result->SetArrayField(TEXT("errors"), Errors);
		return ToString(Result);
	}

	bool ParseObject(const FString& Text, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
	{
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			OutError = Reader->GetErrorMessage();
			return false;
		}
		return true;
	}
}

FString UArcTQSToolset::ListTQSNodeTypes(const FString& Role, const FString& Filter)
{
	return ArcTQSToolsetPrivate::ToString(ArcTQSDefinitionJson::ListNodeTypes(Role, Filter));
}

FString UArcTQSToolset::ExportTQSDefinition(const FString& Path)
{
	return ArcTQSToolsetPrivate::ToString(ArcTQSDefinitionJson::ExportAsset(Path));
}

FString UArcTQSToolset::ApplyTQSDefinition(const FString& Path, const FString& DefinitionJson)
{
	TSharedPtr<FJsonObject> Parsed;
	FString Error;
	if (!ArcTQSToolsetPrivate::ParseObject(DefinitionJson, Parsed, Error))
	{
		return ArcTQSToolsetPrivate::MakeError(FString::Printf(TEXT("DefinitionJson is not a JSON object: %s"), *Error));
	}
	return ArcTQSToolsetPrivate::ToString(ArcTQSDefinitionJson::ApplyToAsset(Path, Parsed.ToSharedRef()));
}

FString UArcTQSToolset::CompileTQSDefinition(const FString& Path)
{
	return ArcTQSToolsetPrivate::ToString(ArcTQSDefinitionJson::CompileAsset(Path));
}

FString UArcTQSToolset::SetTQSReference(
	const FString& ObjectPath,
	const FString& PropertyName,
	const FString& DefinitionPath,
	const FString& ParametersJson)
{
	TSharedPtr<FJsonObject> Parameters;
	if (!ParametersJson.TrimStartAndEnd().IsEmpty())
	{
		FString Error;
		if (!ArcTQSToolsetPrivate::ParseObject(ParametersJson, Parameters, Error))
		{
			return ArcTQSToolsetPrivate::MakeError(FString::Printf(TEXT("ParametersJson is not a JSON object: %s"), *Error));
		}
	}
	return ArcTQSToolsetPrivate::ToString(
		ArcTQSDefinitionJson::SetQueryReference(ObjectPath, PropertyName, DefinitionPath, Parameters));
}
