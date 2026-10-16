/**
 * This file is part of Velesarc
 * Copyright (C) 2025-2025 Lukasz Baran
 *
 * Licensed under the European Union Public License (EUPL), Version 1.2 or –
 * as soon as they will be approved by the European Commission – later versions
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

#include "ArcYyJson.h"

#include "Containers/StringConv.h"
#include "Logging/LogMacros.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/StringBuilder.h"

#include <stdlib.h>

DEFINE_LOG_CATEGORY_STATIC(LogArcJson, Log, All);

namespace ArcYyJsonPrivate
{
	const yyjson_write_flag PrettyWriteFlags = YYJSON_WRITE_PRETTY_TWO_SPACES | YYJSON_WRITE_NEWLINE_AT_END | YYJSON_WRITE_INF_AND_NAN_AS_NULL;

	FString FormatReadError(const ANSICHAR* Data, SIZE_T Length, const yyjson_read_err& Error)
	{
		const ANSICHAR* Message = Error.msg != nullptr ? Error.msg : "unknown error";
		size_t Line = 0;
		size_t Column = 0;
		size_t Character = 0;
		if (yyjson_locate_pos(Data, Length, Error.pos, &Line, &Column, &Character))
		{
			return FString::Printf(TEXT("%s (line %llu, column %llu)"), StringCast<TCHAR>(Message).Get(), static_cast<uint64>(Line), static_cast<uint64>(Column));
		}
		return FString::Printf(TEXT("%s (byte %llu)"), StringCast<TCHAR>(Message).Get(), static_cast<uint64>(Error.pos));
	}

	char* WriteRaw(const yyjson_mut_doc* Doc, int32& OutLength)
	{
		OutLength = 0;
		size_t Length = 0;
		yyjson_write_err Error;
		char* Json = yyjson_mut_write_opts(Doc, PrettyWriteFlags, nullptr, &Length, &Error);
		if (Json == nullptr)
		{
			UE_LOGF(LogArcJson, Error, "yyjson write failed, code %u: %s", static_cast<uint32>(Error.code), Error.msg != nullptr ? Error.msg : "unknown error");
			return nullptr;
		}
		if (Length > static_cast<size_t>(MAX_int32))
		{
			UE_LOGF(LogArcJson, Error, "yyjson write produced %llu bytes, above the int32 buffer limit", static_cast<uint64>(Length));
			free(Json);
			return nullptr;
		}
		OutLength = static_cast<int32>(Length);
		return Json;
	}
}

namespace ArcJson
{
	void FDocDeleter::operator()(yyjson_doc* Doc) const
	{
		yyjson_doc_free(Doc);
	}

	void FMutDocDeleter::operator()(yyjson_mut_doc* Doc) const
	{
		yyjson_mut_doc_free(Doc);
	}

	FDocPtr Parse(const TArray<uint8>& Bytes, FString* OutError)
	{
		return Parse(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), static_cast<SIZE_T>(Bytes.Num()), OutError);
	}

	FDocPtr Parse(FStringView Text, FString* OutError)
	{
		const FTCHARToUTF8 Utf8(Text.GetData(), Text.Len());
		return Parse(reinterpret_cast<const ANSICHAR*>(Utf8.Get()), static_cast<SIZE_T>(Utf8.Length()), OutError);
	}

	FDocPtr Parse(const ANSICHAR* Data, SIZE_T Length, FString* OutError)
	{
		yyjson_read_err Error;
		yyjson_doc* Doc = yyjson_read_opts(const_cast<ANSICHAR*>(Data), Length, YYJSON_READ_NOFLAG, nullptr, &Error);
		if (Doc == nullptr && OutError != nullptr)
		{
			*OutError = ArcYyJsonPrivate::FormatReadError(Data, Length, Error);
		}
		return FDocPtr(Doc);
	}

	FDocPtr ParseInSitu(TArray<uint8>& Bytes, FString* OutError)
	{
		const int32 Length = Bytes.Num();
		Bytes.AddZeroed(YYJSON_PADDING_SIZE);
		ANSICHAR* Data = reinterpret_cast<ANSICHAR*>(Bytes.GetData());
		yyjson_read_err Error;
		yyjson_doc* Doc = yyjson_read_opts(Data, static_cast<size_t>(Length), YYJSON_READ_INSITU | YYJSON_READ_ALLOW_BOM, nullptr, &Error);
		if (Doc == nullptr && OutError != nullptr)
		{
			*OutError = ArcYyJsonPrivate::FormatReadError(Data, static_cast<SIZE_T>(Length), Error);
		}
		return FDocPtr(Doc);
	}

	TArray<uint8> WritePretty(const yyjson_mut_doc* Doc)
	{
		TArray<uint8> Bytes;
		int32 Length = 0;
		char* Json = ArcYyJsonPrivate::WriteRaw(Doc, Length);
		if (Json != nullptr)
		{
			Bytes.Append(reinterpret_cast<const uint8*>(Json), Length);
			free(Json);
		}
		return Bytes;
	}

	FString WritePrettyString(const yyjson_mut_doc* Doc)
	{
		int32 Length = 0;
		char* Json = ArcYyJsonPrivate::WriteRaw(Doc, Length);
		if (Json == nullptr)
		{
			return FString();
		}
		FString Text = FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(Json), Length);
		free(Json);
		return Text;
	}

	FString ToFString(const yyjson_val* Val)
	{
		const char* Str = yyjson_get_str(Val);
		if (Str == nullptr)
		{
			return FString();
		}
		return FString::ConstructFromPtrSize(reinterpret_cast<const UTF8CHAR*>(Str), static_cast<int32>(yyjson_get_len(Val)));
	}

	yyjson_mut_val* CopyString(yyjson_mut_doc* Doc, FStringView Text)
	{
		if (Text.IsEmpty())
		{
			return yyjson_mut_strncpy(Doc, "", 0);
		}
		const FTCHARToUTF8 Utf8(Text.GetData(), Text.Len());
		return yyjson_mut_strncpy(Doc, reinterpret_cast<const char*>(Utf8.Get()), static_cast<size_t>(Utf8.Length()));
	}

	yyjson_mut_val* CopyName(yyjson_mut_doc* Doc, FName Name)
	{
		TUtf8StringBuilder<FName::StringBufferSize> Builder;
		Name.AppendString(Builder);
		return yyjson_mut_strncpy(Doc, reinterpret_cast<const char*>(Builder.GetData()), static_cast<size_t>(Builder.Len()));
	}

	bool GetString(const yyjson_val* Obj, const ANSICHAR* Key, FString& OutValue)
	{
		const yyjson_val* Val = yyjson_obj_get(Obj, Key);
		if (!yyjson_is_str(Val))
		{
			return false;
		}
		OutValue = ToFString(Val);
		return true;
	}

	bool GetFloat(const yyjson_val* Obj, const ANSICHAR* Key, float& OutValue)
	{
		const yyjson_val* Val = yyjson_obj_get(Obj, Key);
		if (!yyjson_is_num(Val))
		{
			return false;
		}
		OutValue = static_cast<float>(yyjson_get_num(Val));
		return true;
	}

	bool GetInt(const yyjson_val* Obj, const ANSICHAR* Key, int32& OutValue)
	{
		const yyjson_val* Val = yyjson_obj_get(Obj, Key);
		if (yyjson_is_int(Val))
		{
			OutValue = static_cast<int32>(yyjson_get_sint(Val));
			return true;
		}
		if (yyjson_is_real(Val))
		{
			OutValue = static_cast<int32>(FMath::Clamp(yyjson_get_real(Val), static_cast<double>(MIN_int32), static_cast<double>(MAX_int32)));
			return true;
		}
		return false;
	}

	bool GetBool(const yyjson_val* Obj, const ANSICHAR* Key, bool& OutValue)
	{
		const yyjson_val* Val = yyjson_obj_get(Obj, Key);
		if (!yyjson_is_bool(Val))
		{
			return false;
		}
		OutValue = yyjson_get_bool(Val);
		return true;
	}
}
