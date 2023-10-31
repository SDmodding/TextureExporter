#pragma once

struct DDSheader_t
{
	uint32_t m_Header[3] = { 0x20534444, 0x7C, 0x81007 };
	uint32_t m_Height;
	uint32_t m_Width;
	uint32_t m_Pitch;
	uint32_t m_Dummy1[13] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	uint32_t m_Size; // Structure Size
	uint32_t m_Flags;
	uint32_t m_Format;
	uint32_t m_BitCount;
	uint32_t m_RMask;
	uint32_t m_GMask;
	uint32_t m_BMask;
	uint32_t m_AMask;
	uint32_t m_Caps;
	uint32_t m_Dummy2[4] = { 0, 0, 0, 0 };
};