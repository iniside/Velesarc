// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Simulation/ArcEcotopeChunkLayout.h"
#include "Simulation/ArcEcotopeSiteMetadata.h"
#include "Simulation/ArcEcotopeSiteChunkStorage.h"

#if WITH_EDITORONLY_DATA
#include "Serialization/EditorBulkData.h"
#endif

#if WITH_EDITORONLY_DATA
/** One immutable, torn-off editor bulk descriptor and its trusted manifest identity. */
struct ARCECOTOPE_API FArcEcotopeFrozenSitePayload
{
	UE::Serialization::FEditorBulkData Data;
	int64 ExpectedSize = 0;
	uint64 ExpectedHash = 0;
	bool bMemoryOnly = false;
	bool bLoadedAfterFreeze = false;

	FArcEcotopeFrozenSitePayload() = default;
	FArcEcotopeFrozenSitePayload(FArcEcotopeFrozenSitePayload&&) = default;
	FArcEcotopeFrozenSitePayload& operator=(FArcEcotopeFrozenSitePayload&&) = default;
	FArcEcotopeFrozenSitePayload(const FArcEcotopeFrozenSitePayload&) = delete;
	FArcEcotopeFrozenSitePayload& operator=(const FArcEcotopeFrozenSitePayload&) = delete;
};
#endif

/**
 * Immutable worker input. Torn-off descriptors can outlive actor edits,
 * rebakes, unload, and destruction without retaining a UObject reference.
 */
class ARCECOTOPE_API FArcEcotopeFrozenSiteSource final
{
public:
	FArcEcotopeSiteMetadata Metadata;
	FArcEcotopeChunkLayout Layout;

#if WITH_EDITORONLY_DATA
	FArcEcotopeSiteChunkManifest Manifest;
	TArray<FArcEcotopeFrozenSitePayload> Payloads;
#endif

	FArcEcotopeFrozenSiteSource() = default;
	FArcEcotopeFrozenSiteSource(FArcEcotopeFrozenSiteSource&&) = default;
	FArcEcotopeFrozenSiteSource& operator=(FArcEcotopeFrozenSiteSource&&) = default;
	FArcEcotopeFrozenSiteSource(const FArcEcotopeFrozenSiteSource&) = delete;
	FArcEcotopeFrozenSiteSource& operator=(const FArcEcotopeFrozenSiteSource&) = delete;

#if WITH_EDITORONLY_DATA
	int64 GetLoadedSourceBytes() const;
	int64 GetMemoryOnlySourceBytes() const;
#endif
};

using FArcEcotopeFrozenSiteSourcePtr =
	TSharedPtr<const FArcEcotopeFrozenSiteSource, ESPMode::ThreadSafe>;
