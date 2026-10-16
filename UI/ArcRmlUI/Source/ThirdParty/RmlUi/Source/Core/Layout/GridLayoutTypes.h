// CSS Grid layout types. Ported from Taffy (https://github.com/DioxusLabs/taffy) src/compute/grid at 66b3364, without subgrid.
// Taffy is MIT licensed: Copyright (c) 2018 Visly Inc., Copyright (c) 2026 Taffy Authors.
#pragma once

#include "../../../Include/RmlUi/Core/StyleTypes.h"
#include "../../../Include/RmlUi/Core/Types.h"
#include "../GridDefinition.h"
#include <optional>

namespace Rml {

class Element;

using GridOptional = std::optional<float>;

// Inline is the horizontal (column) axis, Block the vertical (row) axis.
enum class GridAxis : uint8_t { Inline, Block };

inline GridAxis GridOtherAxis(GridAxis axis)
{
	return axis == GridAxis::Inline ? GridAxis::Block : GridAxis::Inline;
}

constexpr int GridMaxTracks = 10000;
constexpr int GridMinOzLine = -GridMaxTracks;
constexpr int GridMaxOzLine = GridMaxTracks;

struct GridOptionalSize {
	GridOptional width;
	GridOptional height;

	GridOptional Get(GridAxis axis) const { return axis == GridAxis::Inline ? width : height; }
	void Set(GridAxis axis, GridOptional value) { (axis == GridAxis::Inline ? width : height) = value; }
	GridOptionalSize With(GridAxis axis, GridOptional value) const
	{
		GridOptionalSize result = *this;
		result.Set(axis, value);
		return result;
	}
	bool operator==(const GridOptionalSize& other) const { return width == other.width && height == other.height; }
};

struct GridAvailableSpace {
	enum class Type : uint8_t { Definite, MinContent, MaxContent };
	Type type = Type::MaxContent;
	float value = 0.f;

	static GridAvailableSpace Definite(float value) { return GridAvailableSpace{Type::Definite, value}; }
	static GridAvailableSpace MinContent() { return GridAvailableSpace{Type::MinContent, 0.f}; }
	static GridAvailableSpace MaxContent() { return GridAvailableSpace{Type::MaxContent, 0.f}; }

	bool IsDefinite() const { return type == Type::Definite; }
	float ComputeFreeSpace(float used_space) const;
};

struct GridAvailableSize {
	GridAvailableSpace width;
	GridAvailableSpace height;

	GridAvailableSpace Get(GridAxis axis) const { return axis == GridAxis::Inline ? width : height; }
	void Set(GridAxis axis, GridAvailableSpace value) { (axis == GridAxis::Inline ? width : height) = value; }
};

// Origin-zero line range: line 0 is the start edge of the explicit grid.
struct GridLineRange {
	int start = 0;
	int end = 0;

	int Span() const { return end > start ? end - start : 0; }
};

struct GridTrackCounts {
	int negative_implicit = 0;
	int explicit_count = 0;
	int positive_implicit = 0;

	int Len() const { return negative_implicit + explicit_count + positive_implicit; }
	int ImplicitStartLine() const { return -negative_implicit; }
	int ImplicitEndLine() const { return explicit_count + positive_implicit; }
	int OzLineToNextTrack(int line) const { return line + negative_implicit; }
	int TrackToPrevOzLine(int index) const { return index - negative_implicit; }
	int OzLineToTrackVecIndex(int line) const { return 2 * (line + negative_implicit); }
	bool TryOzLineToTrackVecIndex(int line, int& out_index) const
	{
		if (line < -negative_implicit || line > explicit_count + positive_implicit)
			return false;
		out_index = OzLineToTrackVecIndex(line);
		return true;
	}
};

enum class GridSizingKind : uint8_t { Auto, Length, Percentage, Calc, Fr, MinContent, MaxContent, FitContentLength, FitContentPercentage };

// A resolved track sizing function. Length values are in px, percentages are fractions of the basis, fr values are the flex factor.
struct GridSizingFunction {
	GridSizingKind kind = GridSizingKind::Auto;
	float value = 0.f;
	CalcExpressionPtr calc;

	static GridSizingFunction Zero() { return GridSizingFunction{GridSizingKind::Length, 0.f}; }

