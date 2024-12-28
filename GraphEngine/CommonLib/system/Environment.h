#pragma once

namespace CommonLib {
    namespace system {

        class CEnvironment {
        public:
            static void SetVariable(const astr &varName, const astr &value);
            static void RemoveVariable(const astr& varName);
        };

        typedef std::pair<std::string, std::string> TEnvValue;

        class CEnvValue
        {
        public:
            CEnvValue(const std::string& name, const std::string& value);
            const std::string& GetName() const;
            const std::string& GetValue() const;

            explicit operator const char*() const;
        private:
            const std::string m_name;
            const std::string m_value;
            std::string  m_systemParam;

        };


    }
}
