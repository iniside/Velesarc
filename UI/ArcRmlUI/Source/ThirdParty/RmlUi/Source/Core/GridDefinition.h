// CSS Grid style values. Derived from Taffy (https://github.com/DioxusLabs/taffy) src/style/grid.rs at 66b3364.
// Taffy is MIT licensed: Copyright (c) 2018 Visly Inc., Copyright (c) 2026 Taffy Authors.
#pragma once

#include "../../Include/RmlUi/Core/NumericValue.h"
#include "../../Include/RmlUi/Core/Types.h"

namespace Rml {

enum class GridTrackSizingType : uint8_t { Auto, Length, Percentage, Calc, Flex, MinContent, MaxContent, FitContentLength, FitContentPercentage };

struct GridTrackSizingValue {
	GridTrackSizingType type = GridTrackSizingType::Auto;
	NumericValue value;
	CalcExpressionPtr calc;
};

struct GridTrackSize {
	GridTrackSizingValue min;
	GridTrackSizingValue max;
};

enum class GridRepeatType : uint8_t { None, Count, AutoFill, AutoFit };

struct GridTrackComponent {
	GridRepeatType repeat = GridRepeatType::None;
	int repeat_count = 1;
	Vector<GridTrackSize> tracks;
	// Repeat only: either empty or one name set per line of a single repetition (tracks.size() + 1).
	Vector<StringList> line_names;
};

struct GridTrackList {
	Vector<GridTrackComponent> components;
	// One name set per line between components: components.size() + 1 entries, or empty.
	Vector<StringList> line_names;
};

struct GridTemplateArea {
	String name;
	int row_start = 0;
	int row_end = 0;
	int column_start = 0;
	int column_end = 0;
};

struct GridTemplateAreas {
	Vector<GridTemplateArea> areas;
	int row_count = 0;
	int column_count = 0;
};

enum class GridPlacementType : uint8_t { Auto, Line, Span };

struct GridPlacement {
	GridPlacementType type = GridPlacementType::Auto;
	int index = 0;
	String name;
};

struct GridDefinition {
	enum class Type : uint8_t { TrackList, Areas, Placement };
	Type type = Type::TrackList;
	GridTrackList track_list;
	GridTemplateAreas areas;
	GridPlacement placement;
	String value;
};

} // namespace Rml
