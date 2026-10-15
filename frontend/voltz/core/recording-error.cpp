#include "recording-error.hpp"

namespace voltz {

RecordingStopKind classifyRecordingStop(int code)
{
	switch (code) {
	case 0:
		return RecordingStopKind::Success;
	case -6:
		return RecordingStopKind::Unsupported;
	case -7:
		return RecordingStopKind::NoSpace;
	case -8:
		return RecordingStopKind::EncodeError;
	default:
		return RecordingStopKind::Generic;
	}
}

RecordingErrorText recordingErrorText(RecordingStopKind kind, bool hasLastError)
{
	switch (kind) {
	case RecordingStopKind::Success:
		return {};
	case RecordingStopKind::Unsupported:
		return {"Output.RecordFail.Title", "Output.RecordFail.Unsupported", LastErrorUse::None};
	case RecordingStopKind::NoSpace:
		return {"Output.RecordNoSpace.Title", "Output.RecordNoSpace.Msg", LastErrorUse::None};
	case RecordingStopKind::EncodeError:
		if (hasLastError) {
			return {"Output.RecordError.Title", "Output.RecordError.EncodeErrorMsg.LastError",
				LastErrorUse::FormatArg};
		}
		return {"Output.RecordError.Title", "Output.RecordError.EncodeErrorMsg", LastErrorUse::None};
	case RecordingStopKind::Generic:
		return {"Output.RecordError.Title", "Output.RecordError.Msg",
			hasLastError ? LastErrorUse::Append : LastErrorUse::None};
	}
	return {};
}

} // namespace voltz
