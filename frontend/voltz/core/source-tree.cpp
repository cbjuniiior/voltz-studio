#include "source-tree.hpp"

namespace voltz {

namespace {

void appendLevel(const std::vector<SourceNode> &bottomToTop, int64_t groupId, int depth, std::vector<SourceRow> &out)
{
	const int count = static_cast<int>(bottomToTop.size());
	for (int i = count - 1; i >= 0; i--) {
		const SourceNode &node = bottomToTop[static_cast<size_t>(i)];
		SourceRow row;
		row.id = node.id;
		row.groupId = groupId;
		row.depth = depth;
		row.isGroup = node.isGroup;
		row.collapsed = node.collapsed;
		row.siblingIndex = count - 1 - i;
		row.siblingCount = count;
		out.push_back(row);
		if (node.isGroup && !node.collapsed) {
			appendLevel(node.children, node.id, depth + 1, out);
		}
	}
}

} // namespace

std::vector<SourceRow> flattenSourceTree(const std::vector<SourceNode> &bottomToTop)
{
	std::vector<SourceRow> rows;
	appendLevel(bottomToTop, -1, 0, rows);
	return rows;
}

} // namespace voltz
