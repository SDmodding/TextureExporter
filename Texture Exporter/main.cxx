#define _CRT_SECURE_NO_WARNINGS
#include <algorithm>
#include <iostream>
#include <Windows.h>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <d3d11.h>
#include <string>
#pragma comment(lib, "d3d11")

#include "DDSHeader.hxx"
#include "FileUtil.hxx"

// Defines
#define PROJECT_VERSION		"v1.0.2"
#define PROJECT_NAME		"Texture Exporter " PROJECT_VERSION

#define ILLUSION_TEXTURE_PC64		0xCDBFA090

// SDK Stuff
#define UFG_MAX(a, b) max(a, b)
#define UFG_PAD_INSERT(x, y) x ## y
#define UFG_PAD_DEFINE(x, y) UFG_PAD_INSERT(x, y)
#define UFG_PAD(size) char UFG_PAD_DEFINE(padding_, __LINE__)[size] = { 0x0 }
#include <SDK/Optional/PermFile/.Includes.hpp>
#include <SDK/Optional/StringHash.hpp>

// DDSTextureLoader
#include "3rdParty/DDSTextureLoader11.h"

// DirectXTex
#include "3rdParty/DirectXTex/DirectXTex.h"

// XML
#include "3rdParty/tinyxml2.h"

struct DirectX_t
{
	ID3D11Device* m_Device = nullptr;
	ID3D11DeviceContext* m_DeviceCtx = nullptr;
	D3D_FEATURE_LEVEL m_FeatureLevel;

	HRESULT CreateDevice()
	{
		D3D_FEATURE_LEVEL m_FeatureLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
		HRESULT m_Res = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, 0, m_FeatureLevels, ARRAYSIZE(m_FeatureLevels), D3D11_SDK_VERSION, &m_Device, &m_FeatureLevel, &m_DeviceCtx);
		if (m_Res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
			m_Res = D3D11CreateDevice(0, D3D_DRIVER_TYPE_WARP, 0, 0, m_FeatureLevels, ARRAYSIZE(m_FeatureLevels), D3D11_SDK_VERSION, &m_Device, &m_FeatureLevel, &m_DeviceCtx);

		return m_Res;
	}
};
DirectX_t g_DirectX;

namespace Helper
{
	std::string GetPermFilePath()
	{
		OPENFILENAMEA m_OpenFileName = { 0 };
		ZeroMemory(&m_OpenFileName, sizeof(OPENFILENAMEA));

		m_OpenFileName.lStructSize = sizeof(OPENFILENAMEA);

		static char m_FilePath[MAX_PATH] = { '\0' };
		m_OpenFileName.lpstrFile = m_FilePath;
		m_OpenFileName.nMaxFile = sizeof(m_FilePath);
		m_OpenFileName.lpstrTitle = "Select Perm File";
		m_OpenFileName.lpstrFilter = "(Perm File)\0*.bin\0";
		m_OpenFileName.Flags = (OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST);

		if (GetOpenFileNameA(&m_OpenFileName) == 0)
			m_OpenFileName.lpstrFile[0] = '\0';

		return m_OpenFileName.lpstrFile;
	}

	namespace Texture
	{
		void GetWidthAndHeight(Illusion::Texture_t* p_Texture, uint32_t* p_Width, uint32_t* p_Height)
		{
			*p_Width	= static_cast<uint32_t>(p_Texture->m_Width);
			*p_Height	= static_cast<uint32_t>(p_Texture->m_Height);
		}

		uint32_t CalculatePitch(Illusion::Texture_t* p_Texture, uint32_t p_Width, uint32_t p_Height)
		{
			uint32_t m_Pitch = ((p_Width + 3) / 4) * ((p_Height + 3) / 4) * ((32 + 3) / 4);

			switch (p_Texture->m_Format)
			{
				case Illusion::FORMAT_DXT1:
					m_Pitch *= 1; break;
				case Illusion::FORMAT_DXN: case Illusion::FORMAT_DXT5:
					m_Pitch *= 2; break;
				default:
					m_Pitch *= 8; break;
			}

			return m_Pitch;
		}

