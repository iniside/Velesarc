// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ArcMassSmartObjectTypes.h"
#include "SmartObjectRequestTypes.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "Items/EnvQueryItemType_MassEntityHandle.h"
#include "ArcEnvQueryItemType_EntitySmartObject.generated.h"

/**
 * 
 */
UCLASS()
class ARCAI_API UArcEnvQueryItemType_EntitySmartObject : public UEnvQueryItemType_MassEntityHandle
{
	GENERATED_BODY()
	
public:
	typedef FArcMassSmartObjectItem FValueType;

	UArcEnvQueryItemType_EntitySmartObject();

	static const FArcMassSmartObjectItem& GetValue(const uint8* RawData);
	static void SetValue(uint8* RawData, const FArcMassSmartObjectItem& Value);

	virtual FVector GetItemLocation(const uint8* RawData) const override;
	virtual FRotator GetItemRotation(const uint8* RawData) const override;
};

/** Fetches Smart Object slots within QueryBoxExtent from locations given by QueryOriginContext, that match SmartObjectRequestFilter */
UCLASS(MinimalAPI, meta = (DisplayName = "Arc Entity Smart Objects"))
class UArcEnvQueryGenerator_EntitySmartObjects : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UArcEnvQueryGenerator_EntitySmartObjects();

protected:
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	/** The context indicating the locations to be used as query origins */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> QueryOriginContext;

	/** If set will be used to filter gathered results */
	UPROPERTY(EditAnywhere, Category=Generator)
	FSmartObjectRequestFilter SmartObjectRequestFilter;

	/** Combined with generator's origin(s) (as indicated by QueryOriginContext) determines query's volume */
	UPROPERTY(EditAnywhere, Category = Generator)
	FVector QueryBoxExtent;

	/** Determines whether only currently claimable slots are allowed */
	UPROPERTY(EditAnywhere, Category = Generator)
	bool bOnlyClaimable = true;
};


UCLASS(MinimalAPI, meta = (DisplayName = "Arc Entity Smart Object Location"))
class UArcEnvQueryContext_EntitySmartObjectLocations : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	UArcEnvQueryContext_EntitySmartObjectLocations();

protected:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
	
	/** The context indicating the locations to be used as query origins */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> QueryOriginContext;

	/** If set will be used to filter gathered results */
	UPROPERTY(EditAnywhere, Category=Generator)
	FSmartObjectRequestFilter SmartObjectRequestFilter;

	/** Combined with generator's origin(s) (as indicated by QueryOriginContext) determines query's volume */
	UPROPERTY(EditAnywhere, Category = Generator)
	FVector QueryBoxExtent = FVector(10000.f);

	/** Determines whether only currently claimable slots are allowed */
	UPROPERTY(EditAnywhere, Category = Generator)
	bool bOnlyClaimable = true;
};
