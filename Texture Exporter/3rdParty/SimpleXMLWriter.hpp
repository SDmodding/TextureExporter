#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <unordered_map>
#include <map>

class CXMLWriterNode
{
public:
    std::string Key;
    std::string Value;
    int Depth = 0;

    std::map<std::string, CXMLWriterNode> Nodes;
    std::unordered_map<std::string, std::string> Attributes;

    void RemoveIllegalChars(std::string& m_String)
    {
        static std::unordered_map<char, std::string> m_CharsMap = { { '<', "&lt;" }, { '>', "&gt;" }, { '&', "&amp;" }, { '\'', "&apos;" }, { '\"', "&quot;" } };

        size_t m_Pos = 0;
        while (m_String.size() > m_Pos) 
        {
            auto m_Itter = m_CharsMap.find(m_String[m_Pos]);
            if (m_Itter != m_CharsMap.end())
            {
                m_String.replace(m_Pos, 1, m_Itter->second);
                m_Pos += m_Itter->second.size();
            }
            else
               ++m_Pos;
        }
    }

    CXMLWriterNode& operator=(const char* m_Value)
    {
        Value = m_Value;
        RemoveIllegalChars(Value);
        return *this;
    }

    CXMLWriterNode& operator[](const char* m_Key)
    {
        CXMLWriterNode& m_Node = Nodes[m_Key];
        m_Node.Key      = m_Key;
        m_Node.Depth    = Depth + 1;

        return m_Node;
    }

    CXMLWriterNode& operator()(const char* m_AttributeKey, const char* m_AttributeValue)
    {
        Attributes[m_AttributeKey] = m_AttributeValue;
        RemoveIllegalChars(Attributes[m_AttributeKey]);
        return *this;
    }

    std::string GetKey()
    {
        size_t m_HashTag = Key.find_first_of("##");
        if (m_HashTag == std::string::npos)
            return Key;

        return Key.substr(0, m_HashTag);
    }

    std::string GetOpenTag()
    {
        std::string m_String = "<" + GetKey();

        for (auto& m_Attribute : Attributes)
            m_String += " " + m_Attribute.first + "=\"" + m_Attribute.second + "\"";

        return (m_String + ">");
    }

    std::string GetCloseTag() { return ("</" + GetKey() + ">");  }
};

class CXMLWriter
{
public:
    CXMLWriterNode m_Node;
    CXMLWriterNode& operator[](const char* m_Key) { return m_Node[m_Key]; }

    bool m_InsertHeader = true;
    std::string m_Version   = "1.0";
    std::string m_Encoding  = "UTF-8";
    std::string GetHeader() { return "<?xml version=\"" + m_Version + "\" encoding=\"" + m_Encoding + "\"?>\n"; }

    void BuildString(std::string& m_String, CXMLWriterNode& m_Node)
    {
        for (auto& m_Nodes : m_Node.Nodes)
        {
            CXMLWriterNode& m_NextNode = m_Nodes.second;

            std::string m_Tabs(m_NextNode.Depth - 1, '\t');

            std::string m_OpenTag = m_NextNode.GetOpenTag();
            std::string m_CloseTag = m_NextNode.GetCloseTag();

            if (m_NextNode.Nodes.empty())
            {
                if (m_NextNode.Value.empty())
                {
                    m_OpenTag.pop_back();
                    m_String += m_Tabs + m_OpenTag + "/>\n";
                }
                else
                    m_String += m_Tabs + m_OpenTag + m_NextNode.Value + m_CloseTag + "\n";
            }
            else
            {
                m_String += m_Tabs + m_OpenTag + "\n";
                if (!m_NextNode.Value.empty())
                    m_String += m_Tabs + "\t" + m_NextNode.Value + "\n";

                BuildString(m_String, m_NextNode);
                m_String += m_Tabs + m_CloseTag + "\n";
            }
        }
    }

    operator std::string()
    {
        std::string m_String = (m_InsertHeader ? GetHeader() : "");
        BuildString(m_String, m_Node);

        return m_String;
    }

    std::string GetString() { return operator std::string(); }

    bool OutputFile(const char* m_Path)
    {
        FILE* m_File = fopen(m_Path, "w");
        if (!m_File)
            return false;

        std::string m_String = GetString();
        fwrite(&m_String[0], sizeof(char), m_String.size(), m_File);
        fclose(m_File);

        return true;
    }
};
