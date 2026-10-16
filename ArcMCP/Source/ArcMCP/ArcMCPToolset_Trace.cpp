// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/MiscTrace.h"

#include "TraceServices/AnalysisService.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/ModuleService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/Bookmarks.h"
#include "TraceServices/Model/Counters.h"
#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/Log.h"

namespace ArcMCPToolsetTracePrivate
{
	const TCHAR* SafeString(const TCHAR* Value)
	{
		return Value != nullptr ? Value : TEXT("");
	}

	TSharedPtr<FJsonObject> GetSubObject(const TSharedPtr<FJsonObject>& Owner, const TCHAR* FieldName)
	{
		if (!Owner.IsValid())
		{
			return nullptr;
		}

		const TSharedPtr<FJsonObject>* Found = nullptr;
		if (Owner->TryGetObjectField(FieldName, Found) && Found != nullptr)
		{
			return *Found;
		}

		return nullptr;
	}

	FString GetStringOr(const TSharedPtr<FJsonObject>& Owner, const TCHAR* FieldName, const FString& DefaultValue)
	{
		FString Value;
		if (Owner.IsValid() && Owner->TryGetStringField(FieldName, Value))
		{
			return Value;
		}

		return DefaultValue;
	}

	double GetNumberOr(const TSharedPtr<FJsonObject>& Owner, const TCHAR* FieldName, double DefaultValue)
	{
		double Value = 0.0;
		if (Owner.IsValid() && Owner->TryGetNumberField(FieldName, Value))
		{
			return Value;
		}

		return DefaultValue;
	}

	int32 GetIntOr(const TSharedPtr<FJsonObject>& Owner, const TCHAR* FieldName, int32 DefaultValue)
	{
		int32 Value = 0;
		if (Owner.IsValid() && Owner->TryGetNumberField(FieldName, Value))
		{
			return Value;
		}

		return DefaultValue;
	}

	bool GetBoolOr(const TSharedPtr<FJsonObject>& Owner, const TCHAR* FieldName, bool bDefaultValue)
	{
		bool bValue = false;
		if (Owner.IsValid() && Owner->TryGetBoolField(FieldName, bValue))
		{
			return bValue;
		}

		return bDefaultValue;
	}

	bool TryParseVerbosity(const FString& Text, ELogVerbosity::Type& OutVerbosity)
	{
		static const TCHAR* Names[] =
		{
			TEXT("Fatal"), TEXT("Error"), TEXT("Warning"), TEXT("Display"),
			TEXT("Log"), TEXT("Verbose"), TEXT("VeryVerbose")
		};
		static const ELogVerbosity::Type Values[] =
		{
			ELogVerbosity::Fatal, ELogVerbosity::Error, ELogVerbosity::Warning, ELogVerbosity::Display,
			ELogVerbosity::Log, ELogVerbosity::Verbose, ELogVerbosity::VeryVerbose
		};

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
		{
			if (Text.Equals(Names[Index], ESearchCase::IgnoreCase))
			{
				OutVerbosity = Values[Index];
				return true;
			}
		}

		return false;
	}

	bool TryParseFrameType(const FString& Text, ETraceFrameType& OutFrameType)
	{
		if (Text.Equals(TEXT("game"), ESearchCase::IgnoreCase))
		{
			OutFrameType = TraceFrameType_Game;
			return true;
		}
		if (Text.Equals(TEXT("rendering"), ESearchCase::IgnoreCase))
		{
			OutFrameType = TraceFrameType_Rendering;
			return true;
		}

		return false;
	}

