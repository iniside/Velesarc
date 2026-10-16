#pragma once

#include "Header.h"
#include "Traits.h"
#include "Types.h"

namespace Rml {

using AsyncInstall = Function<void()>;
using AsyncJob = Function<AsyncInstall()>;

class RMLUICORE_API AsyncJobInterface : public NonCopyMoveable {
public:
	virtual ~AsyncJobInterface();

	virtual void Launch(Function<void()> task) = 0;
};

class RMLUICORE_API AsyncJobOwner : public NonCopyMoveable {
public:
	AsyncJobOwner();
	~AsyncJobOwner();

	void Launch(AsyncJob job) const;
	void Invalidate();

private:
	SharedPtr<int> token;
};

RMLUICORE_API int ProcessAsyncResults();

} // namespace Rml
