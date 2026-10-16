// Copyright Lukasz Baran. All Rights Reserved.

#include "MVVM/ArcRmlActionRouter.h"

#include "ArcRmlUILog.h"
#include "Interfaces/ArcRmlStrings.h"
#include "Thread/ArcRmlUIQueues.h"

#include "RmlUi/Core/Element.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/Event.h"

namespace ArcRmlActionRouterPrivate
{
	const Rml::String ActionAttribute = "data-action";
	const Rml::String ModelAttribute = "data-model";
	const TCHAR* const ActionFunctionName = TEXT("HandleAction");
}

FArcRmlActionRouter::FArcRmlActionRouter(const TSharedRef<FArcRmlViewChannel, ESPMode::ThreadSafe>& InChannel)
	: Channel(InChannel)
{
}

void FArcRmlActionRouter::ProcessEvent(Rml::Event& Event)
{
	Rml::Element* Element = Event.GetTargetElement();
	Rml::String Action;
	while (Element != nullptr && Action.empty())
	{
		Action = Element->GetAttribute<Rml::String>(ArcRmlActionRouterPrivate::ActionAttribute, Rml::String());
		Element = Element->GetParentNode();
	}
	if (Action.empty())
	{
		return;
	}

	const Rml::Element* ModelRoot = Event.GetTargetElement()->GetOwnerDocument();
	for (const Rml::Element* Ancestor = Event.GetTargetElement(); Ancestor != nullptr; Ancestor = Ancestor->GetParentNode())
	{
		if (Ancestor->HasAttribute(ArcRmlEmbedAttributes::Wrapper))
		{
			ModelRoot = Ancestor;
			break;
		}
	}
	const Rml::String ModelName = ModelRoot ? ModelRoot->GetAttribute<Rml::String>(ArcRmlActionRouterPrivate::ModelAttribute, Rml::String()) : Rml::String();
	if (ModelName.empty())
	{
		UE_LOGF(LogArcRmlUI, Warning, "ArcRmlUI: action '%ls' was clicked under a <body> or embedded document root that has no data-model", *ArcRmlStrings::ToFString(Action));
		return;
	}

	FArcRmlOutboundEvent Outbound;
	Outbound.Type = EArcRmlOutboundType::ViewModelCall;
	FArcRmlViewModelCall& Call = Outbound.Call;
	Call.ModelName = ArcRmlStrings::ToFString(ModelName);
	Call.FunctionName = ArcRmlActionRouterPrivate::ActionFunctionName;
	Call.bRoutedAction = true;
	FArcRmlValue& Argument = Call.Arguments.AddDefaulted_GetRef();
	Argument.Kind = EArcRmlValueKind::String;
	Argument.String = ArcRmlStrings::ToFString(Action);

	Channel->PushOutbound(MoveTemp(Outbound));
}
