// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EArcRmlValueKind : uint8
{
	Empty,
	Bool,
	Int,
	Float,
	String,
	Colour,
	Array,
	Struct
};

struct FArcRmlValue
{
	EArcRmlValueKind Kind = EArcRmlValueKind::Empty;
	bool bBool = false;
	int64 Int = 0;
	double Float = 0.0;
	FString String;
	FColor Colour = FColor::Transparent;
	TArray<FArcRmlValue> Children;
	TArray<FString> MemberNames;
};

struct FArcRmlPathEntry
{
	FString Name;
	int32 Index = INDEX_NONE;
};

enum class EArcRmlListOpType : uint8
{
	Insert,
	Remove,
	Move,
	Update
};

struct FArcRmlListOp
{
	EArcRmlListOpType Type = EArcRmlListOpType::Insert;
	int32 Index = INDEX_NONE;
	int32 ToIndex = INDEX_NONE;
	FArcRmlValue Value;
};

struct FArcRmlViewModelCall
{
	FString ModelName;
	FString ChildPath;
	FString FunctionName;
	TArray<FArcRmlValue> Arguments;
	TMap<FString, FString> DragData;
	TMap<FString, FString> ElementData;
	bool bRoutedAction = false;
};
