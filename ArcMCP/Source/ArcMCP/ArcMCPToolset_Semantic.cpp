// Copyright Lukasz Baran. All Rights Reserved.

#include "ArcMCPToolset.h"
#include "ArcMCPToolsetUtils.h"

#include "Dom/JsonObject.h"

#include "ISemanticSearchModule.h"
#include "HybridSearchIndex.h"
#include "Interfaces/IEmbeddingProvider.h"
#include "Settings/SemanticSearchSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"

//------------------------------------------------------------------------------
// Semantic asset search over MCP — two-phase async.
//
// The engine SemanticSearch plugin exposes querying only as an async console
// command (logs to LogSemanticSearch) or the C++ FHybridSearchIndex API — neither
// is callable/consumable over MCP.
//
// Querying is doubly async: the query text is embedded via an HTTP round-trip to
// the provider, then the hybrid search returns via a game-thread callback. An MCP
// UFUNCTION is dispatched from INSIDE FTSTicker::Tick (the ModelContextProtocol
// HTTP server is itself a ticker element), so a tool MUST NOT block-and-pump the
// game thread to await the result — re-ticking the core ticker re-enters the tick
// loop and crashes. Instead this is split in two: Start kicks the async chain and
// returns a request id immediately; the engine's normal tick advances the HTTP and
// fires the callbacks; Result polls the stored state. No manual ticking anywhere.
//------------------------------------------------------------------------------

namespace ArcMCPSemanticPrivate
{
	FString Fail(const FString& Message)
	{
		return ArcMCPToolsetPrivate::MakeError(Message);
	}

	// Per-request state. Held alive by both the pending-request map and the async
	// callbacks (TSharedRef). All fields are written from game-thread callbacks and
	// read from game-thread Result calls, so no synchronization is needed.
	struct FSemSearchState
	{
		FString Query;
		bool bDone = false;
		bool bError = false;
		FString Error;
		TArray<UE::SemanticSearch::FHybridSearchResult> Results;
		TMap<int64, FString> IDToPath;
	};

	TMap<int32, TSharedRef<FSemSearchState>>& PendingRequests()
	{
		static TMap<int32, TSharedRef<FSemSearchState>> Store;
		return Store;
	}

	int32 NextRequestId()
	{
		static int32 Counter = 0;
		return ++Counter;
	}
}

//------------------------------------------------------------------------------
// SemanticSearchStart
//------------------------------------------------------------------------------

