#include "Icons.hpp"

#include <voltz/core/svg-recolor.hpp>

#include <obs.h>

#include <QFile>
#include <QFontDatabase>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPaintDevice>
#include <QPixmap>
#include <QSvgRenderer>

#include <cmath>
#include <utility>

namespace voltz {

namespace {

constexpr qreal kDisabledOpacity = 0.4;

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

// Desenha o SVG recolorido no tamanho físico pedido (tamanho lógico × escala do monitor), então o
// ícone fica nítido em qualquer DPI. Off = SVG normal; On = SVG do estado ligado (se houver).
// Modo Disabled sai com opacidade reduzida.
class PhosphorIconEngine final : public QIconEngine {
public:
	PhosphorIconEngine(QByteArray off, QByteArray on) : off_(std::move(off)), on_(std::move(on)) {}

	void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
	{
		const qreal scale = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
		painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, scale));
	}

	QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
	{
		return scaledPixmap(size, mode, state, 1.0);
	}

	QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
	{
		if (size.isEmpty() || isNull()) {
			return QPixmap();
		}
		if (scale <= 0) {
			scale = 1.0;
		}
		const CacheKey key{size.width(), size.height(), qRound(scale * 1000), mode == QIcon::Disabled,
				   state == QIcon::On && !on_.isEmpty()};
		auto it = cache_.constFind(key);
		if (it != cache_.constEnd()) {
			return it.value();
		}

		const QSize physical(int(std::ceil(size.width() * scale)), int(std::ceil(size.height() * scale)));
		QPixmap pixmap(physical);
		pixmap.fill(Qt::transparent);
		QSvgRenderer renderer(key.on ? on_ : off_);
		QPainter painter(&pixmap);
		painter.setRenderHint(QPainter::Antialiasing);
		if (key.disabled) {
			painter.setOpacity(kDisabledOpacity);
		}
		renderer.render(&painter, QRectF(QPointF(0, 0), QSizeF(physical)));
		painter.end();
		pixmap.setDevicePixelRatio(scale);

		cache_.insert(key, pixmap);
		return pixmap;
	}

	QIconEngine *clone() const override { return new PhosphorIconEngine(off_, on_); }

	QString key() const override { return QStringLiteral("voltz-phosphor"); }

	bool isNull() override { return off_.isEmpty(); }

private:
	struct CacheKey {
		int width;
		int height;
		int scaleMilli; /* escala × 1000: chave exata, sem comparar ponto flutuante */
		bool disabled;
		bool on;

		bool operator==(const CacheKey &other) const
		{
			return width == other.width && height == other.height && scaleMilli == other.scaleMilli &&
			       disabled == other.disabled && on == other.on;
		}

		friend size_t qHash(const CacheKey &key, size_t seed = 0)
		{
			return qHashMulti(seed, key.width, key.height, key.scaleMilli, key.disabled, key.on);
		}
	};

	QByteArray off_;
	QByteArray on_;
	QHash<CacheKey, QPixmap> cache_;
};

} // namespace

QIcon phosphorIcon(const QString &name, const QColor &color, IconWeight weight)
{
	/* Vazamento proposital: QIcon/QPixmap não podem ser destruídos depois do QApplication. */
	static auto *cache = new QHash<QString, QIcon>();
	const QString key = QStringLiteral("%1|%2|%3").arg(name, color.name()).arg(int(weight));
	auto it = cache->constFind(key);
	if (it != cache->constEnd()) {
		return it.value();
	}
	const QIcon icon(new PhosphorIconEngine(recoloredSvg(name, color, weight), QByteArray()));
	cache->insert(key, icon);
	return icon;
}

QIcon phosphorToggleIcon(const QString &name, const QColor &off, const QColor &on)
{
	return QIcon(new PhosphorIconEngine(recoloredSvg(name, off, IconWeight::Regular),
					    recoloredSvg(name, on, IconWeight::Fill)));
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
