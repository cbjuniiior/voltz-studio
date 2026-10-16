#include "ProjectRecorder.hpp"
#include "CursorTracker.hpp"
#include "MonitorLocator.hpp"

#include <voltz/core/atomic-file.hpp>
#include <voltz/core/disk-space.hpp>
#include <voltz/core/project-naming.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <OBSApp.hpp> // QTStr, App(), GetAppConfigPath

#include <util/config-file.h>
#include <util/platform.h>

#include <QDateTime>
#include <QFileInfo>
#include <QStorageInfo>
#include <QStringList>

#include <algorithm>
#include <cstring>

#include "moc_ProjectRecorder.cpp"

namespace voltz {

namespace {

constexpr const char *kMarkerHotkeyName = "VoltzStudio.AddMarker";
constexpr int kHeartbeatMs = 5000;
constexpr int kIsoStopTimeoutMs = 5000;

int64_t nowNs()
{
	return static_cast<int64_t>(os_gettime_ns());
}

std::string utf8(const QString &text)
{
	return text.toUtf8().toStdString();
}

// "2026-10-15T15:30:12-03:00"
QString isoNowWithOffset()
{
	const QDateTime now = QDateTime::currentDateTime();
	return now.toOffsetFromUtc(now.offsetFromUtc()).toString(Qt::ISODate);
}

// Faixas do Voltz (spec §5.1): 1 = mix, 2 = microfone, 3 = som do sistema. Só nas fontes globais de áudio (canais
// 1-6) que ainda estão em "todas as faixas" (0xFF ao criar, 0x3F ao carregar sem valor): escolha do usuário fica.
void applyVoltzAudioRouting()
{
	for (uint32_t channel = 1; channel <= 6; channel++) {
		OBSSourceAutoRelease source = obs_get_output_source(channel);
		if (!source || (obs_source_get_audio_mixers(source) & 0x3F) != 0x3F) {
			continue;
		}
		const char *id = obs_source_get_unversioned_id(source);
		uint32_t mixers = 0;
		if (strcmp(id, "wasapi_output_capture") == 0) {
			mixers = (1u << 0) | (1u << 2);
		} else if (strcmp(id, "wasapi_input_capture") == 0) {
			mixers = (1u << 0) | (1u << 1);
		} else {
			continue;
		}
		obs_source_set_audio_mixers(source, mixers);
		blog(LOG_INFO, "[voltz] faixas de audio de '%s': %s", obs_source_get_name(source),
		     mixers == 5u ? "1+3 (sistema)" : "1+2 (microfone)");
	}
}

// Tela do cursor: a "Tela" compartilhada se for captura de monitor; senão a primeira captura de monitor da coleção.
OBSSource findScreenCaptureSource()
{
	OBSSource shared = findSharedSource(SharedSource::Screen);
	if (shared && strcmp(obs_source_get_unversioned_id(shared), "monitor_capture") == 0) {
		return shared;
	}
	OBSSource found;
	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			const char *id = obs_source_get_unversioned_id(source);
			if (!id || strcmp(id, "monitor_capture") != 0) {
				return true;
			}
			*static_cast<OBSSource *>(param) = source;
			return false;
		},
		&found);
	return found;
}

// Fonte de papel de um ISO: a compartilhada do preset (user.ini [Voltz]). Nos testes, VOLTZ_TEST_ISO_SCREEN /
// VOLTZ_TEST_ISO_CAMERA trocam pela fonte com esse nome — nunca uma captura de tela/janela/jogo.
OBSSource findIsoSource(SharedSource role)
{
	const char *variable = role == SharedSource::Screen ? "VOLTZ_TEST_ISO_SCREEN" : "VOLTZ_TEST_ISO_CAMERA";
	const QString testName = qEnvironmentVariable(variable);
	if (testName.isEmpty()) {
		return findSharedSource(role);
	}
	OBSSourceAutoRelease source = obs_get_source_by_name(testName.toUtf8().constData());
	const char *id = source ? obs_source_get_unversioned_id(source) : "";
	if (!source || strcmp(id, "monitor_capture") == 0 || strcmp(id, "window_capture") == 0 ||
	    strcmp(id, "game_capture") == 0) {
		blog(LOG_WARNING, "[voltz] %s='%s' ignorada (inexistente ou captura de tela)", variable,
		     qUtf8Printable(testName));
		return {};
	}
	blog(LOG_INFO, "[voltz] iso: fonte de teste '%s' (%s)", qUtf8Printable(testName), variable);
	return OBSSource(source.Get());
}

