#include "HotkeySettings.hpp"

#include "HotkeyBinding.hpp"
#include "Icons.hpp"

#include <OBSApp.hpp> // App(), QTStr
#include <settings/OBSHotkeyEdit.hpp>

#include <QFocusEvent>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <string>
#include <utility>

namespace voltz {

// O campo de teclas do OBS (o mesmo das Configurações → Atalhos), sem OBSBasicSettings: `settings` nulo só seria usado
// pelo aviso de atalho duplicado do OBS (UpdateDuplicationState), que o Voltz nunca chama. Enquanto ele tem o foco, os
// atalhos do OBS ficam pausados (OBSApp::DisableHotkeys, como a janela de adicionar fonte do OBS faz) e voltam ao sair
// (UpdateHotkeyFocusSetting) — ou ao ser destruído.
class VoltzHotkeyEdit : public OBSHotkeyEdit {
public:
	VoltzHotkeyEdit(QWidget *parent, obs_key_combination_t original) : OBSHotkeyEdit(parent, original, nullptr)
	{
		setObjectName("hotkeyEdit");
		/* só o clique dá o foco (nunca o Tab nem a troca de página): pausar os atalhos é sempre um gesto explícito */
		setFocusPolicy(Qt::ClickFocus);
	}

	~VoltzHotkeyEdit() override { resume(); }

	std::function<void()> focusLeft;

protected:
	void focusInEvent(QFocusEvent *event) override
	{
		OBSHotkeyEdit::focusInEvent(event);
		if (!paused_) {
			paused_ = true;
			App()->DisableHotkeys();
			blog(LOG_INFO, "[voltz] atalhos: editando (atalhos pausados)");
		}
	}

	void focusOutEvent(QFocusEvent *event) override
	{
		resume();
		OBSHotkeyEdit::focusOutEvent(event);
		if (focusLeft) {
			focusLeft();
		}
	}

private:
	void resume()
	{
		if (paused_) {
			paused_ = false;
			App()->UpdateHotkeyFocusSetting();
			blog(LOG_INFO, "[voltz] atalhos: edicao terminada (atalhos de volta)");
		}
	}

