// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mass/Processor.h"
#include "Mass/EntityHandle.h"
#include "UObject/ObjectKey.h"
#include "ArcMassMechanics/Resources/ArcMetaWriter.h"
#include "ArcResourceProcessor.generated.h"

class UArcAttributeHandlerConfig;
class UArcResourceConfig;
struct FArcTagCountContainer;

UCLASS(meta = (DisplayName = "Arc Resource Processor"))
class ARCMASSMECHANICS_API UArcResourceProcessor : public UMassProcessor
{
	GENERATED_BODY()

public:
	UArcResourceProcessor();

protected:
	virtual void ConfigureQueries(const TSharedRef<FMassEntityManager>& EntityManager) override;
	virtual void Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context) override;

private:
	void UpdateEntity(FMassEntityManager& EntityManager, FMassEntityHandle Entity, const UArcResourceConfig& Config,
		const UArcAttributeHandlerConfig* HandlerConfig, const FArcTagCountContainer* OwnedTags, float StepSeconds);

	FMassEntityQuery EntityQuery{*this};

	TSet<FObjectKey> ConfigsLoggedWithoutOwnedTags;
	FArcMetaWriter MetaWriter;
};