// Cadeia de encoders do núcleo (spec §5.4) convertida para o libobs.
std::vector<IsoEncoderAttempt> isoAttempts(RecordingQuality quality)
{
	std::vector<IsoEncoderAttempt> attempts;
	const int level = isoQualityLevel(quality);
	for (const IsoEncoderCandidate &candidate :
	     isoEncoderChain(RecordingConfig::registeredEncoderIds(), /*allowSoftware=*/false)) {
		const EncoderSettings values = isoEncoderSettings(candidate, level);
		OBSDataAutoRelease data = obs_data_create();
		for (const auto &item : values.strings) {
			obs_data_set_string(data, item.first.c_str(), item.second.c_str());
		}
		for (const auto &item : values.ints) {
			obs_data_set_int(data, item.first.c_str(), item.second);
		}
		attempts.push_back(IsoEncoderAttempt{candidate.id, OBSData(data.Get())});
	}
	return attempts;
}

FrameCounters programFrameCounters()
{
	video_t *video = obs_get_video();
	FrameCounters counters;
	counters.rendered = obs_get_total_frames();
	counters.lagged = obs_get_lagged_frames();
	counters.encoded = video ? video_output_get_total_frames(video) : 0u;
	counters.skipped = video ? video_output_get_skipped_frames(video) : 0u;
	/* Só para testes: VOLTZ_TEST_ISO_OVERLOAD soma aos "pulados" 20% dos quadros entregues (excesso fictício do
	 * codificador) para exercitar a regra de sobrecarga sem forçar a GPU. */
	if (qEnvironmentVariableIsSet("VOLTZ_TEST_ISO_OVERLOAD")) {
		counters.skipped += counters.encoded / 5u;
	}
	return counters;
}

} // namespace

ProjectRecorder::ProjectRecorder(QObject *parent) : QObject(parent)
{
	heartbeat_.setInterval(kHeartbeatMs);
	connect(&heartbeat_, &QTimer::timeout, this, &ProjectRecorder::heartbeat);
	obs_frontend_add_event_callback(&ProjectRecorder::handleEvent, this);
	/* A casca nasce depois do OBSBasic::OBSInit: perfil e atalhos do OBS já estão carregados. */
	RecordingConfig::applyProjectDefaults();
	registerHotkey();
}

ProjectRecorder::~ProjectRecorder()
{
	stopCursor();
	finishIsoTracks();
	if (registered_) {
		obs_frontend_remove_event_callback(&ProjectRecorder::handleEvent, this);
	}
	unregisterHotkey();
}

QString ProjectRecorder::nextProjectName() const
{
	const char *name = config_get_string(App()->GetUserConfig(), "Voltz", "NextProjectName");
	const QString value = QString::fromUtf8(name ? name : "").trimmed();
	return value.isEmpty() ? QTStr("Voltz.Project.DefaultName") : value;
}

