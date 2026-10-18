#pragma once

#include <voltz/core/hotkey-bindings.hpp>

#include <QFrame>
#include <QList>

#include <vector>

class QLabel;
class QToolButton;

namespace voltz {

class VoltzHotkeyEdit;

// Ajustes › Atalhos (spec §4.3, Plano 4D): os atalhos do Voltz registrados (desenhar, desfazer, limpar, marcador,
// holofote e, com VOLTZ_DEBUG_FREEZE_UI, o de depuração), cada um com o campo de teclas do OBS (OBSHotkeyEdit,
// reaproveitado sem patch), "Limpar" e "Restaurar padrão". Grava na hora (perfil [Hotkeys], o mesmo das Configurações do
// OBS, que continuam editando os mesmos atalhos no modo clássico) só combinações aceitas pelo checkVoltzHotkeyCombo do
// núcleo, trocando a primeira tecla do atalho (as outras, vindas do modo clássico, ficam); mostra os conflitos com
// qualquer outro atalho. Enquanto o campo está ativo os atalhos do OBS ficam pausados (senão apertar Ctrl+Shift+D para
// gravá-lo ligaria o modo desenho). Os rótulos ficam em formLabels() para o SettingsPage alinhar as colunas. Thread da UI.
class HotkeySettingsSection : public QFrame {
public:
	explicit HotkeySettingsSection(QWidget *parent = nullptr);

	const QList<QLabel *> &formLabels() const { return formLabels_; }
	// Relê os atalhos (podem ter mudado nas Configurações do OBS ou ao trocar de perfil).
	void reload();

private:
	struct Row {
		const char *name = nullptr;
		QString title;
		VoltzHotkeyEdit *edit = nullptr;
		QToolButton *clear = nullptr;
		QToolButton *reset = nullptr;
		QLabel *status = nullptr;
	};

	// O campo mudou (a pessoa apertou teclas): grava se a combinação vale; senão explica.
	void keyChanged(size_t index);
	void save(size_t index, const std::vector<KeyCombo> &bindings);
	// Campo, conflitos e "+N" a partir do atalho de agora.
	void refreshRow(size_t index);

	std::vector<Row> rows_;
	QList<QLabel *> formLabels_;
	bool loading_ = false;
};

} // namespace voltz