		void GetDDSHeader(Illusion::Texture_t* p_Texture, DDSheader_t* p_DDSHeader)
		{
			GetWidthAndHeight(p_Texture, &p_DDSHeader->m_Width, &p_DDSHeader->m_Height);
			p_DDSHeader->m_Pitch	= CalculatePitch(p_Texture, p_DDSHeader->m_Width, p_DDSHeader->m_Height);
			p_DDSHeader->m_Size		= 32;
			p_DDSHeader->m_Flags	= 0;

			// Flags
			switch (p_Texture->m_Format)
			{
				case Illusion::FORMAT_A8R8G8B8:
				case Illusion::FORMAT_X8:
					p_DDSHeader->m_Flags |= 0x40; break;
				case Illusion::FORMAT_DXT1: case Illusion::FORMAT_DXT3: case Illusion::FORMAT_DXT5:
				case Illusion::FORMAT_DXN:
					p_DDSHeader->m_Flags |= 0x4; break;
			}

			// Set Format
			switch (p_Texture->m_Format)
			{
				default: p_DDSHeader->m_Format = 0; break;
				case Illusion::FORMAT_DXT1: p_DDSHeader->m_Format = MAKEFOURCC('D', 'X', 'T', '1'); break;
				case Illusion::FORMAT_DXT3: p_DDSHeader->m_Format = MAKEFOURCC('D', 'X', 'T', '3'); break;
				case Illusion::FORMAT_DXT5: p_DDSHeader->m_Format = MAKEFOURCC('D', 'X', 'T', '5'); break;
				case Illusion::FORMAT_DXN: p_DDSHeader->m_Format = MAKEFOURCC('A', 'T', 'I', '2'); break;
			}

			p_DDSHeader->m_BitCount = 32;

			// RGBA Mask
			switch (p_Texture->m_Format)
			{
				case Illusion::FORMAT_A8R8G8B8:
				case Illusion::FORMAT_X8:
				{
					p_DDSHeader->m_RMask = 0x000000FF;
					p_DDSHeader->m_GMask = 0x0000FF00;
					p_DDSHeader->m_BMask = 0x00FF0000;
					p_DDSHeader->m_AMask = 0xFF000000;
				}
				break;
				case Illusion::FORMAT_DXT1: case Illusion::FORMAT_DXT3: case Illusion::FORMAT_DXT5:
				case Illusion::FORMAT_DXN:
				{
					p_DDSHeader->m_RMask = 0xC5FBE8;
					p_DDSHeader->m_GMask = 0x10745D6;
					p_DDSHeader->m_BMask = 0x108C1C0;
					p_DDSHeader->m_AMask = 0xC5FBF4;
				}
				break;
				default:
				{
					p_DDSHeader->m_RMask = 0x0;
					p_DDSHeader->m_GMask = 0x0;
					p_DDSHeader->m_BMask = 0x0;
					p_DDSHeader->m_AMask = 0x0;
				}
				break;
			}

			if (p_Texture->m_NumMipMaps)
				p_DDSHeader->m_Caps = 0x401008; // texture w/ mipmaps
			else
				p_DDSHeader->m_Caps = 0x1000; // texture w/o mipmaps
		}

		const char* GetFormat(Illusion::eTextureFormat p_Format)
		{
			switch (p_Format)
			{
			case Illusion::FORMAT_A8R8G8B8:
				return "A8R8G8B8";
			case Illusion::FORMAT_DXT1:
				return "DXT1";
			case Illusion::FORMAT_DXT3:
				return "DXT3";
			case Illusion::FORMAT_DXT5:
				return "DXT5";
			case Illusion::FORMAT_R5G6B5:
				return "R5G6B5";
			case Illusion::FORMAT_A1R5G5B5:
				return "A1R5G5B5";
			case Illusion::FORMAT_X8:
				return "X8";
			case Illusion::FORMAT_X16:
				return "X16";
			case Illusion::FORMAT_CXT1:
				return "CXT1";
			case Illusion::FORMAT_DXN:
				return "DXN";
			case Illusion::FORMAT_BC6H_UF16:
				return "BC6H_UF16";
			case Illusion::FORMAT_BC6H_SF16:
				return "BC6H_SF16";
			case Illusion::FORMAT_BC7_UNORM:
				return "BC7_UNORM";
			case Illusion::FORMAT_BC7_UNORM_SRGB:
				return "BC7_UNORM_SRGB";
			case Illusion::FORMAT_R32F:
				return "R32F";
			case Illusion::FORMAT_X16FY16FZ16FW16F:
				return "X16FY16FZ16FW16F";
			case Illusion::FORMAT_D24S8:
				return "D24S8";
			case Illusion::FORMAT_D24FS8:
				return "D24FS8";
			case Illusion::FORMAT_SHADOW:
				return "SHADOW";
			case Illusion::FORMAT_DEPTHCOPY:
				return "DEPTHCOPY";
			case Illusion::FORMAT_A2R10G10B10:
				return "A2R10G10B10";
			case Illusion::FORMAT_A2R10G10B10F:
				return "A2R10G10B10F";
			case Illusion::FORMAT_A16B16G16R16:
				return "A16B16G16R16";
			}

			return nullptr;
		}

