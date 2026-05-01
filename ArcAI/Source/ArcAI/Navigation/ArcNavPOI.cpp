// Copyright Lukasz Baran. All Rights Reserved.

#include "Navigation/ArcNavPOI.h"

#if WITH_EDITOR
#include "Components/BillboardComponent.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ArcNavPOI)

AArcNavPOI::AArcNavPOI(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetCanBeDamaged(false);

#if WITH_EDITORONLY_DATA
    SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
    if (SpriteComponent)
    {
        SpriteComponent->SetupAttachment(RootComponent);
        SpriteComponent->bIsScreenSizeScaled = true;
    }
#endif
}
