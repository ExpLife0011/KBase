/*
 @ 0xCCCCCCCC
*/

#include "kbase/path.h"

#include <cassert>
#include <cstdlib>

#include <algorithm>
#include <functional>
#include <iterator>
#include <stdexcept>

#include "kbase/error_exception_util.h"
#include "kbase/string_util.h"
#include "kbase/sys_string_encoding_conversions.h"

namespace {

using kbase::Path;

using value_type = Path::value_type;
using string_type = Path::string_type;

// If the |path| contains a drive letter specification, returns the position of the
// last character of the specification; otherwise, return npos.
string_type::size_type FindDriveLetter(const string_type& path)
{
    if (path.length() >= 2 && path[1] == L':' &&
        ((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z'))) {
        return 1;
    }

    return string_type::npos;
}

inline bool EqualPathValue(const string_type& x, const string_type& y)
{
    return !kbase::SysStringCompareCaseInsensitive(x, y);
}

// This function adheres the equality stipulated for two Path objects:
// They can differ in the case, which is permitted by Windows.
bool EqualDriveLetterCaseInsensitive(const string_type& x, const string_type& y)
{
    auto letter_pos_x = FindDriveLetter(x);
    auto letter_pos_y = FindDriveLetter(y);

    // if only one contains a drive letter, the comparison result is same as
    // x == y
    if (letter_pos_x == string_type::npos || letter_pos_y == string_type::npos) {
        return EqualPathValue(x, y);
    }

    string_type&& letter_x = x.substr(0, letter_pos_x + 1);
    string_type&& letter_y = y.substr(0, letter_pos_y + 1);
    if (!EqualPathValue(letter_x, letter_y)) {
        return false;
    }

    string_type&& rest_x = x.substr(letter_pos_x + 1);
    string_type&& rest_y = y.substr(letter_pos_y + 1);
    return EqualPathValue(rest_x, rest_y);
}

bool IsPathAbsolute(const string_type& path)
{
    string_type::size_type drive_letter = FindDriveLetter(path);

    // Such as c:\foo or \\foo .etc
    if (drive_letter != string_type::npos) {
        return (drive_letter + 1 < path.length()) &&
            (Path::IsSeparator(path[drive_letter + 1]));
    }

    return (path.length() > 1) &&
        (Path::IsSeparator(path[0]) && Path::IsSeparator(path[1]));
}

string_type::size_type ExtensionSeparatorPosition(const string_type& path)
{
    // Special cases for which path contains '.' but does not follow with an extension.
    if (path == Path::kCurrentDir || path == Path::kParentDir) {
        return string_type::npos;
    }

    return path.rfind(Path::kExtensionSeparator);
}

bool IsPathEmptyOrSpecialCase(const string_type& path)
{
    if (path.empty() || path == Path::kCurrentDir ||
        path == Path::kParentDir) {
        return true;
    }

    return false;
}

}   // namespace

namespace kbase {

Path::Path(const Path& other)
    : path_(other.path_)
{}

Path::Path(Path&& other)
    : path_(std::move(other.path_))
{}

Path::Path(const string_type& path)
    : path_(path)
{
    // The null-terminator '\0' indicates the end of a path.
    string_type::size_type null_pos = path_.find(kStringTerminator);
    if (null_pos != string_type::npos) {
        path_ = path_.substr(0, null_pos);
    }
}

Path& Path::operator=(const Path& other)
{
    path_ = other.path_;
    return *this;
}

Path& Path::operator=(Path&& other)
{
    path_ = std::move(other.path_);
    return *this;
}

bool operator==(const Path& lhs, const Path& rhs)
{
    return EqualDriveLetterCaseInsensitive(lhs.value(), rhs.value());
}

bool operator!=(const Path& lhs, const Path& rhs)
{
    return !EqualDriveLetterCaseInsensitive(lhs.value(), rhs.value());
}

bool operator<(const Path& lhs, const Path& rhs)
{
    return (kbase::SysStringCompareCaseInsensitive(lhs.value(), rhs.value()) < 0);
}

// static
bool Path::IsSeparator(value_type ch)
{
    using std::placeholders::_1;
    using std::equal_to;

    return std::any_of(std::begin(kSeparators), std::end(kSeparators),
                       std::bind(equal_to<value_type>(), ch, _1));
}

bool Path::EndsWithSeparator() const
{
    if (empty()) {
        return false;
    }

    return IsSeparator(path_.back());
}

Path Path::AsEndingWithSeparator() const
{
    if (empty() || EndsWithSeparator()) {
        return Path(*this);
    }

    string_type new_path_str;
    new_path_str.reserve(path_.length() + 1);

    new_path_str = path_;
    new_path_str.append(1, kSeparators[0]);

    return Path(new_path_str);
}

void Path::StripTrailingSeparatorsInternal()
{
    // start always points to the position one-offset past the leading separator
    string_type::size_type start = FindDriveLetter(path_) + 2;

    string_type::size_type last_stripped = string_type::npos;
    string_type::size_type pos = path_.length();
    for (; pos > start && IsSeparator(path_[pos-1]); --pos) {
        if (pos != start + 1 ||
            !IsSeparator(path_[start-1]) ||
            last_stripped != string_type::npos) {
            last_stripped = pos;
        } else {
            break;
        }
    }

    path_.resize(pos);
}

Path Path::StripTrailingSeparators() const
{
    Path new_path(path_);
    new_path.StripTrailingSeparatorsInternal();

    return new_path;
}

Path Path::DirName() const
{
    Path new_path(path_);
    new_path.StripTrailingSeparatorsInternal();

    auto letter = FindDriveLetter(new_path.path_);
    auto last_separator = new_path.path_.find_last_of(kSeparators, string_type::npos,
                                                      kSeparatorCount);

    // there might be a drive letter in the path
    if (last_separator == string_type::npos) {
        // in current dir
        new_path.path_.resize(letter + 1);
    } else if (last_separator == letter + 1) {
        // in root dir
        new_path.path_.resize(letter + 2);
    } else if (last_separator == letter + 2 &&
               IsSeparator(new_path.path_[letter+1])) {
        // preserves the leading double-separator
        new_path.path_.resize(letter + 3);
    } else {
        new_path.path_.resize(last_separator);
    }

    new_path.StripTrailingSeparatorsInternal();
    if (new_path.path_.empty()) {
        new_path.path_ = kCurrentDir;
    }

    return new_path;
}

Path Path::BaseName() const
{
    Path new_path(path_);
    new_path.StripTrailingSeparatorsInternal();

    auto letter = FindDriveLetter(new_path.path_);
    if (letter != string_type::npos) {
        new_path.path_.erase(0, letter + 1);
    }

    auto last_separator = new_path.path_.find_last_of(kSeparators, string_type::npos,
                                                      kSeparatorCount);
    if (last_separator != string_type::npos &&
        last_separator < new_path.path_.length() - 1) {
        new_path.path_.erase(0, last_separator + 1);
    }

    return new_path;
}

void Path::GetComponents(std::vector<string_type>* components) const
{
    assert(components);
#if defined(NDEBUG)
    if (!components) {
        throw std::invalid_argument("components cannot be NULL!");
    }
#endif

    components->clear();

    if (path_.empty()) {
        return;
    }

    std::vector<string_type> parts;
    Path current(path_);
    Path base;

    auto AreAllSeparators = [](const string_type& path) -> bool {
        return std::all_of(path.cbegin(), path.cend(), Path::IsSeparator);
    };

    // main body
    while (current != current.DirName()) {
        base = current.BaseName();
        if (!AreAllSeparators(base.value())) {
            parts.push_back(base.value());
        }
        current = current.DirName();
    }

    // root
    base = current.BaseName();
    if (!base.empty() && base.value() != kCurrentDir) {
        parts.push_back(base.value());
    }

    // drive letter
    auto letter = FindDriveLetter(current.value());
    if (letter != string_type::npos) {
        parts.push_back(current.value().substr(0, letter + 1));
    }

    std::copy(parts.crbegin(), parts.crend(), std::back_inserter(*components));
}

bool Path::IsAbsolute() const
{
    return IsPathAbsolute(path_);
}

void Path::Append(const string_type& components)
{
    const string_type* need_appended = &components;
    string_type without_null;

    string_type::size_type null_pos = need_appended->find(kStringTerminator);
    if (null_pos != string_type::npos) {
        without_null = components.substr(0, null_pos);
        need_appended = &without_null;
    }

    ENSURE(CHECK, !IsPathAbsolute(*need_appended)).Require();

    // If appends to the current dir, just set the path as the components.
    if (path_ == kCurrentDir) {
        path_ = *need_appended;
        return;
    }

    StripTrailingSeparatorsInternal();

    // If the path is empty, that indicates current directory.
    // If the path component is empty, that indicates nothing to append.
    if (!need_appended->empty() && !path_.empty()) {
        // Don't append separator, if there is already one.
        if (!IsSeparator(path_[path_.length()-1])) {
            // Don't append separator, if the path is a drive letter,
            // which is a valid relative path.
            if (FindDriveLetter(path_) + 1 != path_.length()) {
                path_.append(1, kSeparators[0]);
            }
        }
    }

    path_.append(*need_appended);
}

void Path::Append(const Path& components)
{
    Append(components.value());
}

Path Path::AppendTo(const string_type& components) const
{
    Path new_path(*this);
    new_path.Append(components);

    return new_path;
}

Path Path::AppendTo(const Path& components) const
{
    return AppendTo(components.value());
}

bool Path::AppendRelativePath(const Path& child, Path* path) const
{
    std::vector<string_type> current_components;
    std::vector<string_type> child_components;
    GetComponents(&current_components);
    child.GetComponents(&child_components);

    if (current_components.empty() ||
        current_components.size() >= child_components.size()) {
        return false;
    }

    auto current_iter = current_components.cbegin();
    auto child_iter = child_components.cbegin();
    for (; current_iter != current_components.cend(); ++current_iter, ++child_iter) {
        if (!EqualPathValue(*current_iter, *child_iter)) {
            return false;
        }
    }

    if (path) {
        for (; child_iter != child_components.cend(); ++child_iter) {
            path->Append(*child_iter);
        }
    }

    return true;
}

bool Path::IsParent(const Path& child) const
{
    return AppendRelativePath(child, nullptr);
}

void Path::AppendASCII(const std::string& components)
{
    ENSURE(CHECK, IsStringASCII(components)).Require();

    Append(ASCIIToWide(components));
}

kbase::Path Path::AppendASCIITo(const std::string& components) const
{
    Path new_path(*this);
    new_path.AppendASCII(components);

    return new_path;
}

string_type Path::Extension() const
{
    Path base(BaseName());
    string_type::size_type separator_pos = ExtensionSeparatorPosition(base.path_);

    if (separator_pos == string_type::npos) {
        return string_type();
    }

    return base.path_.substr(separator_pos);
}

void Path::RemoveExtension()
{
    if (Extension().empty()) {
        return;
    }

    string_type::size_type separator_pos = ExtensionSeparatorPosition(path_);
    if (separator_pos != string_type::npos) {
        path_ = path_.substr(0, separator_pos);
    }
}

Path Path::StripExtention() const
{
    Path new_path(path_);
    new_path.RemoveExtension();

    return new_path;
}

Path Path::InsertBeforeExtension(const string_type& suffix) const
{
    if (suffix.empty()) {
        return Path(*this);
    }

    if (IsPathEmptyOrSpecialCase(BaseName().value())) {
        return Path();
    }

    string_type extension = Extension();
    Path new_path = StripExtention();
    new_path.path_.append(suffix).append(extension);

    return new_path;
}

Path Path::AddExtension(const string_type& extension) const
{
    if (IsPathEmptyOrSpecialCase(BaseName().value())) {
        return Path();
    }

    if (extension.empty() || extension == string_type(1, kExtensionSeparator)) {
        return Path(*this);
    }

    Path new_path(path_);

    // If neither the path nor the extension contains the separator, adds
    // one manually.
    if (new_path.path_.back() != kExtensionSeparator &&
        extension.front() != kExtensionSeparator) {
        new_path.path_.append(1, kExtensionSeparator);
    }

    new_path.path_.append(extension);

    return new_path;
}

Path Path::ReplaceExtension(const string_type& extension) const
{
    if (IsPathEmptyOrSpecialCase(BaseName().value())) {
        return Path();
    }

    Path new_path = StripExtention();

    if (extension.empty() || extension == string_type(1, kExtensionSeparator)) {
        return new_path;
    }

    if (extension[0] != kExtensionSeparator) {
        new_path.path_.append(1, kExtensionSeparator);
    }

    new_path.path_.append(extension);

    return new_path;
}

bool Path::MatchExtension(const string_type& extension) const
{
    string_type ext = Extension();
    return kbase::StringToLowerASCII(ext) == kbase::StringToLowerASCII(extension);
}

bool Path::ReferenceParent() const
{
    std::vector<string_type> components;
    GetComponents(&components);

    // It seems redundant spaces at the tail of '..' can be ignored by Windows.
    string_type trimed_component;
    for (const string_type& component : components) {
        kbase::TrimTailingStr(component, L" ", &trimed_component);
        if (trimed_component == kParentDir) {
            return true;
        }
    }

    return false;
}

std::string Path::AsASCII() const
{
    if (!kbase::IsStringASCII(path_)) {
        return std::string();
    }

    return kbase::WideToASCII(path_);
}

std::string Path::AsUTF8() const
{
    return kbase::SysWideToUTF8(path_);
}

// static
Path Path::FromASCII(const std::string& path_in_ascii)
{
    if (!kbase::IsStringASCII(path_in_ascii)) {
        return Path();
    }

    return Path(kbase::ASCIIToWide(path_in_ascii));
}

// static
Path Path::FromUTF8(const std::string& path_in_utf8)
{
    return Path(kbase::SysUTF8ToWide(path_in_utf8));
}

Path Path::NormalizePathSeparatorTo(value_type separator) const
{
    string_type new_path_str = path_;

    for (value_type sep : kSeparators) {
        std::replace(new_path_str.begin(), new_path_str.end(), sep, separator);
    }

    return Path(new_path_str);
}

Path Path::NormalizePathSeparator() const
{
    return NormalizePathSeparatorTo(kSeparators[0]);
}

// static
int Path::CompareIgnoreCase(const string_type& str1, const string_type& str2)
{
    return kbase::SysStringCompareCaseInsensitive(str1, str2);
}

}   // namespace kbase