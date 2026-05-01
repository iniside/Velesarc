// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SSpinBox.h"

class UArcStrategyTrainingSubsystem;
class UArcStrategyCurriculumAsset;
class UArcArchetypeRewardWeights;
class SArcRewardChart;

/**
 * Two-panel Slate dashboard for controlling and monitoring ML strategy training.
 * Left: config (asset pickers, PPO settings, start/stop).
 * Right: telemetry (status, reward chart, agent table).
 */
class SArcStrategyTrainingTab : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SArcStrategyTrainingTab) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float InDeltaTime) override;

private:
	// Config state
	TWeakObjectPtr<UArcStrategyCurriculumAsset> SelectedCurriculum;
	TWeakObjectPtr<UArcArchetypeRewardWeights> SelectedRewardWeights;
	FString CurriculumObjectPath;
	FString RewardWeightsObjectPath;
	FString SavePath = TEXT("/Game/ML/Strategy/");
	float LearningRatePolicy = 0.0001f;
	float LearningRateCritic = 0.001f;
	int32 HiddenLayerSize = 128;
	float FixedTimeStepHz = 60.0f;

	// Widgets
	TSharedPtr<SArcRewardChart> RewardChart;
	TSharedPtr<STextBlock> StatusText;
	TSharedPtr<STextBlock> StageText;
	TSharedPtr<STextBlock> EpisodeText;
	TSharedPtr<STextBlock> RewardText;
	TSharedPtr<SWidget> ThrobberWidget;

	// Helpers
	UArcStrategyTrainingSubsystem* GetTrainingSubsystem() const;
	FReply OnStartTraining();
	FReply OnStopTraining();
	FReply OnSavePolicies();
	bool IsTraining() const;
	bool IsNotTraining() const;
};
