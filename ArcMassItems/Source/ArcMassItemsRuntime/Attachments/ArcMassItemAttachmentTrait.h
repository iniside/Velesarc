// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "Mass/ArchetypeCompositionTypes.h"
#include "MassLogging.h"
#include "ArcMass/Config/ArcMassEntityTraitBase.h"
#include "ArcMassItemAttachmentTrait.generated.h"

class UArcMassItemAttachmentConfig;

USTRUCT()
struct ARCMASSITEMSRUNTIME_API FArcMassItemAttachmentTag : public FMassTag
{
	GENERATED_BODY()
};

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Item Attachment", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassItemAttachmentTrait : public UArcMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Attachments")
	TObjectPtr<UArcMassItemAttachmentConfig> AttachmentConfig;

	// Store types whose events drive this attachment trait. Empty == default to
	// {FArcMassItemStoreFragment} for backward compatibility.
	UPROPERTY(EditAnywhere, Category = "Attachments", meta = (MetaStruct = "/Script/ArcMassItemsRuntime.ArcMassItemStoreFragment"))
	TArray<TObjectPtr<UScriptStruct>> InterestedStoreTypes;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
	virtual void BuildSparseFragments(FArcSparseFragmentBuildContext& Out, const UWorld& World) const override;
};
