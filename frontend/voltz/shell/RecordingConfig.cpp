#include "RecordingConfig.hpp"

#include <widgets/OBSBasic.hpp>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <cstring>
#include <string>
#include <vector>

namespace voltz {

namespace {

config_t *profile()
{
	return OBSBasic::Get()->Config();
}

std::string getString(const char *section, const char *key)
{
	const char *value = config_get_string(profile(), section, key);
	return value ? value : "";
}

} // namespace

bool RecordingConfig::isSimpleMode()
{
	return getString("Output", "Mode") != "Advanced";
}

QString RecordingConfig::folder()
{
	return QString::fromStdString(getString("SimpleOutput", "FilePath"));
}

void RecordingConfig::setFolder(const QString &path)
{
	config_set_string(profile(), "SimpleOutput", "FilePath", path.toUtf8().constData());
	commit(false);
}

RecordingFormat RecordingConfig::format()
{
	return formatFromObs(getString("SimpleOutput", "RecFormat2"));
}

void RecordingConfig::setFormat(RecordingFormat format)
{
	config_set_string(profile(), "SimpleOutput", "RecFormat2", obsRecFormat(format).c_str());
	commit(false);
}

RecordingQuality RecordingConfig::quality()
{
	return qualityFromObs(getString("SimpleOutput", "RecQuality"));
}

void RecordingConfig::setQuality(RecordingQuality quality)
{
	config_set_string(profile(), "SimpleOutput", "RecQuality", obsRecQuality(quality).c_str());
	config_set_string(profile(), "SimpleOutput", "RecEncoder", automaticEncoder().c_str());
	commit(true);
}

std::string RecordingConfig::automaticEncoder()
{
	std::vector<std::string> ids;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (id) {
			ids.emplace_back(id);
		}
	}
	return pickRecEncoder(ids);
}

std::string RecordingConfig::currentEncoder()
{
	return getString("SimpleOutput", "RecEncoder");
}

bool RecordingConfig::recordsWithStreamEncoder()
{
	return getString("SimpleOutput", "RecQuality") == "Stream";
}

std::string RecordingConfig::streamEncoder()
{
	return getString("SimpleOutput", "StreamEncoder");
}

void RecordingConfig::applyVoltzDefaults()
{
	if (!isSimpleMode() || getString("SimpleOutput", "RecQuality") != "Stream") {
		return;
	}
	config_set_string(profile(), "SimpleOutput", "RecQuality", "HQ");
	config_set_string(profile(), "SimpleOutput", "RecEncoder", automaticEncoder().c_str());
	config_set_string(profile(), "SimpleOutput", "RecFormat2", "hybrid_mp4");
	commit(true);
	blog(LOG_INFO, "[voltz] padroes de gravacao aplicados (HQ, %s, hybrid_mp4)", currentEncoder().c_str());
}

void RecordingConfig::commit(bool needsOutputReset)
{
	if (needsOutputReset && !obs_frontend_recording_active() && !obs_frontend_streaming_active() &&
	    !obs_frontend_replay_buffer_active() && !obs_frontend_virtualcam_active()) {
		OBSBasic::Get()->ResetOutputs();
	}
	config_save_safe(profile(), "tmp", nullptr);
}

} // namespace voltz
