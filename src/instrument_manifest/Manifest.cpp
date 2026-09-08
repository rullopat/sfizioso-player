// SPDX-License-Identifier: BSD-2-Clause
#include "Manifest.h"
#include <sfzbundle/Parser.h>
#include <filesystem>
#include <set>
#include <cstring>

namespace sfizioso_manifest
{
namespace
{
juce::String string (const juce::var& value)
{
    return value.isString() && value.toString().length() <= 4096 ? value.toString() : juce::String();
}
bool integer (const juce::var& value) { return value.isInt() || value.isInt64(); }
bool boundedJson (const char* bytes, std::size_t size)
{
    int depth = 0;
    bool quoted = false, escaped = false;
    for (std::size_t i = 0; i < size; ++i)
    {
        const char c = bytes[i];
        if (c == 0) return false;
        if (quoted)
        {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') quoted = false;
        }
        else if (c == '"') quoted = true;
        else if (c == '{' || c == '[') { if (++depth > 32) return false; }
        else if (c == '}' || c == ']') { if (--depth < 0) return false; }
    }
    return ! quoted && depth == 0;
}
bool colour (const juce::String& s)
{
    return s.length() == 7 && s.startsWithChar ('#')
        && s.substring (1).containsOnly ("0123456789abcdefABCDEF");
}
}

bool isPackagePath (const juce::String& path)
{
    if (path.isEmpty() || path.length() > 4096 || path.startsWithChar ('/')
        || path.containsAnyOf ("\\:%?#")) return false;
    for (auto c : path) if (c < 32 || c == 127) return false;
    const auto parts = juce::StringArray::fromTokens (path, "/", "");
    for (const auto& part : parts)
        if (part.isEmpty() || part == "." || part == "..") return false;
    return true;
}

Result parse (const void* data, std::size_t size)
{
    Result out;
    auto warn = [&] (const juce::String& text) { out.diagnostics.add (text); };
    if (! data || size == 0 || size > maxJsonBytes
        || ! sfzbundle::isValidUtf8 ({ static_cast<const std::uint8_t*> (data), size })
        || ! boundedJson (static_cast<const char*> (data), size))
    {
        warn ("Manifest exceeds byte/depth limits or is not valid UTF-8 JSON."); return out;
    }
    juce::var root;
    if (juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), static_cast<int> (size)), root).failed()
        || ! root.isObject())
    {
        warn ("Manifest must be a JSON object."); return out;
    }
    if (string (root["format"]) != "sfizioso.instrument-manifest"
        || ! integer (root["formatVersion"]) || static_cast<juce::int64> (root["formatVersion"]) != 1)
    {
        warn ("Unsupported manifest format or major version."); return out;
    }
    Manifest manifest;
    const auto identity = root["instrument"];
    manifest.name = string (identity["name"]);
    manifest.id = string (identity["id"]);
    manifest.author = string (identity["author"]);
    manifest.version = string (identity["version"]);
    const auto* presets = root["presets"].getArray();
    if (manifest.name.trim().isEmpty() || ! presets || presets->isEmpty() || presets->size() > 256)
    {
        warn ("Instrument name and 1..256 presets are required."); return out;
    }
    std::set<juce::String> ids, paths;
    for (const auto& value : *presets)
    {
        Preset preset;
        preset.id = string (value["id"]);
        preset.name = string (value["name"]);
        preset.sfz = string (value["sfz"]);
        if (preset.id.trim().isEmpty() || preset.name.trim().isEmpty()
            || ! isPackagePath (preset.sfz) || ! preset.sfz.endsWithIgnoreCase (".sfz")
            || ! ids.insert (preset.id).second || ! paths.insert (preset.sfz).second)
        {
            warn ("Preset IDs and SFZ paths must be unique; presets need id, name and a safe SFZ path.");
            return out;
        }
        preset.category = string (value["category"]);
        preset.author = string (value["author"]);
        preset.background = string (value["artwork"]["controlsBackground"]);
        if (preset.background.isNotEmpty() && (! isPackagePath (preset.background)
            || ! (preset.background.endsWithIgnoreCase (".png") || preset.background.endsWithIgnoreCase (".jpg")
                  || preset.background.endsWithIgnoreCase (".jpeg"))))
        {
            warn ("Ignored unsafe or unsupported artwork path in " + preset.id); preset.background.clear();
        }
        const auto presentation = value["presentation"];
        preset.accent = string (presentation["accent"]);
        if (preset.accent.isNotEmpty() && ! colour (preset.accent))
        {
            warn ("Ignored invalid accent in " + preset.id); preset.accent.clear();
        }
        const auto* sections = presentation["sections"].getArray();
        if (sections && sections->size() > 32) warn ("Ignored presentation with more than 32 sections.");
        else if (sections)
        {
            std::set<juce::String> sectionIds;
            std::set<int> ccs;
            for (const auto& s : *sections)
            {
                Section section;
                section.id = string (s["id"]);
                section.label = string (s["label"]);
                const auto* controls = s["controls"].getArray();
                if (section.id.isEmpty() || ! sectionIds.insert (section.id).second
                    || ! controls || controls->size() > 128)
                { warn ("Ignored invalid/oversized section in " + preset.id); continue; }
                if (section.label.isEmpty()) section.label = section.id;
                for (const auto& c : *controls)
                {
                    const auto target = c["target"];
                    const auto n = target["number"];
                    if (string (target["type"]) != "cc" || ! integer (n)
                        || static_cast<juce::int64> (n) < 0 || static_cast<juce::int64> (n) > 127
                        || ! ccs.insert (static_cast<int> (n)).second)
                    { warn ("Ignored invalid or duplicate CC in " + preset.id); continue; }
                    Control control;
                    control.cc = static_cast<int> (n);
                    control.label = string (c["label"]);
                    const auto widget = string (c["widget"]);
                    if (widget == "knob" || widget == "slider" || widget == "toggle") control.widget = widget;
                    else if (widget.isNotEmpty()) warn ("Unknown widget uses knob in " + preset.id);
                    section.controls.push_back (std::move (control));
                }
                if (! section.controls.empty()) preset.sections.push_back (std::move (section));
            }
        }
        manifest.presets.push_back (std::move (preset));
    }
    out.manifest = std::move (manifest);
    return out;
}

