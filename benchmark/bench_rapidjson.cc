#include "bench_rapidjson.h"

#include <vector>

#include "../ext/rapidjson/reader.h"

#include "traverse.h"

namespace {

class SaxHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, SaxHandler>
{
public:
    jsonstats& stats;

public:
    typedef typename rapidjson::UTF8<>::Ch Ch;

    SaxHandler(jsonstats& s) : stats(s) {}

    bool Null()
    {
        ++stats.null_count;
        return true;
    }

    bool Bool(bool value)
    {
        if (value)
            ++stats.true_count;
        else
            ++stats.false_count;
        return true;
    }

    bool Int(int value)
    {
        ++stats.number_count;
        stats.total_number_value += static_cast<double>(value);
        return true;
    }

    bool Uint(unsigned value)
    {
        ++stats.number_count;
        stats.total_number_value += static_cast<double>(value);
        return true;
    }

    bool Int64(int64_t value)
    {
        ++stats.number_count;
        stats.total_number_value += static_cast<double>(value);
        return true;
    }

    bool Uint64(uint64_t value)
    {
        ++stats.number_count;
        stats.total_number_value += static_cast<double>(value);
        return true;
    }

    bool Double(double value)
    {
        ++stats.number_count;
        stats.total_number_value += value;
        return true;
    }

    bool String(Ch const* /*ptr*/, rapidjson::SizeType length, bool /*copy*/)
    {
        ++stats.string_count;
        stats.total_string_length += length;
        return true;
    }

    bool StartObject()
    {
        ++stats.object_count;
        return true;
    }

    bool Key(Ch const* /*ptr*/, rapidjson::SizeType length, bool /*copy*/)
    {
        ++stats.key_count;
        stats.total_key_length += length;
        return true;
    }

    bool EndObject(rapidjson::SizeType /*memberCount*/)
    {
        //stats.total_object_length += memberCount;
        return true;
    }

    bool StartArray()
    {
        ++stats.array_count;
        return true;
    }

    bool EndArray(rapidjson::SizeType /*elementCount*/)
    {
        //stats.total_array_length += elementCount;
        return true;
    }
};

class DomHandler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, DomHandler>
{
public:
    std::vector<json::Value> stack;
    std::vector<json::String> keys;

public:
    typedef typename rapidjson::UTF8<>::Ch Ch;

    bool Null()
    {
        stack.emplace_back(json::null_tag);
        return true;
    }

    bool Bool(bool value)
    {
        stack.emplace_back(json::boolean_tag, value);
        return true;
    }

    bool Int(int value)
    {
        stack.emplace_back(json::number_tag, static_cast<double>(value));
        return true;
    }

    bool Uint(unsigned value)
    {
        stack.emplace_back(json::number_tag, static_cast<double>(value));
        return true;
    }

    bool Int64(int64_t value)
    {
        stack.emplace_back(json::number_tag, static_cast<double>(value));
        return true;
    }

    bool Uint64(uint64_t value)
    {
        stack.emplace_back(json::number_tag, static_cast<double>(value));
        return true;
    }

    bool Double(double value)
    {
        stack.emplace_back(json::number_tag, value);
        return true;
    }

    bool String(Ch const* ptr, rapidjson::SizeType length, bool /*copy*/)
    {
        stack.emplace_back(json::string_tag, ptr, ptr + length);
        return true;
    }

    bool StartObject()
    {
        stack.emplace_back(json::object_tag);
        return true;
    }

    bool Key(Ch const* ptr, rapidjson::SizeType length, bool /*copy*/)
    {
        keys.emplace_back(ptr, ptr + length);
        return true;
    }

    bool EndObject(rapidjson::SizeType memberCount)
    {
        PopMembers(memberCount);
        return true;
    }

    bool StartArray()
    {
        stack.emplace_back(json::array_tag);
        return true;
    }

    bool EndArray(rapidjson::SizeType elementCount)
    {
        PopElements(elementCount);
        return true;
    }

private:
    void PopElements(size_t num_elements)
    {
        if (num_elements == 0)
            return;

        auto const count = static_cast<std::ptrdiff_t>(num_elements);

        auto const I = stack.end() - count;
        auto const E = stack.end();

        auto& arr = I[-1].get_array();
        arr.insert(arr.end(), std::make_move_iterator(I), std::make_move_iterator(E));

        stack.erase(I, E);
    }

    void PopMembers(size_t num_members)
    {
        if (num_members == 0)
            return;

        auto const count = static_cast<std::ptrdiff_t>(num_members);

        auto const Iv = stack.end() - count;
        auto const Ik = keys.end() - count;

        auto& obj = Iv[-1].get_object();

        for (std::ptrdiff_t i = 0; i != count; ++i)
        {
            auto& K = Ik[i];
            auto& V = Iv[i];
            obj[std::move(K)] = std::move(V);
        }

        stack.erase(Iv, stack.end());
        keys.erase(Ik, keys.end());
    }
};

} // namespace

constexpr int kParseFlags = rapidjson::kParseFullPrecisionFlag | rapidjson::kParseStopWhenDoneFlag | rapidjson::kParseValidateEncodingFlag;

bool rapidjson_sax_stats(jsonstats& stats, char const* first, char const* last)
{
    SaxHandler handler(stats);

    rapidjson::MemoryStream is(first, static_cast<size_t>(last - first));

    rapidjson::Reader reader;
    auto const res = reader.Parse<kParseFlags>(is, handler);

    return !res.IsError();
}

bool rapidjson_dom_stats(jsonstats& stats, char const* first, char const* last)
{
    DomHandler handler;

    rapidjson::MemoryStream is(first, static_cast<size_t>(last - first));

    rapidjson::Reader reader;
    auto const res = reader.Parse<kParseFlags>(is, handler);

    if (res.IsError())
        return false;

    traverse(stats, handler.stack.back());
    return true;
}
