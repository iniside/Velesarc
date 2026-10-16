// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "ArcCueTypes.h"

#include "ArcCueNiagaraTypes.generated.h"

class UNiagaraComponent;
struct FArcNiagaraVisParamsFragment;

UENUM(BlueprintType)
enum class EArcCueNiagaraParameterType : uint8
{
	Float,
	Integer,
	Boolean,
	Vector2D,
	Vector,
	Vector4,
	Color,
	Quaternion
};

UENUM(BlueprintType)
enum class EArcCueNiagaraAction : uint8
{
	None,
	Activate,
	Deactivate,
	DeactivateImmediate
};

USTRUCT(BlueprintType)
struct ARCCUES_API FArcCueNiagaraParameter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Niagara")
	FName Name;

	UPROPERTY(EditAnywhere, Category="Niagara")
	EArcCueNiagaraParameterType Type = EArcCueNiagaraParameterType::Float;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Float", EditConditionHides))
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Integer", EditConditionHides))
	int32 IntegerValue = 0;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Boolean", EditConditionHides))
	bool BooleanValue = false;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Vector2D", EditConditionHides))
	FVector2D Vector2DValue = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Vector", EditConditionHides))
	FVector VectorValue = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Vector4", EditConditionHides))
	FVector4 Vector4Value = FVector4(0.0, 0.0, 0.0, 0.0);

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Color", EditConditionHides))
	FLinearColor ColorValue = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category="Niagara", meta=(EditCondition="Type == EArcCueNiagaraParameterType::Quaternion", EditConditionHides))
	FQuat QuaternionValue = FQuat::Identity;

	void Apply(UNiagaraComponent& Component) const;
	void Apply(FArcNiagaraVisParamsFragment& Parameters) const;
};

USTRUCT(BlueprintType)
struct ARCCUES_API FArcCueNiagaraReaction
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Niagara")
	EArcCueEvent Event = EArcCueEvent::Update;

	UPROPERTY(EditAnywhere, Category="Niagara")
	FGameplayTagQuery PayloadQuery;

	UPROPERTY(EditAnywhere, Category="Niagara")
	EArcCueNiagaraAction Action = EArcCueNiagaraAction::None;

	UPROPERTY(EditAnywhere, Category="Niagara")
	TArray<FArcCueNiagaraParameter> Parameters;

	bool Matches(const FArcCueEvent& CueEvent) const;
};

USTRUCT(BlueprintType)
struct ARCCUES_API FArcCueNiagaraControl
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category="Niagara")
	bool bStartOnApplication = true;

	UPROPERTY(EditAnywhere, Category="Niagara")
	TArray<FArcCueNiagaraParameter> InitialParameters;

	UPROPERTY(EditAnywhere, Category="Niagara")
	TArray<FArcCueNiagaraReaction> Reactions;

	EArcCueNiagaraAction ResolveAction(const FArcCueEvent& Event) const;
	void ApplyInitial(UNiagaraComponent& Component) const;
	void ApplyInitial(FArcNiagaraVisParamsFragment& Parameters) const;
	void ApplyEvent(const FArcCueEvent& Event, UNiagaraComponent& Component) const;
	void ApplyEvent(const FArcCueEvent& Event, FArcNiagaraVisParamsFragment& Parameters) const;
};
