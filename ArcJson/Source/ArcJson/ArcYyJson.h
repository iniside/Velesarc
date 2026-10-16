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

#pragma once

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"

THIRD_PARTY_INCLUDES_START
#include "yyjson/yyjson.h"
THIRD_PARTY_INCLUDES_END

namespace ArcJson
{
	struct ARCJSON_API FDocDeleter
	{
		void operator()(yyjson_doc* Doc) const;
	};

	struct ARCJSON_API FMutDocDeleter
	{
		void operator()(yyjson_mut_doc* Doc) const;
	};

	using FDocPtr = TUniquePtr<yyjson_doc, FDocDeleter>;
	using FMutDocPtr = TUniquePtr<yyjson_mut_doc, FMutDocDeleter>;

	ARCJSON_API FDocPtr Parse(const TArray<uint8>& Bytes, FString* OutError = nullptr);
	ARCJSON_API FDocPtr Parse(FStringView Text, FString* OutError = nullptr);
	ARCJSON_API FDocPtr Parse(const ANSICHAR* Data, SIZE_T Length, FString* OutError = nullptr);
	// Bytes gains YYJSON_PADDING_SIZE zero bytes and is rewritten in place; it must outlive the returned document.
	ARCJSON_API FDocPtr ParseInSitu(TArray<uint8>& Bytes, FString* OutError = nullptr);

	ARCJSON_API TArray<uint8> WritePretty(const yyjson_mut_doc* Doc);
	ARCJSON_API FString WritePrettyString(const yyjson_mut_doc* Doc);

	ARCJSON_API FString ToFString(const yyjson_val* Val);
	ARCJSON_API yyjson_mut_val* CopyString(yyjson_mut_doc* Doc, FStringView Text);
	ARCJSON_API yyjson_mut_val* CopyName(yyjson_mut_doc* Doc, FName Name);

	ARCJSON_API bool GetString(const yyjson_val* Obj, const ANSICHAR* Key, FString& OutValue);
	ARCJSON_API bool GetFloat(const yyjson_val* Obj, const ANSICHAR* Key, float& OutValue);
	ARCJSON_API bool GetInt(const yyjson_val* Obj, const ANSICHAR* Key, int32& OutValue);
	ARCJSON_API bool GetBool(const yyjson_val* Obj, const ANSICHAR* Key, bool& OutValue);
}
