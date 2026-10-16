// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "ArcOcean.generated.h"

class UArcOceanComponent;
class UPostProcessComponent;

UCLASS(ClassGroup = (ArcWater))
class ARCWATER_API AArcOcean : public AActor
{
	GENERATED_BODY()

public:
	AArcOcean();

	//~ Begin AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
#endif
	virtual void PostRegisterAllComponents() override;
	//~ End AActor interface

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ocean")
	TObjectPtr<UArcOceanComponent> OceanComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ocean")
	TObjectPtr<UPostProcessComponent> UnderwaterPostProcess;

private:
	void LockTransform();
};
