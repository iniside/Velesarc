#include "DecoratorGradient.h"
#include "../../Include/RmlUi/Core/ComputedValues.h"
#include "../../Include/RmlUi/Core/Element.h"
#include "../../Include/RmlUi/Core/ElementUtilities.h"
#include "../../Include/RmlUi/Core/Geometry.h"
#include "../../Include/RmlUi/Core/Math.h"
#include "../../Include/RmlUi/Core/MeshUtilities.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "ComputeProperty.h"
#include "DecoratorShader.h"
#include <float.h>

namespace Rml {

// Returns the point along the input line ('line_point', 'line_vector') closest to the input 'point'.
static Vector2f IntersectionPointToLineNormal(const Vector2f point, const Vector2f line_point, const Vector2f line_vector)
{
	const Vector2f delta = line_point - point;
	return line_point - delta.DotProduct(line_vector) * line_vector;
}

/// Convert all color stop positions to normalized numbers.
/// @param[in] element The element to resolve lengths against.
/// @param[in] gradient_line_length The length of the gradient line, along which color stops are placed.
/// @param[in] soft_spacing The desired minimum distance between stops to avoid aliasing, in normalized number units.
/// @param[in] unresolved_stops
/// @return A list of resolved color stops, all in number units.
static ColorStopList ResolveColorStops(Element* element, const float gradient_line_length, const float soft_spacing,
	const ColorStopList& unresolved_stops)
{
	ColorStopList stops = unresolved_stops;
	const int num_stops = (int)stops.size();

	// Resolve all lengths, percentages, and angles to numbers. After this step all stops with a unit other than Number are considered as Auto.
	for (ColorStop& stop : stops)
	{
		if (stop.current_color)
			stop.color = element->GetComputedValues().color().ToPremultiplied();
		if (Any(stop.position.unit & Unit::LENGTH))
		{
			const float resolved_position = element->ResolveLength(stop.position);
			stop.position = NumericValue(resolved_position / gradient_line_length, Unit::NUMBER);
		}
		else if (stop.position.unit == Unit::PERCENT)
		{
			stop.position = NumericValue(stop.position.number * 0.01f, Unit::NUMBER);
		}
		else if (Any(stop.position.unit & Unit::ANGLE))
		{
			stop.position = NumericValue(ComputeAngle(stop.position) * (1.f / (2.f * Math::RMLUI_PI)), Unit::NUMBER);
		}
	}

	// Resolve auto positions of the first and last color stops.
	auto resolve_edge_stop = [](ColorStop& stop, float auto_to_number) {
		if (stop.position.unit != Unit::NUMBER)
			stop.position = NumericValue(auto_to_number, Unit::NUMBER);
	};
	resolve_edge_stop(stops[0], 0.f);
	resolve_edge_stop(stops[num_stops - 1], 1.f);

	// Ensures that color stop positions are strictly increasing, and have at least 1px spacing to avoid aliasing.
	auto nudge_stop = [prev_position = stops[0].position.number](ColorStop& stop, bool update_prev = true) mutable {
		stop.position.number = Math::Max(stop.position.number, prev_position);
		if (update_prev)
			prev_position = stop.position.number;
	};
	int auto_begin_i = -1;

	// Evenly space stops with sequential auto indices, and nudge stop positions to ensure strictly increasing positions.
	for (int i = 1; i < num_stops; i++)
	{
		ColorStop& stop = stops[i];
		if (stop.position.unit != Unit::NUMBER)
		{
			// Mark the first of any consecutive auto stops.
			if (auto_begin_i < 0)
				auto_begin_i = i;
		}
		else if (auto_begin_i < 0)
		{
			// The stop has a definite position and there are no previous autos to handle, just ensure it is properly spaced.
			nudge_stop(stop);
		}
		else
		{
			// Space out all the previous auto stops, indices [auto_begin_i, i).
			nudge_stop(stop, false);
			const int num_auto_stops = i - auto_begin_i;
			const float t0 = stops[auto_begin_i - 1].position.number;
			const float t1 = stop.position.number;

			for (int j = 0; j < num_auto_stops; j++)
			{
				const float fraction_along_t0_t1 = float(j + 1) / float(num_auto_stops + 1);
				stops[j + auto_begin_i].position = NumericValue(t0 + (t1 - t0) * fraction_along_t0_t1, Unit::NUMBER);
				nudge_stop(stops[j + auto_begin_i]);
			}

			nudge_stop(stop);
			auto_begin_i = -1;
		}
	}

	// Ensures that stops are placed some minimum distance from each other to avoid aliasing, if possible.
	for (int i = 1; i < num_stops - 1; i++)
	{
		const float p0 = stops[i - 1].position.number;
		const float p1 = stops[i].position.number;
		const float p2 = stops[i + 1].position.number;
		float& new_position = stops[i].position.number;

		if (p1 - p0 < soft_spacing)
		{
			if (p2 - p0 < 2.f * soft_spacing)
				new_position = 0.5f * (p2 + p0);
			else
				new_position = p0 + soft_spacing;
		}
	}

	RMLUI_ASSERT(std::all_of(stops.begin(), stops.end(), [](auto&& stop) { return stop.position.unit == Unit::NUMBER; }));

	return stops;
}


namespace {
	struct ClipVertex {
		Vector2f position;
		ColourbPremultiplied colour;
	};
} // namespace

static void ClipPolygonToEdge(Vector<ClipVertex>& polygon, Vector<ClipVertex>& scratch, int axis, float bound, bool keep_greater)
{
	scratch.clear();
	const int count = (int)polygon.size();
	for (int i = 0; i < count; i++)
	{
		const ClipVertex& current = polygon[i];
		const ClipVertex& next = polygon[(i + 1) % count];
		const float current_value = (axis == 0 ? current.position.x : current.position.y);
		const float next_value = (axis == 0 ? next.position.x : next.position.y);
		const bool current_inside = (keep_greater ? current_value >= bound : current_value <= bound);
		const bool next_inside = (keep_greater ? next_value >= bound : next_value <= bound);

		if (current_inside)
			scratch.push_back(current);
		if (current_inside != next_inside)
		{
			const float t = (bound - current_value) / (next_value - current_value);
			scratch.push_back(ClipVertex{current.position + (next.position - current.position) * t, Math::RoundedLerp(t, current.colour, next.colour)});
		}
	}
	polygon.swap(scratch);
}

// Adds the part of the source mesh inside the given tile, with texture coordinates relative to the tile origin.
static void AppendMeshClippedToTile(Mesh& out_mesh, const Mesh& source, Vector2f tile_min, Vector2f tile_max)
{
	Vector<ClipVertex> polygon;
	Vector<ClipVertex> scratch;
	for (size_t i = 0; i + 2 < source.indices.size(); i += 3)
	{
		polygon.clear();
		for (size_t j = 0; j < 3; j++)
		{
			const Vertex& vertex = source.vertices[source.indices[i + j]];
			polygon.push_back(ClipVertex{vertex.position, vertex.colour});
		}

		ClipPolygonToEdge(polygon, scratch, 0, tile_min.x, true);
		ClipPolygonToEdge(polygon, scratch, 0, tile_max.x, false);
		ClipPolygonToEdge(polygon, scratch, 1, tile_min.y, true);
		ClipPolygonToEdge(polygon, scratch, 1, tile_max.y, false);
		if (polygon.size() < 3)
			continue;

		const int first_vertex = (int)out_mesh.vertices.size();
		for (const ClipVertex& clip_vertex : polygon)
			out_mesh.vertices.push_back(Vertex{clip_vertex.position, clip_vertex.colour, clip_vertex.position - tile_min});
		for (int j = 1; j + 1 < (int)polygon.size(); j++)
			out_mesh.indices.insert(out_mesh.indices.end(), {first_vertex, first_vertex + j, first_vertex + j + 1});
	}
}

// Generates the mesh of a gradient layer: the paint area, clipped to each tile of the layer, with texture coordinates relative to the tile
// containing them. Returns the tile size, which the gradient shape is computed for.
static Vector2f GenerateGradientLayerMesh(Mesh& mesh, Element* element, BoxArea paint_area, const DecoratorLayer& layer, ColourbPremultiplied colour)
{
	const RenderBox render_box = element->GetRenderBox(paint_area);

	if (layer == DecoratorLayer())
	{
		MeshUtilities::GenerateBackground(mesh, render_box, colour);
		const Vector2f render_offset = render_box.GetFillOffset();
		for (Vertex& vertex : mesh.vertices)
			vertex.tex_coord = vertex.position - render_offset;
		return render_box.GetFillSize();
	}

	const RenderBox origin_box = element->GetRenderBox(layer.origin == BoxArea::Auto ? paint_area : layer.origin);
	const Vector2f area_offset = origin_box.GetFillOffset();
	const Vector2f area_size = origin_box.GetFillSize();

	Vector2f tile_size = area_size;
	if (layer.size_type == DecoratorLayer::SizeType::Explicit)
	{
		if (layer.size_x.unit != Unit::KEYWORD)
			tile_size.x = element->ResolveNumericValue(layer.size_x, area_size.x);
		if (layer.size_y.unit != Unit::KEYWORD)
			tile_size.y = element->ResolveNumericValue(layer.size_y, area_size.y);
	}

	if (tile_size.x <= 0.f || tile_size.y <= 0.f)
		return tile_size;

	const Vector2f tile_position = area_offset +
		Vector2f(element->ResolveNumericValue(layer.position_x, area_size.x - tile_size.x),
			element->ResolveNumericValue(layer.position_y, area_size.y - tile_size.y));

	Mesh paint_mesh;
	MeshUtilities::GenerateBackground(paint_mesh, render_box, colour);
	if (paint_mesh.vertices.empty())
		return tile_size;

	Vector2f paint_min(FLT_MAX, FLT_MAX);
	Vector2f paint_max(-FLT_MAX, -FLT_MAX);
	for (const Vertex& vertex : paint_mesh.vertices)
	{
		paint_min = Math::Min(paint_min, vertex.position);
		paint_max = Math::Max(paint_max, vertex.position);
	}

	constexpr int max_tiles_per_axis = 256;
	int num_tiles[2] = {1, 1};
	Vector2f first_tile = tile_position;
	const bool repeat[2] = {layer.repeat_x, layer.repeat_y};
	for (int axis = 0; axis < 2; axis++)
	{
		if (!repeat[axis])
			continue;
		const float size = tile_size[axis];
		first_tile[axis] = tile_position[axis] - Math::RoundUp((tile_position[axis] - paint_min[axis]) / size) * size;
		num_tiles[axis] = Math::Clamp((int)Math::RoundUp((paint_max[axis] - first_tile[axis]) / size), 1, max_tiles_per_axis);
	}

	for (int y = 0; y < num_tiles[1]; y++)
	{
		for (int x = 0; x < num_tiles[0]; x++)
		{
			const Vector2f tile_min = first_tile + Vector2f(float(x) * tile_size.x, float(y) * tile_size.y);
			AppendMeshClippedToTile(mesh, paint_mesh, tile_min, tile_min + tile_size);
		}
	}

	return tile_size;
}

DecoratorStraightGradient::DecoratorStraightGradient() {}

DecoratorStraightGradient::~DecoratorStraightGradient() {}

bool DecoratorStraightGradient::Initialise(const Direction in_direction, const Colourb in_start, const Colourb in_stop)
{
	direction = in_direction;
	start = in_start;
	stop = in_stop;
	return true;
}

DecoratorDataHandle DecoratorStraightGradient::GenerateElementData(Element* element, BoxArea paint_area) const
{
	const RenderBox render_box = element->GetRenderBox(paint_area);
	const ComputedValues& computed = element->GetComputedValues();
	const float opacity = computed.opacity();

	Mesh mesh;
	MeshUtilities::GenerateBackground(mesh, render_box, ColourbPremultiplied());

	ColourbPremultiplied colour_start = start.ToPremultiplied(opacity);
	ColourbPremultiplied colour_stop = stop.ToPremultiplied(opacity);

	const Vector2f offset = render_box.GetFillOffset();
	const Vector2f size = render_box.GetFillSize();

	Vector<Vertex>& vertices = mesh.vertices;

	if (direction == Direction::Horizontal)
	{
		for (int i = 0; i < (int)vertices.size(); i++)
		{
			const float t = Math::Clamp((vertices[i].position.x - offset.x) / size.x, 0.0f, 1.0f);
			vertices[i].colour = Math::RoundedLerp(t, colour_start, colour_stop);
		}
	}
	else if (direction == Direction::Vertical)
	{
		for (int i = 0; i < (int)vertices.size(); i++)
		{
			const float t = Math::Clamp((vertices[i].position.y - offset.y) / size.y, 0.0f, 1.0f);
			vertices[i].colour = Math::RoundedLerp(t, colour_start, colour_stop);
		}
	}

	Geometry* geometry = new Geometry(element->GetRenderManager()->MakeGeometry(std::move(mesh)));

	return reinterpret_cast<DecoratorDataHandle>(geometry);
}

void DecoratorStraightGradient::ReleaseElementData(DecoratorDataHandle element_data) const
{
	delete reinterpret_cast<Geometry*>(element_data);
}

void DecoratorStraightGradient::RenderElement(Element* element, DecoratorDataHandle element_data) const
{
	auto* data = reinterpret_cast<Geometry*>(element_data);
	data->Render(element->GetAbsoluteOffset(BoxArea::Border));
}

DecoratorStraightGradientInstancer::DecoratorStraightGradientInstancer()
{
	ids.direction = RegisterProperty("direction", "horizontal").AddParser("keyword", "horizontal, vertical").GetId();
	ids.start = RegisterProperty("start-color", "#ffffff").AddParser("color").GetId();
	ids.stop = RegisterProperty("stop-color", "#ffffff").AddParser("color").GetId();
	RegisterShorthand("decorator", "direction, start-color, stop-color", ShorthandType::FallThrough);
}

DecoratorStraightGradientInstancer::~DecoratorStraightGradientInstancer() {}

SharedPtr<Decorator> DecoratorStraightGradientInstancer::InstanceDecorator(const String& name, const PropertyDictionary& properties_,
	const DecoratorInstancerInterface& /*interface_*/)
{
	using Direction = DecoratorStraightGradient::Direction;
	Direction direction;
	if (name == "horizontal-gradient")
		direction = Direction::Horizontal;
	else if (name == "vertical-gradient")
		direction = Direction::Vertical;
	else
	{
		direction = (Direction)properties_.GetProperty(ids.direction)->Get<int>();
		Log::Message(Log::LT_WARNING,
			"Decorator syntax 'gradient(horizontal|vertical ...)' is deprecated, please replace with 'horizontal-gradient(...)' or "
			"'vertical-gradient(...)'");
	}

	Colourb start = properties_.GetProperty(ids.start)->Get<Colourb>();
	Colourb stop = properties_.GetProperty(ids.stop)->Get<Colourb>();

	auto decorator = MakeShared<DecoratorStraightGradient>();
	if (decorator->Initialise(direction, start, stop))
		return decorator;

	return nullptr;
}

DecoratorLinearGradient::DecoratorLinearGradient() {}

DecoratorLinearGradient::~DecoratorLinearGradient() {}

bool DecoratorLinearGradient::Initialise(bool in_repeating, Corner in_corner, float in_angle, const ColorStopList& in_color_stops)
{
	repeating = in_repeating;
	corner = in_corner;
	angle = in_angle;
	color_stops = in_color_stops;
	return !color_stops.empty();
}

DecoratorDataHandle DecoratorLinearGradient::GenerateElementData(Element* element, BoxArea paint_area) const
{
	return GenerateLayerElementData(element, paint_area, DecoratorLayer());
}

DecoratorDataHandle DecoratorLinearGradient::GenerateLayerElementData(Element* element, BoxArea paint_area, const DecoratorLayer& layer) const
{
	RenderManager* render_manager = element->GetRenderManager();
	if (!render_manager)
		return INVALID_DECORATORDATAHANDLE;

	RMLUI_ASSERT(!color_stops.empty());

	Mesh mesh;
	const ComputedValues& computed = element->GetComputedValues();
	const byte alpha = byte(computed.opacity() * 255.f);
	const Vector2f tile_size = GenerateGradientLayerMesh(mesh, element, paint_area, layer, ColourbPremultiplied(alpha, alpha));

	LinearGradientShape gradient_shape = CalculateShape(tile_size);

	// One-pixel minimum color stop spacing to avoid aliasing.
	const float soft_spacing = 1.f / gradient_shape.length;

	ColorStopList resolved_stops = ResolveColorStops(element, gradient_shape.length, soft_spacing, color_stops);

	CompiledShader shader = render_manager->CompileShader("linear-gradient",
		Dictionary{
			{"p0", Variant(gradient_shape.p0)},
			{"p1", Variant(gradient_shape.p1)},
			{"length", Variant(gradient_shape.length)},
			{"repeating", Variant(repeating)},
			{"color_stop_list", Variant(std::move(resolved_stops))},
		});
	if (!shader)
		return INVALID_DECORATORDATAHANDLE;

	ShaderElementData* element_data =
		GetShaderElementDataPool().AllocateAndConstruct(render_manager->MakeGeometry(std::move(mesh)), std::move(shader));

	return reinterpret_cast<DecoratorDataHandle>(element_data);
}

void DecoratorLinearGradient::ReleaseElementData(DecoratorDataHandle handle) const
{
	ShaderElementData* element_data = reinterpret_cast<ShaderElementData*>(handle);
	GetShaderElementDataPool().DestroyAndDeallocate(element_data);
}

void DecoratorLinearGradient::RenderElement(Element* element, DecoratorDataHandle handle) const
{
	ShaderElementData* element_data = reinterpret_cast<ShaderElementData*>(handle);
	element_data->geometry.Render(element->GetAbsoluteOffset(BoxArea::Border), {}, element_data->shader);
}

DecoratorLinearGradient::LinearGradientShape DecoratorLinearGradient::CalculateShape(Vector2f dim) const
{
	using uint = unsigned int;
	const Vector2f corners[(int)Corner::Count] = {Vector2f(dim.x, 0), dim, Vector2f(0, dim.y), Vector2f(0, 0)};
	const Vector2f center = 0.5f * dim;

	uint quadrant = 0;
	Vector2f line_vector;

	if (corner == Corner::None)
	{
		// Find the target quadrant and unit vector for the given angle.
		quadrant = uint(Math::NormaliseAngle(angle) * (4.f / (2.f * Math::RMLUI_PI))) % 4u;
		line_vector = Vector2f(Math::Sin(angle), -Math::Cos(angle));
	}
	else
	{
		// Quadrant given by the corner, need to find the vector perpendicular to the line connecting the neighboring corners.
		quadrant = uint(corner);
		const Vector2f v_neighbors = (corners[(quadrant + 1u) % 4u] - corners[(quadrant + 3u) % 4u]).Normalise();
		line_vector = {v_neighbors.y, -v_neighbors.x};
	}

	const uint quadrant_opposite = (quadrant + 2u) % 4u;

	const Vector2f starting_point = IntersectionPointToLineNormal(corners[quadrant_opposite], center, line_vector);
	const Vector2f ending_point = IntersectionPointToLineNormal(corners[quadrant], center, line_vector);

	const float length = Math::Absolute(dim.x * line_vector.x) + Math::Absolute(-dim.y * line_vector.y);

	return LinearGradientShape{starting_point, ending_point, length};
}

DecoratorLinearGradientInstancer::DecoratorLinearGradientInstancer()
{
	ids.angle = RegisterProperty("angle", "180deg").AddParser("angle").GetId();
	ids.direction_to = RegisterProperty("to", "unspecified").AddParser("keyword", "unspecified, to").GetId();
	// See Direction enum for keyword values.
	ids.direction_x = RegisterProperty("direction-x", "unspecified").AddParser("keyword", "unspecified=0, left=8, right=2").GetId();
	ids.direction_y = RegisterProperty("direction-y", "unspecified").AddParser("keyword", "unspecified=0, top=1, bottom=4").GetId();
	ids.color_stop_list = RegisterProperty("color-stops", "").AddParser("color_stop_list").GetId();

	RegisterShorthand("direction", "angle, to, direction-x, direction-y, direction-x", ShorthandType::FallThrough);
	RegisterShorthand("decorator", "direction?, color-stops#", ShorthandType::RecursiveCommaSeparated);
}

DecoratorLinearGradientInstancer::~DecoratorLinearGradientInstancer() {}

SharedPtr<Decorator> DecoratorLinearGradientInstancer::InstanceDecorator(const String& name, const PropertyDictionary& properties_,
	const DecoratorInstancerInterface& /*interface_*/)
{
	const Property* p_angle = properties_.GetProperty(ids.angle);
	const Property* p_direction_to = properties_.GetProperty(ids.direction_to);
	const Property* p_direction_x = properties_.GetProperty(ids.direction_x);
	const Property* p_direction_y = properties_.GetProperty(ids.direction_y);
	const Property* p_color_stop_list = properties_.GetProperty(ids.color_stop_list);

	if (!p_angle || !p_direction_to || !p_direction_x || !p_direction_y || !p_color_stop_list)
		return nullptr;

	using Corner = DecoratorLinearGradient::Corner;
	Corner corner = Corner::None;
	float angle = 0.f;

	if (p_direction_to->Get<bool>())
	{
		const Direction direction = (Direction)(p_direction_x->Get<int>() | p_direction_y->Get<int>());
		switch (direction)
		{
		case Direction::Top: angle = 0.f; break;
		case Direction::Right: angle = 0.5f * Math::RMLUI_PI; break;
		case Direction::Bottom: angle = Math::RMLUI_PI; break;
		case Direction::Left: angle = 1.5f * Math::RMLUI_PI; break;
		case Direction::TopLeft: corner = Corner::TopLeft; break;
		case Direction::TopRight: corner = Corner::TopRight; break;
		case Direction::BottomRight: corner = Corner::BottomRight; break;
		case Direction::BottomLeft: corner = Corner::BottomLeft; break;
		case Direction::None:
		default: return nullptr; break;
		}
	}
	else
	{
		angle = ComputeAngle(p_angle->GetNumericValue());
	}

	if (p_color_stop_list->unit != Unit::COLORSTOPLIST)
		return nullptr;
	const ColorStopList& color_stop_list = p_color_stop_list->value.GetReference<ColorStopList>();
	const bool repeating = (name == "repeating-linear-gradient");

	auto decorator = MakeShared<DecoratorLinearGradient>();
	if (decorator->Initialise(repeating, corner, angle, color_stop_list))
		return decorator;

	return nullptr;
}

DecoratorRadialGradient::DecoratorRadialGradient() {}

DecoratorRadialGradient::~DecoratorRadialGradient() {}

bool DecoratorRadialGradient::Initialise(bool in_repeating, Shape in_shape, SizeType in_size_type, Vector2Numeric in_size, Vector2Numeric in_position,
	const ColorStopList& in_color_stops)
{
	repeating = in_repeating;
	shape = in_shape;
	size_type = in_size_type;
	size = in_size;
	position = in_position;
	color_stops = in_color_stops;
	return !color_stops.empty();
}

DecoratorDataHandle DecoratorRadialGradient::GenerateElementData(Element* element, BoxArea paint_area) const
{
	return GenerateLayerElementData(element, paint_area, DecoratorLayer());
}

DecoratorDataHandle DecoratorRadialGradient::GenerateLayerElementData(Element* element, BoxArea paint_area, const DecoratorLayer& layer) const
{
	RenderManager* render_manager = element->GetRenderManager();
	if (!render_manager)
		return INVALID_DECORATORDATAHANDLE;

	RMLUI_ASSERT(!color_stops.empty() && (shape == Shape::Circle || shape == Shape::Ellipse));

	Mesh mesh;
	const ComputedValues& computed = element->GetComputedValues();
	const byte alpha = byte(computed.opacity() * 255.f);
	const Vector2f dimensions = GenerateGradientLayerMesh(mesh, element, paint_area, layer, ColourbPremultiplied(alpha, alpha));

	RadialGradientShape gradient_shape = CalculateRadialGradientShape(element, dimensions);

	// One-pixel minimum color stop spacing to avoid aliasing.
	const float soft_spacing = 1.f / Math::Min(gradient_shape.radius.x, gradient_shape.radius.y);

	ColorStopList resolved_stops = ResolveColorStops(element, gradient_shape.radius.x, soft_spacing, color_stops);

	CompiledShader shader = render_manager->CompileShader("radial-gradient",
		Dictionary{
			{"center", Variant(gradient_shape.center)},
			{"radius", Variant(gradient_shape.radius)},
			{"repeating", Variant(repeating)},
			{"color_stop_list", Variant(std::move(resolved_stops))},
		});
	if (!shader)
		return INVALID_DECORATORDATAHANDLE;

	ShaderElementData* element_data =
		GetShaderElementDataPool().AllocateAndConstruct(render_manager->MakeGeometry(std::move(mesh)), std::move(shader));
	return reinterpret_cast<DecoratorDataHandle>(element_data);
}

void DecoratorRadialGradient::ReleaseElementData(DecoratorDataHandle handle) const
{
	ShaderElementData* element_data = reinterpret_cast<ShaderElementData*>(handle);
	GetShaderElementDataPool().DestroyAndDeallocate(element_data);
}

void DecoratorRadialGradient::RenderElement(Element* element, DecoratorDataHandle handle) const
{
	ShaderElementData* element_data = reinterpret_cast<ShaderElementData*>(handle);
	element_data->geometry.Render(element->GetAbsoluteOffset(BoxArea::Border), {}, element_data->shader);
}

DecoratorRadialGradient::RadialGradientShape DecoratorRadialGradient::CalculateRadialGradientShape(Element* element, Vector2f dimensions) const
{
	RadialGradientShape result;
	result.center.x = element->ResolveNumericValue(position.x, dimensions.x);
	result.center.y = element->ResolveNumericValue(position.y, dimensions.y);
	const bool is_circle = (shape == Shape::Circle);

	auto Abs = [](Vector2f v) { return Vector2f{Math::Absolute(v.x), Math::Absolute(v.y)}; };
	auto d = dimensions;
	auto c = result.center;
	Vector2f r;

	switch (size_type)
	{
	case SizeType::ClosestSide:
	{
		r = Abs(Math::Min(c, d - c));
		result.radius = (is_circle ? Vector2f(Math::Min(r.x, r.y)) : r);
	}
	break;
	case SizeType::FarthestSide:
	{
		r = Abs(Math::Max(c, d - c));
		result.radius = (is_circle ? Vector2f(Math::Max(r.x, r.y)) : r);
	}
	break;
	case SizeType::ClosestCorner:
	case SizeType::FarthestCorner:
	{
		if (size_type == SizeType::ClosestCorner)
			r = Abs(Math::Min(c, d - c)); // Same as closest-side.
		else
			r = Abs(Math::Max(c, d - c)); // Same as farthest-side.

		if (is_circle)
		{
			result.radius = Vector2f(r.Magnitude());
		}
		else
		{
			r = Math::Max(r, Vector2f(1)); // In case r.x ~= 0
			result.radius.x = Math::SquareRoot(2.f * r.x * r.x);
			result.radius.y = result.radius.x * (r.y / r.x);
		}
	}
	break;
	case SizeType::LengthPercentage:
	{
		result.radius.x = element->ResolveNumericValue(size.x, d.x);
		result.radius.y = (is_circle ? result.radius.x : element->ResolveNumericValue(size.y, d.y));
		result.radius = Abs(result.radius);
	}
	break;
	}

	result.radius = Math::Max(result.radius, Vector2f(1.f));
	return result;
}

DecoratorRadialGradientInstancer::DecoratorRadialGradientInstancer()
{
	ids.ending_shape = RegisterProperty("ending-shape", "unspecified").AddParser("keyword", "circle, ellipse, unspecified").GetId();
	ids.size_x = RegisterProperty("size-x", "farthest-corner")
					 .AddParser("keyword", "closest-side, farthest-side, closest-corner, farthest-corner")
					 .AddParser("length_percent")
					 .GetId();
	ids.size_y = RegisterProperty("size-y", "unspecified").AddParser("keyword", "unspecified").AddParser("length_percent").GetId();

	RegisterProperty("at", "unspecified").AddParser("keyword", "at, unspecified");
	ids.position_x = RegisterProperty("position-x", "center").AddParser("keyword", "left, center, right").AddParser("length_percent").GetId();
	ids.position_y = RegisterProperty("position-y", "center").AddParser("keyword", "top, center, bottom").AddParser("length_percent").GetId();

	ids.color_stop_list = RegisterProperty("color-stops", "").AddParser("color_stop_list").GetId();

	RegisterShorthand("shape", "ending-shape, size-x, size-y, at, position-x, position-y, position-x", ShorthandType::FallThrough);

	RegisterShorthand("decorator", "shape?, color-stops#", ShorthandType::RecursiveCommaSeparated);
}

DecoratorRadialGradientInstancer::~DecoratorRadialGradientInstancer() {}

SharedPtr<Decorator> DecoratorRadialGradientInstancer::InstanceDecorator(const String& name, const PropertyDictionary& properties_,
	const DecoratorInstancerInterface& /*interface_*/)
{
	const Property* p_ending_shape = properties_.GetProperty(ids.ending_shape);
	const Property* p_size_x = properties_.GetProperty(ids.size_x);
	const Property* p_size_y = properties_.GetProperty(ids.size_y);
	Array<const Property*, 2> p_position = {properties_.GetProperty(ids.position_x), properties_.GetProperty(ids.position_y)};
	const Property* p_color_stop_list = properties_.GetProperty(ids.color_stop_list);

	if (!p_ending_shape || !p_size_x || !p_size_y || !p_position[0] || !p_position[1] || !p_color_stop_list)
		return nullptr;

	using SizeType = DecoratorRadialGradient::SizeType;
	using Shape = DecoratorRadialGradient::Shape;

	Shape shape = (Shape)p_ending_shape->Get<int>();
	if (shape == Shape::Unspecified)
	{
		const bool circle_sized = (Any(p_size_x->unit & Unit::LENGTH_PERCENT) && p_size_y->unit == Unit::KEYWORD);
		shape = (circle_sized ? Shape::Circle : Shape::Ellipse);
	}
	if (shape == Shape::Circle && (p_size_x->unit == Unit::PERCENT || p_size_y->unit != Unit::KEYWORD))
		return nullptr;

	SizeType size_type = {};
	Vector2Numeric size;
	if (p_size_x->unit == Unit::KEYWORD)
	{
		size_type = (SizeType)p_size_x->Get<int>();
	}
	else
	{
		size_type = SizeType::LengthPercentage;
		size.x = p_size_x->GetNumericValue();
		size.y = (p_size_y->unit == Unit::KEYWORD ? size.x : p_size_y->GetNumericValue());
	}

	const Vector2Numeric position = ComputePosition(p_position);
	const bool repeating = (name == "repeating-radial-gradient");

	if (p_color_stop_list->unit != Unit::COLORSTOPLIST)
		return nullptr;
	const ColorStopList& color_stop_list = p_color_stop_list->value.GetReference<ColorStopList>();

	auto decorator = MakeShared<DecoratorRadialGradient>();
	if (decorator->Initialise(repeating, shape, size_type, size, position, color_stop_list))
		return decorator;

	return nullptr;
}

DecoratorConicGradient::DecoratorConicGradient() {}

DecoratorConicGradient::~DecoratorConicGradient() {}

bool DecoratorConicGradient::Initialise(bool in_repeating, float in_angle, Vector2Numeric in_position, const ColorStopList& in_color_stops)
{
	repeating = in_repeating;
	angle = in_angle;
	position = in_position;
	color_stops = in_color_stops;
	return !color_stops.empty();
}

DecoratorDataHandle DecoratorConicGradient::GenerateElementData(Element* element, BoxArea paint_area) const
{
	return GenerateLayerElementData(element, paint_area, DecoratorLayer());
}

DecoratorDataHandle DecoratorConicGradient::GenerateLayerElementData(Element* element, BoxArea paint_area, const DecoratorLayer& layer) const
{
	RenderManager* render_manager = element->GetRenderManager();
	if (!render_manager)
		return INVALID_DECORATORDATAHANDLE;

	RMLUI_ASSERT(!color_stops.empty());

	Mesh mesh;
	const ComputedValues& computed = element->GetComputedValues();
	const byte alpha = byte(computed.opacity() * 255.f);
	const Vector2f dimensions = GenerateGradientLayerMesh(mesh, element, paint_area, layer, ColourbPremultiplied(alpha, alpha));

	const Vector2f center =
		Vector2f{element->ResolveNumericValue(position.x, dimensions.x), element->ResolveNumericValue(position.y, dimensions.y)}.Round();

	ColorStopList resolved_stops = ResolveColorStops(element, 1.f, 0.f, color_stops);

	CompiledShader shader = render_manager->CompileShader("conic-gradient",
		Dictionary{
			{"angle", Variant(angle)},
			{"center", Variant(center)},
			{"repeating", Variant(repeating)},
			{"color_stop_list", Variant(std::move(resolved_stops))},
		});
	if (!shader)
		return INVALID_DECORATORDATAHANDLE;

	ShaderElementData* element_data =
		GetShaderElementDataPool().AllocateAndConstruct(render_manager->MakeGeometry(std::move(mesh)), std::move(shader));
	return reinterpret_cast<DecoratorDataHandle>(element_data);
}

void DecoratorConicGradient::ReleaseElementData(DecoratorDataHandle handle) const
{
	ShaderElementData* element_data = reinterpret_cast<ShaderElementData*>(handle);
	GetShaderElementDataPool().DestroyAndDeallocate(element_data);
}

void DecoratorConicGradient::RenderElement(Element* element, DecoratorDataHandle handle) const
{
	ShaderElementData* element_data = reinterpret_cast<ShaderElementData*>(handle);
	element_data->geometry.Render(element->GetAbsoluteOffset(BoxArea::Border), {}, element_data->shader);
}

DecoratorConicGradientInstancer::DecoratorConicGradientInstancer()
{
	RegisterProperty("from", "from").AddParser("keyword", "from");
	ids.angle = RegisterProperty("angle", "0deg").AddParser("angle").GetId();

	RegisterProperty("at", "unspecified").AddParser("keyword", "at, unspecified");
	ids.position_x = RegisterProperty("position-x", "center").AddParser("keyword", "left, center, right").AddParser("length_percent").GetId();
	ids.position_y = RegisterProperty("position-y", "center").AddParser("keyword", "top, center, bottom").AddParser("length_percent").GetId();

	ids.color_stop_list = RegisterProperty("color-stops", "").AddParser("color_stop_list", "angle").GetId();

	RegisterShorthand("shape", "from, angle, at, position-x, position-y, position-x", ShorthandType::FallThrough);
	RegisterShorthand("decorator", "shape?, color-stops#", ShorthandType::RecursiveCommaSeparated);
}

DecoratorConicGradientInstancer::~DecoratorConicGradientInstancer() {}

SharedPtr<Decorator> DecoratorConicGradientInstancer::InstanceDecorator(const String& name, const PropertyDictionary& properties_,
	const DecoratorInstancerInterface& /*interface_*/)
{
	const Property* p_angle = properties_.GetProperty(ids.angle);
	Array<const Property*, 2> p_position = {properties_.GetProperty(ids.position_x), properties_.GetProperty(ids.position_y)};
	const Property* p_color_stop_list = properties_.GetProperty(ids.color_stop_list);

	if (!p_angle || !p_position[0] || !p_position[1] || !p_color_stop_list)
		return nullptr;

	const float angle = ComputeAngle(p_angle->GetNumericValue());
	const Vector2Numeric position = ComputePosition(p_position);
	const bool repeating = (name == "repeating-conic-gradient");

	if (p_color_stop_list->unit != Unit::COLORSTOPLIST)
		return nullptr;
	const ColorStopList& color_stop_list = p_color_stop_list->value.GetReference<ColorStopList>();

	auto decorator = MakeShared<DecoratorConicGradient>();
	if (decorator->Initialise(repeating, angle, position, color_stop_list))
		return decorator;

	return nullptr;
}

} // namespace Rml
