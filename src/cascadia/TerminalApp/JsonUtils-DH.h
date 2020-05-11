/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- JsonUtils.h

Abstract:
- Helpers for the TerminalApp project
Author(s):
- Mike Griese - August 2019

--*/
#pragma once

#include <json.h>

#include "../types/inc/utils.hpp"

namespace winrt
{
    struct guid;
}

namespace TerminalApp::JsonUtils
{
    namespace Detail
    {
        __declspec(noinline) inline const std::string_view GetStringView(const Json::Value& json)
        {
            const char* begin{ nullptr };
            const char* end{ nullptr };
            json.getString(&begin, &end);
            const std::string_view zeroCopyString{ begin, gsl::narrow_cast<size_t>(end - begin) };
            return zeroCopyString;
        }
    }

    class TypeMismatchException : public std::runtime_error
    {
    public:
        TypeMismatchException() :
            runtime_error("invalid type") {}
    };

    class KeyedException : public std::runtime_error
    {
    public:
        KeyedException(const std::string_view key, const std::exception& exception) :
            runtime_error(fmt::format("error parsing \"{0}\": {1}", key, exception.what()).c_str()) {}
    };

    template<typename T>
    struct ConversionTrait
    {
        // FromJson, ToJson are not defined so as to cause a compile error (which forces a specialization)
    };

    template<>
    struct ConversionTrait<std::string>
    {
        static std::string FromJson(const Json::Value& json)
        {
            return json.asString();
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }
    };

    template<>
    struct ConversionTrait<std::wstring>
    {
        static std::wstring FromJson(const Json::Value& json)
        {
            return til::u8u16(Detail::GetStringView(json));
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }
    };

    template<>
    struct ConversionTrait<bool>
    {
        static bool FromJson(const Json::Value& json)
        {
            return json.asBool();
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isBool();
        }
    };

    template<>
    struct ConversionTrait<int>
    {
        static int FromJson(const Json::Value& json)
        {
            return json.asInt();
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isInt();
        }
    };

    template<>
    struct ConversionTrait<unsigned int>
    {
        static unsigned int FromJson(const Json::Value& json)
        {
            return json.asUInt();
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isInt();
        }
    };

    template<>
    struct ConversionTrait<float>
    {
        static float FromJson(const Json::Value& json)
        {
            return json.asFloat();
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isNumeric();
        }
    };

    template<>
    struct ConversionTrait<double>
    {
        static double FromJson(const Json::Value& json)
        {
            return json.asDouble();
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isNumeric();
        }
    };

    template<>
    struct ConversionTrait<GUID>
    {
        static GUID FromJson(const Json::Value& json)
        {
            return ::Microsoft::Console::Utils::GuidFromString(Detail::GetStringView(json));
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }
    };

    // (GUID and winrt::guid are mutually convertible!)
    template<>
    struct ConversionTrait<winrt::guid> : public ConversionTrait<GUID>
    {
    };

    template<>
    struct ConversionTrait<til::color>
    {
        static til::color FromJson(const Json::Value& json)
        {
            return ::Microsoft::Console::Utils::ColorFromHexString(Detail::GetStringView(json));
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }
    };

    template<typename T, typename TBase>
    struct KeyValueMapper
    {
        using pair_type = std::pair<std::string_view, T>;
        static T FromJson(const Json::Value& json)
        {
            const auto name{ Detail::GetStringView(json) };
            for (const auto& pair : TBase::mappings)
            {
                if (pair.first == name)
                {
                    return pair.second;
                }
            }
            // the first mapping is the "Default"
            return TBase::mappings[0].second;
        }

        static bool CanConvert(const Json::Value& json)
        {
            return json.isString();
        }
    };

    // Method Description:
    // - Helper that will populate a reference with a value converted from a json object.
    // Arguments:
    // - json: the json object to convert
    // - target: the value to populate with the converted result
    // Return Value:
    // - a boolean indicating whether the value existed (in this case, was non-null)
    template<typename T>
    bool GetValue(const Json::Value& json, T& target)
    {
        if (json)
        {
            if (!ConversionTrait<T>::CanConvert(json))
            {
                throw TypeMismatchException{};
            }

            target = ConversionTrait<T>::FromJson(json);
            return true;
        }
        return false;
    }

    // Method Description:
    // - Overload on GetValue that will populate a std::optional with a value converted from json
    //    - If the json value doesn't exist we'll leave the target object unmodified.
    //    - If the json object is set to `null`, then
    //      we'll instead set the target back to nullopt.
    // Arguments:
    // - json: the json object to convert
    // - target: the value to populate with the converted result
    // Return Value:
    // - a boolean indicating whether the optional was changed
    template<typename TOpt>
    bool GetValue(const Json::Value& json, std::optional<TOpt>& target)
    {
        if (json.isNull())
        {
            target = std::nullopt;
            return true; // null is valid for optionals
        }

        TOpt local{};
        if (GetValue(json, local))
        {
            target = std::move(local);
            return true;
        }
        return false;
    }

    template<typename T>
    bool GetValueForKey(const Json::Value& json, std::string_view key, T& target)
    {
        if (auto found{ json.find(&*key.cbegin(), (&*key.cbegin()) + key.size()) })
        {
            try
            {
                return GetValue(*found, target);
            }
            catch (const std::exception& e)
            {
                // WRAP! WRAP LIKE YOUR LIFE DEPENDS ON IT!
                throw KeyedException(key, e);
            }
        }
        return false;
    }

    template<typename T>
    void GetRequiredValueForKey(const Json::Value& json, std::string_view key, T& target)
    {
        if (!GetValueForKey(json, key, target))
        {
            THROW_HR(E_UNEXPECTED);
        }
    }

    // THIS may be useful?
    constexpr void GetValuesForKeys(const Json::Value& /*json*/) {}

    template<typename T, typename... Args>
    void GetValuesForKeys(const Json::Value& json, std::string_view key1, T&& val1, Args&&... args)
    {
        GetValueForKey(json, key1, val1);
        GetValuesForKeys(json, std::forward<Args>(args)...);
    }
};
