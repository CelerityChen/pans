#include <stdexcept>
#include <string>
#include <string_view>

#include <config/convert.h>

namespace pans::detail
{
    void ThrowConvertError(std::string_view type_name, std::string_view reason, std::string_view value)
    {
        std::string message;
        message.reserve(type_name.size() + reason.size() + value.size() + 32);
        message.append("ConfigConvert<").append(type_name).append(">: ").append(reason);
        if (!value.empty())
        {
            message.append(", value=").append(value);
        }
        throw std::runtime_error(message);
    }

} // namespace pans
