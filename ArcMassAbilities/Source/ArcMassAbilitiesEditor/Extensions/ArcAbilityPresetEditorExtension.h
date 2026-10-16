// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Templates/SubclassOf.h"
#include "StateTreeEditorDataExtension.h"

class UArcAbilityTreePreset;

#include "ArcAbilityPresetEditorExtension.generated.h"

UCLASS()
class UArcAbilityPresetEditorExtension : public UStateTreeEditorDataExtension
{
	GENERATED_BODY()

public:
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	UPROPERTY(EditAnywhere, Category = "Preset", meta = (ExcludeFromHash))
	TSubclassOf<UArcAbilityTreePreset> SourcePreset;

	/** ExcludeFromHash is load-bearing: without it this field would count toward the very CRC it stores. */
	UPROPERTY(VisibleAnywhere, Category = "Preset", meta = (ExcludeFromHash))
	uint32 HashAtGeneration = 0;

private:
	TSubclassOf<UArcAbilityTreePreset> PresetBeforeEdit;

	FTimerHandle RebuildTimerHandle;
};
