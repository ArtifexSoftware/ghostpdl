using System;
using System.Collections.Generic;
using System.Drawing;
using System.Runtime.InteropServices;
using GhostNET;

namespace gs_mono_example
{
	public partial class MainWindow
	{
		int m_firstpage;
		int m_lastpage;

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
			m_firstpage = first_page;
			m_lastpage = last_page;
			//m_ghostscript.gsDisplayDeviceRender(m_currfile, first_page + 1, last_page + 1, 1.0);
		}

        /* Callback from ghostscript with the rendered image. */
        private void MainPageCallback(int width, int height, int raster, double zoom_in,
            int page_num, IntPtr data)
        {
            Byte[] bitmap = new byte[raster * height];

            idata_t page = new idata_t();

            Marshal.Copy(data, bitmap, 0, raster * height);

            page.bitmap = bitmap;
            page.page_num = page_num;
            page.width = width;
            page.height = height;
            page.raster = raster;
            page.zoom = zoom_in;
            m_images_rendered.Add(page);

            /* Dispatch progress bar update on UI thread */
            Gtk.Application.Invoke(delegate {
                m_GtkProgressBar.Fraction = ((double)page_num / ((double)page_num + 1));
            });
        }

        /* Done rendering. Again in MS .NET this is on the main UI thread.
           In MONO .NET not the case. */
        private void RenderingDone()
        {
            /* Dispatch on UI thread */
            Gtk.Application.Invoke(delegate {
                RenderingDoneMain();
            });
        }

        private void RenderingDoneMain()
		{
            Gtk.TreeIter tree_iter;
            m_GtkimageStoreMain.GetIterFirst(out tree_iter);

            for (int k = 0; k < m_images_rendered.Count; k++)
            {
                DocPage doc_page = m_docPages[k];

                if (doc_page.Content != Page_Content_t.FULL_RESOLUTION ||
                    m_aa_change)
                {
                    doc_page.Width = m_images_rendered[k].width;
                    doc_page.Height = m_images_rendered[k].height;
                    doc_page.Content = Page_Content_t.FULL_RESOLUTION;

                    doc_page.Zoom = m_doczoom;
                    doc_page.PixBuf = new Gdk.Pixbuf(m_images_rendered[k].bitmap,
                            Gdk.Colorspace.Rgb, false, 8, m_images_rendered[k].width,
                            m_images_rendered[k].height, m_images_rendered[k].raster);

                    m_GtkimageStoreMain.SetValue(tree_iter, 0, m_docPages[k].PixBuf);
                    m_GtkimageStoreMain.IterNext(ref tree_iter);
                }
            }

            m_aa_change = false;
            m_firstime = false;
            m_busy_render = false;
            m_images_rendered.Clear();
            m_file_open = true;
            m_ghostscript.gsPageRenderedMain -= new ghostsharp.gsCallBackPageRenderedMain(gsPageRendered);
            m_GtkaaCheck.Sensitive = true;
            m_GtkZoomMinus.Sensitive = true;
            m_GtkZoomPlus.Sensitive = true;
        }

		/* Render all pages full resolution */
		private void RenderMainFirst()
		{
			m_firstpage = 1;
			m_busy_render = true;
			m_page_progress_count = 0;
            m_ghostscript.gsPageRenderedMain += new ghostsharp.gsCallBackPageRenderedMain(gsPageRendered);
			m_ghostscript.gsDisplayDeviceRenderAll(m_currfile, m_doczoom, m_aa, GS_Task_t.DISPLAY_DEV_NON_PDF);
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