		const char* GetAlphaState(uint32_t p_AlphaStateUID)
		{
			switch (p_AlphaStateUID)
			{
			case 0x2782CCE6:
				return "None";
			case 0xA3833FDE:
				return "Blend";
			case 0x69DAE2D1:
				return "Additive";
			case 0x2B068C0A:
				return "PunchThru";
			case 0xFAB11CA1:
				return "Premultiplied";
			case 0xEDE83382:
				return "Overlay";
			case 0x6EBDEDA0:
				return "ModulatedRGBSrcAlpha";
			case 0xD668AB18:
				return "Screen";
			}

			return nullptr;
		}

		const char* GetFilter(Illusion::eTextureFilter p_Filter)
		{
			switch (p_Filter)
			{
			case Illusion::FILTER_LINEAR:
				return "Linear";
			case Illusion::FILTER_POINT:
				return "Point";
			case Illusion::FILTER_ANISOTROPIC:
				return "Anisotropic";
			case Illusion::FILTER_CONVOLUTION:
				return "Convolution";
			}

			return nullptr;
		}
		
		const char* GetAniso(Illusion::eTextureAniso p_Aniso)
		{
			switch (p_Aniso)
			{
			case Illusion::ANISO_X2:
				return "x2";
			case Illusion::ANISO_X4:
				return "x4";
			case Illusion::ANISO_X6:
				return "x6";
			case Illusion::ANISO_X8:
				return "x8";
			case Illusion::ANISO_X10:
				return "x10";
			case Illusion::ANISO_X12:
				return "x12";
			case Illusion::ANISO_X16:
				return "x16";
			}

			return nullptr;
		}


		bool ExportToPNG(Illusion::Texture_t* p_Texture, uint8_t* p_Data, const char* p_FilePath)
		{
			DDSheader_t m_DDSHeader;
			GetDDSHeader(p_Texture, &m_DDSHeader);

			uint32_t m_DataByteSize = p_Texture->m_ImageDataByteSize;
			uint64_t m_DataPosition = p_Texture->m_ImageDataPosition;

			size_t m_DDSSize = (sizeof(DDSheader_t) + m_DataByteSize);
			uint8_t* m_DDSData = new uint8_t[m_DDSSize];

			memcpy(m_DDSData, &m_DDSHeader, sizeof(DDSheader_t));
			memcpy(&m_DDSData[sizeof(DDSheader_t)], &p_Data[m_DataPosition], m_DataByteSize);

			ID3D11Resource* m_DDSTexture = nullptr;
			ID3D11ShaderResourceView* m_DDSResourceView = nullptr;
			if (DirectX::CreateDDSTextureFromMemory(g_DirectX.m_Device, m_DDSData, m_DDSSize, &m_DDSTexture, &m_DDSResourceView) == S_OK)
			{
				bool m_SuccessfullyExported = false;

				DirectX::ScratchImage m_DDSScratchImage;
				if (DirectX::CaptureTexture(g_DirectX.m_Device, g_DirectX.m_DeviceCtx, m_DDSTexture, m_DDSScratchImage) == S_OK)
				{
					DirectX::ScratchImage* m_DDSScratchImagePtr = &m_DDSScratchImage;
					const DirectX::Image* m_DDSImage = m_DDSScratchImage.GetImage(0, 0, 0);

					bool m_IsCompressed = DirectX::IsCompressed(m_DDSImage->format);
					bool m_DecompressValid = false;

					if (m_IsCompressed)
					{
						m_DDSScratchImagePtr = new DirectX::ScratchImage;
						if (DirectX::Decompress(*m_DDSImage, DXGI_FORMAT_R8G8B8A8_UNORM, *m_DDSScratchImagePtr) == S_OK)
							m_DecompressValid = true;
					}

					if (!m_IsCompressed || m_DecompressValid)
					{
						wchar_t m_ImagePath[MAX_PATH];
						swprintf_s(m_ImagePath, ARRAYSIZE(m_ImagePath), L"%hs", p_FilePath);

						if (DirectX::SaveToWICFile(*m_DDSScratchImagePtr->GetImage(0, 0, 0), DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), m_ImagePath) == S_OK)
							m_SuccessfullyExported = true;
					}

					if (m_IsCompressed)
						delete m_DDSScratchImagePtr;
				}

				m_DDSTexture->Release();
				m_DDSResourceView->Release();

				delete[] m_DDSData;

				if (m_SuccessfullyExported)
					return true;
			}

