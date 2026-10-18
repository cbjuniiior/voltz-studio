#include <doctest.h>

#include <voltz/core/annotation-journal.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

using voltz::AnnotationTool;
using voltz::LiveStroke;
using voltz::StrokePoint;

namespace {

constexpr int64_t kMs = 1000000;
constexpr int64_t kT0 = 5000 * kMs;

LiveStroke makeLive(int id, AnnotationTool tool, int64_t startNs, int64_t endNs, std::vector<StrokePoint> points)
{
	LiveStroke live;
	live.stroke.id = id;
	live.stroke.tool = tool;
	live.stroke.points = std::move(points);
	live.startNs = startNs;
	live.endNs = endNs;
	return live;
}

} // namespace

TEST_CASE("AnnotationJournal: linha completa ao terminar, uma vez, em ms relativos ao t0")
{
	voltz::AnnotationJournal journal(kT0);
	const LiveStroke live =
		makeLive(7, AnnotationTool::Arrow, kT0 + 1200 * kMs, kT0 + 1850 * kMs, {{10, 20}, {30, 40}});
	CHECK(journal.strokeFinished(live) ==
	      R"({"id":7,"ferr":"seta","cor":"#D4F53C","esp":4,"pts":[[10,20],[30,40]],"t_ini":1200,"t_fim":1850})");
	CHECK(journal.strokeFinished(live).empty());
	CHECK(journal.strokeCount() == 1);
	CHECK(journal.strokeFinished(makeLive(8, AnnotationTool::Pen, kT0, -1, {{1, 1}})).empty());
}

TEST_CASE("AnnotationJournal: sair da tela vira linha curta; traço ainda sem linha vira linha completa")
{
	voltz::AnnotationJournal journal(kT0);
	const LiveStroke done = makeLive(1, AnnotationTool::Line, kT0 + 100 * kMs, kT0 + 200 * kMs, {{0, 0}, {5, 5}});
	journal.strokeFinished(done);
	CHECK(journal.strokeRemoved(voltz::RemovedStroke{done, kT0 + 9000 * kMs}) == R"({"id":1,"t_limpo":9000})");
	const LiveStroke open = makeLive(2, AnnotationTool::Pen, kT0 + 300 * kMs, -1, {{1, 1}, {2, 2}});
	CHECK(journal.strokeRemoved(voltz::RemovedStroke{open, kT0 + 400 * kMs}) ==
	      R"({"id":2,"ferr":"caneta","cor":"#D4F53C","esp":4,"pts":[[1,1],[2,2]],"t_ini":300,"t_fim":400,"t_limpo":400})");
	CHECK(journal.strokeCount() == 2);
}

TEST_CASE("AnnotationJournal: traços de antes do início valem 0; em andamento no fim fecham no fim")
{
	voltz::AnnotationJournal journal(kT0);
	const std::vector<LiveStroke> visible = {makeLive(1, AnnotationTool::Rectangle, kT0 - 3000 * kMs,
							  kT0 - 2000 * kMs, {{0, 0}, {9, 9}}),
						 makeLive(2, AnnotationTool::Pen, kT0 - 500 * kMs, -1, {{4, 4}})};
	const std::vector<std::string> initial = journal.initialLines(visible);
	REQUIRE(initial.size() == 1);
	CHECK(initial[0] ==
	      R"({"id":1,"ferr":"retangulo","cor":"#D4F53C","esp":4,"pts":[[0,0],[9,9]],"t_ini":0,"t_fim":0})");
	const std::vector<std::string> closing = journal.finishOpen(visible, kT0 + 7000 * kMs);
	REQUIRE(closing.size() == 1);
	CHECK(closing[0] == R"({"id":2,"ferr":"caneta","cor":"#D4F53C","esp":4,"pts":[[4,4]],"t_ini":0,"t_fim":7000})");
	CHECK(journal.finishOpen(visible, kT0 + 8000 * kMs).empty());

	/* texto em branco (nunca apareceu): sem linha ao limpar nem no fim da gravação */
	LiveStroke blank = makeLive(3, AnnotationTool::Text, kT0 + 100 * kMs, -1, {{4, 4}});
	blank.stroke.text = " \n";
	CHECK(journal.strokeRemoved(voltz::RemovedStroke{blank, kT0 + 200 * kMs}).empty());
	CHECK(journal.finishOpen({blank}, kT0 + 300 * kMs).empty());
	blank.endNs = kT0 + 150 * kMs;
	CHECK(journal.strokeFinished(blank).empty());
	CHECK(journal.initialLines({blank}).empty());
	CHECK(journal.strokeCount() == 2);
}

TEST_CASE("consolidateAnnotations: junta t_limpo, mantém ordem e campos desconhecidos, ignora o resto")
{
	const std::string journal =
		"{\"id\":1,\"ferr\":\"seta\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[1,2]],\"t_ini\":0,\"t_fim\":5,\"extra\":true}\n"
		"lixo\n"
		"{\"id\":2,\"ferr\":\"linha\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[3,4]],\"t_ini\":6,\"t_fim\":7}\n"
		"{\"id\":1,\"t_limpo\":90}\n"
		"{\"id\":5,\"t_limpo\":99}\n"
		"{\"t_limpo\":1}\n";
	CHECK(voltz::consolidateAnnotations(journal) ==
	      "{\"id\":1,\"ferr\":\"seta\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[1,2]],\"t_ini\":0,\"t_fim\":5,\"extra\":true,\"t_limpo\":90}\n"
	      "{\"id\":2,\"ferr\":\"linha\",\"cor\":\"#D4F53C\",\"esp\":4,\"pts\":[[3,4]],\"t_ini\":6,\"t_fim\":7}\n");
}

TEST_CASE("consolidateAnnotations: idempotente, aceita CRLF e última linha sem quebra")
{
	const std::string journal = "{\"id\":3,\"ferr\":\"caneta\",\"pts\":[]}\r\n{\"id\":3,\"t_limpo\":12}";
	const std::string once = voltz::consolidateAnnotations(journal);
	CHECK(once == "{\"id\":3,\"ferr\":\"caneta\",\"pts\":[],\"t_limpo\":12}\n");
	CHECK(voltz::consolidateAnnotations(once) == once);
	CHECK(voltz::consolidateAnnotations("").empty());
}
