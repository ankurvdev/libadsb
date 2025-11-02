// cppforge-sync
#pragma once
#include "CommonMacros.h"

#if ((defined HAVE_EMBEDRESOURCE) && HAVE_EMBEDRESOURCE)
#include <EmbeddedResource.h>
#endif

#if ((defined HAVE_RAPIDJSON) && HAVE_RAPIDJSON)
SUPPRESS_WARNINGS_START
SUPPRESS_STL_WARNINGS
SUPPRESS_FMT_WARNINGS
SUPPRESS_MSVC_WARNING(5232)    // 5232: in C++20 this comparison calls
#include <rapidjson/document.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/rapidjson.h>
SUPPRESS_WARNINGS_END
#endif

SUPPRESS_WARNINGS_START
SUPPRESS_STL_WARNINGS
SUPPRESS_FMT_WARNINGS
SUPPRESS_MSVC_WARNING(4388)    // signed / unsigned mismatch (Catch2)
#include <catch2/catch_all.hpp>
#include <dtl/dtl.hpp>
#include <fmt/ostream.h>
#include <fmt/ranges.h>
SUPPRESS_WARNINGS_END

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#if defined HAVE_EMBEDRESOURCE
DECLARE_RESOURCE_COLLECTION(testdata);
#endif
namespace TestCommon
{
#if !defined _WIN32
inline bool IsDebuggerPresent()
{
    return true;
}
#endif

/*
inline std::vector<std::string> readlines(std::filesystem::path const& path)
{
    std::fstream file(path);
    return readlines(file);
}
*/

inline auto WriteStrResourse(std::vector<std::string> const& actualstring, std::string_view const& resname)
{
    auto          outf = std::filesystem::absolute(std::string(resname) + ".txt");
    std::ofstream f(outf);
    for (auto& l : actualstring) { f << l << "\n"; }
    return outf;
}

inline auto WriteBinResourse(std::vector<std::string> const& actualstring, std::string_view const& resname)
{
    auto          outf = std::filesystem::absolute(std::string(resname) + ".bin");
    std::ofstream f(outf, std::ios::binary);
    for (auto& l : actualstring)
    {
        size_t size = l.size();
        f.write(reinterpret_cast<char const*>(&size), sizeof(size));
        f.write(l.data(), static_cast<std::streamsize>(l.size()));
    }
    return outf;
}

inline auto WriteStrmResourse(std::string const& actualstring, std::string_view const& resname)
{
    auto          outf = std::filesystem::absolute(std::string(resname) + ".bin");
    std::ofstream f(outf, std::ios::binary);
    size_t        size = actualstring.size();
    f.write(reinterpret_cast<char const*>(&size), sizeof(size));
    f.write(actualstring.data(), static_cast<std::streamsize>(actualstring.size()));
    return outf;
}

inline void PrintLinesDiff(std::vector<std::string> const& actualstring, std::vector<std::string> const& expectedstring)
{

    dtl::Diff<std::string, std::vector<std::string>> d(expectedstring, actualstring);
    d.compose();                // construct an edit distance and LCS and SES
    d.composeUnifiedHunks();    // construct a difference as Unified Format with SES.

    if (actualstring.size() == expectedstring.size())
    {
        for (size_t i = 0; i < actualstring.size(); i++)
        {
            if (actualstring[i] != expectedstring[i])
            {
                std::cout << "Line: " << i << " Expected: " << expectedstring[i] << std::endl;
                std::cout << "Line: " << i << " Actual: " << actualstring[i] << std::endl;
                std::cout << "Line: " << i << " Delta: ";
                dtl::Diff<char, std::string> ld(expectedstring[i], actualstring[i]);
                ld.compose();
                auto                     ses      = ld.getSes().getSequence();
                int                      lasttype = 0;
                std::string              merged;
                std::vector<std::string> deltas;
                for (auto& sesobj : ses)
                {
                    if (sesobj.second.type != lasttype)
                    {
                        if (lasttype == dtl::SES_COMMON) { merged = fmt::format("[{}:{}]", i, sesobj.second.afterIdx); }
                        merged += (sesobj.second.type == dtl::SES_ADD ? '+' : '-');
                    }
                    if (sesobj.second.type == dtl::SES_COMMON)
                    {
                        if (merged.size() > 0) deltas.push_back(std::move(merged));
                    }
                    else
                    {
                        merged += sesobj.first;
                    }
                    lasttype = sesobj.second.type;
                }
                if (merged.size() > 0) deltas.push_back(std::move(merged));
                for (auto& delta : deltas) { std::cout << delta << " "; }
                std::cout << std::endl;
            }
        }
    }
    else
    {
        d.printUnifiedFormat();    // print a difference as Unified Format.
    }
}
/*
inline void CompareStrLines(std::vector<std::string> const& actualstring,
                            std::vector<std::string> const& expectedstring,
                            std::string_view const&         resname = "test")
{
    if (actualstring != expectedstring)
    {
        PrintDiff(actualstring, expectedstring);
        WriteStrResourse(actualstring, resname);
    }
}

inline void CompareBinLines(std::vector<std::string> const& actualstring,
                            std::vector<std::string> const& expectedstring,
                            std::string_view const&         resname = "test")
{
    if (actualstring != expectedstring)
    {
        PrintDiff(actualstring, expectedstring);
        auto outf = WriteBinResourse(actualstring, resname);
        FAIL_CHECK(fmt::format("Comparison Failed: Output: \n{}", outf.string()));
    }
}
*/
#if defined HAVE_EMBEDRESOURCE

inline std::string GeneratePrefixFromTestName()
{
    auto prefix = Catch::getResultCapture().getCurrentTestName() + "_";
    for (auto& c : prefix)
    {
        if (std::isalpha(c) || std::isdigit(c)) continue;
        c = '_';
    }
    return prefix;
}

struct ResourceFileManager
{

