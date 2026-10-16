// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "ArcMCPToolset.generated.h"

/**
 * ArcGame MCP Toolset — exposes editor tools for StateTree, containers, semantic search, traces and tactics
 * via the ToolsetRegistry / ModelContextProtocol server.
 */
UCLASS()
class UArcMCPToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	// ---- StateTree ----

	/** List all available StateTree schema classes with their context data and allowed features. */
	UFUNCTION(meta = (AICallable))
	static FString ListStateTreeSchemas();

	/** List StateTree node types (tasks, conditions, evaluators, considerations, property functions).
	 *  Optionally filter by Schema class name, Category ('task','condition','evaluator','consideration','property_function'), or text Filter. */
	UFUNCTION(meta = (AICallable))
	static FString ListStateTreeNodes(const FString& Schema, const FString& Category, const FString& Filter);

	/** Create a compiled StateTree asset from a JSON Description string.
	 *  The Description is a JSON object with name, package_path, schema, states, bindings, etc. */
	UFUNCTION(meta = (AICallable))
	static FString CreateStateTree(const FString& Description);

	/** Validate a StateTree JSON Description without creating an asset. Returns errors and warnings. */
	UFUNCTION(meta = (AICallable))
	static FString ValidateStateTree(const FString& Description);

	// ---- Containers ----
	// Surgical editing of FInstancedStruct elements nested in a TSet / TArray / TMap
	// property on ANY UObject. Mutates the live element in place, so nested soft-object
	// refs (e.g. an item icon) are imported per-field and preserved — unlike the generic
	// ObjectTools.set_properties container round-trip, which nulls them.
	//
	// ObjectPath        : full object path, e.g. "/Game/.../ID_Foo.ID_Foo".
	// ContainerProperty : name of the TSet/TArray/TMap<FInstancedStruct-or-wrapper> property.
	// Selector (JSON)   : which element — {"structType":"X"} (TSet/TArray),
	//                     {"index":N} (TArray), or {"key":"K"} (TMap; matched by exported key text).
	// After editing, the caller saves via AssetTools.save_assets.

	/** Read one container element's inner struct as JSON. */
	UFUNCTION(meta = (AICallable))
	static FString GetStructInContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& Selector);

	/** Merge FieldValuesJson (a JSON object of field->value) into the selected element's inner struct. Only listed fields change. */
	UFUNCTION(meta = (AICallable))
	static FString SetStructFieldInContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& Selector, const FString& FieldValuesJson);

	/** Append a new element of StructType to a TSet/TArray, initialising it from FieldValuesJson. TMap add is unsupported (needs a key). */
	UFUNCTION(meta = (AICallable))
	static FString AddStructToContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& StructType, const FString& FieldValuesJson);

	/** Remove the selected element from a TSet/TArray/TMap. */
	UFUNCTION(meta = (AICallable))
	static FString RemoveStructFromContainer(const FString& ObjectPath, const FString& ContainerProperty, const FString& Selector);

	// ---- Semantic search (two-phase async) ----
	// Hybrid (vector + BM25) semantic search over the SemanticSearch-indexed asset folders.
	// Two-phase because querying embeds the text over HTTP and returns via a game-thread
	// callback: a UFUNCTION runs inside FTSTicker::Tick, so it must not block/pump. Start
	// returns a requestId immediately; poll Result until status is "done".

	/** Begin a semantic search. Query is embedded via the configured provider, fused with the
	 *  text index. TopK <= 0 defaults to 15. FolderFilter (optional path substring, e.g.
	 *  "ResourceIcons") restricts results. Returns {requestId, status:"pending"}. Requires an
	 *  embedding provider configured and a built index. */
	UFUNCTION(meta = (AICallable))
	static FString SemanticSearchStart(const FString& Query, int32 TopK, const FString& FolderFilter);

	/** Poll a search started with SemanticSearchStart. Returns {status:"pending"} until ready,
	 *  then {status:"done", results:[{path, rrf, vectorDistance, bm25}, ...]} ranked best-first
	 *  (and consumes the request). */
	UFUNCTION(meta = (AICallable))
	static FString SemanticSearchResult(int32 RequestId);

	// ---- Trace ----

	/** Query a .utrace file through the engine's own trace providers. TraceFilePath is a filesystem
	 *  path (e.g. "G:/ArcGame/ArcGame/Saved/Profiling/Foo.utrace"); analysis runs synchronously.
	 *
	 *  QueryJson (all fields optional; empty string = defaults):
	 *  {
	 *    "startTime": 0.0, "endTime": 0.0,            // endTime <= startTime means whole session
	 *    "sections": ["logs","counters","bookmarks","frames","categories"], // default: all but "categories"
	 *    "maxResults": 200,
	 *    "expectModules": ["MassProvider"],           // analysis modules that must be registered AND enabled
	 *    "logs":      {"category":"", "categoryContains":"", "minVerbosity":"Warning", "contains":""},
	 *    "counters":  {"nameContains":"Arc/Economy/", "includeValues":false, "maxValuesPerCounter":64},
	 *    "bookmarks": {"contains":"Arc/Economy/"},
	 *    "frames":    {"frameType":"game", "includeFrames":false}
	 *  }
	 *
	 *  Returns {trace, modules, logs, counters, bookmarks, frames, warnings}. Each section carries
	 *  "available": a section that is empty because its provider or analyzer module never ran says so
	 *  in "reason"/"analyzerModuleRegistered"/"analyzerModuleEnabled"/"modules.missing" instead of
	 *  looking like "no events". Only enabled modules instantiate analyzers, so "modules.missing"
	 *  lists expected modules that are not enabled (registered-but-disabled included) while
	 *  "modules.analysis[].enabled" keeps the registered-vs-enabled distinction visible.
	 *  "trace.analysisMessageCount" is non-zero when analysis itself reported errors/warnings — a
	 *  file that is not a trace, or a truncated one, yields zero events with a non-zero count. */
	UFUNCTION(meta = (AICallable))
	static FString QueryTrace(const FString& TraceFilePath, const FString& QueryJson);

	// ---- Records ----

	/** Find what references a JSON record id ("Type:Name", e.g. "ArcItem:3838454B44191775E9E17199ADF5F258").
	 *  assets: packages whose saved FArcNamedPrimaryAssetId values hold the id (Asset Registry searchable names;
	 *  packages saved before the marking existed are not found until resaved). records: every indexed record holding
	 *  the id in an FArcNamedPrimaryAssetId, with the property path. Bare FPrimaryAssetId fields are not searched.
	 *  An id without an indexed record is still searched ("indexed": false).
	 *  Returns {id, indexed, assets[], records[{id, file, property}]} or {error}; errors while the Asset Registry is loading. */
	UFUNCTION(meta = (AICallable))
	static FString FindRecordReferencers(const FString& RecordId);

	// ---- Tactics ----

	/** Export an ArcTactics debugger recording (.utrace written by the Tactics tab's recorder) to JSON.
	 *  TraceFilePath is a filesystem path; empty takes the newest .utrace in Saved/ArcTactics/Traces.
	 *  Analysis runs synchronously. The JSON is always written next to the trace with the extension
	 *  changed to .json (Foo.utrace -> Foo.json), overwriting it; it is never returned inline.
	 *
	 *  OptionsJson (all fields optional; empty string = defaults, unknown fields are an error):
	 *  {
	 *    "entity": {"index": 123, "serial": 4},   // only this Mass entity; serial optional
	 *    "fromTime": 0.0, "toTime": 10.0,          // seconds from the recording origin, inclusive
	 *    "queryItems": "all"                       // "all" | "valid" | "none"; "none" keeps each query's
	 *                                              // counts and its winner's location and score
	 *  }
	 *
	 *  File layout: {file, duration, worlds[], summary, entities[]}. Each entity has worldId (string),
	 *  entityIndex, entitySerial, configs (decision package names) and arrays decisions[], actions[], events[], paths[], queries[];
	 *  every element carries its original "index", and cross references (decision.queries, action.paths,
	 *  action.queries, event.span/path/query, query.decision/span) use those indices. A decision has
	 *  time, generation, selectionMode, packages[{config, minScore, active, excluded, diagnostic}], selectedOrder, selected (name or null), status,
	 *  diagnostic, position and candidates[]: {order, package, name, sourceIndex, fromSelector, target, score,
	 *  priority, outcome, reason, selected, considerations[{name, raw, curved, state}]} where state is
	 *  "scored" | "earlyOut" | "veto" | "notEvaluated" (raw/curved null when not evaluated). An action
	 *  span has package, name (the selector name for selector items), actionSerial, target, startTime, endTime, ended, openAtRecordingEnd, start/end
	 *  position and end {status, reason, cooldownStamped, cooldown, historyTag, historyAppended} or null.
	 *  A query has generation, sourceKind (GlobalTask | Step), sourceName, actionSerial, queryName,
	 *  queryId, status, selectionMode, querier, complete, decision, span, counts (totalItemCount, recordedItemCount, validItemCount,
	 *  selectedItemCount), winner, steps[{name, type, filteredCount}] and items[{index, location, score,
	 *  valid, selected, applied, stepRaw[], stepCumulative[]}]. Enums are names, locations are [x,y,z],
	 *  times are seconds from the recording origin.
	 *
	 *  Returns {success, input, output, sizeBytes, summary} or {success:false, error}. summary is
	 *  {file, duration, entityCount, unclaimedCandidateCount, unclaimedQueryCount, filter, entities[]};
	 *  each summary entity has worldId, entityIndex, entitySerial, configs, decisionCount,
	 *  candidates {<package>/<name>: {evaluated, selected, rejections {<outcome>: {<reason>: count}}}} and
	 *  actions {<package>/<name>: {spans, endStatus {<Succeeded|Failed|Cancelled|NotEnded>: count}}}, where spans
	 *  entered before the recording started are keyed "<enteredBeforeRecording>". Filters
	 *  apply to the summary too. An unreadable file, a file without ArcTactics entities, or an entity
	 *  filter that matches nothing is an error. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"))
	static FString ExportTacticsRecording(const FString& TraceFilePath, const FString& OptionsJson);

};
