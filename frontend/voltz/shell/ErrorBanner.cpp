#include "ErrorBanner.hpp"

#include "Icons.hpp"

#include <OBSApp.hpp> // QTStr

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_ErrorBanner.cpp"

namespace voltz {

ErrorBanner::ErrorBanner(QWidget *parent) : QFrame(parent)
{
	setObjectName("errorBanner");

	auto *icon = new QLabel(this);
	icon->setPixmap(phosphorIcon(QStringLiteral("warning-circle"), palette::kRec, IconWeight::Fill).pixmap(20, 20));
	icon->setAlignment(Qt::AlignTop);

	title_ = new QLabel(this);
	title_->setObjectName("errorTitle");
	message_ = new QLabel(this);
	message_->setWordWrap(true);
	message_->setTextFormat(Qt::RichText);

	auto *texts = new QVBoxLayout();
	texts->setSpacing(2);
	texts->addWidget(title_);
	texts->addWidget(message_);

	auto *close = new QToolButton(this);
	close->setObjectName("iconButton");
	close->setIcon(phosphorIcon(QStringLiteral("x"), palette::kMuted));
	close->setIconSize(QSize(16, 16));
	setButtonHint(close, QTStr("Voltz.Error.Dismiss"));
	connect(close, &QToolButton::clicked, this, &QWidget::hide);

	auto *layout = new QHBoxLayout(this);
	layout->setContentsMargins(12, 10, 8, 10);
	layout->setSpacing(10);
	layout->addWidget(icon);
	layout->addLayout(texts, 1);
	layout->addWidget(close, 0, Qt::AlignTop);

	hide();
}

void ErrorBanner::showError(const QString &title, const QString &message)
{
	title_->setText(title);
	message_->setText(message);
	show();
}

} // namespace voltz
