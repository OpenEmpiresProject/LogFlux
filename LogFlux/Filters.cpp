#include "Filters.h"
#include <QtGlobal>

bool LiteralFilter::matches(const QString& line) const
{
	// Case-insensitive substring match
	return line.contains(m_token, Qt::CaseInsensitive);
}

bool OrFilter::matches(const QString& line) const
{
	for (const auto& p : m_parts)
	{
		if (p && p->matches(line))
			return true;
	}
	return false;
}

std::shared_ptr<IFilter> parseFilter(const QString& tag)
{
	if (tag.trimmed().isEmpty())
		return nullptr;

	// Split by '|' and create parts. 'a|b|c' -> OrFilter(Literal(a), Literal(b), Literal(c))
	QStringList parts = tag.split(QLatin1Char('|'), Qt::SkipEmptyParts);

	// Trim each part
	for (auto& p : parts)
		p = p.trimmed();

	if (parts.isEmpty())
		return nullptr;

	if (parts.size() == 1)
	{
		return std::make_shared<LiteralFilter>(parts.first());
	}

	std::vector<std::shared_ptr<IFilter>> filters;
	filters.reserve(parts.size());
	for (const auto& p : parts)
	{
		filters.push_back(std::make_shared<LiteralFilter>(p));
	}

	return std::make_shared<OrFilter>(std::move(filters));
}

std::vector<std::shared_ptr<IFilter>> parseFilters(const QStringList& tags)
{
	std::vector<std::shared_ptr<IFilter>> out;
	out.reserve(tags.size());
	for (const auto& t : tags)
	{
		auto f = parseFilter(t);
		if (f)
			out.push_back(std::move(f));
	}
	return out;
}