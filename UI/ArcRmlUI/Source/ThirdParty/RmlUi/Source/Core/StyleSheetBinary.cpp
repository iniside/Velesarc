#include "StyleSheetBinary.h"
#include "../../Include/RmlUi/Core/Factory.h"
#include "../../Include/RmlUi/Core/Property.h"
#include "../../Include/RmlUi/Core/PropertyDefinition.h"
#include "../../Include/RmlUi/Core/PropertyDictionary.h"
#include "../../Include/RmlUi/Core/PropertySpecification.h"
#include "../../Include/RmlUi/Core/StringUtilities.h"
#include "../../Include/RmlUi/Core/StyleSheet.h"
#include "../../Include/RmlUi/Core/StyleSheetContainer.h"
#include "../../Include/RmlUi/Core/StyleSheetSpecification.h"
#include "../../Include/RmlUi/Core/XMLParser.h"
#include "PropertyShorthandDefinition.h"
#include "StyleSheetNode.h"
#include "StyleSheetParser.h"
#include "StyleSheetSelector.h"
#include <algorithm>
#include <string.h>

namespace Rml {

namespace StyleSheetBinaryDetail {

	static bool registry_hash_computed = false;
	static uint64_t registry_hash = 0;

	static uint64_t HashFnv1a(uint64_t hash, const String& text)
	{
		for (const char c : text)
		{
			hash ^= static_cast<uint8_t>(c);
			hash *= 1099511628211ull;
		}
		return hash;
	}

	static String SortedSetSignature(const SmallUnorderedSet<String>& set)
	{
		StringList items;
		for (const String& item : set)
			items.push_back(item);
		std::sort(items.begin(), items.end());

		String result;
		StringUtilities::JoinString(result, items, '\n');
		return result;
	}

	static bool IsDeclarationType(Variant::Type type)
	{
		switch (type)
		{
		case Variant::TRANSFORMPTR:
		case Variant::TRANSITIONLIST:
		case Variant::ANIMATIONLIST:
		case Variant::DECORATORSPTR:
		case Variant::FILTERSPTR:
		case Variant::FONTEFFECTSPTR:
		case Variant::COLORSTOPLIST:
		case Variant::BOXSHADOWLIST:
		case Variant::GRIDDEFINITIONPTR:
		case Variant::CALCEXPRESSIONPTR: return true;
		default: return false;
		}
	}

	class BinaryWriter {
	public:
		explicit BinaryWriter(Vector<byte>& out) : out(out) {}

		void Raw(const void* data, size_t size)
		{
			const byte* bytes = static_cast<const byte*>(data);
			out.insert(out.end(), bytes, bytes + size);
		}
		void U8(uint8_t value) { out.push_back(value); }
		void U32(uint32_t value) { Raw(&value, sizeof(value)); }
		void I32(int32_t value) { Raw(&value, sizeof(value)); }
		void U64(uint64_t value) { Raw(&value, sizeof(value)); }
		void I64(int64_t value) { Raw(&value, sizeof(value)); }
		void F32(float value) { Raw(&value, sizeof(value)); }
		void F64(double value) { Raw(&value, sizeof(value)); }
		void Str(const String& value)
		{
			U32(static_cast<uint32_t>(value.size()));
			Raw(value.data(), value.size());
		}
		void StrList(const StringList& values)
		{
			U32(static_cast<uint32_t>(values.size()));
			for (const String& value : values)
				Str(value);
		}

	private:
		Vector<byte>& out;
	};

	class BinaryReader {
	public:
		explicit BinaryReader(Span<const byte> data) : data(data) {}

