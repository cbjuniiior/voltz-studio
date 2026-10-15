#include "FrontendBridge.hpp"

#include <obs.hpp>

#include "moc_FrontendBridge.cpp"

namespace voltz {

FrontendBridge::FrontendBridge(QObject *parent) : QObject(parent)
{
	obs_frontend_add_event_callback(&FrontendBridge::handleEvent, this);
}

FrontendBridge::~FrontendBridge()
{
	/* Após o EXIT a frontend API já foi desligada; remover de novo só gera erro no log */
	if (registered_) {
		obs_frontend_remove_event_callback(&FrontendBridge::handleEvent, this);
	}
}

QStringList FrontendBridge::sceneNames() const
{
	QStringList names;
	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++) {
		names << QString::fromUtf8(obs_source_get_name(scenes.sources.array[i]));
	}
	obs_frontend_source_list_free(&scenes);
	return names;
}

QString FrontendBridge::currentSceneName() const
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	return scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
}

void FrontendBridge::setCurrentScene(const QString &name)
{
	OBSSourceAutoRelease scene = obs_get_source_by_name(name.toUtf8().constData());
	if (scene) {
		obs_frontend_set_current_scene(scene);
	}
}

bool FrontendBridge::isRecording() const
{
	return obs_frontend_recording_active();
}

bool FrontendBridge::isRecordingPaused() const
{
	return obs_frontend_recording_paused();
}

void FrontendBridge::startRecording()
{
	obs_frontend_recording_start();
}

void FrontendBridge::stopRecording()
{
	obs_frontend_recording_stop();
}

void FrontendBridge::setRecordingPaused(bool paused)
{
	obs_frontend_recording_pause(paused);
}

void FrontendBridge::handleEvent(enum obs_frontend_event event, void *data)
{
	static_cast<FrontendBridge *>(data)->dispatch(event);
}

void FrontendBridge::dispatch(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneName());
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		emit currentSceneChanged(currentSceneName());
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
		emit recordingStateChanged(true, false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
		emit recordingStateChanged(true, true);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		emit recordingStateChanged(false, false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		emit recordingTransition(false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		emit recordingTransition(true);
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		/* Seguro durante o próprio evento: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
		obs_frontend_remove_event_callback(&FrontendBridge::handleEvent, this);
		registered_ = false;
		emit exiting();
		break;
	default:
		break;
	}
}

} // namespace voltz
