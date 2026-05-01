// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "Components/InstancedSkinnedMeshComponent.h"
#include "ArcEditorInstancedSkinnedMeshComponent.generated.h"

/**
 * Editor-only subclass of UInstancedSkinnedMeshComponent that provides
 * per-instance SkMInstance typed element handles for viewport selection.
 */
UCLASS()
class ARCMASS_API UArcEditorInstancedSkinnedMeshComponent : public UInstancedSkinnedMeshComponent
{
	GENERATED_BODY()

public:
	virtual FTypedElementHandle GetInstanceElementHandle(int32 InstanceIndex, bool bAllowCreate = true) const override;
};

#endif // WITH_EDITOR
