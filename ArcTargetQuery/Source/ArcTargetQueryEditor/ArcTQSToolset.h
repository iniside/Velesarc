// Copyright Lukasz Baran. All Rights Reserved.

#pragma once

#include "ToolsetRegistry/UToolsetRegistry.h"

#include "ArcTQSToolset.generated.h"

UCLASS()
class UArcTQSToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/** List concrete target query (TQS) node types usable in ApplyTQSDefinition. Role: Generator, Step, Function or empty for all. Filter: optional substring of the type or display name. Each type has name (the value for "type" in ApplyTQSDefinition), displayName, role, description, and fields; a Function type also has output (the name of its single output field, always bound implicitly) and outputType. Each field has name, C++ type and required (true when the field carries ArcTQSRequiredBinding and the definition does not compile until it is bound); a Function field also has output (true for the output field, which is read-only); when present a field also has baseStruct, default (the default value in the JSON form ApplyTQSDefinition accepts), tooltip, clampMin and clampMax (numbers from the ClampMin/ClampMax meta), units (Units meta) and enumValues (the accepted enum value names). The result also lists sources: the readable inputs Parameters (the definition's parameter bag; readable by Generator and steps), Querier (Entity, Location, Forward, Actor; readable by Generator and steps) and Item (Location, Entity of the item being evaluated; readable by steps only, per item), each with its fields and readableBy. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"))
	static FString ListTQSNodeTypes(const FString& Role, const FString& Filter);

	/** Export the UArcTQSQueryDefinition at Path (e.g. /ArcRPG/Enemies/TQS_Patrol) as version-1 JSON that ApplyTQSDefinition accepts unchanged: {"version": 1, "parameters": [{name, type, valueType?, container?, value?}], "selection": {mode, topN, minPassingScore, topPercent}, "generator": {type, data?} or null, "steps": [{type, data?}], "bindings": [...]}. A node's data lists only editable fields that differ from the type defaults; node IDs are never written. Bindings are sorted by target (Generator, Steps[0], Steps[1], ...) and then by path. A plain binding is the string "Steps[0].ReferenceLocation <- Querier.Location". A property-function binding is an object {"to": "Generator.Center", "function": {"type", "data"?, "bindings"?}} whose function bindings set the function's own inputs by bare field name ("Entity <- Querier.Entity") or by a nested function object ({"to": "Entity", "function": {...}}); the function output feeds "to" implicitly. Bindings whose endpoints or paths no longer resolve are dropped and listed in "warnings". */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Read"))
	static FString ExportTQSDefinition(const FString& Path);

	/** Rebuild the whole UArcTQSQueryDefinition at Path from DefinitionJson (the ExportTQSDefinition format, "version": 1) and compile it; a missing asset is created at Path (a long package path such as /ArcRPG/Enemies/TQS_New; an explicit object name must equal the package short name). Keys: "options" {"dryRun": bool, "save": bool}; "parameters" bag entries {name, type (EPropertyBagPropertyType: Bool, Int32, Float, Double, Name, String, Enum, Struct, Object, ...), valueType (Enum/Struct/Object/Class types), container (Array or Set), value}; "selection" {mode (an EArcTQSSelectionMode name such as HighestScore, TopN, AllPassing, RandomWeighted, RandomFromTopPercent, WeightedRandomFromTopPercent), topN (integer >= 1), minPassingScore, topPercent (1..100)}, missing keys use the class defaults; "generator" is {type, data?} or null; "steps" is an ordered array of {type, data?}; type is a name from ListTQSNodeTypes and data sets editable fields (an instanced struct value needs "_structType"). "bindings" is an array of strings "to <- from" or function objects. Targets are Generator.<path> or Steps[i].<path>; sources are Parameters.<name>, Querier.<field> and Item.<field>; paths are property names with optional [index] (e.g. Steps[2].Offsets[0].X). Read access: Generator reads Parameters and Querier; steps also read Item (Location, Entity; per item). A function object {"to": <target>, "function": {"type": <Function type>, "data"?: {...}, "bindings"?: [...]}} computes a value: its inner bindings target the function's own input fields by bare name ("Entity <- Querier.Entity") and may read any source the root node may read, or nest another function object ({"to": "Entity", "function": {...}}); the function output is bound to "to" implicitly. Each target path takes one binding. Unknown top-level keys are rejected. Every type, value, path and access rule is validated and the result is compiled on a scratch copy first: if anything fails (including any compile error) nothing changes and no asset is created. A parameter whose name, type, container and valueType are unchanged keeps its identity, so an unchanged parameter schema keeps existing references valid; when the schema changes, warnings list the referencing packages to re-apply, recompile or re-set. dryRun only validates and compiles the scratch copy; save saves the package. Returns {success, asset, compiled, revision, errors[], warnings[], diagnostics[{node, message, error}], dryRun, created}; diagnostics[].node names the endpoint (Generator, Steps[2]) or the full function chain (Generator.Center/Entity). */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"))
	static FString ApplyTQSDefinition(const FString& Path, const FString& DefinitionJson);

	/** Compile the UArcTQSQueryDefinition at Path exactly like the editor (repair node IDs, compile, notify listeners) and return {success, asset, compiled, revision, errors[], warnings[], diagnostics[{node, message, error}]}. compiled is true only when the definition is executable; diagnostics[].node names the endpoint (Generator, Steps[2]) or the full function chain (Generator.Center/Entity). */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"))
	static FString CompileTQSDefinition(const FString& Path);

	/** Set an FArcTQSQueryReference property on any object other than an ArcTactics config (for those use ApplyTacticsConfig with {"definition", "parameters", "overrides"}). ObjectPath: an asset or object path; a Blueprint path (e.g. /ArcRPG/Enemies/BP_TestEnemySpawner) edits its generated class defaults. PropertyName: the top-level FArcTQSQueryReference property (e.g. SpawnQuery). DefinitionPath: the UArcTQSQueryDefinition to reference, or empty to clear it. ParametersJson: empty or a JSON object {"<parameter name>": value} of definition parameters to override; every listed parameter is marked overridden, all others follow the definition defaults. The reference is rebuilt from the definition (its parameters resynced), validated, then written in one transaction with PostEditChange; a Blueprint is marked modified. For class defaults, loaded instances that still had the old default are updated too (propagated), and loaded instances that override the property are listed in warnings; unloaded levels are not inspected. Nothing is saved. Returns {success, object, property, definition, parameters (the overridden values), propagated, errors[], warnings[]}. */
	UFUNCTION(meta = (AICallable, AIAccessMode = "Write"))
	static FString SetTQSReference(
		const FString& ObjectPath,
		const FString& PropertyName,
		const FString& DefinitionPath,
		const FString& ParametersJson);
};
