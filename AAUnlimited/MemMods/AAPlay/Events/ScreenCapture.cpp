#include "ScreenCapture.h"

#include <Windows.h>
#include <GdiPlus.h>

#include "Files/Config.h"
#include "Files/Logger.h"

#include "MemMods/Hook.h"
#include "General/ModuleInfo.h"

#pragma comment(lib, "Gdiplus.lib")

namespace PlayInjections {
	namespace ScreenCapture {

		CLSID encoderClsid;
		Gdiplus::EncoderParameters jpegParameters;
		ULONG jpegQuality;
		bool gdiInit;

		int GetEncoderClsid(const WCHAR* format, CLSID* pClsid)
		{
			UINT  num = 0;          // number of image encoders
			UINT  size = 0;         // size of the image encoder array in bytes

			Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;
			Gdiplus::GetImageEncodersSize(&num, &size);
			if (size == 0)
				return -1;  // Failure

			pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
			if (pImageCodecInfo == NULL)
				return -1;  // Failure

			GetImageEncoders(num, size, pImageCodecInfo);

			for (UINT j = 0; j < num; ++j)
			{
				if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0)
				{
					*pClsid = pImageCodecInfo[j].Clsid;
					free(pImageCodecInfo);
					return j;  // Success
				}
			}

			free(pImageCodecInfo);
			return -1;  // Failure
		}

		void InitGDI() {
			ULONG_PTR gdiToken;
			Gdiplus::GdiplusStartupInput gdiStartupInput;
			gdiStartupInput.GdiplusVersion = 1;
			gdiStartupInput.DebugEventCallback = NULL;
			gdiStartupInput.SuppressBackgroundThread = FALSE;
			gdiStartupInput.SuppressExternalCodecs = FALSE;

			Gdiplus::GdiplusStartup(&gdiToken, &gdiStartupInput, NULL);

			GetEncoderClsid(L"image/jpeg", &encoderClsid);
			gdiInit = true;
		}

		void __stdcall SaveAs(DWORD gdiBitmapInfo, DWORD gdiBitmapData, WCHAR* path) {
			if (!gdiInit) {
				InitGDI();
				jpegParameters.Count = 1;
				jpegParameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
				jpegParameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
				jpegParameters.Parameter[0].NumberOfValues = 1;
				jpegParameters.Parameter[0].Value = &jpegQuality;
				jpegQuality = 100;
			}
			Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromBITMAPINFO((BITMAPINFO*)gdiBitmapInfo, (void*)gdiBitmapData);
			size_t pathLength = wcslen(path);
			static const WCHAR jpgExtension[]{ L"jpg" };
			wcsncpy_s(path + pathLength - 3, 4, jpgExtension, 3);
			bitmap->Save(path, &encoderClsid, &jpegParameters);
			delete bitmap;
		}

		void __declspec(naked) SaveRedirect() {
			__asm {
				lea ecx, [esp + 0x6C]
				mov edx, [esp + 0x3C]
				push [esp+0xDC]
				push edx
				push ecx
				call SaveAs
				ret
			}
		}

		void InitInjection() {
			gdiInit = false;
			if (!g_Config.GetKeyValue(Config::SCREENSHOT_FORMAT).iVal) return;
			// Screenshot formats:
			// 0 - BMP (don't redirect)
			// 1 - JPG
			// 2 - ...
			
			/*
			00E85EEC   > 8B9424 B4000000MOV EDX,DWORD PTR SS:[ESP+B4]            ;  Full path pointer
			00E85EF3   . 8B02           MOV EAX,DWORD PTR DS:[EDX]               ;  Full path pointer value
			00E85EF5   . E8 66AB0000    CALL AA2Play.00E90A60                    ;  Create file
			*/
			DWORD address = General::GameBase + 0x1C5EEC;
			DWORD redirectAddress = (DWORD)(&SaveRedirect);
			Hook((BYTE*)address,
			{ 0x8B, 0x94, 0x24, 0xB4, 0x00, 0x00, 0x00,				//expected values
			  0x8B, 0x02,
			  0xE8 },
			{ 0xE8, HookControl::RELATIVE_DWORD, redirectAddress,	//redirect to our function
			  0xE9, 0x0B, 0x01, 0x00, 0x00 },						//jump to cleanup
			  NULL);
		}

	}
}
