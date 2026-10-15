#pragma once

#include <obs-frontend-api.h>

#include <QObject>
#include <QStringList>

namespace voltz {

// Traduz eventos da frontend API do OBS em sinais Qt e expõe os comandos que a casca usa.
// Vive na thread da UI; os eventos do frontend também chegam nela.
class FrontendBridge : public QObject {
	Q_OBJECT

public:
	explicit FrontendBridge(QObject *parent = nullptr);
	~FrontendBridge() override;

	QStringList sceneNames() const;
	QString currentSceneName() const;
	void setCurrentScene(const QString &name);

	bool isRecording() const;
	bool isRecordingPaused() const;
	void startRecording();
	void stopRecording();
	void setRecordingPaused(bool paused);

signals:
	void sceneListChanged();
	void currentSceneChanged(const QString &name);
	void recordingStateChanged(bool active, bool paused);
	void exiting();

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	void dispatch(enum obs_frontend_event event);
};

} // namespace voltz
