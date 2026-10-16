// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMassMechanics/Resources/ArcMetaWriter.h"

#include "ArcMassMechanics/ArcMassMechanicsLogs.h"
#include "Attributes/ArcAttribute.h"
#include "Attributes/ArcAttributeHandlerConfig.h"
#include "Attributes/ArcInstantModifier.h"
#include "Effects/ArcEffectTypes.h"

bool FArcMetaWriter::Write(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcAttributeHandlerConfig* HandlerConfig,
	const FArcAttributeRef& Meta, float Amount, const UObject& Config)
{
	if (!HandlerConfig || !HandlerConfig->FindHandler(Meta))
	{
		WarnOnce(Config, Meta, TEXT("has no handler on the entity"));
		return false;
	}

	if (!ArcAttributes::EnqueueInstantModifier(EntityManager, Entity, Meta, EArcModifierOp::Add, Amount, EArcAttributeOpType::Execute, Entity))
	{
		WarnOnce(Config, Meta, TEXT("cannot be written because the entity has no FArcPendingAttributeOpsFragment"));
		return false;
	}

	return true;
}

void FArcMetaWriter::WarnOnce(const UObject& Config, const FArcAttributeRef& Meta, const TCHAR* Reason)
{
	bool bAlreadyLogged = false;
	ConfigsLoggedWithoutWrite.Add(FObjectKey(&Config), &bAlreadyLogged);
	if (!bAlreadyLogged)
	{
		UE_LOGF(LogArcMassMechanics, Warning, "Config %ls writes meta %ls, which %ls; the write is dropped.",
			*Config.GetName(), *Meta.PropertyName.ToString(), Reason);
	}
}
