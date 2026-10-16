#pragma once

#include <voltz/core/mouse-style.hpp>

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QObject>
#include <QString>
#include <QTimer>

#include <vector>

namespace voltz {

// Estilo do destaque do mouse (spec §6.1): o atual em user.ini [Voltz] MouseStyle e os salvos em [Voltz] MouseStyles
// (JSON do núcleo, mesmo formato do projeto.voltz → mouse). Avisa quem desenha ou registra (LiveMouse → plugin,
// Ajustes › Mouse, ProjectRecorder) e registra o atalho do OBS "VoltzStudio.ToggleSpotlight" (padrão Ctrl+Shift+H).
// Thread da UI.
class MouseStyleStore : public QObject {
	Q_OBJECT

public:
	explicit MouseStyleStore(QObject *parent = nullptr);
	~MouseStyleStore() override;

	const MouseStyle &style() const { return style_; }
	// Limita, aplica na hora (styleChanged; spotlightToggled se o holofote mudou) e grava no user.ini 400 ms depois
	// da última mudança (arrastar um slider não regrava o arquivo a cada passo).
	void setStyle(const MouseStyle &style);
	void toggleSpotlight();

	std::vector<NamedMouseStyle> savedStyles() const;
	void saveCurrentAs(const QString &name);
	bool applySaved(const QString &name);
	void removeSaved(const QString &name);

	// Teclas do atalho do holofote em texto (ex. "Ctrl + Shift + H"); vazio sem atalho.
	QString spotlightShortcutText() const;

signals:
	void styleChanged();
	void spotlightToggled(bool active);
	void savedStylesChanged();

private:
	static void handleEvent(enum obs_frontend_event event, void *data);
	static void handleHotkey(void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed);
	void loadHotkeyBinding();
	void unregisterHotkey();
	void save();
	void writeSaved(const std::vector<NamedMouseStyle> &styles);

	MouseStyle style_;
	QTimer saveTimer_;
	obs_hotkey_id spotlightHotkey_ = OBS_INVALID_HOTKEY_ID;
	bool registered_ = true;
};

} // namespace voltz