	bool IsAuto() const { return kind == GridSizingKind::Auto; }
	bool IsFr() const { return kind == GridSizingKind::Fr; }
	bool IsMinContent() const { return kind == GridSizingKind::MinContent; }
	bool IsMaxContent() const { return kind == GridSizingKind::MaxContent; }
	bool IsFitContent() const { return kind == GridSizingKind::FitContentLength || kind == GridSizingKind::FitContentPercentage; }
	bool IsMaxOrFitContent() const { return IsMaxContent() || IsFitContent(); }
	bool IsMaxContentAlike() const { return IsAuto() || IsMaxContent() || IsFitContent(); }
	bool IsMinOrMaxContent() const { return IsMinContent() || IsMaxContent(); }
	bool IsIntrinsic() const { return IsAuto() || IsMinContent() || IsMaxContent() || IsFitContent(); }
	bool IsLengthOrPercentage() const { return kind == GridSizingKind::Length || kind == GridSizingKind::Percentage || kind == GridSizingKind::Calc; }
	bool UsesPercentage() const
	{
		return kind == GridSizingKind::Percentage || kind == GridSizingKind::Calc || kind == GridSizingKind::FitContentPercentage;
	}

	bool HasDefiniteValue(GridOptional parent_size) const;
	GridOptional DefiniteValue(GridOptional parent_size) const;
	GridOptional DefiniteLimit(GridOptional parent_size) const;
};

struct GridResolvedTrackSize {
	GridSizingFunction min;
	GridSizingFunction max;

	bool HasFixedComponent() const { return min.IsLengthOrPercentage() || max.IsLengthOrPercentage(); }
};

struct GridResolvedComponent {
	GridRepeatType repeat = GridRepeatType::None;
	int repeat_count = 1;
	Vector<GridResolvedTrackSize> tracks;
	const Vector<StringList>* line_names = nullptr;

	bool IsAutoRepetition() const { return repeat == GridRepeatType::AutoFill || repeat == GridRepeatType::AutoFit; }
};

struct GridResolvedTrackList {
	Vector<GridResolvedComponent> components;
	const Vector<StringList>* line_names = nullptr;
};

GridResolvedTrackList ResolveGridTrackList(const GridDefinition* definition, Element* element);
GridSizingFunction ResolveGridGap(Style::LengthPercentage gap);

struct GridTrack {
	bool is_gutter = false;
	bool is_collapsed = false;
	GridSizingFunction min_track_sizing_function;
	GridSizingFunction max_track_sizing_function;
	float offset = 0.f;
	float base_size = 0.f;
	float growth_limit = 0.f;
	float content_alignment_adjustment = 0.f;
	float item_incurred_increase = 0.f;
	float base_size_planned_increase = 0.f;
	float growth_limit_planned_increase = 0.f;
	bool infinitely_growable = false;

	static GridTrack Track(GridSizingFunction min, GridSizingFunction max);
	static GridTrack Gutter(GridSizingFunction size);

	void Collapse();
	bool IsFlexible() const { return max_track_sizing_function.IsFr(); }
	bool UsesPercentage() const { return min_track_sizing_function.UsesPercentage() || max_track_sizing_function.UsesPercentage(); }
	bool HasIntrinsicSizingFunction() const { return min_track_sizing_function.IsIntrinsic() || max_track_sizing_function.IsIntrinsic(); }
	float FitContentLimit(GridOptional axis_available_grid_space) const;
	float FitContentLimitedGrowthLimit(GridOptional axis_available_grid_space) const;
	float FlexFactor() const { return max_track_sizing_function.IsFr() ? max_track_sizing_function.value : 0.f; }
};

enum class GridPlacementKind : uint8_t { Auto, Line, Span };

struct GridPlacementLine {
	GridPlacementKind kind = GridPlacementKind::Auto;
	int value = 0;
};

// A placement along one axis. Before conversion the line values are CSS grid lines (1-based, negative from the end), after conversion origin-zero lines.
struct GridAxisPlacement {
	GridPlacementLine start;
	GridPlacementLine end;

	bool IsDefinite() const { return start.kind == GridPlacementKind::Line || end.kind == GridPlacementKind::Line; }
	GridAxisPlacement IntoOriginZero(int explicit_track_count) const;
	GridLineRange ResolveDefiniteGridLines() const;
	void ResolveAbsolutelyPositionedGridLines(std::optional<int>& out_start, std::optional<int>& out_end) const;
	int IndefiniteSpan() const;
};

enum class GridContentAlignment : uint8_t { Start, End, Center, Stretch, SpaceBetween, SpaceAround, SpaceEvenly };

enum class GridTrackSizeEstimate : uint8_t { MaxTrackSizingFunction, BaseSize };

struct GridItem {
	Element* element = nullptr;
	int source_order = 0;

	GridLineRange row;
	GridLineRange column;
	GridLineRange row_indexes;
	GridLineRange column_indexes;

