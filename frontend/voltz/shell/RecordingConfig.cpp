#include "RecordingConfig.hpp"

#include <widgets/OBSBasic.hpp>
#include <utility/platform.hpp> // GetDefaultVideoSavePath

#include <voltz/core/disk-space.hpp>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QDir>

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

// Faixas da gravação em projeto (spec §5.1): 1 = mix, 2 = microfone, 3 = som do sistema.
constexpr int64_t kVoltzRecTracks = (1 << 0) | (1 << 1) | (1 << 2);

int trackCount(int64_t bits)
{
	int count = 0;
	for (int i = 0; i < 6; i++) {
		if (bits & (int64_t(1) << i)) {
			count++;
		}
	}
	return count > 0 ? count : 1;
}

// Neste libobs, config_set_default_* também copia o padrão para os valores do usuário quando a chave não existe
// (config_set_item_default), então config_has_user_value é sempre verdadeiro para chaves com padrão. "Ainda no
// padrão do OBS" = sem valor do usuário ou valor igual ao padrão.
bool stillObsDefault(config_t *config, const char *section, const char *key)
{
	if (!config_has_user_value(config, section, key)) {
		return true;
	}
	if (!config_has_default_value(config, section, key)) {
		return false;
	}
	const char *value = config_get_string(config, section, key);
	const char *fallback = config_get_default_string(config, section, key);
	return strcmp(value ? value : "", fallback ? fallback : "") == 0;
}

QString voltzDefaultFolder()
{
	return QDir::toNativeSeparators(QString::fromStdString(GetDefaultVideoSavePath()) +
					QStringLiteral("/Voltz Studio"));
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

RecordingConfig::OutputPathKey RecordingConfig::outputPathKey()
{
	if (isSimpleMode()) {
		return {"SimpleOutput", "FilePath"};
	}
	if (getString("AdvOut", "RecType") == "FFmpeg") {
		if (!config_get_bool(profile(), "AdvOut", "FFOutputToFile")) {
			return {};
		}
		return {"AdvOut", "FFFilePath"};
	}
	return {"AdvOut", "RecFilePath"};
}

QString RecordingConfig::outputFolder()
{
	const OutputPathKey key = outputPathKey();
	return key.section ? QString::fromStdString(getString(key.section, key.key)) : QString();
}

int RecordingConfig::recordingAudioTracks()
{
	if (isSimpleMode()) {
		if (getString("SimpleOutput", "RecQuality") == "Stream" ||
		    getString("SimpleOutput", "RecFormat2") == "flv") {
			return 1;
		}
		return trackCount(config_get_int(profile(), "SimpleOutput", "RecTracks"));
	}
	return trackCount(config_get_int(profile(), "AdvOut", "RecTracks"));
}

void RecordingConfig::applyProjectDefaults()
{
	config_t *config = profile();
	const QString voltzFolder = voltzDefaultFolder();
	const std::string voltzFolderUtf8 = voltzFolder.toUtf8().toStdString();
	/* Uma vez por perfil (marca [Voltz] ProjectDefaults no perfil): depois disso, qualquer valor — inclusive o
	 * padrão do OBS escolhido de volta pelo usuário — é escolha do usuário. */
	if (!config_get_bool(config, "Voltz", "ProjectDefaults")) {
		const OutputPathKey keys[] = {{"SimpleOutput", "FilePath"}, {"AdvOut", "RecFilePath"}};
		for (const OutputPathKey &key : keys) {
			if (stillObsDefault(config, key.section, key.key)) {
				config_set_string(config, key.section, key.key, voltzFolderUtf8.c_str());
			}
		}
		if (stillObsDefault(config, "SimpleOutput", "RecTracks")) {
			config_set_int(config, "SimpleOutput", "RecTracks", kVoltzRecTracks);
		}
		config_set_bool(config, "Voltz", "ProjectDefaults", true);
		commit(false);
		blog(LOG_INFO, "[voltz] padroes de projeto aplicados (pasta '%s', %d faixa(s) de audio)",
		     qUtf8Printable(outputFolder()), recordingAudioTracks());
	}
	/* O OBS recusa gravar numa pasta que não existe (OBSBasic::OutputPathValid): cria só a pasta Voltz. */
	if (QDir::toNativeSeparators(outputFolder()).compare(voltzFolder, Qt::CaseInsensitive) == 0) {
		QDir().mkpath(voltzFolder);
	}
}

uint64_t RecordingConfig::estimatedBytesPerMinute()
{
	RateInput input;
	obs_video_info ovi = {};
	if (obs_get_video_info(&ovi)) {
		input.width = static_cast<int>(ovi.output_width);
		input.height = static_cast<int>(ovi.output_height);
		input.fps = ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 60.0;
	}
	input.audioTracks = recordingAudioTracks();
	if (isSimpleMode()) {
		const std::string quality = getString("SimpleOutput", "RecQuality");
		input.streamEncoder = quality == "Stream";
		input.compact = quality == "Small";
		input.streamKbps = static_cast<int>(config_get_int(profile(), "SimpleOutput", "VBitrate"));
	}
	return estimateBytesPerMinute(input);
}

void ProfileOverride::set(const char *section, const char *key, const std::string &value)
{
	config_t *config = profile();
	Saved saved;
	saved.section = section;
	saved.key = key;
	saved.hadUserValue = config_has_user_value(config, section, key);
	if (saved.hadUserValue) {
		const char *current = config_get_string(config, section, key);
		saved.value = current ? current : "";
	}
	saved_.push_back(saved);
	config_set_string(config, section, key, value.c_str());
}

void ProfileOverride::restore()
{
	if (saved_.empty()) {
		return;
	}
	config_t *config = profile();
	for (auto it = saved_.rbegin(); it != saved_.rend(); ++it) {
		if (it->hadUserValue) {
			config_set_string(config, it->section.c_str(), it->key.c_str(), it->value.c_str());
		} else {
			config_remove_value(config, it->section.c_str(), it->key.c_str());
		}
	}
	saved_.clear();
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
