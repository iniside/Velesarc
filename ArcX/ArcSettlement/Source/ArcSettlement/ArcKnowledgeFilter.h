// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "ArcKnowledgeFilter.generated.h"

struct FArcKnowledgeEntry;
struct FArcKnowledgeQueryContext;

/**
 * Base for knowledge query filters. Subtype this to create custom filters.
 * Filters are binary — they discard entries that don't pass.
 */
USTRUCT(BlueprintType, meta = (ExcludeBaseStruct))
struct ARCSETTLEMENT_API FArcKnowledgeFilter
{
	GENERATED_BODY()

	virtual ~FArcKnowledgeFilter() = default;

	/** Return true if the entry passes this filter. */
	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const
	{
		return true;
	}
};

/** Filters entries within a maximum distance from the query origin. */
USTRUCT(BlueprintType, DisplayName = "Max Distance")
struct ARCSETTLEMENT_API FArcKnowledgeFilter_MaxDistance : public FArcKnowledgeFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = 0.0))
	float MaxDistance = 10000.0f;

	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Filters entries that belong to the same settlement as the querier. */
USTRUCT(BlueprintType, DisplayName = "Same Settlement")
struct ARCSETTLEMENT_API FArcKnowledgeFilter_SameSettlement : public FArcKnowledgeFilter
{
	GENERATED_BODY()

	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Filters entries with relevance above a threshold. */
USTRUCT(BlueprintType, DisplayName = "Min Relevance")
struct ARCSETTLEMENT_API FArcKnowledgeFilter_MinRelevance : public FArcKnowledgeFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float MinRelevance = 0.1f;

	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Filters entries not older than a specified age in seconds. */
USTRUCT(BlueprintType, DisplayName = "Max Age")
struct ARCSETTLEMENT_API FArcKnowledgeFilter_MaxAge : public FArcKnowledgeFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter", meta = (ClampMin = 0.0))
	float MaxAgeSeconds = 300.0f;

	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Filters entries whose payload is a specific struct type. */
USTRUCT(BlueprintType, DisplayName = "Payload Type")
struct ARCSETTLEMENT_API FArcKnowledgeFilter_PayloadType : public FArcKnowledgeFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Filter")
	TObjectPtr<UScriptStruct> RequiredPayloadType = nullptr;

	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};

/** Filters out entries that have been claimed. */
USTRUCT(BlueprintType, DisplayName = "Not Claimed")
struct ARCSETTLEMENT_API FArcKnowledgeFilter_NotClaimed : public FArcKnowledgeFilter
{
	GENERATED_BODY()

	virtual bool PassesFilter(const FArcKnowledgeEntry& Entry, const FArcKnowledgeQueryContext& Context) const override;
};
