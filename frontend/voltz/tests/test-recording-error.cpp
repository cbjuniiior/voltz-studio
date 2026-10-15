#include <doctest.h>

#include <voltz/core/recording-error.hpp>

using voltz::LastErrorUse;
using voltz::RecordingStopKind;

TEST_CASE("classifyRecordingStop mapeia os códigos OBS_OUTPUT_*")
{
	CHECK(voltz::classifyRecordingStop(0) == RecordingStopKind::Success);
	CHECK(voltz::classifyRecordingStop(-6) == RecordingStopKind::Unsupported);
	CHECK(voltz::classifyRecordingStop(-7) == RecordingStopKind::NoSpace);
	CHECK(voltz::classifyRecordingStop(-8) == RecordingStopKind::EncodeError);
	CHECK(voltz::classifyRecordingStop(-1) == RecordingStopKind::Generic);
	CHECK(voltz::classifyRecordingStop(-4) == RecordingStopKind::Generic);
	CHECK(voltz::classifyRecordingStop(42) == RecordingStopKind::Generic);
}

TEST_CASE("recordingErrorText: sucesso não tem texto")
{
	const voltz::RecordingErrorText t = voltz::recordingErrorText(RecordingStopKind::Success, true);
	CHECK(t.titleKey.empty());
	CHECK(t.messageKey.empty());
	CHECK(t.lastErrorUse == LastErrorUse::None);
}

TEST_CASE("recordingErrorText: sem suporte e sem espaço ignoram o last_error")
{
	const voltz::RecordingErrorText u = voltz::recordingErrorText(RecordingStopKind::Unsupported, true);
	CHECK(u.titleKey == "Output.RecordFail.Title");
	CHECK(u.messageKey == "Output.RecordFail.Unsupported");
	CHECK(u.lastErrorUse == LastErrorUse::None);

	const voltz::RecordingErrorText n = voltz::recordingErrorText(RecordingStopKind::NoSpace, true);
	CHECK(n.titleKey == "Output.RecordNoSpace.Title");
	CHECK(n.messageKey == "Output.RecordNoSpace.Msg");
	CHECK(n.lastErrorUse == LastErrorUse::None);
}

TEST_CASE("recordingErrorText: erro de codificação usa %1 quando há last_error")
{
	const voltz::RecordingErrorText with = voltz::recordingErrorText(RecordingStopKind::EncodeError, true);
	CHECK(with.titleKey == "Output.RecordError.Title");
	CHECK(with.messageKey == "Output.RecordError.EncodeErrorMsg.LastError");
	CHECK(with.lastErrorUse == LastErrorUse::FormatArg);

	const voltz::RecordingErrorText without = voltz::recordingErrorText(RecordingStopKind::EncodeError, false);
	CHECK(without.messageKey == "Output.RecordError.EncodeErrorMsg");
	CHECK(without.lastErrorUse == LastErrorUse::None);
}

TEST_CASE("recordingErrorText: erro genérico anexa o last_error quando existe")
{
	const voltz::RecordingErrorText with = voltz::recordingErrorText(RecordingStopKind::Generic, true);
	CHECK(with.titleKey == "Output.RecordError.Title");
	CHECK(with.messageKey == "Output.RecordError.Msg");
	CHECK(with.lastErrorUse == LastErrorUse::Append);

	const voltz::RecordingErrorText without = voltz::recordingErrorText(RecordingStopKind::Generic, false);
	CHECK(without.lastErrorUse == LastErrorUse::None);
}