void ProjectRecorder::setNextProjectName(const QString &name)
{
	const QString trimmed = name.trimmed();
	if (trimmed == nextProjectName()) {
		return;
	}
	config_set_string(App()->GetUserConfig(), "Voltz", "NextProjectName", trimmed.toUtf8().constData());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

void ProjectRecorder::handleEvent(enum obs_frontend_event event, void *data)
{
	static_cast<ProjectRecorder *>(data)->dispatch(event);
}

void ProjectRecorder::dispatch(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		onStarting();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		onStarted();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		requestIsoStop();
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		onPaused(true);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		onPaused(false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		onStopped();
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		RecordingConfig::applyProjectDefaults();
		loadHotkeyBinding();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		onExit();
		break;
	default:
		break;
	}
}

void ProjectRecorder::onStarting()
{
	if (active_) {
		/* STARTING sem o STOPPED do projeto anterior (não deveria acontecer): fecha o anterior primeiro. */
		finish(ProjectStatus::Failed, "interrompido");
	}
	const RecordingConfig::OutputPathKey key = RecordingConfig::outputPathKey();
	if (!key.section) {
		blog(LOG_INFO, "[voltz] gravacao para URL (FFmpeg personalizado): sem pasta de projeto");
		return;
	}
	const QString rootPath = RecordingConfig::outputFolder();
	const QDir root(rootPath);
	if (rootPath.isEmpty() || !root.exists()) {
		blog(LOG_WARNING, "[voltz] pasta de gravacoes inexistente: sem pasta de projeto");
		return;
	}

	const QString name = nextProjectName();
	const QDateTime now = QDateTime::currentDateTime();
	const LocalDateTime when{now.date().year(), now.date().month(), now.date().day(), now.time().hour(),
				 now.time().minute()};
	const std::string safeName = sanitizeProjectName(utf8(name), utf8(QTStr("Voltz.Project.DefaultName")));
	const std::string folder =
		uniqueFolderName(projectFolderName(when, safeName), [&root](const std::string &candidate) {
			return root.exists(QString::fromStdString(candidate));
		});
	if (!root.mkdir(QString::fromStdString(folder))) {
		blog(LOG_ERROR, "[voltz] nao foi possivel criar a pasta do projeto '%s'", folder.c_str());
		return;
	}
	projectDir_ = root.absoluteFilePath(QString::fromStdString(folder));

	manifest_ = ProjectManifest{};
	manifest_.app = std::string("Voltz Studio ") + VOLTZ_VERSION;
	manifest_.engine = std::string("libobs ") + obs_get_version_string();
	manifest_.name = utf8(name);
	manifest_.createdAt = utf8(isoNowWithOffset());
	manifest_.status = ProjectStatus::Recording;
	obs_video_info ovi = {};
	obs_get_video_info(&ovi);
	manifest_.program.present = true;
	manifest_.program.file = std::string(kProgramBaseName) + ".mp4";
	manifest_.program.width = static_cast<int>(ovi.output_width);
	manifest_.program.height = static_cast<int>(ovi.output_height);
	manifest_.program.fps = ovi.fps_den ? static_cast<double>(ovi.fps_num) / static_cast<double>(ovi.fps_den) : 0.0;
	manifest_.program.audioTracks = RecordingConfig::recordingAudioTracks();
	timeline_ = ProjectTimeline{};
	markers_.clear();
	lowDiskStop_ = false;
	manifestWarned_ = false;
	outputStartNs_ = 0;
	outputStopNs_ = 0;
	lastStopCode_ = 0;
	active_ = true;
	writeManifest();
	writeMarkers();

	applyVoltzAudioRouting();

	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	if (output) {
		/* (não chamar a variável de "signals": é palavra-chave/macro do Qt) */
		signal_handler_t *handler = obs_output_get_signal_handler(output);
		startSignal_.Connect(handler, "start", &ProjectRecorder::handleOutputStart, this);
		stoppingSignal_.Connect(handler, "stopping", &ProjectRecorder::handleOutputStopping, this);
		stopSignal_.Connect(handler, "stop", &ProjectRecorder::handleOutputStop, this);
	}

	/* O OBSBasic::StartRecording lê a pasta e o formato do nome do arquivo logo depois deste evento, na mesma pilha
	 * da thread da UI (SimpleOutput/AdvancedOutput::ConfigureRecording). Troca temporária, desfeita quando a pilha
	 * volta ao laço de eventos: o perfil salvo nunca vê a pasta do projeto. */
	override_.set(key.section, key.key, nativeUtf8Path(projectDir_));
	override_.set("Output", "FilenameFormatting", kProgramBaseName);
	QTimer::singleShot(0, this, [this]() {
		override_.restore();
		OBSOutputAutoRelease recording = obs_frontend_get_recording_output();
		const bool running = obs_frontend_recording_active() || (recording && obs_output_active(recording));
		if (active_ && !timeline_.started() && !running) {
			blog(LOG_WARNING, "[voltz] a gravacao nao iniciou; projeto marcado como falhou");
			finish(ProjectStatus::Failed, "inicio_falhou");
		}
	});
	blog(LOG_INFO, "[voltz] projeto: %s", qUtf8Printable(projectDir_));
}

void ProjectRecorder::onStarted()
{
	if (!active_ || timeline_.started()) {
		return;
	}
	const uint64_t startNs = outputStartNs_.load();
	timeline_.start(startNs ? static_cast<int64_t>(startNs) : nowNs());
	manifest_.t0Ns = timeline_.t0Ns();
	if (char *last = obs_frontend_get_last_recording()) {
		const QFileInfo file(QString::fromUtf8(last));
		bfree(last);
		if (QDir(file.absolutePath()) == QDir(projectDir_)) {
			manifest_.program.file = utf8(file.fileName());
		} else {
			blog(LOG_WARNING, "[voltz] programa fora da pasta do projeto: %s",
			     qUtf8Printable(file.filePath()));
		}
	}
	startCursor();
	startIsoTracks();
	writeManifest();
	heartbeat_.start();
	blog(LOG_INFO, "[voltz] projeto gravando: %s (%d faixa(s) de audio)", manifest_.program.file.c_str(),
	     manifest_.program.audioTracks);
	emit projectStarted(projectDir_, QString::fromStdString(manifest_.name));
	for (const auto &failure : pendingIsoFailures_) {
		emit isoFailed(failure.first, failure.second);
	}
	pendingIsoFailures_.clear();
	checkDisk();
}

void ProjectRecorder::onPaused(bool paused)
{
	if (!active_ || !timeline_.started()) {
		return;
	}
	for (const auto &track : isoTracks_) {
		track->setPaused(paused);
	}
	const int64_t now = nowNs();
	if (paused) {
		timeline_.pause(now);
	} else {
		timeline_.resume(now);
	}
	manifest_.pauses = timeline_.pauses();
	manifest_.durationMs = timeline_.grossMs(now);
	writeManifest();
}

void ProjectRecorder::onStopped()
{
	if (!active_) {
		return;
	}
	const int code = static_cast<int>(lastStopCode_.load());
	if (code != 0) {
		blog(LOG_WARNING, "[voltz] gravacao terminou com erro (codigo %d)", code);
		finish(ProjectStatus::Failed, "erro_gravacao", code);
	} else {
		finish(ProjectStatus::Ok, nullptr);
	}
}

void ProjectRecorder::onExit()
{
	if (active_) {
		/* Fechar o app gravando: a saída ainda está ativa e o OBS finaliza o arquivo ao destruí-la, depois deste
		 * evento (OBSBasic::~OBSBasic → outputHandler.reset()). */
		finish(ProjectStatus::Ok, "app_fechado_gravando");
	}
	unregisterHotkey();
	/* Seguro durante o próprio evento: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
	obs_frontend_remove_event_callback(&ProjectRecorder::handleEvent, this);
	registered_ = false;
}

void ProjectRecorder::finish(ProjectStatus status, const char *eventType, int code)
{
	const uint64_t stopNs = outputStopNs_.load();
	const int64_t end = stopNs ? static_cast<int64_t>(stopNs) : nowNs();
	if (timeline_.started()) {
		if (eventType) {
			manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(end), eventType, "", code});
		}
		timeline_.stop(end);
		manifest_.durationMs = timeline_.grossMs(end);
		manifest_.pauses = timeline_.pauses();
	} else if (eventType) {
		manifest_.events.push_back(ManifestEvent{0, eventType, "", code});
	}
	manifest_.status = status;
	heartbeat_.stop();
	stopCursor();
	finishIsoTracks();
	writeManifest();
	startSignal_.Disconnect();
	stoppingSignal_.Disconnect();
	stopSignal_.Disconnect();
	override_.restore();
	active_ = false;
	blog(LOG_INFO, "[voltz] projeto finalizado (%s, %lld ms, %d marcador(es)): %s", statusToString(status),
	     static_cast<long long>(manifest_.durationMs), static_cast<int>(markers_.size()),
	     qUtf8Printable(projectDir_));
	emit projectFinished(projectDir_, QString::fromUtf8(statusToString(status)));
}

void ProjectRecorder::heartbeat()
{
	if (!active_ || !timeline_.started()) {
		return;
	}
	const int64_t now = nowNs();
	manifest_.durationMs = timeline_.grossMs(now);
	manifest_.pauses = timeline_.pauses();
	checkIsoLoad();
	writeManifest();
	checkDisk();
}

void ProjectRecorder::checkDisk()
{
	quint64 free = 0;
	if (!active_ || lowDiskStop_ || !freeBytes(projectDir_, free) || diskLevel(free) != DiskLevel::Critical) {
		return;
	}
	lowDiskStop_ = true;
	manifest_.events.push_back(ManifestEvent{timeline_.relativeMs(nowNs()), "disco_cheio", "", 0});
	writeManifest();
	blog(LOG_WARNING, "[voltz] disco quase cheio (%llu bytes livres): parando a gravacao",
	     static_cast<unsigned long long>(free));
	/* Parada normal (não forçada): o OBS finaliza o arquivo e o STOPPED fecha o manifesto como "ok". */
	obs_frontend_recording_stop();
	emit stoppedForLowDisk(free);
}

bool ProjectRecorder::freeBytes(const QString &folder, quint64 &out)
{
	bool fake = false;
	const quint64 fakeValue = qEnvironmentVariable("VOLTZ_FAKE_FREE_BYTES").toULongLong(&fake);
	if (fake) {
		out = fakeValue;
		return true;
	}
	if (folder.isEmpty()) {
		return false;
	}
	const QStorageInfo storage(folder);
	if (!storage.isValid() || storage.bytesAvailable() < 0) {
		return false;
	}
	out = static_cast<quint64>(storage.bytesAvailable());
	return true;
}

uint64_t ProjectRecorder::measuredBytesPerMinute() const
{
	if (!active_ || !timeline_.started()) {
		return 0;
	}
	const QDir dir(projectDir_);
	qint64 size = 0;
	auto add = [&dir, &size](bool present, const std::string &file) {
		if (!present || file.empty()) {
			return;
		}
		const QFileInfo info(dir.filePath(QString::fromStdString(file)));
		if (info.exists()) {
			size += info.size();
		}
	};
	add(manifest_.program.present, manifest_.program.file);
	add(manifest_.screen.present, manifest_.screen.file);
	add(manifest_.camera.present, manifest_.camera.file);
	/* qualificado: a função livre do núcleo tem o mesmo nome deste método */
	return voltz::measuredBytesPerMinute(static_cast<uint64_t>(size), timeline_.netMs(nowNs()));
}

bool ProjectRecorder::writeManifest()
{
	const std::string json = serializeManifest(manifest_);
	const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(kManifestFileName)));
	std::string error;
	/* §8: tenta de novo; se persistir, guarda uma cópia em <config>/recovery e avisa uma vez. */
	if (writeFileAtomically(path, json, &error) || writeFileAtomically(path, json, &error)) {
		return true;
	}
	blog(LOG_ERROR, "[voltz] falha ao gravar o manifesto: %s", error.c_str());
	char dir[512];
	if (GetAppConfigPath(dir, sizeof(dir), VOLTZ_CONFIG_DIR "/recovery") <= 0) {
		return false;
	}
	const QString recoveryDir = QString::fromUtf8(dir);
	QDir().mkpath(recoveryDir);
	const QString backup = QDir(recoveryDir).filePath(QFileInfo(projectDir_).fileName() + QStringLiteral(".voltz"));
	if (writeFileAtomically(nativeUtf8Path(backup), json, &error)) {
		blog(LOG_WARNING, "[voltz] copia do manifesto em %s", qUtf8Printable(backup));
		if (!manifestWarned_) {
			manifestWarned_ = true;
			emit manifestWriteFailed(backup);
		}
	}
	return false;
}

