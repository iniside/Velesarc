// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/UEdMode.h"
#include "ArcPlacedEntityEditorMode.generated.h"

class AArcPlacedEntityPartitionActor;
class AArcAlwaysLoadedEntityActor;

UCLASS()
class UArcPlacedEntityEditorMode : public UEdMode
{
	GENERATED_BODY()

public:
	static const FEditorModeID ModeId;

	UArcPlacedEntityEditorMode();

	//~ Begin UEdMode Interface
	virtual void Enter() override;
	virtual void Exit() override;
	virtual bool IsSelectionDisallowed(AActor* InActor, bool bInSelection) const override;
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;
	//~ End UEdMode Interface

	virtual void BindCommands();

	void SetEditTarget(AActor* InTarget);
	AActor* GetEditTarget() const;

	static void EnterEditMode(AActor* InTarget);
	static void ExitEditMode();

private:
	void ExitModeCommand();

	TWeakObjectPtr<AActor> EditTarget;
};