FString UArcMCPToolset::SemanticSearchStart(const FString& Query, int32 TopK, const FString& FolderFilter)
{
	using namespace ArcMCPSemanticPrivate;
	using namespace UE::SemanticSearch;

	if (!IsInGameThread())
	{
		return Fail(TEXT("SemanticSearchStart must run on the game thread"));
	}
	if (Query.IsEmpty())
	{
		return Fail(TEXT("Missing required parameter: Query"));
	}

	const int32 K = TopK > 0 ? TopK : 15;

	FHybridSearchIndex& Index = FHybridSearchIndex::Get();
	if (!Index.GetCachedIndexStats().bIsInitialized)
	{
		return Fail(TEXT("Semantic search index is not initialised (nothing indexed yet)"));
	}

	TSharedPtr<IEmbeddingProvider> Provider = ISemanticSearchModule::Get().GetEmbeddingProvider();
	if (!Provider.IsValid())
	{
		return Fail(TEXT("No embedding provider is registered (configure OpenAI/Loci in SemanticSearch settings)"));
	}

	const USemanticSearchSettings* Settings = USemanticSearchSettings::Get();
	if (!Settings || Settings->IndexedPaths.IsEmpty())
	{
		return Fail(TEXT("No indexed paths configured in SemanticSearch settings"));
	}

	// id -> asset path over the configured indexed folders, plus an optional id filter.
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	TSharedRef<FSemSearchState> State = MakeShared<FSemSearchState>();
	State->Query = Query;
	for (const FDirectoryPath& Dir : Settings->IndexedPaths)
	{
		TArray<FAssetData> FolderAssets;
		// "/" (or empty) is the content root: GetAssetsByPath("/") returns nothing because
		// the mount roots are /Game, /Engine, /<Plugin>, etc. — enumerate everything instead.
		if (Dir.Path.IsEmpty() || Dir.Path == TEXT("/"))
		{
			AssetRegistry.GetAllAssets(FolderAssets, /*bIncludeOnlyOnDiskAssets=*/false);
		}
		else
		{
			AssetRegistry.GetAssetsByPath(FName(*Dir.Path), FolderAssets, /*bRecursive=*/true);
		}
		for (const FAssetData& Asset : FolderAssets)
		{
			State->IDToPath.Add(GetAssetIndexID(Asset), Asset.GetObjectPathString());
		}
	}
	if (State->IDToPath.IsEmpty())
	{
		return Fail(TEXT("No assets found in the configured indexed folders"));
	}

	TArray<int64> IDFilter;
	if (!FolderFilter.IsEmpty())
	{
		for (const TPair<int64, FString>& Pair : State->IDToPath)
		{
			if (Pair.Value.Contains(FolderFilter))
			{
				IDFilter.Add(Pair.Key);
			}
		}
		if (IDFilter.IsEmpty())
		{
			return Fail(FString::Printf(TEXT("No indexed assets match FolderFilter '%s'"), *FolderFilter));
		}
	}

	const int32 RequestId = NextRequestId();
	PendingRequests().Add(RequestId, State);

	// Kick the async embed -> search chain. Callbacks land on the game thread during
	// normal engine ticks; nothing here blocks or pumps.
	Provider->GenerateEmbeddingAsync(Query,
		[State, &Index, Query, K, IDFilter = MoveTemp(IDFilter)](FEmbeddingResponse&& Response) mutable
		{
			if (!Response.ErrorMessage.IsEmpty())
			{
				State->bError = true;
				State->Error = Response.ErrorMessage;
				State->bDone = true;
				return;
			}
			Index.SearchAsync(Query, MoveTemp(Response.Embedding), K, MoveTemp(IDFilter),
				TNumericLimits<float>::Max(),
				[State](TArray<FHybridSearchResult>&& Results)
				{
					State->Results = MoveTemp(Results);
					State->bDone = true;
				});
		},
		nullptr);

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("requestId"), RequestId);
	Root->SetStringField(TEXT("status"), TEXT("pending"));
	Root->SetStringField(TEXT("query"), Query);
	return ArcMCPToolsetPrivate::JsonObjToString(Root);
}

//------------------------------------------------------------------------------
// SemanticSearchResult
//------------------------------------------------------------------------------

FString UArcMCPToolset::SemanticSearchResult(int32 RequestId)
{
	using namespace ArcMCPSemanticPrivate;
	using namespace UE::SemanticSearch;

	if (!IsInGameThread())
	{
		return Fail(TEXT("SemanticSearchResult must run on the game thread"));
	}

	TSharedRef<FSemSearchState>* Found = PendingRequests().Find(RequestId);
	if (!Found)
	{
		return Fail(FString::Printf(TEXT("Unknown or already-consumed requestId: %d"), RequestId));
	}
	TSharedRef<FSemSearchState> State = *Found;

	if (!State->bDone)
	{
		TSharedPtr<FJsonObject> Pending = MakeShared<FJsonObject>();
		Pending->SetNumberField(TEXT("requestId"), RequestId);
		Pending->SetStringField(TEXT("status"), TEXT("pending"));
		return ArcMCPToolsetPrivate::JsonObjToString(Pending);
	}

	// Terminal — consume the request.
	PendingRequests().Remove(RequestId);

	if (State->bError)
	{
		return Fail(FString::Printf(TEXT("Embedding failed: %s"), *State->Error));
	}

	TArray<TSharedPtr<FJsonValue>> ResultArray;
	for (const FHybridSearchResult& Result : State->Results)
	{
		const FString* Path = State->IDToPath.Find(Result.ID);
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("path"), Path ? *Path : FString(TEXT("<unknown>")));
		Entry->SetNumberField(TEXT("rrf"), Result.RRFScore);
		Entry->SetNumberField(TEXT("vectorDistance"), Result.VectorDistance);
		Entry->SetNumberField(TEXT("bm25"), Result.BM25Score);
		ResultArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("requestId"), RequestId);
	Root->SetStringField(TEXT("status"), TEXT("done"));
	Root->SetStringField(TEXT("query"), State->Query);
	Root->SetNumberField(TEXT("count"), ResultArray.Num());
	Root->SetArrayField(TEXT("results"), ResultArray);
	return ArcMCPToolsetPrivate::JsonObjToString(Root);
}