	TSharedPtr<FJsonObject> MakeUnavailable(const FString& Reason)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetBoolField(TEXT("available"), false);
		Object->SetStringField(TEXT("reason"), Reason);
		return Object;
	}

	TSharedPtr<FJsonObject> MakeFrameTypeJson(
		const TraceServices::IFrameProvider& FrameProvider,
		ETraceFrameType FrameType,
		double RangeStart,
		double RangeEnd,
		bool bIncludeFrames,
		int32 MaxResults)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("totalCount"), static_cast<double>(FrameProvider.GetFrameCount(FrameType)));

		int32 MatchedCount = 0;
		TArray<TSharedPtr<FJsonValue>> Frames;

		FrameProvider.EnumerateFrames(FrameType, RangeStart, RangeEnd,
			[&MatchedCount, &Frames, bIncludeFrames, MaxResults](const TraceServices::FFrame& Frame)
			{
				++MatchedCount;
				if (!bIncludeFrames || Frames.Num() >= MaxResults)
				{
					return;
				}

				TSharedPtr<FJsonObject> FrameObject = MakeShared<FJsonObject>();
				FrameObject->SetNumberField(TEXT("index"), static_cast<double>(Frame.Index));
				FrameObject->SetNumberField(TEXT("startTime"), Frame.StartTime);
				FrameObject->SetNumberField(TEXT("endTime"), Frame.EndTime);
				FrameObject->SetNumberField(TEXT("durationMs"), (Frame.EndTime - Frame.StartTime) * 1000.0);
				Frames.Add(MakeShared<FJsonValueObject>(FrameObject));
			});

		Object->SetNumberField(TEXT("matched"), MatchedCount);
		if (bIncludeFrames)
		{
			Object->SetArrayField(TEXT("frames"), Frames);
			Object->SetBoolField(TEXT("truncated"), MatchedCount > Frames.Num());
		}

		return Object;
	}
}

