#pragma once

#include <obs-frontend-api.h>

#include <QObject>
#include <QList>
#include <QString>

namespace voltz {

struct SceneEntry {
	QString uuid;
	QString name;
};

// Traduz eventos da frontend API do OBS em sinais Qt e expõe os comandos que a casca usa.
// Vive na thread da UI; os eventos do frontend também chegam nela.
class FrontendBridge : public QObject {
	Q_OBJECT

public:
	explicit FrontendBridge(QObject *parent = nullptr);
	~FrontendBridge() override;

	QList<SceneEntry> scenes() const;
	QString currentSceneUuid() const;
	void setCurrentSceneByUuid(const QString &uuid);

	bool isRecording() const;
	bool isRecordingPaused() const;
	void startRecording();
	void stopRecording();
	void setRecordingPaused(bool paused);
	bool canPauseRecording() const;

signals:
	void sceneListChanged();
	void currentSceneChanged(const QString &uuid);
	void recordingStateChanged(bool active, bool paused);
	// Gravação iniciando (stopping=false) ou finalizando (stopping=true): estado transitório.
	void recordingTransition(bool stopping);
	void exiting();

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	void dispatch(enum obs_frontend_event event);

	bool registered_ = true;
};

} // namespace voltz
