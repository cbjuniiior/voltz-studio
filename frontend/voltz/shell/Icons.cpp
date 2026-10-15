#include "Icons.hpp"

#include <voltz/core/svg-recolor.hpp>

#include <obs.h>

#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace voltz {

namespace {

constexpr int kSizes[] = {16, 20, 24, 32, 40, 48};

QString resourcePath(const QString &name, IconWeight weight)
{
	return weight == IconWeight::Fill ? QStringLiteral(":/voltz/icons/fill/%1-fill.svg").arg(name)
					  : QStringLiteral(":/voltz/icons/regular/%1.svg").arg(name);
}

QByteArray recoloredSvg(const QString &name, const QColor &color, IconWeight weight)
{
	QFile file(resourcePath(name, weight));
	if (!file.open(QIODevice::ReadOnly)) {
		blog(LOG_WARNING, "[voltz] icone ausente: %s", qUtf8Printable(file.fileName()));
		return {};
	}
	const std::string svg = file.readAll().toStdString();
	return QByteArray::fromStdString(recolorSvg(svg, color.name(QColor::HexRgb).toStdString()));
}

void addPixmaps(QIcon &icon, const QByteArray &svg, QIcon::State state)
{
	if (svg.isEmpty()) {
		return;
	}
	QSvgRenderer renderer(svg);
	for (int size : kSizes) {
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter painter(&pixmap);
		renderer.render(&painter);
		painter.end();
		icon.addPixmap(pixmap, QIcon::Normal, state);
	}
}

} // namespace

QIcon phosphorIcon(const QString &name, const QColor &color, IconWeight weight)
{
	static QHash<QString, QIcon> cache;
	const QString key = QStringLiteral("%1|%2|%3").arg(name, color.name()).arg(int(weight));
	auto it = cache.find(key);
	if (it != cache.end()) {
		return it.value();
	}
	QIcon icon;
	addPixmaps(icon, recoloredSvg(name, color, weight), QIcon::Off);
	cache.insert(key, icon);
	return icon;
}

QIcon phosphorToggleIcon(const QString &name, const QColor &off, const QColor &on)
{
	QIcon icon;
	addPixmaps(icon, recoloredSvg(name, off, IconWeight::Regular), QIcon::Off);
	addPixmaps(icon, recoloredSvg(name, on, IconWeight::Fill), QIcon::On);
	return icon;
}

void loadShellFonts()
{
	static bool loaded = false;
	if (loaded) {
		return;
	}
	loaded = true;
	const char *files[] = {":/voltz/fonts/DMSans-Regular.ttf", ":/voltz/fonts/DMSans-Medium.ttf",
			       ":/voltz/fonts/DMSans-SemiBold.ttf", ":/voltz/fonts/DMSans-Bold.ttf"};
	for (const char *file : files) {
		if (QFontDatabase::addApplicationFont(QString::fromLatin1(file)) < 0) {
			blog(LOG_WARNING, "[voltz] falha ao carregar fonte %s", file);
		}
	}
}

} // namespace voltz
