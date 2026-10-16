// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/EntityHandle.h"
#include "UObject/ObjectKey.h"

class UArcAttributeHandlerConfig;
struct FArcAttributeRef;
struct FMassEntityManager;

struct ARCMASSMECHANICS_API FArcMetaWriter
{
	bool Write(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcAttributeHandlerConfig* HandlerConfig,
		const FArcAttributeRef& Meta, float Amount, const UObject& Config);

private:
	void WarnOnce(const UObject& Config, const FArcAttributeRef& Meta, const TCHAR* Reason);

	TSet<FObjectKey> ConfigsLoggedWithoutWrite;
};
