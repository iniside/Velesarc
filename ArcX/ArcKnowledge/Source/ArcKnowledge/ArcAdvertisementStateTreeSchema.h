// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeSchema.h"
#include "GameFramework/Actor.h"
#include "ArcAdvertisementStateTreeSchema.generated.h"

struct FStateTreeExternalDataDesc;

namespace UE::ArcKnowledge::Names
{
	extern ARCKNOWLEDGE_API const FName ContextActor;
	extern ARCKNOWLEDGE_API const FName ExecutingEntityHandle;
	extern ARCKNOWLEDGE_API const FName SourceEntityHandle;
	extern ARCKNOWLEDGE_API const FName AdvertisementHandle;
}

/**
 * Schema for advertisement behavior StateTrees.
 * Defines the execution context available to tasks within an advertisement's StateTree.
 *
 * Context data provided:
 * - ContextActor (AActor*) — the actor executing the behavior
 * - ExecutingEntityHandle (FMassEntityHandle) — the Mass entity executing
 * - SourceEntityHandle (FMassEntityHandle) — the entity that posted the advertisement
 * - AdvertisementHandle (FArcKnowledgeHandle) — the knowledge handle of the advertisement
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Advertisement Behavior"))
class ARCKNOWLEDGE_API UArcAdvertisementStateTreeSchema : public UStateTreeSchema
{
	GENERATED_BODY()

public:
	UArcAdvertisementStateTreeSchema();

	UClass* GetContextActorClass() const { return ContextActorClass; }

protected:
	virtual bool IsStructAllowed(const UScriptStruct* InScriptStruct) const override;
	virtual bool IsClassAllowed(const UClass* InClass) const override;
	virtual bool IsExternalItemAllowed(const UStruct& InStruct) const override;

	virtual TConstArrayView<FStateTreeExternalDataDesc> GetContextDataDescs() const override { return ContextDataDescs; }

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif

	/** Actor class the StateTree is expected to run on. Allows binding to specific Actor class properties. */
	UPROPERTY(EditAnywhere, Category = "Defaults")
	TSubclassOf<AActor> ContextActorClass;

	/** List of named external data required by schema and provided to the state tree through the execution context. */
	UPROPERTY()
	TArray<FStateTreeExternalDataDesc> ContextDataDescs;
};