	bool paused_ = false;
};

namespace {

// "Voltz Studio: ligar/desligar modo desenho" → "Ligar/desligar modo desenho"
QString shortTitle(const QString &description)
{
	QString title = description.contains(QStringLiteral(": ")) ? description.section(QStringLiteral(": "), 1)
								   : description;
	if (!title.isEmpty()) {
		title[0] = title[0].toUpper();
	}
	return title;
}

void setWarn(QLabel *label, bool warn)
{
	if (label->property("warn").toBool() != warn) {
		label->setProperty("warn", warn);
		label->style()->unpolish(label);
		label->style()->polish(label);
	}
}

} // namespace

HotkeySettingsSection::HotkeySettingsSection(QWidget *parent) : QFrame(parent)
{
	setObjectName("panel");
	setFocusPolicy(Qt::ClickFocus); /* ponto de repouso do foco: clicar no fundo tira o foco do campo de teclas */
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(16, 14, 16, 16);
	layout->setSpacing(10);
	auto *title = new QLabel(QTStr("Voltz.Settings.Hotkeys").toUpper(), this);
	title->setObjectName("panelTitle");
	layout->addWidget(title);
	auto *hint = new QLabel(QTStr("Voltz.Hotkeys.Hint"), this);
	hint->setObjectName("muted");
	hint->setWordWrap(true);
	layout->addWidget(hint);

	auto *grid = new QGridLayout();
	grid->setContentsMargins(0, 0, 0, 0);
	grid->setHorizontalSpacing(16);
	grid->setVerticalSpacing(4);
	grid->setColumnStretch(1, 1);
	int gridRow = 0;
	/* só os registrados: o de depuração existe só com VOLTZ_DEBUG_FREEZE_UI */
	rows_.reserve(voltzHotkeys().size());
	for (const VoltzHotkey &hotkey : voltzHotkeys()) {
		if (findHotkey(hotkey.name) == OBS_INVALID_HOTKEY_ID) {
			continue;
		}
		const size_t index = rows_.size();
		Row row;
		row.name = hotkey.name;
		row.title = shortTitle(QTStr(hotkey.localeKey));
		auto *label = new QLabel(row.title, this);
		formLabels_.append(label);
		row.edit = new VoltzHotkeyEdit(this, obs_key_combination_t{0, OBS_KEY_NONE});
		row.edit->setAccessibleName(QTStr("Voltz.Hotkeys.Edit") + QStringLiteral(" \u2014 ") + row.title);
		row.clear = new QToolButton(this);
		row.clear->setObjectName("iconButton");
		row.clear->setIcon(phosphorIcon(QStringLiteral("x"), palette::kText));
		setButtonHint(row.clear, QTStr("Voltz.Hotkeys.Clear"), row.title);
		row.reset = new QToolButton(this);
		row.reset->setObjectName("iconButton");
		row.reset->setIcon(phosphorIcon(QStringLiteral("arrow-counter-clockwise"), palette::kText));
		setButtonHint(row.reset, QTStr("Voltz.Hotkeys.Default"), row.title);
		row.status = new QLabel(this);
		row.status->setObjectName("hotkeyStatus");
		row.status->setWordWrap(true);
		row.status->hide();
		grid->addWidget(label, gridRow, 0, Qt::AlignLeft | Qt::AlignVCenter);
		grid->addWidget(row.edit, gridRow, 1);
		grid->addWidget(row.clear, gridRow, 2);
		grid->addWidget(row.reset, gridRow, 3);
		gridRow++;
		grid->addWidget(row.status, gridRow, 1, 1, 3);
		gridRow++;

		connect(row.edit, &OBSHotkeyEdit::KeyChanged, this, [this, index]() { keyChanged(index); });
		connect(row.clear, &QToolButton::clicked, this, [this, index]() {
			save(index,
			     replaceFirstBinding(currentHotkeyBindings(findHotkey(rows_[index].name)), KeyCombo{}));
		});
		connect(row.reset, &QToolButton::clicked, this,
			[this, index]() { save(index, defaultHotkeyBindings(rows_[index].name)); });
		row.edit->focusLeft = [this, index]() {
			refreshRow(index);
		}; /* combinação incompleta: volta à salva */
		rows_.push_back(row);
	}
	layout->addLayout(grid);
	reload();
}

void HotkeySettingsSection::reload()
{
	for (size_t i = 0; i < rows_.size(); i++) {
		refreshRow(i);
	}
}

void HotkeySettingsSection::refreshRow(size_t index)
{
	Row &row = rows_[index];
	const std::vector<KeyCombo> bindings = currentHotkeyBindings(findHotkey(row.name));
	loading_ = true;
	row.edit->original = bindings.empty() ? obs_key_combination_t{0, OBS_KEY_NONE}
					      : toObsCombination(bindings.front());
	row.edit->ResetKey();
	loading_ = false;
	/* Antes de desabilitar o botão com o foco, o foco vai para a seção: senão o Qt o passa ao próximo botão da ordem de
	 * Tab (de outra linha) e um segundo Espaço limparia ou restauraria outro atalho. */
	const auto setEnabledKeepingFocus = [this](QToolButton *button, bool enabled) {
		if (!enabled && button->hasFocus()) {
			setFocus(Qt::OtherFocusReason);
		}
		button->setEnabled(enabled);
	};
	setEnabledKeepingFocus(row.clear, !bindings.empty());
	setEnabledKeepingFocus(row.reset, !(bindings == defaultHotkeyBindings(row.name)));

	QStringList notes;
	if (bindings.size() > 1) {
		notes.append(QTStr("Voltz.Hotkeys.More").arg(static_cast<int>(bindings.size() - 1)));
	}
	QStringList others;
	for (const std::string &name : hotkeyConflicts(row.name, bindings, allHotkeyBindings())) {
		const QString description = hotkeyDescription(name.c_str());
		others.append(description.isEmpty() ? QString::fromStdString(name) : description);
	}
	if (!others.isEmpty()) {
		notes.append(QTStr("Voltz.Hotkeys.Conflict").arg(others.join(QStringLiteral(", "))));
	}
	row.status->setText(notes.join(QStringLiteral(" ")));
	setWarn(row.status, !others.isEmpty());
	row.status->setVisible(!notes.isEmpty());
}

void HotkeySettingsSection::keyChanged(size_t index)
{
	if (loading_) {
		return;
	}
	Row &row = rows_[index];
	const KeyCombo combo = fromObsCombination(row.edit->key);
	switch (checkVoltzHotkeyCombo(combo)) {
	case HotkeyComboCheck::Ok:
		save(index, replaceFirstBinding(currentHotkeyBindings(findHotkey(row.name)), combo));
		return;
	case HotkeyComboCheck::NeedsKey:
		row.status->setText(QTStr("Voltz.Hotkeys.NeedsKey"));
		break;
	case HotkeyComboCheck::NeedsModifier:
		row.status->setText(QTStr("Voltz.Hotkeys.NeedsModifier"));
		break;
	case HotkeyComboCheck::Empty:
		return;
	}
	/* não grava: a pessoa continua apertando teclas ou sai do campo (que volta ao atalho salvo) */
	setWarn(row.status, true);
	row.status->setVisible(true);
}

void HotkeySettingsSection::save(size_t index, const std::vector<KeyCombo> &bindings)
{
	const char *name = rows_[index].name;
	saveHotkeyBinding(findHotkey(name), name, bindings);
	reload(); /* os conflitos das outras linhas podem ter mudado */
}

} // namespace voltz
