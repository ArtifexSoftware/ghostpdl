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
using System.Runtime;
using System.Runtime.InteropServices;
using GhostMono;

namespace gs_mono_example
{
	public partial class MainWindow
	{
        int m_current_page;
        Gtk.TreeIter m_tree_iter;

        /* For PDF optimization */
        private void PageRangeRender(int first_page, int last_page)
		{
			bool render_pages = false;
			for (int k = first_page; k <= last_page; k++)
			{
				if (m_docPages[k].Content != Page_Content_t.FULL_RESOLUTION)
				{
					render_pages = true;
					break;
				}
			}
			if (!render_pages)
				return;

			m_busy_render = true;
			//m_ghostscript.gsDisplayDeviceRender(m_currfile, first_page + 1, last_page + 1, 1.0);
		}

        /* Callback from ghostscript with the rendered image. */
        private void MainPageCallback(int width, int height, int raster, double zoom_in,
            int page_num, IntPtr data)
        {
            Byte[] bitmap = null;

            bitmap = new byte[raster * height];
            Marshal.Copy(data, bitmap, 0, raster * height);
            DocPage doc_page = m_docPages[m_current_page];

            if (doc_page.Content != Page_Content_t.FULL_RESOLUTION ||
                m_aa_change)
            {
                doc_page.Width = width;
                doc_page.Height = height;
                doc_page.Content = Page_Content_t.FULL_RESOLUTION;

                doc_page.Zoom = m_doczoom;
                doc_page.PixBuf = new Gdk.Pixbuf(bitmap,
                        Gdk.Colorspace.Rgb, false, 8, width,
                        height, raster);

                m_GtkimageStoreMain.SetValue(m_tree_iter, 0, doc_page.PixBuf);
            }

            if (page_num == 1)
                m_page_scroll_pos[0] = height;
            else
                m_page_scroll_pos[page_num - 1] = height + m_page_scroll_pos[page_num - 2];

            m_GtkimageStoreMain.IterNext(ref m_tree_iter);
            m_current_page += 1;
            m_GtkProgressBar.Fraction = ((double)page_num / ((double)page_num + 1));
            return;
        }

        private void RenderingDone()
		{
            if (m_firstime)
            {
                for (int k = 0; k < m_numpages; k++)
                {
                    pagesizes_t size = new pagesizes_t();
                    size.height = m_docPages[k].Height;
                    size.width = m_docPages[k].Width;
                    m_page_sizes.Add(size);
                }
            }

            m_aa_change = false;
            m_firstime = false;
            m_busy_render = false;

            /* Free up the large objects generated from the image pages.  If
             * this is not done, the application will run out of memory as
             * the user does repeated zoom or aa changes, generating more and
             * more pages without freeing previous ones.  The large object heap
             * is handled differently than other memory in terms of its GC */            
            GCSettings.LargeObjectHeapCompactionMode = 
                GCLargeObjectHeapCompactionMode.CompactOnce;
                GC.Collect();

            RemoveProgressBar();
            m_file_open = true;
            m_ghostscript.PageRenderedCallBack -= new GSMONO.PageRendered(gsPageRendered);
            m_GtkaaCheck.Sensitive = true;
            m_GtkZoomMinus.Sensitive = true;
            m_GtkZoomPlus.Sensitive = true;
        }

		/* Render all pages full resolution */
		private void RenderMainFirst()
		{
            m_current_page = 0;
            m_GtkimageStoreMain.GetIterFirst(out m_tree_iter);
            m_busy_render = true;
            m_ghostscript.PageRenderedCallBack += new GSMONO.PageRendered(gsPageRendered);
            AddProgressBar("Rendering Pages");
            m_ghostscript.DisplayDeviceRenderAll(m_currfile, m_doczoom, m_aa, GS_Task_t.DISPLAY_DEV_NON_PDF);
		}

		/* Render all, but only if not already busy,  called via zoom or aa changes */
		private void RenderMainAll()
		{
            try
            {
                if (!m_init_done)
                    return;
                m_GtkaaCheck.Sensitive = false;
                m_GtkZoomMinus.Sensitive = false;
                m_GtkZoomPlus.Sensitive = false;
                RenderMainFirst();
            }
            catch(Exception except)
            {
                var mess = except.Message;
            }
        }
	}
}