    ResourceFileManager() = default;
    ~ResourceFileManager()
    {
        for (auto const& [k, v] : _openedfiles) { std::filesystem::remove(v); }
    }

    auto load(std::string const& name, std::string const& prefix)
    {
        auto testresname = GeneratePrefixFromTestName() + name;

        auto it = _openedfiles.find(testresname);
        if (it != _openedfiles.end()) { return it->second; }
        auto resourceCollection = LOAD_RESOURCE_COLLECTION(testdata);
        for (auto const r : resourceCollection)
        {
            auto resname = std::string(r.name());
            if (resname == testresname || resname == name)
            {
                auto          path = std::filesystem::current_path() / (prefix + resname);
                std::ofstream f(path);
                auto const&   str = r.string();
                if (!f.is_open()) { throw std::runtime_error("Cannot write resource file : " + path.string()); }
                f << str;
                f.close();
                _openedfiles[resname] = path;
                return path;
            }
        }
        throw std::logic_error("Cannot find resource : " + testresname);
    }
    std::unordered_map<std::string, std::filesystem::path> _openedfiles;
};
/*
inline std::vector<std::string> LoadStrResource(std::string_view const& name)
{
    auto testresname = GeneratePrefixFromTestName() + std::string(name) + ".txt";

    auto resourceCollection = LOAD_RESOURCE_COLLECTION(testdata);
    for (auto const r : resourceCollection)
    {
        auto resname = r.name();
        if (resname == testresname || resname == name)
        {
            std::string       str(r.string());
            std::stringstream ss(str);
            return readlines(ss);
        }
    }
    return std::vector<std::string>();
}

inline std::vector<std::string> LoadBinResource(std::string_view const& name)
{
    auto testresname = GeneratePrefixFromTestName() + std::string(name) + ".bin";

    auto resourceCollection = LOAD_RESOURCE_COLLECTION(testdata);
    for (auto const r : resourceCollection)
    {
        auto resname = r.name();
        if (resname == testresname || resname == name)
        {
            std::string              str(r.string());
            std::istringstream       ss(str);
            std::size_t              size;
            std::vector<std::string> data;
            ss.peek();
            while (!ss.eof())
            {
                ss.read(reinterpret_cast<char*>(&size), sizeof(size));
                if (size > 1024 * 1024) { return data; }
                std::string line;
                line.resize(size);
                ss.read(line.data(), static_cast<std::streamsize>(size));
                if (ss.eof()) throw std::logic_error("Reached end unexpectedly");
                data.push_back(line);
                ss.peek();
            }
            return data;
        }
    }
    return std::vector<std::string>();
}*/

inline std::vector<std::string> ReadStrStream(std::istream& istr)
{
    std::vector<std::string> lines;
    std::string              line;

    while (std::getline(istr, line))
    {
        if (line.length() > 0 && line[line.length() - 1] == '\r') { line.resize(line.length() - 1); }
        lines.push_back(std::move(line));
    }
    return lines;
}

inline auto ResplitLines(std::vector<std::string> const& actual)
{
    std::stringstream ss;
    for (auto& line : actual) ss << line << std::endl;
    return TestCommon::ReadStrStream(ss);
}

inline std::vector<std::string> ReadBinStream(std::istream& ss)
{
    std::size_t              size;
    std::vector<std::string> data;
    ss.peek();
    while (!ss.eof())
    {
        ss.read(reinterpret_cast<char*>(&size), sizeof(size));
        if (size > 1024 * 1024) { return data; }
        std::string line;
        line.resize(size);
        ss.read(line.data(), static_cast<std::streamsize>(size));
        if (ss.eof()) throw std::logic_error("Reached end unexpectedly");
        data.push_back(line);
        ss.peek();
    }
    return data;
}

struct StrFormat
{
    static auto ReadStream(std::istream& ss) { return ReadStrStream(ss); }
    static auto WriteResource(std::vector<std::string> const& actualstring, std::string_view const& resname)
    {
        return WriteStrResourse(actualstring, resname);
    }
    static auto PrintDiff(std::vector<std::string> const& actualstring, std::istream& ss) { PrintLinesDiff(actualstring, ReadStream(ss)); }

