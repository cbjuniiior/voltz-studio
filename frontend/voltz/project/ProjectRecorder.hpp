#pragma once

#include "IsoTrack.hpp"

#include <voltz/core/iso-encoders.hpp>
#include <voltz/core/annotation-journal.hpp>
#include <voltz/core/stroke-store.hpp>
#include <voltz/core/project-manifest.hpp>
#include <voltz/core/project-markers.hpp>
#include <voltz/core/project-timeline.hpp>
#include <voltz/shell/RecordingConfig.hpp>
#include <voltz/shell/SharedSources.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QDir>
#include <QFile>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace voltz {

class CursorTracker;
class MouseStyleStore;
class AnnotationController;

// Caminho de pasta/arquivo do Qt → UTF-8 com separadores do Windows, para as funções do núcleo.
inline std::string nativeUtf8Path(const QString &path)
{
	return QDir::toNativeSeparators(path).toUtf8().toStdString();
}

// Gravação em projeto (spec §5). Toda gravação — pela casca, modo clássico, atalho, obs-websocket ou
// --startrecording — vira a pasta "<pasta de gravações>/AAAA-MM-DD HHhMM · Nome" com programa.mp4, projeto.voltz,
// marcadores.json, cursor.jsonl, anotacoes.jsonl e, por fonte de papel Tela/Câmera, tela.mkv/camera.mkv (ISO, Plano
// 3B). Não conhece a casca: reage aos eventos do frontend do OBS e emite sinais (thread da UI).
class ProjectRecorder : public QObject {
	Q_OBJECT

public:
	explicit ProjectRecorder(QObject *parent = nullptr);
	~ProjectRecorder() override;

	// Nome do próximo projeto (user.ini [Voltz] NextProjectName; vazio = "Gravação").
	QString nextProjectName() const;
	void setNextProjectName(const QString &name);

	bool isActive() const { return active_; }
	QString currentProjectDir() const { return projectDir_; }
	QString currentProjectName() const { return QString::fromStdString(manifest_.name); }

	// "Marcador N" agora: capítulo no MP4 + marcadores.json + manifesto. Falso sem projeto gravando ou com a
	// gravação pausada (o OBS não aceita capítulo pausado).
	bool addMarker();

	// Bytes/min medidos nos arquivos do projeto atual (programa + ISO; 0 sem projeto ou antes de 10 s).
	uint64_t measuredBytesPerMinute() const;

	// Faixas ISO (tela.mkv/camera.mkv) gravando agora.
	int runningIsoCount() const;
	// Estilo do destaque do mouse: vai para o manifesto no início do projeto (spec §6.1) e o holofote ligado/desligado
	// durante a gravação vira evento. Chamar uma vez, antes de gravar.
	void setMouseStyleStore(MouseStyleStore *store);
	// Anotações (Plano 4B): durante a gravação, os traços vão para anotacoes.jsonl (diário consolidado no fim) e o
	// "queimadas" vai para o manifesto. Chamar uma vez, antes de gravar.
	void setAnnotationController(AnnotationController *controller);

	// Espaço livre da pasta. VOLTZ_FAKE_FREE_BYTES (só para testes) substitui o valor real. Falso se não souber.
	static bool freeBytes(const QString &folder, quint64 &out);

signals:
	void projectStarted(const QString &dir, const QString &name);
	void projectFinished(const QString &dir, const QString &status);
	void markerAdded(int number, qint64 tMs);
	void stoppedForLowDisk(quint64 freeBytes);
	void manifestWriteFailed(const QString &backupPath);
	// Uma faixa ISO ("tela" | "camera") não iniciou ou parou durante a gravação; `reason` é o "motivo" do evento
	// iso_falhou ("sem_imagem", "canvas", "sem_codificador", "saida", "erro_saida", "sobrecarga"). O programa segue.
	void isoFailed(const QString &track, const QString &reason);

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	static void handleOutputStart(void *data, calldata_t *params);
	static void handleOutputStopping(void *data, calldata_t *params);
	static void handleOutputStop(void *data, calldata_t *params);

	void dispatch(enum obs_frontend_event event);
	void onStarting();
	void onStarted();
	void onPaused(bool paused);
	void onStopped();
	void onExit();
	void finish(ProjectStatus status, const char *eventType, int code = 0);
	void heartbeat();
	void checkDisk();
	bool writeManifest();
	bool writeMarkers();
	void registerHotkey();
	void loadHotkeyBinding();
	void unregisterHotkey();
	void startCursor();
	void stopCursor();
	void startIsoTracks();
	void startIsoTrack(SharedSource role, const char *trackName, const char *fileName, IsoFile &entry,
			   const std::vector<IsoEncoderAttempt> &attempts);
	void requestIsoStop();
	void finishIsoTracks();
	void onIsoFailed(uint64_t generation, const std::string &track, int code);
	void checkIsoLoad();
	void onSpotlightToggled(bool active);
	void startAnnotations();
	// Fecha o diário (traços em andamento terminam em `endNs`) e o consolida no formato da spec.
	void stopAnnotations(int64_t endNs);
	void appendAnnotationLines(const std::vector<std::string> &lines);

	bool registered_ = true;
	bool active_ = false;
	// Muda a cada projeto (onStarting): falha de ISO enfileirada de um projeto anterior não cai no seguinte.
	uint64_t generation_ = 0;
	bool lowDiskStop_ = false;
	bool manifestWarned_ = false;
	QString projectDir_;
	ProjectManifest manifest_;
	ProjectTimeline timeline_;
	std::vector<Marker> markers_;
	std::unique_ptr<CursorTracker> cursor_;
	std::vector<std::unique_ptr<IsoTrack>> isoTracks_;
	// Falhas de início dos ISO, emitidas (isoFailed) depois do projectStarted.
	std::vector<std::pair<QString, QString>> pendingIsoFailures_;
	IsoOverloadMonitor overload_; // exige #include <voltz/core/iso-encoders.hpp> no .hpp
	MouseStyleStore *mouseStyles_ = nullptr;
	/* QPointer: o controlador é filho da casca e pode morrer antes do recorder (no fim do app) */
	QPointer<AnnotationController> annotations_;
	std::unique_ptr<AnnotationJournal> journal_;
	std::unique_ptr<QFile> annotationsFile_;
	ProfileOverride override_;
	QTimer heartbeat_;
	obs_hotkey_id markerHotkey_ = OBS_INVALID_HOTKEY_ID;
	OBSSignal startSignal_;
	OBSSignal stoppingSignal_;
	OBSSignal stopSignal_;
	std::atomic<uint64_t> outputStartNs_{0};
	std::atomic<uint64_t> outputStopNs_{0};
	std::atomic<long long> lastStopCode_{0};
};

} // namespace voltz
