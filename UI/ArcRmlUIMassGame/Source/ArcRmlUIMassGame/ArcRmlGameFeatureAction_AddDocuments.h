// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ArcRmlDocumentRef.h"
#include "GameFeatures/ArcMassGameFeatureAction_WorldActionBase.h"

#include "ArcRmlGameFeatureAction_AddDocuments.generated.h"

class AActor;
class APawn;
class ULocalPlayer;
class UArcMassPawnExtensionComponent;
struct FComponentRequestHandle;
struct FWorldContext;

USTRUCT()
struct FArcRmlDocumentEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "RmlUI")
	FArcRmlDocumentRef Document;
};

UCLASS(MinimalAPI, meta = (DisplayName = "Add RML Documents"))
class UArcRmlGameFeatureAction_AddDocuments final : public UArcMassGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "RmlUI", meta = (TitleProperty = "Document"))
	TArray<FArcRmlDocumentEntry> Documents;

	struct FPerContextData
	{
		TArray<TSharedPtr<FComponentRequestHandle>> ComponentRequests;
		TWeakObjectPtr<APawn> Pawn;
		TWeakObjectPtr<ULocalPlayer> LocalPlayer;
	};

	TMap<FGameFeatureStateChangeContext, FPerContextData> ContextData;

	virtual void AddToWorld(const FWorldContext& WorldContext
							, const FGameFeatureStateChangeContext& ChangeContext) override;

	void CloseDocuments(FPerContextData& ActiveData) const;

	void HandlePawnExtension(AActor* Actor
							 , FName EventName
							 , FGameFeatureStateChangeContext ChangeContext);

	void HandlePawnReady(UArcMassPawnExtensionComponent* PawnExt
						 , FGameFeatureStateChangeContext ChangeContext);
};
