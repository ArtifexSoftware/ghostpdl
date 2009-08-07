//
//  Little cms
//  Copyright (C) 1998-2007 Marti Maria
//
// Permission is hereby granted, free of charge, to any person obtaining 
// a copy of this software and associated documentation files (the "Software"), 
// to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, 
// and/or sell copies of the Software, and to permit persons to whom the Software 
// is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in 
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, 
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO 
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION 
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
// 


// Example: how to show white points of profiles


#include "lcms.h"



static
void ShowWhitePoint(LPcmsCIEXYZ WtPt)
{       
	   cmsCIELab Lab;
	   cmsCIELCh LCh;
	   cmsCIExyY xyY;
       char Buffer[1024];

		
	   _cmsIdentifyWhitePoint(Buffer, WtPt);
       printf("%s\n", Buffer);
       
       cmsXYZ2Lab(NULL, &Lab, WtPt);
	   cmsLab2LCh(&LCh, &Lab);
	   cmsXYZ2xyY(&xyY, WtPt);

	   printf("XYZ=(%3.1f, %3.1f, %3.1f)\n", WtPt->X, WtPt->Y, WtPt->Z);
       printf("Lab=(%3.3f, %3.3f, %3.3f)\n", Lab.L, Lab.a, Lab.b);
	   printf("(x,y)=(%3.3f, %3.3f)\n", xyY.x, xyY.y);
	   printf("Hue=%3.2f, Chroma=%3.2f\n", LCh.h, LCh.C);
       printf("\n");
       
}


int main (int argc, char *argv[])
{
       printf("Show media white of profiles, identifying black body locus. v2\n\n");


       if (argc == 2) {
				  cmsCIEXYZ WtPt;
		          cmsHPROFILE hProfile = cmsOpenProfileFromFile(argv[1], "r");

				  printf("%s\n", cmsTakeProductName(hProfile));
				  cmsTakeMediaWhitePoint(&WtPt, hProfile);
				  ShowWhitePoint(&WtPt);
				  cmsCloseProfile(hProfile);
              }
       else
              {
              cmsCIEXYZ xyz;
              
              printf("usage:\n\nIf no parameters are given, then this program will\n");
              printf("ask for XYZ value of media white. If parameter given, it must be\n");
              printf("the profile to inspect.\n\n");

              printf("X? "); scanf("%lf", &xyz.X);
              printf("Y? "); scanf("%lf", &xyz.Y);
              printf("Z? "); scanf("%lf", &xyz.Z);

			  ShowWhitePoint(&xyz);
              }

	   return 0;
}

