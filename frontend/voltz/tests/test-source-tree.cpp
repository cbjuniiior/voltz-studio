#include <doctest.h>

#include <voltz/core/source-tree.hpp>

#include <utility>
#include <vector>

using voltz::SourceNode;
using voltz::SourceRow;

namespace {

SourceNode item(int64_t id)
{
	SourceNode node;
	node.id = id;
	return node;
}

SourceNode group(int64_t id, bool collapsed, std::vector<SourceNode> children)
{
	SourceNode node;
	node.id = id;
	node.isGroup = true;
	node.collapsed = collapsed;
	node.children = std::move(children);
	return node;
}

} // namespace

TEST_CASE("flattenSourceTree: cena vazia não tem linhas")
{
	CHECK(voltz::flattenSourceTree({}).empty());
}

TEST_CASE("flattenSourceTree: camada de cima primeiro (o libobs enumera de baixo para cima)")
{
	const std::vector<SourceRow> rows = voltz::flattenSourceTree({item(1), item(2), item(3)});
	REQUIRE(rows.size() == 3);
	CHECK(rows[0].id == 3);
	CHECK(rows[1].id == 2);
	CHECK(rows[2].id == 1);
	for (const SourceRow &row : rows) {
		CHECK(row.groupId == -1);
		CHECK(row.depth == 0);
		CHECK_FALSE(row.isGroup);
		CHECK(row.siblingCount == 3);
	}
	CHECK(rows[0].siblingIndex == 0);
	CHECK(rows[2].siblingIndex == 2);
}

TEST_CASE("flattenSourceTree: filhos do grupo expandido logo abaixo dele, também de cima para baixo")
{
	/* cena (baixo → cima): 1, grupo 2 {10, 11}, 3 */
	const auto rows = voltz::flattenSourceTree({item(1), group(2, false, {item(10), item(11)}), item(3)});
	REQUIRE(rows.size() == 5);
	CHECK(rows[0].id == 3);
	CHECK(rows[1].id == 2);
	CHECK(rows[1].isGroup);
	CHECK_FALSE(rows[1].collapsed);
	CHECK(rows[2].id == 11);
	CHECK(rows[2].groupId == 2);
	CHECK(rows[2].depth == 1);
	CHECK(rows[2].siblingIndex == 0);
	CHECK(rows[2].siblingCount == 2);
	CHECK(rows[3].id == 10);
	CHECK(rows[3].siblingIndex == 1);
	CHECK(rows[4].id == 1);
	CHECK(rows[4].groupId == -1);
	CHECK(rows[4].siblingIndex == 2);
}

TEST_CASE("flattenSourceTree: grupo recolhido esconde os filhos")
{
	const auto rows = voltz::flattenSourceTree({group(5, true, {item(10), item(11)})});
	REQUIRE(rows.size() == 1);
	CHECK(rows[0].id == 5);
	CHECK(rows[0].collapsed);
}

TEST_CASE("canMoveUp/canMoveDown consideram só os irmãos")
{
	/* linhas: grupo 2 (topo da cena), 11 (topo do grupo), 10 (base do grupo), 1 (base da cena) */
	const auto rows = voltz::flattenSourceTree({item(1), group(2, false, {item(10), item(11)})});
	REQUIRE(rows.size() == 4);
	CHECK_FALSE(voltz::canMoveUp(rows[0]));
	CHECK(voltz::canMoveDown(rows[0]));
	CHECK_FALSE(voltz::canMoveUp(rows[1]));
	CHECK(voltz::canMoveDown(rows[1]));
	CHECK(voltz::canMoveUp(rows[2]));
	CHECK_FALSE(voltz::canMoveDown(rows[2]));
	CHECK(voltz::canMoveUp(rows[3]));
	CHECK_FALSE(voltz::canMoveDown(rows[3]));
}
