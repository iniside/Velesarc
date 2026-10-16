#pragma once

#include "Header.h"
#include "Types.h"

namespace Rml {

class DataTypeRegister;

enum class ConstantDataExpressionResult {
	Constant,
	ReferencesVariable,
	Invalid,
};

// Evaluates a data expression that references no data variable (including 'ev.' addresses), without a data model; the parser and interpreter do not log, transforms still may.
RMLUICORE_API ConstantDataExpressionResult EvaluateConstantDataExpression(const String& expression, DataTypeRegister& type_register,
	Variant& out_value);

} // namespace Rml
