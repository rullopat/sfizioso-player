#include "PreferenceStore.h"

// Vendor/product directory under userApplicationDataDirectory holding the shared
// preferences.json. Defaults to "Sample Machine" so every Sample Machine product
// (autosampler + branded romplers) shares one prefs file, as before. The open
// standalone Sfizioso Player overrides this at compile time (e.g. -D...="Sfizioso")
// to keep its own prefs separate from the SM product family. (SMPL-79 Stage 2.)
#ifndef SAMPLEMACHINE_PREFS_VENDOR_DIR
#define SAMPLEMACHINE_PREFS_VENDOR_DIR "Sample Machine"
#endif

namespace samplemachine
{
namespace PreferenceStore
{

namespace
{

juce::File getPreferencesFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile (SAMPLEMACHINE_PREFS_VENDOR_DIR)
            .getChildFile ("preferences.json");
}

juce::var loadAll()
{
    const auto file = getPreferencesFile();
    if (! file.existsAsFile())
        return juce::var (new juce::DynamicObject());

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (! parsed.isObject())
        return juce::var (new juce::DynamicObject());

    return parsed;
}

void saveAll (const juce::var& data)
{
    const auto file = getPreferencesFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (data, true));
}

} // namespace

juce::String get (const juce::String& key, const juce::String& defaultValue)
{
    const auto data = loadAll();
    if (auto* obj = data.getDynamicObject())
    {
        if (obj->hasProperty (key))
            return obj->getProperty (key).toString();
    }
    return defaultValue;
}

void set (const juce::String& key, const juce::String& value)
{
    auto data = loadAll();
    auto* obj = data.getDynamicObject();
    if (obj == nullptr)
    {
        obj = new juce::DynamicObject();
        data = juce::var (obj);
    }
    obj->setProperty (key, value);
    saveAll (data);
}

} // namespace PreferenceStore
} // namespace samplemachine
