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



#include "ArcNamedPrimaryAssetId.h"

bool FArcNamedPrimaryAssetId::ExportTextItem(FString& ValueStr
	, FArcNamedPrimaryAssetId const& DefaultValue
	, UObject* Parent
	, int32 PortFlags
	, UObject* ExportRootScope) const
{
#if WITH_EDITORONLY_DATA
	if ((PortFlags & PPF_PropertyWindow) || (PortFlags & PPF_ForDiff))
	{
		ValueStr += AssetId.PrimaryAssetType.ToString() + ":" + DisplayName;
	}
	else if (!(PortFlags & PPF_Delimited))
	{
		ValueStr += AssetId.ToString();
	}
	else
	{
		ValueStr += FString::Printf(TEXT("\"%s\""), *AssetId.ToString());
	}
#else
	if (!(PortFlags & PPF_Delimited))
	{
		ValueStr += AssetId.ToString();
	}
	else
	{
		ValueStr += FString::Printf(TEXT("\"%s\""), *AssetId.ToString());
	}
#endif
	

	return true;
}

bool FArcNamedPrimaryAssetId::ImportTextItem(const TCHAR*& Buffer
	, int32 PortFlags
	, UObject* Parent
	, FOutputDevice* ErrorText)
{
	if (PortFlags & PPF_PropertyWindow)
	{
		return false;
	}
	
	// This handles both quoted and unquoted
	FString ImportedString = TEXT("");
	const TCHAR* NewBuffer = FPropertyHelpers::ReadToken(Buffer, ImportedString, 1);
	
	if (!NewBuffer)
	{
		return false;
	}

	AssetId = FPrimaryAssetId(ImportedString);
	
	Buffer = NewBuffer;

	return true;
}
