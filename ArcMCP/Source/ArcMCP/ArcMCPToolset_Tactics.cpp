// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Tactics/ArcTacticsRecording.h"
#include "Tactics/ArcTacticsRecordingJson.h"
#include "Tactics/ArcTacticsTraceLoader.h"
#include "Tactics/ArcTacticsTraceRecorder.h"

namespace ArcMCPToolsetTacticsPrivate
{
	FString MakeFailure(const FString& Message)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), Message);
		return ArcMCPToolsetPrivate::JsonObjToString(Result);
	}

	bool ReadNumber(const TSharedPtr<FJsonValue>& Value, const FString& Name, double& OutNumber, FString& OutError)
	{
		if (!Value.IsValid() || Value->Type != EJson::Number || !Value->TryGetNumber(OutNumber))
		{
			OutError = FString::Printf(TEXT("Option '%s' must be a number."), *Name);
			return false;
		}
		return true;
	}

	bool ReadUInt32(const TSharedPtr<FJsonValue>& Value, const FString& Name, uint32& OutNumber, FString& OutError)
	{
		double Number = 0.0;
		if (!ReadNumber(Value, Name, Number, OutError))
		{
			return false;
		}
		if (Number < 0.0 || Number > static_cast<double>(MAX_uint32) || FMath::Frac(Number) != 0.0)
		{
			OutError = FString::Printf(TEXT("Option '%s' must be a non-negative integer."), *Name);
			return false;
		}
		OutNumber = static_cast<uint32>(Number);
		return true;
	}

	bool ParseEntity(const TSharedPtr<FJsonValue>& Value, FArcTacticsRecordingJsonOptions& OutOptions, FString& OutError)
	{
		const TSharedPtr<FJsonObject>* Entity = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Entity) || Entity == nullptr || !Entity->IsValid())
		{
			OutError = TEXT("Option 'entity' must be an object {\"index\": N, \"serial\": S}.");
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : (*Entity)->Values)
		{
			uint32 Number = 0;
			if (Field.Key == TEXT("index"))
			{
				if (!ReadUInt32(Field.Value, TEXT("entity.index"), Number, OutError))
				{
					return false;
				}
				OutOptions.EntityIndex = Number;
			}
			else if (Field.Key == TEXT("serial"))
			{
				if (!ReadUInt32(Field.Value, TEXT("entity.serial"), Number, OutError))
				{
					return false;
				}
				OutOptions.EntitySerial = Number;
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown option 'entity.%s'. Allowed: index, serial."), *Field.Key);
				return false;
			}
		}
		if (!OutOptions.EntityIndex.IsSet())
		{
			OutError = TEXT("Option 'entity' requires 'index'.");
			return false;
		}
		return true;
	}

	bool ParseOptions(const FString& OptionsJson, FArcTacticsRecordingJsonOptions& OutOptions, FString& OutError)
	{
		if (OptionsJson.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}
		const TSharedPtr<FJsonObject> Options = ArcMCPToolsetPrivate::StringToJsonObj(OptionsJson);
		if (!Options.IsValid())
		{
			OutError = TEXT("OptionsJson is not a valid JSON object.");
			return false;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Field : Options->Values)
		{
			if (Field.Key == TEXT("entity"))
			{
				if (!ParseEntity(Field.Value, OutOptions, OutError))
				{
					return false;
				}
			}
			else if (Field.Key == TEXT("fromTime") || Field.Key == TEXT("toTime"))
			{
				double Time = 0.0;
				if (!ReadNumber(Field.Value, Field.Key, Time, OutError))
				{
					return false;
				}
				if (Field.Key == TEXT("fromTime"))
				{
					OutOptions.FromTime = Time;
				}
				else
				{
					OutOptions.ToTime = Time;
				}
			}
			else if (Field.Key == TEXT("queryItems"))
			{
				FString Mode;
				if (!Field.Value.IsValid() || Field.Value->Type != EJson::String || !Field.Value->TryGetString(Mode))
				{
					OutError = TEXT("Option 'queryItems' must be \"all\", \"valid\" or \"none\".");
					return false;
				}
				if (Mode.Equals(TEXT("all"), ESearchCase::IgnoreCase))
				{
					OutOptions.QueryItems = EArcTacticsRecordingQueryItems::All;
				}
				else if (Mode.Equals(TEXT("valid"), ESearchCase::IgnoreCase))
				{
					OutOptions.QueryItems = EArcTacticsRecordingQueryItems::Valid;
				}
				else if (Mode.Equals(TEXT("none"), ESearchCase::IgnoreCase))
				{
					OutOptions.QueryItems = EArcTacticsRecordingQueryItems::None;
				}
				else
				{
					OutError = FString::Printf(TEXT("Option 'queryItems' is '%s'; expected \"all\", \"valid\" or \"none\"."), *Mode);
					return false;
				}
			}
			else
			{
				OutError = FString::Printf(TEXT("Unknown option '%s'. Allowed: entity, fromTime, toTime, queryItems."), *Field.Key);
				return false;
			}
		}
		if (OutOptions.FromTime.IsSet() && OutOptions.ToTime.IsSet() && OutOptions.FromTime.GetValue() > OutOptions.ToTime.GetValue())
		{
			OutError = TEXT("Option 'fromTime' is greater than 'toTime'.");
			return false;
		}
		return true;
	}

	bool HasMatchingEntity(const FArcTacticsRecording& Recording, const FArcTacticsRecordingJsonOptions& Options)
	{
		if (!Options.EntityIndex.IsSet())
		{
			return true;
		}
		return Recording.Entities.ContainsByPredicate([&Options](const FArcTacticsRecordedEntity& Entity)
		{
			return Entity.EntityIndex == Options.EntityIndex.GetValue()
				&& (!Options.EntitySerial.IsSet() || Entity.EntitySerial == Options.EntitySerial.GetValue());
		});
	}
}