std::pair<juce::String, juce::String> parseLegacy (const void* data, std::size_t size)
{
    if (! data || size == 0 || size > maxJsonBytes
        || ! sfzbundle::isValidUtf8 ({ static_cast<const std::uint8_t*> (data), size })
        || ! boundedJson (static_cast<const char*> (data), size)) return {};
    juce::var root;
    if (juce::JSON::parse (juce::String::fromUTF8 (static_cast<const char*> (data), static_cast<int> (size)), root).failed()
        || ! root.isObject()) return {};
    return { string (root["instrumentName"]), string (root["patchName"]) };
}

juce::MemoryBlock readLocal (const juce::File& root, const juce::String& path, std::size_t limit)
{
    if (! isPackagePath (path)) return {};
    std::error_code ec;
    const auto base = std::filesystem::canonical (std::filesystem::u8path (root.getFullPathName().toStdString()), ec);
    if (ec) return {};
    const auto file = std::filesystem::canonical (base / std::filesystem::u8path (path.toStdString()), ec);
    if (ec) return {};
    auto b = base.begin(), f = file.begin();
    for (; b != base.end(); ++b, ++f) if (f == file.end() || *b != *f) return {};
    const juce::File resolved (juce::String::fromUTF8 (file.u8string().c_str()));
    auto input = resolved.createInputStream();
    if (! input || input->getTotalLength() <= 0 || input->getTotalLength() > static_cast<juce::int64> (limit)) return {};
    juce::MemoryBlock bytes;
    input->readIntoMemoryBlock (bytes, static_cast<ssize_t> (limit));
    return bytes;
}

Result loadBeside (const juce::File& sfz)
{
    auto bytes = readLocal (sfz.getParentDirectory(), "instrument.json", maxJsonBytes);
    if (bytes.isEmpty())
    {
        Result unavailable;
        if (sfz.getSiblingFile ("instrument.json").exists())
            unavailable.diagnostics.add ("Sibling manifest is empty, unreadable, unsafe, or exceeds the byte limit.");
        return unavailable;
    }
    auto result = parse (bytes.getData(), bytes.getSize());
    if (result.manifest)
    {
        bool matched = false;
        for (const auto& preset : result.manifest->presets)
            matched = matched || preset.sfz == sfz.getFileName();
        if (! matched)
        {
            result.manifest.reset();
            result.diagnostics.add ("Sibling manifest does not describe the loaded SFZ.");
        }
    }
    return result;
}

juce::String imageMime (const void* data, std::size_t size)
{
    if (! data || size < 24 || size > maxAssetBytes) return {};
    const auto* b = static_cast<const std::uint8_t*> (data);
    auto dimensions = [] (std::uint32_t w, std::uint32_t h)
    { return w > 0 && h > 0 && w <= 4096 && h <= 4096 && std::uint64_t (w) * h <= 8388608; };
    const std::uint8_t png[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    if (std::memcmp (b, png, 8) == 0)
    {
        auto u32 = [&] (int at) { return (std::uint32_t (b[at]) << 24) | (std::uint32_t (b[at + 1]) << 16)
                                             | (std::uint32_t (b[at + 2]) << 8) | b[at + 3]; };
        if (size >= 33 && u32 (8) == 13 && std::memcmp (b + 12, "IHDR", 4) == 0
            && dimensions (u32 (16), u32 (20))) return "image/png";
        return {};
    }
    if (b[0] != 0xff || b[1] != 0xd8) return {};
    for (std::size_t at = 2; at + 4 <= size;)
    {
        if (b[at++] != 0xff) return {};
        while (at < size && b[at] == 0xff) ++at;
        if (at >= size) return {};
        const auto marker = b[at++];
        if (marker == 0xda || marker == 0xd9) return {};
        if (at + 2 > size) return {};
        const std::size_t length = std::size_t (b[at]) * 256 + b[at + 1];
        if (length < 2 || length > size - at) return {};
        // Baseline and progressive JPEG only.
        if (marker == 0xc0 || marker == 0xc2)
        {
            if (length >= 8 && dimensions (std::uint32_t (b[at + 5]) * 256 + b[at + 6],
                                           std::uint32_t (b[at + 3]) * 256 + b[at + 4])) return "image/jpeg";
            return {};
        }
        at += length;
    }
    return {};
}
}
