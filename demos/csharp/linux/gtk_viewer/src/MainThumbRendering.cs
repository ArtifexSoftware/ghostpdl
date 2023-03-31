/* Copyright (C) 2020-2023 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using GhostMono;

namespace gs_mono_example
{
	public partial class MainWindow
	{
	    private static List<DocPage> m_thumbnails;

		/* Assign current pages to blown up thumbnail images */
		private void ThumbAssignMain(int page_num, int width, int height, double zoom_in)
		{
			DocPage doc_page = new DocPage();
			doc_page.Content = Page_Content_t.THUMBNAIL;
			doc_page.Zoom = zoom_in;
            doc_page.Width = (int)(width / (Constants.SCALE_THUMB));
            doc_page.Height = (int)(height / (Constants.SCALE_THUMB));
            doc_page.PixBuf = m_thumbnails[page_num - 1].PixBuf.ScaleSimple(doc_page.Width, doc_page.Height, Gdk.InterpType.Nearest);
			doc_page.PageNum = page_num;
			m_docPages.Add(doc_page);

            if (page_num == 1)
                m_page_scroll_pos.Add(height);
            else
                m_page_scroll_pos.Add(height + m_page_scroll_pos[page_num - 2]);
        }

        private void ThumbsDone()
        {
            m_GtkProgressBar.Fraction = 1.0;
            m_ghostscript.PageRenderedCallBack -= new GSMONO.PageRendered(gsThumbRendered);
            m_numpages = m_thumbnails.Count;
            if (m_numpages < 1)
            {
                ShowMessage(NotifyType_t.MESS_STATUS, "File failed to open properly");
                CleanUp();
            }
            else
            {
                m_currpage = 1;
                m_GtkpageEntry.Text = "1";
                m_GtkpageTotal.Text = "/" + m_numpages;
                m_GtkzoomEntry.Text = "100";

                m_GtkmainScroll.ShowAll();
                m_init_done = true;
                RemoveProgressBar();
                RenderMainFirst();
            }
        }

        /* Callback from GS with the rendered thumbnail.  Also update progress */
        private void ThumbPageCallback(int width, int height, int raster, double zoom_in,
            int page_num, IntPtr data)
        {
            Byte[] bitmap = null;

            bitmap = new byte[raster * height];
            Marshal.Copy(data, bitmap, 0, raster * height);
            DocPage doc_page = new DocPage();
            m_thumbnails.Add(doc_page);

            doc_page.Content = Page_Content_t.THUMBNAIL;
            doc_page.Width = width;
            doc_page.Height = height;
            doc_page.Zoom = zoom_in;
            doc_page.PixBuf = new Gdk.Pixbuf(bitmap,
                        Gdk.Colorspace.Rgb, false, 8, width,
                        height, raster);
            m_GtkimageStoreThumb.AppendValues(doc_page.PixBuf);
                  
            ThumbAssignMain(page_num, width, height, 1.0);
            m_GtkimageStoreMain.AppendValues(m_docPages[page_num-1].PixBuf);

            m_GtkProgressBar.Fraction = ((double)page_num / ((double)page_num + 1));
            return;
		}

		/* Render the thumbnail images */
		private void RenderThumbs()
		{
            AddProgressBar("Rendering Thumbs");
            m_ghostscript.PageRenderedCallBack +=
                new GSMONO.PageRendered(gsThumbRendered);
            m_ghostscript.DisplayDeviceRenderThumbs(m_currfile,
                Constants.SCALE_THUMB, false);
		}
	}
}