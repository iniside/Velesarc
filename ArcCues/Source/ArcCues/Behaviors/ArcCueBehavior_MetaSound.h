// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "GameplayTagContainer.h"

#include "ArcCueBehavior.h"

#include "ArcCueBehavior_MetaSound.generated.h"

class UMetaSoundSource;

UENUM(BlueprintType)
enum class EArcCueMetaSoundPlacement : uint8
{
	World,
	TargetComponent
};

UENUM(BlueprintType)
enum class EArcCueMetaSoundParameterType : uint8
{
	Float,
	Integer,
	Boolean,
	String,
	Object
};

UENUM(BlueprintType)
enum class EArcCueMetaSoundAction : uint8
{
	None,
	Play,
	Stop,
	FadeOut
};

USTRUCT(BlueprintType)
struct ARCCUES_API FArcCueMetaSoundParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="MetaSound")
	FName Name;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	EArcCueMetaSoundParameterType Type = EArcCueMetaSoundParameterType::Float;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Type == EArcCueMetaSoundParameterType::Float", EditConditionHides))
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Type == EArcCueMetaSoundParameterType::Integer", EditConditionHides))
	int32 IntegerValue = 0;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Type == EArcCueMetaSoundParameterType::Boolean", EditConditionHides))
	bool BooleanValue = false;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Type == EArcCueMetaSoundParameterType::String", EditConditionHides))
	FString StringValue;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Type == EArcCueMetaSoundParameterType::Object", EditConditionHides))
	TObjectPtr<UObject> ObjectValue = nullptr;

	void Apply(UAudioComponent& Component) const;
};

USTRUCT(BlueprintType)
struct ARCCUES_API FArcCueMetaSoundReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="MetaSound")
	EArcCueEvent Event = EArcCueEvent::Update;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	FGameplayTagQuery PayloadQuery;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	EArcCueMetaSoundAction Action = EArcCueMetaSoundAction::None;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	TArray<FArcCueMetaSoundParameter> Parameters;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	TArray<FName> Triggers;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Action == EArcCueMetaSoundAction::FadeOut", EditConditionHides, ClampMin="0.0"))
	float FadeOutDuration = 0.0f;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Action == EArcCueMetaSoundAction::FadeOut", EditConditionHides, ClampMin="0.0"))
	float FadeVolumeLevel = 0.0f;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Action == EArcCueMetaSoundAction::FadeOut", EditConditionHides))
	EAudioFaderCurve FadeCurve = EAudioFaderCurve::Linear;

	bool Matches(const FArcCueEvent& CueEvent) const;
};

USTRUCT()
struct ARCCUES_API FArcCueBehaviorState_MetaSound : public FArcCueBehaviorState
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAudioComponent> Component = nullptr;
};

USTRUCT(BlueprintType, meta=(DisplayName="MetaSound"))
struct ARCCUES_API FArcCueBehavior_MetaSound : public FArcCueBehavior
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="MetaSound")
	TObjectPtr<UMetaSoundSource> MetaSound = nullptr;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	EArcCueMetaSoundPlacement Placement = EArcCueMetaSoundPlacement::World;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Placement == EArcCueMetaSoundPlacement::TargetComponent", EditConditionHides))
	FName ComponentTag;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(EditCondition="Placement == EArcCueMetaSoundPlacement::TargetComponent", EditConditionHides))
	FName SocketName;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	FTransform LocalOffset = FTransform::Identity;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	bool bStartOnApplication = true;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	TArray<FArcCueMetaSoundParameter> InitialParameters;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	TArray<FName> InitialTriggers;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	TArray<FArcCueMetaSoundReaction> Reactions;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(ClampMin="0.0"))
	float DefaultFadeOutDuration = 0.0f;

	UPROPERTY(EditAnywhere, Category="MetaSound", meta=(ClampMin="0.0"))
	float DefaultFadeVolumeLevel = 0.0f;

	UPROPERTY(EditAnywhere, Category="MetaSound")
	EAudioFaderCurve DefaultFadeCurve = EAudioFaderCurve::Linear;

	virtual void OnExecute(const FArcCueContext& Context) const override;
	virtual TInstancedStruct<FArcCueBehaviorState> OnApplication(const FArcCueContext& Context) const override;
	virtual void OnEvent(const FArcCueContext& Context, const FArcCueEvent& Event, TInstancedStruct<FArcCueBehaviorState>& State) const override;
	virtual void OnRemoved(const FArcCueContext& Context, FArcCueBehaviorState& State) const override;
};
