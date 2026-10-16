// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Decision/ArcTacticsBindings.h"
#include "Decision/ArcTacticsConfig.h"
#include "UObject/SoftObjectPath.h"

#include "ArcTacticsEditorClipboard.generated.h"

UENUM()
enum class EArcTacticsEditorItemKind : uint8
{
	Config,
	Context,
	GlobalTask,
	Action,
	Selector,
	SelectorAction,
	Step,
	ActiveStep,
	Consideration
};

struct FArcTacticsEditorSelection
{
	EArcTacticsEditorItemKind Kind = EArcTacticsEditorItemKind::Config;
	FGuid ID;

	static FArcTacticsEditorSelection Config()
	{
		return FArcTacticsEditorSelection();
	}

	bool IsConfig() const { return Kind == EArcTacticsEditorItemKind::Config; }

	friend bool operator==(const FArcTacticsEditorSelection&, const FArcTacticsEditorSelection&) = default;
};

USTRUCT()
struct FArcTacticsEditorClipboardPayload
{
	GENERATED_BODY()

	UPROPERTY()
	int32 Version = 13;

	UPROPERTY()
	EArcTacticsEditorItemKind Kind = EArcTacticsEditorItemKind::Config;

	UPROPERTY()
	FSoftObjectPath SourceAsset;

	UPROPERTY()
	FGuid SourceContextID;

	UPROPERTY()
	FGuid SourceGlobalParametersID;

	UPROPERTY()
	FGuid SourceScopeParametersID;

	UPROPERTY()
	FGuid SourceScopeParameterBagID;

	UPROPERTY()
	FGuid SourceScopeSelectorID;

	UPROPERTY()
	FArcTacticsNodeDefinition Node;

	UPROPERTY()
	FArcTacticsActionDefinition Action;

	UPROPERTY()
	FArcTacticsSelectorDefinition Selector;

	UPROPERTY()
	TArray<FArcTacticsBinding> Bindings;
};

class ARCTACTICSEDITOR_API FArcTacticsEditorClipboard
{
public:
	static bool Copy(
		const UArcTacticsConfig& Config,
		const FArcTacticsEditorSelection& Selection,
		FText& OutError);
	static bool Read(FArcTacticsEditorClipboardPayload& OutPayload, FText& OutError);
};
