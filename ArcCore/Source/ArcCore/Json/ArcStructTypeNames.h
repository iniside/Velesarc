// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UObjectGlobals.h"

namespace ArcStructTypeNames
{
	inline const TCHAR* const StructTypeKey = TEXT("_structType");

	template <typename TType>
	TType* FindType(const FString& InName)
	{
		const FString Name = InName.TrimStartAndEnd();
		if (Name.IsEmpty())
		{
			return nullptr;
		}
		if (Name.StartsWith(TEXT("/")))
		{
			if (TType* Found = FindObject<TType>(nullptr, *Name))
			{
				return Found;
			}
			return LoadObject<TType>(nullptr, *Name, FStringView(), LOAD_NoWarn | LOAD_Quiet);
		}
		if (TType* Found = FindFirstObject<TType>(*Name, EFindFirstObjectOptions::NativeFirst))
		{
			return Found;
		}
		const bool bHasTypePrefix = Name.Len() > 1
			&& (Name[0] == TEXT('F') || Name[0] == TEXT('U') || Name[0] == TEXT('A'))
			&& FChar::IsUpper(Name[1]);
		return bHasTypePrefix
			? FindFirstObject<TType>(*Name.RightChop(1), EFindFirstObjectOptions::NativeFirst)
			: nullptr;
	}

	template <typename TType>
	FString GetTypeName(const TType* Type)
	{
		if (!Type)
		{
			return FString();
		}
		const FString ShortName = Type->GetName();
		return FindType<TType>(ShortName) == Type ? ShortName : Type->GetPathName();
	}
}