	// Style, indexed [0] = horizontal, [1] = vertical; box edges indexed top, right, bottom, left.
	Style::LengthPercentageAuto size[2];
	Style::LengthPercentage min_size[2];
	bool min_size_auto[2] = {true, true};
	Style::LengthPercentage max_size[2];
	Style::LengthPercentage padding[4];
	float border[4] = {};
	Style::LengthPercentageAuto margin[4];
	bool border_box_sizing = false;
	float aspect_ratio = 0.f;
	Style::Overflow overflow[2] = {Style::Overflow::Visible, Style::Overflow::Visible};
	Style::AlignSelf align_self = Style::AlignSelf::Stretch;
	Style::AlignSelf justify_self = Style::AlignSelf::Stretch;

	GridOptional baseline;
	float baseline_shim = 0.f;

	bool crosses_flexible_row = false;
	bool crosses_flexible_column = false;
	bool crosses_intrinsic_row = false;
	bool crosses_intrinsic_column = false;

	std::optional<GridOptionalSize> grid_area_size_cache;
	bool has_known_dimensions_cache = false;
	GridOptionalSize known_dimensions_cache_area;
	GridOptionalSize known_dimensions_cache_value;
	GridOptionalSize min_content_contribution_cache;
	GridOptionalSize minimum_contribution_cache;
	GridOptionalSize max_content_contribution_cache;

	float y_position = 0.f;
	float height = 0.f;
	GridOptional final_baseline;

	GridLineRange Placement(GridAxis axis) const { return axis == GridAxis::Block ? row : column; }
	GridLineRange PlacementIndexes(GridAxis axis) const { return axis == GridAxis::Block ? row_indexes : column_indexes; }
	int TrackRangeStart(GridAxis axis) const { return PlacementIndexes(axis).start + 1; }
	int TrackRangeEnd(GridAxis axis) const { return PlacementIndexes(axis).end; }
	int Span(GridAxis axis) const { return Placement(axis).Span(); }
	bool CrossesFlexibleTrack(GridAxis axis) const { return axis == GridAxis::Inline ? crosses_flexible_column : crosses_flexible_row; }
	bool CrossesIntrinsicTrack(GridAxis axis) const { return axis == GridAxis::Inline ? crosses_intrinsic_column : crosses_intrinsic_row; }
	bool IsScrollContainer(GridAxis axis) const { return overflow[axis == GridAxis::Inline ? 0 : 1] != Style::Overflow::Visible; }
	bool HasAutoBlockMargin() const { return margin[0].type == Style::LengthPercentageAuto::Auto || margin[2].type == Style::LengthPercentageAuto::Auto; }
	bool HasCyclicBlockSizeDependency() const;
	bool ParticipatesInBaselineAlignment() const;
};

// Measures grid items through the host layout engine. Sizes are border-box sizes excluding margins.
class GridItemMeasurer {
public:
	virtual float MeasureChildSize(const GridItem& item, GridAxis axis, GridOptionalSize known_dimensions, GridOptionalSize parent_size,
		GridAvailableSize available_space) const = 0;
	virtual void MeasureChildBaseline(const GridItem& item, GridOptionalSize parent_size, float& out_baseline, float& out_height) const = 0;

protected:
	~GridItemMeasurer() = default;
};

enum class GridAbsoluteAxis : uint8_t { Horizontal, Vertical };

inline GridAbsoluteAxis GridOtherAbsoluteAxis(GridAbsoluteAxis axis)
{
	return axis == GridAbsoluteAxis::Horizontal ? GridAbsoluteAxis::Vertical : GridAbsoluteAxis::Horizontal;
}

enum class GridCellOccupancyState : uint8_t { Unoccupied, DefinitelyPlaced, AutoPlaced };

class GridCellOccupancyMatrix {
public:
	GridCellOccupancyMatrix(GridTrackCounts columns, GridTrackCounts rows);

	const GridTrackCounts& TrackCounts(GridAbsoluteAxis axis) const { return axis == GridAbsoluteAxis::Horizontal ? columns : rows; }

	void MarkAreaAs(GridAbsoluteAxis primary_axis, GridLineRange primary_span, GridLineRange secondary_span, GridCellOccupancyState value);
	bool LineAreaCollisionJump(GridAbsoluteAxis primary_axis, GridLineRange primary_span, GridLineRange secondary_span, int& out_next_position) const;
	bool OccupiedTrackJump(GridAbsoluteAxis axis, GridLineRange span, int& out_next_position) const;
	bool RowIsOccupied(int row_index) const;
	bool ColumnIsOccupied(int column_index) const;
	bool LastOfType(GridAbsoluteAxis track_type, int start_at, GridCellOccupancyState kind, int& out_line) const;

private:
	void ExpandToFitRange(GridLineRange row_span, GridLineRange column_span);
	GridCellOccupancyState Get(int row_index, int column_index) const { return cells[size_t(row_index * columns.Len() + column_index)]; }

