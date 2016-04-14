/*
 @ 0xCCCCCCCC
*/

#include "stdafx.h"

#include "gtest\gtest.h"
#include "kbase\path.h"

#include <algorithm>
#include <functional>
#include <tuple>
#include <xutility>

#include "kbase\basic_types.h"

using std::placeholders::_1;
using kbase::PathString;
using kbase::Path;

namespace {

typedef std::pair<Path, Path> PathTestPair;

bool ContainsNullterminator(const PathString& path_str)
{
    return std::any_of(path_str.begin(), path_str.end(),
                       std::bind(std::equal_to<wchar_t>(), L'\0', _1));
}

}   // namespace

// Null-terminator character indicates the end of a path.
// |Path| internally cuts them off.
TEST(PathTest, Ctor)
{
    PathString test_path = L"C:\\abc\\def";
    PathString test_path2 = test_path + PathString(2, L'\0');
    ASSERT_FALSE(ContainsNullterminator(test_path));
    ASSERT_TRUE(ContainsNullterminator(test_path2));

    Path path(test_path2);
    EXPECT_FALSE(ContainsNullterminator(path.value()));
}

TEST(PathTest, Comparison)
{
    Path path1(L"c:\\abc\\def\\ghi.ado"), path2(L"C:\\ABC\\DEF\\GHI.ado");
    Path path3(L"C:\\test\\unk");

    EXPECT_EQ(path1, path2);
    EXPECT_NE(path1, path3);
    EXPECT_LT(path1, path3);
}

TEST(PathTest, PathSeparator)
{
    // IsSeparator
    EXPECT_TRUE(Path::IsSeparator(L'\\'));
    EXPECT_TRUE(Path::IsSeparator(L'/'));
    EXPECT_FALSE(Path::IsSeparator(L'|'));

    {
        EXPECT_FALSE(Path().EndsWithSeparator());
        EXPECT_TRUE(Path().AsEndingWithSeparator().empty());
        Path path(L"C:\\test\\path");
        EXPECT_FALSE(path.EndsWithSeparator());
        path = path.AsEndingWithSeparator();
        EXPECT_TRUE(path.EndsWithSeparator());
        EXPECT_FALSE(path.StripTrailingSeparators().EndsWithSeparator());
        Path p(L"C:\\test\\path");
        EXPECT_EQ(p, p.StripTrailingSeparators());
    }

    {
        Path path(L"C:\\test\\path");
        Path crit(L"C:/test/path");
        EXPECT_EQ(crit, path.NormalizePathSeparatorTo(L'/'));
        EXPECT_EQ(crit.NormalizePathSeparator(), path);
    }
}

TEST(PathTest, PathComponents)
{
    const Path kCurrentPath(L".");
    const Path kRootDir(L"..");

    Path cur_paths[] { Path(L""), Path(L"."), Path(L"abc"), Path(L"./abc") };
    for (const auto& path : cur_paths) {
        EXPECT_EQ(kCurrentPath, path.DirName());
    }

    Path root_paths[] { Path(L"../abc") };
    for (const auto& path : root_paths) {
        EXPECT_EQ(kRootDir, path.DirName());
    }

    Path path(L"C:\\test\\path\\data.txt");
    EXPECT_EQ(path.DirName(), Path(L"C:\\test\\path"));
    EXPECT_EQ(Path(L"C:\\"), Path(L"C:\\").DirName());

    PathTestPair base_dir_test[] {
        { Path(), Path() },
        { Path(L"."), Path(L".") },
        { Path(L"abc"), Path(L"abc") },
        { Path(L"abc"), Path(L"./abc") },
        { Path(L"abc"), Path(L"C:\\abc") },
        { Path(L"\\"), Path(L"C:\\") }
    };

    for (const auto& p : base_dir_test) {
        EXPECT_EQ(p.first, p.second.BaseName());
    }

    typedef std::pair<Path, std::vector<std::wstring>> PathComponentPair;
    PathComponentPair componnet_test[] {
        { Path(L"C:"), { L"C:" } },
        { Path(L"C:\\"), { L"C:", L"\\" } },
        { Path(L"C:\\foo\\bar"), { L"C:", L"\\", L"foo", L"bar"} }
    };

    std::vector<std::wstring> comp;
    for (const auto& p : componnet_test) {
        p.first.GetComponents(&comp);
        EXPECT_EQ(comp, p.second);
    }
}

TEST(PathTest, PathProperty)
{
    typedef std::pair<Path, bool> PathPropertyPair;

    // IsAbsolte
    PathPropertyPair abs_path[] {
        { Path(L"."), false },
        { Path(L"abc"), false },
        { Path(L"./abc"), false },
        { Path(L".."), false },
        { Path(L"../abc"), false },
        { Path(L"C:abc"), false },
        { Path(L"C://abc"), true }
    };

    for (const auto& p : abs_path) {
        EXPECT_EQ(p.first.IsAbsolute(), p.second);
    }

    // ReferenceParent
    PathPropertyPair refer_parent_path[] {
        { Path(L"./abc"), false },
        { Path(L".."), true },
        { Path(L"../abc"), true }
    };

    for (const auto& p : refer_parent_path) {
      EXPECT_EQ(p.first.ReferenceParent(), p.second);
    }

    typedef std::tuple<Path, Path, bool> PathPropertyTuple;
    PathPropertyTuple paths[] {
        std::make_tuple(Path(L"./abc"), Path(L"./abc/def"), true),
        std::make_tuple(Path(L"C:\\"), Path(L"C:\\abc"), true),
        std::make_tuple(Path(L"C:\\test\\"), Path(L"C:\\abc\\"), false)
    };

    for (const auto& t : paths) {
        EXPECT_EQ(std::get<0>(t).IsParent(std::get<1>(t)), std::get<2>(t));
    }
}

TEST(PathTest, PathAppend)
{
    typedef std::tuple<Path, Path, std::wstring> PathAppendTuple;
    PathAppendTuple paths[] {
        std::make_tuple(Path(L"C:abc"), Path(L"C:"), L"abc"),
        std::make_tuple(Path(L"C:\\abc"), Path(L"C:\\"), L"abc"),
        std::make_tuple(Path(L"C:\\abc\\def"), Path(L"C:\\abc"), L"def")
    };

    for (const auto& t : paths) {
        EXPECT_EQ(std::get<0>(t), std::get<1>(t).AppendTo(std::get<2>(t)));
    }

    Path current(LR"(C:\user\kingsley chen\)");
    Path child(LR"(C:\user\kingsley chen\app data\test)");
    Path path(LR"(C:\user\kingsley chen\documents\)");
    EXPECT_TRUE(current.AppendRelativePath(child, &path));
    EXPECT_EQ(path, Path(L"C:\\user\\kingsley chen\\documents\\app data\\test"));
}

TEST(PathTest, PathExtension)
{
    Path path(L"C:\\abc\\def\\xxx.dat");
    EXPECT_EQ(path.Extension(), std::wstring(L".dat"));
    path.RemoveExtension();
    EXPECT_EQ(path.Extension(), std::wstring());
    EXPECT_EQ(path, Path(L"C:\\abc\\def\\xxx"));
    path = path.AddExtension(L".txt");
    EXPECT_EQ(path.Extension(), std::wstring(L".txt"));
    path = path.ReplaceExtension(L".avi");
    EXPECT_EQ(path.Extension(), std::wstring(L".avi"));
    EXPECT_TRUE(path.MatchExtension(L".avi"));
}