// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMass/Config/ArcMassConfigTraitAccess.h"

#if WITH_EDITORONLY_DATA

namespace ArcMassConfig
{

TArray<TObjectPtr<UMassEntityTraitBase>>* GetMutableTraitsArray(FMassEntityConfig& Config)
{
	FProperty* Prop = FMassEntityConfig::StaticStruct()->FindPropertyByName(TEXT("Traits"));
	return Prop ? Prop->ContainerPtrToValuePtr<TArray<TObjectPtr<UMassEntityTraitBase>>>(&Config) : nullptr;
}

} // namespace ArcMassConfig

#endif // WITH_EDITORONLY_DATA
