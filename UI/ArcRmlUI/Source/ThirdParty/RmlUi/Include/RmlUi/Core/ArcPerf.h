#pragma once

#ifdef RMLUI_ARC_PERF

	#include "Header.h"
	#include "ObserverPtr.h"
	#include "Types.h"
	#include <cstdint>

namespace Rml {

class Element;
class StyleSheet;
struct OutOfFlowLayoutInputs;

namespace ArcPerf {

	// Exclusive time buckets. Nested scopes subtract their time from the enclosing bucket, so the buckets of one root call sum to its total.
	enum class Bucket : uint8_t {
		UpdateOther,
		RenderOther,
		LoadOther,

		ScrollController,
		HoverChain,
		EventDispatch,
		DataModel,
		DataViewUpdate,
		DataBindingCreate,
		ElementUpdate,
		ElementOnUpdate,
		AnimationSetup,
		AnimationAdvance,
		ElementScroll,
		UpdateProperties,
		DefinitionApply,
		DefinitionMatch,
		DefinitionCreate,
		PseudoElements,
		StyleCompute,
		StyleVarResolve,
		StyleVarShorthands,
		StyleFontFace,
		StyleFontFaceCreate,
		FontGlyphInstall,
		PropertyChange,
		EffectsInstance,
		DecoratorInstance,
		DomAppendChild,
		DomRemoveChild,
		DocumentLayout,
		LayoutBlock,
		LayoutFlex,
		LayoutGrid,
		LayoutTable,
		LayoutReplaced,
		LayoutTextLine,
		LayoutClampScroll,
		LayoutVerify,
		DocumentPosition,
		ReleaseDocuments,

		ElementRender,
		RenderOffsets,
		StackingContext,
		TransformState,
		EffectsData,
		FilterCompile,
		ShaderCompile,
		BackgroundBorder,
		BoxShadow,
		BoxShadowTexture,
		ElementOnRender,
		TextGeometry,
		TextFontEffects,
		TextShadows,
		FontAtlas,
		SvgHandle,
		SvgRasterize,
		SvgUpload,
		ImageInstall,
		ImageUpload,
		RenderInterfaceDraw,
		CompileGeometry,

		CompiledRead,
		XmlParse,
		HeaderProcess,
		Template,
		RcssFactory,
		RcssParse,
		RcssMerge,
		RcssCompile,
		RcssCombineSheets,
		RcssBuildIndex,
		StyleSheetChange,
		ElementInstance,
		ElementAttributes,
		TextInstance,

		Count
	};

	enum class Counter : uint8_t {
		ElementsUpdated,
		ElementsRendered,
		StyleComputes,
		StyleFirstComputes,
		StyleShareCandidates,
		StyleShareTuples,
		StyleShareHits,
		PropertiesComputed,
		DefinitionLookups,
		DefinitionsChanged,
		DefinitionsCreated,
		SelectorCandidates,
		SelectorCandidatesMatched,
		SelectorNodeTests,
		VarResolutions,
		VarShorthandExpansions,
		FontFaceLookups,
		FontHandleCreations,
		FontGlyphBatches,
		FontGlyphsInstalled,
		PropertyChangeCalls,
		PropertiesChanged,
		LayoutForcingChanges,
		ClassChanges,
		PseudoClassChanges,
		LayoutDirtyCalls,
		LayoutRuns,
		LayoutResidualPasses,
		LayoutResidualElements,
		LayoutBoundaryPasses,
		LayoutBoundaryFormats,
		LayoutFullRootDirty,
		LayoutFullNoBoundaryInputs,
		FormatCalls,
		FormatFinalFlex,
		FormatFinalGrid,
		FormatFinalBlock,
		FormatNestedFinal,
		FormatFlexMain,
		FormatFlexCross,
		FormatShrinkToFit,
		FormatGridItemWidth,
		FormatGridMeasure,
		FormatOther,
		FormatsInFlexMain,
		FormatsInFlexCross,
		FormatsInShrinkToFit,
		FormatsInFlexMaxContent,
		FormatsInFlexMinContent,
		FormatsInGridMaxContent,
		FormatsInGridMinContent,
		FormatsInGridItemWidth,
		FormatsInGridMeasure,
		MeasureShrinkToFitHits,
		MeasureShrinkToFitMisses,
		MeasureFlexMaxContentHits,
		MeasureFlexMaxContentMisses,
		MeasureFlexMinContentHits,
		MeasureFlexMinContentMisses,
		MeasureGridMaxContentHits,
		MeasureGridMaxContentMisses,
		MeasureGridMinContentHits,
		MeasureGridMinContentMisses,
		MeasureGridItemWidthHits,
		MeasureGridItemWidthMisses,
		MeasureFormatHits,
		MeasureFormatMisses,
		BoxesChanged,
		TextLines,
		AnimationsAdvanced,
		AnimationPropertySets,
		AnimationRestarts,
		UpdateRequests,
		EventsDispatched,
		DataModelUpdates,
		DataVariablesDirty,
		DataViewsUpdated,
		DataViewsCreated,
		DataControllersCreated,
		DataExpressionSeedMisses,
		EffectsInstanced,
		DecoratorsInstanced,
		EffectsDataReloads,
		FiltersCompiled,
		ShadersCompiled,
		BackgroundBorderRegens,
		BoxShadowLookups,
		BoxShadowCacheMisses,
		TextGeometryRegens,
		FontEffectsRegens,
		TextShadowRegens,
		SvgHandleLookups,
		SvgDocumentsParsed,
		SvgRasterizations,
		TexturesFontAtlas,
		TexturesSvg,
		TexturesBoxShadow,
		TexturesImage,
		TexturesOther,
		GeometriesMade,
		GeometriesCompiled,
		DrawCalls,
		ElementsInstanced,
		TextNodesInstanced,
		StyleSheetsParsed,
		StyleSheetCacheHits,
		StyleSheetMerges,

