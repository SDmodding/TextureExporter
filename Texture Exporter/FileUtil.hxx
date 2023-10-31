#pragma once

struct File_t
{
	uint8_t* m_Data;
	size_t m_Size;

	~File_t()
	{
		if (m_Data)
		{
			delete[] m_Data;
			m_Data = nullptr;
		}
	}

	long Read(const char* p_Path)
	{
		FILE* m_File = fopen(p_Path, "rb");
		if (!m_File)
			return ERROR_FILE_NOT_FOUND;

		fseek(m_File, 0, SEEK_END);

		m_Size = static_cast<size_t>(ftell(m_File));
		m_Data = new uint8_t[m_Size];

		fseek(m_File, 0, SEEK_SET);

		if (fread(m_Data, sizeof(uint8_t), m_Size, m_File) != m_Size)
		{
			fclose(m_File);
			return ERROR_READ_FAULT;
		}

		fclose(m_File);
		return ERROR_SUCCESS;
	}

	long Write(const char* p_Path, uint8_t* p_Data, size_t p_Size)
	{
		FILE* m_File = fopen(p_Path, "wb");
		if (!m_File)
			return ERROR_FILE_NOT_FOUND;

		if (fwrite(p_Data, sizeof(uint8_t), p_Size, m_File) != p_Size)
		{
			fclose(m_File);
			return ERROR_WRITE_FAULT;
		}

		fclose(m_File);
		return ERROR_SUCCESS;
	}
};