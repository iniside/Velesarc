// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcTacticsToolset.h"

#include "ArcTacticsConfigJson.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcTacticsToolset)

namespace ArcTacticsToolsetPrivate
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
}

FString UArcTacticsToolset::ListTacticsNodeTypes(const FString& Role, const FString& Filter)
{
	return ArcTacticsToolsetPrivate::ToString(ArcTacticsConfigJson::ListNodeTypes(Role, Filter));
}

FString UArcTacticsToolset::ExportTacticsConfig(const FString& Path)
{
	return ArcTacticsToolsetPrivate::ToString(ArcTacticsConfigJson::ExportAsset(Path));
}

FString UArcTacticsToolset::ApplyTacticsConfig(const FString& Path, const FString& ConfigJson)
{
	TSharedPtr<FJsonObject> Parsed;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(ConfigJson);
	if (!FJsonSerializer::Deserialize(Reader, Parsed) || !Parsed.IsValid())
	{
		return ArcTacticsToolsetPrivate::MakeError(FString::Printf(
			TEXT("ConfigJson is not a JSON object: %s"),
			*Reader->GetErrorMessage()));
	}
	return ArcTacticsToolsetPrivate::ToString(ArcTacticsConfigJson::ApplyToAsset(Path, Parsed.ToSharedRef()));
}

FString UArcTacticsToolset::CompileTacticsConfig(const FString& Path)
{
	return ArcTacticsToolsetPrivate::ToString(ArcTacticsConfigJson::CompileAsset(Path));
}