	GridTrackCounts columns;
	GridTrackCounts rows;
	Vector<GridCellOccupancyState> cells;
};

class GridNamedLineResolver {
public:
	GridNamedLineResolver(const GridResolvedTrackList& columns, const GridResolvedTrackList& rows, const GridTemplateAreas* areas,
		int column_auto_repetitions, int row_auto_repetitions);

	int AreaColumnCount() const { return area_column_count; }
	int AreaRowCount() const { return area_row_count; }
	void SetExplicitColumnCount(int count) { explicit_column_count = count; }
	void SetExplicitRowCount(int count) { explicit_row_count = count; }

	GridAxisPlacement ResolveColumnNames(const GridPlacement* start, const GridPlacement* end) const;
	GridAxisPlacement ResolveRowNames(const GridPlacement* start, const GridPlacement* end) const;

private:
	using LineMap = UnorderedMap<String, Vector<int>>;

	static void AddTemplateLineNames(LineMap& lines, const GridResolvedTrackList& tracks, int auto_repetitions);
	static GridAxisPlacement ResolveLineNames(const LineMap& lines, int explicit_track_count, const GridPlacement* start, const GridPlacement* end);

	LineMap row_lines;
	LineMap column_lines;
	int area_column_count = 0;
	int area_row_count = 0;
	int explicit_column_count = 0;
	int explicit_row_count = 0;
};

void ComputeGridExplicitSizeInAxis(const GridResolvedTrackList& track_template, GridSizingFunction gap, GridOptional auto_fit_container_size,
	bool max_repetitions_that_do_not_overflow, int& out_auto_repetition_count, int& out_template_track_count);

void InitializeGridTracks(Vector<GridTrack>& tracks, GridTrackCounts counts, const GridResolvedTrackList& track_template,
	const GridResolvedTrackList& auto_tracks, GridSizingFunction gap, int auto_repetition_count, const GridCellOccupancyMatrix& occupancy,
	GridAbsoluteAxis axis);

struct GridItemPlacementStyle {
	GridAxisPlacement column;
	GridAxisPlacement row;
};

void ComputeGridSizeEstimate(int explicit_column_count, int explicit_row_count, const Vector<GridItemPlacementStyle>& placements,
	GridTrackCounts& out_column_counts, GridTrackCounts& out_row_counts);

void PlaceGridItems(GridCellOccupancyMatrix& occupancy, Vector<GridItem>& items, const Vector<GridItemPlacementStyle>& placements,
	Style::GridAutoFlow auto_flow);

void ResolveGridItemTrackIndexes(Vector<GridItem>& items, GridTrackCounts column_counts, GridTrackCounts row_counts);
void DetermineIfGridItemsCrossFlexibleOrIntrinsicTracks(Vector<GridItem>& items, const Vector<GridTrack>& columns, const Vector<GridTrack>& rows);

void RunGridTrackSizingAlgorithm(const GridItemMeasurer& tree, GridAxis axis, GridOptional axis_min_size, GridOptional axis_max_size,
	GridContentAlignment axis_alignment, GridContentAlignment other_axis_alignment, GridAvailableSize available_grid_space,
	GridOptionalSize inner_node_size, Vector<GridTrack>& axis_tracks, Vector<GridTrack>& other_axis_tracks, Vector<GridItem>& items,
	GridTrackSizeEstimate estimate, bool has_baseline_aligned_item);

GridOptionalSize GridItemAreaSize(const GridItem& item, GridAxis axis, const Vector<GridTrack>& axis_tracks, const Vector<GridTrack>& other_axis_tracks,
	GridOptionalSize available_space, GridTrackSizeEstimate estimate);
float GridItemMinContentContribution(GridItem& item, GridAxis axis, const GridItemMeasurer& tree, GridOptionalSize grid_area_size,
	GridOptionalSize available_space);

void AlignGridTracks(float grid_container_content_box_size, Vector<GridTrack>& tracks, GridContentAlignment track_alignment_style);

struct GridItemAxisAlignment {
	float position = 0.f;
	float margin_start = 0.f;
	float margin_end = 0.f;
};
GridItemAxisAlignment AlignGridItemWithinArea(float area_start, float area_end, Style::AlignSelf alignment_style, float resolved_size,
	GridOptional margin_start, GridOptional margin_end, float baseline_shim);

} // namespace Rml
