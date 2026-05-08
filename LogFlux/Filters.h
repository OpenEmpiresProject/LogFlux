#pragma once

#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

// Simple filter model: base interface and concrete implementations.
// - Each top-level tag (from TagBar) will produce one IFilter instance.
// - A single tag text containing '|' is parsed into an OrFilter that
//   contains multiple LiteralFilter parts.
// - Across tags the matching logic remains AND (handled by the caller).

class IFilter
{
  public:
    virtual ~IFilter() = default;
    // Return true if the given line matches this filter.
    virtual bool matches(const QString& line) const = 0;
};

class LiteralFilter : public IFilter
{
  public:
    explicit LiteralFilter(QString token) : m_token(std::move(token))
    {
    }

    bool matches(const QString& line) const override;

  private:
    QString m_token;
};

class OrFilter : public IFilter
{
  public:
    explicit OrFilter(std::vector<std::shared_ptr<IFilter>> parts) : m_parts(std::move(parts))
    {
    }

    bool matches(const QString& line) const override;

  private:
    std::vector<std::shared_ptr<IFilter>> m_parts;
};

// Parse a single tag string into an IFilter instance.
// If the tag contains '|' it will return an OrFilter composed of LiteralFilters.
// Empty segments are ignored. If after parsing nothing remains, returns nullptr.
std::shared_ptr<IFilter> parseFilter(const QString& tag);

// Parse a list of tag strings into a vector of IFilter instances.
std::vector<std::shared_ptr<IFilter>> parseFilters(const QStringList& tags);