bool ProjectRecorder::writeMarkers()
{
	const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(kMarkersFileName)));
	std::string error;
	if (writeFileAtomically(path, serializeMarkers(markers_), &error)) {
		return true;
	}
	blog(LOG_ERROR, "[voltz] falha ao gravar marcadores.json: %s", error.c_str());
	return false;
}

bool ProjectRecorder::addMarker()
{
	if (!active_ || !timeline_.started() || timeline_.paused()) {
		blog(LOG_INFO, "[voltz] marcador ignorado: sem projeto gravando ou gravacao pausada");
		return false;
	}
	const int64_t now = nowNs();
	const int number = static_cast<int>(markers_.size()) + 1;
	const QString name = QTStr("Voltz.Marker.Name").arg(number);
	const int64_t tMs = timeline_.relativeMs(now);
	/* Capítulo no MP4/MOV híbrido; em outros formatos o OBS devolve falso e fica só o marcadores.json. */
	const bool chapter = obs_frontend_recording_add_chapter(name.toUtf8().constData());
	markers_.push_back(Marker{tMs, utf8(name)});
	writeMarkers();
	manifest_.durationMs = timeline_.grossMs(now);
	writeManifest();
	blog(LOG_INFO, "[voltz] marcador %d em %lld ms (capitulo no MP4: %s)", number, static_cast<long long>(tMs),
	     chapter ? "sim" : "nao");
	emit markerAdded(number, tMs);
	return true;
}

