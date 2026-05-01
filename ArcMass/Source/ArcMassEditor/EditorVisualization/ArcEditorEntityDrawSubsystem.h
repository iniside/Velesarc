// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "ArcEditorEntityDrawContributor.h"
#include "ArcEditorEntityDrawSubsystem.generated.h"

class UArcEditorEntityDrawComponent;

UCLASS()
class ARCMASSEDITOR_API UArcEditorEntityDrawSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void RegisterContributor(TSharedPtr<IArcEditorEntityDrawContributor> Contributor);
	void UnregisterContributor(TSharedPtr<IArcEditorEntityDrawContributor> Contributor);

	void SetCategoryEnabled(FName Category, bool bEnabled);
	bool IsCategoryEnabled(FName Category) const;
	TArray<FName> GetAllCategories() const;

	void Activate(UWorld* World);
	void Deactivate();
	bool IsActive() const { return bActive; }

	void RefreshShapes();

private:
	void BuildContext(FArcEditorDrawContext& OutContext) const;
	void SpawnDrawActor();
	void DestroyDrawActor();

	TArray<TSharedPtr<IArcEditorEntityDrawContributor>> Contributors;
	TMap<FName, bool> CategoryToggleState;

	UPROPERTY(Transient)
	TObjectPtr<AActor> DrawActor = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UArcEditorEntityDrawComponent> DrawComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UWorld> ActiveWorld = nullptr;

	bool bActive = false;
};