FString UArcMCPToolset::QueryTrace(const FString& TraceFilePath, const FString& QueryJson)
{
	using namespace ArcMCPToolsetTracePrivate;

	if (TraceFilePath.IsEmpty())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("TraceFilePath is empty."));
	}

	const FString FullPath = FPaths::ConvertRelativePathToFull(TraceFilePath);
	if (!FPaths::FileExists(FullPath))
	{
		return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Trace file not found: %s"), *FullPath));
	}

	TArray<TSharedPtr<FJsonValue>> Warnings;

	TSharedPtr<FJsonObject> Query;
	if (!QueryJson.IsEmpty())
	{
		Query = ArcMCPToolsetPrivate::StringToJsonObj(QueryJson);
		if (!Query.IsValid())
		{
			return ArcMCPToolsetPrivate::MakeError(TEXT("QueryJson is not a valid JSON object."));
		}
	}

	TSet<FString> Sections;
	const TArray<TSharedPtr<FJsonValue>>* SectionValues = nullptr;
	if (Query.IsValid() && Query->TryGetArrayField(TEXT("sections"), SectionValues) && SectionValues != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& SectionValue : *SectionValues)
		{
			Sections.Add(SectionValue->AsString().ToLower());
		}
	}
	else
	{
		Sections.Append({TEXT("logs"), TEXT("counters"), TEXT("bookmarks"), TEXT("frames")});
	}

	const bool bWantLogs = Sections.Contains(TEXT("logs"));
	const bool bWantCategories = Sections.Contains(TEXT("categories"));
	const bool bWantCounters = Sections.Contains(TEXT("counters"));
	const bool bWantBookmarks = Sections.Contains(TEXT("bookmarks"));
	const bool bWantFrames = Sections.Contains(TEXT("frames"));

	const int32 MaxResults = FMath::Max(1, GetIntOr(Query, TEXT("maxResults"), 200));

	TArray<TSharedPtr<FJsonValue>> LoadedModules;
	static const TCHAR* AnalyzerHostModules[] =
	{
		TEXT("TraceServices"), TEXT("MassDeveloper"), TEXT("ArcAIEditor"), TEXT("ArcMassAbilitiesEditor")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(AnalyzerHostModules); ++Index)
	{
		const FName ModuleName(AnalyzerHostModules[Index]);
		const bool bWasAlreadyLoaded = FModuleManager::Get().IsModuleLoaded(ModuleName);
		if (!bWasAlreadyLoaded)
		{
			FModuleManager::Get().LoadModule(ModuleName);
		}

		TSharedPtr<FJsonObject> ModuleObject = MakeShared<FJsonObject>();
		ModuleObject->SetStringField(TEXT("name"), AnalyzerHostModules[Index]);
		ModuleObject->SetBoolField(TEXT("loaded"), FModuleManager::Get().IsModuleLoaded(ModuleName));
		ModuleObject->SetBoolField(TEXT("wasAlreadyLoaded"), bWasAlreadyLoaded);
		LoadedModules.Add(MakeShared<FJsonValueObject>(ModuleObject));
	}

	ITraceServicesModule& TraceServicesModule = FModuleManager::LoadModuleChecked<ITraceServicesModule>(TEXT("TraceServices"));

	TSharedPtr<TraceServices::IModuleService> ModuleService = TraceServicesModule.GetModuleService();
	TArray<TSharedPtr<FJsonValue>> AnalysisModules;
	TSet<FName> AvailableModuleNames;
	TSet<FName> EnabledModuleNames;
	if (ModuleService.IsValid())
	{
		TArray<TraceServices::FModuleInfoEx> ModuleInfos;
		ModuleService->GetAvailableModulesEx(ModuleInfos);
		for (const TraceServices::FModuleInfoEx& ModuleInfo : ModuleInfos)
		{
			AvailableModuleNames.Add(ModuleInfo.Info.Name);
			if (ModuleInfo.bIsEnabled)
			{
				EnabledModuleNames.Add(ModuleInfo.Info.Name);
			}

			TSharedPtr<FJsonObject> ModuleObject = MakeShared<FJsonObject>();
			ModuleObject->SetStringField(TEXT("name"), ModuleInfo.Info.Name.ToString());
			ModuleObject->SetStringField(TEXT("displayName"), SafeString(ModuleInfo.Info.DisplayName));
			ModuleObject->SetBoolField(TEXT("enabled"), ModuleInfo.bIsEnabled);
			AnalysisModules.Add(MakeShared<FJsonValueObject>(ModuleObject));
		}
	}

	TArray<FString> ExpectedModules;
	if (bWantCounters)
	{
		ExpectedModules.Add(TEXT("TraceModule_Counters"));
	}
	const TArray<TSharedPtr<FJsonValue>>* ExpectValues = nullptr;
	if (Query.IsValid() && Query->TryGetArrayField(TEXT("expectModules"), ExpectValues) && ExpectValues != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& ExpectValue : *ExpectValues)
		{
			ExpectedModules.AddUnique(ExpectValue->AsString());
		}
	}

	TArray<TSharedPtr<FJsonValue>> MissingModules;
	for (const FString& ExpectedModule : ExpectedModules)
	{
		const FName ExpectedName(*ExpectedModule);
		if (EnabledModuleNames.Contains(ExpectedName))
		{
			continue;
		}

		MissingModules.Add(MakeShared<FJsonValueString>(ExpectedModule));
		if (AvailableModuleNames.Contains(ExpectedName))
		{
			Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
				TEXT("Analysis module '%s' is registered but not enabled: no analyzer was instantiated for it, so results depending on it are empty for that reason, not because the trace has no events."),
				*ExpectedModule)));
		}
		else
		{
			Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
				TEXT("Analysis module '%s' is not registered: results depending on it are empty because its analyzer never ran, not because the trace has no events."),
				*ExpectedModule)));
		}
	}

	TSharedPtr<TraceServices::IAnalysisService> AnalysisService = TraceServicesModule.GetAnalysisService();
	if (!AnalysisService.IsValid())
	{
		return ArcMCPToolsetPrivate::MakeError(TEXT("TraceServices returned no analysis service."));
	}

	TSharedPtr<const TraceServices::IAnalysisSession> Session = AnalysisService->StartAnalysis(*FullPath);
	if (!Session.IsValid())
	{
		return ArcMCPToolsetPrivate::MakeError(FString::Printf(TEXT("Failed to open trace file for analysis: %s"), *FullPath));
	}
	Session->Wait();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	TSharedPtr<FJsonObject> ModulesObject = MakeShared<FJsonObject>();
	ModulesObject->SetArrayField(TEXT("forceLoaded"), LoadedModules);
	ModulesObject->SetArrayField(TEXT("analysis"), AnalysisModules);
	ModulesObject->SetArrayField(TEXT("missing"), MissingModules);
	Result->SetObjectField(TEXT("modules"), ModulesObject);

	TraceServices::FAnalysisSessionReadScope ReadScope(*Session);

	const double SessionDuration = Session->GetDurationSeconds();
	const double RangeStart = GetNumberOr(Query, TEXT("startTime"), 0.0);
	const double RequestedEnd = GetNumberOr(Query, TEXT("endTime"), 0.0);
	const double RangeEnd = RequestedEnd > RangeStart ? RequestedEnd : SessionDuration;

	const uint32 PendingMessageCount = Session->GetNumPendingMessages();
	if (PendingMessageCount > 0)
	{
		Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
			TEXT("Analysis reported %u message(s) (errors/warnings such as an unrecognized or truncated trace stream); empty sections may mean the file was never parsed as a trace, not that it has no events."),
			PendingMessageCount)));
	}

	TSharedPtr<FJsonObject> TraceObject = MakeShared<FJsonObject>();
	TraceObject->SetStringField(TEXT("path"), FullPath);
	TraceObject->SetStringField(TEXT("name"), SafeString(Session->GetName()));
	TraceObject->SetNumberField(TEXT("durationSeconds"), SessionDuration);
	TraceObject->SetBoolField(TEXT("analysisComplete"), Session->IsAnalysisComplete());
	TraceObject->SetNumberField(TEXT("analysisMessageCount"), static_cast<double>(PendingMessageCount));
	TraceObject->SetNumberField(TEXT("rangeStart"), RangeStart);
	TraceObject->SetNumberField(TEXT("rangeEnd"), RangeEnd);
	TraceObject->SetNumberField(TEXT("maxResults"), MaxResults);
	Result->SetObjectField(TEXT("trace"), TraceObject);

	if (bWantLogs || bWantCategories)
	{
		const TraceServices::ILogProvider* LogProvider =
			Session->ReadProvider<TraceServices::ILogProvider>(TraceServices::GetLogProviderName());

		if (LogProvider == nullptr)
		{
			if (bWantLogs)
			{
				Result->SetObjectField(TEXT("logs"), MakeUnavailable(TEXT("Log provider is not registered in this analysis session.")));
			}
			if (bWantCategories)
			{
				Result->SetObjectField(TEXT("categories"), MakeUnavailable(TEXT("Log provider is not registered in this analysis session.")));
			}
		}
		else
		{
			if (bWantLogs)
			{
				const TSharedPtr<FJsonObject> LogQuery = GetSubObject(Query, TEXT("logs"));
				const FString CategoryExact = GetStringOr(LogQuery, TEXT("category"), FString());
				const FString CategoryContains = GetStringOr(LogQuery, TEXT("categoryContains"), FString());
				const FString MessageContains = GetStringOr(LogQuery, TEXT("contains"), FString());
				const FString VerbosityText = GetStringOr(LogQuery, TEXT("minVerbosity"), FString());

				ELogVerbosity::Type MinVerbosity = ELogVerbosity::VeryVerbose;
				if (!VerbosityText.IsEmpty() && !TryParseVerbosity(VerbosityText, MinVerbosity))
				{
					Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
						TEXT("Unknown minVerbosity '%s'; verbosity filter ignored."), *VerbosityText)));
				}

				int32 MatchedCount = 0;
				TArray<TSharedPtr<FJsonValue>> Messages;

				LogProvider->EnumerateMessages(RangeStart, RangeEnd,
					[&MatchedCount, &Messages, &CategoryExact, &CategoryContains, &MessageContains, MinVerbosity, MaxResults]
					(const TraceServices::FLogMessageInfo& Message)
					{
						const FString CategoryName = Message.Category != nullptr ? FString(SafeString(Message.Category->Name)) : FString();

						if (!CategoryExact.IsEmpty() && !CategoryName.Equals(CategoryExact, ESearchCase::IgnoreCase))
						{
							return;
						}
						if (!CategoryContains.IsEmpty() && !CategoryName.Contains(CategoryContains))
						{
							return;
						}
						if (Message.Verbosity > MinVerbosity)
						{
							return;
						}

						const FString MessageText = FString(SafeString(Message.Message));
						if (!MessageContains.IsEmpty() && !MessageText.Contains(MessageContains))
						{
							return;
						}

						++MatchedCount;
						if (Messages.Num() >= MaxResults)
						{
							return;
						}

						TSharedPtr<FJsonObject> MessageObject = MakeShared<FJsonObject>();
						MessageObject->SetNumberField(TEXT("index"), static_cast<double>(Message.Index));
						MessageObject->SetNumberField(TEXT("time"), Message.Time);
						MessageObject->SetStringField(TEXT("category"), CategoryName);
						MessageObject->SetStringField(TEXT("verbosity"), ToString(Message.Verbosity));
						MessageObject->SetStringField(TEXT("file"), SafeString(Message.File));
						MessageObject->SetNumberField(TEXT("line"), Message.Line);
						MessageObject->SetStringField(TEXT("message"), MessageText);
						Messages.Add(MakeShared<FJsonValueObject>(MessageObject));
					});

				TSharedPtr<FJsonObject> LogsObject = MakeShared<FJsonObject>();
				LogsObject->SetBoolField(TEXT("available"), true);
				LogsObject->SetNumberField(TEXT("totalCount"), static_cast<double>(LogProvider->GetMessageCount()));
				LogsObject->SetNumberField(TEXT("matched"), MatchedCount);
				LogsObject->SetBoolField(TEXT("truncated"), MatchedCount > Messages.Num());
				LogsObject->SetArrayField(TEXT("messages"), Messages);
				Result->SetObjectField(TEXT("logs"), LogsObject);
			}

			if (bWantCategories)
			{
				TArray<TSharedPtr<FJsonValue>> Categories;
				LogProvider->EnumerateCategories(
					[&Categories](const TraceServices::FLogCategoryInfo& Category)
					{
						TSharedPtr<FJsonObject> CategoryObject = MakeShared<FJsonObject>();
						CategoryObject->SetStringField(TEXT("name"), SafeString(Category.Name));
						CategoryObject->SetStringField(TEXT("defaultVerbosity"), ToString(Category.DefaultVerbosity));
						Categories.Add(MakeShared<FJsonValueObject>(CategoryObject));
					});

				TSharedPtr<FJsonObject> CategoriesObject = MakeShared<FJsonObject>();
				CategoriesObject->SetBoolField(TEXT("available"), true);
				CategoriesObject->SetNumberField(TEXT("count"), Categories.Num());
				CategoriesObject->SetArrayField(TEXT("categories"), Categories);
				Result->SetObjectField(TEXT("categories"), CategoriesObject);
			}
		}
	}

	if (bWantCounters)
	{
		const TraceServices::ICounterProvider* CounterProvider =
			Session->ReadProvider<TraceServices::ICounterProvider>(TraceServices::GetCounterProviderName());

		if (CounterProvider == nullptr)
		{
			Result->SetObjectField(TEXT("counters"), MakeUnavailable(TEXT("Counter provider is not registered in this analysis session.")));
		}
		else
		{
			const TSharedPtr<FJsonObject> CounterQuery = GetSubObject(Query, TEXT("counters"));
			const FString NameContains = GetStringOr(CounterQuery, TEXT("nameContains"), FString());
			const bool bIncludeValues = GetBoolOr(CounterQuery, TEXT("includeValues"), false);
			const int32 MaxValuesPerCounter = FMath::Max(1, GetIntOr(CounterQuery, TEXT("maxValuesPerCounter"), 64));

			int32 MatchedCount = 0;
			TArray<TSharedPtr<FJsonValue>> Counters;

			CounterProvider->EnumerateCounters(
				[&MatchedCount, &Counters, &NameContains, bIncludeValues, MaxValuesPerCounter, MaxResults, RangeStart, RangeEnd]
				(uint32 CounterId, const TraceServices::ICounter& Counter)
				{
					const FString CounterName = FString(SafeString(Counter.GetName()));
					if (!NameContains.IsEmpty() && !CounterName.Contains(NameContains))
					{
						return;
					}

					++MatchedCount;
					if (Counters.Num() >= MaxResults)
					{
						return;
					}

					TSharedPtr<FJsonObject> CounterObject = MakeShared<FJsonObject>();
					CounterObject->SetNumberField(TEXT("id"), static_cast<double>(CounterId));
					CounterObject->SetStringField(TEXT("name"), CounterName);
					CounterObject->SetStringField(TEXT("group"), SafeString(Counter.GetGroup()));
					CounterObject->SetBoolField(TEXT("isFloatingPoint"), Counter.IsFloatingPoint());

					int32 ValueCount = 0;
					bool bHasValues = false;
					double MinValue = 0.0;
					double MaxValue = 0.0;
					double FirstTime = 0.0;
					double LastTime = 0.0;
					TArray<TSharedPtr<FJsonValue>> Values;

					TFunction<void(double, double)> Accumulate =
						[&ValueCount, &bHasValues, &MinValue, &MaxValue, &FirstTime, &LastTime, &Values, bIncludeValues, MaxValuesPerCounter]
						(double Time, double Value)
						{
							if (!bHasValues)
							{
								bHasValues = true;
								MinValue = Value;
								MaxValue = Value;
								FirstTime = Time;
							}
							MinValue = FMath::Min(MinValue, Value);
							MaxValue = FMath::Max(MaxValue, Value);
							LastTime = Time;
							++ValueCount;

							if (bIncludeValues && Values.Num() < MaxValuesPerCounter)
							{
								TSharedPtr<FJsonObject> SampleObject = MakeShared<FJsonObject>();
								SampleObject->SetNumberField(TEXT("time"), Time);
								SampleObject->SetNumberField(TEXT("value"), Value);
								Values.Add(MakeShared<FJsonValueObject>(SampleObject));
							}
						};

					if (Counter.IsFloatingPoint())
					{
						Counter.EnumerateFloatValues(RangeStart, RangeEnd, true,
							[&Accumulate](double Time, double Value)
							{
								Accumulate(Time, Value);
							});
					}
					else
					{
						Counter.EnumerateValues(RangeStart, RangeEnd, true,
							[&Accumulate](double Time, int64 Value)
							{
								Accumulate(Time, static_cast<double>(Value));
							});
					}

					CounterObject->SetNumberField(TEXT("valueCount"), ValueCount);
					if (bHasValues)
					{
						CounterObject->SetNumberField(TEXT("minValue"), MinValue);
						CounterObject->SetNumberField(TEXT("maxValue"), MaxValue);
						CounterObject->SetNumberField(TEXT("firstTime"), FirstTime);
						CounterObject->SetNumberField(TEXT("lastTime"), LastTime);
					}
					if (bIncludeValues)
					{
						CounterObject->SetArrayField(TEXT("values"), Values);
						CounterObject->SetBoolField(TEXT("valuesTruncated"), ValueCount > Values.Num());
					}

					Counters.Add(MakeShared<FJsonValueObject>(CounterObject));
				});

			TSharedPtr<FJsonObject> CountersObject = MakeShared<FJsonObject>();
			CountersObject->SetBoolField(TEXT("available"), true);
			CountersObject->SetBoolField(TEXT("analyzerModuleRegistered"), AvailableModuleNames.Contains(FName(TEXT("TraceModule_Counters"))));
			CountersObject->SetBoolField(TEXT("analyzerModuleEnabled"), EnabledModuleNames.Contains(FName(TEXT("TraceModule_Counters"))));
			CountersObject->SetNumberField(TEXT("totalCount"), static_cast<double>(CounterProvider->GetCounterCount()));
			CountersObject->SetNumberField(TEXT("matched"), MatchedCount);
			CountersObject->SetBoolField(TEXT("truncated"), MatchedCount > Counters.Num());
			CountersObject->SetArrayField(TEXT("counters"), Counters);
			Result->SetObjectField(TEXT("counters"), CountersObject);
		}
	}

	if (bWantBookmarks)
	{
		const TraceServices::IBookmarkProvider* BookmarkProvider =
			Session->ReadProvider<TraceServices::IBookmarkProvider>(TraceServices::GetBookmarkProviderName());

		if (BookmarkProvider == nullptr)
		{
			Result->SetObjectField(TEXT("bookmarks"), MakeUnavailable(TEXT("Bookmark provider is not registered in this analysis session.")));
		}
		else
		{
			const TSharedPtr<FJsonObject> BookmarkQuery = GetSubObject(Query, TEXT("bookmarks"));
			const FString TextContains = GetStringOr(BookmarkQuery, TEXT("contains"), FString());

			int32 MatchedCount = 0;
			TArray<TSharedPtr<FJsonValue>> Bookmarks;

			BookmarkProvider->EnumerateBookmarks(RangeStart, RangeEnd,
				[&MatchedCount, &Bookmarks, &TextContains, MaxResults](const TraceServices::FBookmark& Bookmark)
				{
					const FString BookmarkText = FString(SafeString(Bookmark.Text));
					if (!TextContains.IsEmpty() && !BookmarkText.Contains(TextContains))
					{
						return;
					}

					++MatchedCount;
					if (Bookmarks.Num() >= MaxResults)
					{
						return;
					}

					TSharedPtr<FJsonObject> BookmarkObject = MakeShared<FJsonObject>();
					BookmarkObject->SetNumberField(TEXT("time"), Bookmark.Time);
					BookmarkObject->SetStringField(TEXT("text"), BookmarkText);
					Bookmarks.Add(MakeShared<FJsonValueObject>(BookmarkObject));
				});

			TSharedPtr<FJsonObject> BookmarksObject = MakeShared<FJsonObject>();
			BookmarksObject->SetBoolField(TEXT("available"), true);
			BookmarksObject->SetNumberField(TEXT("totalCount"), static_cast<double>(BookmarkProvider->GetBookmarkCount()));
			BookmarksObject->SetNumberField(TEXT("matched"), MatchedCount);
			BookmarksObject->SetBoolField(TEXT("truncated"), MatchedCount > Bookmarks.Num());
			BookmarksObject->SetArrayField(TEXT("bookmarks"), Bookmarks);
			Result->SetObjectField(TEXT("bookmarks"), BookmarksObject);
		}
	}

	if (bWantFrames)
	{
		const TraceServices::IFrameProvider* FrameProvider =
			Session->ReadProvider<TraceServices::IFrameProvider>(TraceServices::GetFrameProviderName());

		if (FrameProvider == nullptr)
		{
			Result->SetObjectField(TEXT("frames"), MakeUnavailable(TEXT("Frame provider is not registered in this analysis session.")));
		}
		else
		{
			const TSharedPtr<FJsonObject> FrameQuery = GetSubObject(Query, TEXT("frames"));
			const bool bIncludeFrames = GetBoolOr(FrameQuery, TEXT("includeFrames"), false);
			const FString FrameTypeText = GetStringOr(FrameQuery, TEXT("frameType"), FString());

			ETraceFrameType SingleFrameType = TraceFrameType_Game;
			const bool bSingleType = !FrameTypeText.IsEmpty() && TryParseFrameType(FrameTypeText, SingleFrameType);
			if (!FrameTypeText.IsEmpty() && !bSingleType)
			{
				Warnings.Add(MakeShared<FJsonValueString>(FString::Printf(
					TEXT("Unknown frameType '%s'; expected 'game' or 'rendering'. Reporting both."), *FrameTypeText)));
			}

			TSharedPtr<FJsonObject> FramesObject = MakeShared<FJsonObject>();
			FramesObject->SetBoolField(TEXT("available"), true);

			if (!bSingleType || SingleFrameType == TraceFrameType_Game)
			{
				FramesObject->SetObjectField(TEXT("game"),
					MakeFrameTypeJson(*FrameProvider, TraceFrameType_Game, RangeStart, RangeEnd, bIncludeFrames, MaxResults));
			}
			if (!bSingleType || SingleFrameType == TraceFrameType_Rendering)
			{
				FramesObject->SetObjectField(TEXT("rendering"),
					MakeFrameTypeJson(*FrameProvider, TraceFrameType_Rendering, RangeStart, RangeEnd, bIncludeFrames, MaxResults));
			}

			Result->SetObjectField(TEXT("frames"), FramesObject);
		}
	}

	Result->SetArrayField(TEXT("warnings"), Warnings);

	return ArcMCPToolsetPrivate::JsonObjToString(Result);
}
