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

QList<SceneEntry> FrontendBridge::scenes() const
{
	QList<SceneEntry> result;
	struct obs_frontend_source_list list = {};
	obs_frontend_get_scenes(&list);
	for (size_t i = 0; i < list.sources.num; i++) {
		obs_source_t *source = list.sources.array[i];
		result.append({QString::fromUtf8(obs_source_get_uuid(source)),
			       QString::fromUtf8(obs_source_get_name(source))});
	}
	obs_frontend_source_list_free(&list);
	return result;
}

QString FrontendBridge::currentSceneUuid() const
{
	OBSSourceAutoRelease scene = obs_frontend_get_current_scene();
	return scene ? QString::fromUtf8(obs_source_get_uuid(scene)) : QString();
}

void FrontendBridge::setCurrentSceneByUuid(const QString &uuid)
{
	OBSSourceAutoRelease scene = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (scene) {
		obs_frontend_set_current_scene(scene);
	}
}

bool FrontendBridge::canPauseRecording() const
{
	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	return output && obs_output_can_pause(output);
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

void FrontendBridge::watchRecordingOutput()
{
	OBSOutputAutoRelease output = obs_frontend_get_recording_output();
	if (!output) {
		recordingStopSignal_.Disconnect();
		return;
	}
	recordingStopSignal_.Connect(obs_output_get_signal_handler(output), "stop", &FrontendBridge::handleOutputStop,
				     this);
}

void FrontendBridge::handleOutputStop(void *data, calldata_t *params)
{
	/* Thread do libobs: só copia os dados e entrega na thread da UI. */
	auto *bridge = static_cast<FrontendBridge *>(data);
	const int code = int(calldata_int(params, "code"));
	const char *error = calldata_string(params, "last_error");
	const QString lastError = error ? QString::fromUtf8(error) : QString();
	if (code == 0) {
		return;
	}
	QMetaObject::invokeMethod(
		bridge, [bridge, code, lastError]() { emit bridge->recordingFailed(code, lastError); },
		Qt::QueuedConnection);
}

void FrontendBridge::handleEvent(enum obs_frontend_event event, void *data)
{
	static_cast<FrontendBridge *>(data)->dispatch(event);
}

void FrontendBridge::dispatch(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		emit sceneCollectionChanged();
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneUuid());
		break;
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		emit sceneListChanged();
		emit currentSceneChanged(currentSceneUuid());
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
		emit currentSceneChanged(currentSceneUuid());
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
		watchRecordingOutput();
		emit recordingTransition(false);
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		emit recordingTransition(true);
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		recordingStopSignal_.Disconnect();
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
