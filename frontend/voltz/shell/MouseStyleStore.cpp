#include "MouseStyleStore.hpp"
#include "HotkeyBinding.hpp"

#include <OBSApp.hpp> // QTStr, App()

#include <util/config-file.h>

#include <string>

#include "moc_MouseStyleStore.cpp"

namespace voltz {

namespace {

constexpr const char *kSpotlightHotkeyName = "VoltzStudio.ToggleSpotlight";
constexpr int kSaveDelayMs = 400;

std::string readVoltz(const char *key)
{
	const char *value = config_get_string(App()->GetUserConfig(), "Voltz", key);
	return value ? value : "";
}

void writeVoltz(const char *key, const std::string &value)
{
	config_set_string(App()->GetUserConfig(), "Voltz", key, value.c_str());
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

} // namespace

MouseStyleStore::MouseStyleStore(QObject *parent) : QObject(parent)
{
	const std::string json = readVoltz("MouseStyle");
	if (!json.empty() && !mouseStyleFromJson(json, style_)) {
		blog(LOG_WARNING, "[voltz] MouseStyle invalido no user.ini; usando o padrao");
	}
	saveTimer_.setSingleShot(true);
	saveTimer_.setInterval(kSaveDelayMs);
	connect(&saveTimer_, &QTimer::timeout, this, [this]() { save(); });
	/* A casca nasce depois do OBSBasic::OBSInit: perfil e atalhos do OBS já estão carregados. */
	spotlightHotkey_ = obs_hotkey_register_frontend(kSpotlightHotkeyName,
							QTStr("Voltz.Hotkey.ToggleSpotlight").toUtf8().constData(),
							&MouseStyleStore::handleHotkey, this);
	loadHotkeyBinding();
	obs_frontend_add_event_callback(&MouseStyleStore::handleEvent, this);
}

MouseStyleStore::~MouseStyleStore()
{
	if (saveTimer_.isActive()) {
		save();
	}
	if (registered_) {
		obs_frontend_remove_event_callback(&MouseStyleStore::handleEvent, this);
	}
	unregisterHotkey();
}

void MouseStyleStore::setStyle(const MouseStyle &style)
{
	const bool spotlightBefore = style_.spotlight.active;
	style_ = clampMouseStyle(style);
	saveTimer_.start();
	emit styleChanged();
	if (style_.spotlight.active != spotlightBefore) {
		blog(LOG_INFO, "[voltz] holofote: %s", style_.spotlight.active ? "ligado" : "desligado");
		emit spotlightToggled(style_.spotlight.active);
	}
}

void MouseStyleStore::toggleSpotlight()
{
	MouseStyle next = style_;
	next.spotlight.active = !next.spotlight.active;
	setStyle(next);
}

void MouseStyleStore::save()
{
	saveTimer_.stop();
	writeVoltz("MouseStyle", mouseStyleToJson(style_));
}

std::vector<NamedMouseStyle> MouseStyleStore::savedStyles() const
{
	return savedStylesFromJson(readVoltz("MouseStyles"));
}

void MouseStyleStore::writeSaved(const std::vector<NamedMouseStyle> &styles)
{
	writeVoltz("MouseStyles", savedStylesToJson(styles));
	emit savedStylesChanged();
}

void MouseStyleStore::saveCurrentAs(const QString &name)
{
	writeSaved(upsertSavedStyle(savedStyles(), name.toStdString(), style_));
}

bool MouseStyleStore::applySaved(const QString &name)
{
	const std::string wanted = name.trimmed().toStdString();
	for (const NamedMouseStyle &item : savedStyles()) {
		if (item.name == wanted) {
			setStyle(item.style);
			blog(LOG_INFO, "[voltz] estilo do mouse aplicado: %s", item.name.c_str());
			return true;
		}
	}
	return false;
}

void MouseStyleStore::removeSaved(const QString &name)
{
	writeSaved(removeSavedStyle(savedStyles(), name.toStdString()));
}

QString MouseStyleStore::spotlightShortcutText() const
{
	return voltz::hotkeyShortcutText(spotlightHotkey_);
}

void MouseStyleStore::handleEvent(enum obs_frontend_event event, void *data)
{
	auto *self = static_cast<MouseStyleStore *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		self->loadHotkeyBinding();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		if (self->saveTimer_.isActive()) {
			self->save();
		}
		self->unregisterHotkey();
		/* Seguro durante o próprio evento: OBSStudioAPI::on_event percorre os callbacks de trás para frente */
		obs_frontend_remove_event_callback(&MouseStyleStore::handleEvent, self);
		self->registered_ = false;
		break;
	default:
		break;
	}
}

void MouseStyleStore::handleHotkey(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed) {
		return;
	}
	auto *self = static_cast<MouseStyleStore *>(data);
	/* Teclado: thread de atalhos do libobs; obs-websocket TriggerHotkeyByName: thread do websocket. Só enfileira. */
	QMetaObject::invokeMethod(self, [self]() { self->toggleSpotlight(); }, Qt::QueuedConnection);
}

void MouseStyleStore::loadHotkeyBinding()
{
	if (spotlightHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	/* perfil [Hotkeys] (formato do OBS); nada salvo: Ctrl+Shift+H (tabela do núcleo, hotkey-bindings) */
	voltz::loadHotkeyBinding(spotlightHotkey_, kSpotlightHotkeyName);
}

void MouseStyleStore::unregisterHotkey()
{
	if (spotlightHotkey_ == OBS_INVALID_HOTKEY_ID) {
		return;
	}
	obs_hotkey_unregister(spotlightHotkey_);
	spotlightHotkey_ = OBS_INVALID_HOTKEY_ID;
}

} // namespace voltz
