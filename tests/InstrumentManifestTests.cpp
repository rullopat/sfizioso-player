#include <instrument_manifest/Manifest.h>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <string>
using namespace sfizioso_manifest;
namespace {
const std::string valid = R"({"format":"sfizioso.instrument-manifest","formatVersion":1,
"instrument":{"name":"Example"},"presets":[{"id":"one","name":"One","sfz":"one.sfz",
"artwork":{"controlsBackground":"artwork/background.png"},
"presentation":{"accent":"#f05a28","sections":[{"id":"tone","label":"Tone","controls":[
{"target":{"type":"cc","number":74},"widget":"slider"},
{"target":{"type":"cc","number":1},"label":"Vibrato","widget":"toggle"}]}]}}]})";
Result parseText (const std::string& text) { return parse (text.data(), text.size()); }
std::string replace (std::string text, const std::string& from, const std::string& to)
{ text.replace (text.find (from), from.size(), to); return text; }
struct Temp {
    juce::File root = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("manifest-tests", {}, false);
    Temp() { REQUIRE (root.createDirectory().wasOk()); }
    ~Temp() { root.deleteRecursively(); }
};
}
TEST_CASE ("Manifest reads identity and ordered semantic CC presentation", "[manifest]")
{
    const auto result = parseText (valid);
    REQUIRE (result.manifest);
    CHECK (result.diagnostics.isEmpty());
    CHECK (result.manifest->name == "Example");
    const auto& preset = result.manifest->presets.front();
    CHECK (preset.background == "artwork/background.png");
    REQUIRE (preset.sections.size() == 1);
    REQUIRE (preset.sections[0].controls.size() == 2);
    CHECK (preset.sections[0].controls[0].cc == 74);
    CHECK (preset.sections[0].controls[0].widget == "slider");
    CHECK (preset.sections[0].controls[1].label == "Vibrato");
    CHECK (preset.sections[0].controls[1].widget == "toggle");
    CHECK (parseText (replace (valid, "\"formatVersion\":1", "\"extra\":42,\"formatVersion\":1")).manifest.has_value());
}
TEST_CASE ("Manifest rejects invalid core and unsafe package paths", "[manifest]")
{
    for (const auto& text : { std::string ("[]"), std::string ("{bad"),
        replace (valid, "\"formatVersion\":1", "\"formatVersion\":2"),
        replace (valid, "\"formatVersion\":1", "\"formatVersion\":\"1\""),
        replace (valid, "\"name\":\"Example\"", "\"name\":false"),
        replace (valid, "one.sfz", "../one.sfz") })
    {
        const auto result = parseText (text);
        CHECK_FALSE (result.manifest);
        CHECK_FALSE (result.diagnostics.isEmpty());
    }
    for (const auto* path : { "", "../a", "/a", "C:/a", "https://a", "a/../b", "a//b", "./a", "a\\b", "a%2fb", "a?x", "a#b" })
    { INFO (path); CHECK_FALSE (isPackagePath (path)); }
    CHECK (isPackagePath (juce::String::fromUTF8 ("artwork/Żółty obraz.png")));
}
TEST_CASE ("Bad presentation falls back locally without losing valid controls", "[manifest]")
{
    const auto result = parseText (replace (replace (replace (valid,
        "\"number\":74", "\"number\":128"), "#f05a28", "red"), "artwork/background.png", "../background.png"));
    REQUIRE (result.manifest);
    CHECK (result.diagnostics.size() == 3);
    const auto& preset = result.manifest->presets.front();
    CHECK (preset.background.isEmpty());
    CHECK (preset.accent.isEmpty());
    REQUIRE (preset.sections.size() == 1);
    REQUIRE (preset.sections[0].controls.size() == 1);
    CHECK (preset.sections[0].controls[0].cc == 1);
    auto duplicate = parseText (replace (valid, "\"number\":74", "\"number\":1"));
    REQUIRE (duplicate.manifest);
    CHECK (duplicate.manifest->presets[0].sections[0].controls.size() == 1);
}
TEST_CASE ("Manifest parser bounds bytes nesting and invalid encodings before JSON parsing", "[manifest]")
{
    CHECK_FALSE (parseText (std::string (maxJsonBytes + 1, ' ')).manifest);
    CHECK_FALSE (parseText (std::string (33, '[') + std::string (33, ']')).manifest);
    CHECK_FALSE (parseText (valid + std::string (1, '\0')).manifest);
    CHECK_FALSE (parseText (std::string ("\xc3\x28", 2)).manifest);
    CHECK (parseLegacy ("{bad", 4).first.isEmpty());
    const std::string legacy = R"({"instrumentName":"Old instrument","patchName":"Old preset"})";
    CHECK (parseLegacy (legacy.data(), legacy.size()).first == "Old instrument");
}
TEST_CASE ("Loose manifest must describe the selected sibling and stay inside its directory", "[manifest][disk]")
{
    Temp temp;
    const auto sfz = temp.root.getChildFile ("one.sfz");
    REQUIRE (sfz.replaceWithText ("<region> sample=*sine"));
    CHECK_FALSE (loadBeside (sfz).manifest);
    REQUIRE (temp.root.getChildFile ("instrument.json").replaceWithText (valid));
    CHECK (loadBeside (sfz).manifest.has_value());
    CHECK_FALSE (loadBeside (temp.root.getChildFile ("other.sfz")).manifest);
    CHECK (readLocal (temp.root, "../outside", 100).isEmpty());
    CHECK (readLocal (temp.root, "instrument.json", 2).isEmpty());
    Temp outside;
    REQUIRE (outside.root.getChildFile ("secret.png").replaceWithText ("outside"));
    std::error_code ec;
    std::filesystem::create_directory_symlink (
        std::filesystem::u8path (outside.root.getFullPathName().toStdString()),
        std::filesystem::u8path (temp.root.getChildFile ("escape").getFullPathName().toStdString()), ec);
    if (! ec) CHECK (readLocal (temp.root, "escape/secret.png", 100).isEmpty());
    else WARN ("Symlink creation unavailable on this platform; other path tests still ran.");
}
TEST_CASE ("Artwork checks signature and dimensions before browser decoding", "[manifest][artwork]")
{
    std::vector<unsigned char> png {137,80,78,71,13,10,26,10, 0,0,0,13, 'I','H','D','R', 0,0,0,1, 0,0,0,1};
    png.resize (33);
    CHECK (imageMime (png.data(), png.size()) == "image/png");
    png[16] = 1;
    CHECK (imageMime (png.data(), png.size()).isEmpty());
    CHECK (imageMime ("<svg onload='alert(1)'/>", 24).isEmpty());
    CHECK (imageMime (png.data(), maxAssetBytes + 1).isEmpty());
    const unsigned char jpeg[] {255,216,255,192,0,17,8,0,16,0,32,3,1,17,0,2,17,0,3,17,0,255,217,0};
    CHECK (imageMime (jpeg, sizeof jpeg) == "image/jpeg");
}
TEST_CASE ("Manifest rejects duplicate catalogue identities and bounds sections", "[manifest]")
{
    auto root = juce::JSON::parse (valid);
    auto* presets = root.getDynamicObject()->getProperty ("presets").getArray();
    REQUIRE (presets);
    presets->add ((*presets)[0].clone());
    const auto duplicateText = juce::JSON::toString (root).toStdString();
    CHECK_FALSE (parseText (duplicateText).manifest);
    presets->getReference (1).getDynamicObject()->setProperty ("id", "two");
    CHECK_FALSE (parseText (juce::JSON::toString (root).toStdString()).manifest); // duplicate SFZ path
    presets->remove (1);
    auto& preset = presets->getReference (0);
    auto* sections = preset.getDynamicObject()->getProperty ("presentation").getDynamicObject()->getProperty ("sections").getArray();
    REQUIRE (sections);
    const auto original = (*sections)[0].clone();
    while (sections->size() < 33) sections->add (original.clone());
    const auto oversized = parseText (juce::JSON::toString (root).toStdString());
    REQUIRE (oversized.manifest);
    CHECK (oversized.manifest->presets[0].sections.empty());
    CHECK_FALSE (oversized.diagnostics.isEmpty());
}
