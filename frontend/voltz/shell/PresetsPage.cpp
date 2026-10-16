#include "PresetsPage.hpp"

#include "PresetThumbnail.hpp"
#include "RecordingConfig.hpp"

#include <OBSApp.hpp>

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "moc_PresetsPage.cpp"

namespace voltz {

bool PresetsPage::isFirstRun()
{
	config_t *config = App()->GetUserConfig();
	config_set_default_bool(config, "Voltz", "PresetsDone", false);
	return !config_get_bool(config, "Voltz", "PresetsDone");
}

PresetsPage::PresetsPage(QWidget *parent) : QWidget(parent)
{
	auto *heading = new QLabel(QTStr("Voltz.Presets.Title"), this);
	heading->setObjectName("pageTitle");
	auto *subtitle = new QLabel(QTStr("Voltz.Presets.Subtitle"), this);
	subtitle->setObjectName("muted");

	welcome_ = new QFrame(this);
	welcome_->setObjectName("welcomeBanner");
	auto *welcomeText = new QLabel(QTStr("Voltz.Presets.FirstRun"), welcome_);
	welcomeText->setWordWrap(true);
	auto *skip = new QPushButton(QTStr("Voltz.Presets.Skip"), welcome_);
	auto *welcomeLayout = new QHBoxLayout(welcome_);
	welcomeLayout->setContentsMargins(14, 10, 10, 10);
	welcomeLayout->addWidget(welcomeText, 1);
	welcomeLayout->addWidget(skip);
	welcome_->setVisible(isFirstRun());
	connect(skip, &QPushButton::clicked, this, [this]() {
		RecordingConfig::applyVoltzDefaults();
		markFirstRunDone();
		emit firstRunDismissed();
	});

	auto *grid = new QGridLayout();
	grid->setSpacing(16);
	grid->addWidget(buildCard(PresetId::Tutorial, "Voltz.Preset.Tutorial.Name", "Voltz.Preset.Tutorial.Desc"), 0,
			0);
	grid->addWidget(buildCard(PresetId::ScreenCamera, "Voltz.Preset.ScreenCamera.Name",
				  "Voltz.Preset.ScreenCamera.Desc"),
			0, 1);
	grid->addWidget(buildCard(PresetId::SideBySide, "Voltz.Preset.SideBySide.Name", "Voltz.Preset.SideBySide.Desc"),
			1, 0);
	grid->addWidget(buildCard(PresetId::Opening, "Voltz.Preset.Opening.Name", "Voltz.Preset.Opening.Desc"), 1, 1);

	auto *content = new QWidget(this);
	auto *contentLayout = new QVBoxLayout(content);
	contentLayout->setContentsMargins(32, 28, 32, 28);
	contentLayout->setSpacing(10);
	contentLayout->addWidget(heading);
	contentLayout->addWidget(subtitle);
	contentLayout->addWidget(welcome_);
	contentLayout->addSpacing(8);
	contentLayout->addLayout(grid);
	contentLayout->addStretch(1);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);

	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->addWidget(scroll);
}

QWidget *PresetsPage::buildCard(PresetId preset, const QString &nameKey, const QString &descKey)
{
	auto *card = new QFrame(this);
	card->setObjectName("presetCard");

	auto *name = new QLabel(QTStr(nameKey.toUtf8().constData()), card);
	name->setObjectName("cardTitle");
	auto *desc = new QLabel(QTStr(descKey.toUtf8().constData()), card);
	desc->setObjectName("muted");
	desc->setWordWrap(true);
	auto *applyButton = new QPushButton(QTStr("Voltz.Presets.Apply"), card);
	applyButton->setObjectName("primaryButton");
	applyButton->setAccessibleName(QTStr(nameKey.toUtf8().constData()) + QStringLiteral(" — ") +
				       QTStr("Voltz.Presets.Apply"));

	auto *layout = new QVBoxLayout(card);
	layout->setContentsMargins(14, 14, 14, 14);
	layout->setSpacing(8);
	layout->addWidget(new PresetThumbnail(preset, card));
	layout->addWidget(name);
	layout->addWidget(desc, 1);
	layout->addWidget(applyButton, 0, Qt::AlignLeft);

	connect(applyButton, &QPushButton::clicked, this, [this, preset]() { this->apply(preset); });
	return card;
}

void PresetsPage::apply(PresetId preset)
{
	RecordingConfig::applyVoltzDefaults();
	OBSSource scene = applyPreset(preset);
	if (scene) {
		obs_frontend_set_current_scene(scene);
	}
	markFirstRunDone();
	emit presetApplied();
}

void PresetsPage::markFirstRunDone()
{
	config_t *config = App()->GetUserConfig();
	config_set_bool(config, "Voltz", "PresetsDone", true);
	config_save_safe(config, "tmp", nullptr);
	welcome_->hide();
}

} // namespace voltz