FString UArcMCPToolset::ExportTacticsRecording(const FString& TraceFilePath, const FString& OptionsJson)
{
	using namespace ArcMCPToolsetTacticsPrivate;

	FArcTacticsRecordingJsonOptions Options;
	FString Error;
	if (!ParseOptions(OptionsJson, Options, Error))
	{
		return MakeFailure(Error);
	}

	FString InputPath;
	if (TraceFilePath.TrimStartAndEnd().IsEmpty())
	{
		InputPath = FArcTacticsTraceRecorder::FindNewestTraceFile();
		if (InputPath.IsEmpty())
		{
			return MakeFailure(FString::Printf(TEXT("No .utrace in %s."), *FArcTacticsTraceRecorder::GetTraceDirectory()));
		}
	}
	else
	{
		InputPath = FPaths::ConvertRelativePathToFull(TraceFilePath);
	}
	if (!FPaths::FileExists(InputPath))
	{
		return MakeFailure(FString::Printf(TEXT("Trace file not found: %s"), *InputPath));
	}

	const TUniquePtr<FArcTacticsRecording> Recording = FArcTacticsTraceLoader::LoadBlocking(InputPath, Error);
	if (!Recording.IsValid())
	{
		return MakeFailure(Error);
	}
	if (!HasMatchingEntity(*Recording, Options))
	{
		return MakeFailure(Options.EntitySerial.IsSet()
			? FString::Printf(TEXT("No entity with index %u and serial %u in %s."), Options.EntityIndex.GetValue(), Options.EntitySerial.GetValue(), *InputPath)
			: FString::Printf(TEXT("No entity with index %u in %s."), Options.EntityIndex.GetValue(), *InputPath));
	}

	const FArcTacticsRecordingJsonResult Result = ArcTacticsRecordingJson::Write(*Recording, Options);
	const FString OutputPath = FPaths::ChangeExtension(InputPath, TEXT("json"));
	if (!FFileHelper::SaveStringToFile(Result.Json, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		return MakeFailure(FString::Printf(TEXT("Cannot write %s"), *OutputPath));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("input"), InputPath);
	Response->SetStringField(TEXT("output"), OutputPath);
	Response->SetNumberField(TEXT("sizeBytes"), static_cast<double>(IFileManager::Get().FileSize(*OutputPath)));
	Response->SetObjectField(TEXT("summary"), Result.Summary);
	return ArcMCPToolsetPrivate::JsonObjToString(Response);
}