void ProjectRecorder::registerHotkey()
{
	if (markerHotkey_ != OBS_INVALID_HOTKEY_ID) {
		return;
	}
	markerHotkey_ = obs_hotkey_register_frontend(kMarkerHotkeyName,
						     QTStr("Voltz.Hotkey.AddMarker").toUtf8().constData(),
						     &ProjectRecorder::handleHotkey, this);
	loadHotkeyBinding();
}

void ProjectRecorder::loadHotkeyBinding()
{
	if (markerHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	/* Mesmo formato do OBS (OBSBasic::InitHotkeys / OBSBasicSettings::SaveHotkeySettings): perfil
	 * [Hotkeys] <nome> = {"bindings":[...]}. Nada salvo: Ctrl+Shift+M. Lista vazia salva = sem atalho. */
	config_t *profile = obs_frontend_get_profile_config();
	const char *json = profile ? config_get_string(profile, "Hotkeys", kMarkerHotkeyName) : nullptr;
	OBSDataArrayAutoRelease bindings;
	if (json && *json) {
		OBSDataAutoRelease data = obs_data_create_from_json(json);
		if (data) {
			bindings = obs_data_get_array(data, "bindings");
		}
	} else {
		bindings = obs_data_array_create();
		OBSDataAutoRelease combo = obs_data_create();
		obs_data_set_bool(combo, "control", true);
		obs_data_set_bool(combo, "shift", true);
		obs_data_set_string(combo, "key", "OBS_KEY_M");
		obs_data_array_push_back(bindings, combo);
	}
	obs_hotkey_load(markerHotkey_, bindings);
}

void ProjectRecorder::unregisterHotkey()
{
	if (markerHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	obs_hotkey_unregister(markerHotkey_);
	markerHotkey_ = OBS_INVALID_HOTKEY_ID;
}

void ProjectRecorder::handleHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<ProjectRecorder *>(data);
	/* Teclado: chega pela rota do OBSBasic (thread da UI). obs-websocket TriggerHotkeyByName: thread do websocket,
	 * com o lock dos atalhos do libobs preso — só enfileira. */
	QMetaObject::invokeMethod(self, [self]() { self->addMarker(); }, Qt::QueuedConnection);
}

void ProjectRecorder::handleOutputStart(void *data, calldata_t *)
{
	static_cast<ProjectRecorder *>(data)->outputStartNs_.store(os_gettime_ns());
}

void ProjectRecorder::handleOutputStopping(void *data, calldata_t *)
{
	static_cast<ProjectRecorder *>(data)->outputStopNs_.store(os_gettime_ns());
}

void ProjectRecorder::handleOutputStop(void *data, calldata_t *params)
{
	auto *self = static_cast<ProjectRecorder *>(data);
	self->lastStopCode_.store(calldata_int(params, "code"));
	if (self->outputStopNs_.load() == 0) {
		self->outputStopNs_.store(os_gettime_ns());
	}
}

void ProjectRecorder::startCursor()
{
	stopCursor();
	OBSSource screen = findScreenCaptureSource();
	std::string monitorId;
	MonitorDescription monitor;
	if (screen) {
		OBSDataAutoRelease settings = obs_source_get_settings(screen);
		monitorId = obs_data_get_string(settings, "monitor_id");
		monitor = findMonitor(monitorId);
	}
	if (!monitor.found) {
		/* Sem captura de monitor (ex. só captura de janela, fora do 3A): projeto sem cursor.jsonl. */
		blog(LOG_INFO, "[voltz] cursor: nenhuma captura de monitor encontrada; sem cursor.jsonl");
		manifest_.events.push_back(ManifestEvent{0, "cursor_indisponivel", "", 0});
		return;
	}
	const CaptureMapping mapping{monitor.rect, monitor.rect.width, monitor.rect.height};
	auto tracker = std::make_unique<CursorTracker>();
	const QString path = QDir(projectDir_).filePath(QString::fromUtf8(kCursorFileName));
	if (!tracker->start(nativeUtf8Path(path), mapping, timeline_.t0Ns())) {
		blog(LOG_WARNING, "[voltz] cursor: nao foi possivel iniciar o gancho do mouse");
		manifest_.events.push_back(ManifestEvent{0, "cursor_indisponivel", "", 0});
		return;
	}
	cursor_ = std::move(tracker);
	manifest_.cursor.present = true;
	manifest_.cursor.file = kCursorFileName;
	manifest_.cursor.width = monitor.rect.width;
	manifest_.cursor.height = monitor.rect.height;
	manifest_.cursor.origin.type = "monitor";
	manifest_.cursor.origin.id = monitorId;
	manifest_.cursor.origin.rect[0] = monitor.rect.left;
	manifest_.cursor.origin.rect[1] = monitor.rect.top;
	manifest_.cursor.origin.rect[2] = monitor.rect.width;
	manifest_.cursor.origin.rect[3] = monitor.rect.height;
	manifest_.cursor.origin.scale = monitor.scale;
	blog(LOG_INFO, "[voltz] cursor: monitor (%d,%d %dx%d) escala %d%%", monitor.rect.left, monitor.rect.top,
	     monitor.rect.width, monitor.rect.height, static_cast<int>(monitor.scale * 100.0 + 0.5));
}

void ProjectRecorder::stopCursor()
{
	if (!cursor_) {
		return;
	}
	cursor_->stop();
	blog(LOG_INFO, "[voltz] cursor: %llu linha(s), %llu evento(s) descartado(s)",
	     static_cast<unsigned long long>(cursor_->linesWritten()),
	     static_cast<unsigned long long>(cursor_->droppedEvents()));
	cursor_.reset();
}

int ProjectRecorder::runningIsoCount() const
{
	int running = 0;
	for (const auto &track : isoTracks_) {
		if (track->running()) {
			running++;
		}
	}
	return running;
}

void ProjectRecorder::startIsoTracks()
{
	finishIsoTracks();
	pendingIsoFailures_.clear();
	overload_.reset(programFrameCounters());
	manifest_.screen = IsoFile{};
	manifest_.camera = IsoFile{};
	if (RecordingConfig::programOnly()) {
		blog(LOG_INFO, "[voltz] iso: desligado (gravar so a versao montada)");
		return;
	}
	const std::vector<IsoEncoderAttempt> attempts = isoAttempts(RecordingConfig::quality());
	startIsoTrack(SharedSource::Screen, kScreenTrack, kScreenIsoFileName, manifest_.screen, attempts);
	startIsoTrack(SharedSource::Camera, kCameraTrack, kCameraIsoFileName, manifest_.camera, attempts);
}

void ProjectRecorder::startIsoTrack(SharedSource role, const char *trackName, const char *fileName, IsoFile &entry,
				    const std::vector<IsoEncoderAttempt> &attempts)
{
	OBSSource source = findIsoSource(role);
	if (!source) {
		/* Sem preset (ou sem a fonte): não há papel a gravar. Sem evento — não é falha. */
		blog(LOG_INFO, "[voltz] iso %s: sem fonte de papel; sem %s", trackName, fileName);
		return;
	}
	auto track = std::make_unique<IsoTrack>(trackName);
	const std::string name = trackName;
	track->onFailed = [this, name](int code) {
		/* thread do libobs: só enfileira */
		QMetaObject::invokeMethod(
			this, [this, name, code]() { onIsoFailed(name, code); }, Qt::QueuedConnection);
	};
	IsoFailure failure = IsoFailure::None;
	const std::string path = nativeUtf8Path(QDir(projectDir_).filePath(QString::fromUtf8(fileName)));
	if (!track->start(source, path, attempts, failure)) {
		const char *reason = isoFailureCode(failure);
		manifest_.events.push_back(
			ManifestEvent{timeline_.relativeMs(nowNs()), "iso_falhou", trackName, 0, reason});
		pendingIsoFailures_.emplace_back(QString::fromUtf8(trackName), QString::fromUtf8(reason));
		return;
	}
	entry.present = true;
	entry.file = fileName;
	entry.width = static_cast<int>(track->width());
	entry.height = static_cast<int>(track->height());
	entry.fps = track->fps();
	entry.offsetMs = track->startNs() ? timeline_.relativeMs(static_cast<int64_t>(track->startNs())) : 0;
	entry.encoder = track->encoderId();
	if (role == SharedSource::Screen && strcmp(obs_source_get_unversioned_id(source), "monitor_capture") == 0) {
		OBSDataAutoRelease settings = obs_source_get_settings(source);
		const std::string monitorId = obs_data_get_string(settings, "monitor_id");
		const MonitorDescription monitor = findMonitor(monitorId);
		if (monitor.found) {
			entry.hasOrigin = true;
			entry.origin.type = "monitor";
			entry.origin.id = monitorId;
			entry.origin.rect[0] = monitor.rect.left;
			entry.origin.rect[1] = monitor.rect.top;
			entry.origin.rect[2] = monitor.rect.width;
			entry.origin.rect[3] = monitor.rect.height;
			entry.origin.scale = monitor.scale;
		}
	}
	blog(LOG_INFO, "[voltz] iso %s: %dx%d @ %.3f fps, %s, offset %lld ms", trackName, entry.width, entry.height,
	     entry.fps, entry.encoder.c_str(), static_cast<long long>(entry.offsetMs));
	isoTracks_.push_back(std::move(track));
}

void ProjectRecorder::onIsoFailed(const std::string &track, int code)
{
	if (!active_) {
		return;
	}
	blog(LOG_WARNING, "[voltz] iso %s: parou sozinha (codigo %d); o programa continua", track.c_str(), code);
	manifest_.events.push_back(
		ManifestEvent{timeline_.relativeMs(nowNs()), "iso_falhou", track, code, "erro_saida"});
	writeManifest();
	emit isoFailed(QString::fromStdString(track), QStringLiteral("erro_saida"));
}

void ProjectRecorder::checkIsoLoad()
{
	if (runningIsoCount() == 0) {
		return;
	}
	/* A primeira janela só calibra; sobrecarga = duas janelas ruins seguidas (ver IsoOverloadMonitor). */
	if (!overload_.feed(programFrameCounters())) {
		return;
	}
	blog(LOG_WARNING, "[voltz] iso: o programa esta perdendo quadros; desligando as faixas separadas");
	const int64_t tMs = timeline_.relativeMs(nowNs());
	QStringList stopped;
	for (const auto &track : isoTracks_) {
		if (track->running()) {
			manifest_.events.push_back(ManifestEvent{tMs, "iso_falhou", track->track(), 0, "sobrecarga"});
			stopped.append(QString::fromStdString(track->track()));
		}
	}
	finishIsoTracks();
	for (const QString &track : stopped) {
		emit isoFailed(track, QStringLiteral("sobrecarga"));
	}
}

void ProjectRecorder::requestIsoStop()
{
	for (const auto &track : isoTracks_) {
		track->requestStop();
	}
}

void ProjectRecorder::finishIsoTracks()
{
	requestIsoStop();
	for (const auto &track : isoTracks_) {
		track->finish(kIsoStopTimeoutMs);
	}
	isoTracks_.clear();
}

} // namespace voltz
