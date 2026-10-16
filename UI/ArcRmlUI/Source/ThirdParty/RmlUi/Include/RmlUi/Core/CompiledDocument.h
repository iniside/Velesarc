#pragma once

#include "Debug.h"
#include "Header.h"
#include "Span.h"
#include "Types.h"

namespace Rml {

struct CompiledDocumentData;

class CompiledDocument {
public:
	CompiledDocument() = default;
	explicit CompiledDocument(SharedPtr<const CompiledDocumentData> data) : data(std::move(data)) {}

	const SharedPtr<const CompiledDocumentData>& GetData() const { return data; }

private:
	SharedPtr<const CompiledDocumentData> data;
};

RMLUICORE_API bool CompileDocument(const String& document_rml, const String& source_url, Vector<byte>& out_body, String& out_error);

RMLUICORE_API bool ReadCompiledDocument(Span<const byte> body, CompiledDocument& out_document, String& out_error);

RMLUICORE_API void GetCompiledDocumentStyleSheetLinks(const CompiledDocument& document, StringList& out_hrefs);

} // namespace Rml
