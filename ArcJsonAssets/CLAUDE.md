# ArcJsonAssets Plugin

Editor side of JSON asset records: Content Browser items, edit tab, hot reload, id picker sources, and the cook asset. The runtime core (type registry, content roots, file read/write, yyjson record serializer) lives in ArcCore `JsonAssets/`, and the index lives in `UArcCoreAssetManager` (see [ArcCore CLAUDE.md](../ArcCore/CLAUDE.md)). Records are parsed into USTRUCTs; no UObject per record. The asset manager keeps one shared parsed record per id; consumers read it through `FindJsonRecord`. Status and follow-ups: [docs/todo/json-asset-type-support.md](../../../../docs/todo/json-asset-type-support.md).

## Module Structure

- `ArcJsonAssets` (Runtime, no deny list). Only the module class and `Cook/` (`UArcJsonAssetsCookFiles`). Depends on ArcCore.
- `ArcJsonAssetsEditor` (Editor). Content Browser items, edit tab, watcher, id sources, `ArcJsonAssets.Resave`, `ArcJsonAssets.Convert`/`ConvertAll`. Depends on ArcCore, ArcCoreEditor, ContentBrowserFileDataSource.

## JSON Asset Type

A type is a file extension mapped to a USTRUCT. One record = one file = one asset. The file is plain JSON with the struct's fields: no header, no type name; the extension is the type. Files live anywhere under Content of the project or any project plugin. They are not committed (Content rule).

## Registering a Type

Call `ArcJsonAssets::RegisterType(FArcJsonAssetTypeInfo)` (`ArcCore/JsonAssets/ArcJsonAssetType.h`) in a **runtime module** `StartupModule`; call `UnregisterType(Extension)` in `ShutdownModule` (example: `ArcCraftModule.cpp`, `.arcrecipe`). The registering module depends on ArcCore, not on ArcJsonAssets.

Registered types. Each is the only form of its data (the data assets are deleted, consumers read the records by id):

| Extension | Struct | PrimaryAssetType | Editor |
|---|---|---|---|
| `arcrecipe` | `FArcRecipeDefinitionData` | `ArcRecipe` (record may set `RecipeType`) | generic tab; recipe graph (ArcCraftEditor) |
| `arcmaterialtable` | `FArcMaterialPropertyTableData` | `ArcMaterialPropertyTable` | ArcCraftEditor toolkit |
| `arcrandompool` | `FArcRandomPoolDefinitionData` | `ArcRandomPool` | ArcCraftEditor toolkit |
| `arcstattransfer` | `FArcStatTransferTableData` | `ArcStatTransferTable` | generic tab |
| `arcstaticmods` | `FArcStaticModifierSetData` | `ArcStaticModifierSet` | ArcCraftEditor toolkit |
| `arccraftrules` | `FArcCraftRulesData` | `ArcCraftRules` | generic tab |
| `arcstatcatalog` | `FArcStatCatalogData` | `ArcStatCatalog` | generic tab |
| `arcitem` | `FArcItemDefinitionData` (ArcMassItemsRuntime) | `ArcItem` (record may set `ItemType`) | item editor (ArcMassItemsEditor; save runs fragment `PreSave` and scalable `Initialize`); recipe graph creates items; record tags from `FArcItemFragment_Tags` go to the Asset Registry via `AddAssetRegistryTags` |

| Field | Meaning |
|---|---|
| `Extension` | File extension without dot, unique |
| `Struct` | The record USTRUCT |
| `PrimaryAssetType` | Default `FPrimaryAssetType` of the ids, unique, **not** the type of any scanned asset |
| `TypeProperty` | Optional: a serialized `FPrimaryAssetType` property of the struct; a record that sets it chooses its id type (`ItemType`, `RecipeType`) |
| `IdProperty` | Name of an `FGuid` property in the struct (`VisibleAnywhere`: the editor generates it on create); registration fails otherwise |
| `AddAssetRegistryTags` | Optional `(const FInstancedStruct& Record, FAssetDataTagMap& Tags)`: adds tags to the record's Asset Registry entry wherever the index builds it (initial walk, mount walk, reindex); runtime, not editor-gated. Items: `FArcItemFragment::GetAssetRegistryTags` of every `FragmentSet` element |

