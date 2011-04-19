#include "stdafx.h"

#using <mscorlib.dll>

using namespace System;
using namespace System::IO;
using namespace System::Collections::Generic;
using namespace System::IO::Packaging;
using namespace System::Xml;
using namespace System::Windows::Forms;
using namespace System::Windows::Xps;
using namespace System::Windows::Xps::Packaging;
using namespace System::Printing;
using namespace System::Windows::Documents;
using namespace System::Windows::Media::Imaging;


void SaveXpsPageToBitmap(Int32 res, String ^xpsFileName, String ^tiffFileName)
{
	XpsDocument^ xpsDoc = gcnew XpsDocument(xpsFileName, FileAccess::Read);


	int pageCount = xpsDoc->GetFixedDocumentSequence()->References[0]->GetDocument(false)->Pages->Count;

    BitmapEncoder ^encoder = gcnew TiffBitmapEncoder();  // Choose type here ie: JpegBitmapEncoder, TiffBitmapEncoder etc.

	// You can get the total page count from docSeq.PageCount
    for(int pageNum = 0; pageNum < pageCount; pageNum ++)
    {
        DocumentPage ^docPage = xpsDoc->GetFixedDocumentSequence()->DocumentPaginator->GetPage(pageNum);
        BitmapImage ^bitmap = gcnew BitmapImage();
        RenderTargetBitmap ^renderTarget =
            gcnew RenderTargetBitmap((int)docPage->Size.Width*res/96,
                                    (int)docPage->Size.Height*res/96,
                                    res, // WPF (Avalon) units are 96dpi based
                                    res,
									System::Windows::Media::PixelFormats::Pbgra32);
        renderTarget->Render(docPage->Visual);

		encoder->Frames->Add(BitmapFrame::Create(renderTarget));
    }

	FileStream ^pageOutStream = gcnew FileStream(tiffFileName, FileMode::Create, FileAccess::Write);
    encoder->Save(pageOutStream);
    pageOutStream->Close();
}

[STAThread]
int _tmain(int argc, _TCHAR* argv[])
{
 	if (argc != 4)
	{
	  Console::WriteLine("Convert XPS files to TIFF");
	  Console::WriteLine("Usage: xps2tiff RESOLUTION INPUT.XPS OUTPUT.TIFF");
	  return 0;
	}

   SaveXpsPageToBitmap(atoi(argv[1]), %String(argv[2]), %String(argv[3]));

	return 0;
}