		bool Raw(void* destination, size_t size)
		{
			if (failed || size > data.size() - position)
			{
				Fail("unexpected end of data");
				return false;
			}
			if (size > 0)
				memcpy(destination, data.data() + position, size);
			position += size;
			return true;
		}
		uint8_t U8()
		{
			uint8_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		uint32_t U32()
		{
			uint32_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		int32_t I32()
		{
			int32_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		uint64_t U64()
		{
			uint64_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		int64_t I64()
		{
			int64_t value = 0;
			Raw(&value, sizeof(value));
			return value;
		}
		float F32()
		{
			float value = 0.f;
			Raw(&value, sizeof(value));
			return value;
		}
		double F64()
		{
			double value = 0.0;
			Raw(&value, sizeof(value));
			return value;
		}
		String Str()
		{
			const uint32_t size = U32();
			if (failed || size > data.size() - position)
			{
				Fail("unexpected end of data");
				return String();
			}
			String value(reinterpret_cast<const char*>(data.data() + position), size);
			position += size;
			return value;
		}
		bool StrList(StringList& values)
		{
			const uint32_t count = U32();
			for (uint32_t i = 0; i < count && !failed; i++)
				values.push_back(Str());
			return !failed;
		}

		void Fail(const String& message)
		{
			if (!failed)
				error = message;
			failed = true;
		}
		bool Failed() const { return failed; }
		bool AtEnd() const { return position == data.size(); }
		const String& GetError() const { return error; }

	private:
		Span<const byte> data;
		size_t position = 0;
		bool failed = false;
		String error;
	};

} // namespace StyleSheetBinaryDetail

struct StyleSheetBinaryAccess {
	using BinaryWriter = StyleSheetBinaryDetail::BinaryWriter;
	using BinaryReader = StyleSheetBinaryDetail::BinaryReader;

	struct WriteState {
		const StyleSheetCompileRecord* record = nullptr;
		UnorderedMap<const PropertySource*, uint32_t> source_indices;
		Vector<const PropertySource*> sources;
		String error;
	};

	struct ReadState {
		BinaryReader& in;
		Vector<SharedPtr<const PropertySource>> sources;
	};

	static const StyleSheetDeclaration* FindDeclaration(const WriteState& state, PropertyId id, const Property& property,
		const StyleSheetDeclarationMap* keyframe_declarations)
	{
		const StyleSheetDeclarationMap* declarations = keyframe_declarations;
		if (!declarations && property.source)
		{
			UnorderedMap<const PropertySource*, StyleSheetCompileRecord::RuleEntry>::const_iterator it_rule =
				state.record->rules.find(property.source.get());
			if (it_rule != state.record->rules.end())
				declarations = it_rule->second.declarations.get();
		}
		if (!declarations)
			return nullptr;

		StyleSheetDeclarationMap::const_iterator it_declaration = declarations->find(id);
		return (it_declaration != declarations->end() ? &it_declaration->second : nullptr);
	}

	static bool WriteProperty(WriteState& state, BinaryWriter& out, const Property& property, const StyleSheetDeclaration* declaration,
		const String& name)
	{
		const Variant::Type type = property.value.GetType();
		out.U8(static_cast<uint8_t>(type));

		switch (type)
		{
		case Variant::NONE: break;
		case Variant::BOOL: out.U8(property.value.GetReference<bool>() ? 1 : 0); break;
		case Variant::BYTE: out.U8(property.value.GetReference<byte>()); break;
		case Variant::CHAR: out.U8(static_cast<uint8_t>(property.value.GetReference<char>())); break;
		case Variant::FLOAT: out.F32(property.value.GetReference<float>()); break;
		case Variant::DOUBLE: out.F64(property.value.GetReference<double>()); break;
		case Variant::INT: out.I32(property.value.GetReference<int>()); break;
		case Variant::INT64: out.I64(property.value.GetReference<int64_t>()); break;
		case Variant::UINT: out.U32(property.value.GetReference<unsigned int>()); break;
		case Variant::UINT64: out.U64(property.value.GetReference<uint64_t>()); break;
		case Variant::STRING: out.Str(property.value.GetReference<String>()); break;
		case Variant::VECTOR2:
		{
			const Vector2f& value = property.value.GetReference<Vector2f>();
			out.F32(value.x);
			out.F32(value.y);
		}
		break;
		case Variant::VECTOR3:
		{
			const Vector3f& value = property.value.GetReference<Vector3f>();
			out.F32(value.x);
			out.F32(value.y);
			out.F32(value.z);
		}
		break;
		case Variant::VECTOR4:
		{
			const Vector4f& value = property.value.GetReference<Vector4f>();
			out.F32(value.x);
			out.F32(value.y);
			out.F32(value.z);
			out.F32(value.w);
		}
		break;
		case Variant::COLOURF:
		{
			const Colourf& value = property.value.GetReference<Colourf>();
			out.F32(value.red);
			out.F32(value.green);
			out.F32(value.blue);
			out.F32(value.alpha);
		}
		break;
		case Variant::COLOURB:
		{
			const Colourb& value = property.value.GetReference<Colourb>();
			out.U8(value.red);
			out.U8(value.green);
			out.U8(value.blue);
			out.U8(value.alpha);
		}
		break;
		default:
		{
			if (!StyleSheetBinaryDetail::IsDeclarationType(type))
			{
				state.error = "property '" + name + "' holds a value type that cannot be compiled";
				return false;
			}
			if (!declaration)
			{
				state.error = "no declaration was recorded for property '" + name + "'";
				return false;
			}
			out.Str(declaration->name);
			out.Str(declaration->value);
		}
		break;
		}

		out.U64(static_cast<uint64_t>(property.unit));
		out.I32(property.specificity);
		out.U8(property.important ? 1 : 0);
		out.I32(property.parser_index);
		out.U8(property.definition ? 1 : 0);

		if (!property.source)
		{
			out.I32(-1);
		}
		else
		{
			const PropertySource* source = property.source.get();
			UnorderedMap<const PropertySource*, uint32_t>::const_iterator it_source = state.source_indices.find(source);
			uint32_t source_index = 0;
			if (it_source != state.source_indices.end())
			{
				source_index = it_source->second;
			}
			else
			{
				source_index = static_cast<uint32_t>(state.sources.size());
				state.sources.push_back(source);
				state.source_indices.emplace(source, source_index);
			}
			out.I32(static_cast<int32_t>(source_index));
		}

		return true;
	}

	static bool WriteDictionary(WriteState& state, BinaryWriter& out, const PropertyDictionary& dictionary, const PropertySpecification& specification,
		bool allow_declarations, const StyleSheetDeclarationMap* keyframe_declarations)
	{
		out.U32(static_cast<uint32_t>(dictionary.GetProperties().size()));
		for (const PropertyMap::value_type& entry : dictionary.GetProperties())
		{
			const PropertyDefinition* definition = specification.GetProperty(entry.first);
			if (!definition)
			{
				state.error = "a property has no registered definition";
				return false;
			}
			const String& name = specification.GetPropertyName(entry.first);
			out.Str(name);

			const StyleSheetDeclaration* declaration = nullptr;
			if (allow_declarations && StyleSheetBinaryDetail::IsDeclarationType(entry.second.value.GetType()))
				declaration = FindDeclaration(state, entry.first, entry.second, keyframe_declarations);

			if (!WriteProperty(state, out, entry.second, declaration, name))
				return false;
		}

		out.U32(static_cast<uint32_t>(dictionary.GetCustomProperties().size()));
		for (const UnorderedMap<String, Property>::value_type& entry : dictionary.GetCustomProperties())
		{
			out.Str(entry.first);
			if (!WriteProperty(state, out, entry.second, nullptr, entry.first))
				return false;
		}

		out.U32(static_cast<uint32_t>(dictionary.GetVarShorthands().size()));
		for (const UnorderedMap<ShorthandId, Property>::value_type& entry : dictionary.GetVarShorthands())
		{
			const String& name = specification.GetShorthandName(entry.first);
			if (!specification.GetShorthand(entry.first))
			{
				state.error = "a var() shorthand has no registered definition";
				return false;
			}
			out.Str(name);
			if (!WriteProperty(state, out, entry.second, nullptr, name))
				return false;
		}

		return true;
	}

	static void CollectPreorder(const StyleSheetNode& node, Vector<const StyleSheetNode*>& order)
	{
		order.push_back(&node);
		for (const UniquePtr<StyleSheetNode>& child : node.children)
			CollectPreorder(*child, order);
	}

	static bool WriteSelectorTree(WriteState& state, BinaryWriter& out, const SelectorTree& tree)
	{
		Vector<const StyleSheetNode*> order;
		CollectPreorder(*tree.root, order);

		if (!WriteNodeContent(state, out, *tree.root))
			return false;

		out.U32(static_cast<uint32_t>(tree.leafs.size()));
		for (const StyleSheetNode* leaf : tree.leafs)
		{
			Vector<const StyleSheetNode*>::const_iterator it_leaf = std::find(order.begin(), order.end(), leaf);
			if (it_leaf == order.end())
			{
				state.error = "a selector tree leaf is not owned by its tree";
				return false;
			}
			out.U32(static_cast<uint32_t>(it_leaf - order.begin()));
		}
		return true;
	}

	static bool WriteSelector(WriteState& state, BinaryWriter& out, const CompoundSelector& selector)
	{
		out.Str(selector.tag);
		out.Str(selector.id);
		out.StrList(selector.class_names);
		out.StrList(selector.pseudo_class_names);

		out.U32(static_cast<uint32_t>(selector.attributes.size()));
		for (const AttributeSelector& attribute : selector.attributes)
		{
			out.U8(static_cast<uint8_t>(attribute.type));
			out.Str(attribute.name);
			out.Str(attribute.value);
		}

		out.U32(static_cast<uint32_t>(selector.structural_selectors.size()));
		for (const StructuralSelector& structural : selector.structural_selectors)
		{
			out.U8(static_cast<uint8_t>(structural.type));
			out.I32(structural.a);
			out.I32(structural.b);
			out.I32(structural.specificity);
			out.U8(structural.selector_tree ? 1 : 0);
			if (structural.selector_tree && !WriteSelectorTree(state, out, *structural.selector_tree))
				return false;
		}

		out.U8(static_cast<uint8_t>(selector.pseudo_element));
		out.U8(static_cast<uint8_t>(selector.scrollbar_part));
		out.StrList(selector.scrollbar_part_pseudo_class_names);
		out.U8(static_cast<uint8_t>(selector.combinator));
		return true;
	}

	static bool WriteNodeContent(WriteState& state, BinaryWriter& out, const StyleSheetNode& node)
	{
		if (!WriteDictionary(state, out, node.properties, StyleSheetSpecification::GetPropertySpecification(), true, nullptr))
			return false;

		out.U32(static_cast<uint32_t>(node.children.size()));
		for (const UniquePtr<StyleSheetNode>& child : node.children)
		{
			if (!WriteSelector(state, out, child->selector) || !WriteNodeContent(state, out, *child))
				return false;
		}
		return true;
	}

	static const StyleSheetDeclarationMap* FindKeyframeDeclarations(const WriteState& state, const KeyframesMap& keyframes_map, const String& name,
		float normalized_time)
	{
		UnorderedMap<const KeyframesMap*, UnorderedMap<String, Vector<StyleSheetCompileRecord::KeyframeEntry>>>::const_iterator it_sheet =
			state.record->keyframes.find(&keyframes_map);
		if (it_sheet == state.record->keyframes.end())
			return nullptr;

		UnorderedMap<String, Vector<StyleSheetCompileRecord::KeyframeEntry>>::const_iterator it_keyframes = it_sheet->second.find(name);
		if (it_keyframes == it_sheet->second.end())
			return nullptr;

		for (const StyleSheetCompileRecord::KeyframeEntry& entry : it_keyframes->second)
		{
			if (entry.normalized_time == normalized_time)
				return entry.declarations.get();
		}
		return nullptr;
	}

	static bool WriteStyleSheet(WriteState& state, BinaryWriter& out, const StyleSheet& sheet)
	{
		if (!sheet.named_decorator_map.empty() || sheet.spritesheet_list.NumSpriteSheets() > 0)
		{
			state.error = "@decorator and @spritesheet rules cannot be compiled";
			return false;
		}

		out.I32(sheet.specificity_offset);

		if (!WriteNodeContent(state, out, *sheet.root))
			return false;

		out.U32(static_cast<uint32_t>(sheet.keyframes.size()));
		for (const KeyframesMap::value_type& entry : sheet.keyframes)
		{
			out.Str(entry.first);
			out.U32(static_cast<uint32_t>(entry.second.blocks.size()));
			for (const KeyframeBlock& block : entry.second.blocks)
			{
				out.F32(block.normalized_time);
				const StyleSheetDeclarationMap* declarations = FindKeyframeDeclarations(state, sheet.keyframes, entry.first, block.normalized_time);
				if (!WriteDictionary(state, out, block.properties, StyleSheetSpecification::GetPropertySpecification(), true, declarations))
					return false;
			}
		}
		return true;
	}

	static bool Write(const StyleSheetContainer& container, Vector<byte>& out_body, String& out_error)
	{
		out_body.clear();

		if (!container.compile_record)
		{
			out_error = "the style sheet was not parsed in compile mode";
			return false;
		}

		WriteState state;
		state.record = container.compile_record.get();

		Vector<byte> content;
		BinaryWriter content_out(content);

		content_out.U32(static_cast<uint32_t>(container.media_blocks.size()));
		for (const MediaBlock& media_block : container.media_blocks)
		{
			content_out.U8(static_cast<uint8_t>(media_block.modifier));
			if (!WriteDictionary(state, content_out, media_block.properties, StyleSheetParser::GetMediaQuerySpecification(), false, nullptr) ||
				!WriteStyleSheet(state, content_out, *media_block.stylesheet))
			{
				out_error = state.error;
				return false;
			}
		}

		BinaryWriter out(out_body);
		out.U32(static_cast<uint32_t>(state.sources.size()));
		for (const PropertySource* source : state.sources)
		{
			out.I32(source->line_number);
			out.Str(source->rule_name);
		}
		out.Raw(content.data(), content.size());
		return true;
	}

	static bool ReadProperty(ReadState& state, Property& property, PropertyId id, const PropertySpecification* specification,
		const PropertySpecification* reparse_specification, const String& name)
	{
		BinaryReader& in = state.in;
		const Variant::Type type = static_cast<Variant::Type>(in.U8());

		switch (type)
		{
		case Variant::NONE: break;
		case Variant::BOOL: property.value = Variant(in.U8() != 0); break;
		case Variant::BYTE: property.value = Variant(static_cast<byte>(in.U8())); break;
		case Variant::CHAR: property.value = Variant(static_cast<char>(in.U8())); break;
		case Variant::FLOAT: property.value = Variant(in.F32()); break;
		case Variant::DOUBLE: property.value = Variant(in.F64()); break;
		case Variant::INT: property.value = Variant(static_cast<int>(in.I32())); break;
		case Variant::INT64: property.value = Variant(in.I64()); break;
		case Variant::UINT: property.value = Variant(static_cast<unsigned int>(in.U32())); break;
		case Variant::UINT64: property.value = Variant(in.U64()); break;
		case Variant::STRING: property.value = Variant(in.Str()); break;
		case Variant::VECTOR2:
		{
			Vector2f value;
			value.x = in.F32();
			value.y = in.F32();
			property.value = Variant(value);
		}
		break;
		case Variant::VECTOR3:
		{
			Vector3f value;
			value.x = in.F32();
			value.y = in.F32();
			value.z = in.F32();
			property.value = Variant(value);
		}
		break;
		case Variant::VECTOR4:
		{
			Vector4f value;
			value.x = in.F32();
			value.y = in.F32();
			value.z = in.F32();
			value.w = in.F32();
			property.value = Variant(value);
		}
		break;
		case Variant::COLOURF:
		{
			const float red = in.F32();
			const float green = in.F32();
			const float blue = in.F32();
			const float alpha = in.F32();
			property.value = Variant(Colourf(red, green, blue, alpha));
		}
		break;
		case Variant::COLOURB:
		{
			const byte red = in.U8();
			const byte green = in.U8();
			const byte blue = in.U8();
			const byte alpha = in.U8();
			property.value = Variant(Colourb(red, green, blue, alpha));
		}
		break;
		default:
		{
			if (!StyleSheetBinaryDetail::IsDeclarationType(type) || !reparse_specification || id == PropertyId::Invalid)
			{
				in.Fail("property '" + name + "' has an invalid value type");
				return false;
			}

			const String declaration_name = in.Str();
			const String declaration_value = in.Str();
			if (in.Failed())
				return false;

			PropertyDictionary parsed;
			const Property* parsed_property = nullptr;
			if (reparse_specification->ParsePropertyDeclaration(parsed, declaration_name, declaration_value))
				parsed_property = parsed.GetProperty(id);
			if (!parsed_property || parsed_property->value.GetType() != type)
			{
				in.Fail("declaration '" + declaration_name + ": " + declaration_value + "' no longer yields property '" + name + "'");
				return false;
			}
			property = *parsed_property;
		}
		break;
		}

		const Unit unit = static_cast<Unit>(in.U64());
		if (unit == Unit::DEVICE_PX)
		{
			in.Fail("property '" + name + "' has the internal unit devpx");
			return false;
		}
		if (StyleSheetBinaryDetail::IsDeclarationType(type) && property.unit != unit)
		{
			in.Fail("declaration of property '" + name + "' now parses to a different unit");
			return false;
		}
		property.unit = unit;
		property.specificity = in.I32();
		property.important = (in.U8() != 0);
		property.parser_index = in.I32();

		property.definition = nullptr;
		if (in.U8() != 0)
		{
			property.definition = (specification && id != PropertyId::Invalid ? specification->GetProperty(id) : nullptr);
			if (!property.definition)
			{
				in.Fail("property '" + name + "' has no definition");
				return false;
			}
		}

		const int32_t source_index = in.I32();
		property.source = nullptr;
		if (source_index >= 0)
		{
			if (static_cast<size_t>(source_index) >= state.sources.size())
			{
				in.Fail("property '" + name + "' refers to an invalid source");
				return false;
			}
			property.source = state.sources[source_index];
		}

		return !in.Failed();
	}

	static bool ReadDictionary(ReadState& state, PropertyDictionary& dictionary, const PropertySpecification& specification,
		const PropertySpecification* reparse_specification)
	{
		BinaryReader& in = state.in;

		const uint32_t num_properties = in.U32();
		for (uint32_t i = 0; i < num_properties && !in.Failed(); i++)
		{
			const String name = in.Str();
			const PropertyDefinition* definition = specification.GetProperty(name);
			if (!definition)
			{
				in.Fail("unknown property '" + name + "'");
				return false;
			}
			Property property;
			if (!ReadProperty(state, property, definition->GetId(), &specification, reparse_specification, name))
				return false;
			dictionary.SetProperty(definition->GetId(), property);
		}

		const uint32_t num_custom_properties = in.U32();
		for (uint32_t i = 0; i < num_custom_properties && !in.Failed(); i++)
		{
			String name = in.Str();
			Property property;
			if (!ReadProperty(state, property, PropertyId::Invalid, nullptr, nullptr, name))
				return false;
			dictionary.SetCustomProperty(std::move(name), std::move(property));
		}

		const uint32_t num_var_shorthands = in.U32();
		for (uint32_t i = 0; i < num_var_shorthands && !in.Failed(); i++)
		{
			const String name = in.Str();
			const ShorthandDefinition* shorthand = specification.GetShorthand(name);
			if (!shorthand)
			{
				in.Fail("unknown shorthand '" + name + "'");
				return false;
			}
			Property property;
			if (!ReadProperty(state, property, PropertyId::Invalid, nullptr, nullptr, name))
				return false;
			if (property.unit != Unit::VAR_EXPRESSION)
			{
				in.Fail("shorthand '" + name + "' is not a var() expression");
				return false;
			}
			dictionary.SetVarShorthand(shorthand->id, std::move(property));
		}

		return !in.Failed();
	}

	static bool ReadSelectorTree(ReadState& state, SelectorTree& tree)
	{
		tree.root = MakeUnique<StyleSheetNode>();
		Vector<StyleSheetNode*> order;
		order.push_back(tree.root.get());

		if (!ReadNodeContent(state, *tree.root, &order))
			return false;

		BinaryReader& in = state.in;
		const uint32_t num_leafs = in.U32();
		for (uint32_t i = 0; i < num_leafs && !in.Failed(); i++)
		{
			const uint32_t index = in.U32();
			if (index >= order.size())
			{
				in.Fail("a selector tree leaf index is out of range");
				return false;
			}
			tree.leafs.push_back(order[index]);
		}
		return !in.Failed();
	}

	static bool ReadSelector(ReadState& state, CompoundSelector& selector)
	{
		BinaryReader& in = state.in;

		selector.tag = in.Str();
		selector.id = in.Str();
		in.StrList(selector.class_names);
		in.StrList(selector.pseudo_class_names);

		const uint32_t num_attributes = in.U32();
		for (uint32_t i = 0; i < num_attributes && !in.Failed(); i++)
		{
			AttributeSelector attribute;
			attribute.type = static_cast<AttributeSelectorType>(in.U8());
			attribute.name = in.Str();
			attribute.value = in.Str();
			selector.attributes.push_back(std::move(attribute));
		}

		const uint32_t num_structural = in.U32();
		for (uint32_t i = 0; i < num_structural && !in.Failed(); i++)
		{
			const StructuralSelectorType type = static_cast<StructuralSelectorType>(in.U8());
			const int a = in.I32();
			const int b = in.I32();
			StructuralSelector structural(type, a, b);
			structural.specificity = in.I32();
			if (in.U8() != 0)
			{
				SharedPtr<SelectorTree> tree = MakeShared<SelectorTree>();
				if (!ReadSelectorTree(state, *tree))
					return false;
				structural.selector_tree = std::move(tree);
			}
			selector.structural_selectors.push_back(std::move(structural));
		}

		selector.pseudo_element = static_cast<PseudoElement>(in.U8());
		selector.scrollbar_part = static_cast<ScrollbarPart>(in.U8());
		in.StrList(selector.scrollbar_part_pseudo_class_names);
		selector.combinator = static_cast<SelectorCombinator>(in.U8());
		return !in.Failed();
	}

	static bool ReadNodeContent(ReadState& state, StyleSheetNode& node, Vector<StyleSheetNode*>* order)
	{
		const PropertySpecification& specification = StyleSheetSpecification::GetPropertySpecification();
		if (!ReadDictionary(state, node.properties, specification, &specification))
			return false;

		BinaryReader& in = state.in;
		const uint32_t num_children = in.U32();
		for (uint32_t i = 0; i < num_children && !in.Failed(); i++)
		{
			CompoundSelector selector;
			if (!ReadSelector(state, selector))
				return false;

			UniquePtr<StyleSheetNode> child = MakeUnique<StyleSheetNode>(&node, std::move(selector));
			StyleSheetNode* child_node = child.get();
			node.children.push_back(std::move(child));
			if (order)
				order->push_back(child_node);

			if (!ReadNodeContent(state, *child_node, order))
				return false;
		}
		return !in.Failed();
	}

	static UniquePtr<StyleSheet> ReadStyleSheet(ReadState& state)
	{
		BinaryReader& in = state.in;
		const PropertySpecification& specification = StyleSheetSpecification::GetPropertySpecification();

		UniquePtr<StyleSheet> sheet(new StyleSheet());
		sheet->specificity_offset = in.I32();

		if (!ReadNodeContent(state, *sheet->root, nullptr))
			return nullptr;

		const uint32_t num_keyframes = in.U32();
		for (uint32_t i = 0; i < num_keyframes && !in.Failed(); i++)
		{
			const String name = in.Str();
			const uint32_t num_blocks = in.U32();
			for (uint32_t block_index = 0; block_index < num_blocks && !in.Failed(); block_index++)
			{
				Keyframes& keyframes = sheet->keyframes[name];
				keyframes.blocks.emplace_back(in.F32());
				if (!ReadDictionary(state, keyframes.blocks.back().properties, specification, &specification))
					return nullptr;
			}
		}

		if (in.Failed())
			return nullptr;

		StyleSheetParser::PostprocessKeyframes(sheet->keyframes);
		return sheet;
	}

	static UniquePtr<StyleSheetContainer> Read(Span<const byte> body, const String& source_path, String& out_error)
	{
		BinaryReader in(body);
		ReadState state{in, {}};

		const uint32_t num_sources = in.U32();
		for (uint32_t i = 0; i < num_sources && !in.Failed(); i++)
		{
			const int line_number = in.I32();
			String rule_name = in.Str();
			state.sources.push_back(MakeShared<PropertySource>(source_path, line_number, std::move(rule_name)));
		}

		UniquePtr<StyleSheetContainer> container = MakeUnique<StyleSheetContainer>();

		const uint32_t num_media_blocks = in.U32();
		for (uint32_t i = 0; i < num_media_blocks && !in.Failed(); i++)
		{
			const MediaQueryModifier modifier = static_cast<MediaQueryModifier>(in.U8());
			PropertyDictionary media_properties;
			if (!ReadDictionary(state, media_properties, StyleSheetParser::GetMediaQuerySpecification(), nullptr))
				break;

			UniquePtr<StyleSheet> sheet = ReadStyleSheet(state);
			if (!sheet)
				break;

			container->media_blocks.emplace_back(std::move(media_properties), SharedPtr<StyleSheet>(std::move(sheet)), modifier);
		}

		if (!in.Failed() && !in.AtEnd())
			in.Fail("unexpected data after the last media block");

		if (in.Failed())
		{
			out_error = in.GetError();
			return nullptr;
		}

		return container;
	}
};

namespace StyleSheetBinaryInternal {
	void Shutdown()
	{
		StyleSheetBinaryDetail::registry_hash_computed = false;
		StyleSheetBinaryDetail::registry_hash = 0;
	}
} // namespace StyleSheetBinaryInternal

uint64_t GetCompiledRegistryHash()
{
	if (!StyleSheetBinaryDetail::registry_hash_computed)
	{
		uint64_t hash = 14695981039346656037ull;
		hash = StyleSheetBinaryDetail::HashFnv1a(hash, StyleSheetSpecification::GetPropertySpecification().GetRegistrySignature());
		hash = StyleSheetBinaryDetail::HashFnv1a(hash, "\n#media\n" + StyleSheetParser::GetMediaQuerySpecification().GetRegistrySignature());
		hash = StyleSheetBinaryDetail::HashFnv1a(hash, "\n#cdata\n" + StyleSheetBinaryDetail::SortedSetSignature(XMLParser::GetPersistentCDATATags()));
		hash = StyleSheetBinaryDetail::HashFnv1a(hash,
			"\n#structural\n" + StyleSheetBinaryDetail::SortedSetSignature(Factory::GetStructuralDataViewAttributeNames()));

		StyleSheetBinaryDetail::registry_hash = hash;
		StyleSheetBinaryDetail::registry_hash_computed = true;
	}
	return StyleSheetBinaryDetail::registry_hash;
}

bool WriteStyleSheetBinary(const StyleSheetContainer& container, Vector<byte>& out_body, String& out_error)
{
	return StyleSheetBinaryAccess::Write(container, out_body, out_error);
}

UniquePtr<StyleSheetContainer> ReadStyleSheetBinary(Span<const byte> body, const String& source_path, String& out_error)
{
	return StyleSheetBinaryAccess::Read(body, source_path, out_error);
}

} // namespace Rml