The id of a record is computed only by `ArcJsonAssets::MakeRecordId(Info, Record, OutError)` (type property or default type, name rule, GUID digits); the index and the editors use it.
| `DisplayName`, `Color` | Content Browser type name and color |

Registration must happen in a Default-phase (or earlier) module: those start before the asset manager's `StartInitialLoading`, which builds the index, and before the editor data source is created. Late types are unsupported (no index, no file actions, one warning on their first file change).

## File Format (`ArcJsonRecord`, yyjson)

`ArcCore/JsonAssets/ArcJsonRecordSerializer.h`; `ArcJsonAssetFile` is the only file read/write path (index, editor tab, `Resave`, `Dump`).

- UTF-8 bytes end to end; in-situ parse (the buffer is padded and rewritten, never reused). A UTF-8 BOM is accepted on read. Written pretty (two-space indent), LF, trailing newline, no BOM.
- Keys are property authored names, matched exactly. Unknown key = error; missing key = default.
- Written: top-level `CPF_Edit` properties that differ from the struct defaults; nested structs and instanced struct contents in full. `CPF_EditorOnly`, `CPF_Deprecated`, `CPF_Transient` are never read or written (cooked builds have no editor-only fields).
- Values: numbers/bools as JSON (integers range-checked); enums by name; FString/FName/FText as strings (FText is a plain string); FGuid as 32 hex digits; soft pointers and `FSoftObjectPath` as path strings; gameplay tag as name, container as array of names (unregistered tag = error); structs with native text export/import (`FPrimaryAssetId`, `FArcNamedPrimaryAssetId`) as their text (`"Type:Name"`); `FInstancedStruct` as an object with `_structType` (short name, `ArcStructTypeNames`) plus fields; maps as an object for string-like keys, else an array of `{"Key","Value"}`.
- Hard object references (`TObjectPtr`, raw pointers, `TSubclassOf`) as path strings, `null` for none. Read: `FindObject`, then `LoadObject`, so referenced assets load **when the file is parsed** (initial index, mount walk, reindex); the object must match the property class. Unresolved = error. The cooker does not see these paths (cooked use needs dependency registration, todo).
- `FGameplayTagQuery`: `{}` empty, else one key named by `EGameplayTagQueryExprType`: `{"AllTagsMatch": ["Item.Wood"]}`, `{"AnyExprMatch": [{...}, {...}]}`. The description is not stored.
- `FRichCurve` and `FRichCurveKey` write every reflected field (tangents, weights, extrapolation are not `CPF_Edit`).
- Instanced struct wrappers: a struct registered with `ArcJsonRecord::RegisterInstancedStructWrapper` (its `FInstancedStruct` member plus a post-import fixup) reads and writes exactly like `FInstancedStruct`. ArcMassItems registers `FArcInstancedStruct` (fixup `UpdateStructName`).
- The field rule (`IsSerializedField`), native-text check and wrapper lookup are exported from `ArcJsonRecordSerializer.h`; tools comparing records use them, never a copy.
- Errors: instanced subobject, weak, lazy and interface references, property bags and other unsupported kinds; messages carry field paths (`ModifierSources[2].Tags[0]: ...`). A file is accepted only with no errors.
- `BaseStruct`/`ExcludeBaseStruct` are checked only under `WITH_METADATA`: cooked builds trust editor validation.

## Ids and Index (`UArcCoreAssetManager`)

