// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Rml
{
	class DataTypeRegister;
	class FileInterface;
	class TransformFuncRegister;
}

struct FArcRmlContentRoots;

namespace ArcRmlComponents
{
	inline const TCHAR* const EmptyArrayName = TEXT("ArcEmpty");
	inline const ANSICHAR* const IconDirectory = "/Game/UI/Rml/icons/";

	void RegisterTransforms(Rml::TransformFuncRegister& Register);
}

#if WITH_EDITOR
class FArcRmlComponentExpander
{
public:
	FArcRmlComponentExpander(Rml::FileInterface& InFileInterface, const FArcRmlContentRoots& InRoots);
	~FArcRmlComponentExpander();

	bool ExpandDocument(const FString& DocumentPath, FString& OutText);

private:
	struct FComponent;
	struct FCursor;
	struct FExpandState;
	struct FScope;
	struct FSlot;

	const FComponent* FindComponent(const FString& Name, const FString& File, int32 Line);
	TUniquePtr<FComponent> LoadComponent(const FString& Path, const FString& Text);
	bool ReadFile(const FString& Path, FString& OutText);
	bool FileExists(const FString& Path);

	void Expand(FStringView Text, int32 FirstLine, const FScope& Scope, FExpandState& State, FString& Out);
	bool ExpandTokens(FCursor& Cursor, const FScope& Scope, FExpandState& State, FString& Out, bool bUntilImportClose);
	void ExpandImport(FCursor& Cursor, const FScope& Scope, bool bInModel, FExpandState& State, FString& Out);
	void SkipElement(FCursor& Cursor, FStringView Name, bool bMarkUsed, const FString& File, FExpandState& State);
	void MarkUsed(const FString& Name, const FString& File, int32 Line, FExpandState& State);
	bool EvaluateConstantCondition(FStringView RawValue, bool& bOutValue);

	Rml::FileInterface& FileInterface;
	const FArcRmlContentRoots& Roots;
	TUniquePtr<Rml::DataTypeRegister> TypeRegister;
	TMap<FString, TUniquePtr<FComponent>> Components;
	int32 ErrorCount = 0;
};
#endif
