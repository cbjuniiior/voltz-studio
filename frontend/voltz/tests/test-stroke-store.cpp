#include <doctest.h>

#include <voltz/core/stroke-store.hpp>

#include <cstdint>
#include <vector>

using voltz::AnnotationTool;
using voltz::RgbColor;
using voltz::StrokePoint;
using voltz::StrokeStore;

namespace {
constexpr int64_t kMs = 1000000;
const RgbColor kLime{0xD4, 0xF5, 0x3C};
} // namespace

TEST_CASE("StrokeStore: começar, acrescentar e terminar; ids crescentes")
{
	StrokeStore store;
	const int first = store.begin(AnnotationTool::Pen, kLime, 99, StrokePoint{1, 1}, 100 * kMs);
	CHECK(first == 1);
	CHECK(store.openId() == 1);
	CHECK(store.extend({{2, 2}, {3, 3}}));
	const voltz::LiveStroke *ended = store.end(150 * kMs);
	REQUIRE(ended != nullptr);
	CHECK(ended->stroke.id == 1);
	CHECK(ended->stroke.width == voltz::kStrokeWidthMax);
	CHECK(ended->stroke.points.size() == 3);
	CHECK(ended->startNs == 100 * kMs);
	CHECK(ended->endNs == 150 * kMs);
	CHECK(store.openId() == 0);
	CHECK(store.end(160 * kMs) == nullptr);
	CHECK(store.begin(AnnotationTool::Arrow, kLime, 4, StrokePoint{0, 0}, 200 * kMs) == 2);
	REQUIRE(store.visible().size() == 2);
	CHECK(store.strokes()[1].tool == AnnotationTool::Arrow);
}

TEST_CASE("StrokeStore: só um traço em andamento; acrescentar sem traço falha")
{
	StrokeStore store;
	CHECK_FALSE(store.extend({{1, 1}}));
	CHECK(store.begin(AnnotationTool::Line, kLime, 4, StrokePoint{0, 0}, 0) == 1);
	CHECK(store.begin(AnnotationTool::Line, kLime, 4, StrokePoint{5, 5}, 1) == 0);
	CHECK_FALSE(store.extend({}));
	CHECK(store.extend({{7, 7}, {9, 9}}));
	REQUIRE(store.visible().size() == 1);
	CHECK(store.visible()[0].stroke.points.size() == 2);
	CHECK(store.visible()[0].stroke.points[1] == StrokePoint{9, 9});
}

TEST_CASE("StrokeStore: desfazer tira o mais recente, mesmo em andamento")
{
	StrokeStore store;
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 0);
	store.end(10 * kMs);
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{1, 1}, 20 * kMs);
	const std::vector<voltz::RemovedStroke> removed = store.undo(30 * kMs);
	REQUIRE(removed.size() == 1);
	CHECK(removed[0].live.stroke.id == 2);
	CHECK(removed[0].live.open());
	CHECK(removed[0].removedNs == 30 * kMs);
	CHECK(store.openId() == 0);
	CHECK(store.undo(40 * kMs).size() == 1);
	CHECK(store.undo(50 * kMs).empty());
}

TEST_CASE("StrokeStore: limpar tira todos na ordem")
{
	StrokeStore store;
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 0);
	store.end(1);
	store.begin(AnnotationTool::Ellipse, kLime, 4, StrokePoint{0, 0}, 2);
	const std::vector<voltz::RemovedStroke> removed = store.clear(9 * kMs);
	REQUIRE(removed.size() == 2);
	CHECK(removed[0].live.stroke.id == 1);
	CHECK(removed[1].live.stroke.id == 2);
	CHECK(removed[1].removedNs == 9 * kMs);
	CHECK(store.visible().empty());
	CHECK(store.clear(10 * kMs).empty());
	CHECK(store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 11) == 3);
}

TEST_CASE("StrokeStore: somem após N s no instante exato; em andamento não some")
{
	const int64_t lifetime = 1000 * kMs;
	StrokeStore store;
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 0);
	store.end(100 * kMs);
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 200 * kMs);
	store.end(300 * kMs);
	store.begin(AnnotationTool::Pen, kLime, 4, StrokePoint{0, 0}, 400 * kMs);
	CHECK(store.nextExpiryNs(lifetime) == 1100 * kMs);
	CHECK(store.nextExpiryNs(0) == -1);
	CHECK(store.expire(5000 * kMs, 0).empty());
	const std::vector<voltz::RemovedStroke> removed = store.expire(1200 * kMs, lifetime);
	REQUIRE(removed.size() == 1);
	CHECK(removed[0].live.stroke.id == 1);
	CHECK(removed[0].removedNs == 1100 * kMs);
	CHECK(store.nextExpiryNs(lifetime) == 1300 * kMs);
	CHECK(store.expire(9000 * kMs, lifetime).size() == 1);
	REQUIRE(store.visible().size() == 1);
	CHECK(store.visible()[0].open());
	CHECK(store.nextExpiryNs(lifetime) == -1);
}