- Id = `FPrimaryAssetId(Type, Guid)`; the name is the GUID as 32 hex digits. Type = the record's `TypeProperty` value when set, else the registered `PrimaryAssetType`. Example: `.arcitem` with `"ItemType": "ArmorChestItem"` -> `ArmorChestItem:<guid>`.
- Record type rules (file rejected otherwise): one package-path segment (no `:`, `.`, `/`, `\`, whitespace, invalid long-package-name characters); not a scanned (non-dynamic) asset manager type; not another registration's default type; not in use by records of another extension (such a file waits and is retried when the type leaves use). Types in use = registered defaults plus custom types while an indexed id has them (`GetJsonRecordTypes`, `FindJsonRecordType`). A changed type on reindex works like a changed GUID.
- Duplicates are keyed by (extension, GUID): two files of one extension with the same GUID are duplicates even with different types.
- `AllowedTypes` lists only the named types: a recipe with a custom `RecipeType` is not listed under `ArcRecipe`. The editor adds a picker source for every type in use.
- Asset Registry entry per record: package `/ArcJsonIndex/<Type>/<Guid>`, asset name `<Guid>`, class path = the struct path. `/ArcJsonIndex` is unmounted, so the Content Browser does not show it. Tags: `PrimaryAssetType`, `PrimaryAssetName`, `SourcePath` (file package path), `DisplayName` (file base name), plus one per struct property with the `AssetRegistrySearchable` **UPROPERTY specifier** (not `meta`).
- Entries are appended with `AppendState`, then `AddDynamicAsset(Id, IndexObjectPath, MarkerBundle)` (one bundle entry, no paths: the engine registers a dynamic id only with a non-empty bundle list). Empty bundles unregister.
- Single source: the id set is the engine's `AssetMap` (`GetPrimaryAssetIdList`); id -> file is the `SourcePath` tag (`GetPrimaryAssetData`). The manager additionally stores `JsonRecords` (one parsed value per indexed id, written and removed on exactly the same paths as the id registration), file -> id, and the duplicate-GUID waiting list.
- Initial index: at the end of `StartInitialLoading` (after native tags), via `CallOrRegister_OnCompletedInitialScan`; not in the cook commandlet. Content roots mounted later are walked on mount; dismounted roots remove their files.
- Accepted files: clean parse, valid GUID, GUID not used by another file. A duplicate is rejected (both logged) and retried when the owning file goes away. A changed GUID removes the old id.
- `ReinitializeFromConfig` (editor) is overridden: JSON ids are removed before `Super` and re-added after, so the upstream `check(AssetPathMap.IsEmpty())` holds. No engine edit.
- Records: the index keeps the record it parsed (no second parse). A reload copies the new value into the existing record; a failed reload keeps the previous record and index entry. A removed id loses its record first, then `OnJsonRecordChanged` fires.
- API (game thread): `FindJsonRecord(Id, Struct)` / `FindJsonRecord<T>(Id)` returns a `const` pointer to the shared record (null when the id is not indexed; null plus an error when the struct does not match the type), `GetJsonRecordFilename`, `GetJsonRecordEntries(Type)` (id + display name, sorted), `ReindexJsonFile`, `RemoveJsonFile` (false before the initial index), `OnJsonRecordChanged()` (after the store update: reindex, removal, and per id indexed by a walk).
- Pointer lifetime: only the top-level record pointer survives a reload, until the record is removed. Anything inside it (array elements, strings, nested instanced struct memory) is valid only until the next `OnJsonRecordChanged(Id)`; listeners re-read there. Records are `const`; a consumer that needs its own snapshot copies it.
- Console: `ArcJsonAssets.Dump [PrimaryAssetType]` logs per record the id, file, export, `GetPrimaryAssetPath` and tags. Editor: `ArcJsonAssets.Resave <file or package path>` parses a file and writes it back. Editor: `ArcJsonAssets.Convert <extension> <asset object path> <output package path or dir>` copies a data asset's properties into the registered struct by name (same type required, id copied or new), writes `<dir>/<AssetName>.<ext>` (no overwrite, must be under a content root), reindexes, then parses the file back and logs every field that differs from the asset ("N differences"). Editor: `ArcJsonAssets.ConvertAll [-DryRun] <extension>=<class path> ... <package path> ...` does the same for every asset of exactly the listed classes (no subclasses) under the paths, each record next to its source asset; it skips existing records, continues past failures, prints a summary, every empty instanced struct element of a container (deleted struct type; left out of the record, listed per asset and property path) and every unregistered gameplay tag met (a record with one is not written). Both merge `Saved/ArcJsonAssets/ConvertMap.json` (source object path -> record id) by source path; `-DryRun` writes nothing.

## Referencing Records

Use `FArcNamedPrimaryAssetId` with `meta = (AllowedTypes = "<PrimaryAssetType>")`; stored value `Type:Guid`. The details picker lists the type's records by file name (ArcCoreEditor `ArcPrimaryAssetIdSources`, fed from `GetJsonRecordEntries`) and offers browse to the file. Example: `UArcMassCraftingStationTrait::AvailableRecipes` (`ArcRecipe`).

Who references a record: ArcMCP `FindRecordReferencers("Type:Name")` (uassets via Asset Registry searchable names, records via reflection; see [ArcMCP CLAUDE.md](../ArcMCP/CLAUDE.md)).

## Editor Behavior

- Files appear as Content Browser items through `UArcJsonAssetsFileDataSource` (engine `UContentBrowserFileDataSource`, one `FFileActions` per type). The data source is created after engine init and not in commandlets; it reads the content roots then and binds `ArcJsonAssetPaths::OnContentRootChanged()`.
- Create writes defaults with a **new GUID** through `ArcJsonRecordEditing::CreateRecordFile(Type, Filename)` (`Editing/ArcJsonRecordEditing.h`, exported; refuses an existing file, reindexes). Rename, move and delete are enabled.
- **Copy and duplicate are blocked**: the engine ignores `FFileActions::CanCopy`, so the data source overrides `CanCopyItem`. A copy made outside the editor repeats the id and is rejected as a duplicate.
- Record edit object (`Editing/ArcJsonRecordEditing.h`): one transient, transactional `UArcJsonRecordEditObject` per open file, shared by every tool. `AcquireRecord(Filename)` returns it (created from the file on first acquire), each non-null acquire is paired with one `ReleaseRecord`; it leaves the registry at the last release or on `Discard()`, and a later acquire reads the file again. Edits: `Modify()`, change `GetMutableRecord()`, `PostEditChange()` (struct details views do this through an `FNotifyHook`, see `SArcJsonAssetEditor`). Edit and undo/redo mark it dirty; `Save()` writes through `ArcJsonAssetFile` and reindexes. `OnChanged()` reports `Edited`, `UndoRedo`, `Reloaded`, `Saved`, `Discarded`, `Removed`; undo/redo and reload reallocate the record memory, so views re-point there. No Save As.
- Per-extension editors: `RegisterRecordEditor(Extension, FArcJsonRecordEditorOpen)` / `UnregisterRecordEditor`; `OpenRecordEditor` (and double-click) dispatches to it, else to the generic tab. A registered editor keeps one tab per file itself.
- Generic tab: a dockable struct editor tab per file (`IStructureDetailsView` over the edit object's record memory). Save button and Ctrl+S save the object; a magnifier button next to Save selects the file in the Content Browser (`ArcJsonRecordEditing::BrowseToRecordFile`, also behind the Browse button of the registered toolkits); Ctrl+Z/Ctrl+Y reach the editor's global undo when unhandled; dirty marker; closing a dirty tab asks save/discard (discard drops the edits for every holder). Use flat categories (Hard Rules).
- Hot reload: `IDirectoryWatcher` per root, debounced; folder renames/deletes included; removals are processed before additions and go through `RemoveJsonFile`/`ReindexJsonFile`, then reach the file's edit object. A clean object reloads as its own transaction ("Reload from disk") or is removed (its tabs close) when its file changes or disappears; a dirty object keeps its edits and warns. Its own save does not reload (bytes match).

## Cook

`/ArcJsonAssets/Cook/ArcJsonAssetsCookFiles` (`UArcJsonAssetsCookFiles`, a data asset with no fields) is listed in `DefaultGame.ini` `DirectoriesToAlwaysCook`. Its `CookAdditionalFilesOverride` writes every registered-extension file to `<CookDir>/<ProjectName>/<project-relative path>` (files must be under the project dir); its cook dependency hashes the files and the extension list. Files ship as text and are parsed at the initial index. Cook asset is Content (uncommitted); do not delete it.

## Hard Rules

- **Never** call `LoadPrimaryAsset` / `GetPrimaryAssetObject` for these types (no package; the latter returns null). Use `FindJsonRecord`.
- **Never** list a JSON type in `PrimaryAssetTypesToScan` (dynamic-only types cannot also be scanned).
- **No `|` sub-categories on a JSON type's own properties.** The editor tab is a struct-rooted details view; the stock engine folds `A|B` categories there and destroys the sub-category, leaving nested ticking rows (`FInstancedStruct`) dangling in `FDetailLayoutBuilderImpl::TickableNodes` → crash on the next tick (`DetailLayoutBuilderImpl.cpp:460, 512-580`, `DetailItemNode.cpp:85-89`). Use flat categories.
- **Asset Registry entries go stale after a file is deleted** (no public remove) until restart or a rescan. Registry queries over `/ArcJsonIndex` must confirm the id with the asset manager (`GetPrimaryAssetPath`).
- Record structs may hold hard object references (written as paths, loaded at parse). Prefer id or soft references in new record fields; references between JSON types are ids.