			return false;
		}
	}
}

int g_Argc = 0;
char** g_Argv = nullptr;
void InitArgParam(int p_Argc, char** p_Argv)
{
	g_Argc = p_Argc;
	g_Argv = p_Argv;
}

std::string GetArgParam(const char* p_Arg)
{
	for (int i = 0; g_Argc > i; ++i)
	{
		if (_stricmp(p_Arg, g_Argv[i]) != 0)
			continue;

		int m_ParamIndex = (i + 1);
		if (m_ParamIndex >= g_Argc)
			break;

		return g_Argv[m_ParamIndex];
	}

	return "";
}

bool HasArgSet(const char* p_Arg)
{
	for (int i = 0; g_Argc > i; ++i)
	{
		if (_stricmp(p_Arg, g_Argv[i]) == 0)
			return true;
	}

	return false;
}

void ReturnKeyWait()
{
	if (HasArgSet("-silent"))
		return;

	int m_Dummy = getchar();
}

int main(int p_Argc, char** p_Argv)
{
#ifdef _DEBUG
	if (!IsDebuggerPresent())
		int m_DebugKey = getchar();
#endif

	char m_CurrentDirectory[MAX_PATH] = { '\0' };
	GetCurrentDirectoryA(sizeof(m_CurrentDirectory), m_CurrentDirectory);

	SetConsoleTitleA(PROJECT_NAME);
	InitArgParam(p_Argc, p_Argv);

	if (FAILED(g_DirectX.CreateDevice()))
	{
		printf("[ ! ] Failed to initialize D3D11 Device.\n"); ReturnKeyWait();
		return 1;
	}

	HRESULT m_CoRes = CoInitializeEx(0, COINIT_MULTITHREADED);
	if (FAILED(m_CoRes))
		printf("[ WARNING ] Failed to initialize COM.\n");

	std::string m_PermPath	= GetArgParam("-perm");
	std::string m_OutputDir = GetArgParam("-outdir");

	if (m_PermPath.empty())
	{
		printf("[ ! ] No '-perm' defined, opening file explorer to select file.\n");
		m_PermPath = Helper::GetPermFilePath();

		if (m_PermPath.empty())
		{
			printf("[ ERROR ] You didn't select perm file!\n"); ReturnKeyWait();
			return 1;
		}
	}

	size_t m_PermPos = m_PermPath.find(".perm.bin");
	if (m_PermPos == std::string::npos)
	{
		printf("[ ERROR ] This is not perm file!\n"); ReturnKeyWait();
		return 1;
	}

	SDK::PermFile_t m_PermFile;
	if (!m_PermFile.LoadFile(&m_PermPath[0]))
	{
		printf("[ ERROR ] Failed to load perm file!\n"); ReturnKeyWait();
		return 1;
	}

	if (m_PermFile.m_Resources.empty())
	{
		printf("[ ERROR ] This perm file doesn't seems to have any resources!\n"); ReturnKeyWait();
		return 1;
	}

	std::vector<Illusion::Texture_t*> m_Textures;
	for (UFG::ResourceEntry_t* m_Resource : m_PermFile.m_Resources)
	{
		if (m_Resource->m_TypeUID != ILLUSION_TEXTURE_PC64)
			continue;

		m_Textures.emplace_back(reinterpret_cast<Illusion::Texture_t*>(m_Resource->GetData()));
	}

	if (m_Textures.empty())
	{
		printf("[ ERROR ] This perm file doesn't seems to have any textures!\n"); ReturnKeyWait();
		return 1;
	}

	std::string m_TempPath = m_PermPath;
	memcpy(&m_TempPath[m_PermPos + 1], "temp", 4);

	File_t m_TempFile;
	switch (m_TempFile.Read(&m_TempPath[0]))
	{
		default: break;
		case ERROR_FILE_NOT_FOUND:
		{
			printf("[ ERROR ] Temp file for current perm file is missing!\n"); ReturnKeyWait();
			return 1;
		}
		case ERROR_READ_FAULT:
		{
			printf("[ ERROR ] Failed to read whole Temp file\n"); ReturnKeyWait();
			return 1;
		}
	}

	// Setup Output Directory
	{
		if (m_OutputDir.empty())
			m_OutputDir = m_PermFile.m_Name;

		if (m_CurrentDirectory[0] != '\0')
			SetCurrentDirectoryA(m_CurrentDirectory); // Restore working directory, just in case...

		std::filesystem::create_directory(m_OutputDir);
	}

	bool m_UseHashName		= HasArgSet("-hashname");
	bool m_GenerateConfig	= HasArgSet("-config");
	#ifdef _DEBUG
		m_GenerateConfig = true;
	#endif


	tinyxml2::XMLDocument m_XMLDoc;
	m_XMLDoc.InsertEndChild(m_XMLDoc.NewComment(" Generated by " PROJECT_NAME " "));
	m_XMLDoc.InsertEndChild(m_XMLDoc.NewDeclaration());

	tinyxml2::XMLElement* m_XMLMediaPack = m_XMLDoc.NewElement("MediaPack");
	m_XMLDoc.InsertEndChild(m_XMLMediaPack);
	char m_FormatBuffer[128];

	size_t m_TextureIndex = 0;
	size_t m_TexturesExported = 0;

	for (Illusion::Texture_t* m_Texture : m_Textures)
	{
		++m_TextureIndex;
		std::string m_FileName = m_Texture->m_DebugName;
		if (m_UseHashName)
		{
			sprintf_s(m_FormatBuffer, sizeof(m_FormatBuffer), "0x%X", m_Texture->m_NameUID);
			m_FileName = m_FormatBuffer;
		}

		m_FileName += ".png";

		// XML Resource
		if (m_GenerateConfig)
		{
			tinyxml2::XMLElement* m_XMLResource = m_XMLDoc.NewElement("Resource");
			{
				// Insert output name if using hash...
				if (m_UseHashName)
					m_XMLResource->SetAttribute("OutputName", m_Texture->m_DebugName);

				// Format (Compression)
				const char* m_Format = Helper::Texture::GetFormat(m_Texture->m_Format);
				if (m_Format)
					m_XMLResource->SetAttribute("Compression", m_Format);

				// AlphaState
				const char* m_AlphaState = Helper::Texture::GetAlphaState(m_Texture->m_AlphaStateUID);
				if (m_AlphaState)
					m_XMLResource->SetAttribute("AlphaState", m_AlphaState);

				// Filter
				const char* m_Filter = Helper::Texture::GetFilter(m_Texture->m_Filter);
				if (m_Filter)
					m_XMLResource->SetAttribute("Filter", m_Filter);

				// MipMaps
				if (m_Texture->m_NumMipMaps)
					m_XMLResource->SetAttribute("MipMaps", m_Texture->m_NumMipMaps);

				// TilingU
				if (m_Texture->m_Flags & Illusion::FLAG_CLAMPU)
					m_XMLResource->SetAttribute("TilingU", "Clamp");
				else if (m_Texture->m_Flags & Illusion::FLAG_MIRRORU)
						m_XMLResource->SetAttribute("TilingU", "Mirror");

				// TilingV
				if (m_Texture->m_Flags & Illusion::FLAG_CLAMPV)
					m_XMLResource->SetAttribute("TilingV", "Clamp");
				else if (m_Texture->m_Flags & Illusion::FLAG_MIRRORV)
					m_XMLResource->SetAttribute("TilingV", "Mirror");

				// Aniso
				const char* m_Aniso = Helper::Texture::GetAniso(m_Texture->m_Aniso);
				if (m_Aniso)
					m_XMLResource->SetAttribute("Aniso", m_Aniso);

				// Path
				m_XMLResource->SetText(m_FileName.c_str());
			}
			m_XMLMediaPack->InsertEndChild(m_XMLResource);
		}

		std::string m_OutputFilePath = (m_OutputDir + "\\" + m_FileName);
		if (!Helper::Texture::ExportToPNG(m_Texture, m_TempFile.m_Data, &m_OutputFilePath[0]))
		{
			printf("[ Error ] Failed to export: %s\n", m_Texture->m_DebugName);
			continue;
		}

		printf("[%zu/%zu] Exported: %s\n", m_TextureIndex, m_Textures.size(), m_Texture->m_DebugName);
		++m_TexturesExported;
	}

	if (m_GenerateConfig)
	{
		std::string m_ConfigPath = m_OutputDir + "\\config.xml";
		m_XMLDoc.SaveFile(m_ConfigPath.c_str());
	}

	printf("\n[ ~ ] Exported %zu textures out of %zu.\n", m_TexturesExported, m_Textures.size()); ReturnKeyWait();

	return 0;
}