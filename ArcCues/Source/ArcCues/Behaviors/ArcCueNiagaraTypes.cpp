// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcCueNiagaraTypes.h"

#include "NiagaraComponent.h"

#include "ArcMass/NiagaraVisualization/ArcNiagaraVisFragments.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcCueNiagaraTypes)

void FArcCueNiagaraParameter::Apply(UNiagaraComponent& Component) const
{
	if (Name.IsNone())
	{
		return;
	}

	switch (Type)
	{
	case EArcCueNiagaraParameterType::Float:
		Component.SetVariableFloat(Name, FloatValue);
		break;
	case EArcCueNiagaraParameterType::Integer:
		Component.SetVariableInt(Name, IntegerValue);
		break;
	case EArcCueNiagaraParameterType::Boolean:
		Component.SetVariableBool(Name, BooleanValue);
		break;
	case EArcCueNiagaraParameterType::Vector2D:
		Component.SetVariableVec2(Name, Vector2DValue);
		break;
	case EArcCueNiagaraParameterType::Vector:
		Component.SetVariableVec3(Name, VectorValue);
		break;
	case EArcCueNiagaraParameterType::Vector4:
		Component.SetVariableVec4(Name, Vector4Value);
		break;
	case EArcCueNiagaraParameterType::Color:
		Component.SetVariableLinearColor(Name, ColorValue);
		break;
	case EArcCueNiagaraParameterType::Quaternion:
		Component.SetVariableQuat(Name, QuaternionValue);
		break;
	}
}

void FArcCueNiagaraParameter::Apply(FArcNiagaraVisParamsFragment& Parameters) const
{
	if (Name.IsNone())
	{
		return;
	}

	switch (Type)
	{
	case EArcCueNiagaraParameterType::Float:
		Parameters.SetFloat(Name, FloatValue);
		break;
	case EArcCueNiagaraParameterType::Integer:
		Parameters.SetInt(Name, IntegerValue);
		break;
	case EArcCueNiagaraParameterType::Boolean:
		Parameters.SetBool(Name, BooleanValue);
		break;
	case EArcCueNiagaraParameterType::Vector2D:
		Parameters.SetVector2D(Name, Vector2DValue);
		break;
	case EArcCueNiagaraParameterType::Vector:
		Parameters.SetVector(Name, VectorValue);
		break;
	case EArcCueNiagaraParameterType::Vector4:
		Parameters.SetVector4(Name, Vector4Value);
		break;
	case EArcCueNiagaraParameterType::Color:
		Parameters.SetColor(Name, ColorValue);
		break;
	case EArcCueNiagaraParameterType::Quaternion:
		Parameters.SetQuat(Name, QuaternionValue);
		break;
	}
}

bool FArcCueNiagaraReaction::Matches(const FArcCueEvent& CueEvent) const
{
	return Event == CueEvent.Type && (PayloadQuery.IsEmpty() || PayloadQuery.Matches(CueEvent.PayloadTags));
}

EArcCueNiagaraAction FArcCueNiagaraControl::ResolveAction(const FArcCueEvent& Event) const
{
	EArcCueNiagaraAction Result = EArcCueNiagaraAction::None;
	switch (Event.Type)
	{
	case EArcCueEvent::Start:
		Result = EArcCueNiagaraAction::Activate;
		break;
	case EArcCueEvent::Stop:
		Result = EArcCueNiagaraAction::Deactivate;
		break;
	case EArcCueEvent::End:
		Result = EArcCueNiagaraAction::Deactivate;
		break;
	case EArcCueEvent::Update:
		break;
	}

	for (const FArcCueNiagaraReaction& Reaction : Reactions)
	{
		if (Reaction.Matches(Event) && Reaction.Action != EArcCueNiagaraAction::None)
		{
			Result = Reaction.Action;
		}
	}
	return Result;
}

void FArcCueNiagaraControl::ApplyInitial(UNiagaraComponent& Component) const
{
	for (const FArcCueNiagaraParameter& Parameter : InitialParameters)
	{
		Parameter.Apply(Component);
	}
}

void FArcCueNiagaraControl::ApplyInitial(FArcNiagaraVisParamsFragment& Parameters) const
{
	for (const FArcCueNiagaraParameter& Parameter : InitialParameters)
	{
		Parameter.Apply(Parameters);
	}
}

void FArcCueNiagaraControl::ApplyEvent(const FArcCueEvent& Event, UNiagaraComponent& Component) const
{
	for (const FArcCueNiagaraReaction& Reaction : Reactions)
	{
		if (Reaction.Matches(Event))
		{
			for (const FArcCueNiagaraParameter& Parameter : Reaction.Parameters)
			{
				Parameter.Apply(Component);
			}
		}
	}
}

void FArcCueNiagaraControl::ApplyEvent(const FArcCueEvent& Event, FArcNiagaraVisParamsFragment& Parameters) const
{
	for (const FArcCueNiagaraReaction& Reaction : Reactions)
	{
		if (Reaction.Matches(Event))
		{
			for (const FArcCueNiagaraParameter& Parameter : Reaction.Parameters)
			{
				Parameter.Apply(Parameters);
			}
		}
	}
}