		Count
	};

	enum class ElementList : uint8_t {
		StyleTime,
		DefinitionMatch,
		LayoutTime,
		LayoutDirty,
		PropertyChanges,
		ClassChanges,
		AnimationTime,
		UpdateRequests,
		TextGeometry,
		BackgroundBorder,
		Effects,
		Svg,

		Count
	};

	enum class NamedList : uint8_t {
		ChangedProperties,
		VarResolutions,
		DirtyCssVariables,
		DirtyDataVariables,
		DataViewVariables,
		UpdateRequestSources,
		StyleSheetParse,
		StyleSheetCache,
		InstancedTags,
		DataBindingTypes,

		Count
	};

	struct ElementStat {
		ObserverPtr<Element> element;
		double ms = 0.0;
		int count = 0;
		UnorderedMap<String, int> details;
	};

	struct NamedStat {
		double ms = 0.0;
		int count = 0;
	};

	struct FrameStats {
		double bucket_ms[size_t(Bucket::Count)] = {};
		int bucket_calls[size_t(Bucket::Count)] = {};
		int64_t counters[size_t(Counter::Count)] = {};

		void Reset() { *this = FrameStats(); }
	};

	struct Sink {
		FrameStats frame;
		UnorderedMap<const Element*, ElementStat> elements[size_t(ElementList::Count)];
		UnorderedMap<String, NamedStat> named[size_t(NamedList::Count)];

		void ResetLists()
		{
			for (auto& list : elements)
				list.clear();
			for (auto& list : named)
				list.clear();
		}
	};

	struct AnimationInfo {
		ObserverPtr<Element> element;
		String property;
		String origin;
		String state;
		float duration = 0.f;
		int num_iterations = 0;
		int current_iteration = 0;
		bool alternate = false;
	};

	struct StyleSheetStats {
		int nodes = 0;
		int styled_nodes = 0;
		int indexed_by_id = 0;
		int indexed_by_class = 0;
		int indexed_by_tag = 0;
		int unindexed = 0;
		int id_keys = 0;
		int class_keys = 0;
		int tag_keys = 0;
		int cached_definitions = 0;
		int keyframes = 0;
		int named_decorators = 0;
	};

	struct LayoutMismatch {
		String element;
		String field;
		String optimized;
		String reference;
	};

	struct LayoutVerifyPass {
		String document;
		// False for the check of an update that ran no layout: any mismatch there is a missed invalidation.
		bool layout_ran = true;
		bool update_state = false;
		int dirty_before_reference = 0;
		int dirty_after_reference = 0;
		int elements = 0;
		int mismatched_elements = 0;
		int mismatches = 0;
		Vector<LayoutMismatch> first_mismatches;
	};

	struct LayoutVerifyLog {
		int max_mismatches = 0;
		Vector<LayoutVerifyPass> passes;
	};

	// Installs the sink that receives all measurements until the next call. A null sink or level 0 disables recording. UI-thread only.
	RMLUICORE_API void SetSink(Sink* sink, int level);

	// Null disables it. UI-thread only.
	RMLUICORE_API void SetLayoutVerify(LayoutVerifyLog* log);

	RMLUICORE_API const char* GetBucketName(Bucket bucket);
	RMLUICORE_API const char* GetCounterName(Counter counter);
	RMLUICORE_API const char* GetElementListName(ElementList list);
	RMLUICORE_API const char* GetNamedListName(NamedList list);

	RMLUICORE_API String DescribeElement(const Element* element, int num_ancestors);
	RMLUICORE_API void CollectAnimations(Element* root, Vector<AnimationInfo>& out_animations);
	RMLUICORE_API bool GetStyleSheetStats(const StyleSheet* style_sheet, StyleSheetStats& out_stats);
	// Null unless a containing block placed the element and nothing has set its offset since.
	RMLUICORE_API const OutOfFlowLayoutInputs* GetOutOfFlowLayoutInputs(const Element* element);

} // namespace ArcPerf
} // namespace Rml

#endif
