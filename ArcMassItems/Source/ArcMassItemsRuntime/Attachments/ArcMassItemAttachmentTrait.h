// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MassEntityTraitBase.h"
#include "ArcMassItemAttachmentTrait.generated.h"

class UArcMassItemAttachmentConfig;

UCLASS(BlueprintType, EditInlineNew, CollapseCategories, meta = (DisplayName = "Arc Mass Item Attachment", Category = "Items"))
class ARCMASSITEMSRUNTIME_API UArcMassItemAttachmentTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Attachments")
	TObjectPtr<UArcMassItemAttachmentConfig> AttachmentConfig;

	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