    static bool Compare(std::vector<std::string> const& actual, std::istream& ss)
    {
        std::stringstream actualstrm;
        for (auto const& line : actual) actualstrm << line << '\n';
        return ReadStream(actualstrm) == ReadStream(ss);
    }
};

SUPPRESS_WARNINGS_START
SUPPRESS_CLANG_WARNING("-Wmissing-noreturn")
inline bool JsonStringEqual([[maybe_unused]] std::string const& lhs, [[maybe_unused]] std::string const& rhs)
{
#if ((defined HAVE_RAPIDJSON) && HAVE_RAPIDJSON)
    rapidjson::Document doclhs, docrhs;
    doclhs.Parse(lhs.c_str());
    docrhs.Parse(rhs.c_str());
    return doclhs == docrhs;
#else
    throw std::logic_error("RapidJson needed");
#endif
}
SUPPRESS_WARNINGS_END

struct JsonFormat : StrFormat
{
    static bool Compare(std::vector<std::string> const& actual, std::istream& ss)
    {

        auto expected = ReadStrStream(ss);
        if (actual.size() != expected.size()) return false;

        for (size_t i = 0; i != actual.size(); i++)
        {
            if (!JsonStringEqual(actual[i], expected[i])) return false;
        }
        return true;
    }
};

struct BinFormat
{
    static auto ReadStream(std::istream& ss) { return ReadBinStream(ss); }

    static auto WriteResource(std::vector<std::string> const& actualstring, std::string_view const& resname)
    {
        return WriteBinResourse(actualstring, resname);
    }
    static auto PrintDiff(std::vector<std::string> const& /*actualstring*/, std::istream& /*ss*/) {}

    static bool Compare(std::vector<std::string> const& actual, std::istream& ss) { return actual == ReadStream(ss); }
};

template <typename TFormat> inline bool _CheckResource(std::vector<std::string> const& actual, std::string_view const& resourcename)
{
    auto testresname = fmt::format("{}{}", GeneratePrefixFromTestName(), resourcename);
    for (auto const r : LOAD_RESOURCE_COLLECTION(testdata))
    {
        auto resname = r.name();
        if (resname.find(testresname) == std::string::npos) continue;
        std::string       str(r.string());
        std::stringstream ss(str);
        if (TFormat::Compare(actual, ss)) return true;
    }

    for (auto const r : LOAD_RESOURCE_COLLECTION(testdata))
    {
        auto resname = r.name();
        if (resname.find(testresname) == std::string::npos) continue;
        std::string       str(r.string());
        std::stringstream ss(str);
        TFormat::PrintDiff(actual, ss);
        break;
    }

    auto outf = TFormat::WriteResource(actual, testresname);
    FAIL_CHECK(fmt::format("Comparison Failed: Output: \n{}", outf.string()));
    return false;
}

template <typename TFormat> inline void CheckResource(std::vector<std::string> const& actual, std::string_view const& resourcename)
{
    _CheckResource<TFormat>(actual, resourcename);
}
#endif

}    // namespace TestCommon
